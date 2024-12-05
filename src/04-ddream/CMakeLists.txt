set(nncaseruntime_DIR ${PROJECT_SOURCE_DIR}/lib/nncase/v1-rev/lib/cmake/nncaseruntime)
find_package(nncaseruntime REQUIRED)
include_directories(${PROJECT_SOURCE_DIR}/lib/nncase/v1-rev/include)

file(GLOB files *.c *.h *.cpp)
add_executable(s04-ddream ${files})
target_link_libraries(s04-ddream PUBLIC kendryte m nncaseruntime nncase_rt_modules_k210)
target_include_directories(s04-ddream PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_dump_binary(s04-ddream)
