# SIMD Mathematical Library for XINIM
cmake_minimum_required(VERSION 3.20)

# Set the library name
set(SIMD_MATH_LIB xinim_simd_math)

# Collect all source files
file(GLOB_RECURSE SIMD_MATH_SOURCES
    "math/*.cpp"
    "arch/*.cpp"
    "ops/*.cpp"
    "detect.cpp"
    "core.cpp"
)

# Collect all header files
file(GLOB_RECURSE SIMD_MATH_HEADERS
    "../../include/xinim/simd/*.hpp"
)

# Create the library
add_library(${SIMD_MATH_LIB} STATIC ${SIMD_MATH_SOURCES} ${SIMD_MATH_HEADERS})

# Set include directories
target_include_directories(${SIMD_MATH_LIB} PUBLIC
    "../../include"
    "../../include/xinim"
    "../../include/xinim/simd"
)

# Set compilation features
target_compile_features(${SIMD_MATH_LIB} PUBLIC cxx_std_23)

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${SIMD_MATH_LIB} PRIVATE
        -O3
        -march=native
        -mtune=native
        -ffast-math
        -funroll-loops
        -fomit-frame-pointer
        -DNDEBUG
    )
    
    # x86-64 specific optimizations
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
        target_compile_options(${SIMD_MATH_LIB} PRIVATE
            -msse2
            -msse3
            -mssse3
            -msse4.1
            -msse4.2
            -mavx
            -mavx2
            -mfma
        )
        
        # Check for AVX-512 support
        include(CheckCXXCompilerFlag)
        check_cxx_compiler_flag("-mavx512f" COMPILER_SUPPORTS_AVX512F)
        if(COMPILER_SUPPORTS_AVX512F)
            target_compile_options(${SIMD_MATH_LIB} PRIVATE
                -mavx512f
                -mavx512vl
                -mavx512bw
                -mavx512dq
                -mavx512cd
                -mavx512vnni
            )
            target_compile_definitions(${SIMD_MATH_LIB} PRIVATE XINIM_SIMD_AVX512_SUPPORT=1)
        endif()
    endif()
    
    # ARM64 specific optimizations
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
        target_compile_options(${SIMD_MATH_LIB} PRIVATE
            -mcpu=native
            -mfpu=neon
        )
        
        # Check for SVE support
        check_cxx_compiler_flag("-march=armv8-a+sve" COMPILER_SUPPORTS_SVE)
        if(COMPILER_SUPPORTS_SVE)
            target_compile_options(${SIMD_MATH_LIB} PRIVATE -march=armv8-a+sve)
            target_compile_definitions(${SIMD_MATH_LIB} PRIVATE XINIM_SIMD_SVE_SUPPORT=1)
        endif()
    endif()
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${SIMD_MATH_LIB} PRIVATE
        /O2
        /Oi
        /GL
        /arch:AVX2
        /fp:fast
        /DNDEBUG
    )
endif()

# Platform-specific definitions
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_definitions(${SIMD_MATH_LIB} PRIVATE XINIM_SIMD_LINUX=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    target_compile_definitions(${SIMD_MATH_LIB} PRIVATE XINIM_SIMD_WINDOWS=1)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_definitions(${SIMD_MATH_LIB} PRIVATE XINIM_SIMD_MACOS=1)
endif()

# Architecture-specific definitions
if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    target_compile_definitions(${SIMD_MATH_LIB} PRIVATE XINIM_SIMD_X86_64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
    target_compile_definitions(${SIMD_MATH_LIB} PRIVATE XINIM_SIMD_ARM64=1)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv64")
    target_compile_definitions(${SIMD_MATH_LIB} PRIVATE XINIM_SIMD_RISCV64=1)
endif()

# Link libraries
target_link_libraries(${SIMD_MATH_LIB} PUBLIC
    m  # Math library
)

# Threading support
find_package(Threads REQUIRED)
target_link_libraries(${SIMD_MATH_LIB} PUBLIC Threads::Threads)

# Install rules
install(TARGETS ${SIMD_MATH_LIB}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY "../../include/xinim/simd"
    DESTINATION include/xinim
    FILES_MATCHING PATTERN "*.hpp"
)

# Create test executable if testing is enabled
option(XINIM_SIMD_BUILD_TESTS "Build SIMD math library tests" OFF)
if(XINIM_SIMD_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Benchmarking executable
option(XINIM_SIMD_BUILD_BENCHMARKS "Build SIMD math library benchmarks" OFF)
if(XINIM_SIMD_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# Documentation
option(XINIM_SIMD_BUILD_DOCS "Build SIMD math library documentation" OFF)
if(XINIM_SIMD_BUILD_DOCS)
    find_package(Doxygen REQUIRED)
    configure_file(Doxyfile.in Doxyfile @ONLY)
    add_custom_target(simd_docs
        COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating SIMD math library documentation"
    )
endif()

# Print configuration summary
message(STATUS "XINIM SIMD Math Library Configuration:")
message(STATUS "  Target architecture: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
if(COMPILER_SUPPORTS_AVX512F)
    message(STATUS "  AVX-512 support: YES")
else()
    message(STATUS "  AVX-512 support: NO")
endif()
if(COMPILER_SUPPORTS_SVE)
    message(STATUS "  ARM SVE support: YES")
else()
    message(STATUS "  ARM SVE support: NO")
endif()
message(STATUS "  Tests: ${XINIM_SIMD_BUILD_TESTS}")
message(STATUS "  Benchmarks: ${XINIM_SIMD_BUILD_BENCHMARKS}")
message(STATUS "  Documentation: ${XINIM_SIMD_BUILD_DOCS}")
