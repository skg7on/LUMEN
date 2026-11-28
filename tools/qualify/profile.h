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

// Frozen-profile validation for the qualification harness.
//
// Validation rejects zero, negative, non-finite, or unrepresentable bounds.
// It proves only that a profile is structurally usable; it never claims the
// target is qualified.

#pragma once

#include "core/status.h"
#include "qualify/types.h"

namespace lumen::qualification {

// Checks every budget and count for positivity and overflow.
Result<RunLimits> validate_limits(const RunLimits &limits);

// Checks that the target identity is complete and the limits are usable.
Status validate_profile(const TargetIdentity &target, const RunLimits &limits);

} // namespace lumen::qualification
