cmake_minimum_required(VERSION 3.5)

project(k210-project VERSION 0.0.1)

include("cmake/k210-toolchain.cmake")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (CMAKE_BUILD_TYPE STREQUAL Debug)
    add_compile_definitions(-DDEBUG=1)
endif ()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_compile_options(
    -mcmodel=medany
    -mabi=lp64f
    -march=rv64imafc
    -fno-common
    -ffunction-sections
    -fdata-sections
    -fstrict-volatile-bitfields
    -fno-zero-initialized-in-bss
    -ffast-math
    -fno-math-errno
    -fsingle-precision-constant
    -Os
    -ggdb
)
# list(APPEND CMAKE_CXX_FLAGS -fno-use-cxa-atexit)
list(APPEND CMAKE_CXX_FLAGS "-Wno-pointer-arith")
list(APPEND CMAKE_C_FLAGS "-Wno-pointer-to-int-cast -Wno-incompatible-pointer-types -Wno-discarded-qualifiers")
add_link_options(
    -nostartfiles
    -static
    -Wl,--gc-sections
    -Wl,-static
    -Wl,--start-group
    -Wl,--whole-archive
    -Wl,--no-whole-archive
    -Wl,--end-group
    -Wl,-EL
    -Wl,--no-relax
    -T ${CMAKE_SOURCE_DIR}/lds/kendryte.ld
)
add_definitions(
    -DCONFIG_LOG_LEVEL=LOG_VERBOSE
    -DCONFIG_LOG_ENABLE
    -DCONFIG_LOG_COLORS
    -DLOG_KERNEL
    -D__riscv64
    -DLV_CONF_INCLUDE_SIMPLE

    -DTCB_SPAN_NO_EXCEPTIONS
    -DTCB_SPAN_NO_CONTRACT_CHECKING

    -DNNCASE_TARGET=k210
)

add_subdirectory(third_party/gsl-lite)
add_subdirectory(third_party/mpark-variant)
add_subdirectory(third_party/nlohmann_json)
add_subdirectory(third_party/xtl)
add_subdirectory(lib)

include_directories(include)
add_subdirectory(examples)