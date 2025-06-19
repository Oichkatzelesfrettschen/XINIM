/**
 * @file echo.cpp
 * @brief Modern C++23 echo utility for XINIM
 * @author Andy Tanenbaum (original), modernized for XINIM
 * 
 * Echo command that outputs its arguments to stdout.
 * Supports -n flag to suppress trailing newline.
 * 
 * Fully modernized with C++23 features including:
 * - constexpr constants
 * - RAII resource management
 * - Type safety improvements
 * - Modern error handling
 */

#include <array>
#include <iostream>
#include <string_view>
#include <vector>

// Modern constants with proper sizing
constexpr size_t BUFFER_SIZE = 2048;

/**
 * @brief Output buffer for collecting echo text
 */
class EchoBuffer {
private:
    std::array<char, BUFFER_SIZE> buffer_{};
    size_t position_{0};
    
public:
    /**
     * @brief Add text to the output buffer
     * @param text String to add to buffer
     * @return true if text was added, false if buffer would overflow
     */
    [[nodiscard]] bool add_text(std::string_view text) noexcept {
        if (position_ + text.size() >= BUFFER_SIZE) {
            return false;  // Buffer would overflow
        }
        
        std::copy(text.begin(), text.end(), buffer_.begin() + position_);
        position_ += text.size();
        return true;
    }
    
    /**
     * @brief Flush buffer contents to stdout
     */
    void flush() const noexcept {
        if (position_ > 0) {
            std::cout.write(buffer_.data(), static_cast<std::streamsize>(position_));
            std::cout.flush();
        }
    }
    
    /**
     * @brief Check if buffer is empty
     * @return true if buffer contains no data
     */
    [[nodiscard]] bool empty() const noexcept {
        return position_ == 0;
    }
};

/**
 * @brief Modern echo implementation with proper error handling
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return Exit status (0 for success, 1 for error)
 */
int main(int argc, char* argv[]) {
    try {
        bool suppress_newline = false;
        int start_index = 1;
        
        // Check for -n flag (suppress trailing newline)
        if (argc > 1 && std::string_view{argv[1]} == "-n") {
            suppress_newline = true;
            start_index = 2;
        }
        
        EchoBuffer output_buffer;
        
        // Process each argument
        for (int i = start_index; i < argc; ++i) {
            if (!output_buffer.add_text(argv[i])) {
                std::cerr << "echo: output too long\n";
                return 1;
            }
            
            // Add space between arguments (except after the last one)
            if (i < argc - 1) {
                if (!output_buffer.add_text(" ")) {
                    std::cerr << "echo: output too long\n";
                    return 1;
                }
            }
        }
        
        // Add trailing newline unless -n flag was specified
        if (!suppress_newline && !output_buffer.empty()) {
            if (!output_buffer.add_text("\n")) {
                std::cerr << "echo: output too long\n";
                return 1;
            }
        }
        
        // Output the final result
        output_buffer.flush();
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "echo: " << e.what() << '\n';
        return 1;
    }
}
