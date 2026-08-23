cmake_minimum_required(VERSION 3.14)
project(concurrency_examples CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Sanitizers ------------------------------------------------------------
# ThreadSanitizer finds data races even when the run produced the right answer.
# Run every concurrency change under it.
#   cmake -S . -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DENABLE_TSAN=ON
#   cmake --build build-tsan && ctest --test-dir build-tsan --output-on-failure
#
# ASan+UBSan catch the memory bugs that concurrency bugs turn into.
# TSan and ASan cannot be combined.
option(ENABLE_TSAN "Build with ThreadSanitizer" OFF)
option(ENABLE_ASAN "Build with Address+UndefinedBehaviorSanitizer" OFF)

if(ENABLE_TSAN AND ENABLE_ASAN)
    message(FATAL_ERROR "ENABLE_TSAN and ENABLE_ASAN are mutually exclusive")
endif()

add_library(sanitizer_flags INTERFACE)
if(ENABLE_TSAN)
    target_compile_options(sanitizer_flags INTERFACE -fsanitize=thread -g -O1 -fno-omit-frame-pointer)
    target_link_options(sanitizer_flags INTERFACE -fsanitize=thread)
    message(STATUS "ThreadSanitizer: ON")
elseif(ENABLE_ASAN)
    target_compile_options(sanitizer_flags INTERFACE -fsanitize=address,undefined -g -O1 -fno-omit-frame-pointer)
    target_link_options(sanitizer_flags INTERFACE -fsanitize=address,undefined)
    message(STATUS "Address+UBSanitizer: ON")
endif()

# --- Keep assert() alive ---------------------------------------------------
# Every test in this repo asserts its invariants. Release-style builds define
# NDEBUG, which compiles assert() to nothing and would turn the whole suite
# into a silent no-op. Strip NDEBUG so the checks always run.
foreach(cfg RELEASE RELWITHDEBINFO MINSIZEREL)
    string(REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_${cfg} "${CMAKE_CXX_FLAGS_${cfg}}")
endforeach()

find_package(Threads REQUIRED)

# --- Examples --------------------------------------------------------------
# Group 1: queues and buffers (the producer/consumer core)
set(EXAMPLES_QUEUES
    bounded_blocking_queue
    thread_safe_queue
    thread_safe_ring_buffer
    spsc_ring_buffer
)

# Group 2: camera pipeline scenarios
set(EXAMPLES_PIPELINE
    producer_consumer_frame_dropping
    video_ring_buffer
    local_sd_card_writer
)

# Group 3: fundamentals -- primitives, races, lock discipline
set(EXAMPLES_CORE
    thread_pool
    atomics_and_memory_order
    deadlock_and_lock_discipline
    data_race_demos
)

# Group 4: real-time + historical data structures
set(EXAMPLES_DATA
    timeseries_store
    readers_writer_patterns
    thread_safe_lru_cache
    rate_limiter
)

set(EXAMPLES
    ${EXAMPLES_QUEUES}
    ${EXAMPLES_PIPELINE}
    ${EXAMPLES_CORE}
    ${EXAMPLES_DATA}
)

foreach(name IN LISTS EXAMPLES)
    add_executable(${name} ${name}.cpp)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    target_link_libraries(${name} PRIVATE Threads::Threads sanitizer_flags)
    add_test(NAME ${name} COMMAND ${name})
endforeach()

# Timing-sensitive tests get more headroom under sanitizers.
set_tests_properties(rate_limiter timeseries_store PROPERTIES TIMEOUT 120)

enable_testing()
