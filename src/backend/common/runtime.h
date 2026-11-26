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

// Shared implementation entry points for the CPU and mock adapters.
//
// Both adapters expose the same public C function table. This module owns the
// handle registry, validation, and command lifecycle; adapters differ only in
// completion policy (immediate for CPU, simulated time for mock).

#pragma once

#include "lumen/backend.h"

namespace lumen::backend {

enum class Mode {
  cpu,
  mock,
};

lumen_status_t open(Mode mode, lumen_context_t *out_context);

lumen_status_t close_context(lumen_context_t context);
lumen_status_t reset_context(lumen_context_t context);

lumen_status_t open_device(lumen_context_t context, lumen_device_t *out_device);
lumen_status_t close_device(lumen_device_t device);
lumen_status_t device_capabilities(lumen_device_t device,
                                   lumen_capabilities_v1 *out_capabilities);

lumen_status_t create_buffer(lumen_device_t device,
                             const lumen_buffer_desc_v1 *desc,
                             lumen_buffer_t *out_buffer);
lumen_status_t destroy_buffer(lumen_buffer_t buffer);
lumen_status_t map_buffer(lumen_buffer_t buffer, void **out_data,
                          uint64_t *out_size);
lumen_status_t unmap_buffer(lumen_buffer_t buffer);

lumen_status_t create_queue(lumen_device_t device,
                            const lumen_queue_desc_v1 *desc,
                            lumen_queue_t *out_queue);
lumen_status_t destroy_queue(lumen_queue_t queue);
lumen_status_t submit(lumen_queue_t queue, const lumen_command_v1 *command,
                      lumen_event_t *out_event);

lumen_status_t poll_event(lumen_event_t event, lumen_event_state_v1 *out_state);
lumen_status_t wait_event(lumen_event_t event, uint64_t timeout_ns,
                          lumen_event_state_v1 *out_state);
lumen_status_t release_event(lumen_event_t event);

// Mock-only controls. Each rejects a non-mock or retired context.
lumen_status_t mock_set_completion_delay(lumen_context_t context,
                                         uint64_t delay_ticks);
lumen_status_t mock_advance(lumen_context_t context, uint64_t ticks);
lumen_status_t mock_pending_count(lumen_context_t context, uint64_t *out_count);

} // namespace lumen::backend
