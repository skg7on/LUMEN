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

inline void report_failure(const char* expression, const char* file, int line) {
  std::fprintf(stderr, "FAIL %s:%d: %s\n", file, line, expression);
  ++failures;
}

inline int summary(const char* suite) {
  if (failures == 0) {
    std::fprintf(stderr, "PASS %s (%d checks)\n", suite, checks);
    return 0;
  }
  std::fprintf(stderr, "FAILED %s (%d failures / %d checks)\n", suite, failures, checks);
  return 1;
}

}  // namespace lumen::test

#define CHECK(expr)                                                      \
  do {                                                                   \
    ++::lumen::test::checks;                                             \
    if (!(expr)) {                                                       \
      ::lumen::test::report_failure(#expr, __FILE__, __LINE__);          \
    }                                                                    \
  } while (false)

#define REQUIRE(expr)                                                    \
  do {                                                                   \
    ++::lumen::test::checks;                                             \
    if (!(expr)) {                                                       \
      ::lumen::test::report_failure(#expr, __FILE__, __LINE__);          \
      return;                                                            \
    }                                                                    \
  } while (false)
