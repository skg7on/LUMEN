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
