cmake_minimum_required(VERSION 3.14)
project(llm_handwrite CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(LLM_HANDWRITE
    flash_attention
    kv_cache
    paged_attention
)

foreach(name IN LISTS LLM_HANDWRITE)
    add_executable(${name} ${name}.cpp)
    target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME ${name} COMMAND ${name})
endforeach()

enable_testing()

add_custom_target(llm_handwrite
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS ${LLM_HANDWRITE}
    COMMENT "Run all LLM handwrite tests"
)
