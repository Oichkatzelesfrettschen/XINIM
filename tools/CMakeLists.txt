# This CMake file builds each tool as a separate executable.

file(GLOB TOOL_SOURCES "*.cpp") # Use quotes and a more descriptive variable name

foreach(TOOL_SOURCE_FILE ${TOOL_SOURCES})
  get_filename_component(TOOL_NAME ${TOOL_SOURCE_FILE} NAME_WE) # NAME_WE extracts name without extension
  set(TARGET_NAME minix_tool_${TOOL_NAME})

  add_executable(${TARGET_NAME} ${TOOL_SOURCE_FILE})

  # Link each tool against the C library (minix_libc) and the common project settings.
  # The `xinim_common` library provides project-wide compiler flags and include paths.
  target_link_libraries(${TARGET_NAME} PRIVATE minix_libc xinim_common)

  # Add include directories for each tool
  target_include_directories(${TARGET_NAME} PUBLIC
    "." # For any local headers specific to a tool (e.g., diskio.hpp)
    # The global include paths from include/ and h/ are provided by xinim_common.
  )
endforeach()