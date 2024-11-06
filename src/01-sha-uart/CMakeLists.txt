file(GLOB files *.c *.h)
add_executable(s01-sha-uart ${files})
target_link_libraries(s01-sha-uart PUBLIC kendryte m)
target_dump_binary(s01-sha-uart)