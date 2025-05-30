file(GLOB TOOL_SRC *.cpp)
foreach(src ${TOOL_SRC})
  get_filename_component(prog ${src} NAME_WE)
  set(target tool_${prog})
  add_executable(${target} ${src})
  target_link_libraries(${target} PRIVATE minixlib)
endforeach()

