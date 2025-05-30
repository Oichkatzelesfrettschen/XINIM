file(GLOB CMD_SRC *.cpp)
foreach(src ${CMD_SRC})
  get_filename_component(prog ${src} NAME_WE)
  set(target cmd_${prog})
  add_executable(${target} ${src})
  target_link_libraries(${target} PRIVATE minixlib)
endforeach()

