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

// Shared CPU/mock backend runtime.
//
// Handles are opaque pointer-sized identifiers into a process-local registry;
// they are never dereferenced, so a released handle can be detected instead of
// becoming a dangling pointer. Objects stay registered as retired tombstones
// until their owning context closes, which makes double release and use-after-
// free report LUMEN_STATUS_STALE.

#include "backend/common/runtime.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/checked_math.h"

namespace lumen::backend {
namespace {

constexpr uint64_t kMaxBufferBytes = 1ull << 30; // 1 GiB
constexpr uint64_t kMaxInflightCommands = 1024;
constexpr uint32_t kMaxAlignment = 1u << 20;

static_assert(sizeof(std::uintptr_t) >= sizeof(std::uint64_t),
              "opaque handles require a 64-bit host");
static_assert(sizeof(std::size_t) >= sizeof(std::uint64_t),
              "buffer sizes must fit host size_t");

enum class Kind {
  context,
  device,
  buffer,
  queue,
  event,
};

struct Object {
  virtual ~Object() = default;
  Kind kind = Kind::context;
  std::uint64_t id = 0;
  std::uint64_t context_id = 0;
  std::uint64_t device_id = 0;
  bool retired = false;
};

struct Context : Object {
  Mode mode = Mode::cpu;
  std::uint64_t tick = 0;
  std::uint64_t delay_ticks = 0;
  std::uint64_t next_sequence = 1;
  std::vector<std::uint64_t> pending_events;
};

struct Device : Object {};

struct Buffer : Object {
  void *data = nullptr;
  std::uint64_t size = 0;
  std::uint32_t alignment = 0;
  std::uint64_t live_events = 0;
};

struct Queue : Object {
  std::uint64_t max_inflight = kMaxInflightCommands;
  std::uint64_t pending = 0;
  std::uint64_t last_ready_tick = 0;
};

enum class CompletionState {
  pending,
  complete,
  failed,
};

struct Event : Object {
  std::uint64_t queue_id = 0;
  std::uint64_t sequence = 0;
  std::uint64_t ready_tick = 0;
  bool queued = false;
  CompletionState state = CompletionState::pending;
  lumen_status_t command_status = LUMEN_STATUS_OK;
  std::uint32_t command_kind = LUMEN_COMMAND_NOP;
  std::uint64_t dst_id = 0;
  std::uint64_t src_id = 0;
  std::uint64_t dst_offset = 0;
  std::uint64_t src_offset = 0;
  std::uint64_t length = 0;
  std::uint32_t fill_byte = 0;
  std::vector<std::uint64_t> buffer_refs;
};

class Registry {
public:
  static Registry &instance() {
    static Registry registry;
    return registry;
  }

  // Returns 0 when registration cannot allocate; no exception may cross the
  // C ABI, so this call is the single allocation point for new handles.
  std::uint64_t add(std::shared_ptr<Object> object) noexcept {
    try {
      std::lock_guard<std::mutex> lock(mutex_);
      const std::uint64_t id = next_id_++;
      object->id = id;
      objects_.emplace(id, std::move(object));
      return id;
    } catch (...) {
      return 0;
    }
  }

  std::shared_ptr<Object> find(std::uint64_t id) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto it = objects_.find(id);
    return it == objects_.end() ? nullptr : it->second;
  }

  bool has_live_child(std::uint64_t context_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &[id, object] : objects_) {
      if (id != context_id && object->context_id == context_id &&
          !object->retired) {
        return true;
      }
    }
    return false;
  }

  bool has_live_device_child(std::uint64_t device_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &[id, object] : objects_) {
      (void)id;
      if (object->device_id == device_id && !object->retired &&
          (object->kind == Kind::buffer || object->kind == Kind::queue ||
           object->kind == Kind::event)) {
        return true;
      }
    }
    return false;
  }

  void erase_context(std::uint64_t context_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto it = objects_.begin(); it != objects_.end();) {
      if (it->second->context_id == context_id) {
        it = objects_.erase(it);
      } else {
        ++it;
      }
    }
  }

private:
  std::mutex mutex_;
  std::uint64_t next_id_ = 1;
  std::unordered_map<std::uint64_t, std::shared_ptr<Object>> objects_;
};

enum class Lookup {
  ok,
  stale,
  invalid,
};

