-- XINIM: Modern C++23 Post-Quantum Microkernel Operating System
-- Minimal xmake Build Configuration for Testing

set_project("XINIM")
set_version("1.0.0")
set_languages("c++23")

-- Basic configuration
add_rules("mode.debug", "mode.release")
set_warnings("all")

-- Include paths
add_includedirs("include")

-- Simple test target
target("xinim-test")
    set_kind("binary")
    add_files("src/main.cpp")
    add_includedirs("include")
