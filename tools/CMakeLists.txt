# This CMake file builds each tool as a separate executable.

file(GLOB TOOL_SOURCES "*.cpp") # Use quotes and a more descriptive variable name

foreach(TOOL_SOURCE_FILE ${TOOL_SOURCES})
  get_filename_component(TOOL_NAME ${TOOL_SOURCE_FILE} NAME_WE) # NAME_WE extracts name without extension
  set(TARGET_NAME minix_tool_${TOOL_NAME})

  add_executable(${TARGET_NAME} ${TOOL_SOURCE_FILE})

  # Link each tool against the C library (minix_libc)
  # Some tools might have more specific linking needs later.
  target_link_libraries(${TARGET_NAME} PRIVATE minix_libc xinim_common)
endforeach()
