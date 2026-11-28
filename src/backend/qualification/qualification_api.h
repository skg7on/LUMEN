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

/*
 * Private, experimental qualification-only C extension (v1).
 *
 * This header is not part of the public backend ABI in include/lumen/backend.h.
 * It exists to qualify one NPU's compute launch and forced-recovery behavior
 * before any production ABI change. It is statically linked, versioned, and
 * must not be treated as the serving contract.
 *
 * The extension points at a validated base `lumen_backend_api_v1` and reuses
 * its opaque handles for ordinary buffer/queue/event operations. It adds only:
 *  - opaque program registration/unregistration;
 *  - bounded compute launch and command submission;
 *  - memory and telemetry queries;
 *  - a context-scoped forced-recovery proof.
 *
 * Submission functions return a receipt by value so an error can still carry a
 * completion obligation. Dispositions:
 *  - `REJECTED`: no command references the resources; immediate rollback is
 *    safe only when `call_status` is not OK.
 *  - `ACCEPTED`: an owned completion fence is required.
 *  - `UNCERTAIN`: the full pre-reserved resource set is retained even if no
 *    fence is available. Only a real completion or a successful recovery proof
 *    may retire it; a timeout is not cancellation.
 *
 * No STL types or C++ exceptions cross this boundary.
 */

#ifndef LUMEN_QUALIFICATION_API_H
#define LUMEN_QUALIFICATION_API_H

#include <stdint.h>

#include "lumen/backend.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LUMEN_QUALIFICATION_ABI_MAJOR 1u
#define LUMEN_QUALIFICATION_ABI_MINOR 0u

/* Opaque registered-program handle. */
typedef struct lumen_qual_program lumen_qual_program;
typedef lumen_qual_program *lumen_qual_program_t;

typedef enum lumen_qual_disposition {
  LUMEN_QUAL_REJECTED = 0,
  LUMEN_QUAL_ACCEPTED = 1,
  LUMEN_QUAL_UNCERTAIN = 2
} lumen_qual_disposition_t;

typedef enum lumen_qual_recovery_mechanism {
  LUMEN_QUAL_RECOVERY_NONE = 0,
  LUMEN_QUAL_RECOVERY_CONTEXT = 1,
  LUMEN_QUAL_RECOVERY_DEVICE = 2
} lumen_qual_recovery_mechanism_t;

typedef enum lumen_qual_access {
  LUMEN_QUAL_ACCESS_READ = 1u << 0,
  LUMEN_QUAL_ACCESS_WRITE = 1u << 1
} lumen_qual_access_t;

/* Program bytes are host memory only during registration. A zero digest means
 * "not provided"; it never means verified. */
typedef struct lumen_qual_program_desc_v1 {
  uint32_t struct_size;
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t flags;
  const uint8_t *program_bytes;
  uint64_t program_size;
  uint8_t digest[32];
  uint32_t digest_kind;
  uint32_t reserved;
} lumen_qual_program_desc_v1;

typedef struct lumen_qual_binding_v1 {
  uint32_t struct_size;
  uint32_t access; /* lumen_qual_access_t */
  lumen_buffer_t buffer;
  uint64_t offset;
  uint64_t length;
} lumen_qual_binding_v1;

typedef struct lumen_qual_shape_v1 {
  uint64_t sizes[4];
  uint32_t rank;
  uint32_t dtype;
  uint32_t layout;
  uint32_t reserved;
} lumen_qual_shape_v1;

typedef struct lumen_qual_launch_desc_v1 {
  uint32_t struct_size;
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t flags;
  lumen_qual_program_t program;
  uint64_t context_epoch;
  uint64_t workspace_offset;
  uint64_t workspace_bytes;
  const lumen_qual_binding_v1 *bindings;
  uint32_t binding_count;
  uint32_t reserved0;
  lumen_qual_shape_v1 input_shape;
  lumen_qual_shape_v1 output_shape;
} lumen_qual_launch_desc_v1;

/* A receipt is returned by value. The caller must not shrink the conservative
 * lease set based only on an adapter-supplied `touched_resource_set_id`. */
typedef struct lumen_qual_submit_receipt_v1 {
  uint32_t struct_size;
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t call_status; /* lumen_status_t */
  uint32_t disposition; /* lumen_qual_disposition_t */
  uint32_t reserved0;
  uint64_t context_epoch;
  lumen_event_t completion_event; /* owned only when accepted */
  uint64_t touched_resource_set_id;
} lumen_qual_submit_receipt_v1;

typedef struct lumen_qual_memory_v1 {
  uint32_t struct_size;
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t validity_flags;
  uint64_t total_bytes;
  uint64_t free_bytes;
  uint64_t reserved_bytes;
  uint32_t alignment_min;
  uint32_t reserved;
} lumen_qual_memory_v1;

typedef struct lumen_qual_telemetry_v1 {
  uint32_t struct_size;
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t validity_flags;
  double temperature_celsius;
  double power_watts;
  uint64_t monotonic_ns;
} lumen_qual_telemetry_v1;

/* A proof is valid only when `call_status` is OK, `quiescent` and
 * `old_handles_invalidated` are both set, `old_epoch` matches the affected
 * lease epoch, and `new_epoch == old_epoch + 1` without wrapping. */
typedef struct lumen_qual_recovery_proof_v1 {
  uint32_t struct_size;
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t call_status; /* lumen_status_t */
  uint64_t old_epoch;
  uint64_t new_epoch;
  uint32_t quiescent;
  uint32_t old_handles_invalidated;
  uint32_t mechanism; /* lumen_qual_recovery_mechanism_t */
  uint32_t reserved;
  uint64_t evidence_id;
} lumen_qual_recovery_proof_v1;

typedef struct lumen_qualification_api_v1 lumen_qualification_api_v1;

struct lumen_qualification_api_v1 {
  uint32_t struct_size;
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t flags;
  const lumen_backend_api_v1 *base_api;

  lumen_status_t (*program_register)(lumen_device_t,
                                     const lumen_qual_program_desc_v1 *,
                                     lumen_qual_program_t *);
  lumen_status_t (*program_unregister)(lumen_qual_program_t);
  lumen_qual_submit_receipt_v1 (*submit_command)(lumen_queue_t,
                                                 const lumen_command_v1 *);
  lumen_qual_submit_receipt_v1 (*launch)(lumen_queue_t,
                                         const lumen_qual_launch_desc_v1 *);
  lumen_status_t (*query_memory)(lumen_device_t, lumen_qual_memory_v1 *);
  lumen_status_t (*read_telemetry)(lumen_device_t, lumen_qual_telemetry_v1 *);
  lumen_qual_recovery_proof_v1 (*recover)(lumen_context_t,
                                          uint64_t expected_epoch);
};

/* Validates exact table size, extension version, the base table, and every
 * required function pointer. Never treats an incompatible table as usable. */
LUMEN_API lumen_status_t
lumen_qualification_api_validate(const lumen_qualification_api_v1 *api);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LUMEN_QUALIFICATION_API_H */
