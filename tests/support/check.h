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

// Minimal native test harness for LUMEN P0.
//
// Assertions are always active: this header never consults NDEBUG and the build
// never defines it for test targets. A failed CHECK records the failure and
// continues so one run reports every broken assertion; REQUIRE records the
// failure and abandons the current test function.

#pragma once

#include <cstdio>

namespace lumen::test {

inline int failures = 0;
inline int checks = 0;

inline void report_failure(const char *expression, const char *file, int line) {
  std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expression);
  ++failures;
}

inline int summary(const char *suite) {
  if (failures == 0) {
    std::fprintf(stderr, "PASS %s (%d checks)\n", suite, checks);
    return 0;
  }
  std::fprintf(stderr, "FAILED %s (%d failures / %d checks)\n", suite, failures,
               checks);
  return 1;
}

} // namespace lumen::test

#define CHECK(expr)                                                            \
  do {                                                                         \
    ++::lumen::test::checks;                                                   \
    if (!(expr)) {                                                             \
      ::lumen::test::report_failure(#expr, __FILE__, __LINE__);                \
    }                                                                          \
  } while (false)

#define REQUIRE(expr)                                                          \
  do {                                                                         \
    ++::lumen::test::checks;                                                   \
    if (!(expr)) {                                                             \
      ::lumen::test::report_failure(#expr, __FILE__, __LINE__);                \
      return;                                                                  \
    }                                                                          \
  } while (false)
