cmake_minimum_required(VERSION 3.16)
project(dfs_riscv_program CXX C ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROGRAM_SOURCES
    main.cpp
    harness/harness.cpp
    harness/accelerator.cpp
    harness/report.cpp
    cases/datasets.cpp
    algorithms/dfs_algorithm.cpp
    algorithms/number_of_islands/number_of_islands.cpp
    algorithms/unique_paths_iii/unique_paths_iii.cpp
    algorithms/word_search_ii/word_search_ii.cpp
    algorithms/longest_increasing_path/longest_increasing_path.cpp
    algorithms/pacific_atlantic/pacific_atlantic.cpp
)

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    list(APPEND PROGRAM_SOURCES bare/crt0.S bare/syscalls.c)
endif()

add_executable(program ${PROGRAM_SOURCES})

# Headers are included by directory prefix, e.g. "harness/harness.h".
target_include_directories(program PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
    add_executable(accelerator_driver_test driver/accelerator_driver_test.cpp)
    target_include_directories(accelerator_driver_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../model)
    enable_testing()
    add_test(NAME accelerator_driver_test COMMAND accelerator_driver_test)
endif()
