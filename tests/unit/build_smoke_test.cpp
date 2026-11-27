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

// Verifies the host build configuration itself: C++20 is selected, the test
// harness stays active, and the target is a 64-bit host like the ABI assumes.

#include "check.h"

#if __cplusplus < 202002L
#error "LUMEN host code requires C++20"
#endif

static void host_configuration_is_cxx20_and_active() {
  CHECK(__cplusplus >= 202002L);
  CHECK(sizeof(void *) == 8);
  CHECK(sizeof(unsigned long long) == 8);
}

static void harness_reports_checks() {
  const int before = lumen::test::checks;
  CHECK(true);
  const int after = lumen::test::checks;
  CHECK(after == before + 1);
}

int main() {
  host_configuration_is_cxx20_and_active();
  harness_reports_checks();
  return lumen::test::summary("unit-build-smoke");
}
