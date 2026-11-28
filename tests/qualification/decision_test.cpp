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

// Fail-closed decision tests: missing, unmeasured, synthetic, mismatched, or
// unsafe evidence must never produce `qualified`.

#include "check.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "qualify/decision.h"
#include "qualify/types.h"

namespace {

using namespace lumen::qualification;

RunLimits valid_limits() {
  RunLimits limits;
  limits.host_budget_bytes = 1ull << 30;
  limits.staging_budget_bytes = 1ull << 28;
  limits.device_budget_bytes = 1ull << 31;
  limits.workspace_budget_bytes = 1ull << 27;
  limits.max_inflight_commands = 8;
  limits.max_graphs = 16;
  limits.sample_count = 1000;
  limits.warmup_count = 100;
  limits.per_case_timeout_ns = 30'000'000'000ull;
  limits.soak_duration_ns = 1'800'000'000'000ull;
  limits.temperature_stop_celsius = 85.0;
  limits.power_stop_watts = 20.0;
  limits.profile_hash = "sha256:fixture";
  return limits;
}

TargetIdentity valid_target() {
  TargetIdentity target;
  target.vendor = "vendor";
  target.sku = "sku";
  target.board = "board";
  target.board_revision = "r0";
  target.device_identifier = "device-0";
  target.architecture = "arm64";
  target.os = "linux";
  target.kernel_version = "6.1.0";
  target.firmware = "1.0.0";
  target.driver_version = "1.0.0";
  target.sdk_version = "1.0.0";
  target.runtime_version = "1.0.0";
  target.compiler_version = "clang-20";
  return target;
}

RunIdentity valid_run() {
  RunIdentity run;
  run.schema_version = 1;
  run.run_id = "run-1";
  run.started_utc = "2026-11-28T00:00:00Z";
  run.finished_utc = "2026-11-28T00:05:00Z";
  run.git_commit = "0123456789abcdef";
  run.compiler = "clang-20";
  run.linker = "lld";
  run.cmake_version = "4.2.3";
  run.build_flags = "Release";
  run.adapter_hash = "sha256:adapter";
  run.profile_hash = "sha256:fixture";
  run.graph_hash = "sha256:graph";
  run.source = SourceKind::real_device;
  return run;
}

ProbeResult make_probe(std::string_view id) {
  ProbeResult probe;
  probe.id = std::string(id);
  probe.state = EvidenceState::supported;
  probe.required = true;
  probe.cleanup_ok = true;
  return probe;
}

ProbeResult async_probe() {
  ProbeResult probe = make_probe(kProbeAsyncCompute);
  probe.native_async = true;
  probe.independent_correctness = true;
  probe.device_placement_verified = true;
  probe.tested_operator_families = {"gemm"};
  probe.tested_shapes = {"16x16x16"};
  probe.tested_precisions = {"fp16"};
  return probe;
}

ProbeResult attention_probe() {
  ProbeResult probe = make_probe(kProbeAttention);
  probe.bounded_attention_path = true;
  probe.bounded_gather = true;
  probe.tested_operator_families = {"attention"};
  return probe;
}

EvidenceBundle valid_bundle() {
  EvidenceBundle bundle;
  bundle.target = valid_target();
  bundle.run = valid_run();
  bundle.limits = valid_limits();
  bundle.measurement_complete = true;
  bundle.integrity = IntegrityState::pending;
  bundle.probes = {
      async_probe(),
      make_probe(kProbeMemory),
      make_probe(kProbeFence),
      make_probe(kProbeRecovery),
      [] {
        ProbeResult probe = make_probe(kProbeSustained);
        probe.sustained_complete = true;
        return probe;
      }(),
      attention_probe(),
  };
  return bundle;
}

ProbeResult *probe_named(EvidenceBundle &bundle, std::string_view id) {
  for (ProbeResult &probe : bundle.probes) {
    if (probe.id == id) {
      return &probe;
    }
  }
  return nullptr;
}

bool has_reason(const QualificationDecision &decision, std::string_view code) {
  return std::find(decision.reason_codes.begin(), decision.reason_codes.end(),
                   code) != decision.reason_codes.end();
}

bool has_reason_prefix(const QualificationDecision &decision,
                       std::string_view prefix) {
  return std::any_of(decision.reason_codes.begin(), decision.reason_codes.end(),
                     [prefix](const std::string &code) {
                       return std::string_view(code).starts_with(prefix);
                     });
}

bool has_family(const QualificationDecision &decision,
                std::string_view family) {
  return std::find(decision.p3_eligible_probe_families.begin(),
                   decision.p3_eligible_probe_families.end(),
                   family) != decision.p3_eligible_probe_families.end();
}

void complete_real_evidence_is_qualified() {
  const QualificationDecision decision = evaluate(valid_bundle());
  CHECK(decision.overall == OverallDecision::qualified);
  CHECK(decision.reason_codes.empty());
  CHECK(decision.p1_eligible);
  CHECK(decision.p2_mode == P2Mode::gather);
  CHECK(has_family(decision, "gemm"));
  CHECK(has_family(decision, "attention"));
}

void synthetic_source_is_blocked() {
  EvidenceBundle bundle = valid_bundle();
  bundle.run.source = SourceKind::synthetic;
  const QualificationDecision decision = evaluate(bundle);
  CHECK(decision.overall == OverallDecision::blocked);
  CHECK(has_reason(decision, kReasonSyntheticSource));
}

void incomplete_measurement_is_blocked() {
  EvidenceBundle bundle = valid_bundle();
  bundle.measurement_complete = false;
  const QualificationDecision decision = evaluate(bundle);
  CHECK(decision.overall == OverallDecision::blocked);
  CHECK(has_reason(decision, kReasonMeasurementIncomplete));
}

void missing_required_probe_is_blocked() {
  EvidenceBundle bundle = valid_bundle();
  bundle.probes.erase(std::remove_if(bundle.probes.begin(), bundle.probes.end(),
                                     [](const ProbeResult &probe) {
                                       return probe.id == kProbeAsyncCompute;
                                     }),
                      bundle.probes.end());
  const QualificationDecision decision = evaluate(bundle);
  CHECK(decision.overall == OverallDecision::blocked);
  CHECK(has_reason_prefix(decision, kReasonMissingProbe));
}

void unavailable_or_untested_probe_is_blocked() {
  for (const EvidenceState state :
       {EvidenceState::unavailable, EvidenceState::not_tested,
        EvidenceState::unsupported}) {
    EvidenceBundle bundle = valid_bundle();
    probe_named(bundle, kProbeMemory)->state = state;
    const QualificationDecision decision = evaluate(bundle);
    CHECK(decision.overall == OverallDecision::blocked);
    CHECK(has_reason_prefix(decision, kReasonProbeUnsupported));
    CHECK(!decision.p1_eligible);
  }
}

void failed_required_probe_fails() {
  EvidenceBundle bundle = valid_bundle();
  probe_named(bundle, kProbeFence)->state = EvidenceState::failed;
  const QualificationDecision decision = evaluate(bundle);
  CHECK(decision.overall == OverallDecision::failed);
  CHECK(has_reason_prefix(decision, kReasonProbeFailed));
}

void missing_async_semantics_blocks() {
  const std::vector<bool ProbeResult::*> flags = {
      &ProbeResult::native_async, &ProbeResult::independent_correctness,
      &ProbeResult::device_placement_verified};
  for (bool ProbeResult::*flag : flags) {
    EvidenceBundle bundle = valid_bundle();
    probe_named(bundle, kProbeAsyncCompute)->*flag = false;
    const QualificationDecision decision = evaluate(bundle);
    CHECK(decision.overall == OverallDecision::blocked);
    CHECK(!decision.p1_eligible);
  }

  EvidenceBundle bundle = valid_bundle();
  probe_named(bundle, kProbeAsyncCompute)->native_async = false;
  CHECK(has_reason(evaluate(bundle), kReasonNativeAsync));

  bundle = valid_bundle();
  probe_named(bundle, kProbeAsyncCompute)->independent_correctness = false;
  CHECK(has_reason(evaluate(bundle), kReasonIndependentCorrectness));

  bundle = valid_bundle();
  probe_named(bundle, kProbeAsyncCompute)->device_placement_verified = false;
  CHECK(has_reason(evaluate(bundle), kReasonDevicePlacement));
}

void missing_sustained_run_blocks() {
  EvidenceBundle bundle = valid_bundle();
  probe_named(bundle, kProbeSustained)->sustained_complete = false;
  const QualificationDecision decision = evaluate(bundle);
  CHECK(decision.overall == OverallDecision::blocked);
  CHECK(has_reason(decision, kReasonSustained));
}

void missing_bounded_attention_blocks() {
  EvidenceBundle bundle = valid_bundle();
  probe_named(bundle, kProbeAttention)->bounded_attention_path = false;
  const QualificationDecision decision = evaluate(bundle);
  CHECK(decision.overall == OverallDecision::blocked);
  CHECK(has_reason(decision, kReasonBoundedAttention));
  CHECK(decision.p2_mode == P2Mode::no_go);
}

void unresolved_quarantine_fails() {
  EvidenceBundle bundle = valid_bundle();
  probe_named(bundle, kProbeRecovery)->quarantine_resolved = false;
  const QualificationDecision decision = evaluate(bundle);
  CHECK(decision.overall == OverallDecision::failed);
  CHECK(has_reason(decision, kReasonQuarantine));
}

void invalid_profile_or_identity_blocks() {
  {
    EvidenceBundle bundle = valid_bundle();
    bundle.limits.device_budget_bytes = 0;
    CHECK(evaluate(bundle).overall == OverallDecision::blocked);
    CHECK(has_reason(evaluate(bundle), kReasonInvalidLimits));
  }
  {
    EvidenceBundle bundle = valid_bundle();
    bundle.target.driver_version.clear();
    CHECK(evaluate(bundle).overall == OverallDecision::blocked);
    CHECK(has_reason(evaluate(bundle), kReasonMissingTargetIdentity));
  }
  {
    EvidenceBundle bundle = valid_bundle();
    bundle.run.profile_hash = "sha256:other";
    CHECK(evaluate(bundle).overall == OverallDecision::blocked);
    CHECK(has_reason(evaluate(bundle), kReasonProfileHashMismatch));
  }
  {
    EvidenceBundle bundle = valid_bundle();
    bundle.run.schema_version = 0;
    CHECK(evaluate(bundle).overall == OverallDecision::blocked);
    CHECK(has_reason(evaluate(bundle), kReasonRunSchema));
  }
}

void duplicate_probe_ids_block() {
  EvidenceBundle bundle = valid_bundle();
  bundle.probes.push_back(make_probe(kProbeMemory));
  const QualificationDecision decision = evaluate(bundle);
  CHECK(decision.overall == OverallDecision::blocked);
  CHECK(has_reason(decision, kReasonDuplicateProbe));
}

void unknown_evidence_state_cannot_support() {
  EvidenceBundle bundle = valid_bundle();
  probe_named(bundle, kProbeAsyncCompute)->state =
      static_cast<EvidenceState>(99);
  const QualificationDecision decision = evaluate(bundle);
  CHECK(decision.overall == OverallDecision::blocked);
  CHECK(!decision.p1_eligible);
}

void optional_unsupported_collective_does_not_block() {
  EvidenceBundle bundle = valid_bundle();
  ProbeResult collective;
  collective.id = "collective/reduction";
  collective.state = EvidenceState::unsupported;
  collective.required = false;
  collective.reason_code = "no_peer_transport";
  bundle.probes.push_back(collective);
  const QualificationDecision decision = evaluate(bundle);
  CHECK(decision.overall == OverallDecision::qualified);
}

void p2_native_requires_native_paged_attention() {
  EvidenceBundle bundle = valid_bundle();
  probe_named(bundle, kProbeAttention)->native_paged_attention = true;
  CHECK(evaluate(bundle).p2_mode == P2Mode::native_paged);

  bundle = valid_bundle();
  probe_named(bundle, kProbeAttention)->bounded_gather = false;
  CHECK(evaluate(bundle).p2_mode == P2Mode::no_go);
}

void p3_only_includes_tested_families() {
  EvidenceBundle bundle = valid_bundle();
  ProbeResult unsupported = make_probe("gemm/unsupported-shape");
  unsupported.state = EvidenceState::unsupported;
  unsupported.required = false;
  unsupported.tested_operator_families = {"convolution"};
  bundle.probes.push_back(unsupported);
  const QualificationDecision decision = evaluate(bundle);
  CHECK(decision.overall == OverallDecision::qualified);
  CHECK(has_family(decision, "gemm"));
  CHECK(!has_family(decision, "convolution"));
}

void capabilities_report_supporting_evidence() {
  const QualificationDecision decision = evaluate(valid_bundle());
  CHECK(decision.capabilities.size() >= 6);
  bool saw_execution = false;
  for (const CapabilityDecision &capability : decision.capabilities) {
    if (capability.capability == "execution.async_compute") {
      saw_execution = true;
      CHECK(capability.state == EvidenceState::supported);
      CHECK(!capability.supporting_probes.empty());
    }
  }
  CHECK(saw_execution);
}

} // namespace

int main() {
  complete_real_evidence_is_qualified();
  synthetic_source_is_blocked();
  incomplete_measurement_is_blocked();
  missing_required_probe_is_blocked();
  unavailable_or_untested_probe_is_blocked();
  failed_required_probe_fails();
  missing_async_semantics_blocks();
  missing_sustained_run_blocks();
  missing_bounded_attention_blocks();
  unresolved_quarantine_fails();
  invalid_profile_or_identity_blocks();
  duplicate_probe_ids_block();
  unknown_evidence_state_cannot_support();
  optional_unsupported_collective_does_not_block();
  p2_native_requires_native_paged_attention();
  p3_only_includes_tested_families();
  capabilities_report_supporting_evidence();
  return lumen::test::summary("p0-qual-decision");
}
