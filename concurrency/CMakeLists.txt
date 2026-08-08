cmake_minimum_required(VERSION 3.14)
project(concurrency_examples CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(EXAMPLES
    bounded_blocking_queue
    thread_safe_queue
    thread_safe_ring_buffer
    spsc_ring_buffer
    producer_consumer_frame_dropping
    video_ring_buffer
    local_sd_card_writer
)

foreach(name IN LISTS EXAMPLES)
    add_executable(${name} ${name}.cpp)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    find_package(Threads REQUIRED)
    target_link_libraries(${name} PRIVATE Threads::Threads)
    add_test(NAME ${name} COMMAND ${name})
endforeach()

enable_testing()
