cmake_minimum_required(VERSION 3.14)
project(interview_handwrite_cpp CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ENGINEERING_DS
    bounded_blocking_queue
    spsc_ring_buffer
    thread_safe_ring_buffer
    lru_cache_ds
    object_pool
)

foreach(name IN LISTS ENGINEERING_DS)
    add_executable(${name} ${name}.cpp)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME ${name} COMMAND ${name})
endforeach()

enable_testing()

add_custom_target(engineering_ds
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${ENGINEERING_DS}
    COMMENT "Run all engineering data structure tests"
)
