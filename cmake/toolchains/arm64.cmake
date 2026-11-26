# Cross-compilation toolchain for 64-bit ARM Linux targets.
#
# This file is selected by the `target-npu` CMake preset. It intentionally does
# not reference any vendor NPU SDK: the CPU and mock backends must configure and
# build for the target architecture before a vendor adapter is qualified.
#
# Override the compiler triple with LUMEN_TARGET_C_COMPILER /
# LUMEN_TARGET_CXX_COMPILER when the default GNU cross toolchain is not on PATH.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(LUMEN_TARGET_C_COMPILER "$ENV{LUMEN_TARGET_C_COMPILER}" CACHE STRING
    "C compiler for the ARM64 target; defaults to aarch64-linux-gnu-gcc")
set(LUMEN_TARGET_CXX_COMPILER "$ENV{LUMEN_TARGET_CXX_COMPILER}" CACHE STRING
    "C++ compiler for the ARM64 target; defaults to aarch64-linux-gnu-g++")

if(NOT LUMEN_TARGET_C_COMPILER)
  set(LUMEN_TARGET_C_COMPILER aarch64-linux-gnu-gcc)
endif()
if(NOT LUMEN_TARGET_CXX_COMPILER)
  set(LUMEN_TARGET_CXX_COMPILER aarch64-linux-gnu-g++)
endif()

set(CMAKE_C_COMPILER "${LUMEN_TARGET_C_COMPILER}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${LUMEN_TARGET_CXX_COMPILER}" CACHE FILEPATH "" FORCE)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Host-only verification does not run under emulation in P0.
set(LUMEN_BUILD_TESTS OFF CACHE BOOL "Host tests are not run for the cross target" FORCE)
