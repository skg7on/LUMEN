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

#include "qualification_fake.h"

namespace lumen::qualification::test {

Receipt make_receipt(lumen_qual_disposition_t disposition,
                     lumen_status_t call_status, std::uint64_t epoch,
                     lumen_event_t event, std::uint64_t resource_set_id) {
  Receipt receipt{};
  receipt.struct_size = sizeof(receipt);
  receipt.abi_major = LUMEN_QUALIFICATION_ABI_MAJOR;
  receipt.abi_minor = LUMEN_QUALIFICATION_ABI_MINOR;
  receipt.call_status = static_cast<std::uint32_t>(call_status);
  receipt.disposition = static_cast<std::uint32_t>(disposition);
  receipt.context_epoch = epoch;
  receipt.completion_event = event;
  receipt.touched_resource_set_id = resource_set_id;
  return receipt;
}

Receipt accepted_receipt(std::uint64_t epoch, lumen_event_t event,
                         std::uint64_t resource_set_id) {
  return make_receipt(LUMEN_QUAL_ACCEPTED, LUMEN_STATUS_OK, epoch, event,
                      resource_set_id);
}

Receipt rejected_receipt(lumen_status_t call_status) {
  return make_receipt(LUMEN_QUAL_REJECTED, call_status, 0, nullptr, 0);
}

Receipt uncertain_receipt(std::uint64_t epoch, lumen_event_t event,
                          std::uint64_t resource_set_id) {
  return make_receipt(LUMEN_QUAL_UNCERTAIN, LUMEN_STATUS_DEVICE_ERROR, epoch,
                      event, resource_set_id);
}

FenceObservation complete_fence(std::uint64_t resource_set_id,
                                std::uint64_t epoch, std::uint64_t event_id) {
  FenceObservation observation{};
  observation.resource_set_id = resource_set_id;
  observation.epoch = epoch;
  observation.event_id = event_id;
  observation.query_status = LUMEN_STATUS_OK;
  observation.final_event_status = LUMEN_EVENT_COMPLETE;
  observation.final_command_status = LUMEN_STATUS_OK;
  observation.all_work_stopped = true;
  return observation;
}

FenceObservation pending_fence(std::uint64_t resource_set_id,
                               std::uint64_t epoch, std::uint64_t event_id) {
  FenceObservation observation{};
  observation.resource_set_id = resource_set_id;
  observation.epoch = epoch;
  observation.event_id = event_id;
  observation.query_status = LUMEN_STATUS_OK;
  observation.final_event_status = LUMEN_EVENT_PENDING;
  observation.final_command_status = LUMEN_STATUS_OK;
  observation.all_work_stopped = false;
  return observation;
}

FenceObservation failed_fence(std::uint64_t resource_set_id,
                              std::uint64_t epoch, std::uint64_t event_id,
                              bool all_work_stopped) {
  FenceObservation observation{};
  observation.resource_set_id = resource_set_id;
  observation.epoch = epoch;
  observation.event_id = event_id;
  observation.query_status = LUMEN_STATUS_OK;
  observation.final_event_status = LUMEN_EVENT_FAILED;
  observation.final_command_status = LUMEN_STATUS_DEVICE_ERROR;
  observation.all_work_stopped = all_work_stopped;
  return observation;
}

FenceObservation query_error_fence(std::uint64_t resource_set_id,
                                   std::uint64_t epoch,
                                   std::uint64_t event_id) {
  FenceObservation observation{};
  observation.resource_set_id = resource_set_id;
  observation.epoch = epoch;
  observation.event_id = event_id;
  observation.query_status = LUMEN_STATUS_DEVICE_ERROR;
  observation.final_event_status = LUMEN_EVENT_PENDING;
  observation.final_command_status = LUMEN_STATUS_OK;
  observation.all_work_stopped = false;
  return observation;
}

RecoveryProof recovery_proof(std::uint64_t old_epoch, std::uint64_t new_epoch,
                             bool quiescent, bool old_handles_invalidated,
                             lumen_status_t call_status,
                             lumen_qual_recovery_mechanism_t mechanism,
                             std::uint64_t evidence_id) {
  RecoveryProof proof{};
  proof.struct_size = sizeof(proof);
  proof.abi_major = LUMEN_QUALIFICATION_ABI_MAJOR;
  proof.abi_minor = LUMEN_QUALIFICATION_ABI_MINOR;
  proof.call_status = static_cast<std::uint32_t>(call_status);
  proof.old_epoch = old_epoch;
  proof.new_epoch = new_epoch;
  proof.quiescent = quiescent ? 1u : 0u;
  proof.old_handles_invalidated = old_handles_invalidated ? 1u : 0u;
  proof.mechanism = static_cast<std::uint32_t>(mechanism);
  proof.evidence_id = evidence_id;
  return proof;
}

lumen_event_t event_token(std::uintptr_t value) {
  return reinterpret_cast<lumen_event_t>(value);
}

} // namespace lumen::qualification::test