lumen_status_t lookup_status(Lookup lookup) {
  switch (lookup) {
  case Lookup::ok:
    return LUMEN_STATUS_OK;
  case Lookup::stale:
    return LUMEN_STATUS_STALE;
  case Lookup::invalid:
    return LUMEN_STATUS_INVALID_INPUT;
  }
  return LUMEN_STATUS_INVALID_INPUT;
}

template <typename T> T *to_handle(std::uint64_t id) {
  return reinterpret_cast<T *>(static_cast<std::uintptr_t>(id));
}

std::uint64_t from_handle(const void *handle) {
  return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
}

Lookup lookup_object(const void *handle, Kind kind,
                     std::shared_ptr<Object> &out) {
  if (handle == nullptr) {
    return Lookup::invalid;
  }
  auto object = Registry::instance().find(from_handle(handle));
  if (!object) {
    return Lookup::stale;
  }
  if (object->kind != kind) {
    return Lookup::invalid;
  }
  if (object->retired) {
    return Lookup::stale;
  }
  out = std::move(object);
  return Lookup::ok;
}

template <typename T>
Lookup lookup_typed(const void *handle, Kind kind, std::shared_ptr<T> &out) {
  std::shared_ptr<Object> object;
  const Lookup result = lookup_object(handle, kind, object);
  if (result == Lookup::ok) {
    out = std::static_pointer_cast<T>(std::move(object));
  }
  return result;
}

std::shared_ptr<Context> find_context(std::uint64_t id) {
  const auto object = Registry::instance().find(id);
  if (!object || object->kind != Kind::context || object->retired) {
    return nullptr;
  }
  return std::static_pointer_cast<Context>(object);
}

template <typename T>
std::shared_ptr<T> find_live(std::uint64_t id, Kind kind) {
  const auto object = Registry::instance().find(id);
  if (!object || object->kind != kind || object->retired) {
    return nullptr;
  }
  return std::static_pointer_cast<T>(object);
}

bool is_power_of_two(std::uint32_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

lumen_status_t validate_out_record(std::uint32_t struct_size,
                                   std::size_t expected_size,
                                   std::uint32_t abi_major,
                                   std::uint32_t abi_minor) {
  if (struct_size != expected_size) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  if (abi_major != LUMEN_BACKEND_ABI_MAJOR ||
      abi_minor > LUMEN_BACKEND_ABI_MINOR) {
    return LUMEN_STATUS_UNSUPPORTED;
  }
  return LUMEN_STATUS_OK;
}

void *allocate_bytes(std::uint64_t size, std::uint32_t alignment) {
  if (alignment <= alignof(std::max_align_t)) {
    return ::operator new(static_cast<std::size_t>(size));
  }
  return ::operator new(static_cast<std::size_t>(size),
                        std::align_val_t{static_cast<std::size_t>(alignment)});
}

void release_bytes(void *data, std::uint32_t alignment) {
  if (alignment <= alignof(std::max_align_t)) {
    ::operator delete(data);
  } else {
    ::operator delete(data,
                      std::align_val_t{static_cast<std::size_t>(alignment)});
  }
}

lumen_status_t execute_command(const Event &event) {
  if (event.command_kind == LUMEN_COMMAND_NOP) {
    return LUMEN_STATUS_OK;
  }
  auto dst = find_live<Buffer>(event.dst_id, Kind::buffer);
  if (!dst || dst->data == nullptr) {
    return LUMEN_STATUS_DEVICE_ERROR;
  }
  auto *dst_bytes = static_cast<std::byte *>(dst->data) + event.dst_offset;
  if (event.command_kind == LUMEN_COMMAND_FILL) {
    std::memset(dst_bytes, static_cast<int>(event.fill_byte & 0xffu),
                static_cast<std::size_t>(event.length));
    return LUMEN_STATUS_OK;
  }
  auto src = find_live<Buffer>(event.src_id, Kind::buffer);
  if (!src || src->data == nullptr) {
    return LUMEN_STATUS_DEVICE_ERROR;
  }
  const auto *src_bytes =
      static_cast<const std::byte *>(src->data) + event.src_offset;
  std::memmove(dst_bytes, src_bytes, static_cast<std::size_t>(event.length));
  return LUMEN_STATUS_OK;
}

void drop_buffer_refs(Event &event) {
  for (const std::uint64_t id : event.buffer_refs) {
    if (auto buffer = find_live<Buffer>(id, Kind::buffer)) {
      if (buffer->live_events > 0) {
        --buffer->live_events;
      }
    }
  }
  event.buffer_refs.clear();
}

void complete_event(Event &event, lumen_status_t result) {
  event.state = result == LUMEN_STATUS_OK ? CompletionState::complete
                                          : CompletionState::failed;
  event.command_status = result;
  drop_buffer_refs(event);
  if (event.queued) {
    if (auto queue = find_live<Queue>(event.queue_id, Kind::queue)) {
      if (queue->pending > 0) {
        --queue->pending;
      }
    }
    event.queued = false;
  }
}

lumen_status_t add_buffer_ref(Event &event, std::uint64_t buffer_id) {
  if (buffer_id == 0) {
    return LUMEN_STATUS_OK;
  }
  for (const std::uint64_t existing : event.buffer_refs) {
    if (existing == buffer_id) {
      return LUMEN_STATUS_OK;
    }
  }
  auto buffer = find_live<Buffer>(buffer_id, Kind::buffer);
  if (!buffer) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  ++buffer->live_events;
  try {
    event.buffer_refs.push_back(buffer_id);
  } catch (const std::bad_alloc &) {
    --buffer->live_events;
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }
  return LUMEN_STATUS_OK;
}

void fill_event_state(const Event &event, lumen_event_state_v1 &out) {
  out.status =
      event.state == CompletionState::pending
          ? LUMEN_EVENT_PENDING
          : (event.state == CompletionState::complete ? LUMEN_EVENT_COMPLETE
                                                      : LUMEN_EVENT_FAILED);
  out.command_status = static_cast<std::uint32_t>(event.command_status);
  out.flags = 0;
  out.sequence = event.sequence;
}

} // namespace

