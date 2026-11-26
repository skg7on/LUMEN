/*
 * Copyright 2026 Gavin Sun
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// CPU adapter lifecycle: opaque handle ownership, failed-call side effects,
// buffer/queue/device retirement order, reset semantics, and the event
// contract.

#include "check.h"

#include <cstdint>
#include <cstring>

#include "backend/mock/mock_control.h"
#include "lumen/backend.h"

namespace {

const lumen_backend_api_v1 *cpu_api() { return lumen_cpu_backend_api(); }

template <typename T> T sentinel_handle() {
  return reinterpret_cast<T>(static_cast<std::uintptr_t>(0x1));
}

lumen_buffer_desc_v1 host_buffer_desc(std::uint64_t size,
                                      std::uint32_t alignment = 64) {
  lumen_buffer_desc_v1 desc{};
  desc.struct_size = sizeof(desc);
  desc.memory_kind = LUMEN_MEMORY_HOST;
  desc.size_bytes = size;
  desc.alignment = alignment;
  return desc;
}

lumen_queue_desc_v1 queue_desc(std::uint64_t max_inflight = 8) {
  lumen_queue_desc_v1 desc{};
  desc.struct_size = sizeof(desc);
  desc.max_inflight = max_inflight;
  return desc;
}

lumen_event_state_v1 event_state() {
  lumen_event_state_v1 state{};
  state.struct_size = sizeof(state);
  state.abi_major = LUMEN_BACKEND_ABI_MAJOR;
  state.abi_minor = LUMEN_BACKEND_ABI_MINOR;
  return state;
}

lumen_capabilities_v1 capabilities() {
  lumen_capabilities_v1 caps{};
  caps.struct_size = sizeof(caps);
  caps.abi_major = LUMEN_BACKEND_ABI_MAJOR;
  caps.abi_minor = LUMEN_BACKEND_ABI_MINOR;
  return caps;
}

// Small scope guard for tests that expect successful teardown.
struct Fixture {
  const lumen_backend_api_v1 *api = cpu_api();
  lumen_context_t context = nullptr;
  lumen_device_t device = nullptr;

  bool open() {
    if (api->backend_open(api, &context) != LUMEN_STATUS_OK) {
      return false;
    }
    return api->device_open(context, &device) == LUMEN_STATUS_OK;
  }

  ~Fixture() {
    if (device != nullptr) {
      api->device_close(device);
    }
    if (context != nullptr) {
      api->context_close(context);
    }
  }
};

} // namespace

static void capabilities_describe_the_cpu_adapter() {
  Fixture fixture;
  REQUIRE(fixture.open());

  lumen_capabilities_v1 caps = capabilities();
  REQUIRE(fixture.api->device_capabilities(fixture.device, &caps) ==
          LUMEN_STATUS_OK);
  CHECK((caps.flags & LUMEN_CAP_HOST_MEMORY) != 0u);
  CHECK((caps.flags & LUMEN_CAP_DEVICE_MEMORY) == 0u);
  CHECK(caps.queue_order == LUMEN_QUEUE_ORDER_IN_ORDER);
  CHECK(caps.supports_reset == 1u);
  CHECK(caps.max_buffer_bytes > 0u);
  CHECK(caps.max_inflight_commands > 0u);
  CHECK(caps.memory_alignment_min >= 1u);

  lumen_capabilities_v1 wrong_size = capabilities();
  wrong_size.struct_size = static_cast<std::uint32_t>(sizeof(wrong_size) - 4);
  const lumen_capabilities_v1 wrong_size_before = wrong_size;
  CHECK(fixture.api->device_capabilities(fixture.device, &wrong_size) ==
        LUMEN_STATUS_INVALID_INPUT);
  CHECK(std::memcmp(&wrong_size, &wrong_size_before, sizeof(wrong_size)) == 0);

  lumen_capabilities_v1 wrong_version = capabilities();
  wrong_version.abi_major = 2;
  const lumen_capabilities_v1 wrong_version_before = wrong_version;
  CHECK(fixture.api->device_capabilities(fixture.device, &wrong_version) ==
        LUMEN_STATUS_UNSUPPORTED);
  CHECK(std::memcmp(&wrong_version, &wrong_version_before,
                    sizeof(wrong_version)) == 0);

  CHECK(fixture.api->device_capabilities(fixture.device, nullptr) ==
        LUMEN_STATUS_INVALID_INPUT);
}

static void device_close_requires_retired_children() {
  Fixture fixture;
  REQUIRE(fixture.open());

  lumen_buffer_desc_v1 desc = host_buffer_desc(128);
  lumen_buffer_t buffer = nullptr;
  REQUIRE(fixture.api->buffer_create(fixture.device, &desc, &buffer) ==
          LUMEN_STATUS_OK);

  CHECK(fixture.api->device_close(fixture.device) ==
        LUMEN_STATUS_INVALID_INPUT);

  void *data = nullptr;
  std::uint64_t size = 0;
  CHECK(fixture.api->buffer_map(buffer, &data, &size) == LUMEN_STATUS_OK);
  CHECK(data != nullptr);
  CHECK(size == 128);

  CHECK(fixture.api->buffer_destroy(buffer) == LUMEN_STATUS_OK);
  CHECK(fixture.api->buffer_destroy(buffer) == LUMEN_STATUS_STALE);
  CHECK(fixture.api->device_close(fixture.device) == LUMEN_STATUS_OK);
  CHECK(fixture.api->device_close(fixture.device) == LUMEN_STATUS_STALE);

  fixture.device = nullptr;
  CHECK(fixture.api->context_close(fixture.context) == LUMEN_STATUS_OK);
  CHECK(fixture.api->context_close(fixture.context) == LUMEN_STATUS_STALE);
  fixture.context = nullptr;
}

static void buffer_failures_do_not_touch_outputs() {
  Fixture fixture;
  REQUIRE(fixture.open());

  lumen_capabilities_v1 caps = capabilities();
  REQUIRE(fixture.api->device_capabilities(fixture.device, &caps) ==
          LUMEN_STATUS_OK);

  lumen_buffer_t out = sentinel_handle<lumen_buffer_t>();

  CHECK(fixture.api->buffer_create(fixture.device, nullptr, &out) ==
        LUMEN_STATUS_INVALID_INPUT);
  CHECK(out == sentinel_handle<lumen_buffer_t>());

  lumen_buffer_desc_v1 desc = host_buffer_desc(64);
  desc.struct_size = static_cast<std::uint32_t>(sizeof(desc) - 8);
  CHECK(fixture.api->buffer_create(fixture.device, &desc, &out) ==
        LUMEN_STATUS_INVALID_INPUT);
  CHECK(out == sentinel_handle<lumen_buffer_t>());

  desc = host_buffer_desc(64);
  desc.alignment = 3;
  CHECK(fixture.api->buffer_create(fixture.device, &desc, &out) ==
        LUMEN_STATUS_INVALID_INPUT);
  CHECK(out == sentinel_handle<lumen_buffer_t>());

  desc = host_buffer_desc(0);
  CHECK(fixture.api->buffer_create(fixture.device, &desc, &out) ==
        LUMEN_STATUS_INVALID_INPUT);
  CHECK(out == sentinel_handle<lumen_buffer_t>());

  desc = host_buffer_desc(64);
  desc.memory_kind = LUMEN_MEMORY_DEVICE;
  CHECK(fixture.api->buffer_create(fixture.device, &desc, &out) ==
        LUMEN_STATUS_UNSUPPORTED);
  CHECK(out == sentinel_handle<lumen_buffer_t>());

  desc = host_buffer_desc(caps.max_buffer_bytes + 1);
  CHECK(fixture.api->buffer_create(fixture.device, &desc, &out) ==
        LUMEN_STATUS_RESOURCE_EXHAUSTED);
  CHECK(out == sentinel_handle<lumen_buffer_t>());

  CHECK(fixture.api->buffer_create(fixture.device, &desc, nullptr) ==
        LUMEN_STATUS_INVALID_INPUT);

  desc = host_buffer_desc(64);
  lumen_buffer_t buffer = nullptr;
  REQUIRE(fixture.api->buffer_create(fixture.device, &desc, &buffer) ==
          LUMEN_STATUS_OK);
  CHECK(fixture.api->buffer_map(buffer, nullptr, nullptr) ==
        LUMEN_STATUS_INVALID_INPUT);

  void *mapped = nullptr;
  std::uint64_t mapped_size = 0;
  CHECK(fixture.api->buffer_map(buffer, &mapped, &mapped_size) ==
        LUMEN_STATUS_OK);
  CHECK(mapped != nullptr);
  CHECK(mapped_size == 64);
  CHECK(fixture.api->buffer_unmap(buffer) == LUMEN_STATUS_OK);

  CHECK(fixture.api->buffer_destroy(buffer) == LUMEN_STATUS_OK);
  CHECK(fixture.api->buffer_unmap(buffer) == LUMEN_STATUS_STALE);
}

static void reset_requires_no_live_resources() {
  Fixture fixture;
  REQUIRE(fixture.open());

  CHECK(fixture.api->context_reset(fixture.context) ==
        LUMEN_STATUS_INVALID_INPUT);

  CHECK(fixture.api->device_close(fixture.device) == LUMEN_STATUS_OK);
  fixture.device = nullptr;

  CHECK(fixture.api->context_reset(fixture.context) == LUMEN_STATUS_OK);

  CHECK(lumen_mock_advance_ticks(fixture.context, 1) ==
        LUMEN_STATUS_INVALID_INPUT);
}

static void cpu_events_complete_at_submission_and_copy_bytes() {
  Fixture fixture;
  REQUIRE(fixture.open());

  lumen_queue_desc_v1 qdesc = queue_desc();
  lumen_queue_t queue = nullptr;
  REQUIRE(fixture.api->queue_create(fixture.device, &qdesc, &queue) ==
          LUMEN_STATUS_OK);

  lumen_buffer_desc_v1 bdesc = host_buffer_desc(64);
  lumen_buffer_t src = nullptr;
  lumen_buffer_t dst = nullptr;
  REQUIRE(fixture.api->buffer_create(fixture.device, &bdesc, &src) ==
          LUMEN_STATUS_OK);
  REQUIRE(fixture.api->buffer_create(fixture.device, &bdesc, &dst) ==
          LUMEN_STATUS_OK);

  void *src_data = nullptr;
  void *dst_data = nullptr;
  std::uint64_t src_size = 0;
  std::uint64_t dst_size = 0;
  REQUIRE(fixture.api->buffer_map(src, &src_data, &src_size) ==
          LUMEN_STATUS_OK);
  REQUIRE(fixture.api->buffer_map(dst, &dst_data, &dst_size) ==
          LUMEN_STATUS_OK);
  for (std::uint64_t i = 0; i < src_size; ++i) {
    static_cast<std::uint8_t *>(src_data)[i] =
        static_cast<std::uint8_t>(i * 3 + 1);
  }
  std::memset(dst_data, 0, dst_size);

  lumen_command_v1 command{};
  command.struct_size = sizeof(command);
  command.kind = LUMEN_COMMAND_COPY;
  command.src = src;
  command.dst = dst;
  command.length = 64;

  lumen_event_t event = nullptr;
  REQUIRE(fixture.api->queue_submit(queue, &command, &event) ==
          LUMEN_STATUS_OK);
  REQUIRE(event != nullptr);

  lumen_event_state_v1 state = event_state();
  REQUIRE(fixture.api->event_poll(event, &state) == LUMEN_STATUS_OK);
  CHECK(state.status == LUMEN_EVENT_COMPLETE);
  CHECK(state.command_status == LUMEN_STATUS_OK);
  CHECK(std::memcmp(src_data, dst_data, 64) == 0);

  // Fill a prefix and leave the remainder untouched.
  command.kind = LUMEN_COMMAND_FILL;
  command.src = nullptr;
  command.length = 16;
  command.fill_byte = 0xAB;
  lumen_event_t fill_event = nullptr;
  REQUIRE(fixture.api->queue_submit(queue, &command, &fill_event) ==
          LUMEN_STATUS_OK);
  CHECK(static_cast<std::uint8_t *>(dst_data)[0] == 0xAB);
  CHECK(static_cast<std::uint8_t *>(dst_data)[15] == 0xAB);
  CHECK(static_cast<std::uint8_t *>(dst_data)[16] != 0xAB);
  CHECK(fixture.api->event_release(fill_event) == LUMEN_STATUS_OK);

  // A completed event is a fence: the queue and buffers may retire before
  // event release, but the device still owns the unreleased event.
  CHECK(fixture.api->queue_destroy(queue) == LUMEN_STATUS_OK);
  CHECK(fixture.api->queue_destroy(queue) == LUMEN_STATUS_STALE);
  CHECK(fixture.api->buffer_destroy(src) == LUMEN_STATUS_OK);
  CHECK(fixture.api->buffer_destroy(dst) == LUMEN_STATUS_OK);
  CHECK(fixture.api->device_close(fixture.device) ==
        LUMEN_STATUS_INVALID_INPUT);

  CHECK(fixture.api->event_release(event) == LUMEN_STATUS_OK);
  CHECK(fixture.api->event_release(event) == LUMEN_STATUS_STALE);

  state = event_state();
  CHECK(fixture.api->event_poll(event, &state) == LUMEN_STATUS_STALE);

  CHECK(fixture.api->device_close(fixture.device) == LUMEN_STATUS_OK);
  fixture.device = nullptr;
}

static void event_records_reject_bad_versions_and_sizes() {
  Fixture fixture;
  REQUIRE(fixture.open());

  lumen_queue_desc_v1 qdesc = queue_desc();
  lumen_queue_t queue = nullptr;
  REQUIRE(fixture.api->queue_create(fixture.device, &qdesc, &queue) ==
          LUMEN_STATUS_OK);

  lumen_command_v1 command{};
  command.struct_size = sizeof(command);
  command.kind = LUMEN_COMMAND_NOP;

  lumen_event_t event = nullptr;
  REQUIRE(fixture.api->queue_submit(queue, &command, &event) ==
          LUMEN_STATUS_OK);

  lumen_event_state_v1 wrong_size = event_state();
  wrong_size.struct_size = static_cast<std::uint32_t>(sizeof(wrong_size) - 4);
  wrong_size.status = 0xDEADBEEFu;
  CHECK(fixture.api->event_poll(event, &wrong_size) ==
        LUMEN_STATUS_INVALID_INPUT);
  CHECK(wrong_size.status == 0xDEADBEEFu);

  lumen_event_state_v1 wrong_version = event_state();
  wrong_version.abi_major = 9;
  CHECK(fixture.api->event_poll(event, &wrong_version) ==
        LUMEN_STATUS_UNSUPPORTED);

  CHECK(fixture.api->event_poll(event, nullptr) == LUMEN_STATUS_INVALID_INPUT);

  CHECK(fixture.api->event_release(event) == LUMEN_STATUS_OK);
}

static void submit_rejects_malformed_commands() {
  Fixture fixture;
  REQUIRE(fixture.open());

  lumen_queue_desc_v1 qdesc = queue_desc();
  lumen_queue_t queue = nullptr;
  REQUIRE(fixture.api->queue_create(fixture.device, &qdesc, &queue) ==
          LUMEN_STATUS_OK);

  lumen_buffer_desc_v1 bdesc = host_buffer_desc(32);
  lumen_buffer_t buffer = nullptr;
  REQUIRE(fixture.api->buffer_create(fixture.device, &bdesc, &buffer) ==
          LUMEN_STATUS_OK);

  lumen_event_t out = sentinel_handle<lumen_event_t>();

  CHECK(fixture.api->queue_submit(queue, nullptr, &out) ==
        LUMEN_STATUS_INVALID_INPUT);
  CHECK(out == sentinel_handle<lumen_event_t>());

  lumen_command_v1 command{};
  command.struct_size = static_cast<std::uint32_t>(sizeof(command) - 8);
  command.kind = LUMEN_COMMAND_NOP;
  CHECK(fixture.api->queue_submit(queue, &command, &out) ==
        LUMEN_STATUS_INVALID_INPUT);
  CHECK(out == sentinel_handle<lumen_event_t>());

  command = lumen_command_v1{};
  command.struct_size = sizeof(command);
  command.kind = 99;
  CHECK(fixture.api->queue_submit(queue, &command, &out) ==
        LUMEN_STATUS_INVALID_INPUT);

  command = lumen_command_v1{};
  command.struct_size = sizeof(command);
  command.kind = LUMEN_COMMAND_COPY;
  command.src = buffer;
  command.dst = buffer;
  command.length = 0;
  CHECK(fixture.api->queue_submit(queue, &command, &out) ==
        LUMEN_STATUS_INVALID_INPUT);

  command.length = 33;
  CHECK(fixture.api->queue_submit(queue, &command, &out) ==
        LUMEN_STATUS_INVALID_INPUT);

  command.length = 8;
  command.src_offset = 32;
  CHECK(fixture.api->queue_submit(queue, &command, &out) ==
        LUMEN_STATUS_INVALID_INPUT);

  command.src_offset = 0;
  command.src = nullptr;
  CHECK(fixture.api->queue_submit(queue, &command, &out) ==
        LUMEN_STATUS_INVALID_INPUT);

  command.src = buffer;
  command.dst = nullptr;
  CHECK(fixture.api->queue_submit(queue, &command, &out) ==
        LUMEN_STATUS_INVALID_INPUT);
  CHECK(out == sentinel_handle<lumen_event_t>());

  CHECK(fixture.api->queue_submit(queue, &command, nullptr) ==
        LUMEN_STATUS_INVALID_INPUT);

  CHECK(fixture.api->queue_destroy(queue) == LUMEN_STATUS_OK);
  CHECK(fixture.api->buffer_destroy(buffer) == LUMEN_STATUS_OK);
}

static void context_close_rejects_live_children() {
  Fixture fixture;
  REQUIRE(fixture.open());

  CHECK(fixture.api->context_close(fixture.context) ==
        LUMEN_STATUS_INVALID_INPUT);

  lumen_capabilities_v1 caps = capabilities();
  CHECK(fixture.api->device_capabilities(fixture.device, &caps) ==
        LUMEN_STATUS_OK);

  CHECK(fixture.api->device_close(fixture.device) == LUMEN_STATUS_OK);
  fixture.device = nullptr;
}

int main() {
  capabilities_describe_the_cpu_adapter();
  device_close_requires_retired_children();
  buffer_failures_do_not_touch_outputs();
  reset_requires_no_live_resources();
  cpu_events_complete_at_submission_and_copy_bytes();
  event_records_reject_bad_versions_and_sizes();
  submit_rejects_malformed_commands();
  context_close_rejects_live_children();
  return lumen::test::summary("backend-lifetime");
}
