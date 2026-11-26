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

// Checked size, offset, and alignment arithmetic.
//
// Every helper rejects an unrepresentable result with Status::invalid_input
// rather than wrapping. Callers must validate untrusted sizes before allocating
// or indexing; layer-specific code may translate the status (for example to
// Status::corrupt while parsing a model artifact).

#pragma once

#include <cstddef>
#include <limits>

#include "core/status.h"

namespace lumen {

inline Result<std::size_t> checked_add(std::size_t lhs, std::size_t rhs) {
  constexpr std::size_t max = std::numeric_limits<std::size_t>::max();
  if (rhs > max - lhs) {
    return Result<std::size_t>::error(Status::invalid_input);
  }
  return Result<std::size_t>::ok(lhs + rhs);
}

inline Result<std::size_t> checked_mul(std::size_t lhs, std::size_t rhs) {
  constexpr std::size_t max = std::numeric_limits<std::size_t>::max();
  if (lhs != 0 && rhs > max / lhs) {
    return Result<std::size_t>::error(Status::invalid_input);
  }
  return Result<std::size_t>::ok(lhs * rhs);
}

// Requires offset + length <= bound without overflowing.
inline Status checked_range(std::size_t offset, std::size_t length,
                            std::size_t bound) {
  if (offset > bound || length > bound - offset) {
    return Status::invalid_input;
  }
  return Status::ok;
}

// Alignment must be non-zero and a power of two.
inline Status checked_alignment(std::size_t alignment) {
  if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
    return Status::invalid_input;
  }
  return Status::ok;
}

inline Result<std::size_t> checked_align_up(std::size_t value,
                                            std::size_t alignment) {
  if (const Status status = checked_alignment(alignment);
      status != Status::ok) {
    return Result<std::size_t>::error(status);
  }
  constexpr std::size_t max = std::numeric_limits<std::size_t>::max();
  const std::size_t mask = alignment - 1;
  if (value > max - mask) {
    return Result<std::size_t>::error(Status::invalid_input);
  }
  return Result<std::size_t>::ok((value + mask) & ~mask);
}

} // namespace lumen
