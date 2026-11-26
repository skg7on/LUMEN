/*
 * LUMEN versioned C backend ABI (v1).
 *
 * This header is the public boundary between the serving engine and a device
 * adapter. It is C11-compatible, has no STL or C++ exception surface, and uses
 * fixed-width records plus opaque handles. A backend provides one immutable
 * `lumen_backend_api_v1` function table; hosts validate it with
 * `lumen_backend_api_validate` before use.
 *
 * Ownership and lifecycle rules:
 *  - Context/device: the backend opens a context; the owning worker opens one
 *    or more devices. Close a device only after all of its buffers and queues
 *    retire. Close a context only after its devices, queues, buffers, and
 *    events retire (or through an explicit backend-specific teardown).
 *  - Buffer: free only after every referencing command has fenced (its event
 *    reached COMPLETE or FAILED). Destroying a buffer with a live referencing
 *    command fails with LUMEN_STATUS_INVALID_INPUT and has no side effects.
 *  - Queue: destroy only after all submitted commands have fenced. Queue order
 *    is declared in the device capabilities; v1 adapters advertise in-order.
 *  - Event: `queue_submit` returns an owned completion handle. Poll or wait it
 *    to a final state before releasing it exactly once. Releasing a pending
 *    event fails with LUMEN_STATUS_INVALID_INPUT. Use of a released handle
 *    returns LUMEN_STATUS_STALE.
 *
 * Failed calls never write through output parameters and never consume or
 * retire a handle unless explicitly documented. Submission success is not
 * completion.
 *
 * Threading: an adapter instance is not required to be thread-safe in P0.
 * Handles must not be used concurrently with destruction of their owning
 * context.
 */

#ifndef LUMEN_BACKEND_H
#define LUMEN_BACKEND_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LUMEN_BACKEND_ABI_MAJOR 1u
#define LUMEN_BACKEND_ABI_MINOR 0u

#ifndef LUMEN_API
#define LUMEN_API
#endif

/* Status categories. Values are frozen and match the internal lumen::Status. */
typedef enum lumen_status {
  LUMEN_STATUS_OK = 0,
  LUMEN_STATUS_INVALID_INPUT = 1,
  LUMEN_STATUS_UNSUPPORTED = 2,
  LUMEN_STATUS_RESOURCE_EXHAUSTED = 3,
  LUMEN_STATUS_STALE = 4,
  LUMEN_STATUS_TIMEOUT = 5,
  LUMEN_STATUS_DEVICE_ERROR = 6,
  LUMEN_STATUS_CORRUPT = 7
} lumen_status_t;

/* Opaque handles. Never dereference these pointers; they are backend tokens. */
typedef struct lumen_context lumen_context;
typedef struct lumen_device lumen_device;
typedef struct lumen_buffer lumen_buffer;
typedef struct lumen_queue lumen_queue;
typedef struct lumen_event lumen_event;

typedef lumen_context* lumen_context_t;
typedef lumen_device* lumen_device_t;
typedef lumen_buffer* lumen_buffer_t;
typedef lumen_queue* lumen_queue_t;
typedef lumen_event* lumen_event_t;

/* Records carry their own size and ABI version. The host initializes these
 * fields before passing a writable record; the backend validates them. */
typedef struct lumen_version_v1 {
  uint32_t struct_size;
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t flags;
} lumen_version_v1;

typedef enum lumen_memory_kind {
  LUMEN_MEMORY_HOST = 1,
  LUMEN_MEMORY_DEVICE = 2,
  LUMEN_MEMORY_UNIFIED = 3
} lumen_memory_kind_t;

typedef enum lumen_command_kind {
  LUMEN_COMMAND_NOP = 1,
  LUMEN_COMMAND_COPY = 2,
  LUMEN_COMMAND_FILL = 3
} lumen_command_kind_t;

typedef enum lumen_queue_order {
  LUMEN_QUEUE_ORDER_IN_ORDER = 0,
  LUMEN_QUEUE_ORDER_UNSPECIFIED = 1
} lumen_queue_order_t;

typedef enum lumen_capability_flags {
  LUMEN_CAP_HOST_MEMORY = 1u << 0,
  LUMEN_CAP_DEVICE_MEMORY = 1u << 1,
  LUMEN_CAP_ASYNC_COMPLETION = 1u << 2,
  LUMEN_CAP_DELAYED_COMPLETION = 1u << 3,
  LUMEN_CAP_SIMULATED_TIME = 1u << 4
} lumen_capability_flags_t;

/* Buffer creation descriptor. The backend ignores `flags` in v1. */
typedef struct lumen_buffer_desc_v1 {
  uint32_t struct_size;
  uint32_t memory_kind; /* lumen_memory_kind_t */
  uint64_t size_bytes;
  uint32_t alignment;
  uint32_t flags;
} lumen_buffer_desc_v1;

