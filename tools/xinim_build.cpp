/*
 * XINIM Native Build System - Pure C++23 Implementation
 * Copyright (c) 2025 XINIM Project
 * 
 * BSD 2-Clause License
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <algorithm>
#include <chrono>
#include <concepts>
#include <coroutine>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <ranges>
#include <regex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace xinim::build {

using namespace std::literals;
using namespace std::filesystem;
using namespace std::chrono;

// ═══════════════════════════════════════════════════════════════════════════════
// § 1. BUILD CONFIGURATION TYPES
// ═══════════════════════════════════════════════════════════════════════════════

enum class BuildType : std::uint8_t {
    debug,
    release,
    release_with_debug,
    min_size
};

enum class TargetType : std::uint8_t {
    executable,
    static_library,
    shared_library,
    kernel_binary
};

enum class Architecture : std::uint8_t {
    x86_64,
    arm64,
    riscv64,
    powerpc64
};

struct CompilerFlags {
    std::vector<std::string> cxx_flags;
    std::vector<std::string> c_flags;
    std::vector<std::string> linker_flags;
    std::vector<std::string> defines;
    std::vector<path> include_dirs;
};

struct BuildTarget {
    std::string name;
    TargetType type;
    std::vector<path> sources;
    std::vector<std::string> dependencies;
    CompilerFlags flags;
    std::optional<path> linker_script;
    path output_dir;
};

struct BuildConfiguration {
    std::string project_name{"XINIM"};
    BuildType build_type{BuildType::release};
    Architecture target_arch{Architecture::arm64};
    std::string cxx_compiler{"clang++"};
    std::string c_compiler{"clang"};
    CompilerFlags global_flags;
    std::vector<BuildTarget> targets;
    path source_root;
    path build_root;
};

// ═══════════════════════════════════════════════════════════════════════════════
// § 2. MODERN C++23 BUILD UTILITIES
// ═══════════════════════════════════════════════════════════════════════════════

template<typename T>
concept StringLike = std::convertible_to<T, std::string_view>;

template<StringLike... Args>
[[nodiscard]] constexpr std::string join(std::string_view delimiter, Args&&... args) {
    std::string result;
    std::size_t total_size = ((std::string_view{args}.size() + delimiter.size()) + ...);
    result.reserve(total_size);
    
    bool first = true;
    ((first ? (first = false, result += args) : 
      (result += delimiter, result += args)), ...);
    
    return result;
}

[[nodiscard]] std::expected<std::vector<path>, std::string> 
find_sources(const path& dir, std::span<const std::string_view> patterns) {
    std::vector<path> sources;
    std::error_code ec;
    
    if (!exists(dir, ec)) {
        return std::unexpected{std::format("Directory does not exist: {}", dir.string())};
    }
    
    for (const auto& entry : recursive_directory_iterator(dir, ec)) {
        if (ec) continue;
        if (!entry.is_regular_file()) continue;
        
        const auto& file_path = entry.path();
        const auto extension = file_path.extension().string();
        
        for (const auto pattern : patterns) {
            if (extension == pattern) {
                sources.push_back(file_path);
                break;
            }
        }
    }
    
    std::ranges::sort(sources);
    return sources;
}

[[nodiscard]] std::expected<void, std::string> 
execute_command(std::string_view command) {
    const auto start_time = steady_clock::now();
    const int result = std::system(command.data());
    const auto elapsed = steady_clock::now() - start_time;
    
    const auto elapsed_ms = duration_cast<milliseconds>(elapsed).count();
    std::println("Command completed in {}ms", elapsed_ms);
    
    if (result != 0) {
        return std::unexpected{std::format("Command failed with exit code: {}", result)};
    }
    
    return {};
}

// ═══════════════════════════════════════════════════════════════════════════════
// § 3. COMPILER DETECTION & CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════════

[[nodiscard]] std::expected<std::string, std::string> 
detect_compiler_version(std::string_view compiler) {
    const auto version_cmd = std::format("{} --version", compiler);
    
    // In a real implementation, we'd capture the output
    // For this demonstration, we'll assume success
    return std::string{"Clang 17.0.0"};
}

[[nodiscard]] CompilerFlags create_cxx23_flags(BuildType build_type) {
    CompilerFlags flags;
    
    // Base C++23 flags
    flags.cxx_flags = {
        "-std=c++23",
        "-stdlib=libc++",
        "-Wall", "-Wextra", "-Wpedantic",
        "-Wcast-align", "-Wcast-qual", "-Wconversion",
        "-Wdouble-promotion", "-Wformat=2",
        "-Wmissing-declarations", "-Wnull-dereference",
        "-Wold-style-cast", "-Woverloaded-virtual",
        "-Wshadow", "-Wsign-conversion", "-Wunused",
        "-Wzero-as-null-pointer-constant"
    };
    
    flags.linker_flags = {"-stdlib=libc++", "-lc++", "-lc++abi"};
    
    switch (build_type) {
        case BuildType::debug:
            flags.cxx_flags.insert(flags.cxx_flags.end(), {
                "-O0", "-g3", "-fno-omit-frame-pointer"
            });
            flags.defines.push_back("_DEBUG");
            break;
            
        case BuildType::release:
            flags.cxx_flags.insert(flags.cxx_flags.end(), {
                "-O3", "-march=native", "-mtune=native", "-flto"
            });
            flags.linker_flags.push_back("-flto");
            flags.defines.push_back("NDEBUG");
            break;
            
        case BuildType::release_with_debug:
            flags.cxx_flags.insert(flags.cxx_flags.end(), {
                "-O2", "-g", "-march=native"
            });
            flags.defines.push_back("NDEBUG");
            break;
            
        case BuildType::min_size:
            flags.cxx_flags.insert(flags.cxx_flags.end(), {
                "-Os", "-ffunction-sections", "-fdata-sections"
            });
            flags.linker_flags.push_back("-Wl,--gc-sections");
            flags.defines.push_back("NDEBUG");
            break;
    }
    
    return flags;
}

// ═══════════════════════════════════════════════════════════════════════════════
// § 4. BUILD ORCHESTRATOR
// ═══════════════════════════════════════════════════════════════════════════════

class XinimBuilder {
private:
    BuildConfiguration config_;
    std::unordered_map<std::string, path> built_targets_;
    
public:
    explicit XinimBuilder(BuildConfiguration config) : config_{std::move(config)} {
        create_directories(config_.build_root);
    }
    
    [[nodiscard]] std::expected<void, std::string> build_all() {
        std::println("╔════════════════════════════════════════════════════════════════╗");
        std::println("║                XINIM NATIVE BUILD SYSTEM                      ║");  
        std::println("╠════════════════════════════════════════════════════════════════╣");
        std::println("║ Project:      {:<49} ║", config_.project_name);
        std::println("║ Build Type:   {:<49} ║", to_string(config_.build_type));
        std::println("║ Architecture: {:<49} ║", to_string(config_.target_arch));
        std::println("║ Compiler:     {:<49} ║", config_.cxx_compiler);
        std::println("╚════════════════════════════════════════════════════════════════╝");
        
        // Build targets in dependency order
        for (const auto& target : config_.targets) {
            auto result = build_target(target);
            if (!result) {
                return std::unexpected{result.error()};
            }
        }
        
        std::println("\n🎉 Build completed successfully!");
        return {};
    }
    
private:
    [[nodiscard]] std::expected<void, std::string> 
    build_target(const BuildTarget& target) {
        std::println("\n📦 Building target: {}", target.name);
        
        const auto target_dir = config_.build_root / target.name;
        create_directories(target_dir);
        
        std::vector<path> object_files;
        
        // Compile sources
        for (const auto& source : target.sources) {
            auto obj_result = compile_source(source, target, target_dir);
            if (!obj_result) {
                return std::unexpected{obj_result.error()};
            }
            object_files.push_back(*obj_result);
        }
        
        // Link target
        return link_target(target, object_files, target_dir);
    }
    
    [[nodiscard]] std::expected<path, std::string>
    compile_source(const path& source, const BuildTarget& target, const path& target_dir) {
        const auto obj_file = target_dir / (source.stem().string() + ".o");
        
        // Check if recompilation is needed
        if (exists(obj_file) && last_write_time(obj_file) >= last_write_time(source)) {
            return obj_file;
        }
        
        std::println("  🔨 Compiling: {}", source.filename().string());
        
        // Build compile command
        std::vector<std::string> cmd_parts = {config_.cxx_compiler, "-c"};
        
        // Add flags
        cmd_parts.insert(cmd_parts.end(), 
                        config_.global_flags.cxx_flags.begin(), 
                        config_.global_flags.cxx_flags.end());
        cmd_parts.insert(cmd_parts.end(),
                        target.flags.cxx_flags.begin(),
                        target.flags.cxx_flags.end());
        
        // Add include directories
        for (const auto& inc_dir : config_.global_flags.include_dirs) {
            cmd_parts.push_back(std::format("-I{}", inc_dir.string()));
        }
        for (const auto& inc_dir : target.flags.include_dirs) {
            cmd_parts.push_back(std::format("-I{}", inc_dir.string()));
        }
        
        // Add defines
        for (const auto& define : config_.global_flags.defines) {
            cmd_parts.push_back(std::format("-D{}", define));
        }
        for (const auto& define : target.flags.defines) {
            cmd_parts.push_back(std::format("-D{}", define));
        }
        
        cmd_parts.push_back("-o");
        cmd_parts.push_back(obj_file.string());
        cmd_parts.push_back(source.string());
        
        const auto command = join(" ", cmd_parts | std::views::all);
        
        auto result = execute_command(command);
        if (!result) {
            return std::unexpected{result.error()};
        }
        
        return obj_file;
    }
    
    [[nodiscard]] std::expected<void, std::string>
    link_target(const BuildTarget& target, 
               std::span<const path> object_files,
               const path& target_dir) {
        
        std::string output_name = target.name;
        if (target.type == TargetType::executable) {
            output_name += (target.name == "xinim" ? ".elf" : "");
        } else if (target.type == TargetType::static_library) {
            output_name = "lib" + target.name + ".a";
        }
        
        const auto output_path = target_dir / output_name;
        std::println("  🔗 Linking: {}", output_name);
        
        std::vector<std::string> cmd_parts;
        
        if (target.type == TargetType::static_library) {
            cmd_parts = {"ar", "rcs", output_path.string()};
        } else {
            cmd_parts = {config_.cxx_compiler, "-o", output_path.string()};
        }
        
        // Add object files
        for (const auto& obj : object_files) {
            cmd_parts.push_back(obj.string());
        }
        
        // Add linker flags for non-static libraries
        if (target.type != TargetType::static_library) {
            cmd_parts.insert(cmd_parts.end(),
                           config_.global_flags.linker_flags.begin(),
                           config_.global_flags.linker_flags.end());
            cmd_parts.insert(cmd_parts.end(),
                           target.flags.linker_flags.begin(),
                           target.flags.linker_flags.end());
            
            // Add linker script if specified
            if (target.linker_script) {
                cmd_parts.push_back(std::format("-T{}", target.linker_script->string()));
                cmd_parts.push_back("-nostdlib");
            }
        }
        
        const auto command = join(" ", cmd_parts | std::views::all);
        
        auto result = execute_command(command);
        if (!result) {
            return std::unexpected{result.error()};
        }
        
        built_targets_[target.name] = output_path;
        return {};
    }
    
    [[nodiscard]] static std::string to_string(BuildType type) {
        switch (type) {
            case BuildType::debug: return "Debug";
            case BuildType::release: return "Release";  
            case BuildType::release_with_debug: return "RelWithDebInfo";
            case BuildType::min_size: return "MinSizeRel";
        }
        return "Unknown";
    }
    
    [[nodiscard]] static std::string to_string(Architecture arch) {
        switch (arch) {
            case Architecture::x86_64: return "x86_64";
            case Architecture::arm64: return "arm64";
            case Architecture::riscv64: return "riscv64"; 
            case Architecture::powerpc64: return "powerpc64";
        }
        return "Unknown";
    }
};

// ═══════════════════════════════════════════════════════════════════════════════
// § 5. CONFIGURATION LOADER
// ═══════════════════════════════════════════════════════════════════════════════

[[nodiscard]] std::expected<BuildConfiguration, std::string>
load_xinim_config(const path& source_root) {
    BuildConfiguration config;
    config.source_root = source_root;
    config.build_root = source_root / "build";
    
    // Set up global C++23 flags
    config.global_flags = create_cxx23_flags(config.build_type);
    config.global_flags.include_dirs = {
        source_root / "include",
        source_root / "include" / "xinim"
    };
    
    // Define kernel target
    BuildTarget kernel_target{
        .name = "xinim_kernel",
        .type = TargetType::static_library,
        .sources = {},
        .dependencies = {},
        .flags = {},
        .linker_script = std::nullopt,
        .output_dir = config.build_root / "kernel"
    };
    
    // Find kernel sources
    auto kernel_sources = find_sources(source_root / "kernel", 
                                     std::array{".cpp"sv, ".c"sv});
    if (kernel_sources) {
        kernel_target.sources = std::move(*kernel_sources);
        // Remove main.cpp from library
        std::erase_if(kernel_target.sources, 
                     [](const path& p) { return p.filename() == "main.cpp"; });
    }
    
    kernel_target.flags.cxx_flags = {
        "-ffreestanding", "-fno-exceptions", "-fno-rtti"
    };
    kernel_target.flags.defines.push_back("XINIM_KERNEL");
    
    config.targets.push_back(std::move(kernel_target));
    
    // Define other core targets
    for (const auto& [name, subdir] : {
        std::pair{"xinim_crypto", "crypto"}, 
        {"xinim_fs", "fs"},
        {"xinim_libc", "lib"}
    }) {
        BuildTarget target{
            .name = name,
            .type = TargetType::static_library, 
            .sources = {},
            .dependencies = {},
            .flags = {},
            .linker_script = std::nullopt,
            .output_dir = config.build_root / name
        };
        
        auto sources = find_sources(source_root / subdir,
                                   std::array{".cpp"sv, ".c"sv});
        if (sources) {
            target.sources = std::move(*sources);
        }
        
        if (name == "xinim_crypto") {
            target.flags.cxx_flags = {"-mavx2", "-msse4.2"};
            target.flags.defines.push_back("XINIM_CRYPTO");
        }
        
        config.targets.push_back(std::move(target));
    }
    
    return config;
}

} // namespace xinim::build

// ═══════════════════════════════════════════════════════════════════════════════
// § 6. MAIN ENTRY POINT
// ═══════════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    using namespace xinim::build;
    
    const auto source_root = std::filesystem::current_path();
    
    std::println("XINIM Native Build System v1.0.0");
    std::println("Pure C++23 Implementation with BSD License");
    std::println("============================================\n");
    
    auto config_result = load_xinim_config(source_root);
    if (!config_result) {
        std::println(stderr, "❌ Configuration error: {}", config_result.error());
        return 1;
    }
    
    XinimBuilder builder{std::move(*config_result)};
    
    auto build_result = builder.build_all();
    if (!build_result) {
        std::println(stderr, "❌ Build error: {}", build_result.error());
        return 1;
    }
    
    std::println("\n✅ XINIM build completed successfully!");
    return 0;
}