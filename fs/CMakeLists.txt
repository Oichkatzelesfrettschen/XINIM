file(GLOB FS_SRC *.c)
add_executable(fs ${FS_SRC})
target_link_libraries(fs PRIVATE minixlib)

