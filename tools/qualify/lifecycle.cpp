// Copyright 2026 Gavin Sun
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "qualify/lifecycle.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace lumen::qualification {
namespace {

bool valid_call_status(std::uint32_t status) {
  return status <= static_cast<std::uint32_t>(LUMEN_STATUS_CORRUPT);
}

bool receipt_header_valid(const Receipt &receipt) {
  return receipt.struct_size == sizeof(Receipt) &&
         receipt.abi_major == LUMEN_QUALIFICATION_ABI_MAJOR &&
         receipt.abi_minor == LUMEN_QUALIFICATION_ABI_MINOR;
}

bool proof_header_valid(const RecoveryProof &proof) {
  return proof.struct_size == sizeof(RecoveryProof) &&
         proof.abi_major == LUMEN_QUALIFICATION_ABI_MAJOR &&
         proof.abi_minor == LUMEN_QUALIFICATION_ABI_MINOR;
}

bool contains(const std::vector<std::uint64_t> &ids, std::uint64_t id) {
  return std::find(ids.begin(), ids.end(), id) != ids.end();
}

void retire(LeaseSet &leases) {
  leases.state = LeaseState::retired;
  leases.owns_fence = false;
  leases.accepted_event_id = 0;
}

} // namespace

Result<LeaseSet> reserve_leases(std::uint64_t epoch,
                                std::uint64_t resource_set_id,
                                std::span<const std::uint64_t> resource_ids) {
  if (epoch == 0 || resource_set_id == 0 || resource_ids.empty()) {
    return Result<LeaseSet>::error(Status::invalid_input);
  }

  LeaseSet leases;
  leases.epoch = epoch;
  leases.resource_set_id = resource_set_id;
  leases.resource_ids.reserve(resource_ids.size());
  for (const std::uint64_t id : resource_ids) {
    if (id == 0 || contains(leases.resource_ids, id)) {
      return Result<LeaseSet>::error(Status::invalid_input);
    }
    leases.resource_ids.push_back(id);
  }
  return Result<LeaseSet>::ok(std::move(leases));
}

Status observe_receipt(const Receipt &receipt, LeaseSet &leases) {
  if (!receipt_header_valid(receipt)) {
    return Status::invalid_input;
  }
  if (receipt.disposition > LUMEN_QUAL_UNCERTAIN ||
      !valid_call_status(receipt.call_status)) {
    return Status::invalid_input;
  }
  if (leases.state != LeaseState::reserved) {
    return Status::stale;
  }

  const auto disposition =
      static_cast<lumen_qual_disposition_t>(receipt.disposition);

  if (disposition == LUMEN_QUAL_REJECTED) {
    // A rejected command references no resources. A successful call that
    // claims rejection is contradictory; keep the set quarantined.
    if (receipt.call_status == LUMEN_STATUS_OK) {
      leases.state = LeaseState::quarantined;
      return Status::invalid_input;
    }
    retire(leases);
    return Status::ok;
  }

  if (receipt.context_epoch != leases.epoch ||
      receipt.touched_resource_set_id != leases.resource_set_id) {
    return Status::stale;
  }

  if (disposition == LUMEN_QUAL_ACCEPTED) {
    // Accepted work must own a completion fence. Any contract violation fails
    // closed rather than releasing resources.
    if (receipt.call_status != LUMEN_STATUS_OK) {
      leases.state = LeaseState::quarantined;
      return Status::invalid_input;
    }
    if (receipt.completion_event == nullptr) {
      leases.state = LeaseState::quarantined;
      return Status::invalid_input;
    }
    leases.state = LeaseState::inflight;
    leases.owns_fence = true;
    leases.accepted_event_id =
        reinterpret_cast<std::uintptr_t>(receipt.completion_event);
    return Status::ok;
  }

  // Uncertain: retain the full pre-reserved set even without a fence.
  leases.state = LeaseState::quarantined;
  leases.owns_fence = receipt.completion_event != nullptr;
  leases.accepted_event_id =
      leases.owns_fence
          ? reinterpret_cast<std::uintptr_t>(receipt.completion_event)
          : 0;
  return Status::ok;
}

Status observe_fence(const FenceObservation &observation, LeaseSet &leases) {
  if (leases.state == LeaseState::retired) {
    return Status::stale;
  }
  if (leases.state == LeaseState::reserved) {
    return Status::invalid_input;
  }
  if (observation.resource_set_id != leases.resource_set_id ||
      observation.epoch != leases.epoch) {
    return Status::stale;
  }
  if (leases.owns_fence && observation.event_id != leases.accepted_event_id) {
    return Status::stale;
  }
  if (!valid_call_status(observation.query_status) ||
      !valid_call_status(observation.final_command_status)) {
    return Status::corrupt;
  }

  // A failed poll call or API error is not a fence.
  if (observation.query_status != LUMEN_STATUS_OK) {
    return static_cast<Status>(observation.query_status);
  }

  switch (observation.final_event_status) {
  case LUMEN_EVENT_PENDING:
    return Status::timeout;
  case LUMEN_EVENT_COMPLETE:
    retire(leases);
    return observation.final_command_status == LUMEN_STATUS_OK
               ? Status::ok
               : Status::device_error;
  case LUMEN_EVENT_FAILED:
    if (!observation.all_work_stopped) {
      // The event failed but no quiescence guarantee exists: quarantine and
      // keep the owned fence for a later observation or recovery proof.
      leases.state = LeaseState::quarantined;
      return Status::device_error;
    }
    retire(leases);
    return observation.final_command_status == LUMEN_STATUS_OK
               ? Status::device_error
               : static_cast<Status>(observation.final_command_status);
  default:
    return Status::invalid_input;
  }
}

Status apply_recovery(const RecoveryProof &proof, LeaseSet &leases) {
  if (!proof_header_valid(proof) || !valid_call_status(proof.call_status)) {
    return Status::invalid_input;
  }
  if (proof.mechanism != LUMEN_QUAL_RECOVERY_CONTEXT &&
      proof.mechanism != LUMEN_QUAL_RECOVERY_DEVICE) {
    return Status::unsupported;
  }
  if (leases.state == LeaseState::retired) {
    return Status::stale;
  }
  if (leases.state == LeaseState::reserved) {
    return Status::invalid_input;
  }
  if (proof.old_epoch != leases.epoch) {
    return Status::stale;
  }
  if (leases.epoch == (std::numeric_limits<std::uint64_t>::max)()) {
    return Status::invalid_input;
  }
  if (proof.new_epoch != leases.epoch + 1) {
    return Status::invalid_input;
  }
  if (proof.call_status != LUMEN_STATUS_OK) {
    return static_cast<Status>(proof.call_status);
  }
  if (proof.evidence_id == 0) {
    return Status::invalid_input;
  }
  if (proof.quiescent == 0 || proof.old_handles_invalidated == 0) {
    return Status::device_error;
  }

  retire(leases);
  leases.epoch = proof.new_epoch;
  return Status::ok;
}

Status request_destroy(const LeaseSet &leases, std::uint64_t resource_id) {
  if (!contains(leases.resource_ids, resource_id)) {
    return Status::invalid_input;
  }
  return leases.state == LeaseState::retired ? Status::ok
                                             : Status::invalid_input;
}

bool eligible_for_retirement(const LeaseSet &leases) noexcept {
  return leases.state == LeaseState::retired;
}

} // namespace lumen::qualification
