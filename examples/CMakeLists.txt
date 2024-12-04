file(GLOB subdir RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/*/)
list(FILTER subdir INCLUDE REGEX "[0-9][0-9]-.*")

foreach(proj ${subdir})
    file(GLOB files ${proj}/*.c ${proj}/*.cpp)
    add_executable(${proj} ${files})
    target_link_libraries(${proj} PUBLIC kendryte m)
    target_include_directories(${proj} PRIVATE ${proj})
    target_dump_binary(${proj})
endforeach()

target_link_libraries(22-mnist-inference PUBLIC nncase)
