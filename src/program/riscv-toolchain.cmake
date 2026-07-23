set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

if(NOT DEFINED RISCV_TOOLCHAIN_PREFIX)
    if(DEFINED ENV{RISCV_TOOLCHAIN_PREFIX})
        set(RISCV_TOOLCHAIN_PREFIX $ENV{RISCV_TOOLCHAIN_PREFIX})
    else()
        set(RISCV_TOOLCHAIN_PREFIX riscv64-unknown-elf-)
    endif()
endif()

set(CMAKE_C_COMPILER   ${RISCV_TOOLCHAIN_PREFIX}gcc)
set(CMAKE_CXX_COMPILER ${RISCV_TOOLCHAIN_PREFIX}g++)
set(CMAKE_ASM_COMPILER ${RISCV_TOOLCHAIN_PREFIX}gcc)

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(_arch "-march=rv64imac -mabi=lp64 -mcmodel=medany")
set(_common "${_arch} -ffunction-sections -fdata-sections")
set(_linker_script "${CMAKE_CURRENT_LIST_DIR}/bare/link.ld")

set(CMAKE_C_FLAGS_INIT   "${_common}")
set(CMAKE_CXX_FLAGS_INIT "${_common} -fno-exceptions -fno-rtti")
set(CMAKE_ASM_FLAGS_INIT "${_arch}")
set(CMAKE_EXE_LINKER_FLAGS_INIT "${_arch} -nostartfiles -T${_linker_script} -Wl,--gc-sections")
