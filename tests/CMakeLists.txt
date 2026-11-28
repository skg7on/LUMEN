# Copyright 2026 Gavin Sun
# SPDX-License-Identifier: Apache-2.0
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

add_library(lumen_test_support INTERFACE)
target_include_directories(lumen_test_support INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/support
  ${CMAKE_CURRENT_SOURCE_DIR}/../src
  ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# Register one executable as a named CTest entry. Every host test links the
# internal support header and runs from the build tree with a failing exit code
# on any CHECK/REQUIRE failure.
function(lumen_add_host_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE lumen_test_support lumen::lumen)
  lumen_apply_warnings(${name})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS "host")
endfunction()

# SDK-free qualification support. The synthetic fixture is harness input only;
# it is never device evidence and never a qualification claim.
function(lumen_add_qualification_host_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE
    lumen_qualification_support
    lumen_test_support
    lumen::lumen)
  lumen_apply_warnings(${name})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS "host")
endfunction()

lumen_qualification_configure_profile(
  ${CMAKE_CURRENT_SOURCE_DIR}/qualification/data/synthetic-target-profile.json)
lumen_add_qualification_support()

add_library(lumen_qualification_test_support STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/support/qualification_fake.cpp)
target_link_libraries(lumen_qualification_test_support PUBLIC
  lumen_qualification_support)
lumen_apply_warnings(lumen_qualification_test_support)

function(lumen_add_qualification_host_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE
    lumen_qualification_test_support
    lumen_test_support)
  lumen_apply_warnings(${name})
  add_test(NAME ${name} COMMAND ${name})
  set_tests_properties(${name} PROPERTIES LABELS "host")
endfunction()

lumen_add_qualification_host_test(p0-qual-profile
  qualification/profile_test.cpp)
lumen_add_qualification_host_test(p0-qual-decision
  qualification/decision_test.cpp)
lumen_add_qualification_host_test(p0-qual-lifecycle
  qualification/lifecycle_test.cpp)

lumen_add_host_test(unit-build-smoke unit/build_smoke_test.cpp)
lumen_add_host_test(unit-status unit/status_test.cpp)
lumen_add_host_test(unit-checked-math unit/checked_math_test.cpp)
lumen_add_host_test(unit-tensor-view unit/tensor_view_test.cpp)
lumen_add_host_test(unit-backend-abi unit/backend_abi_test.cpp)
lumen_add_host_test(backend-lifetime unit/backend_lifetime_test.cpp)
lumen_add_host_test(backend-mock unit/backend_mock_test.cpp)

# Compile and link the public header from a C11 translation unit.
add_executable(abi-c-header abi/backend_header_c.c)
target_link_libraries(abi-c-header PRIVATE lumen_test_support lumen::lumen)
target_compile_features(abi-c-header PRIVATE c_std_11)
set_target_properties(abi-c-header PROPERTIES LINKER_LANGUAGE CXX)
lumen_apply_warnings(abi-c-header)
add_test(NAME abi-c-header COMMAND abi-c-header)
set_tests_properties(abi-c-header PROPERTIES LABELS "host")

# Compile and link the private qualification extension from a C11 translation
# unit. It validates exact table size/version, the base table, and pointers.
add_executable(abi-qualification-c abi/qualification_header_c.c)
target_link_libraries(abi-qualification-c PRIVATE lumen_qualification_support)
target_compile_features(abi-qualification-c PRIVATE c_std_11)
set_target_properties(abi-qualification-c PROPERTIES LINKER_LANGUAGE CXX)
lumen_apply_warnings(abi-qualification-c)
add_test(NAME abi-qualification-c COMMAND abi-qualification-c)
set_tests_properties(abi-qualification-c PROPERTIES LABELS "host")