lumen_status_t open(Mode mode, lumen_context_t *out_context) {
  if (out_context == nullptr) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  std::shared_ptr<Context> context;
  try {
    context = std::make_shared<Context>();
  } catch (const std::bad_alloc &) {
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }
  context->kind = Kind::context;
  context->mode = mode;
  const std::uint64_t id = Registry::instance().add(context);
  if (id == 0) {
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }
  context->context_id = id;
  *out_context = to_handle<lumen_context>(id);
  return LUMEN_STATUS_OK;
}

lumen_status_t close_context(lumen_context_t handle) {
  std::shared_ptr<Context> context;
  const Lookup found = lookup_typed(handle, Kind::context, context);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (Registry::instance().has_live_child(context->id)) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  Registry::instance().erase_context(context->id);
  return LUMEN_STATUS_OK;
}

lumen_status_t reset_context(lumen_context_t handle) {
  std::shared_ptr<Context> context;
  const Lookup found = lookup_typed(handle, Kind::context, context);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (Registry::instance().has_live_child(context->id)) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  context->tick = 0;
  context->pending_events.clear();
  return LUMEN_STATUS_OK;
}

lumen_status_t open_device(lumen_context_t handle, lumen_device_t *out_device) {
  if (out_device == nullptr) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  std::shared_ptr<Context> context;
  const Lookup found = lookup_typed(handle, Kind::context, context);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  std::shared_ptr<Device> device;
  try {
    device = std::make_shared<Device>();
  } catch (const std::bad_alloc &) {
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }
  device->kind = Kind::device;
  device->context_id = context->id;
  const std::uint64_t id = Registry::instance().add(device);
  if (id == 0) {
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }
  device->device_id = id;
  *out_device = to_handle<lumen_device>(id);
  return LUMEN_STATUS_OK;
}

lumen_status_t close_device(lumen_device_t handle) {
  std::shared_ptr<Device> device;
  const Lookup found = lookup_typed(handle, Kind::device, device);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (Registry::instance().has_live_device_child(device->id)) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  device->retired = true;
  return LUMEN_STATUS_OK;
}

