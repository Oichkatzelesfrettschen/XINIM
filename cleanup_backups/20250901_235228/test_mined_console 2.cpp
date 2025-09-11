/**
 * @file test_mined_console.cpp
 * @brief Simple console simulator to test MINED editor within XINIM context
 * @author XINIM Project
 * @version 1.0
 * @date 2025
 */

#include "mined_final.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>

namespace xinim::console {

/// Simple command structure
struct Command {
    std::string name;
    std::vector<std::string> args;
    
    static Command parse(const std::string& input) {
        std::istringstream iss(input);
        Command cmd;
        iss >> cmd.name;
        
        std::string arg;
        while (iss >> arg) {
            cmd.args.push_back(arg);
        }
        return cmd;
    }
};

/// Console simulator for testing MINED
class SimpleConsole {
private:
    bool running_{true};
    std::string prompt_{"xinim$ "};

public:
    auto run() -> void {
        std::cout << "XINIM Console Simulator v1.0\n";
        std::cout << "==============================\n";
        std::cout << "Available commands:\n";
        std::cout << "  mined [file]  - Launch MINED editor\n";
        std::cout << "  help          - Show help\n";
        std::cout << "  exit          - Exit console\n";
        std::cout << "\n";
        
        std::string input;
        while (running_) {
            std::cout << prompt_;
            if (!std::getline(std::cin, input)) {
                break; // EOF reached
            }
            
            if (input.empty()) continue;
            
            auto cmd = Command::parse(input);
            handle_command(cmd);
        }
        
        std::cout << "\nXINIM Console Simulator terminated.\n";
    }

private:
    auto handle_command(const Command& cmd) -> void {
        if (cmd.name == "exit" || cmd.name == "quit") {
            running_ = false;
        } else if (cmd.name == "help") {
            show_help();
        } else if (cmd.name == "mined") {
            launch_mined(cmd.args);
        } else if (cmd.name == "echo") {
            for (const auto& arg : cmd.args) {
                std::cout << arg << " ";
            }
            std::cout << "\n";
        } else if (cmd.name == "clear") {
            std::cout << "\033[2J\033[H"; // Clear screen
        } else {
            std::cout << "Unknown command: " << cmd.name << "\n";
            std::cout << "Type 'help' for available commands.\n";
        }
    }
    
    auto show_help() -> void {
        std::cout << "\nXINIM Console Simulator Commands:\n";
        std::cout << "=================================\n";
        std::cout << "  mined [file]  - Launch MINED editor with optional file\n";
        std::cout << "  echo <text>   - Echo text to console\n";
        std::cout << "  clear         - Clear console screen\n";
        std::cout << "  help          - Show this help\n";
        std::cout << "  exit, quit    - Exit console\n";
        std::cout << "\n";
    }
    
    auto launch_mined(const std::vector<std::string>& args) -> void {
        std::cout << "\n=== Launching MINED Editor ===\n";
        
        try {
            auto editor = xinim::mined::create_editor();
            
            // Load file if specified
            if (!args.empty()) {
                std::filesystem::path file_path{args[0]};
                std::cout << "Loading file: " << file_path.string() << "\n";
                
                auto result = editor->load_file(file_path);
                if (!result) {
                    std::cout << "Failed to load file: " << result.error() << "\n";
                    std::cout << "Starting with empty buffer.\n";
                }
            }
            
            // Run the editor
            auto run_result = editor->run();
            if (!run_result) {
                std::cerr << "Editor error: " << run_result.error() << "\n";
            }
            
        } catch (const std::exception& e) {
            std::cerr << "MINED error: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "Unknown MINED error occurred\n";
        }
        
        std::cout << "=== MINED Editor Closed ===\n\n";
    }
};

} // namespace xinim::console

int main() {
    xinim::console::SimpleConsole console;
    console.run();
    return 0;
}
