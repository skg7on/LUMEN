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

#include "backend/qualification/qualification_api.h"

extern "C" lumen_status_t
lumen_qualification_api_validate(const lumen_qualification_api_v1 *api) {
  if (api == nullptr) {
    return LUMEN_STATUS_INVALID_INPUT;
  }
  if (api->struct_size != sizeof(lumen_qualification_api_v1) ||
      api->abi_major != LUMEN_QUALIFICATION_ABI_MAJOR ||
      api->abi_minor != LUMEN_QUALIFICATION_ABI_MINOR) {
    return LUMEN_STATUS_UNSUPPORTED;
  }
  if (api->base_api == nullptr ||
      lumen_backend_api_validate(api->base_api) != LUMEN_STATUS_OK) {
    return LUMEN_STATUS_UNSUPPORTED;
  }
  if (api->program_register == nullptr || api->program_unregister == nullptr ||
      api->submit_command == nullptr || api->launch == nullptr ||
      api->query_memory == nullptr || api->read_telemetry == nullptr ||
      api->recover == nullptr) {
    return LUMEN_STATUS_UNSUPPORTED;
  }
  return LUMEN_STATUS_OK;
}
