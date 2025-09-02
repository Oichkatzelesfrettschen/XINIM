/*
 * XINIM Native Build System - Simplified C++20 Implementation  
 * BSD 2-Clause License - Pure C++20 for maximum compatibility
 */

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>

namespace xinim::build {

using namespace std::filesystem;
using namespace std::chrono;

struct BuildTarget {
    std::string name;
    std::vector<path> sources;
    std::vector<std::string> flags;
    bool is_library{false};
};

[[nodiscard]] std::vector<path> find_sources(const path& dir, const std::string& ext) {
    std::vector<path> sources;
    
    if (!exists(dir)) return sources;
    
    for (const auto& entry : recursive_directory_iterator(dir)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ext) continue;
        
        // Skip test files and problematic patterns
        const auto filename = entry.path().filename().string();
        if (filename.find("test_") == 0) continue;
        if (filename.find(" 2.") != std::string::npos) continue;
        
        sources.push_back(entry.path());
    }
    
    std::sort(sources.begin(), sources.end());
    return sources;
}

[[nodiscard]] bool execute_command(const std::string& command) {
    std::cout << "Executing: " << command << std::endl;
    return std::system(command.c_str()) == 0;
}

[[nodiscard]] std::string join(const std::vector<std::string>& parts, const std::string& sep) {
    if (parts.empty()) return "";
    
    std::string result = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        result += sep + parts[i];
    }
    return result;
}

class XinimBuilder {
private:
    path source_root_;
    path build_root_;
    std::vector<BuildTarget> targets_;
    
public:
    explicit XinimBuilder(const path& source_root) 
        : source_root_(source_root), build_root_(source_root / "build") {
        create_directories(build_root_);
        setup_targets();
    }
    
    [[nodiscard]] bool build_all() {
        std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                XINIM NATIVE BUILD SYSTEM                      ║\n";  
        std::cout << "║                    C++20 Implementation                       ║\n";
        std::cout << "║                     BSD 2-Clause License                      ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
        
        for (const auto& target : targets_) {
            if (!build_target(target)) {
                std::cerr << "❌ Build failed for target: " << target.name << std::endl;
                return false;
            }
        }
        
        std::cout << "\n🎉 Build completed successfully!" << std::endl;
        return true;
    }
    
private:
    void setup_targets() {
        // Kernel library
        BuildTarget kernel{"xinim_kernel", {}, {"-ffreestanding", "-fno-exceptions", "-fno-rtti"}, true};
        auto kernel_sources = find_sources(source_root_ / "kernel", ".cpp");
        // Remove main.cpp
        kernel_sources.erase(
            std::remove_if(kernel_sources.begin(), kernel_sources.end(),
                          [](const path& p) { return p.filename() == "main.cpp"; }),
            kernel_sources.end()
        );
        kernel.sources = std::move(kernel_sources);
        targets_.push_back(std::move(kernel));
        
        // Crypto library
        BuildTarget crypto{"xinim_crypto", {}, {"-mavx2", "-DXINIM_CRYPTO"}, true};
        crypto.sources = find_sources(source_root_ / "crypto", ".cpp");
        targets_.push_back(std::move(crypto));
        
        // Filesystem library
        BuildTarget fs{"xinim_fs", {}, {}, true};
        fs.sources = find_sources(source_root_ / "fs", ".cpp");
        targets_.push_back(std::move(fs));
        
        // Standard library
        BuildTarget libc{"xinim_libc", {}, {}, true};
        libc.sources = find_sources(source_root_ / "lib", ".cpp");
        targets_.push_back(std::move(libc));
        
        // Commands (just a few key ones to test)
        for (const auto& cmd : {"ar", "ls", "cp", "cat"}) {
            const auto cmd_file = source_root_ / "commands" / (std::string{cmd} + ".cpp");
            if (exists(cmd_file)) {
                BuildTarget cmd_target{std::string{"cmd_"} + cmd, {cmd_file}, {}, false};
                targets_.push_back(std::move(cmd_target));
            }
        }
    }
    
    [[nodiscard]] bool build_target(const BuildTarget& target) {
        if (target.sources.empty()) {
            std::cout << "⚠️  Skipping " << target.name << " (no sources)" << std::endl;
            return true;
        }
        
        std::cout << "\n📦 Building target: " << target.name << std::endl;
        
        const auto target_dir = build_root_ / target.name;
        create_directories(target_dir);
        
        std::vector<std::string> object_files;
        
        // Compile sources
        for (const auto& source : target.sources) {
            const auto obj_file = target_dir / (source.stem().string() + ".o");
            object_files.push_back(obj_file.string());
            
            // Check if recompilation needed
            if (exists(obj_file) && last_write_time(obj_file) >= last_write_time(source)) {
                continue;
            }
            
            std::cout << "  🔨 " << source.filename().string() << std::endl;
            
            std::vector<std::string> cmd_parts = {
                "clang++", "-c", "-std=c++20", "-stdlib=libc++",
                "-Wall", "-Wextra", "-O2",
                "-I" + (source_root_ / "include").string(),
                "-I" + (source_root_ / "include" / "xinim").string()
            };
            
            // Add target-specific flags
            cmd_parts.insert(cmd_parts.end(), target.flags.begin(), target.flags.end());
            
            cmd_parts.push_back("-o");
            cmd_parts.push_back(obj_file.string());
            cmd_parts.push_back(source.string());
            
            if (!execute_command(join(cmd_parts, " "))) {
                return false;
            }
        }
        
        // Link
        std::cout << "  🔗 Linking " << target.name << std::endl;
        
        std::string output_name = target.name;
        std::vector<std::string> link_cmd;
        
        if (target.is_library) {
            output_name = "lib" + target.name + ".a";
            link_cmd = {"ar", "rcs", (target_dir / output_name).string()};
        } else {
            link_cmd = {"clang++", "-stdlib=libc++", "-o", (target_dir / output_name).string()};
        }
        
        link_cmd.insert(link_cmd.end(), object_files.begin(), object_files.end());
        
        return execute_command(join(link_cmd, " "));
    }
};

} // namespace xinim::build

int main() {
    using namespace xinim::build;
    
    const auto source_root = std::filesystem::current_path();
    
    std::cout << "XINIM Native Build System v1.0.0\n";
    std::cout << "Pure C++20 Implementation with BSD License\n";
    std::cout << "==========================================\n\n";
    
    XinimBuilder builder{source_root};
    
    const bool success = builder.build_all();
    return success ? 0 : 1;
}