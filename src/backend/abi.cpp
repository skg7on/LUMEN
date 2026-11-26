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

// Validation of the public backend function table.

#include "lumen/backend.h"

extern "C" lumen_status_t
lumen_backend_api_validate(const lumen_backend_api_v1 *api) {
  if (api == nullptr) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  if (api->struct_size != sizeof(lumen_backend_api_v1)) {
    return LUMEN_STATUS_UNSUPPORTED;
  }
  if (api->abi_major != LUMEN_BACKEND_ABI_MAJOR) {
    return LUMEN_STATUS_UNSUPPORTED;
  }
  if (api->abi_minor > LUMEN_BACKEND_ABI_MINOR) {
    return LUMEN_STATUS_UNSUPPORTED;
  }
  if (api->backend_open == nullptr || api->context_close == nullptr ||
      api->context_reset == nullptr || api->device_open == nullptr ||
      api->device_close == nullptr || api->device_capabilities == nullptr ||
      api->buffer_create == nullptr || api->buffer_destroy == nullptr ||
      api->buffer_map == nullptr || api->buffer_unmap == nullptr ||
      api->queue_create == nullptr || api->queue_destroy == nullptr ||
      api->queue_submit == nullptr || api->event_poll == nullptr ||
      api->event_wait == nullptr || api->event_release == nullptr) {
    return LUMEN_STATUS_UNSUPPORTED;
  }
  return LUMEN_STATUS_OK;
}
