// Tests the internal status vocabulary and Result<T> contract. The enum order
// is frozen here because the public C ABI maps it to fixed numeric codes.

#include "check.h"

#include <string_view>
#include <utility>

#include "core/status.h"

using lumen::Result;
using lumen::Status;

namespace {

struct NoDefault {
  explicit NoDefault(int initial) : value(initial) {}
  NoDefault(const NoDefault &) = delete;
  NoDefault &operator=(const NoDefault &) = delete;
  NoDefault(NoDefault &&) = default;
  NoDefault &operator=(NoDefault &&) = default;
  int value;
};

} // namespace

static void status_values_are_stable() {
  CHECK(static_cast<int>(Status::ok) == 0);
  CHECK(static_cast<int>(Status::invalid_input) == 1);
  CHECK(static_cast<int>(Status::unsupported) == 2);
  CHECK(static_cast<int>(Status::resource_exhausted) == 3);
  CHECK(static_cast<int>(Status::stale) == 4);
  CHECK(static_cast<int>(Status::timeout) == 5);
  CHECK(static_cast<int>(Status::device_error) == 6);
  CHECK(static_cast<int>(Status::corrupt) == 7);
}

static void status_names_are_readable() {
  CHECK(lumen::status_name(Status::ok) == std::string_view{"ok"});
  CHECK(lumen::status_name(Status::invalid_input) ==
        std::string_view{"invalid_input"});
  CHECK(lumen::status_name(Status::unsupported) ==
        std::string_view{"unsupported"});
  CHECK(lumen::status_name(Status::resource_exhausted) ==
        std::string_view{"resource_exhausted"});
  CHECK(lumen::status_name(Status::stale) == std::string_view{"stale"});
  CHECK(lumen::status_name(Status::timeout) == std::string_view{"timeout"});
  CHECK(lumen::status_name(Status::device_error) ==
        std::string_view{"device_error"});
  CHECK(lumen::status_name(Status::corrupt) == std::string_view{"corrupt"});
}

static void result_carries_value_or_status() {
  Result<int> success = Result<int>::ok(42);
  CHECK(success.ok());
  CHECK(success.status() == Status::ok);
  CHECK(success.value() == 42);

  Result<int> failure = Result<int>::error(Status::invalid_input);
  CHECK(!failure.ok());
  CHECK(failure.status() == Status::invalid_input);
}

static void result_works_without_default_construction() {
  Result<NoDefault> result = Result<NoDefault>::ok(NoDefault{7});
  REQUIRE(result.ok());
  CHECK(result.value().value == 7);

  Result<NoDefault> moved = std::move(result);
  CHECK(moved.ok());
  CHECK(moved.value().value == 7);
}

int main() {
  status_values_are_stable();
  status_names_are_readable();
  result_carries_value_or_status();
  result_works_without_default_construction();
  return lumen::test::summary("unit-status");
}
