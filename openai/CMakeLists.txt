cmake_minimum_required(VERSION 3.14)
project(openai_cpp_demos CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(sensor_event_loop sensor_event_loop.cpp)
target_compile_options(sensor_event_loop PRIVATE -Wall -Wextra -Wpedantic)

enable_testing()
add_test(NAME sensor_event_loop COMMAND sensor_event_loop)
