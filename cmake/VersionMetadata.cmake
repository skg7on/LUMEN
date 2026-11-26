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

# Host/toolchain metadata for the P0 native foundation.
#
# The dependency lock lives in cmake/dependencies.lock.json and records the
# pinned minimum toolchain versions plus the fact that P0 has no third-party
# runtime dependencies. This module only exposes the values to CMake and fails
# configure when the host toolchain is older than the lock allows.

set(LUMEN_CMAKE_MIN_VERSION "3.25.0" CACHE STRING "Pinned minimum CMake version")
set(LUMEN_NINJA_MIN_VERSION "1.11" CACHE STRING "Pinned minimum Ninja version")

set(LUMEN_DEPENDENCY_LOCK "${CMAKE_CURRENT_LIST_DIR}/dependencies.lock.json")
if(NOT EXISTS "${LUMEN_DEPENDENCY_LOCK}")
  message(FATAL_ERROR
    "Dependency lock is missing: ${LUMEN_DEPENDENCY_LOCK}. "
    "P0 requires a reviewed dependency manifest.")
endif()

if(CMAKE_VERSION VERSION_LESS LUMEN_CMAKE_MIN_VERSION)
  message(FATAL_ERROR
    "CMake ${CMAKE_VERSION} is older than the pinned minimum ${LUMEN_CMAKE_MIN_VERSION}.")
endif()
