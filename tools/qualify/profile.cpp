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

#include "qualify/profile.h"

#include <cmath>
#include <cstdint>
#include <limits>

namespace lumen::qualification {
namespace {

bool positive_finite(double value) {
  return std::isfinite(value) && value > 0.0;
}

} // namespace

Result<RunLimits> validate_limits(const RunLimits &limits) {
  if (limits.host_budget_bytes == 0 || limits.staging_budget_bytes == 0 ||
      limits.device_budget_bytes == 0 || limits.workspace_budget_bytes == 0) {
    return Result<RunLimits>::error(Status::invalid_input);
  }
  if (limits.max_inflight_commands == 0 || limits.max_graphs == 0 ||
      limits.sample_count == 0 || limits.warmup_count == 0) {
    return Result<RunLimits>::error(Status::invalid_input);
  }
  if (limits.per_case_timeout_ns == 0 || limits.soak_duration_ns == 0) {
    return Result<RunLimits>::error(Status::invalid_input);
  }
  if (!positive_finite(limits.temperature_stop_celsius) ||
      !positive_finite(limits.power_stop_watts)) {
    return Result<RunLimits>::error(Status::invalid_input);
  }
  if (limits.profile_hash.empty()) {
    return Result<RunLimits>::error(Status::invalid_input);
  }

  // The combined host/staging/device/workspace envelope must be representable
  // so downstream byte arithmetic cannot wrap.
  constexpr std::uint64_t kMax = (std::numeric_limits<std::uint64_t>::max)();
  std::uint64_t total = 0;
  const std::uint64_t budgets[] = {
      limits.host_budget_bytes, limits.staging_budget_bytes,
      limits.device_budget_bytes, limits.workspace_budget_bytes};
  for (const std::uint64_t budget : budgets) {
    if (budget > kMax - total) {
      return Result<RunLimits>::error(Status::invalid_input);
    }
    total += budget;
  }

  // Warmup and measured counts are summed for reporting; reject wrap.
  constexpr std::uint32_t kMaxCount =
      (std::numeric_limits<std::uint32_t>::max)();
  if (limits.warmup_count > kMaxCount - limits.sample_count) {
    return Result<RunLimits>::error(Status::invalid_input);
  }

  return Result<RunLimits>::ok(limits);
}

Status validate_profile(const TargetIdentity &target, const RunLimits &limits) {
  if (!target.complete()) {
    return Status::invalid_input;
  }
  const Result<RunLimits> checked = validate_limits(limits);
  return checked.ok() ? Status::ok : checked.status();
}

} // namespace lumen::qualification
