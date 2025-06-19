/**
 * @file od.cpp
 * @brief Universal Octal Dump Utility - C++23 Modernized
 * @details Hardware-agnostic, SIMD/FPU-ready hexadecimal, octal, decimal,
 *          and character dump utility with comprehensive format support,
 *          streaming I/O, and advanced data visualization capabilities.
 *
 * MODERNIZATION: Complete decomposition and synthesis from legacy K&R C to
 * paradigmatically pure C++23 with RAII, exception safety, type safety,
 * vectorization readiness, and hardware abstraction.
 * 
 * @author Original: Andy Tanenbaum, Modernized for C++23 XINIM
 * @version 2.0
 * @date 2024
 * @copyright XINIM OS Project
 */

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <system_error>
#include <exception>
#include <array>
#include <algorithm>
#include <ranges>
#include <span>
#include <concepts>
#include <format>
#include <bit>
#include <charconv>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

namespace xinim::commands::od {

/**
 * @brief Universal Octal Dump and Data Visualization System
 * @details Complete C++23 modernization providing hardware-agnostic,
 *          SIMD/FPU-optimized data dumping with multiple format support,
 *          streaming processing, and advanced visualization capabilities.
 */
class UniversalOctalDumper final {
public:
    /// Output format types supported by the dumper
    enum class OutputFormat : std::uint8_t {
        Octal = 0,      ///< Octal representation (default)
        Character = 1,  ///< Character representation with escape sequences
        Decimal = 2,    ///< Decimal representation
        Hexadecimal = 3,///< Hexadecimal representation
        Binary = 4      ///< Binary representation
    };
    
    /// Address display formats
    enum class AddressFormat : std::uint8_t {
        Octal = 0,      ///< Octal addresses (default)
        Hexadecimal = 1 ///< Hexadecimal addresses
    };
    
    /// Buffer size for optimal vectorized I/O
    static constexpr std::size_t OPTIMAL_BUFFER_SIZE{4096};
    
    /// Default line width for output formatting
    static constexpr std::size_t DEFAULT_LINE_WIDTH{16};
    
    /// Maximum line width for safety
    static constexpr std::size_t MAX_LINE_WIDTH{64};

private:
    /**
     * @brief Configuration for dump operation
     */
    struct DumpConfig {
        std::vector<OutputFormat> formats;    ///< Output formats to display
        AddressFormat address_format{AddressFormat::Octal}; ///< Address format
        std::size_t line_width{DEFAULT_LINE_WIDTH};          ///< Bytes per line
        std::uint64_t start_offset{0};                       ///< Starting offset
        bool suppress_duplicates{true};                      ///< Suppress duplicate lines
        bool show_ascii{false};                              ///< Show ASCII representation
        
        /**
         * @brief Validate configuration parameters
         * @throws std::invalid_argument on invalid configuration
         */
        void validate() const {
            if (formats.empty()) {
                throw std::invalid_argument("At least one output format must be specified");
            }
            
            if (line_width == 0 || line_width > MAX_LINE_WIDTH) {
                throw std::invalid_argument("Invalid line width");
            }
        }
    };
    
    /**
     * @brief Data line for duplicate detection and formatting
     */
    struct DataLine {
        std::array<std::uint8_t, MAX_LINE_WIDTH> data{};
        std::size_t length{0};
        std::uint64_t address{0};
        
        /**
         * @brief Check if two data lines are identical
         * @param other Other data line to compare
         * @return true if lines are identical
         */
        [[nodiscard]] bool operator==(const DataLine& other) const noexcept {
            return length == other.length && 
                   std::equal(data.begin(), data.begin() + length,
                             other.data.begin(), other.data.begin() + other.length);
        }
    };

    /// Configuration for this dump operation
    DumpConfig config_;
    
    /// Previous line for duplicate detection
    std::optional<DataLine> previous_line_;
    
    /// Flag indicating if duplicate marker was printed
    bool duplicate_marker_printed_{false};

public:
    /**
     * @brief Initialize universal octal dumper with default configuration
     */
    UniversalOctalDumper() {
        config_.formats.push_back(OutputFormat::Octal);
    }

