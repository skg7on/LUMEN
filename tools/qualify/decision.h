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

// Fail-closed qualification decision logic.
//
// `evaluate` derives the overall outcome and P1/P2/P3 eligibility from recorded
// probe evidence. Missing or unmeasured evidence blocks; failed safety or
// correctness evidence fails. Integrity sealing is an additional publication
// gate enforced by the development validator, not by this function.

#pragma once

#include <string_view>

#include "qualify/types.h"

namespace lumen::qualification {

inline constexpr std::string_view kProbeAsyncCompute =
    "execution/async-compute";
inline constexpr std::string_view kProbeMemory = "memory/kinds-and-copies";
inline constexpr std::string_view kProbeFence = "lifecycle/fence-and-ownership";
inline constexpr std::string_view kProbeRecovery =
    "recovery/partial-submission";
inline constexpr std::string_view kProbeSustained = "thermal/sustained";
inline constexpr std::string_view kProbeAttention = "attention/bounded-path";

inline constexpr std::string_view kReasonInvalidLimits = "invalid_limits";
inline constexpr std::string_view kReasonMissingTargetIdentity =
    "missing_target_identity";
inline constexpr std::string_view kReasonSyntheticSource = "synthetic_source";
inline constexpr std::string_view kReasonMeasurementIncomplete =
    "measurement_incomplete";
inline constexpr std::string_view kReasonProfileHashMismatch =
    "profile_hash_mismatch";
inline constexpr std::string_view kReasonRunSchema = "invalid_run_schema";
inline constexpr std::string_view kReasonMissingProbe =
    "missing_required_probe";
inline constexpr std::string_view kReasonProbeUnsupported =
    "probe_not_supported";
inline constexpr std::string_view kReasonProbeFailed = "probe_failed";
inline constexpr std::string_view kReasonNativeAsync = "missing_native_async";
inline constexpr std::string_view kReasonIndependentCorrectness =
    "missing_independent_correctness";
inline constexpr std::string_view kReasonDevicePlacement =
    "missing_device_placement";
inline constexpr std::string_view kReasonSustained = "missing_sustained_run";
inline constexpr std::string_view kReasonBoundedAttention =
    "missing_bounded_attention";
inline constexpr std::string_view kReasonQuarantine = "unresolved_quarantine";
inline constexpr std::string_view kReasonDuplicateProbe = "duplicate_probe_id";
inline constexpr std::string_view kReasonCorrectnessMismatch =
    "correctness_mismatch";
inline constexpr std::string_view kReasonCleanupFailed = "cleanup_failed";

const ProbeResult *find_probe(const EvidenceBundle &bundle,
                              std::string_view id);

// Derives the overall decision and phase eligibility from the bundle. The
// returned bundle-local decision is provisional until external sealing.
QualificationDecision evaluate(const EvidenceBundle &bundle);

} // namespace lumen::qualification
