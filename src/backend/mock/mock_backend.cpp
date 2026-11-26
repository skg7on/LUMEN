// Mock adapter: host memory with deterministic delayed completion under the
// simulated tick clock controlled by mock_control.h.

#include "lumen/backend.h"

#include "backend/common/runtime.h"

namespace {

lumen_status_t open_mock(const lumen_backend_api_v1* api, lumen_context_t* out_context) {
  if (const lumen_status_t validated = lumen_backend_api_validate(api);
      validated != LUMEN_STATUS_OK) {
    return validated;
  }
  return lumen::backend::open(lumen::backend::Mode::mock, out_context);
}

constexpr lumen_backend_api_v1 kMockApi = {
    .struct_size = sizeof(lumen_backend_api_v1),
    .abi_major = LUMEN_BACKEND_ABI_MAJOR,
    .abi_minor = LUMEN_BACKEND_ABI_MINOR,
    .flags = 0,
    .backend_open = open_mock,
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

}  // namespace

extern "C" const lumen_backend_api_v1* lumen_mock_backend_api(void) { return &kMockApi; }