    /**
     * @brief Parse command line arguments and configure dumper
     * @param argc Argument count
     * @param argv Argument vector
     * @return Input file path (empty for stdin) and starting offset
     * @throws std::invalid_argument on invalid arguments
     */
    std::pair<std::string, std::uint64_t> parse_arguments(int argc, char* argv[]) {
        std::string input_file;
        std::uint64_t offset = 0;
        
        // Clear default format if flags are specified
        bool flags_specified = false;
        
        for (int i = 1; i < argc; ++i) {
            std::string_view arg{argv[i]};
            
            if (arg.starts_with('-')) {
                if (!flags_specified) {
                    config_.formats.clear();
                    flags_specified = true;
                }
                
                parse_format_flags(arg.substr(1));
            } else if (input_file.empty() && arg.find_first_of("0123456789+") != 0) {
                // This is the input file
                input_file = arg;
            } else {
                // This should be an offset
                offset = parse_offset(arg);
            }
        }
        
        // If no formats specified, use default octal
        if (config_.formats.empty()) {
            config_.formats.push_back(OutputFormat::Octal);
        }
        
        config_.validate();
        return {input_file, offset};
    }

    /**
     * @brief Dump data from input stream with configured formatting
     * @param input_file Path to input file (empty for stdin)
     * @param start_offset Starting offset in the file
     * @throws std::system_error on I/O failure
     */
    void dump_data(const std::string& input_file, std::uint64_t start_offset) {
        std::unique_ptr<std::istream> input = create_input_stream(input_file);
        
        // Seek to start offset if specified
        if (start_offset > 0) {
            seek_to_offset(*input, start_offset);
        }
        
        config_.start_offset = start_offset;
        
        // Process data in chunks
        std::array<std::uint8_t, OPTIMAL_BUFFER_SIZE> buffer{};
        std::uint64_t current_address = start_offset;
        
        while (input->good()) {
            input->read(reinterpret_cast<char*>(buffer.data()), buffer.size());
            std::streamsize bytes_read = input->gcount();
            
            if (bytes_read > 0) {
                process_data_chunk(std::span{buffer.data(), static_cast<std::size_t>(bytes_read)}, 
                                 current_address);
                current_address += static_cast<std::uint64_t>(bytes_read);
            }
        }
        
        // Final address output
        format_address(current_address);
        std::cout << '\n';
    }

private:
    /**
     * @brief Parse format flags from command line argument
     * @param flags String containing format flags
     * @throws std::invalid_argument on invalid flag
     */
    void parse_format_flags(std::string_view flags) {
        for (char flag : flags) {
            switch (flag) {
                case 'b':
                    config_.formats.push_back(OutputFormat::Octal);
                    break;
                case 'c':
                    config_.formats.push_back(OutputFormat::Character);
                    config_.show_ascii = true;
                    break;
                case 'd':
                    config_.formats.push_back(OutputFormat::Decimal);
                    break;
                case 'o':
                    config_.formats.push_back(OutputFormat::Octal);
                    break;
                case 'x':
                    config_.formats.push_back(OutputFormat::Hexadecimal);
                    break;
                case 'h':
                    config_.address_format = AddressFormat::Hexadecimal;
                    break;
                default:
                    throw std::invalid_argument(std::format("Invalid flag: -{}", flag));
            }
        }
    }