/* Queue creation descriptor. `max_inflight == 0` selects the backend default. */
typedef struct lumen_queue_desc_v1 {
  uint32_t struct_size;
  uint32_t flags;
  uint64_t max_inflight;
} lumen_queue_desc_v1;

/* One submitted command. COPY requires src and dst ranges; FILL requires dst;
 * NOP ignores buffers. Offsets and lengths are checked against buffer sizes. */
typedef struct lumen_command_v1 {
  uint32_t struct_size;
  uint32_t kind; /* lumen_command_kind_t */
  uint64_t dst_offset;
  uint64_t src_offset;
  uint64_t length;
  uint32_t fill_byte;
  uint32_t flags;
  lumen_buffer_t dst;
  lumen_buffer_t src;
} lumen_command_v1;

/* Device capability snapshot. `supports_reset` is 0 or 1. */
typedef struct lumen_capabilities_v1 {
  uint32_t struct_size;
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t flags;
  uint64_t max_buffer_bytes;
  uint64_t max_inflight_commands;
  uint32_t memory_alignment_min;
  uint32_t queue_order;
  uint32_t supports_reset;
  uint32_t reserved;
} lumen_capabilities_v1;

typedef enum lumen_event_status {
  LUMEN_EVENT_PENDING = 0,
  LUMEN_EVENT_COMPLETE = 1,
  LUMEN_EVENT_FAILED = 2
} lumen_event_status_t;

/* Completion record. `sequence` is assigned by the backend in submission
 * order. Poll/wait does not release the event. */
typedef struct lumen_event_state_v1 {
  uint32_t struct_size;
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t status;         /* lumen_event_status_t */
  uint32_t command_status; /* lumen_status_t of the executed command */
  uint32_t flags;
  uint64_t sequence;
} lumen_event_state_v1;

typedef struct lumen_backend_api_v1 lumen_backend_api_v1;

/*
 * Versioned backend function table. `struct_size` must equal
 * sizeof(lumen_backend_api_v1) for this header revision; a different size or
 * major/minor version is rejected.
 */
struct lumen_backend_api_v1 {
  uint32_t struct_size;
  uint32_t abi_major;
  uint32_t abi_minor;
  uint32_t flags;

  lumen_status_t (*backend_open)(const lumen_backend_api_v1* api,
                                 lumen_context_t* out_context);
  lumen_status_t (*context_close)(lumen_context_t context);
  /* Succeeds only when the context has no live children. Resets simulated
   * device state (mock time) but never invalidates retired handles. */
  lumen_status_t (*context_reset)(lumen_context_t context);

  lumen_status_t (*device_open)(lumen_context_t context, lumen_device_t* out_device);
  lumen_status_t (*device_close)(lumen_device_t device);
  lumen_status_t (*device_capabilities)(lumen_device_t device,
                                        lumen_capabilities_v1* out_capabilities);

  lumen_status_t (*buffer_create)(lumen_device_t device,
                                  const lumen_buffer_desc_v1* desc,
                                  lumen_buffer_t* out_buffer);
  lumen_status_t (*buffer_destroy)(lumen_buffer_t buffer);
  /* Host mapping of a HOST buffer. Map/unmap are not reference counted in v1. */
  lumen_status_t (*buffer_map)(lumen_buffer_t buffer, void** out_data, uint64_t* out_size);
  lumen_status_t (*buffer_unmap)(lumen_buffer_t buffer);

  lumen_status_t (*queue_create)(lumen_device_t device,
                                 const lumen_queue_desc_v1* desc,
                                 lumen_queue_t* out_queue);
  lumen_status_t (*queue_destroy)(lumen_queue_t queue);
  lumen_status_t (*queue_submit)(lumen_queue_t queue,
                                 const lumen_command_v1* command,
                                 lumen_event_t* out_event);

  /* Returns LUMEN_STATUS_OK and a final or pending state for a live event. */
  lumen_status_t (*event_poll)(lumen_event_t event, lumen_event_state_v1* out_state);
  /* Returns LUMEN_STATUS_OK for a final state or LUMEN_STATUS_TIMEOUT while
   * pending. Adapters with simulated time do not advance it implicitly. */
  lumen_status_t (*event_wait)(lumen_event_t event,
                               uint64_t timeout_ns,
                               lumen_event_state_v1* out_state);
  /* Releases an event in a final state exactly once. */
  lumen_status_t (*event_release)(lumen_event_t event);
};

/* Built-in adapters; neither requires a vendor SDK or device. */
LUMEN_API const lumen_backend_api_v1* lumen_cpu_backend_api(void);
LUMEN_API const lumen_backend_api_v1* lumen_mock_backend_api(void);

/* Validates presence, size, ABI version, and required function pointers.
 * Returns LUMEN_STATUS_UNSUPPORTED for an incompatible size or version. */
LUMEN_API lumen_status_t lumen_backend_api_validate(const lumen_backend_api_v1* api);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif /* LUMEN_BACKEND_H */
