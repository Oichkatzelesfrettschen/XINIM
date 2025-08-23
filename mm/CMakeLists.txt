file(GLOB MM_SRC "*.cpp") # Use quotes for robustness

add_executable(minix_mm_server ${MM_SRC})

# MM server should link against the C library (minix_libc)
target_link_libraries(minix_mm_server PRIVATE minix_libc xinim_common)

# Add include directories specific to the MM server
target_include_directories(minix_mm_server PRIVATE ".")
