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

/* Compiles and links the private qualification extension as C11 and checks
 * table validation: exact size, extension version, base table, pointers. */

#include <backend/qualification/qualification_api.h>

#include <stddef.h>

static lumen_status_t
stub_program_register(lumen_device_t device,
                      const lumen_qual_program_desc_v1 *desc,
                      lumen_qual_program_t *out_program) {
  (void)device;
  (void)desc;
  (void)out_program;
  return LUMEN_STATUS_UNSUPPORTED;
}

static lumen_status_t stub_program_unregister(lumen_qual_program_t program) {
  (void)program;
  return LUMEN_STATUS_UNSUPPORTED;
}

static lumen_qual_submit_receipt_v1 unsupported_receipt(void) {
  lumen_qual_submit_receipt_v1 receipt = {0};
  receipt.struct_size = (uint32_t)sizeof(receipt);
  receipt.abi_major = LUMEN_QUALIFICATION_ABI_MAJOR;
  receipt.abi_minor = LUMEN_QUALIFICATION_ABI_MINOR;
  receipt.call_status = LUMEN_STATUS_UNSUPPORTED;
  receipt.disposition = LUMEN_QUAL_REJECTED;
  return receipt;
}

static lumen_qual_submit_receipt_v1
stub_submit_command(lumen_queue_t queue, const lumen_command_v1 *command) {
  (void)queue;
  (void)command;
  return unsupported_receipt();
}

static lumen_qual_submit_receipt_v1
stub_launch(lumen_queue_t queue, const lumen_qual_launch_desc_v1 *desc) {
  (void)queue;
  (void)desc;
  return unsupported_receipt();
}

static lumen_status_t stub_query_memory(lumen_device_t device,
                                        lumen_qual_memory_v1 *out_memory) {
  (void)device;
  (void)out_memory;
  return LUMEN_STATUS_UNSUPPORTED;
}

static lumen_status_t
stub_read_telemetry(lumen_device_t device,
                    lumen_qual_telemetry_v1 *out_telemetry) {
  (void)device;
  (void)out_telemetry;
  return LUMEN_STATUS_UNSUPPORTED;
}

static lumen_qual_recovery_proof_v1 stub_recover(lumen_context_t context,
                                                 uint64_t expected_epoch) {
  (void)context;
  (void)expected_epoch;
  lumen_qual_recovery_proof_v1 proof = {0};
  proof.struct_size = (uint32_t)sizeof(proof);
  proof.abi_major = LUMEN_QUALIFICATION_ABI_MAJOR;
  proof.abi_minor = LUMEN_QUALIFICATION_ABI_MINOR;
  proof.call_status = LUMEN_STATUS_UNSUPPORTED;
  proof.mechanism = LUMEN_QUAL_RECOVERY_NONE;
  return proof;
}

static lumen_qualification_api_v1 make_api(void) {
  lumen_qualification_api_v1 api = {0};
  api.struct_size = (uint32_t)sizeof(api);
  api.abi_major = LUMEN_QUALIFICATION_ABI_MAJOR;
  api.abi_minor = LUMEN_QUALIFICATION_ABI_MINOR;
  api.base_api = lumen_cpu_backend_api();
  api.program_register = stub_program_register;
  api.program_unregister = stub_program_unregister;
  api.submit_command = stub_submit_command;
  api.launch = stub_launch;
  api.query_memory = stub_query_memory;
  api.read_telemetry = stub_read_telemetry;
  api.recover = stub_recover;
  return api;
}

int main(void) {
  lumen_qualification_api_v1 api = make_api();

  if (LUMEN_QUALIFICATION_ABI_MAJOR != 1u) {
    return 1;
  }
  if (api.program_register == NULL || api.program_unregister == NULL ||
      api.submit_command == NULL || api.launch == NULL ||
      api.query_memory == NULL || api.read_telemetry == NULL ||
      api.recover == NULL) {
    return 2;
  }
  if (lumen_qualification_api_validate(NULL) != LUMEN_STATUS_INVALID_INPUT) {
    return 3;
  }
  if (lumen_qualification_api_validate(&api) != LUMEN_STATUS_OK) {
    return 4;
  }

  api.struct_size = 0;
  if (lumen_qualification_api_validate(&api) != LUMEN_STATUS_UNSUPPORTED) {
    return 5;
  }

  api = make_api();
  api.abi_major = 2;
  if (lumen_qualification_api_validate(&api) != LUMEN_STATUS_UNSUPPORTED) {
    return 6;
  }

  api = make_api();
  api.base_api = NULL;
  if (lumen_qualification_api_validate(&api) != LUMEN_STATUS_UNSUPPORTED) {
    return 7;
  }

  api = make_api();
  api.launch = NULL;
  if (lumen_qualification_api_validate(&api) != LUMEN_STATUS_UNSUPPORTED) {
    return 8;
  }

  return 0;
}
