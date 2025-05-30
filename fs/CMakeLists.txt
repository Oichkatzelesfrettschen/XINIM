file(GLOB FS_SRC *.cpp)
add_executable(fs ${FS_SRC})
target_link_libraries(fs PRIVATE minixlib)

