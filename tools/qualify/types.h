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

// Internal qualification evidence records.
//
// These are C++ host types used by the SDK-free harness. They are not part of
// the public backend ABI. Only `EvidenceState::supported` can enable a
// capability, and only when backed by real-device evidence; `unavailable`,
// `failed`, and `not_tested` never do.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lumen::qualification {

enum class EvidenceState {
  supported,
  unsupported,
  unavailable,
  failed,
  not_tested,
};

constexpr std::string_view evidence_state_name(EvidenceState state) noexcept {
  switch (state) {
  case EvidenceState::supported:
    return "supported";
  case EvidenceState::unsupported:
    return "unsupported";
  case EvidenceState::unavailable:
    return "unavailable";
  case EvidenceState::failed:
    return "failed";
  case EvidenceState::not_tested:
    return "not_tested";
  }
  return "unknown";
}

std::optional<EvidenceState>
parse_evidence_state(std::string_view text) noexcept;

enum class SourceKind {
  real_device,
  synthetic,
};

constexpr std::string_view source_kind_name(SourceKind kind) noexcept {
  switch (kind) {
  case SourceKind::real_device:
    return "real_device";
  case SourceKind::synthetic:
    return "synthetic";
  }
  return "unknown";
}

std::optional<SourceKind> parse_source_kind(std::string_view text) noexcept;

enum class ComparisonMode {
  exact,
  abs_rel,
};

constexpr std::string_view comparison_mode_name(ComparisonMode mode) noexcept {
  switch (mode) {
  case ComparisonMode::exact:
    return "exact";
  case ComparisonMode::abs_rel:
    return "abs_rel";
  }
  return "unknown";
}

enum class OverallDecision {
  qualified,
  blocked,
  failed,
};

constexpr std::string_view
overall_decision_name(OverallDecision outcome) noexcept {
  switch (outcome) {
  case OverallDecision::qualified:
    return "qualified";
  case OverallDecision::blocked:
    return "blocked";
  case OverallDecision::failed:
    return "failed";
  }
  return "unknown";
}

enum class P2Mode {
  native_paged,
  gather,
  no_go,
};

constexpr std::string_view p2_mode_name(P2Mode mode) noexcept {
  switch (mode) {
  case P2Mode::native_paged:
    return "native_paged";
  case P2Mode::gather:
    return "gather";
  case P2Mode::no_go:
    return "no_go";
  }
  return "unknown";
}

enum class IntegrityState {
  pending,
  verified,
};

constexpr std::string_view integrity_state_name(IntegrityState state) noexcept {
  switch (state) {
  case IntegrityState::pending:
    return "pending";
  case IntegrityState::verified:
    return "verified";
  }
  return "unknown";
}

// Identifies the selected board, OS, driver, SDK, and toolchain. Every field is
// required; a stable device identifier must not be a secret or credential.
struct TargetIdentity {
  std::string vendor;
  std::string sku;
  std::string board;
  std::string board_revision;
  std::string device_identifier;
  std::string architecture;
  std::string os;
  std::string kernel_version;
  std::string firmware;
  std::string driver_version;
  std::string sdk_version;
  std::string runtime_version;
  std::string compiler_version;

  std::vector<std::string> missing_fields() const;
  bool complete() const { return missing_fields().empty(); }
};

// Provenance for one run. `schema_version` must be 1.
struct RunIdentity {
  std::uint32_t schema_version = 0;
  std::string run_id;
  std::string started_utc;
  std::string finished_utc;
  std::string git_commit;
  bool git_dirty = false;
  std::string diff_digest;
  std::string compiler;
  std::string linker;
  std::string cmake_version;
  std::string build_flags;
  std::string adapter_hash;
  std::string profile_hash;
  std::string graph_hash;
  SourceKind source = SourceKind::synthetic;
};

