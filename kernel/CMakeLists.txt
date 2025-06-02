# Choose the correct wini driver source
if(WINI_DRIVER STREQUAL "at")
  set(WINI_SRC at_wini.cpp)
else()
  set(WINI_SRC xt_wini.cpp) # Default to pc/xt if not at
endif()

set(KERNEL_SRC
    clock.cpp dmp.cpp floppy.cpp main.cpp memory.cpp printer.cpp proc.cpp system.cpp
    table.cpp tty.cpp idt64.cpp mpx64.cpp klib64.cpp klib88.cpp mpx88.cpp paging.cpp syscall.cpp ${WINI_SRC})
# Note: klib88.cpp and mpx88.cpp were added as they appear to be kernel related sources.
# wini.cpp is a generic name, might be a common file or superseded by at_wini/xt_wini.
# For now, including specific at_wini/xt_wini based on WINI_DRIVER.

set(KERNEL_ASM) # No assembly files found directly in kernel/ by ls, assuming C++ for now

# Define the kernel as a static library
add_library(minix_kernel STATIC
    ${KERNEL_SRC}
    ${KERNEL_ASM}
)

# The kernel library should be self-contained and not link against minix_libc.
# Linker scripts will handle final binary creation.
# Removed: target_link_libraries(kernel PRIVATE minixlib)

# Add include directories that might be needed by kernel source files
# (though global ones in parent CMakeLists.txt might cover these)
target_include_directories(minix_kernel PUBLIC
    "."     # For headers in kernel/ itself like proc.h, const.h, glo.h, type.h
    "${CMAKE_SOURCE_DIR}/include" # For global includes like "errno.h"
    "${CMAKE_SOURCE_DIR}/h"       # For global includes from h/
)
