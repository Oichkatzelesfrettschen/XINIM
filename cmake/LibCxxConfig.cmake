# XINIM libc++ Configuration
# Ensures proper C++23 libc++ usage with C17 libc fallback

# Detect and configure libc++
function(xinim_configure_libcxx TARGET)
    # Check if we're using libc++
    include(CheckCXXSourceCompiles)
    
    set(CMAKE_REQUIRED_FLAGS "-stdlib=libc++")
    check_cxx_source_compiles("
        #include <version>
        #ifndef _LIBCPP_VERSION
            #error Not using libc++
        #endif
        int main() { return 0; }
    " XINIM_USING_LIBCXX)
    
    if(XINIM_USING_LIBCXX)
        message(STATUS "Configuring ${TARGET} with libc++ (C++23)")
        
        # Set libc++ flags
        target_compile_options(${TARGET} PRIVATE
            -stdlib=libc++
            -std=c++23
            -fexperimental-library  # Enable experimental C++23 features
        )
        
        target_link_options(${TARGET} PRIVATE
            -stdlib=libc++
            -lc++experimental  # Link experimental library
        )
        
        # Enable C++23 features
        target_compile_definitions(${TARGET} PRIVATE
            _LIBCPP_ENABLE_EXPERIMENTAL=1
            _LIBCPP_STD_VER=23
            XINIM_USE_LIBCXX=1
        )
    else()
        message(STATUS "Configuring ${TARGET} with default stdlib")
        
        # Try to use C++23 anyway
        target_compile_features(${TARGET} PUBLIC cxx_std_23)
    endif()
    
    # Configure C17 fallback
    target_compile_definitions(${TARGET} PRIVATE
        _GNU_SOURCE  # Enable GNU extensions
        _POSIX_C_SOURCE=200809L  # POSIX.1-2008
        XINIM_ENABLE_C17_FALLBACK=1
    )
endfunction()

# Create libc++ interface library
add_library(xinim_libcxx INTERFACE)

# Detect system configuration
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # Clang with libc++
    target_compile_options(xinim_libcxx INTERFACE
        -stdlib=libc++
        -std=c++23
        -fexperimental-library
    )
    
    target_link_options(xinim_libcxx INTERFACE
        -stdlib=libc++
    )
    
    # Find libc++ headers
    find_path(LIBCXX_INCLUDE_DIR
        NAMES __config
        PATHS
            /usr/include/c++/v1
            /usr/local/include/c++/v1
            /opt/homebrew/include/c++/v1
            /opt/local/include/libcxx
    )
    
    if(LIBCXX_INCLUDE_DIR)
        target_include_directories(xinim_libcxx INTERFACE ${LIBCXX_INCLUDE_DIR})
        message(STATUS "Found libc++ headers: ${LIBCXX_INCLUDE_DIR}")
    endif()
    
    # Find libc++ library
    find_library(LIBCXX_LIBRARY
        NAMES c++ libc++
        PATHS
            /usr/lib
            /usr/local/lib
            /opt/homebrew/lib
            /opt/local/lib
    )
    
    if(LIBCXX_LIBRARY)
        target_link_libraries(xinim_libcxx INTERFACE ${LIBCXX_LIBRARY})
        message(STATUS "Found libc++ library: ${LIBCXX_LIBRARY}")
    endif()
    
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    # GCC with libstdc++
    target_compile_options(xinim_libcxx INTERFACE
        -std=c++23
        -fconcepts
        -fcoroutines
    )
    
    # Enable experimental features
    target_compile_definitions(xinim_libcxx INTERFACE
        _GLIBCXX_USE_CXX23=1
    )
    
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
    # MSVC STL
    target_compile_options(xinim_libcxx INTERFACE
        /std:c++23
        /permissive-
        /Zc:__cplusplus
    )
endif()

# Create C17 libc compatibility library
add_library(xinim_libc_compat INTERFACE)

target_compile_definitions(xinim_libc_compat INTERFACE
    _ISOC17_SOURCE=1
    _XOPEN_SOURCE=700
)

# Link both C and C++ standard libraries
target_link_libraries(xinim_libc_compat INTERFACE
    m     # Math library
    dl    # Dynamic loading
    rt    # Real-time extensions
    pthread  # POSIX threads
)

# Combined standard library interface
add_library(xinim_stdlib INTERFACE)
target_link_libraries(xinim_stdlib INTERFACE
    xinim_libcxx
    xinim_libc_compat
)

# Function to check C++23 feature availability
function(xinim_check_cpp23_features)
    include(CheckCXXSourceCompiles)
    
    # Check for std::expected
    check_cxx_source_compiles("
        #include <expected>
        int main() { 
            std::expected<int, int> e{42};
            return e.value();
        }
    " XINIM_HAS_STD_EXPECTED)
    
    # Check for std::format
    check_cxx_source_compiles("
        #include <format>
        int main() {
            auto s = std::format(\"{}\", 42);
            return 0;
        }
    " XINIM_HAS_STD_FORMAT)
    
    # Check for std::ranges improvements
    check_cxx_source_compiles("
        #include <ranges>
        #include <vector>
        int main() {
            std::vector<int> v{1,2,3};
            auto r = v | std::views::as_const;
            return 0;
        }
    " XINIM_HAS_STD_RANGES_V2)
    
    # Check for std::mdspan
    check_cxx_source_compiles("
        #include <mdspan>
        int main() {
            int data[6] = {};
            std::mdspan<int, std::extents<size_t, 2, 3>> m{data};
            return 0;
        }
    " XINIM_HAS_STD_MDSPAN)
    
    # Report findings
    message(STATUS "C++23 Feature Detection:")
    message(STATUS "  std::expected: ${XINIM_HAS_STD_EXPECTED}")
    message(STATUS "  std::format: ${XINIM_HAS_STD_FORMAT}")
    message(STATUS "  std::ranges v2: ${XINIM_HAS_STD_RANGES_V2}")
    message(STATUS "  std::mdspan: ${XINIM_HAS_STD_MDSPAN}")
    
    # Export to parent scope
    set(XINIM_CPP23_FEATURES "" PARENT_SCOPE)
    if(XINIM_HAS_STD_EXPECTED)
        list(APPEND XINIM_CPP23_FEATURES "expected")
    endif()
    if(XINIM_HAS_STD_FORMAT)
        list(APPEND XINIM_CPP23_FEATURES "format")
    endif()
    if(XINIM_HAS_STD_RANGES_V2)
        list(APPEND XINIM_CPP23_FEATURES "ranges_v2")
    endif()
    if(XINIM_HAS_STD_MDSPAN)
        list(APPEND XINIM_CPP23_FEATURES "mdspan")
    endif()
endfunction()

# Perform feature detection
xinim_check_cpp23_features()

# Export configuration
set(XINIM_STDLIB_CONFIG_DONE TRUE)