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

// Checked tensor layout and view primitives.
//
// A TensorLayout is pure metadata: dtype, rank <= 4, shape, strides, byte
// extent, and required alignment. TensorView::bind proves that a layout's byte
// extent and alignment fit inside a caller-provided storage region before any
// access. Scale parameters are host sizes; on-device physical layouts stay
// inside backend adapters.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "core/checked_math.h"
#include "core/status.h"

namespace lumen {

enum class DType {
  f32,
  f16,
  bf16,
  i8,
  u8,
  i32,
  u32,
};

inline constexpr std::size_t kMaxTensorRank = 4;

inline constexpr const char *dtype_name(DType dtype) {
  switch (dtype) {
  case DType::f32:
    return "f32";
  case DType::f16:
    return "f16";
  case DType::bf16:
    return "bf16";
  case DType::i8:
    return "i8";
  case DType::u8:
    return "u8";
  case DType::i32:
    return "i32";
  case DType::u32:
    return "u32";
  }
  return "unknown";
}

inline Result<std::size_t> dtype_size(DType dtype) {
  switch (dtype) {
  case DType::f32:
    return Result<std::size_t>::ok(4);
  case DType::f16:
  case DType::bf16:
    return Result<std::size_t>::ok(2);
  case DType::i8:
  case DType::u8:
    return Result<std::size_t>::ok(1);
  case DType::i32:
  case DType::u32:
    return Result<std::size_t>::ok(4);
  }
  return Result<std::size_t>::error(Status::unsupported);
}

class TensorLayout {
public:
  // Row-major layout with unit stride on the last dimension.
  static Result<TensorLayout> contiguous(DType dtype,
                                         std::span<const std::size_t> shape,
                                         std::size_t required_alignment = 1) {
    std::array<std::size_t, kMaxTensorRank> strides{};
    if (shape.empty() || shape.size() > kMaxTensorRank) {
      return Result<TensorLayout>::error(Status::invalid_input);
    }
    std::size_t stride = 1;
    for (std::size_t i = shape.size(); i-- > 0;) {
      strides[i] = stride;
      const auto next = checked_mul(stride, shape[i]);
      if (!next.ok()) {
        return Result<TensorLayout>::error(next.status());
      }
      stride = next.value();
    }
    return make(dtype, shape, strides, required_alignment);
  }

  // Arbitrary positive strides. The byte extent covers the span from element
  // zero to the furthest element address (it is not necessarily a dense
  // allocation size).
  static Result<TensorLayout> strided(DType dtype,
                                      std::span<const std::size_t> shape,
                                      std::span<const std::size_t> strides,
                                      std::size_t required_alignment = 1) {
    if (shape.empty() || shape.size() > kMaxTensorRank ||
        strides.size() != shape.size()) {
      return Result<TensorLayout>::error(Status::invalid_input);
    }
    return make(dtype, shape, strides, required_alignment);
  }

  [[nodiscard]] DType dtype() const noexcept { return dtype_; }
  [[nodiscard]] std::size_t rank() const noexcept { return rank_; }
  [[nodiscard]] std::size_t dim(std::size_t axis) const { return shape_[axis]; }
  [[nodiscard]] std::size_t stride(std::size_t axis) const {
    return strides_[axis];
  }
  [[nodiscard]] std::size_t element_count() const noexcept {
    return element_count_;
  }
  [[nodiscard]] std::size_t byte_size() const noexcept { return byte_size_; }
  [[nodiscard]] std::size_t required_alignment() const noexcept {
    return required_alignment_;
  }

private:
  static Result<TensorLayout> make(DType dtype,
                                   std::span<const std::size_t> shape,
                                   std::span<const std::size_t> strides,
                                   std::size_t required_alignment) {
    const auto element_size = dtype_size(dtype);
    if (!element_size.ok()) {
      return Result<TensorLayout>::error(element_size.status());
    }
    if (checked_alignment(required_alignment) != Status::ok) {
      return Result<TensorLayout>::error(Status::invalid_input);
    }

    TensorLayout layout;
    layout.dtype_ = dtype;
    layout.rank_ = shape.size();
    layout.required_alignment_ = required_alignment;

    // The furthest element is at sum((dim_i - 1) * stride_i); the element
    // count is that span plus one.
    std::size_t span = 0;
    for (std::size_t i = 0; i < shape.size(); ++i) {
      if (shape[i] == 0 || strides[i] == 0) {
        return Result<TensorLayout>::error(Status::invalid_input);
      }
      layout.shape_[i] = shape[i];
      layout.strides_[i] = strides[i];
      const auto reach = checked_mul(shape[i] - 1, strides[i]);
      if (!reach.ok()) {
        return Result<TensorLayout>::error(reach.status());
      }
      const auto next = checked_add(span, reach.value());
      if (!next.ok()) {
        return Result<TensorLayout>::error(next.status());
      }
      span = next.value();
    }

    const auto elements = checked_add(span, 1);
    if (!elements.ok()) {
      return Result<TensorLayout>::error(elements.status());
    }
    const auto bytes = checked_mul(elements.value(), element_size.value());
    if (!bytes.ok()) {
      return Result<TensorLayout>::error(bytes.status());
    }
    layout.element_count_ = elements.value();
    layout.byte_size_ = bytes.value();
    return Result<TensorLayout>::ok(layout);
  }

  DType dtype_ = DType::f32;
  std::array<std::size_t, kMaxTensorRank> shape_{};
  std::array<std::size_t, kMaxTensorRank> strides_{};
  std::size_t rank_ = 0;
  std::size_t element_count_ = 0;
  std::size_t byte_size_ = 0;
  std::size_t required_alignment_ = 1;
};

// A validated window into caller-owned storage.
class TensorView {
public:
  static Result<TensorView> bind(const TensorLayout &layout,
                                 const void *storage, std::size_t storage_bytes,
                                 std::size_t byte_offset) {
    if (storage == nullptr) {
      return Result<TensorView>::error(Status::invalid_input);
    }
    if (checked_range(byte_offset, layout.byte_size(), storage_bytes) !=
        Status::ok) {
      return Result<TensorView>::error(Status::invalid_input);
    }
    const auto base = reinterpret_cast<std::uintptr_t>(storage);
    const std::uintptr_t max = static_cast<std::uintptr_t>(~std::uintptr_t{0});
    if (byte_offset > max - base) {
      return Result<TensorView>::error(Status::invalid_input);
    }
    const auto address = base + byte_offset;
    if (layout.required_alignment() != 1 &&
        address % static_cast<std::uintptr_t>(layout.required_alignment()) !=
            0) {
      return Result<TensorView>::error(Status::invalid_input);
    }

    TensorView view;
    view.layout_ = layout;
    view.data_ = static_cast<const std::byte *>(storage) + byte_offset;
    view.byte_offset_ = byte_offset;
    return Result<TensorView>::ok(view);
  }

  [[nodiscard]] const TensorLayout &layout() const noexcept { return layout_; }
  [[nodiscard]] const std::byte *data() const noexcept { return data_; }
  [[nodiscard]] std::size_t byte_offset() const noexcept {
    return byte_offset_;
  }
  [[nodiscard]] std::span<const std::byte> bytes() const noexcept {
    return std::span<const std::byte>{data_, layout_.byte_size()};
  }

private:
  TensorLayout layout_{};
  const std::byte *data_ = nullptr;
  std::size_t byte_offset_ = 0;
};

} // namespace lumen
