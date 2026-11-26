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
