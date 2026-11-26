// Deterministic simulated-time controls for the mock adapter.

#include "backend/mock/mock_control.h"

#include "backend/common/runtime.h"

extern "C" lumen_status_t
lumen_mock_set_completion_delay_ticks(lumen_context_t context,
                                      uint64_t delay_ticks) {
  return lumen::backend::mock_set_completion_delay(context, delay_ticks);
}

extern "C" lumen_status_t lumen_mock_advance_ticks(lumen_context_t context,
                                                   uint64_t ticks) {
  return lumen::backend::mock_advance(context, ticks);
}

extern "C" lumen_status_t lumen_mock_pending_commands(lumen_context_t context,
                                                      uint64_t *out_count) {
  return lumen::backend::mock_pending_count(context, out_count);
}
