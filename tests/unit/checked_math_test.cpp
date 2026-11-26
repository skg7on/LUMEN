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

// Checked size/offset/alignment arithmetic must reject overflow before any
// tensor or model data is accessed.

#include "check.h"

#include <cstddef>
#include <limits>

#include "core/checked_math.h"

using lumen::Status;

namespace {
constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();
} // namespace

static void addition_rejects_overflow() {
  CHECK(lumen::checked_add(0, 0).value() == 0);
  CHECK(lumen::checked_add(2, 3).value() == 5);
  CHECK(lumen::checked_add(kMax, 0).value() == kMax);

  CHECK(lumen::checked_add(kMax, 1).status() == Status::invalid_input);
  CHECK(lumen::checked_add(1, kMax).status() == Status::invalid_input);
}

static void multiplication_rejects_overflow() {
  CHECK(lumen::checked_mul(0, kMax).value() == 0);
  CHECK(lumen::checked_mul(kMax, 0).value() == 0);
  CHECK(lumen::checked_mul(4, 8).value() == 32);
  CHECK(lumen::checked_mul(1, kMax).value() == kMax);

  CHECK(lumen::checked_mul(kMax, 2).status() == Status::invalid_input);
  CHECK(lumen::checked_mul(2, kMax).status() == Status::invalid_input);
}

static void ranges_must_fit_the_bound() {
  CHECK(lumen::checked_range(0, 0, 0) == Status::ok);
  CHECK(lumen::checked_range(0, 10, 10) == Status::ok);
  CHECK(lumen::checked_range(7, 3, 10) == Status::ok);
  CHECK(lumen::checked_range(10, 0, 10) == Status::ok);

  CHECK(lumen::checked_range(1, 10, 10) == Status::invalid_input);
  CHECK(lumen::checked_range(11, 0, 10) == Status::invalid_input);
  CHECK(lumen::checked_range(kMax, 1, kMax) == Status::invalid_input);
}

static void alignment_must_be_a_power_of_two() {
  CHECK(lumen::checked_alignment(1) == Status::ok);
  CHECK(lumen::checked_alignment(2) == Status::ok);
  CHECK(lumen::checked_alignment(64) == Status::ok);
  CHECK(lumen::checked_alignment(4096) == Status::ok);

  CHECK(lumen::checked_alignment(0) == Status::invalid_input);
  CHECK(lumen::checked_alignment(3) == Status::invalid_input);
  CHECK(lumen::checked_alignment(48) == Status::invalid_input);
}

static void align_up_is_checked() {
  CHECK(lumen::checked_align_up(0, 64).value() == 0);
  CHECK(lumen::checked_align_up(1, 64).value() == 64);
  CHECK(lumen::checked_align_up(63, 64).value() == 64);
  CHECK(lumen::checked_align_up(64, 64).value() == 64);
  CHECK(lumen::checked_align_up(65, 64).value() == 128);
  CHECK(lumen::checked_align_up(kMax, 1).value() == kMax);

  CHECK(lumen::checked_align_up(kMax, 64).status() == Status::invalid_input);
  CHECK(lumen::checked_align_up(8, 0).status() == Status::invalid_input);
  CHECK(lumen::checked_align_up(8, 3).status() == Status::invalid_input);
}

int main() {
  addition_rejects_overflow();
  multiplication_rejects_overflow();
  ranges_must_fit_the_bound();
  alignment_must_be_a_power_of_two();
  align_up_is_checked();
  return lumen::test::summary("unit-checked-math");
}
