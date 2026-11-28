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

#include "qualify/decision.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "qualify/profile.h"

namespace lumen::qualification {
namespace {

void add_reason(std::vector<std::string> &reasons, std::string_view code) {
  const std::string reason(code);
  if (std::find(reasons.begin(), reasons.end(), reason) == reasons.end()) {
    reasons.push_back(reason);
  }
}

std::string with_id(std::string_view code, std::string_view id) {
  return std::string(code) + ":" + std::string(id);
}

bool is_canonical_probe(std::string_view id) {
  constexpr std::string_view kCanonical[] = {
      kProbeAsyncCompute, kProbeMemory,    kProbeFence,
      kProbeRecovery,     kProbeSustained, kProbeAttention};
  return std::find(std::begin(kCanonical), std::end(kCanonical), id) !=
         std::end(kCanonical);
}

} // namespace

const ProbeResult *find_probe(const EvidenceBundle &bundle,
                              std::string_view id) {
  for (const ProbeResult &probe : bundle.probes) {
    if (probe.id == id) {
      return &probe;
    }
  }
  return nullptr;
}

QualificationDecision evaluate(const EvidenceBundle &bundle) {
  QualificationDecision decision;
  bool blocked = false;
  bool failed = false;

  const auto block = [&](std::string_view code) {
    add_reason(decision.reason_codes, code);
    blocked = true;
  };
  const auto fail = [&](std::string_view code) {
    add_reason(decision.reason_codes, code);
    failed = true;
  };

  // Frozen profile and run provenance must be usable and consistent.
  if (!validate_limits(bundle.limits).ok()) {
    block(kReasonInvalidLimits);
  }
  if (!bundle.target.complete()) {
    block(kReasonMissingTargetIdentity);
  }
  if (bundle.run.schema_version != 1) {
    block(kReasonRunSchema);
  }
  if (bundle.run.source != SourceKind::real_device) {
    block(kReasonSyntheticSource);
  }
  if (!bundle.measurement_complete) {
    block(kReasonMeasurementIncomplete);
  }
  if (bundle.run.profile_hash != bundle.limits.profile_hash) {
    block(kReasonProfileHashMismatch);
  }

  std::set<std::string> seen_ids;
  bool duplicate_probe = false;
  for (const ProbeResult &probe : bundle.probes) {
    if (!seen_ids.insert(probe.id).second) {
      duplicate_probe = true;
    }
  }
  if (duplicate_probe) {
    block(kReasonDuplicateProbe);
  }

  // Required canonical probes and their semantic obligations. Unknown enum
  // values, `unavailable`, `unsupported`, and `not_tested` never support.
  const auto require_probe = [&](std::string_view id) -> const ProbeResult * {
    const ProbeResult *probe = find_probe(bundle, id);
    if (probe == nullptr) {
      block(with_id(kReasonMissingProbe, id));
      return nullptr;
    }
    if (probe->state == EvidenceState::failed) {
      fail(with_id(kReasonProbeFailed, id));
      return nullptr;
    }
    if (probe->state != EvidenceState::supported) {
      block(with_id(kReasonProbeUnsupported, id));
      return nullptr;
    }
    if (!probe->cleanup_ok) {
      fail(with_id(kReasonCleanupFailed, id));
    }
    if (!probe->quarantine_resolved) {
      fail(kReasonQuarantine);
    }
    if (probe->correctness_mismatches != 0) {
      fail(with_id(kReasonCorrectnessMismatch, id));
    }
    return probe;
  };

  const ProbeResult *async = require_probe(kProbeAsyncCompute);
  if (async != nullptr) {
    if (!async->native_async) {
      block(kReasonNativeAsync);
    }
    if (!async->independent_correctness) {
      block(kReasonIndependentCorrectness);
    }
    if (!async->device_placement_verified) {
      block(kReasonDevicePlacement);
    }
  }

  (void)require_probe(kProbeMemory);
  (void)require_probe(kProbeFence);
  (void)require_probe(kProbeRecovery);

  const ProbeResult *sustained = require_probe(kProbeSustained);
  if (sustained != nullptr && !sustained->sustained_complete) {
    block(kReasonSustained);
  }

  const ProbeResult *attention = require_probe(kProbeAttention);
  if (attention != nullptr && !attention->bounded_attention_path) {
    block(kReasonBoundedAttention);
  }

  // Any additional required probe must also be supported and clean. Optional
  // probes (for example collectives) never block the single-NPU result alone.
  for (const ProbeResult &probe : bundle.probes) {
    if (!probe.required || is_canonical_probe(probe.id)) {
      continue;
    }
    if (probe.state == EvidenceState::failed) {
      fail(with_id(kReasonProbeFailed, probe.id));
    } else if (probe.state != EvidenceState::supported) {
      block(with_id(kReasonProbeUnsupported, probe.id));
    }
    if (!probe.cleanup_ok) {
      fail(with_id(kReasonCleanupFailed, probe.id));
    }
    if (!probe.quarantine_resolved) {
      fail(kReasonQuarantine);
    }
    if (probe.correctness_mismatches != 0) {
      fail(with_id(kReasonCorrectnessMismatch, probe.id));
    }
  }

  // P2 eligibility is derived from measured attention evidence only.
  const bool attention_usable = attention != nullptr &&
                                attention->state == EvidenceState::supported &&
                                attention->bounded_attention_path;
  if (attention_usable && attention->native_paged_attention) {
    decision.p2_mode = P2Mode::native_paged;
  } else if (attention_usable && attention->bounded_gather) {
    decision.p2_mode = P2Mode::gather;
  } else {
    decision.p2_mode = P2Mode::no_go;
  }

  // P3 remains bounded to operator families actually measured as supported.
  std::vector<std::string> families;
  for (const ProbeResult &probe : bundle.probes) {
    if (probe.state != EvidenceState::supported) {
      continue;
    }
    for (const std::string &family : probe.tested_operator_families) {
      if (std::find(families.begin(), families.end(), family) ==
          families.end()) {
        families.push_back(family);
      }
    }
  }
  std::sort(families.begin(), families.end());
  decision.p3_eligible_probe_families = std::move(families);

  const auto capability = [&](std::string name, std::string_view probe_id,
                              std::string fallback = {}) {
    CapabilityDecision entry;
    entry.capability = std::move(name);
    entry.fallback_mode = std::move(fallback);
    const ProbeResult *probe = find_probe(bundle, probe_id);
    if (probe == nullptr) {
      entry.state = EvidenceState::not_tested;
      entry.disabled_reason = "probe_missing";
    } else {
      entry.state = probe->state;
      entry.supporting_probes.push_back(probe->id);
      if (entry.state != EvidenceState::supported) {
        entry.disabled_reason = probe->reason_code.empty()
                                    ? "probe_not_supported"
                                    : probe->reason_code;
      }
    }
    decision.capabilities.push_back(std::move(entry));
  };

  capability("execution.async_compute", kProbeAsyncCompute);
  capability("memory.kinds_and_copies", kProbeMemory);
  capability("lifecycle.fence_and_ownership", kProbeFence);
  capability("recovery.partial_submission", kProbeRecovery);
  capability("thermal.sustained", kProbeSustained);
  capability("attention.bounded_path", kProbeAttention);

  if (decision.p2_mode != P2Mode::native_paged) {
    decision.limitations.emplace_back("native_paged_attention_not_qualified");
  }
  if (decision.p2_mode == P2Mode::no_go) {
    decision.limitations.emplace_back("p2_no_safe_attention_path");
  }

  if (failed) {
    decision.overall = OverallDecision::failed;
  } else if (blocked) {
    decision.overall = OverallDecision::blocked;
  } else {
    decision.overall = OverallDecision::qualified;
  }
  decision.p1_eligible = decision.overall == OverallDecision::qualified;

  return decision;
}

} // namespace lumen::qualification
