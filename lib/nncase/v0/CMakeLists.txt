include_directories(include .)
file(GLOB_RECURSE NNCASE_SRC
        "${CMAKE_CURRENT_LIST_DIR}/*.c"
        "${CMAKE_CURRENT_LIST_DIR}/*.cpp"
        )

add_library(nncase-v0 ${NNCASE_SRC})
target_link_libraries(nncase-v0 PUBLIC kendryte)
target_compile_options(nncase-v0 PRIVATE -O2)
set_target_properties(nncase-v0 PROPERTIES LINKER_LANGUAGE C)