# This CMake file builds each userland command as a separate executable.

file(GLOB CMD_SOURCES "*.cpp") # Use quotes and a more descriptive variable name
# svcctl.cpp provides lattice-based service control

foreach(CMD_SOURCE_FILE ${CMD_SOURCES})
  get_filename_component(COMMAND_NAME ${CMD_SOURCE_FILE} NAME_WE) # NAME_WE extracts name without extension
  set(TARGET_NAME minix_cmd_${COMMAND_NAME})

  add_executable(${TARGET_NAME} ${CMD_SOURCE_FILE})

  # Link each command against the C library (minix_libc)
  target_link_libraries(${TARGET_NAME} PRIVATE minix_libc)

  # Add include directories for each command
  # This ensures commands can find their own headers (if any, though less common for commands)
  # and also the global headers from include/ and h/
  target_include_directories(${TARGET_NAME} PUBLIC # PUBLIC so if a command was a library for another, paths would propagate
    "."  # For any local headers specific to a command (rare)
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/h"
  )
endforeach()
