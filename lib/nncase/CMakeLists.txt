add_compile_options(-Wno-multichar)

add_subdirectory(v0)
add_subdirectory(v1-rev)

add_library(nncase STATIC nncase.cpp)
target_link_libraries(nncase PRIVATE nncase-v0 nncase-v1)
target_include_directories(nncase PUBLIC include)
set_target_properties(nncase PROPERTIES LINKER_LANGUAGE C)