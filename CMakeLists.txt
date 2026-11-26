# Copyright 2026 Gavin Sun
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.25)

project(LUMEN VERSION 0.0.1 LANGUAGES C CXX)

# The P0 native foundation builds and tests on a host with no vendor NPU SDK.
option(LUMEN_BUILD_TESTS "Build the native host test suite" ON)
option(LUMEN_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(LUMEN_BUILD_TESTS)
  enable_testing()
endif()

# Shared warning policy for host code. Tests must never compile assertions away,
# so no configuration here defines NDEBUG for test targets.
function(lumen_apply_warnings target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4)
    if(LUMEN_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE /WX)
    endif()
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    if(LUMEN_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

include(cmake/VersionMetadata.cmake)

# Public C ABI plus the CPU/mock adapters and shared core primitives.
add_library(lumen STATIC
  src/backend/abi.cpp
  src/backend/common/runtime.cpp
  src/backend/cpu/cpu_backend.cpp
  src/backend/mock/mock_backend.cpp
  src/backend/mock/mock_control.cpp
)
add_library(lumen::lumen ALIAS lumen)
target_include_directories(lumen
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_features(lumen PUBLIC cxx_std_20)
lumen_apply_warnings(lumen)

if(LUMEN_BUILD_TESTS)
  add_subdirectory(tests)
endif()