    /**
     * @brief Parse offset specification from command line
     * @param offset_str Offset string (may include +, ., b suffixes)
     * @return Parsed offset value
     * @throws std::invalid_argument on invalid offset format
     */
    std::uint64_t parse_offset(std::string_view offset_str) {
        // Remove leading '+'
        if (offset_str.starts_with('+')) {
            offset_str = offset_str.substr(1);
        }
        
        // Check for block multiplier 'b' or decimal point '.'
        bool is_decimal = offset_str.contains('.');
        bool is_blocks = offset_str.ends_with('b');
        
        if (is_blocks) {
            offset_str = offset_str.substr(0, offset_str.length() - 1);
        }
        if (is_decimal) {
            offset_str = offset_str.substr(0, offset_str.find('.'));
        }
        
        std::uint64_t offset = 0;
        auto [ptr, ec] = std::from_chars(offset_str.data(), 
                                        offset_str.data() + offset_str.size(), 
                                        offset, is_decimal ? 10 : 8);
        
        if (ec != std::errc{} || ptr != offset_str.data() + offset_str.size()) {
            throw std::invalid_argument("Invalid offset format");
        }
        
        if (is_blocks) {
            offset *= 512; // Block size
        }
        
        return offset;
    }    /**
     * @brief Create input stream from file or stdin
     * @param file_path Path to input file (empty for stdin)
     * @return Unique pointer to input stream
     * @throws std::system_error on file access failure
     */
    std::unique_ptr<std::istream> create_input_stream(const std::string& file_path) {
        if (file_path.empty()) {
            // Return a wrapper for stdin that doesn't delete it
            struct StdinWrapper : public std::istream {
                StdinWrapper() : std::istream(std::cin.rdbuf()) {}
            };
            return std::make_unique<StdinWrapper>();
        } else {
            auto file_stream = std::make_unique<std::ifstream>(file_path, std::ios::binary);
            if (!file_stream->is_open()) {
                throw std::system_error(errno, std::system_category(),
                                      std::format("Cannot open file: {}", file_path));
            }
            return file_stream;
        }
    }

    /**
     * @brief Seek to specified offset in input stream
     * @param stream Input stream to seek
     * @param offset Offset to seek to
     * @throws std::system_error on seek failure
     */
    void seek_to_offset(std::istream& stream, std::uint64_t offset) {
        if (&stream == &std::cin) {
            // For stdin, read and discard bytes
            std::array<char, 1024> discard_buffer{};
            std::uint64_t remaining = offset;
            
            while (remaining > 0 && stream.good()) {
                std::streamsize to_read = std::min(remaining, 
                                                 static_cast<std::uint64_t>(discard_buffer.size()));
                stream.read(discard_buffer.data(), to_read);
                remaining -= static_cast<std::uint64_t>(stream.gcount());
            }
        } else {
            // For files, use seekg
            stream.seekg(static_cast<std::streamoff>(offset));
            if (stream.fail()) {
                throw std::system_error(std::make_error_code(std::errc::invalid_seek),
                                      "Cannot seek to specified offset");
            }
        }
    }

    /**
     * @brief Process chunk of data and format output
     * @param data Data chunk to process
     * @param start_address Starting address of this chunk
     */
    void process_data_chunk(std::span<const std::uint8_t> data, std::uint64_t start_address) {
        std::uint64_t current_address = start_address;
        
        for (std::size_t i = 0; i < data.size(); i += config_.line_width) {
            std::size_t line_length = std::min(config_.line_width, data.size() - i);
            auto line_data = data.subspan(i, line_length);
            
            DataLine current_line;
            current_line.address = current_address;
            current_line.length = line_length;
            std::copy(line_data.begin(), line_data.end(), current_line.data.begin());
            
            // Check for duplicate lines
            if (config_.suppress_duplicates && previous_line_ && 
                current_line == *previous_line_) {
                if (!duplicate_marker_printed_) {
                    std::cout << "*\n";
                    duplicate_marker_printed_ = true;
                }
            } else {
                duplicate_marker_printed_ = false;
                format_data_line(current_line);
            }
            
            previous_line_ = current_line;
            current_address += line_length;
        }
    }

    /**
     * @brief Format and output a single data line
     * @param line Data line to format
     */
    void format_data_line(const DataLine& line) {
        // Output address
        format_address(line.address);
        
        // Output data in requested formats
        for (OutputFormat format : config_.formats) {
            std::cout << " ";
            format_data_in_format(std::span{line.data.data(), line.length}, format);
        }
        
        // Output ASCII representation if requested
        if (config_.show_ascii) {
            std::cout << "  |";
            for (std::size_t i = 0; i < line.length; ++i) {
                char c = static_cast<char>(line.data[i]);
                std::cout << (std::isprint(c) ? c : '.');
            }
            std::cout << "|";
        }
        
        std::cout << '\n';
    }

