cmake_minimum_required(VERSION 3.14)
project(microsoft_handwrite CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(gemm gemm.cpp)
target_compile_options(gemm PRIVATE -Wall -Wextra -Wpedantic -O3)
add_test(NAME gemm COMMAND gemm)

enable_testing()

# Optional: cmake -S . -B build -DGEMM_ENABLE_AVX512=ON
option(GEMM_ENABLE_AVX512 "Enable AVX-512 FMA path in gemm.cpp" OFF)
if(GEMM_ENABLE_AVX512)
    target_compile_definitions(gemm PRIVATE GEMM_AVX512)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU|Intel")
        target_compile_options(gemm PRIVATE -mavx512f)
    endif()
endif()
