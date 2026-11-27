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

// CPU adapter: host memory with commands completed at submission.

#include "lumen/backend.h"

#include "backend/common/runtime.h"

namespace {

lumen_status_t open_cpu(const lumen_backend_api_v1 *api,
                        lumen_context_t *out_context) {
  if (const lumen_status_t validated = lumen_backend_api_validate(api);
      validated != LUMEN_STATUS_OK) {
    return validated;
  }
  return lumen::backend::open(lumen::backend::Mode::cpu, out_context);
}

constexpr lumen_backend_api_v1 kCpuApi = {
    .struct_size = sizeof(lumen_backend_api_v1),
    .abi_major = LUMEN_BACKEND_ABI_MAJOR,
    .abi_minor = LUMEN_BACKEND_ABI_MINOR,
    .flags = 0,
    .backend_open = open_cpu,
    .context_close = lumen::backend::close_context,
    .context_reset = lumen::backend::reset_context,
    .device_open = lumen::backend::open_device,
    .device_close = lumen::backend::close_device,
    .device_capabilities = lumen::backend::device_capabilities,
    .buffer_create = lumen::backend::create_buffer,
    .buffer_destroy = lumen::backend::destroy_buffer,
    .buffer_map = lumen::backend::map_buffer,
    .buffer_unmap = lumen::backend::unmap_buffer,
    .queue_create = lumen::backend::create_queue,
    .queue_destroy = lumen::backend::destroy_queue,
    .queue_submit = lumen::backend::submit,
    .event_poll = lumen::backend::poll_event,
    .event_wait = lumen::backend::wait_event,
    .event_release = lumen::backend::release_event,
};

} // namespace

extern "C" const lumen_backend_api_v1 *lumen_cpu_backend_api(void) {
  return &kCpuApi;
}
