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

// Scriptable receipts, fences, and recovery proofs for lifecycle tests.
//
// These are synthetic adapters. Nothing here is device evidence, and a passing
// fake can never satisfy a real-board qualification step.

#pragma once

#include <cstdint>

#include "qualify/lifecycle.h"

namespace lumen::qualification::test {

Receipt make_receipt(lumen_qual_disposition_t disposition,
                     lumen_status_t call_status, std::uint64_t epoch,
                     lumen_event_t event, std::uint64_t resource_set_id);

Receipt accepted_receipt(std::uint64_t epoch, lumen_event_t event,
                         std::uint64_t resource_set_id);
Receipt rejected_receipt(lumen_status_t call_status);
Receipt uncertain_receipt(std::uint64_t epoch, lumen_event_t event,
                          std::uint64_t resource_set_id);

FenceObservation complete_fence(std::uint64_t resource_set_id,
                                std::uint64_t epoch, std::uint64_t event_id);
FenceObservation pending_fence(std::uint64_t resource_set_id,
                               std::uint64_t epoch, std::uint64_t event_id);
FenceObservation failed_fence(std::uint64_t resource_set_id,
                              std::uint64_t epoch, std::uint64_t event_id,
                              bool all_work_stopped);
FenceObservation query_error_fence(std::uint64_t resource_set_id,
                                   std::uint64_t epoch, std::uint64_t event_id);

RecoveryProof recovery_proof(
    std::uint64_t old_epoch, std::uint64_t new_epoch, bool quiescent,
    bool old_handles_invalidated, lumen_status_t call_status = LUMEN_STATUS_OK,
    lumen_qual_recovery_mechanism_t mechanism = LUMEN_QUAL_RECOVERY_CONTEXT,
    std::uint64_t evidence_id = 1);

lumen_event_t event_token(std::uintptr_t value);

} // namespace lumen::qualification::test
