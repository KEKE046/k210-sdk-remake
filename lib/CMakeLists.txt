add_compile_options(-Wno-pointer-arith -Wno-multichar)

FILE(GLOB_RECURSE LIB_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/*.s"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/*.s"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.s"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/*.S"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/*.S"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.S"
)

FILE(GLOB_RECURSE ASSEMBLY_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/*.s"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/*.s"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.s"
    "${CMAKE_CURRENT_SOURCE_DIR}/bsp/*.S"
    "${CMAKE_CURRENT_SOURCE_DIR}/drivers/*.S"
    "${CMAKE_CURRENT_SOURCE_DIR}/utils/*.S"
)
SET_PROPERTY(SOURCE ${ASSEMBLY_FILES} PROPERTY LANGUAGE C)
SET_SOURCE_FILES_PROPERTIES(${ASSEMBLY_FILES} PROPERTIES COMPILE_FLAGS "-x assembler-with-cpp -D __riscv64")

add_library(kendryte ${LIB_SRC})
target_link_libraries(kendryte PUBLIC xtl)
target_include_directories(kendryte PUBLIC ./bsp/include ./drivers/include ./nncase/include ./utils/include)
set_target_properties(kendryte PROPERTIES LINKER_LANGUAGE C)

add_subdirectory(nncase)
