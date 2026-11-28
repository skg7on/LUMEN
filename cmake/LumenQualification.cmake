# Copyright 2026 Gavin Sun
# SPDX-License-Identifier: Apache-2.0
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# SDK-free qualification support and configure-time profile compilation.
#
# The frozen profile is JSON consumed here with CMake's string(JSON). The
# executable consumes the generated C header plus checked CLI overrides; there
# is deliberately no second general-purpose JSON parser in C++.
#
# No host build or test requires a vendor SDK. Enabling an adapter and building
# the qualification executable is a separate, explicit opt-in owned by the
# target-NPU work (not part of the SDK-free harness).

set(LUMEN_QUALIFICATION_GENERATED_DIR
    "${CMAKE_BINARY_DIR}/generated/qualification"
    CACHE INTERNAL "Generated qualification profile include directory")

# Compile a profile JSON into the generated qualification_profile_config.h.
# This validates structure only; it never implies qualification or evidence.
function(lumen_qualification_configure_profile profile_path)
  if(NOT EXISTS "${profile_path}")
    message(FATAL_ERROR
            "LUMEN qualification profile does not exist: ${profile_path}")
  endif()

  file(READ "${profile_path}" profile_json)

  string(JSON schema_version GET "${profile_json}" schema_version)
  if(NOT schema_version EQUAL 1)
    message(FATAL_ERROR
            "Unsupported qualification profile schema: ${schema_version}")
  endif()

  string(JSON source_kind GET "${profile_json}" source_kind)
  string(JSON profile_hash GET "${profile_json}" profile_hash)

  string(JSON host_budget GET "${profile_json}" limits host_budget_bytes)
  string(JSON staging_budget GET "${profile_json}" limits staging_budget_bytes)
  string(JSON device_budget GET "${profile_json}" limits device_budget_bytes)
  string(JSON workspace_budget GET "${profile_json}" limits
         workspace_budget_bytes)
  string(JSON max_inflight GET "${profile_json}" limits max_inflight_commands)
  string(JSON max_graphs GET "${profile_json}" limits max_graphs)
  string(JSON sample_count GET "${profile_json}" limits sample_count)
  string(JSON warmup_count GET "${profile_json}" limits warmup_count)
  string(JSON per_case_timeout GET "${profile_json}" limits per_case_timeout_ns)
  string(JSON soak_duration GET "${profile_json}" limits soak_duration_ns)
  string(JSON temperature_stop GET "${profile_json}" limits
         temperature_stop_celsius)
  string(JSON power_stop GET "${profile_json}" limits power_stop_watts)

  set(LUMEN_QUAL_PROFILE_SCHEMA_VERSION "${schema_version}")
  set(LUMEN_QUAL_PROFILE_SOURCE_KIND "${source_kind}")
  set(LUMEN_QUAL_PROFILE_HASH "${profile_hash}")
  set(LUMEN_QUAL_HOST_BUDGET_BYTES "${host_budget}")
  set(LUMEN_QUAL_STAGING_BUDGET_BYTES "${staging_budget}")
  set(LUMEN_QUAL_DEVICE_BUDGET_BYTES "${device_budget}")
  set(LUMEN_QUAL_WORKSPACE_BUDGET_BYTES "${workspace_budget}")
  set(LUMEN_QUAL_MAX_INFLIGHT_COMMANDS "${max_inflight}")
  set(LUMEN_QUAL_MAX_GRAPHS "${max_graphs}")
  set(LUMEN_QUAL_SAMPLE_COUNT "${sample_count}")
  set(LUMEN_QUAL_WARMUP_COUNT "${warmup_count}")
  set(LUMEN_QUAL_PER_CASE_TIMEOUT_NS "${per_case_timeout}")
  set(LUMEN_QUAL_SOAK_DURATION_NS "${soak_duration}")
  set(LUMEN_QUAL_TEMPERATURE_STOP_CELSIUS "${temperature_stop}")
  set(LUMEN_QUAL_POWER_STOP_WATTS "${power_stop}")

  file(MAKE_DIRECTORY "${LUMEN_QUALIFICATION_GENERATED_DIR}")
  configure_file(
    "${PROJECT_SOURCE_DIR}/tools/qualify/profile_config.h.in"
    "${LUMEN_QUALIFICATION_GENERATED_DIR}/qualification_profile_config.h"
    @ONLY)
endfunction()

# Build the SDK-free qualification support library. Safe to call repeatedly.
function(lumen_add_qualification_support)
  if(TARGET lumen_qualification_support)
    return()
  endif()

  add_library(lumen_qualification_support STATIC
    ${PROJECT_SOURCE_DIR}/tools/qualify/types.cpp
    ${PROJECT_SOURCE_DIR}/tools/qualify/profile.cpp
    ${PROJECT_SOURCE_DIR}/tools/qualify/decision.cpp
    ${PROJECT_SOURCE_DIR}/tools/qualify/lifecycle.cpp
    ${PROJECT_SOURCE_DIR}/src/backend/qualification/qualification_api.cpp)

  target_include_directories(lumen_qualification_support
    PUBLIC
      ${PROJECT_SOURCE_DIR}/tools
      ${PROJECT_SOURCE_DIR}/src
      ${PROJECT_SOURCE_DIR}/include
      ${LUMEN_QUALIFICATION_GENERATED_DIR})
  target_link_libraries(lumen_qualification_support PUBLIC lumen::lumen)
  target_compile_features(lumen_qualification_support PUBLIC cxx_std_20)
  lumen_apply_warnings(lumen_qualification_support)
endfunction()
