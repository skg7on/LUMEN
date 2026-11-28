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

#include "qualify/types.h"

namespace lumen::qualification {

std::optional<EvidenceState>
parse_evidence_state(std::string_view text) noexcept {
  if (text == "supported") {
    return EvidenceState::supported;
  }
  if (text == "unsupported") {
    return EvidenceState::unsupported;
  }
  if (text == "unavailable") {
    return EvidenceState::unavailable;
  }
  if (text == "failed") {
    return EvidenceState::failed;
  }
  if (text == "not_tested") {
    return EvidenceState::not_tested;
  }
  return std::nullopt;
}

std::optional<SourceKind> parse_source_kind(std::string_view text) noexcept {
  if (text == "real_device") {
    return SourceKind::real_device;
  }
  if (text == "synthetic") {
    return SourceKind::synthetic;
  }
  return std::nullopt;
}

std::vector<std::string> TargetIdentity::missing_fields() const {
  std::vector<std::string> missing;
  const auto require = [&missing](const std::string &value, const char *name) {
    if (value.empty()) {
      missing.emplace_back(name);
    }
  };
  require(vendor, "vendor");
  require(sku, "sku");
  require(board, "board");
  require(board_revision, "board_revision");
  require(device_identifier, "device_identifier");
  require(architecture, "architecture");
  require(os, "os");
  require(kernel_version, "kernel_version");
  require(firmware, "firmware");
  require(driver_version, "driver_version");
  require(sdk_version, "sdk_version");
  require(runtime_version, "runtime_version");
  require(compiler_version, "compiler_version");
  return missing;
}

} // namespace lumen::qualification
