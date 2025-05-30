file(GLOB MM_SRC *.cpp)
add_executable(mm ${MM_SRC})
target_link_libraries(mm PRIVATE minixlib)

