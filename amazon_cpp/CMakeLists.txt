cmake_minimum_required(VERSION 3.14)
project(amazon_cpp_examples CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXAMPLES
    01_rule_of_five
    02_memory_placement
    03_const_pointers
    04_overload_resolution
    05_move_forwarding
    06_smart_pointers
    07_virtual_vtable
    08_memory_layout
    09_stl_internals
    10_concurrency_atomic
)

foreach(name IN LISTS EXAMPLES)
    add_executable(${name} examples/${name}.cpp)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME ${name} COMMAND ${name})
endforeach()

enable_testing()

add_custom_target(run_all_examples
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${EXAMPLES}
    COMMENT "Run all Amazon C++ examples"
)
