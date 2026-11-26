# Building and testing LUMEN (P0 foundation)

Status: implemented P0/LUMEN-001 foundation | Date: 2026-11-26

This page records the native build, test, and ABI foundation delivered for
roadmap P0.3 (GitHub issue #5). It does not describe an engine, model packer,
or serving binary: those remain future phase work.

## Prerequisites

- CMake 3.25 or newer
- Ninja 1.11 or newer
- A C++20/C11 host compiler (GCC 13+, Clang 17+, or Apple Clang 16+)

No vendor NPU SDK, driver, or device is required. Pinned minimums and the empty
third-party dependency list are recorded in
[`cmake/dependencies.lock.json`](../cmake/dependencies.lock.json); configure fails
if the lock is missing.

## Host presets

```bash
cmake --preset host-debug
cmake --build --preset host-debug
ctest --preset host-debug --output-on-failure --no-tests=error
```

`host-release` provides the same tests in a `Release` build. Test assertions use
`tests/support/check.h`, which never disables itself under `NDEBUG`.

`host-sanitize` builds the same suite with AddressSanitizer and
UndefinedBehaviorSanitizer:

```bash
cmake --preset host-sanitize
cmake --build --preset host-sanitize
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --preset host-sanitize --output-on-failure --no-tests=error
```

macOS does not support LeakSanitizer; omit `detect_leaks=1` there. On macOS the
preset may also need an explicit toolchain override
(`-DCMAKE_C_COMPILER=/usr/bin/clang -DCMAKE_CXX_COMPILER=/usr/bin/clang++`) if
the Homebrew ASan runtime does not start.

Registered host tests:

| CTest name | Coverage |
|---|---|
| `unit-build-smoke` | C++20 and host configuration |
| `unit-status` | `Status`/`Result<T>` contract |
| `unit-checked-math` | size/offset/alignment overflow rejection |
| `unit-tensor-view` | dtype, shape, stride, alignment, bounds |
| `unit-backend-abi` | status mapping, version/size rejection, failed-call outputs |
| `abi-c-header` | public backend header compiled and linked as C11 |
| `backend-lifetime` | CPU handle ownership, retirement order, reset, events |
| `backend-mock` | deterministic delayed completion, queue order, fencing |

## Target presets

`target-npu` and `target-npu-x86_64` cross-configure for Linux using
[`cmake/toolchains/arm64.cmake`](../cmake/toolchains/arm64.cmake) and
[`cmake/toolchains/x86_64.cmake`](../cmake/toolchains/x86_64.cmake). They build
`liblumen` only; host tests are disabled. Override the default GNU triple with
`LUMEN_TARGET_C_COMPILER` / `LUMEN_TARGET_CXX_COMPILER`.

```bash
cmake --preset target-npu
cmake --build --preset target-npu
```

Cross builds verify portability of the CPU/mock foundation. They are not NPU
qualification evidence; device, driver, SDK, queue/event, reset, and kernel
evidence belongs to the NPU qualification work item (LUMEN-002).

## Backend ABI boundary

[`include/lumen/backend.h`](../include/lumen/backend.h) is the versioned C ABI:
fixed-width records, size/ABI fields, opaque handles, status codes, explicit
retirement, and no STL or C++ exception surface. The header's comments are the
normative statement of failed-call side effects, queue ordering, event release,
and reset semantics. `liblumen` provides the CPU and mock adapters; neither
links a vendor SDK.

The internal C++ primitives live in `src/core/status.h`,
`src/core/checked_math.h`, and `src/model/tensor_view.h`. They are not part of
the public C ABI.

## CI

[`.github/workflows/host-ci.yml`](../.github/workflows/host-ci.yml) runs the host
suite with warnings as errors on x86-64 Linux, ARM64 Linux, and ARM64 macOS, runs
the sanitizer suite on x86-64 Linux, and cross-builds the ARM64 target preset on
x86-64 Linux with the GNU cross toolchain.
