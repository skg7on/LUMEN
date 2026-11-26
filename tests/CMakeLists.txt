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
