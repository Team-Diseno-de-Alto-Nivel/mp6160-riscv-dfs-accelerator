cmake_minimum_required(VERSION 3.16)
project(dfs_riscv_program CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(program
    main.cpp
    harness/harness.cpp
    algorithms/number_of_islands/number_of_islands.cpp
    algorithms/unique_paths_iii/unique_paths_iii.cpp
    algorithms/word_search_ii/word_search_ii.cpp
    algorithms/longest_increasing_path/longest_increasing_path.cpp
    algorithms/pacific_atlantic/pacific_atlantic.cpp
)

# Headers are included by directory prefix, e.g. "harness/harness.h".
target_include_directories(program PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