lumen_status_t device_capabilities(lumen_device_t handle,
                                   lumen_capabilities_v1 *out_capabilities) {
  std::shared_ptr<Device> device;
  const Lookup found = lookup_typed(handle, Kind::device, device);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (out_capabilities == nullptr) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  const lumen_status_t record = validate_out_record(
      out_capabilities->struct_size, sizeof(lumen_capabilities_v1),
      out_capabilities->abi_major, out_capabilities->abi_minor);
  if (record != LUMEN_STATUS_OK) {
    return record;
  }
  const auto context = find_context(device->context_id);
  if (!context) {
    return LUMEN_STATUS_STALE;
  }

  out_capabilities->flags = LUMEN_CAP_HOST_MEMORY;
  if (context->mode == Mode::mock) {
    out_capabilities->flags |= LUMEN_CAP_ASYNC_COMPLETION |
                               LUMEN_CAP_DELAYED_COMPLETION |
                               LUMEN_CAP_SIMULATED_TIME;
  }
  out_capabilities->max_buffer_bytes = kMaxBufferBytes;
  out_capabilities->max_inflight_commands = kMaxInflightCommands;
  out_capabilities->memory_alignment_min = 1;
  out_capabilities->queue_order = LUMEN_QUEUE_ORDER_IN_ORDER;
  out_capabilities->supports_reset = 1;
  out_capabilities->reserved = 0;
  return LUMEN_STATUS_OK;
}

lumen_status_t create_buffer(lumen_device_t handle,
                             const lumen_buffer_desc_v1 *desc,
                             lumen_buffer_t *out_buffer) {
  std::shared_ptr<Device> device;
  const Lookup found = lookup_typed(handle, Kind::device, device);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (desc == nullptr || out_buffer == nullptr) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  if (desc->struct_size != sizeof(lumen_buffer_desc_v1)) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  if (desc->memory_kind != LUMEN_MEMORY_HOST) {
    return LUMEN_STATUS_UNSUPPORTED;
  }
  if (desc->size_bytes == 0) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  if (desc->size_bytes > kMaxBufferBytes) {
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }
  if (!is_power_of_two(desc->alignment) || desc->alignment > kMaxAlignment) {
    return LUMEN_STATUS_INVALID_INPUT;
  }

  void *data = nullptr;
  try {
    data = allocate_bytes(desc->size_bytes, desc->alignment);
  } catch (const std::bad_alloc &) {
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }

  std::shared_ptr<Buffer> buffer;
  try {
    buffer = std::make_shared<Buffer>();
  } catch (const std::bad_alloc &) {
    release_bytes(data, desc->alignment);
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }
  buffer->kind = Kind::buffer;
  buffer->context_id = device->context_id;
  buffer->device_id = device->id;
  buffer->data = data;
  buffer->size = desc->size_bytes;
  buffer->alignment = desc->alignment;
  const std::uint64_t id = Registry::instance().add(buffer);
  if (id == 0) {
    release_bytes(data, desc->alignment);
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }
  *out_buffer = to_handle<lumen_buffer>(id);
  return LUMEN_STATUS_OK;
}

lumen_status_t destroy_buffer(lumen_buffer_t handle) {
  std::shared_ptr<Buffer> buffer;
  const Lookup found = lookup_typed(handle, Kind::buffer, buffer);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (buffer->live_events > 0) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  if (buffer->data != nullptr) {
    release_bytes(buffer->data, buffer->alignment);
    buffer->data = nullptr;
  }
  buffer->retired = true;
  return LUMEN_STATUS_OK;
}

lumen_status_t map_buffer(lumen_buffer_t handle, void **out_data,
                          uint64_t *out_size) {
  std::shared_ptr<Buffer> buffer;
  const Lookup found = lookup_typed(handle, Kind::buffer, buffer);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (out_data == nullptr || out_size == nullptr) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  *out_data = buffer->data;
  *out_size = buffer->size;
  return LUMEN_STATUS_OK;
}

lumen_status_t unmap_buffer(lumen_buffer_t handle) {
  std::shared_ptr<Buffer> buffer;
  const Lookup found = lookup_typed(handle, Kind::buffer, buffer);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  return LUMEN_STATUS_OK;
}

lumen_status_t create_queue(lumen_device_t handle,
                            const lumen_queue_desc_v1 *desc,
                            lumen_queue_t *out_queue) {
  std::shared_ptr<Device> device;
  const Lookup found = lookup_typed(handle, Kind::device, device);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (desc == nullptr || out_queue == nullptr) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  if (desc->struct_size != sizeof(lumen_queue_desc_v1)) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  const std::uint64_t max_inflight =
      desc->max_inflight == 0 ? kMaxInflightCommands : desc->max_inflight;
  if (max_inflight > kMaxInflightCommands) {
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }

  std::shared_ptr<Queue> queue;
  try {
    queue = std::make_shared<Queue>();
  } catch (const std::bad_alloc &) {
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }
  queue->kind = Kind::queue;
  queue->context_id = device->context_id;
  queue->device_id = device->id;
  queue->max_inflight = max_inflight;
  const std::uint64_t id = Registry::instance().add(queue);
  if (id == 0) {
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }
  *out_queue = to_handle<lumen_queue>(id);
  return LUMEN_STATUS_OK;
}

