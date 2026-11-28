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

// Lease, epoch, quarantine, and proof-driven retirement state machine for the
// qualification extension.
//
// The caller reserves every resource in a bounded set before dispatch. A
// receipt establishes ownership before any immediate completion is observed.
// An uncertain command retains the full set even without a fence. Only a real
// completion fence or a successful recovery proof retires those leases; a
// timeout, an API error, a failed poll call, or a duplicate observation does
// not.

#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "backend/qualification/qualification_api.h"
#include "core/status.h"

namespace lumen::qualification {

using Receipt = lumen_qual_submit_receipt_v1;
using RecoveryProof = lumen_qual_recovery_proof_v1;

enum class LeaseState {
  reserved,
  inflight,
  quarantined,
  retired,
};

constexpr std::string_view lease_state_name(LeaseState state) noexcept {
  switch (state) {
  case LeaseState::reserved:
    return "reserved";
  case LeaseState::inflight:
    return "inflight";
  case LeaseState::quarantined:
    return "quarantined";
  case LeaseState::retired:
    return "retired";
  }
  return "unknown";
}

struct LeaseSet {
  std::uint64_t epoch = 0;
  std::uint64_t resource_set_id = 0;
  std::vector<std::uint64_t> resource_ids;
  LeaseState state = LeaseState::reserved;
  std::uint64_t accepted_event_id = 0;
  bool owns_fence = false;
};

// One fence/poll observation. `all_work_stopped` may be set only when the
// adapter guarantees all relevant DMA and compute stopped; a FAILED event is
// not itself such a guarantee.
struct FenceObservation {
  std::uint64_t resource_set_id = 0;
  std::uint64_t epoch = 0;
  std::uint64_t event_id = 0;
  lumen_status_t query_status = LUMEN_STATUS_DEVICE_ERROR;
  lumen_event_status_t final_event_status = LUMEN_EVENT_PENDING;
  lumen_status_t final_command_status = LUMEN_STATUS_OK;
  bool all_work_stopped = false;
};

// Reserves a bounded, duplicate-free resource set before dispatch.
Result<LeaseSet> reserve_leases(std::uint64_t epoch,
                                std::uint64_t resource_set_id,
                                std::span<const std::uint64_t> resource_ids);

// Establishes ownership from a submission receipt. Rejected resources retire
// only when `call_status` is not OK. Accepted work requires an owned fence.
// Uncertain work becomes quarantined and retains the full set.
Status observe_receipt(const Receipt &receipt, LeaseSet &leases);

// Applies one completion/poll observation. Pending, query-error, and
// unguaranteed-failure observations never retire. A complete event retires;
// a failed event retires only with `all_work_stopped`.
Status observe_fence(const FenceObservation &observation, LeaseSet &leases);

// Applies a context- or device-scoped quiescence proof. Retirement requires a
// successful, quiescent proof that invalidates old handles and advances the
// epoch by exactly one without wrapping.
Status apply_recovery(const RecoveryProof &proof, LeaseSet &leases);

// Models an early destruction request. Only a retired lease permits it.
Status request_destroy(const LeaseSet &leases, std::uint64_t resource_id);

bool eligible_for_retirement(const LeaseSet &leases) noexcept;

} // namespace lumen::qualification
