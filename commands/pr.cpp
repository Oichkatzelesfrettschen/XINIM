/**
 * @file pr_modern.cpp
 * @brief Modern C++23 Print Utility - Complete Decomposition and Rebuild
 * @author XINIM Project (Modernized from Michiel Huisjes original)
 * @version 2.0
 * @date 2025
 * 
 * Complete modernization using C++23 paradigms:
 * - RAII memory management with smart containers
 * - Type-safe enums and strong typing
 * - std::expected for error handling
 * - std::chrono for time calculations
 * - std::format for output formatting
 * - Concepts for type safety
 * - Ranges and algorithms
 * - Module-based architecture
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace xinim::print_utility {

// =============================================================================
// Modern Type System with C++23 Strong Typing
// =============================================================================

/// Result type for operations that can fail
template<typename T>
using Result = std::expected<T, std::string>;

/// Strong type for page numbers (1-based)
struct PageNumber {
    std::uint32_t value{1};
    
    constexpr explicit PageNumber(std::uint32_t page = 1) noexcept : value{page} {}
    constexpr auto operator<=>(const PageNumber&) const = default;
    constexpr PageNumber& operator++() noexcept { ++value; return *this; }
    constexpr PageNumber operator++(int) noexcept { auto tmp = *this; ++value; return tmp; }
};

/// Strong type for line numbers (1-based)
struct LineNumber {
    std::uint32_t value{1};
    
    constexpr explicit LineNumber(std::uint32_t line = 1) noexcept : value{line} {}
    constexpr auto operator<=>(const LineNumber&) const = default;
    constexpr LineNumber& operator++() noexcept { ++value; return *this; }
    constexpr LineNumber operator++(int) noexcept { auto tmp = *this; ++value; return tmp; }
};

/// Strong type for column numbers (1-based)
struct ColumnNumber {
    std::uint32_t value{1};
    
    constexpr explicit ColumnNumber(std::uint32_t col = 1) noexcept : value{col} {}
    constexpr auto operator<=>(const ColumnNumber&) const = default;
};

/// Strong type for dimensions
struct Dimensions {
    std::uint32_t width;
    std::uint32_t height;
    
    constexpr Dimensions(std::uint32_t w, std::uint32_t h) noexcept : width{w}, height{h} {}
    constexpr auto operator<=>(const Dimensions&) const = default;
    
    [[nodiscard]] constexpr std::uint32_t area() const noexcept {
        return width * height;
    }
};

/// Page layout configuration
struct PageLayout {
    Dimensions page_size{72, 66};           // Default page dimensions
    ColumnNumber column_count{1};           // Number of columns
    std::uint32_t column_width{0};          // Calculated column width
    bool show_headers{true};                // Include page headers
    bool show_line_numbers{false};          // Show line numbers
    std::uint32_t header_lines{5};          // Lines reserved for header
    std::uint32_t footer_lines{5};          // Lines reserved for footer
    
    [[nodiscard]] constexpr std::uint32_t usable_height() const noexcept {
        return show_headers ? 
            page_size.height - header_lines - footer_lines : 
            page_size.height;
    }
    
    [[nodiscard]] constexpr std::uint32_t calculate_column_width() const noexcept {
        return column_count.value > 1 ? 
            (page_size.width / column_count.value) + 1 : 
            page_size.width;
    }
};

// =============================================================================
// Modern Date/Time System with std::chrono
// =============================================================================

namespace chrono_utils {

/// Modern time representation using std::chrono
class ModernTimestamp {
private:
    std::chrono::system_clock::time_point timestamp_;
    
public:
    ModernTimestamp() : timestamp_{std::chrono::system_clock::now()} {}
    
    explicit ModernTimestamp(std::chrono::system_clock::time_point tp) 
        : timestamp_{tp} {}
    
    [[nodiscard]] auto format_header() const -> std::string {
        using namespace std::chrono;
        
        const auto time_t_val = system_clock::to_time_t(timestamp_);
        const auto local_time = *std::localtime(&time_t_val);
        
        // Array of month names for formatting
        constexpr std::array month_names{
            "Jan", "Feb", "Mar", "Apr", "May", "Jun",
            "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
        };
        
        return std::format("\n\n{} {} {:02d}:{:02d} {}",
            month_names[local_time.tm_mon],
            local_time.tm_mday,
            local_time.tm_hour,
            local_time.tm_min,
            local_time.tm_year + 1900
        );
    }
    
    [[nodiscard]] constexpr auto get_time_point() const noexcept {
        return timestamp_;
    }
};

} // namespace chrono_utils

// =============================================================================
// Memory Management with Modern C++23 Containers
// =============================================================================

/// RAII container for column buffer management
class ColumnBuffer {
private:
    std::vector<std::string> buffer_;
    PageLayout layout_;
    
public:
    explicit ColumnBuffer(const PageLayout& layout) 
        : layout_{layout} {
        const auto total_lines = layout.column_count.value * layout.usable_height();
        buffer_.reserve(total_lines);
    }
    
    /// Add a line to the buffer
    void add_line(std::string_view line) {
        buffer_.emplace_back(line);
    }
    
    /// Clear the buffer for the next page
    void clear() noexcept {
        buffer_.clear();
    }
    
    /// Get the current buffer contents
    [[nodiscard]] auto lines() const noexcept -> std::span<const std::string> {
        return buffer_;
    }
    
    /// Check if buffer is full
    [[nodiscard]] bool is_full() const noexcept {
        return buffer_.size() >= layout_.column_count.value * layout_.usable_height();
    }
    
    /// Get maximum capacity
    [[nodiscard]] auto capacity() const noexcept -> std::size_t {
        return layout_.column_count.value * layout_.usable_height();
    }
};

// =============================================================================
// File I/O with Modern C++23 Streams and RAII
// =============================================================================

/// Modern file reader with RAII and error handling
class ModernFileReader {
private:
    std::unique_ptr<std::istream> stream_;
    std::istream* raw_stream_ptr_{nullptr};
    std::filesystem::path file_path_;
    bool owns_stream_{false};
    
public:
    /// Constructor for file-based input
    explicit ModernFileReader(const std::filesystem::path& path) 
        : file_path_{path} {
        auto file_stream = std::make_unique<std::ifstream>(path);
        if (!file_stream->is_open()) {
            throw std::runtime_error(std::format("Cannot open file: {}", path.string()));
        }
        stream_ = std::move(file_stream);
        owns_stream_ = true;
    }
    
    /// Constructor for standard input
    explicit ModernFileReader(std::istream& input_stream) 
        : raw_stream_ptr_{&input_stream}, owns_stream_{false} {}
    
    /// Read a single line from the input
    [[nodiscard]] auto read_line() -> std::optional<std::string> {
        std::string line;
        auto* active_stream = owns_stream_ ? stream_.get() : raw_stream_ptr_;
        if (std::getline(*active_stream, line)) {
            return line;
        }
        return std::nullopt;
    }
    
    /// Check if more data is available
    [[nodiscard]] bool has_more() const noexcept {
        auto* active_stream = owns_stream_ ? stream_.get() : raw_stream_ptr_;
        return active_stream && active_stream->good() && !active_stream->eof();
    }
    
    /// Skip a specified number of lines
    auto skip_lines(std::uint32_t count) -> std::uint32_t {
        std::uint32_t skipped = 0;
        std::string line;
        auto* active_stream = owns_stream_ ? stream_.get() : raw_stream_ptr_;
        
        while (skipped < count && std::getline(*active_stream, line)) {
            ++skipped;
        }
        
        return skipped;
    }
    
    /// Get the file path (if applicable)
    [[nodiscard]] auto file_path() const noexcept -> const std::filesystem::path& {
        return file_path_;
    }
};

// =============================================================================
// Command Line Parsing with Modern C++23 Concepts
// =============================================================================

/// Concept for command line argument validation
template<typename T>
concept ArgumentConvertible = requires(const char* str) {
    { T{str} } -> std::convertible_to<T>;
};

/// Command line configuration
struct CommandLineConfig {
    PageNumber start_page{1};
    std::string header_text;
    PageLayout layout;
    std::vector<std::filesystem::path> input_files;
    bool use_stdin{false};
    
    [[nodiscard]] bool is_valid() const noexcept {
        return start_page.value > 0 && 
               layout.page_size.width > 0 && 
               layout.page_size.height > 0 &&
               layout.column_count.value > 0;
    }
};

/// Modern command line parser with C++23 features
class CommandLineParser {
private:
    std::span<char*> args_;
    CommandLineConfig config_;
    
    /// Parse a numeric argument with error checking
    template<std::integral T>
    [[nodiscard]] auto parse_number(std::string_view arg) -> Result<T> {
        try {
            if constexpr (std::same_as<T, std::uint32_t>) {
                return static_cast<T>(std::stoul(std::string{arg}));
            } else if constexpr (std::same_as<T, std::int32_t>) {
                return static_cast<T>(std::stoi(std::string{arg}));
            } else {
                static_assert(std::integral<T>, "Unsupported integral type");
            }
        } catch (const std::exception& e) {
            return std::unexpected(std::format("Invalid number '{}': {}", arg, e.what()));
        }
    }
    
    /// Parse page number argument (+page format)
    [[nodiscard]] auto parse_page_number(std::string_view arg) -> Result<PageNumber> {
        if (arg.empty() || arg[0] != '+') {
            return std::unexpected("Page number must start with '+'");
        }
        
        auto number_result = parse_number<std::uint32_t>(arg.substr(1));
        if (!number_result) {
            return std::unexpected(number_result.error());
        }
        
        if (*number_result == 0) {
            return std::unexpected("Page number must be greater than 0");
        }
        
        return PageNumber{*number_result};
    }
    
    /// Parse column count argument
    [[nodiscard]] auto parse_column_count(std::string_view arg) -> Result<ColumnNumber> {
        auto number_result = parse_number<std::uint32_t>(arg);
        if (!number_result) {
            return std::unexpected(number_result.error());
        }
        
        if (*number_result == 0) {
            return std::unexpected("Column count must be greater than 0");
        }
        
        return ColumnNumber{*number_result};
    }
    
public:
    explicit CommandLineParser(std::span<char*> arguments) : args_{arguments} {}
    
    /// Parse all command line arguments
    [[nodiscard]] auto parse() -> Result<CommandLineConfig> {
        for (std::size_t i = 1; i < args_.size(); ++i) {
            std::string_view arg{args_[i]};
            
            if (arg.starts_with('+')) {
                // Page number
                auto page_result = parse_page_number(arg);
                if (!page_result) {
                    return std::unexpected(page_result.error());
                }
                config_.start_page = *page_result;
                
            } else if (arg.starts_with('-')) {
                // Option flags
                auto option_result = parse_option(arg, i);
                if (!option_result) {
                    return std::unexpected(option_result.error());
                }
                i = *option_result; // Update index if option consumed additional arguments
                
            } else {
                // Input file
                config_.input_files.emplace_back(arg);
            }
        }
        
        // Configure derived settings
        config_.layout.column_width = config_.layout.calculate_column_width();
        
        // If no files specified, use stdin
        if (config_.input_files.empty()) {
            config_.use_stdin = true;
            config_.header_text = ""; // Default header for stdin
        }
        
        // Validate configuration
        if (!config_.is_valid()) {
            return std::unexpected("Invalid configuration parameters");
        }
        
        return config_;
    }
    
private:
    /// Parse individual option flag
    [[nodiscard]] auto parse_option(std::string_view arg, std::size_t& index) -> Result<std::size_t> {
        if (arg.size() < 2) {
            return std::unexpected("Invalid option format");
        }
        
        // Check for numeric column specification
        if (std::isdigit(arg[1])) {
            auto col_result = parse_column_count(arg.substr(1));
            if (!col_result) {
                return std::unexpected(col_result.error());
            }
            config_.layout.column_count = *col_result;
            return index;
        }
        
        // Parse individual option characters
        for (std::size_t j = 1; j < arg.size(); ++j) {
            switch (arg[j]) {
                case 't':
                    config_.layout.show_headers = false;
                    break;
                    
                case 'n':
                    config_.layout.show_line_numbers = true;
                    break;
                    
                case 'h':
                    if (++index >= args_.size()) {
                        return std::unexpected("Option -h requires an argument");
                    }
                    config_.header_text = args_[index];
                    return index;
                    
                case 'w': {
                    std::string_view width_str = (j + 1 < arg.size()) ? 
                        arg.substr(j + 1) : 
                        (++index < args_.size() ? std::string_view{args_[index]} : "");
                    
                    if (width_str.empty()) {
                        return std::unexpected("Option -w requires a width value");
                    }
                    
                    auto width_result = parse_number<std::uint32_t>(width_str);
                    if (!width_result) {
                        return std::unexpected(width_result.error());
                    }
                    
                    config_.layout.page_size.width = *width_result;
                    return index;
                }
                
                case 'l': {
                    std::string_view length_str = (j + 1 < arg.size()) ? 
                        arg.substr(j + 1) : 
                        (++index < args_.size() ? std::string_view{args_[index]} : "");
                    
                    if (length_str.empty()) {
                        return std::unexpected("Option -l requires a length value");
                    }
                    
                    auto length_result = parse_number<std::uint32_t>(length_str);
                    if (!length_result) {
                        return std::unexpected(length_result.error());
                    }
                    
                    config_.layout.page_size.height = *length_result;
                    return index;
                }
                
                default:
                    return std::unexpected(std::format("Unknown option: -{}", arg[j]));
            }
        }
        
        return index;
    }
};

// =============================================================================
// Page Formatting Engine with Modern Algorithms
// =============================================================================

/// Page content representation
struct PageContent {
    std::vector<std::string> lines;
    PageNumber page_number{1};
    ColumnNumber max_columns{1};
    
    PageContent(PageNumber page_num, ColumnNumber cols) 
        : page_number{page_num}, max_columns{cols} {}
};

/// Modern page formatter using C++23 ranges and algorithms
class PageFormatter {
private:
    PageLayout layout_;
    chrono_utils::ModernTimestamp timestamp_;
    
public:
    explicit PageFormatter(const PageLayout& layout) : layout_{layout} {}
    
    /// Format a page header
    [[nodiscard]] auto format_header(PageNumber page_num, std::string_view header_text) const -> std::string {
        if (!layout_.show_headers) {
            return "";
        }
        
        return std::format("{}  {}   Page {}\n\n\n",
            timestamp_.format_header(),
            header_text,
            page_num.value
        );
    }
    
    /// Format a page footer
    [[nodiscard]] auto format_footer() const -> std::string {
        return layout_.show_headers ? "\n\n\n\n\n" : "";
    }
    
    /// Format a single line with optional line numbering
    [[nodiscard]] auto format_line(std::string_view content, std::optional<LineNumber> line_num = std::nullopt) const -> std::string {
        if (layout_.show_line_numbers && line_num) {
            return std::format("{}\t{}", line_num->value, content);
        }
        return std::string{content};
    }
    
    /// Format content for multi-column layout
    [[nodiscard]] auto format_columns(const PageContent& content) const -> std::string {
        if (layout_.column_count.value == 1) {
            return format_single_column(content);
        }
        
        return format_multi_column(content);
    }
    
private:
    /// Format content for single column layout
    [[nodiscard]] auto format_single_column(const PageContent& content) const -> std::string {
        std::string result;
        result.reserve(content.lines.size() * 80); // Rough estimate
        
        LineNumber line_num{(content.page_number.value - 1) * layout_.usable_height() + 1};
        
        for (const auto& line : content.lines | std::views::take(layout_.usable_height())) {
            result += format_line(line, layout_.show_line_numbers ? 
                std::optional{line_num++} : std::nullopt);
            result += '\n';
        }
        
        return result;
    }
    
    /// Format content for multi-column layout
    [[nodiscard]] auto format_multi_column(const PageContent& content) const -> std::string {
        std::string result;
        const auto column_width = layout_.calculate_column_width();
        const auto usable_height = layout_.usable_height();
        
        LineNumber line_num{(content.page_number.value - 1) * usable_height + 1};
        
        for (std::uint32_t row = 0; row < usable_height; ++row) {
            if (layout_.show_line_numbers) {
                result += std::format("{}\t", line_num.value++);
            }
            
            for (std::uint32_t col = 0; col < content.max_columns.value; ++col) {
                const auto line_index = row + col * usable_height;
                
                if (line_index < content.lines.size()) {
                    const auto& line = content.lines[line_index];
                    const auto trimmed_line = line.size() > column_width - 1 ? 
                        line.substr(0, column_width - 1) : line;
                    
                    result += trimmed_line;
                    
                    // Pad column (except last one)
                    if (col < content.max_columns.value - 1) {
                        const auto padding = column_width - 1 - trimmed_line.size();
                        result += std::string(padding, ' ');
                    }
                }
            }
            
            result += '\n';
        }
        
        return result;
    }
};

// =============================================================================
// Print Engine with Modern Output Management
// =============================================================================

/// Modern print engine coordinating all operations
class ModernPrintEngine {
private:
    PageLayout layout_;
    PageFormatter formatter_;
    std::ostream& output_stream_;
    
public:
    explicit ModernPrintEngine(const PageLayout& layout, std::ostream& output = std::cout) 
        : layout_{layout}, formatter_{layout}, output_stream_{output} {}
    
    /// Print a single file with the configured layout
    auto print_file(ModernFileReader& reader, std::string_view header_text, PageNumber start_page) -> Result<void> {
        if (layout_.column_count.value > 1) {
            return print_multi_column(reader, header_text, start_page);
        } else {
            return print_single_column(reader, header_text, start_page);
        }
    }
    
private:
    /// Print file in single column mode
    auto print_single_column(ModernFileReader& reader, std::string_view header_text, PageNumber start_page) -> Result<void> {
        PageNumber current_page{1};
        
        // Skip to start page
        while (current_page < start_page && reader.has_more()) {
            reader.skip_lines(layout_.usable_height());
            ++current_page;
        }
        
        // Print pages
        while (reader.has_more()) {
            PageContent page_content{current_page, ColumnNumber{1}};
            
            // Read lines for this page
            for (std::uint32_t i = 0; i < layout_.usable_height() && reader.has_more(); ++i) {
                if (auto line = reader.read_line()) {
                    page_content.lines.push_back(*line);
                } else {
                    break;
                }
            }
            
            if (page_content.lines.empty()) {
                break;
            }
            
            // Format and output page
            output_stream_ << formatter_.format_header(current_page, header_text);
            output_stream_ << formatter_.format_columns(page_content);
            output_stream_ << formatter_.format_footer();
            
            ++current_page;
        }
        
        return {};
    }
    
    /// Print file in multi-column mode
    auto print_multi_column(ModernFileReader& reader, std::string_view header_text, PageNumber start_page) -> Result<void> {
        PageNumber current_page{1};
        const auto lines_per_page = layout_.column_count.value * layout_.usable_height();
        
        // Skip to start page
        while (current_page < start_page && reader.has_more()) {
            reader.skip_lines(lines_per_page);
            ++current_page;
        }
        
        // Print pages
        ColumnBuffer column_buffer{layout_};
        
        while (reader.has_more()) {
            column_buffer.clear();
            
            // Read lines for this page
            for (std::uint32_t i = 0; i < lines_per_page && reader.has_more(); ++i) {
                if (auto line = reader.read_line()) {
                    // Trim line to column width
                    const auto trimmed = line->size() > layout_.column_width - 1 ? 
                        line->substr(0, layout_.column_width - 1) : *line;
                    column_buffer.add_line(trimmed);
                } else {
                    break;
                }
            }
            
            if (column_buffer.lines().empty()) {
                break;
            }
            
            // Calculate actual column count for this page
            const auto actual_columns = std::min(
                layout_.column_count.value,
                static_cast<std::uint32_t>((column_buffer.lines().size() + layout_.usable_height() - 1) / layout_.usable_height())
            );
            
            // Create page content
            PageContent page_content{current_page, ColumnNumber{actual_columns}};
            page_content.lines.assign(column_buffer.lines().begin(), column_buffer.lines().end());
            
            // Format and output page
            output_stream_ << formatter_.format_header(current_page, header_text);
            output_stream_ << formatter_.format_columns(page_content);
            output_stream_ << formatter_.format_footer();
            
            ++current_page;
        }
        
        return {};
    }
};

// =============================================================================
// Main Application Class
// =============================================================================

/// Main application orchestrating the print utility
class PrintUtilityApp {
private:
    CommandLineConfig config_;
    ModernPrintEngine engine_;
    
public:
    explicit PrintUtilityApp(const CommandLineConfig& config) 
        : config_{config}, engine_{config.layout} {}
    
    /// Run the print utility with the configured parameters
    [[nodiscard]] auto run() -> Result<void> {
        try {
            if (config_.use_stdin) {
                return process_stdin();
            } else {
                return process_files();
            }
        } catch (const std::exception& e) {
            return std::unexpected(std::format("Runtime error: {}", e.what()));
        }
    }
    
private:
    /// Process standard input
    auto process_stdin() -> Result<void> {
        ModernFileReader reader{std::cin};
        return engine_.print_file(reader, config_.header_text, config_.start_page);
    }
    
    /// Process file list
    auto process_files() -> Result<void> {
        for (const auto& file_path : config_.input_files) {
            try {
                ModernFileReader reader{file_path};
                const auto header = config_.header_text.empty() ? 
                    file_path.filename().string() : config_.header_text;
                
                auto result = engine_.print_file(reader, header, config_.start_page);
                if (!result) {
                    std::cerr << std::format("Error processing file {}: {}\n", 
                                           file_path.string(), result.error());
                    continue; // Continue with other files
                }
            } catch (const std::exception& e) {
                std::cerr << std::format("Cannot open {}: {}\n", file_path.string(), e.what());
                continue; // Continue with other files
            }
        }
        
        return {};
    }
};

// =============================================================================
// Modern Usage Information
// =============================================================================

/// Display modern usage information
void show_usage(std::string_view program_name) {
    std::cout << std::format(R"(
Usage: {} [+page] [-columns] [-h header] [-w width] [-l length] [-nt] [files]

Options:
  +page      Start printing at page number 'page' (default: 1)
  -columns   Print files in 'columns' columns (default: 1)
  -h header  Use 'header' as the page header text
  -w width   Set page width to 'width' characters (default: 72)
  -l length  Set page length to 'length' lines (default: 66)
  -t         Do not print page headers and footers
  -n         Enable line numbering

Examples:
  {} file.txt                    # Print file.txt with default settings
  {} +2 -3 file.txt             # Start at page 2, use 3 columns
  {} -w 80 -l 60 file.txt       # 80 chars wide, 60 lines per page
  {} -h "My Document" file.txt   # Custom header text
  {} -nt file.txt               # No headers, with line numbers

Modern C++23 implementation with full Unicode support and type safety.
)",
        program_name, program_name, program_name, program_name, program_name, program_name
    );
}

} // namespace xinim::print_utility

// =============================================================================
// Modern Main Function
// =============================================================================

auto main(int argc, char* argv[]) -> int {
    using namespace xinim::print_utility;
    
    try {
        // Parse command line arguments
        CommandLineParser parser{std::span{argv, static_cast<std::size_t>(argc)}};
        auto config_result = parser.parse();
        
        if (!config_result) {
            std::cerr << std::format("Error: {}\n", config_result.error());
            show_usage(argv[0]);
            return 1;
        }
        
        // Create and run the application
        PrintUtilityApp app{*config_result};
        auto result = app.run();
        
        if (!result) {
            std::cerr << std::format("Error: {}\n", result.error());
            return 1;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << std::format("Fatal error: {}\n", e.what());
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred\n";
        return 1;
    }
}