lumen_status_t destroy_queue(lumen_queue_t handle) {
  std::shared_ptr<Queue> queue;
  const Lookup found = lookup_typed(handle, Kind::queue, queue);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (queue->pending > 0) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  queue->retired = true;
  return LUMEN_STATUS_OK;
}

lumen_status_t submit(lumen_queue_t handle, const lumen_command_v1 *command,
                      lumen_event_t *out_event) {
  std::shared_ptr<Queue> queue;
  const Lookup found = lookup_typed(handle, Kind::queue, queue);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (command == nullptr || out_event == nullptr) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  if (command->struct_size != sizeof(lumen_command_v1)) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  if (queue->pending >= queue->max_inflight) {
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }

  const auto context = find_context(queue->context_id);
  if (!context) {
    return LUMEN_STATUS_STALE;
  }

  std::uint64_t dst_id = 0;
  std::uint64_t src_id = 0;
  if (command->kind == LUMEN_COMMAND_COPY ||
      command->kind == LUMEN_COMMAND_FILL) {
    if (command->dst == nullptr || command->length == 0) {
      return LUMEN_STATUS_INVALID_INPUT;
    }
    std::shared_ptr<Buffer> dst;
    if (lookup_typed(command->dst, Kind::buffer, dst) != Lookup::ok) {
      return LUMEN_STATUS_INVALID_INPUT;
    }
    if (dst->context_id != queue->context_id) {
      return LUMEN_STATUS_INVALID_INPUT;
    }
    if (checked_range(static_cast<std::size_t>(command->dst_offset),
                      static_cast<std::size_t>(command->length),
                      static_cast<std::size_t>(dst->size)) != Status::ok) {
      return LUMEN_STATUS_INVALID_INPUT;
    }
    dst_id = dst->id;

    if (command->kind == LUMEN_COMMAND_COPY) {
      if (command->src == nullptr) {
        return LUMEN_STATUS_INVALID_INPUT;
      }
      std::shared_ptr<Buffer> src;
      if (lookup_typed(command->src, Kind::buffer, src) != Lookup::ok) {
        return LUMEN_STATUS_INVALID_INPUT;
      }
      if (src->context_id != queue->context_id) {
        return LUMEN_STATUS_INVALID_INPUT;
      }
      if (checked_range(static_cast<std::size_t>(command->src_offset),
                        static_cast<std::size_t>(command->length),
                        static_cast<std::size_t>(src->size)) != Status::ok) {
        return LUMEN_STATUS_INVALID_INPUT;
      }
      src_id = src->id;
    }
  } else if (command->kind != LUMEN_COMMAND_NOP) {
    return LUMEN_STATUS_INVALID_INPUT;
  }

  std::uint64_t ready_tick = 0;
  if (context->mode == Mode::mock) {
    constexpr std::uint64_t max = (std::numeric_limits<std::uint64_t>::max)();
    const std::uint64_t base =
        (std::max)(context->tick, queue->last_ready_tick);
    if (context->delay_ticks > max - base) {
      return LUMEN_STATUS_RESOURCE_EXHAUSTED;
    }
    ready_tick = base + context->delay_ticks;
  }

  std::shared_ptr<Event> event;
  try {
    event = std::make_shared<Event>();
  } catch (const std::bad_alloc &) {
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }
  event->kind = Kind::event;
  event->context_id = queue->context_id;
  event->device_id = queue->device_id;
  event->queue_id = queue->id;
  event->sequence = context->next_sequence++;
  event->command_kind = command->kind;
  event->dst_id = dst_id;
  event->src_id = src_id;
  event->dst_offset = command->dst_offset;
  event->src_offset = command->src_offset;
  event->length = command->length;
  event->fill_byte = command->fill_byte;

  lumen_status_t ref_result = add_buffer_ref(*event, dst_id);
  if (ref_result == LUMEN_STATUS_OK) {
    ref_result = add_buffer_ref(*event, src_id);
  }
  if (ref_result != LUMEN_STATUS_OK) {
    drop_buffer_refs(*event);
    return ref_result;
  }

  const std::uint64_t id = Registry::instance().add(event);
  if (id == 0) {
    drop_buffer_refs(*event);
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }

  if (context->mode == Mode::cpu) {
    complete_event(*event, execute_command(*event));
  } else {
    event->ready_tick = ready_tick;
    if (ready_tick <= context->tick) {
      complete_event(*event, execute_command(*event));
    } else {
      event->queued = true;
      ++queue->pending;
      try {
        context->pending_events.push_back(id);
      } catch (const std::bad_alloc &) {
        --queue->pending;
        event->queued = false;
        event->retired = true;
        drop_buffer_refs(*event);
        return LUMEN_STATUS_RESOURCE_EXHAUSTED;
      }
    }
    queue->last_ready_tick = ready_tick;
  }

  *out_event = to_handle<lumen_event>(id);
  return LUMEN_STATUS_OK;
}

