// Tensor layout and view validation: shapes, strides, alignment, and byte
// ranges must be provably in bounds before any data access.

#include "check.h"

#include <array>
#include <cstddef>
#include <limits>
#include <span>

#include "core/status.h"
#include "model/tensor_view.h"

using lumen::DType;
using lumen::Status;
using lumen::TensorLayout;
using lumen::TensorView;

namespace {
constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();
} // namespace

static void dtype_sizes_are_exact() {
  CHECK(lumen::dtype_size(DType::f32).value() == 4);
  CHECK(lumen::dtype_size(DType::f16).value() == 2);
  CHECK(lumen::dtype_size(DType::bf16).value() == 2);
  CHECK(lumen::dtype_size(DType::i8).value() == 1);
  CHECK(lumen::dtype_size(DType::u8).value() == 1);
  CHECK(lumen::dtype_size(DType::i32).value() == 4);
  CHECK(lumen::dtype_size(DType::u32).value() == 4);
  CHECK(lumen::dtype_size(static_cast<DType>(99)).status() ==
        Status::unsupported);
}

static void contiguous_layout_computes_strides_and_bytes() {
  constexpr std::size_t shape[] = {2, 3, 4};
  const auto layout = TensorLayout::contiguous(DType::f32, shape, 64);
  REQUIRE(layout.ok());

  CHECK(layout.value().rank() == 3);
  CHECK(layout.value().dim(0) == 2);
  CHECK(layout.value().dim(1) == 3);
  CHECK(layout.value().dim(2) == 4);
  CHECK(layout.value().stride(0) == 12);
  CHECK(layout.value().stride(1) == 4);
  CHECK(layout.value().stride(2) == 1);
  CHECK(layout.value().element_count() == 24);
  CHECK(layout.value().byte_size() == 96);
  CHECK(layout.value().required_alignment() == 64);
}

static void layout_rejects_invalid_shapes() {
  const std::array<std::size_t, 0> empty{};
  CHECK(TensorLayout::contiguous(DType::f32,
                                 std::span<const std::size_t>{empty}, 4)
            .status() == Status::invalid_input);

  constexpr std::size_t zero_dim[] = {2, 0};
  CHECK(TensorLayout::contiguous(DType::f32, zero_dim, 4).status() ==
        Status::invalid_input);

  constexpr std::size_t too_ranked[] = {1, 2, 3, 4, 5};
  CHECK(TensorLayout::contiguous(DType::f32, too_ranked, 4).status() ==
        Status::invalid_input);

  constexpr std::size_t valid_shape[] = {4};
  CHECK(TensorLayout::contiguous(static_cast<DType>(99), valid_shape, 4)
            .status() == Status::unsupported);
}

static void layout_rejects_overflowing_extents() {
  constexpr std::size_t huge_shape[] = {kMax, 2};
  CHECK(TensorLayout::contiguous(DType::u8, huge_shape, 1).status() ==
        Status::invalid_input);

  constexpr std::size_t byte_overflow_shape[] = {kMax / 2, 1};
  CHECK(TensorLayout::contiguous(DType::f32, byte_overflow_shape, 1).status() ==
        Status::invalid_input);

  constexpr std::size_t bad_alignment_shape[] = {4};
  CHECK(TensorLayout::contiguous(DType::f32, bad_alignment_shape, 3).status() ==
        Status::invalid_input);
}

static void strided_layout_uses_the_span_of_the_strides() {
  constexpr std::size_t shape[] = {2, 3};
  constexpr std::size_t transposed[] = {1, 2};
  const auto layout = TensorLayout::strided(DType::f32, shape, transposed, 4);
  REQUIRE(layout.ok());

  CHECK(layout.value().stride(0) == 1);
  CHECK(layout.value().stride(1) == 2);
  CHECK(layout.value().element_count() == 6); // (2-1)*1 + (3-1)*2 + 1
  CHECK(layout.value().byte_size() == 24);

  constexpr std::size_t zero_stride[] = {0, 1};
  CHECK(TensorLayout::strided(DType::f32, shape, zero_stride, 4).status() ==
        Status::invalid_input);

  constexpr std::size_t wrong_rank[] = {1};
  CHECK(TensorLayout::strided(DType::f32, shape, wrong_rank, 4).status() ==
        Status::invalid_input);

  constexpr std::size_t huge_shape[] = {2, kMax};
  constexpr std::size_t huge_strides[] = {1, kMax};
  CHECK(
      TensorLayout::strided(DType::u8, huge_shape, huge_strides, 1).status() ==
      Status::invalid_input);
}

static void view_binds_within_storage_and_alignment() {
  alignas(64) std::byte storage[128]{};
  constexpr std::size_t shape[] = {2, 3};
  const auto layout = TensorLayout::contiguous(DType::f32, shape, 8);
  REQUIRE(layout.ok());

  const auto view =
      TensorView::bind(layout.value(), storage, sizeof(storage), 8);
  REQUIRE(view.ok());
  CHECK(view.value().data() == storage + 8);
  CHECK(view.value().byte_offset() == 8);
  CHECK(view.value().bytes().size() == 24);

  CHECK(TensorView::bind(layout.value(), storage, 16, 0).status() ==
        Status::invalid_input);
  CHECK(
      TensorView::bind(layout.value(), nullptr, sizeof(storage), 0).status() ==
      Status::invalid_input);
  CHECK(TensorView::bind(layout.value(), storage, sizeof(storage), kMax)
            .status() == Status::invalid_input);

  const auto aligned = TensorLayout::contiguous(DType::f32, shape, 64);
  REQUIRE(aligned.ok());
  CHECK(TensorView::bind(aligned.value(), storage + 1, sizeof(storage) - 1, 0)
            .status() == Status::invalid_input);
}

int main() {
  dtype_sizes_are_exact();
  contiguous_layout_computes_strides_and_bytes();
  layout_rejects_invalid_shapes();
  layout_rejects_overflowing_extents();
  strided_layout_uses_the_span_of_the_strides();
  view_binds_within_storage_and_alignment();
  return lumen::test::summary("unit-tensor-view");
}
