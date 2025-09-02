-- XINIM Build Configuration
-- A modern C++23 operating system implementation

set_project("XINIM")
set_version("1.0.0")
set_languages("c++23", "c17")

-- Build modes
set_allowedmodes("debug", "release", "profile")
set_defaultmode("release")

-- Platform configuration
set_allowedplats("macosx", "linux", "bsd")
set_allowedarchs("x86_64", "arm64")

-- Compiler configuration
if is_mode("debug") then
    set_symbols("debug")
    set_optimize("none")
    add_defines("DEBUG", "_DEBUG")
elseif is_mode("release") then
    set_symbols("hidden")
    set_optimize("fastest")
    add_defines("NDEBUG")
elseif is_mode("profile") then
    set_symbols("debug")
    set_optimize("fast")
    add_defines("PROFILE")
end

-- C++23 flags
add_cxxflags("-std=c++23", "-stdlib=libc++")
add_ldflags("-stdlib=libc++")

-- Warning flags
add_cxxflags("-Wall", "-Wextra", "-Wpedantic")

-- Include directories
add_includedirs("include", "include/xinim")

-- Helper function to collect sources
function collect_sources(dir, patterns)
    local sources = {}
    for _, pattern in ipairs(patterns) do
        local files = os.files(path.join(dir, "**", pattern))
        for _, file in ipairs(files) do
            -- Skip test files and duplicates
            local filename = path.basename(file)
            if not filename:find("test_") and not filename:find(" 2%.") then
                table.insert(sources, file)
            end
        end
    end
    return sources
end

-- Kernel library
target("kernel")
    set_kind("static")
    add_files(collect_sources("kernel", {"*.cpp", "*.c"}))
    remove_files("kernel/main.cpp")
    add_cxxflags("-ffreestanding", "-fno-exceptions", "-fno-rtti")
    add_defines("XINIM_KERNEL")

-- Memory management
target("memory")
    set_kind("static")
    add_files(collect_sources("mm", {"*.cpp", "*.c"}))
    add_deps("kernel")

-- Filesystem
target("filesystem")
    set_kind("static")
    add_files(collect_sources("fs", {"*.cpp", "*.c"}))
    add_files(collect_sources("lib/xinim_fs", {"*.cpp", "*.c"}))
    add_deps("kernel")

-- Standard library
target("stdlib")
    set_kind("static")
    local sources = collect_sources("lib", {"*.cpp", "*.c"})
    -- Remove filesystem sources
    for i = #sources, 1, -1 do
        if sources[i]:find("xinim_fs/") then
            table.remove(sources, i)
        end
    end
    add_files(sources)

-- Cryptography with architecture-specific optimizations
target("crypto")
    set_kind("static")
    add_files(collect_sources("crypto", {"*.cpp"}))
    add_includedirs("crypto/kyber_impl")
    add_defines("XINIM_CRYPTO")
    
    -- Architecture-specific flags
    if is_arch("x86_64") then
        add_cxxflags("-mavx2", "-msse4.2", "-maes", "-mpclmul")
        add_defines("XINIM_ARCH_X86_64")
    elseif is_arch("arm64") then
        -- ARM64 NEON is always available
        add_defines("XINIM_ARCH_ARM64")
        if is_plat("macosx") then
            -- Apple Silicon specific optimizations
            add_cxxflags("-mcpu=apple-m1")
        end
    end

-- Commands
for _, file in ipairs(os.files("commands/*.cpp")) do
    local name = path.basename(file)
    if not name:find("test_") and not name:find("legacy") then
        target("cmd_" .. name)
            set_kind("binary")
            add_files(file)
            add_deps("stdlib", "filesystem")
            set_targetdir("$(buildir)/bin")
    end
end

-- Main kernel executable
target("xinim")
    set_kind("binary")
    if os.isfile("kernel/main.cpp") then
        add_files("kernel/main.cpp")
    end
    add_deps("kernel", "memory", "filesystem", "crypto")
    set_filename("xinim.elf")
    if os.isfile("kernel/linker.ld") then
        add_ldflags("-T", "kernel/linker.ld", "-nostdlib")
    end

-- Tests with architecture support
for _, file in ipairs(os.files("tests/*.cpp")) do
    local name = path.basename(file)
    if name:find("^test_") then
        target("test_" .. name)
            set_kind("binary")
            add_files(file)
            add_deps("kernel", "stdlib", "crypto")
            set_default(false)
            set_targetdir("$(builddir)/tests")
            
            -- Add architecture defines for tests
            if is_arch("x86_64") then
                add_defines("XINIM_ARCH_X86_64")
            elseif is_arch("arm64") then
                add_defines("XINIM_ARCH_ARM64")
            end
    end
end