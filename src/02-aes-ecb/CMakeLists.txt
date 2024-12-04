file(GLOB files *.c *.h)
add_executable(s02-ase-ecb ${files})
target_link_libraries(s02-ase-ecb PUBLIC kendryte m)
target_dump_binary(s02-ase-ecb)