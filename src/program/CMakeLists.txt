cmake_minimum_required(VERSION 3.16)
project(dfs_riscv_program CXX C ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Algorithm / harness / dataset sources shared by the RISC-V program and the
# native integration co-simulation.
set(LIB_SOURCES
    harness/harness.cpp
    harness/accelerator.cpp
    harness/report.cpp
    cases/datasets.cpp
    cases/generators.cpp
    cases/oracle.cpp
    cases/synthetic.cpp
    algorithms/dfs_algorithm.cpp
    algorithms/number_of_islands/number_of_islands.cpp
    algorithms/unique_paths_iii/unique_paths_iii.cpp
    algorithms/word_search_ii/word_search_ii.cpp
    algorithms/longest_increasing_path/longest_increasing_path.cpp
    algorithms/pacific_atlantic/pacific_atlantic.cpp
)

set(PROGRAM_SOURCES main.cpp ${LIB_SOURCES})

if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    list(APPEND PROGRAM_SOURCES bare/crt0.S bare/syscalls.c)
endif()

add_executable(program ${PROGRAM_SOURCES})

# crt0.S calls main without setting a0/a1, so argc/argv are garbage on bare metal.
if(CMAKE_SYSTEM_NAME STREQUAL "Generic")
    target_compile_definitions(program PRIVATE DFS_BARE_METAL)
endif()

# Headers are included by directory prefix, e.g. "harness/harness.h".
target_include_directories(program PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ── Native-only targets (unit test + integration co-simulation) ────────────────
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Generic")
    enable_testing()

    add_executable(accelerator_driver_test driver/accelerator_driver_test.cpp)
    target_include_directories(accelerator_driver_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../model)
    add_test(NAME accelerator_driver_test COMMAND accelerator_driver_test)

    # Integration co-simulation requires SystemC/TLM (host build only). When it is
    # unavailable the target is skipped so the plain native build still works.
    set(_systemc_home "$ENV{SYSTEMC_HOME}")
    if(_systemc_home)
        find_library(SYSTEMC_LIB NAMES systemc
            PATHS ${_systemc_home}/lib ${_systemc_home}/lib-linux64 ${_systemc_home}/lib-macosx64)
        if(SYSTEMC_LIB)
            add_executable(integration_sim
                integration/integration_main.cpp
                ${LIB_SOURCES})
            target_include_directories(integration_sim PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}
                ${CMAKE_CURRENT_SOURCE_DIR}/../model
                ${_systemc_home}/include)
            find_package(Threads)
            target_link_libraries(integration_sim ${SYSTEMC_LIB} Threads::Threads)
            add_test(NAME integration_sim COMMAND integration_sim)
        else()
            message(STATUS "SystemC not found under SYSTEMC_HOME — skipping integration_sim")
        endif()
    else()
        message(STATUS "SYSTEMC_HOME not set — skipping integration_sim")
    endif()
endif()
