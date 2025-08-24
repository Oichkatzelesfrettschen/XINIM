# CMake configuration for property-based tests using RapidCheck
#
# This subdirectory adds property-based tests demonstrating
# algorithmic invariants with the RapidCheck framework. The tests
# compile as standalone executables and are registered with CTest.

cmake_minimum_required(VERSION 3.16)

include(FetchContent)
# Fetch RapidCheck at configure time. The library is header-only
# with a small runner component and integrates cleanly with CTest.
FetchContent_Declare(
    rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG master
)
FetchContent_MakeAvailable(rapidcheck)

# Example property test: reversing a vector twice yields the original
add_executable(rc_vector_reverse vector_reverse_property.cpp)
target_link_libraries(rc_vector_reverse PRIVATE rapidcheck)
target_compile_options(rc_vector_reverse PRIVATE -Wno-error=deprecated-declarations)

add_test(NAME rc_vector_reverse COMMAND rc_vector_reverse)
