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

// Internal status vocabulary shared by LUMEN modules.
//
// The numeric values are part of the public C ABI contract in
// include/lumen/backend.h; do not reorder or renumber entries.

#pragma once

#include <optional>
#include <string_view>
#include <utility>

namespace lumen {

enum class Status {
  ok = 0,
  invalid_input = 1,
  unsupported = 2,
  resource_exhausted = 3,
  stale = 4,
  timeout = 5,
  device_error = 6,
  corrupt = 7,
};

constexpr std::string_view status_name(Status status) noexcept {
  switch (status) {
  case Status::ok:
    return "ok";
  case Status::invalid_input:
    return "invalid_input";
  case Status::unsupported:
    return "unsupported";
  case Status::resource_exhausted:
    return "resource_exhausted";
  case Status::stale:
    return "stale";
  case Status::timeout:
    return "timeout";
  case Status::device_error:
    return "device_error";
  case Status::corrupt:
    return "corrupt";
  }
  return "unknown";
}

// Result<T> carries either a value or a failure status. Callers must check
// ok()/status() before accessing value(); value() is only meaningful after a
// successful result.
template <typename T> class Result {
public:
  static Result ok(T value) { return Result(std::move(value)); }
  static Result error(Status status) { return Result(status); }

  [[nodiscard]] bool ok() const noexcept { return status_ == Status::ok; }
  [[nodiscard]] Status status() const noexcept { return status_; }

  [[nodiscard]] T &value() & { return *value_; }
  [[nodiscard]] const T &value() const & { return *value_; }
  [[nodiscard]] T &&value() && { return std::move(*value_); }

private:
  // Construct only through the factories so a result cannot carry
  // Status::ok without a value.
  Result(T value) : status_(Status::ok), value_(std::move(value)) {}
  Result(Status status) : status_(status) {}

  Status status_;
  std::optional<T> value_;
};

} // namespace lumen
