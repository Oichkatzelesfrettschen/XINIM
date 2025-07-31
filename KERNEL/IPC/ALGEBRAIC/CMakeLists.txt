# KERNEL/IPC/ALGEBRAIC/CMakeLists.txt

# This library encompasses various algebraic types useful for IPC and security.
# It includes quaternions, octonions, and sedenions.

# Source files for the library
set(MATH_ALGEBRAS_SOURCES
    quaternion/quaternion.cpp
    octonion/octonion.cpp
    sedenion/sedenion.cpp
)

# The quaternion_spinlock.hpp contains hyper::Quaternion (float-based) and the spinlock.
# This is header-only from the perspective of its types being used.
# The XINIM::Algebraic::Quaternion (double-based) is in quaternion/quaternion.hpp + .cpp.
# The XINIM::Algebraic::Octonion (double-based) is in octonion/octonion.hpp + .cpp.
# The XINIM::Algebraic::Sedenion (double-based) is in sedenion/sedenion.hpp + .cpp.

add_library(math_algebras STATIC
    ${MATH_ALGEBRAS_SOURCES}
)

# Public include directories expose headers for the algebraic libraries.
# Consumers will use #include "quaternion/quaternion.hpp", "octonion/octonion.hpp", "sedenion/sedenion.hpp" etc.
target_include_directories(math_algebras PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR} # Allows includes like "quaternion/quaternion.hpp" from KERNEL/IPC/ALGEBRAIC
)

# Request C++17 (or higher, e.g., C++23 as per project goals, but C++17 is a safe baseline for wide toolchain support)
# The sedenion code uses std::array which is C++11.
# The quaternion spinlock uses std::hardware_destructive_interference_size which is C++17.
target_compile_features(math_algebras PUBLIC cxx_std_17)

# Potentially add compile options for SIMD if not globally set,
# or if specific settings are needed for these math types.
# Example: target_compile_options(math_algebras PRIVATE -mavx2 -mavx512f)

set_target_properties(math_algebras PROPERTIES
    OUTPUT_NAME "xinim_math_algebras" # Sets the output file name, e.g., libxinim_math_algebras.a
)

# Note on other existing algebraic types:
# - kernel/octonion.hpp (lattice::Octonion for capabilities) is separate and not part of this general math library.
# - kernel/sedenion.hpp (float-based experimental crypto) was also separate and its concepts are being
#   integrated (as placeholders) into the XINIM::Algebraic::Sedenion here.

# This library, math_algebras, should be linked by the main kernel target.
# Example in parent CMakeLists.txt: target_link_libraries(XINIM_KERNEL PRIVATE math_algebras)
