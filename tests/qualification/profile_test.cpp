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

// Checked qualification profile validation: zero, non-finite, and overflowing
// bounds are rejected before any device work.

#include "check.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "qualification_profile_config.h"
#include "qualify/profile.h"

namespace {

using lumen::Status;
using lumen::qualification::RunLimits;
using lumen::qualification::TargetIdentity;

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
  target.vendor = "synthetic";
  target.sku = "synthetic-sku";
  target.board = "synthetic-board";
  target.board_revision = "r0";
  target.device_identifier = "synthetic-device";
  target.architecture = "arm64";
  target.os = "synthetic-linux";
  target.kernel_version = "6.1.0";
  target.firmware = "none";
  target.driver_version = "0.0.0";
  target.sdk_version = "0.0.0";
  target.runtime_version = "0.0.0";
  target.compiler_version = "synthetic-clang";
  return target;
}

void valid_limits_are_accepted() {
  const auto result = lumen::qualification::validate_limits(valid_limits());
  CHECK(result.ok());
  CHECK(result.status() == Status::ok);
}

void zero_budgets_are_rejected() {
  const std::vector<std::uint64_t RunLimits::*> budgets = {
      &RunLimits::host_budget_bytes, &RunLimits::staging_budget_bytes,
      &RunLimits::device_budget_bytes, &RunLimits::workspace_budget_bytes};
  for (std::uint64_t RunLimits::*budget : budgets) {
    RunLimits limits = valid_limits();
    limits.*budget = 0;
    CHECK(!lumen::qualification::validate_limits(limits).ok());
  }
}

void zero_counts_are_rejected() {
  std::vector<RunLimits> cases;
  {
    RunLimits limits = valid_limits();
    limits.max_inflight_commands = 0;
    cases.push_back(limits);
  }
  {
    RunLimits limits = valid_limits();
    limits.max_graphs = 0;
    cases.push_back(limits);
  }
  {
    RunLimits limits = valid_limits();
    limits.sample_count = 0;
    cases.push_back(limits);
  }
  {
    RunLimits limits = valid_limits();
    limits.warmup_count = 0;
    cases.push_back(limits);
  }
  {
    RunLimits limits = valid_limits();
    limits.per_case_timeout_ns = 0;
    cases.push_back(limits);
  }
  {
    RunLimits limits = valid_limits();
    limits.soak_duration_ns = 0;
    cases.push_back(limits);
  }
  for (const RunLimits &limits : cases) {
    CHECK(!lumen::qualification::validate_limits(limits).ok());
  }
}

void non_finite_stops_are_rejected() {
  RunLimits limits = valid_limits();
  limits.temperature_stop_celsius = std::numeric_limits<double>::quiet_NaN();
  CHECK(!lumen::qualification::validate_limits(limits).ok());

  limits = valid_limits();
  limits.temperature_stop_celsius = std::numeric_limits<double>::infinity();
  CHECK(!lumen::qualification::validate_limits(limits).ok());

  limits = valid_limits();
  limits.power_stop_watts = std::numeric_limits<double>::infinity();
  CHECK(!lumen::qualification::validate_limits(limits).ok());

  limits = valid_limits();
  limits.temperature_stop_celsius = 0.0;
  CHECK(!lumen::qualification::validate_limits(limits).ok());

  limits = valid_limits();
  limits.power_stop_watts = 0.0;
  CHECK(!lumen::qualification::validate_limits(limits).ok());
}

void overflowing_budget_sum_is_rejected() {
  RunLimits limits = valid_limits();
  limits.host_budget_bytes = std::numeric_limits<std::uint64_t>::max();
  limits.device_budget_bytes = 1;
  CHECK(!lumen::qualification::validate_limits(limits).ok());
}

void warmup_plus_sample_overflow_is_rejected() {
  RunLimits limits = valid_limits();
  limits.warmup_count = std::numeric_limits<std::uint32_t>::max();
  limits.sample_count = 1;
  CHECK(!lumen::qualification::validate_limits(limits).ok());
}

void missing_profile_hash_is_rejected() {
  RunLimits limits = valid_limits();
  limits.profile_hash.clear();
  CHECK(!lumen::qualification::validate_limits(limits).ok());
  CHECK(lumen::qualification::validate_profile(valid_target(), limits) ==
        Status::invalid_input);
}

void complete_profile_validates() {
  CHECK(lumen::qualification::validate_profile(valid_target(),
                                               valid_limits()) == Status::ok);
}

void each_missing_identity_field_is_rejected() {
  std::vector<std::pair<std::string TargetIdentity::*, std::string>> fields = {
      {&TargetIdentity::vendor, "vendor"},
      {&TargetIdentity::sku, "sku"},
      {&TargetIdentity::board, "board"},
      {&TargetIdentity::board_revision, "board_revision"},
      {&TargetIdentity::device_identifier, "device_identifier"},
      {&TargetIdentity::architecture, "architecture"},
      {&TargetIdentity::os, "os"},
      {&TargetIdentity::kernel_version, "kernel_version"},
      {&TargetIdentity::firmware, "firmware"},
      {&TargetIdentity::driver_version, "driver_version"},
      {&TargetIdentity::sdk_version, "sdk_version"},
      {&TargetIdentity::runtime_version, "runtime_version"},
      {&TargetIdentity::compiler_version, "compiler_version"},
  };
  CHECK(fields.size() == 13);
  for (const auto &field : fields) {
    TargetIdentity target = valid_target();
    (target.*(field.first)).clear();
    CHECK(lumen::qualification::validate_profile(target, valid_limits()) ==
          Status::invalid_input);
  }
}

void generated_profile_config_matches_the_synthetic_fixture() {
  CHECK(LUMEN_QUAL_PROFILE_SCHEMA_VERSION == 1);
  CHECK(std::string_view(LUMEN_QUAL_PROFILE_SOURCE_KIND) == "synthetic");
  CHECK(std::string_view(LUMEN_QUAL_PROFILE_HASH) ==
        "sha256:synthetic-fixture-not-evidence");
  CHECK(LUMEN_QUAL_DEVICE_BUDGET_BYTES > 0);
  CHECK(LUMEN_QUAL_SAMPLE_COUNT > 0);
  CHECK(LUMEN_QUAL_SOAK_DURATION_NS > 0);
}

} // namespace

int main() {
  valid_limits_are_accepted();
  zero_budgets_are_rejected();
  zero_counts_are_rejected();
  non_finite_stops_are_rejected();
  overflowing_budget_sum_is_rejected();
  warmup_plus_sample_overflow_is_rejected();
  missing_profile_hash_is_rejected();
  complete_profile_validates();
  each_missing_identity_field_is_rejected();
  generated_profile_config_matches_the_synthetic_fixture();
  return lumen::test::summary("p0-qual-profile");
}
