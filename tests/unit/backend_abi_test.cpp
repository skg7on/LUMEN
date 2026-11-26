// Public ABI contract: numeric status mapping, C header usability, and
// rejection of incompatible function-table versions and sizes.

#include "check.h"

#include <cstdint>

#include "core/status.h"
#include "lumen/backend.h"

static_assert(static_cast<int>(lumen::Status::ok) == LUMEN_STATUS_OK);
static_assert(static_cast<int>(lumen::Status::invalid_input) ==
              LUMEN_STATUS_INVALID_INPUT);
static_assert(static_cast<int>(lumen::Status::unsupported) ==
              LUMEN_STATUS_UNSUPPORTED);
static_assert(static_cast<int>(lumen::Status::resource_exhausted) ==
              LUMEN_STATUS_RESOURCE_EXHAUSTED);
static_assert(static_cast<int>(lumen::Status::stale) == LUMEN_STATUS_STALE);
static_assert(static_cast<int>(lumen::Status::timeout) == LUMEN_STATUS_TIMEOUT);
static_assert(static_cast<int>(lumen::Status::device_error) ==
              LUMEN_STATUS_DEVICE_ERROR);
static_assert(static_cast<int>(lumen::Status::corrupt) == LUMEN_STATUS_CORRUPT);

namespace {

lumen_context_t sentinel_context() {
  return reinterpret_cast<lumen_context_t>(static_cast<std::uintptr_t>(0x1));
}

} // namespace

static void status_codes_match_the_internal_vocabulary() {
  CHECK(static_cast<int>(LUMEN_STATUS_OK) == 0);
  CHECK(static_cast<int>(LUMEN_STATUS_INVALID_INPUT) == 1);
  CHECK(static_cast<int>(LUMEN_STATUS_UNSUPPORTED) == 2);
  CHECK(static_cast<int>(LUMEN_STATUS_RESOURCE_EXHAUSTED) == 3);
  CHECK(static_cast<int>(LUMEN_STATUS_STALE) == 4);
  CHECK(static_cast<int>(LUMEN_STATUS_TIMEOUT) == 5);
  CHECK(static_cast<int>(LUMEN_STATUS_DEVICE_ERROR) == 6);
  CHECK(static_cast<int>(LUMEN_STATUS_CORRUPT) == 7);
}

static void valid_tables_pass_validation() {
  const lumen_backend_api_v1 *cpu = lumen_cpu_backend_api();
  const lumen_backend_api_v1 *mock = lumen_mock_backend_api();
  REQUIRE(cpu != nullptr);
  REQUIRE(mock != nullptr);

  CHECK(cpu->struct_size == sizeof(lumen_backend_api_v1));
  CHECK(cpu->abi_major == LUMEN_BACKEND_ABI_MAJOR);
  CHECK(cpu->abi_minor == LUMEN_BACKEND_ABI_MINOR);
  CHECK(mock->struct_size == sizeof(lumen_backend_api_v1));
  CHECK(mock->abi_major == LUMEN_BACKEND_ABI_MAJOR);

  CHECK(lumen_backend_api_validate(cpu) == LUMEN_STATUS_OK);
  CHECK(lumen_backend_api_validate(mock) == LUMEN_STATUS_OK);
  CHECK(lumen_backend_api_validate(nullptr) == LUMEN_STATUS_INVALID_INPUT);
}

static void incompatible_version_and_size_are_rejected() {
  const lumen_backend_api_v1 good = *lumen_cpu_backend_api();

  lumen_backend_api_v1 api = good;
  api.abi_major = 2;
  CHECK(lumen_backend_api_validate(&api) == LUMEN_STATUS_UNSUPPORTED);

  api = good;
  api.abi_minor = 1;
  CHECK(lumen_backend_api_validate(&api) == LUMEN_STATUS_UNSUPPORTED);

  api = good;
  api.struct_size = static_cast<uint32_t>(sizeof(lumen_backend_api_v1) - 8);
  CHECK(lumen_backend_api_validate(&api) == LUMEN_STATUS_UNSUPPORTED);

  api = good;
  api.struct_size = static_cast<uint32_t>(sizeof(lumen_backend_api_v1) + 8);
  CHECK(lumen_backend_api_validate(&api) == LUMEN_STATUS_UNSUPPORTED);

  api = good;
  api.event_release = nullptr;
  CHECK(lumen_backend_api_validate(&api) == LUMEN_STATUS_UNSUPPORTED);

  api = good;
  api.backend_open = nullptr;
  CHECK(lumen_backend_api_validate(&api) == LUMEN_STATUS_UNSUPPORTED);
}

static void backend_open_checks_the_supplied_table() {
  lumen_backend_api_v1 api = *lumen_cpu_backend_api();
  api.abi_major = 3;
  lumen_context_t context = sentinel_context();

  CHECK(api.backend_open(&api, &context) == LUMEN_STATUS_UNSUPPORTED);
  CHECK(context == sentinel_context());

  lumen_context_t null_out = nullptr;
  api = *lumen_cpu_backend_api();
  CHECK(api.backend_open(&api, &null_out) == LUMEN_STATUS_OK);
  REQUIRE(null_out != nullptr);
  CHECK(api.context_close(null_out) == LUMEN_STATUS_OK);
}

int main() {
  status_codes_match_the_internal_vocabulary();
  valid_tables_pass_validation();
  incompatible_version_and_size_are_rejected();
  backend_open_checks_the_supplied_table();
  return lumen::test::summary("unit-backend-abi");
}