// Frozen, positive resource and experiment bounds. The target profile owns the
// board-specific values; CLI overrides may only reduce them.
struct RunLimits {
  std::uint64_t host_budget_bytes = 0;
  std::uint64_t staging_budget_bytes = 0;
  std::uint64_t device_budget_bytes = 0;
  std::uint64_t workspace_budget_bytes = 0;
  std::uint32_t max_inflight_commands = 0;
  std::uint32_t max_graphs = 0;
  std::uint32_t sample_count = 0;
  std::uint32_t warmup_count = 0;
  std::uint64_t per_case_timeout_ns = 0;
  std::uint64_t soak_duration_ns = 0;
  double temperature_stop_celsius = 0.0;
  double power_stop_watts = 0.0;
  std::string profile_hash;
};

// Frozen before device measurement. `exact` requires bitwise-equal outputs;
// `abs_rel` uses `abs(actual - expected) <= absolute + relative * abs(expected)`.
struct NumericTolerance {
  ComparisonMode mode = ComparisonMode::abs_rel;
  double absolute = 0.0;
  double relative = 0.0;
  std::string accumulation_dtype;
  std::string output_dtype;
  std::string profile_hash;
};

// One measured timing observation. Times are monotonic host nanoseconds; the
// optional device duration is recorded separately and never subtracted from an
// incomparable clock.
struct TimingSample {
  std::string run_id;
  std::string probe_id;
  std::uint64_t sample_id = 0;
  enum class Phase { cold, warmup, measured } phase = Phase::measured;
  std::uint64_t submit_start_ns = 0;
  std::uint64_t submit_return_ns = 0;
  std::uint64_t observed_complete_ns = 0;
  std::optional<std::uint64_t> device_duration_ns;
  std::uint64_t operation_bytes = 0;
  std::uint64_t operation_flops = 0;
  std::string final_status;
};

// Result of one bounded probe. A required probe must be `supported` with the
// semantic flags its capability needs; an optional unsupported probe never
// blocks the single-NPU result on its own.
struct ProbeResult {
  std::string id;
  EvidenceState state = EvidenceState::not_tested;
  std::string reason_code;
  std::string reason_detail;
  bool required = true;
  std::vector<std::string> depends_on;
  std::string input_shape;
  std::string input_dtype;
  std::string input_layout;
  std::uint64_t seed = 0;
  std::string observed_semantics;
  std::uint64_t correctness_mismatches = 0;
  std::vector<TimingSample> timings;
  std::vector<std::string> artifacts;

  // Semantic observations that gate downstream eligibility.
  bool native_async = false;
  bool independent_correctness = false;
  bool device_placement_verified = false;
  bool sustained_complete = false;
  bool bounded_attention_path = false;
  bool native_paged_attention = false;
  bool bounded_gather = false;
  bool cleanup_ok = false;
  bool quarantine_resolved = true;
  std::vector<std::string> tested_operator_families;
  std::vector<std::string> tested_shapes;
  std::vector<std::string> tested_precisions;
};

struct CapabilityDecision {
  std::string capability;
  EvidenceState state = EvidenceState::not_tested;
  std::vector<std::string> supporting_probes;
  std::string fallback_mode;
  std::string disabled_reason;
};

struct QualificationDecision {
  OverallDecision overall = OverallDecision::blocked;
  std::vector<std::string> reason_codes;
  bool p1_eligible = false;
  P2Mode p2_mode = P2Mode::no_go;
  std::vector<std::string> p3_eligible_probe_families;
  std::vector<std::string> limitations;
  std::vector<CapabilityDecision> capabilities;
};

struct ArtifactManifestEntry {
  std::string path;
  std::string sha256;
  std::uint64_t size_bytes = 0;
};

struct EvidenceBundle {
  TargetIdentity target;
  RunIdentity run;
  RunLimits limits;
  std::vector<ProbeResult> probes;
  std::vector<CapabilityDecision> capabilities;
  QualificationDecision decision;
  std::vector<ArtifactManifestEntry> artifacts;
  bool measurement_complete = false;
  IntegrityState integrity = IntegrityState::pending;
};

} // namespace lumen::qualification
