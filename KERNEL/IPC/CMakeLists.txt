# KERNEL/IPC/CMakeLists.txt

# This CMakeLists file builds the core IPC functionalities,
# excluding the algebraic math libraries which are in a subdirectory.

# List of source files for the core IPC library
set(IPC_CORE_SOURCES
    lattice_ipc.cpp
    # Add future files like channel_management.cpp, ipc_syscall_handlers.cpp, etc.
)

# Define the static library for core IPC logic
add_library(xinim_ipc_core STATIC
    ${IPC_CORE_SOURCES}
)

# Public include directories for this library.
# This makes KERNEL/IPC/include/public (for lattice_api.hpp) available to linkers.
# KERNEL/IPC/include/internal is not exported as it's for internal use within this module.
target_include_directories(xinim_ipc_core PUBLIC
    # To allow #include "public/lattice_api.hpp" or similar,
    # we need to make KERNEL/IPC/include an include path.
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Core IPC logic depends on the math_algebras library (quaternion, octonion, sedenion)
# for types like ChannelCapabilityToken which uses Algebraic::Octonion.
# The math_algebras target is defined in KERNEL/IPC/ALGEBRAIC/CMakeLists.txt.
# We need to ensure ALGEBRAIC is processed first.
add_subdirectory(ALGEBRAIC) # Ensures math_algebras target is known
target_link_libraries(xinim_ipc_core PUBLIC # PUBLIC so users of ipc_core also link math_algebras if needed transitively
    math_algebras
)

# Set C++ standard. Should be consistent with the rest of the kernel.
target_compile_features(xinim_ipc_core PUBLIC cxx_std_17) # Or higher (e.g., C++23)

set_target_properties(xinim_ipc_core PROPERTIES
    OUTPUT_NAME "xinim_ipc_core" # e.g. libxinim_ipc_core.a
)

# This library will be linked by the main kernel target (XINIM_KERNEL_LIB or XINIM_KERNEL).
# Example in KERNEL/CMakeLists.txt:
# add_subdirectory(IPC)
# target_link_libraries(XINIM_KERNEL_LIB PRIVATE xinim_ipc_core)
