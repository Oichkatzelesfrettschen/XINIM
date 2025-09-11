/**
 * @file mined_simple.cpp
 * @brief Simple standalone MINED editor for immediate use
 * @author XINIM Project
 * @version 2.0
 * @date 2025
 */

#include "mined.hpp"
#include <iostream>
#include <fstream>

using namespace xinim::editor;

/**
 * @brief Entry point for the mined_simple utility.
 * @param argc Number of command-line arguments as per C++23 [basic.start.main].
 * @param argv Array of command-line argument strings.
 * @return Exit status as specified by C++23 [basic.start.main].
 */
int main(int argc, char* argv[]) {
    try {
        std::cout << "XINIM MINED Editor v2.0 - Modern C++23 Implementation\n";
        std::cout << "======================================================\n\n";
        
        if (argc > 1) {
            std::cout << "Loading file: " << argv[1] << "\n";
            
            // Create a simple text buffer and load the file
            TextBuffer buffer;
            std::ifstream file(argv[1]);
            if (file.is_open()) {
                std::string line;
                while (std::getline(file, line)) {
                    buffer.append_line(text::UnicodeString{line});
                }
                file.close();
                
                std::cout << "File loaded successfully!\n";
                std::cout << "Lines: " << buffer.line_count() << "\n";
                std::cout << "Total characters: " << buffer.character_count() << "\n";
                
                // Display first few lines
                std::cout << "\nFirst few lines:\n";
                std::cout << "----------------\n";
                for (LineNumber line_num{1}; line_num.value <= std::min(5u, buffer.line_count()) && line_num.value <= 5; ++line_num) {
                    auto line = buffer.get_line(line_num);
                    if (line) {
                        std::cout << line_num.value << ": " << line->to_string() << "\n";
                    }
                }
            } else {
                std::cout << "Error: Cannot open file " << argv[1] << "\n";
                return 1;
            }
        } else {
            std::cout << "Usage: mined <filename>\n";
            std::cout << "\nThis is a demonstration of the modernized MINED text editor components.\n";
            std::cout << "The full interactive editor is available in the complex implementation.\n";
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown error occurred\n";
        return 1;
    }
}
