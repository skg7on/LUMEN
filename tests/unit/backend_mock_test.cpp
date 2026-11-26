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

// Mock adapter: deterministic delayed completion, in-order queues, fenced
// buffer/queue retirement, and reset semantics under simulated time.

#include "check.h"

#include <cstdint>
#include <cstring>
#include <limits>

#include "backend/mock/mock_control.h"
#include "lumen/backend.h"

namespace {

const lumen_backend_api_v1 *mock_api() { return lumen_mock_backend_api(); }

lumen_buffer_desc_v1 host_buffer_desc(std::uint64_t size) {
  lumen_buffer_desc_v1 desc{};
  desc.struct_size = sizeof(desc);
  desc.memory_kind = LUMEN_MEMORY_HOST;
  desc.size_bytes = size;
  desc.alignment = 64;
  return desc;
}

lumen_queue_desc_v1 queue_desc() {
  lumen_queue_desc_v1 desc{};
  desc.struct_size = sizeof(desc);
  desc.max_inflight = 8;
  return desc;
}

lumen_event_state_v1 event_state() {
  lumen_event_state_v1 state{};
  state.struct_size = sizeof(state);
  state.abi_major = LUMEN_BACKEND_ABI_MAJOR;
  state.abi_minor = LUMEN_BACKEND_ABI_MINOR;
  return state;
}

lumen_command_v1 copy_command(lumen_buffer_t src, lumen_buffer_t dst,
                              std::uint64_t length) {
  lumen_command_v1 command{};
  command.struct_size = sizeof(command);
  command.kind = LUMEN_COMMAND_COPY;
  command.src = src;
  command.dst = dst;
  command.length = length;
  return command;
}

lumen_command_v1 nop_command() {
  lumen_command_v1 command{};
  command.struct_size = sizeof(command);
  command.kind = LUMEN_COMMAND_NOP;
  return command;
}

struct Fixture {
  const lumen_backend_api_v1 *api = mock_api();
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

static void delayed_completion_fences_retirement() {
  Fixture fixture;
  REQUIRE(fixture.open());
  REQUIRE(lumen_mock_set_completion_delay_ticks(fixture.context, 4) ==
          LUMEN_STATUS_OK);

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
  std::uint64_t size = 0;
  REQUIRE(fixture.api->buffer_map(src, &src_data, &size) == LUMEN_STATUS_OK);
  REQUIRE(fixture.api->buffer_map(dst, &dst_data, &size) == LUMEN_STATUS_OK);
  std::memset(src_data, 0x5A, 64);
  std::memset(dst_data, 0, 64);

  lumen_command_v1 command = copy_command(src, dst, 64);
  lumen_event_t event = nullptr;
  REQUIRE(fixture.api->queue_submit(queue, &command, &event) ==
          LUMEN_STATUS_OK);

  lumen_event_state_v1 state = event_state();
  REQUIRE(fixture.api->event_poll(event, &state) == LUMEN_STATUS_OK);
  CHECK(state.status == LUMEN_EVENT_PENDING);
  CHECK(state.command_status == LUMEN_STATUS_OK);

  state = event_state();
  CHECK(fixture.api->event_wait(event, 0, &state) == LUMEN_STATUS_TIMEOUT);

  // Submission is not completion: buffers and queue stay leased.
  CHECK(fixture.api->event_release(event) == LUMEN_STATUS_INVALID_INPUT);
  CHECK(fixture.api->buffer_destroy(dst) == LUMEN_STATUS_INVALID_INPUT);
  CHECK(fixture.api->queue_destroy(queue) == LUMEN_STATUS_INVALID_INPUT);

  std::uint64_t pending = 99;
  CHECK(lumen_mock_pending_commands(fixture.context, &pending) ==
        LUMEN_STATUS_OK);
  CHECK(pending == 1);

  CHECK(lumen_mock_advance_ticks(fixture.context, 3) == LUMEN_STATUS_OK);
  state = event_state();
  REQUIRE(fixture.api->event_poll(event, &state) == LUMEN_STATUS_OK);
  CHECK(state.status == LUMEN_EVENT_PENDING);
  CHECK(static_cast<std::uint8_t *>(dst_data)[0] == 0);

  CHECK(lumen_mock_advance_ticks(fixture.context, 1) == LUMEN_STATUS_OK);
  state = event_state();
  REQUIRE(fixture.api->event_poll(event, &state) == LUMEN_STATUS_OK);
  CHECK(state.status == LUMEN_EVENT_COMPLETE);
  CHECK(state.command_status == LUMEN_STATUS_OK);
  CHECK(std::memcmp(src_data, dst_data, 64) == 0);

  CHECK(lumen_mock_pending_commands(fixture.context, &pending) ==
        LUMEN_STATUS_OK);
  CHECK(pending == 0);

  // Completion is the fence; release of the event is not required first.
  CHECK(fixture.api->buffer_destroy(dst) == LUMEN_STATUS_OK);
  CHECK(fixture.api->event_release(event) == LUMEN_STATUS_OK);
  CHECK(fixture.api->event_release(event) == LUMEN_STATUS_STALE);
  CHECK(fixture.api->queue_destroy(queue) == LUMEN_STATUS_OK);
  CHECK(fixture.api->buffer_destroy(src) == LUMEN_STATUS_OK);
}

static void queue_completions_stay_in_submission_order() {
  Fixture fixture;
  REQUIRE(fixture.open());
  REQUIRE(lumen_mock_set_completion_delay_ticks(fixture.context, 3) ==
          LUMEN_STATUS_OK);

  lumen_queue_desc_v1 qdesc = queue_desc();
  lumen_queue_t queue = nullptr;
  REQUIRE(fixture.api->queue_create(fixture.device, &qdesc, &queue) ==
          LUMEN_STATUS_OK);

  lumen_event_t first = nullptr;
  lumen_command_v1 first_command = nop_command();
  REQUIRE(fixture.api->queue_submit(queue, &first_command, &first) ==
          LUMEN_STATUS_OK);
  lumen_command_v1 second_command = nop_command();
  lumen_event_t second = nullptr;
  REQUIRE(fixture.api->queue_submit(queue, &second_command, &second) ==
          LUMEN_STATUS_OK);

  CHECK(lumen_mock_advance_ticks(fixture.context, 3) == LUMEN_STATUS_OK);

  lumen_event_state_v1 first_state = event_state();
  lumen_event_state_v1 second_state = event_state();
  REQUIRE(fixture.api->event_poll(first, &first_state) == LUMEN_STATUS_OK);
  REQUIRE(fixture.api->event_poll(second, &second_state) == LUMEN_STATUS_OK);
  CHECK(first_state.status == LUMEN_EVENT_COMPLETE);
  CHECK(second_state.status == LUMEN_EVENT_PENDING);
  CHECK(first_state.sequence < second_state.sequence);

  CHECK(lumen_mock_advance_ticks(fixture.context, 3) == LUMEN_STATUS_OK);
  second_state = event_state();
  REQUIRE(fixture.api->event_poll(second, &second_state) == LUMEN_STATUS_OK);
  CHECK(second_state.status == LUMEN_EVENT_COMPLETE);

  CHECK(fixture.api->event_release(first) == LUMEN_STATUS_OK);
  CHECK(fixture.api->event_release(second) == LUMEN_STATUS_OK);
  CHECK(fixture.api->queue_destroy(queue) == LUMEN_STATUS_OK);
}

static void reset_zeroes_simulated_time() {
  Fixture fixture;
  REQUIRE(fixture.open());
  REQUIRE(lumen_mock_set_completion_delay_ticks(fixture.context, 2) ==
          LUMEN_STATUS_OK);

  lumen_queue_desc_v1 qdesc = queue_desc();
  lumen_queue_t queue = nullptr;
  REQUIRE(fixture.api->queue_create(fixture.device, &qdesc, &queue) ==
          LUMEN_STATUS_OK);

  lumen_event_t event = nullptr;
  lumen_command_v1 nop = nop_command();
  REQUIRE(fixture.api->queue_submit(queue, &nop, &event) == LUMEN_STATUS_OK);
  CHECK(lumen_mock_advance_ticks(fixture.context, 2) == LUMEN_STATUS_OK);
  lumen_event_state_v1 state = event_state();
  REQUIRE(fixture.api->event_poll(event, &state) == LUMEN_STATUS_OK);
  CHECK(state.status == LUMEN_EVENT_COMPLETE);
  CHECK(fixture.api->event_release(event) == LUMEN_STATUS_OK);
  CHECK(fixture.api->queue_destroy(queue) == LUMEN_STATUS_OK);
  CHECK(fixture.api->device_close(fixture.device) == LUMEN_STATUS_OK);
  fixture.device = nullptr;

  CHECK(fixture.api->context_reset(fixture.context) == LUMEN_STATUS_OK);
  REQUIRE(fixture.api->device_open(fixture.context, &fixture.device) ==
          LUMEN_STATUS_OK);
  REQUIRE(fixture.api->queue_create(fixture.device, &qdesc, &queue) ==
          LUMEN_STATUS_OK);
  // A maximum-1 delay only fits when tick was reset to zero: with the old
  // tick of 2 the completion tick would overflow and submit would be rejected.
  constexpr std::uint64_t kLargeDelay =
      (std::numeric_limits<std::uint64_t>::max)() - 1;
  REQUIRE(lumen_mock_set_completion_delay_ticks(fixture.context, kLargeDelay) ==
          LUMEN_STATUS_OK);

  event = nullptr;
  lumen_command_v1 nop2 = nop_command();
  REQUIRE(fixture.api->queue_submit(queue, &nop2, &event) == LUMEN_STATUS_OK);

  CHECK(lumen_mock_advance_ticks(fixture.context, kLargeDelay) ==
        LUMEN_STATUS_OK);
  state = event_state();
  REQUIRE(fixture.api->event_poll(event, &state) == LUMEN_STATUS_OK);
  CHECK(state.status == LUMEN_EVENT_COMPLETE);
  CHECK(fixture.api->event_release(event) == LUMEN_STATUS_OK);
  CHECK(fixture.api->queue_destroy(queue) == LUMEN_STATUS_OK);
}

static void completion_tick_overflow_is_rejected() {
  Fixture fixture;
  REQUIRE(fixture.open());
  REQUIRE(lumen_mock_set_completion_delay_ticks(
              fixture.context, (std::numeric_limits<std::uint64_t>::max)()) ==
          LUMEN_STATUS_OK);

  lumen_queue_desc_v1 qdesc = queue_desc();
  lumen_queue_t queue = nullptr;
  REQUIRE(fixture.api->queue_create(fixture.device, &qdesc, &queue) ==
          LUMEN_STATUS_OK);

  // Move off tick zero so base + delay cannot be represented.
  REQUIRE(lumen_mock_advance_ticks(fixture.context, 1) == LUMEN_STATUS_OK);

  const lumen_command_v1 command = nop_command();
  const lumen_event_t sentinel =
      reinterpret_cast<lumen_event_t>(static_cast<std::uintptr_t>(0x1));
  lumen_event_t event = sentinel;
  CHECK(fixture.api->queue_submit(queue, &command, &event) ==
        LUMEN_STATUS_RESOURCE_EXHAUSTED);
  CHECK(event == sentinel);
  CHECK(fixture.api->queue_destroy(queue) == LUMEN_STATUS_OK);

  CHECK(lumen_mock_set_completion_delay_ticks(fixture.context, 0) ==
        LUMEN_STATUS_OK);
}

static void mock_controls_reject_non_mock_or_missing_contexts() {
  const lumen_backend_api_v1 *cpu = lumen_cpu_backend_api();
  lumen_context_t cpu_context = nullptr;
  REQUIRE(cpu->backend_open(cpu, &cpu_context) == LUMEN_STATUS_OK);

  CHECK(lumen_mock_advance_ticks(cpu_context, 1) == LUMEN_STATUS_INVALID_INPUT);
  CHECK(lumen_mock_set_completion_delay_ticks(cpu_context, 1) ==
        LUMEN_STATUS_INVALID_INPUT);

  std::uint64_t pending = 77;
  CHECK(lumen_mock_pending_commands(cpu_context, &pending) ==
        LUMEN_STATUS_INVALID_INPUT);
  CHECK(pending == 77);

  CHECK(lumen_mock_advance_ticks(nullptr, 1) == LUMEN_STATUS_INVALID_INPUT);
  CHECK(lumen_mock_set_completion_delay_ticks(nullptr, 1) ==
        LUMEN_STATUS_INVALID_INPUT);
  CHECK(lumen_mock_pending_commands(nullptr, &pending) ==
        LUMEN_STATUS_INVALID_INPUT);
  CHECK(lumen_mock_pending_commands(cpu_context, nullptr) ==
        LUMEN_STATUS_INVALID_INPUT);

  CHECK(cpu->context_close(cpu_context) == LUMEN_STATUS_OK);
}

int main() {
  delayed_completion_fences_retirement();
  queue_completions_stay_in_submission_order();
  reset_zeroes_simulated_time();
  completion_tick_overflow_is_rejected();
  mock_controls_reject_non_mock_or_missing_contexts();
  return lumen::test::summary("backend-mock");
}
