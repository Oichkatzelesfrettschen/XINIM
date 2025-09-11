-- ═══════════════════════════════════════════════════════════════════════════════
-- XINIM - Simplified xmake Build System for C++23 Operating System
-- ═══════════════════════════════════════════════════════════════════════════════

set_project("XINIM")
set_version("1.0.0")
set_languages("c++23", "c17")

-- ──────────────────────────────────────────────────────────────────────────────
-- Build Configuration
-- ──────────────────────────────────────────────────────────────────────────────

set_defaultmode("release")

if is_mode("release") then
    set_optimize("fastest")
    add_defines("NDEBUG", "XINIM_RELEASE") 
    add_cxxflags("-O3", "-march=native", "-flto")
elseif is_mode("debug") then
    set_optimize("none")
    set_symbols("debug")
    add_defines("_DEBUG", "XINIM_DEBUG")
    add_cxxflags("-O0", "-g3")
end

-- C++23 with libc++
add_cxxflags("-std=c++23", "-stdlib=libc++")
add_ldflags("-stdlib=libc++", "-lc++")

-- Modern warnings
add_cxxflags("-Wall", "-Wextra", "-Wpedantic")

-- ──────────────────────────────────────────────────────────────────────────────
-- Core Libraries
-- ──────────────────────────────────────────────────────────────────────────────

-- Kernel library
target("xinim_kernel")
    set_kind("static")
    add_files("kernel/*.cpp", "kernel/**/*.cpp")
    add_files("arch/**/*.cpp", "arch/**/*.c")
    remove_files("kernel/main.cpp", "**/*test*.cpp")
    add_includedirs("include", "include/xinim")
    add_cxxflags("-ffreestanding", "-fno-exceptions", "-fno-rtti")

-- Memory management
target("xinim_mm")
    set_kind("static")
    add_files("mm/*.cpp")
    add_includedirs("include", "include/xinim")
    add_deps("xinim_kernel")

-- Filesystem
target("xinim_fs") 
    set_kind("static")
    add_files("fs/*.cpp", "lib/xinim_fs/*.cpp")
    add_includedirs("include", "include/xinim")
    add_deps("xinim_kernel")

-- Standard library
target("xinim_libc")
    set_kind("static")  
    add_files("lib/*.cpp")
    remove_files("lib/xinim_fs/*.cpp")
    add_includedirs("include", "include/xinim")

-- Cryptography
target("xinim_crypto")
    set_kind("static")
    add_files("crypto/*.cpp")
    add_includedirs("include", "include/xinim") 
    add_cxxflags("-mavx2", "-msse4.2")

-- ──────────────────────────────────────────────────────────────────────────────
-- Commands
-- ──────────────────────────────────────────────────────────────────────────────

-- Generate command targets
for _, file in ipairs(os.files("commands/*.cpp")) do
    local name = path.basename(file)
    if not name:startswith("test_") then
        target("cmd_" .. name)
            set_kind("binary")
            add_files(file)
            add_includedirs("include", "include/xinim")
            add_deps("xinim_libc", "xinim_fs")
            set_targetdir("$(buildir)/commands")
    end
end

-- ──────────────────────────────────────────────────────────────────────────────
-- Kernel Executable  
-- ──────────────────────────────────────────────────────────────────────────────

target("xinim")
    set_kind("binary")
    add_files("kernel/main.cpp") 
    add_deps("xinim_kernel", "xinim_mm", "xinim_fs", "xinim_crypto")
    set_filename("xinim.elf")

-- ──────────────────────────────────────────────────────────────────────────────
-- Tests
-- ──────────────────────────────────────────────────────────────────────────────

-- Simple test targets
for _, file in ipairs(os.files("tests/test_*.cpp")) do
    local name = path.basename(file)  
    target("test_" .. name)
        set_kind("binary")
        add_files(file)
        add_includedirs("include", "include/xinim")
        add_deps("xinim_kernel", "xinim_crypto", "xinim_libc")
        set_default(false) -- Don't build by default
end