lumen_status_t poll_event(lumen_event_t handle,
                          lumen_event_state_v1 *out_state) {
  std::shared_ptr<Event> event;
  const Lookup found = lookup_typed(handle, Kind::event, event);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (out_state == nullptr) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  const lumen_status_t record =
      validate_out_record(out_state->struct_size, sizeof(lumen_event_state_v1),
                          out_state->abi_major, out_state->abi_minor);
  if (record != LUMEN_STATUS_OK) {
    return record;
  }
  fill_event_state(*event, *out_state);
  return LUMEN_STATUS_OK;
}

lumen_status_t wait_event(lumen_event_t handle, uint64_t timeout_ns,
                          lumen_event_state_v1 *out_state) {
  (void)timeout_ns;
  std::shared_ptr<Event> event;
  const Lookup found = lookup_typed(handle, Kind::event, event);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (out_state == nullptr) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  const lumen_status_t record =
      validate_out_record(out_state->struct_size, sizeof(lumen_event_state_v1),
                          out_state->abi_major, out_state->abi_minor);
  if (record != LUMEN_STATUS_OK) {
    return record;
  }
  if (event->state == CompletionState::pending) {
    return LUMEN_STATUS_TIMEOUT;
  }
  fill_event_state(*event, *out_state);
  return LUMEN_STATUS_OK;
}

lumen_status_t release_event(lumen_event_t handle) {
  std::shared_ptr<Event> event;
  const Lookup found = lookup_typed(handle, Kind::event, event);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (event->state == CompletionState::pending) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  event->retired = true;
  return LUMEN_STATUS_OK;
}

lumen_status_t mock_set_completion_delay(lumen_context_t handle,
                                         uint64_t delay_ticks) {
  std::shared_ptr<Context> context;
  const Lookup found = lookup_typed(handle, Kind::context, context);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (context->mode != Mode::mock) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  context->delay_ticks = delay_ticks;
  return LUMEN_STATUS_OK;
}

lumen_status_t mock_advance(lumen_context_t handle, uint64_t ticks) {
  std::shared_ptr<Context> context;
  const Lookup found = lookup_typed(handle, Kind::context, context);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (context->mode != Mode::mock) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  if (ticks > (std::numeric_limits<std::uint64_t>::max)() - context->tick) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  const std::uint64_t new_tick = context->tick + ticks;

  // Reserve before completing anything so an allocation failure cannot leave
  // a half-advanced simulated clock or duplicate completions.
  std::vector<std::uint64_t> remaining;
  try {
    remaining.reserve(context->pending_events.size());
  } catch (const std::bad_alloc &) {
    return LUMEN_STATUS_RESOURCE_EXHAUSTED;
  }
  for (const std::uint64_t id : context->pending_events) {
    auto event = find_live<Event>(id, Kind::event);
    if (!event) {
      continue;
    }
    if (event->ready_tick <= new_tick) {
      const lumen_status_t result = execute_command(*event);
      complete_event(*event, result);
    } else {
      remaining.push_back(id);
    }
  }
  context->pending_events = std::move(remaining);
  context->tick = new_tick;
  return LUMEN_STATUS_OK;
}

lumen_status_t mock_pending_count(lumen_context_t handle, uint64_t *out_count) {
  std::shared_ptr<Context> context;
  const Lookup found = lookup_typed(handle, Kind::context, context);
  if (found != Lookup::ok) {
    return lookup_status(found);
  }
  if (context->mode != Mode::mock) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  if (out_count == nullptr) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  *out_count = static_cast<std::uint64_t>(context->pending_events.size());
  return LUMEN_STATUS_OK;
}

} // namespace lumen::backend
