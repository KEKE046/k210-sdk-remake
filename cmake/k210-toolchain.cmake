set(CMAKE_SYSTEM_NAME           "Generic")
set(CMAKE_SYSTEM_PROCESSOR      riscv64)

set(KENDRYTE_TOOLCHAIN_ROOT     "/opt/kendryte-toolchain"   CACHE PATH "Kendryte compiler toolchain path")
set(CMAKE_CROSSCOMPILING        ON                          CACHE BOOL "")

set(RISCV_TOOLCHAIN_PREFIX      "riscv64-unknown-elf")

set(CMAKE_C_COMPILER            "${KENDRYTE_TOOLCHAIN_ROOT}/bin/${RISCV_TOOLCHAIN_PREFIX}-gcc")
set(CMAKE_CXX_COMPILER          "${KENDRYTE_TOOLCHAIN_ROOT}/bin/${RISCV_TOOLCHAIN_PREFIX}-g++")
set(CMAKE_LINKER                "${KENDRYTE_TOOLCHAIN_ROOT}/bin/${RISCV_TOOLCHAIN_PREFIX}-ld")
set(CMAKE_AR                    "${KENDRYTE_TOOLCHAIN_ROOT}/bin/${RISCV_TOOLCHAIN_PREFIX}-ar")
set(CMAKE_OBJCOPY               "${KENDRYTE_TOOLCHAIN_ROOT}/bin/${RISCV_TOOLCHAIN_PREFIX}-objcopy")
set(CMAKE_SIZE                  "${KENDRYTE_TOOLCHAIN_ROOT}/bin/${RISCV_TOOLCHAIN_PREFIX}-size")
set(CMAKE_OBJDUMP               "${KENDRYTE_TOOLCHAIN_ROOT}/bin/${RISCV_TOOLCHAIN_PREFIX}-objdump")
set(CMAKE_RANLIB                "${KENDRYTE_TOOLCHAIN_ROOT}/bin/${RISCV_TOOLCHAIN_PREFIX}-ranlib")
set(CMAKE_MAKE_PROGRAM          "${KENDRYTE_TOOLCHAIN_ROOT}/bin/make")
set(CMAKE_SYSTEM_LIBRARY_PATH   "${KENDRYTE_TOOLCHAIN_ROOT}/${RISCV_TOOLCHAIN_PREFIX}/lib/")

set(CMAKE_CXX_STANDARD          17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD            11)
set(CMAKE_C_STANDARD_REQUIRED   ON)

macro(target_dump_binary target)
    add_custom_command(
        TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} --output-format=binary $<TARGET_FILE:${target}> ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/${target}.bin
        DEPENDS ${target}
        COMMENT "Generating .bin file ..."
    )
endmacro()