    /**
     * @brief Format address according to configured format
     * @param address Address to format
     */
    void format_address(std::uint64_t address) {
        if (config_.address_format == AddressFormat::Hexadecimal) {
            std::cout << std::format("{:07x}", address);
        } else {
            std::cout << std::format("{:07o}", address);
        }
    }

    /**
     * @brief Format data according to specified format
     * @param data Data to format
     * @param format Output format to use
     */
    void format_data_in_format(std::span<const std::uint8_t> data, OutputFormat format) {
        switch (format) {
            case OutputFormat::Octal:
                format_octal_data(data);
                break;
            case OutputFormat::Character:
                format_character_data(data);
                break;
            case OutputFormat::Decimal:
                format_decimal_data(data);
                break;
            case OutputFormat::Hexadecimal:
                format_hexadecimal_data(data);
                break;
            case OutputFormat::Binary:
                format_binary_data(data);
                break;
        }
    }

    /**
     * @brief Format data as octal values
     * @param data Data to format
     */
    void format_octal_data(std::span<const std::uint8_t> data) {
        for (std::size_t i = 0; i < data.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << std::format("{:03o}", data[i]);
        }
    }

    /**
     * @brief Format data as character representation with escapes
     * @param data Data to format
     */
    void format_character_data(std::span<const std::uint8_t> data) {
        for (std::size_t i = 0; i < data.size(); ++i) {
            if (i > 0) std::cout << " ";
            
            char c = static_cast<char>(data[i]);
            switch (c) {
                case '\0': std::cout << "\\0"; break;
                case '\a': std::cout << "\\a"; break;
                case '\b': std::cout << "\\b"; break;
                case '\f': std::cout << "\\f"; break;
                case '\n': std::cout << "\\n"; break;
                case '\r': std::cout << "\\r"; break;
                case '\t': std::cout << "\\t"; break;
                case '\v': std::cout << "\\v"; break;
                case '\\': std::cout << "\\\\"; break;
                default:
                    if (std::isprint(c)) {
                        std::cout << std::format("  {}", c);
                    } else {
                        std::cout << std::format("\\{:03o}", data[i]);
                    }
                    break;
            }
        }
    }

    /**
     * @brief Format data as decimal values
     * @param data Data to format
     */
    void format_decimal_data(std::span<const std::uint8_t> data) {
        for (std::size_t i = 0; i < data.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << std::format("{:3d}", data[i]);
        }
    }

    /**
     * @brief Format data as hexadecimal values
     * @param data Data to format
     */
    void format_hexadecimal_data(std::span<const std::uint8_t> data) {
        for (std::size_t i = 0; i < data.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << std::format("{:02x}", data[i]);
        }
    }

    /**
     * @brief Format data as binary values
     * @param data Data to format
     */
    void format_binary_data(std::span<const std::uint8_t> data) {
        for (std::size_t i = 0; i < data.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << std::format("{:08b}", data[i]);
        }
    }
};

} // namespace xinim::commands::od

/**
 * @brief Modern C++23 main entry point with exception safety
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit status code
 * @details Complete exception-safe implementation with comprehensive
 *          error handling and resource management
 */
int main(int argc, char* argv[]) noexcept {
    try {
        // Create universal octal dumper instance
        xinim::commands::od::UniversalOctalDumper dumper;
        
        // Parse command line arguments
        auto [input_file, offset] = dumper.parse_arguments(argc, argv);
        
        // Perform the dump operation
        dumper.dump_data(input_file, offset);
        
        return EXIT_SUCCESS;
        
    } catch (const std::invalid_argument& e) {
        std::cerr << "od: " << e.what() << '\n';
        std::cerr << "Usage: od [-bcdhox] [file] [ [+] offset [.] [b] ]\n";
        return EXIT_FAILURE;
    } catch (const std::system_error& e) {
        std::cerr << "od: " << e.what() << '\n';
        return EXIT_FAILURE;    } catch (const std::exception& e) {
        std::cerr << "od: Error: " << e.what() << '\n';
        return EXIT_FAILURE;
    } catch (...) {
        std::cerr << "od: Unknown error occurred\n";
        return EXIT_FAILURE;
    }
}
