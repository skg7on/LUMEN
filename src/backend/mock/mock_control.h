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
 * Deterministic controls for the mock backend.
 *
 * Mock time is a logical tick counter. A submitted command becomes complete
 * when its queue-order completion tick is reached through
 * `lumen_mock_advance_ticks`. `event_wait` never advances simulated time, so a
 * positive timeout still returns LUMEN_STATUS_TIMEOUT while a pending command
 * exists. The default completion delay is zero, which completes a command at
 * submission like the CPU adapter.
 */

#ifndef LUMEN_MOCK_CONTROL_H
#define LUMEN_MOCK_CONTROL_H

#include "lumen/backend.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sets the delay applied to commands submitted after this call. */
LUMEN_API lumen_status_t lumen_mock_set_completion_delay_ticks(
    lumen_context_t context, uint64_t delay_ticks);

/* Advances simulated time by `ticks` and completes commands that are due, in
 * submission order. Overflow of the tick counter is rejected. */
LUMEN_API lumen_status_t lumen_mock_advance_ticks(lumen_context_t context,
                                                  uint64_t ticks);

/* Reports the number of submitted, not-yet-fenced commands. */
LUMEN_API lumen_status_t lumen_mock_pending_commands(lumen_context_t context,
                                                     uint64_t *out_count);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* LUMEN_MOCK_CONTROL_H */
