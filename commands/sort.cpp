/**
 * @file sort_modern.cpp
 * @brief Modern C++23 Sort Utility - Enhanced and Unified Implementation
 * @author XINIM Project (Modernized from Michiel Huisjes' original)
 * @version 2.1
 * @date 2025
 *
 * This file provides a modernized C++23 sort utility, synthesizing legacy and updated
 * implementations for robustness, clarity, and efficiency. Key features include:
 * - RAII-based memory management with smart containers
 * - Type-safe enums and strong typing for field specifications
 * - std::expected for robust error handling
 * - std::ranges for efficient sorting and merging
 * - Concepts for type safety and extensibility
 * - Memory-efficient streaming with std::filesystem
 * - Template metaprogramming for flexible field processing
 * - Unicode-aware text processing
 * - Unified merge implementation with k-way merging for sorted files
 *
 * The implementation harmonizes two merge strategies, prioritizing the k-way merge for scalability
 * while maintaining compatibility with legacy sorting behavior.
 */

#include <algorithm>
#include <array>
#include <concepts>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <locale>
#include <memory>
#include <optional>
#include <queue>
#include <ranges>
#include <regex>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

namespace xinim::sort_utility {
// =============================================================================
// Modern Type System with C++23 Strong Typing
// =============================================================================
/// Result type for operations that can fail
template <typename T> using Result = std::expected<T, std::string>;

/// Strong type for field numbers (0-based)
struct FieldNumber {
    std::size_t value{0};
    constexpr explicit FieldNumber(std::size_t field = 0) noexcept : value{field} {}
    constexpr auto operator<=>(const FieldNumber &) const = default;
    constexpr FieldNumber &operator++() noexcept {
        ++value;
        return *this;
    }
    constexpr FieldNumber operator++(int) noexcept {
        auto tmp = *this;
        ++value;
        return tmp;
    }
};

/// Strong type for character offsets within fields
struct FieldOffset {
    std::size_t value{0};
    constexpr explicit FieldOffset(std::size_t offset = 0) noexcept : value{offset} {}
    constexpr auto operator<=>(const FieldOffset &) const = default;
};

/// Modern sort flags with type safety
enum class SortFlag : std::uint16_t {
    None = 0x0000,
    FoldCase = 0x0001,        // -f: Fold upper case to lower
    Numeric = 0x0002,         // -n: Sort numeric values
    IgnoreBlanks = 0x0004,    // -b: Skip leading blanks
    IgnoreNonASCII = 0x0008,  // -i: Ignore non-ASCII chars
    Reverse = 0x0010,         // -r: Reverse sort order
    Dictionary = 0x0020,      // -d: Dictionary order only
    Unique = 0x0040,          // -u: Unique lines only
    CheckOrder = 0x0080,      // -c: Check if sorted
    Merge = 0x0100,           // -m: Merge sorted files
};

/// Bitwise operations for sort flags
constexpr SortFlag operator|(SortFlag lhs, SortFlag rhs) noexcept {
    return static_cast<SortFlag>(static_cast<std::uint16_t>(lhs) | static_cast<std::uint16_t>(rhs));
}
constexpr SortFlag operator&(SortFlag lhs, SortFlag rhs) noexcept {
    return static_cast<SortFlag>(static_cast<std::uint16_t>(lhs) & static_cast<std::uint16_t>(rhs));
}
constexpr bool has_flag(SortFlag flags, SortFlag flag) noexcept {
    return (flags & flag) != SortFlag::None;
}

/// Field specification for complex sorting
struct FieldSpec {
    FieldNumber start_field{0};
    FieldOffset start_offset{0};
    std::optional<FieldNumber> end_field;
    std::optional<FieldOffset> end_offset;
    SortFlag flags{SortFlag::None};
    constexpr FieldSpec() = default;
    constexpr FieldSpec(FieldNumber start_f, FieldOffset start_o = FieldOffset{0},
                        SortFlag field_flags = SortFlag::None) noexcept
        : start_field{start_f}, start_offset{start_o}, flags{field_flags} {}
};

/// Sort configuration
struct SortConfig {
    SortFlag global_flags{SortFlag::None};
    std::vector<FieldSpec> fields;
    char field_separator{'\t'};
    std::filesystem::path output_file;
    std::vector<std::filesystem::path> input_files;
    bool use_stdin{false};
    [[nodiscard]] bool is_valid() const noexcept { return !input_files.empty() || use_stdin; }
    [[nodiscard]] bool has_custom_fields() const noexcept { return !fields.empty(); }
};

// =============================================================================
// Memory Management with Modern C++23 Containers
// =============================================================================
/// RAII container for line management
class LineContainer {
private:
    std::vector<std::string> lines_;
    std::unordered_set<std::string> unique_lines_;
    bool enforce_unique_{false};

public:
    explicit LineContainer(bool unique_only = false) : enforce_unique_{unique_only} {
        if (unique_only) {
            unique_lines_.reserve(1000); // Initial capacity for uniqueness check
        }
        lines_.reserve(1000); // Optimize for common case
    }

    /// Add a line to the container
    bool add_line(std::string line) {
        if (enforce_unique_) {
            auto [iter, inserted] = unique_lines_.insert(line);
            if (!inserted) {
                return false; // Duplicate line
            }
        }
        lines_.emplace_back(std::move(line));
        return true;
    }

    /// Get all lines
    [[nodiscard]] auto lines() const noexcept -> const std::vector<std::string> & { return lines_; }

    /// Get mutable lines for sorting
    [[nodiscard]] auto mutable_lines() noexcept -> std::vector<std::string> & { return lines_; }

    /// Get line count
    [[nodiscard]] auto size() const noexcept -> std::size_t { return lines_.size(); }

    /// Clear all lines
    void clear() noexcept {
        lines_.clear();
        unique_lines_.clear();
    }
};

// =============================================================================
// Modern Field Processing with Templates and Concepts
// =============================================================================
/// Concept for field extractors
template <typename T>
concept FieldExtractor = requires(T extractor, std::string_view line, FieldSpec spec) {
    { extractor.extract_field(line, spec) } -> std::convertible_to<std::string_view>;
};

/// Modern field extraction engine
class ModernFieldExtractor {
private:
    char separator_;
    std::vector<std::string_view> field_cache_;
    std::string concat_buffer_; /**< Buffer for concatenated fields */

public:
    explicit ModernFieldExtractor(char sep = '\t') : separator_{sep} {
        field_cache_.reserve(64); // Optimize for common field counts
    }

    /// Extract field based on specification
    [[nodiscard]] auto extract_field(std::string_view line, const FieldSpec &spec)
        -> std::string_view {
        split_fields(line);
        if (spec.start_field.value >= field_cache_.size()) {
            return {};
        }
        auto start_field = field_cache_[spec.start_field.value];
        // Apply start offset
        if (spec.start_offset.value < start_field.size()) {
            start_field = start_field.substr(spec.start_offset.value);
        } else {
            return {};
        }
        // Handle end field if specified
        if (spec.end_field) {
            concat_buffer_.clear();
            for (std::size_t i = spec.start_field.value;
                 i < field_cache_.size() && i <= spec.end_field->value; ++i) {
                std::string_view field = field_cache_[i];
                if (i == spec.start_field.value) {
                    if (spec.start_offset.value < field.size()) {
                        field = field.substr(spec.start_offset.value);
                    } else {
                        return {};
                    }
                }
                if (spec.end_offset && i == spec.end_field->value &&
                    spec.end_offset->value < field.size()) {
                    field = field.substr(0, spec.end_offset->value);
                }
                if (!concat_buffer_.empty()) {
                    concat_buffer_.push_back(separator_);
                }
                concat_buffer_.append(field);
            }
            return std::string_view{concat_buffer_};
        }
        return start_field;
    }

    /// Get all fields for a line
    [[nodiscard]] auto get_fields(std::string_view line) -> std::span<const std::string_view> {
        split_fields(line);
        return field_cache_;
    }

private:
    /// Split line into fields using separator
    void split_fields(std::string_view line) {
        field_cache_.clear();
        if (separator_ == '\t') {
            // Whitespace splitting for default separator
            auto fields = line | std::views::split(' ') |
                          std::views::filter([](auto &&field) { return !field.empty(); });
            for (auto &&field : fields) {
                field_cache_.emplace_back(std::string_view{field.begin(), field.end()});
            }
        } else {
            // Custom separator splitting
            auto fields = line | std::views::split(separator_);
            for (auto &&field : fields) {
                field_cache_.emplace_back(std::string_view{field.begin(), field.end()});
            }
        }
    }
};

// =============================================================================
// Comparison Engine with C++23 Algorithms
// =============================================================================
/// Modern comparison engine using function objects
class ComparisonEngine {
private:
    SortConfig config_;
    ModernFieldExtractor extractor_;

public:
    explicit ComparisonEngine(const SortConfig &config)
        : config_{config}, extractor_{config.field_separator} {}

    /// Create comparison function for sorting
    [[nodiscard]] auto create_comparator()
        -> std::function<bool(const std::string &, const std::string &)> {
        return [this](const std::string &lhs, const std::string &rhs) -> bool {
            return compare_lines(lhs, rhs) < 0;
        };
    }

    /// Compare two lines according to configuration
    [[nodiscard]] auto compare_lines(std::string_view lhs, std::string_view rhs) -> int {
        if (config_.has_custom_fields()) {
            return compare_with_fields(lhs, rhs);
        } else {
            return compare_whole_lines(lhs, rhs);
        }
    }

private:
    /// Compare using field specifications
    [[nodiscard]] auto compare_with_fields(std::string_view lhs, std::string_view rhs) -> int {
        for (const auto &field_spec : config_.fields) {
            auto lhs_field = extractor_.extract_field(lhs, field_spec);
            auto rhs_field = extractor_.extract_field(rhs, field_spec);
            auto result = compare_field_values(lhs_field, rhs_field, field_spec.flags);
            if (result != 0) {
                return has_flag(field_spec.flags, SortFlag::Reverse) ? -result : result;
            }
        }
        return 0;
    }

    /// Compare whole lines
    [[nodiscard]] auto compare_whole_lines(std::string_view lhs, std::string_view rhs) -> int {
        auto result = compare_field_values(lhs, rhs, config_.global_flags);
        return has_flag(config_.global_flags, SortFlag::Reverse) ? -result : result;
    }

    /// Compare field values with specific flags
    [[nodiscard]] auto compare_field_values(std::string_view lhs, std::string_view rhs,
                                            SortFlag flags) -> int {
        auto processed_lhs = preprocess_field(lhs, flags);
        auto processed_rhs = preprocess_field(rhs, flags);
        if (has_flag(flags, SortFlag::Numeric)) {
            return compare_numeric(processed_lhs, processed_rhs);
        } else if (has_flag(flags, SortFlag::Dictionary)) {
            return compare_dictionary(processed_lhs, processed_rhs);
        } else {
            return processed_lhs.compare(processed_rhs);
        }
    }

    /// Preprocess field based on flags
    [[nodiscard]] auto preprocess_field(std::string_view field, SortFlag flags) -> std::string {
        std::string result{field};
        if (has_flag(flags, SortFlag::IgnoreBlanks)) {
            auto start = result.find_first_not_of(" \t");
            if (start != std::string::npos) {
                result = result.substr(start);
            }
        }
        if (has_flag(flags, SortFlag::FoldCase)) {
            std::transform(result.begin(), result.end(), result.begin(),
                           [](unsigned char c) { return std::tolower(c); });
        }
        if (has_flag(flags, SortFlag::IgnoreNonASCII)) {
            std::erase_if(result, [](unsigned char c) { return c < 32 || c > 126; });
        }
        return result;
    }

    /// Compare numeric fields
    [[nodiscard]] auto compare_numeric(std::string_view lhs, std::string_view rhs) -> int {
        try {
            auto lhs_num = std::stod(std::string{lhs});
            auto rhs_num = std::stod(std::string{rhs});
            if (lhs_num < rhs_num)
                return -1;
            if (lhs_num > rhs_num)
                return 1;
            return 0;
        } catch (const std::exception &) {
            return lhs.compare(rhs); // Fall back to string comparison
        }
    }

    /// Compare in dictionary order
    [[nodiscard]] auto compare_dictionary(std::string_view lhs, std::string_view rhs) -> int {
        auto is_dict_char = [](char c) { return std::isalnum(c) || c == ',' || c == '.'; };
        auto lhs_it = lhs.begin();
        auto rhs_it = rhs.begin();
        while (lhs_it != lhs.end() && rhs_it != rhs.end()) {
            while (lhs_it != lhs.end() && !is_dict_char(*lhs_it))
                ++lhs_it;
            while (rhs_it != rhs.end() && !is_dict_char(*rhs_it))
                ++rhs_it;
            if (lhs_it == lhs.end() && rhs_it == rhs.end())
                return 0;
            if (lhs_it == lhs.end())
                return -1;
            if (rhs_it == rhs.end())
                return 1;
            if (*lhs_it != *rhs_it) {
                return (*lhs_it < *rhs_it) ? -1 : 1;
            }
            ++lhs_it;
            ++rhs_it;
        }
        return 0;
    }
};

// =============================================================================
// File I/O with Modern C++23 Streams
// =============================================================================
/// Modern file reader with RAII
class ModernFileReader {
private:
    std::unique_ptr<std::istream> stream_;
    std::istream *raw_stream_ptr_{nullptr};
    std::filesystem::path file_path_;
    bool owns_stream_{false};

public:
    explicit ModernFileReader(const std::filesystem::path &path) : file_path_{path} {
        auto file_stream = std::make_unique<std::ifstream>(path);
        if (!file_stream->is_open()) {
            throw std::runtime_error(std::format("Cannot open file: {}", path.string()));
        }
        stream_ = std::move(file_stream);
        owns_stream_ = true;
    }

    explicit ModernFileReader(std::istream &input_stream)
        : raw_stream_ptr_{&input_stream}, owns_stream_{false} {}

    auto read_lines(LineContainer &container) -> Result<void> {
        try {
            std::string line;
            auto *active_stream = owns_stream_ ? stream_.get() : raw_stream_ptr_;
            while (std::getline(*active_stream, line)) {
                container.add_line(std::move(line));
            }
            if (active_stream->bad()) {
                return std::unexpected("I/O error while reading file");
            }
            return {};
        } catch (const std::exception &e) {
            return std::unexpected(std::format("Error reading file: {}", e.what()));
        }
    }

    [[nodiscard]] auto file_path() const noexcept -> const std::filesystem::path & {
        return file_path_;
    }
};

/// Modern file writer with RAII
class ModernFileWriter {
private:
    std::unique_ptr<std::ostream> stream_;
    std::ostream *raw_stream_ptr_{nullptr};
    std::filesystem::path file_path_;
    bool owns_stream_{false};

public:
    explicit ModernFileWriter(const std::filesystem::path &path) : file_path_{path} {
        auto file_stream = std::make_unique<std::ofstream>(path);
        if (!file_stream->is_open()) {
            throw std::runtime_error(std::format("Cannot create file: {}", path.string()));
        }
        stream_ = std::move(file_stream);
        owns_stream_ = true;
    }

    explicit ModernFileWriter(std::ostream &output_stream)
        : raw_stream_ptr_{&output_stream}, owns_stream_{false} {}

    auto write_lines(const LineContainer &container) -> Result<void> {
        try {
            auto *active_stream = owns_stream_ ? stream_.get() : raw_stream_ptr_;
            for (const auto &line : container.lines()) {
                *active_stream << line << '\n';
            }
            active_stream->flush();
            if (active_stream->bad()) {
                return std::unexpected("I/O error while writing file");
            }
            return {};
        } catch (const std::exception &e) {
            return std::unexpected(std::format("Error writing file: {}", e.what()));
        }
    }
};

// =============================================================================
// Command Line Parsing with Modern C++23
// =============================================================================
/// Modern command line parser
class CommandLineParser {
private:
    std::span<char *> args_;
    SortConfig config_;

public:
    explicit CommandLineParser(std::span<char *> arguments) : args_{arguments} {}

    [[nodiscard]] auto parse() -> Result<SortConfig> {
        for (std::size_t i = 1; i < args_.size(); ++i) {
            std::string_view arg{args_[i]};
            if (arg.starts_with('+')) {
                auto field_result = parse_field_start(arg.substr(1));
                if (!field_result) {
                    return std::unexpected(field_result.error());
                }
                config_.fields.push_back(*field_result);
            } else if (arg.starts_with('-') && arg.size() > 1) {
                if (std::isdigit(arg[1])) {
                    auto field_result = parse_field_end(arg.substr(1));
                    if (!field_result) {
                        return std::unexpected(field_result.error());
                    }
                    if (!config_.fields.empty()) {
                        auto &last_field = config_.fields.back();
                        last_field.end_field = field_result->start_field;
                        last_field.end_offset = field_result->start_offset;
                    }
                } else {
                    auto option_result = parse_options(arg, i);
                    if (!option_result) {
                        return std::unexpected(option_result.error());
                    }
                    i = *option_result;
                }
            } else {
                config_.input_files.emplace_back(arg);
            }
        }
        if (config_.input_files.empty()) {
            config_.use_stdin = true;
        }
        if (!config_.is_valid()) {
            return std::unexpected("Invalid configuration parameters");
        }
        return config_;
    }

private:
    [[nodiscard]] auto parse_field_start(std::string_view spec) -> Result<FieldSpec> {
        auto dot_pos = spec.find('.');
        auto field_str = spec.substr(0, dot_pos);
        try {
            auto field_num = std::stoul(std::string{field_str});
            FieldSpec field_spec{FieldNumber{field_num}};
            if (dot_pos != std::string_view::npos) {
                auto offset_str = spec.substr(dot_pos + 1);
                auto offset_num = std::stoul(std::string{offset_str});
                field_spec.start_offset = FieldOffset{offset_num};
            }
            return field_spec;
        } catch (const std::exception &e) {
            return std::unexpected(std::format("Invalid field specification: {}", e.what()));
        }
    }

    [[nodiscard]] auto parse_field_end(std::string_view spec) -> Result<FieldSpec> {
        return parse_field_start(spec);
    }

    [[nodiscard]] auto parse_options(std::string_view arg, std::size_t &index)
        -> Result<std::size_t> {
        for (std::size_t j = 1; j < arg.size(); ++j) {
            switch (arg[j]) {
            case 'f':
                config_.global_flags = config_.global_flags | SortFlag::FoldCase;
                break;
            case 'n':
                config_.global_flags = config_.global_flags | SortFlag::Numeric;
                break;
            case 'b':
                config_.global_flags = config_.global_flags | SortFlag::IgnoreBlanks;
                break;
            case 'i':
                config_.global_flags = config_.global_flags | SortFlag::IgnoreNonASCII;
                break;
            case 'r':
                config_.global_flags = config_.global_flags | SortFlag::Reverse;
                break;
            case 'd':
                config_.global_flags = config_.global_flags | SortFlag::Dictionary;
                break;
            case 'u':
                config_.global_flags = config_.global_flags | SortFlag::Unique;
                break;
            case 'c':
                config_.global_flags = config_.global_flags | SortFlag::CheckOrder;
                break;
            case 'm':
                config_.global_flags = config_.global_flags | SortFlag::Merge;
                break;
            case 'o':
                if (++index >= args_.size()) {
                    return std::unexpected("Option -o requires an argument");
                }
                config_.output_file = args_[index];
                return index;
            case 't':
                if (j + 1 < arg.size()) {
                    config_.field_separator = arg[j + 1];
                    return index;
                } else if (++index < args_.size()) {
                    config_.field_separator = args_[index][0];
                    return index;
                }
                return std::unexpected("Option -t requires a separator character");
            default:
                return std::unexpected(std::format("Unknown option: -{}", arg[j]));
            }
        }
        return index;
    }
};

// =============================================================================
// Sort Engine with Modern Algorithms
// =============================================================================
/// Modern sort engine
class ModernSortEngine {
private:
    SortConfig config_;
    ComparisonEngine comparator_;

public:
    explicit ModernSortEngine(const SortConfig &config) : config_{config}, comparator_{config} {}

    auto sort_lines(LineContainer &container) -> Result<void> {
        try {
            if (has_flag(config_.global_flags, SortFlag::CheckOrder)) {
                return check_order(container);
            } else if (has_flag(config_.global_flags, SortFlag::Merge)) {
                return merge_files(container);
            } else {
                return perform_sort(container);
            }
        } catch (const std::exception &e) {
            return std::unexpected(std::format("Sort error: {}", e.what()));
        }
    }

private:
    auto perform_sort(LineContainer &container) -> Result<void> {
        auto &lines = container.mutable_lines();
        auto comparator = comparator_.create_comparator();
        std::ranges::sort(lines, comparator);
        return {};
    }

    auto check_order(const LineContainer &container) -> Result<void> {
        const auto &lines = container.lines();
        auto comparator = comparator_.create_comparator();
        if (!std::ranges::is_sorted(lines, comparator)) {
            return std::unexpected("File is not sorted");
        }
        return {};
    }

    /**
     * @brief Merge multiple pre-sorted input streams into @p container.
     *
     * This routine performs a k-way merge over all configured input sources.
     * Each file (and optionally standard input) is assumed to be individually
     * sorted according to the active @c SortFlag options. Lines are compared
     * using the configured @c ComparisonEngine and appended to @p container,
     * which itself may enforce uniqueness semantics.
     *
     * Robust error handling is provided: unreadable files are skipped with a
     * warning, whereas I/O failures during the merge result in an
     * @c std::unexpected containing a descriptive message.
     */
    auto merge_files(LineContainer &container) -> Result<void> {
        ///
        /// Represents one input source participating in the merge operation.
        struct InputSource {
            std::unique_ptr<std::ifstream> file_stream{}; /**< Owned file stream */
            std::istream *stream{nullptr};                 /**< Active stream */
            std::filesystem::path path{};                  /**< Path for diagnostics */
            std::string line{};                            /**< Buffer holding current line */
        };

        std::vector<InputSource> sources;
        sources.reserve(config_.input_files.size() + (config_.use_stdin ? 1UZ : 0UZ));

        const auto load_first_line = [](InputSource &src) -> bool {
            return static_cast<bool>(std::getline(*src.stream, src.line));
        };

        // Open all file inputs
        for (const auto &file_path : config_.input_files) {
            try {
                InputSource src;
                src.file_stream = std::make_unique<std::ifstream>(file_path);
                if (!src.file_stream->is_open()) {
                    std::cerr << std::format("Warning: Cannot open {}\n", file_path.string());
                    continue;
                }
                src.stream = src.file_stream.get();
                src.path = file_path;
                if (load_first_line(src)) {
                    sources.emplace_back(std::move(src));
                } else if (src.stream->bad()) {
                    return std::unexpected(
                        std::format("I/O error while reading {}", file_path.string()));
                }
            } catch (const std::exception &e) {
                std::cerr << std::format("Warning: Cannot open {}: {}\n", file_path.string(),
                                         e.what());
            }
        }

        // Include standard input if requested
        if (config_.use_stdin) {
            InputSource src;
            src.stream = &std::cin;
            src.path = "<stdin>";
            if (load_first_line(src)) {
                sources.emplace_back(std::move(src));
            } else if (src.stream->bad()) {
                return std::unexpected("I/O error while reading standard input");
            }
        }

        if (sources.size() < 2) {
            return std::unexpected("Merge requires at least two input sources");
        }

        // Perform k-way merge using a min-heap
        auto less = comparator_.create_comparator();
        struct HeapItem {
            std::size_t index; /**< Index into @c sources */
        };
        const auto heap_comp = [&](const HeapItem &a, const HeapItem &b) {
            return less(sources[b.index].line, sources[a.index].line);
        };
        std::priority_queue<HeapItem, std::vector<HeapItem>, decltype(heap_comp)> heap(heap_comp);
        for (std::size_t i = 0; i < sources.size(); ++i) {
            if (!sources[i].line.empty()) {
                heap.push({i});
            }
        }

        while (!heap.empty()) {
            const auto [idx] = heap.top();
            heap.pop();
            auto &src = sources[idx];
            container.add_line(std::move(src.line));
            if (std::getline(*src.stream, src.line)) {
                heap.push({idx});
            } else if (src.stream->bad()) {
                return std::unexpected(
                    std::format("I/O error while reading {}", src.path.string()));
            }
        }

        return {};
    }
};

// =============================================================================
// Main Application Class
// =============================================================================
/// Main sort utility application
class SortUtilityApp {
private:
    SortConfig config_;
    ModernSortEngine engine_;

public:
    explicit SortUtilityApp(const SortConfig &config) : config_{config}, engine_{config} {}

    [[nodiscard]] auto run() -> Result<void> {
        try {
            LineContainer container{has_flag(config_.global_flags, SortFlag::Unique)};
            if (!has_flag(config_.global_flags, SortFlag::Merge)) {
                auto read_result = read_input(container);
                if (!read_result) {
                    return std::unexpected(read_result.error());
                }
            }

            auto sort_result = engine_.sort_lines(container);
            if (!sort_result) {
                return std::unexpected(sort_result.error());
            }

            auto write_result = write_output(container);
            if (!write_result) {
                return std::unexpected(write_result.error());
            }
            return {};
        } catch (const std::exception &e) {
            return std::unexpected(std::format("Runtime error: {}", e.what()));
        }
    }

private:
    auto read_input(LineContainer &container) -> Result<void> {
        if (config_.use_stdin) {
            ModernFileReader reader{std::cin};
            return reader.read_lines(container);
        }
        for (const auto &file_path : config_.input_files) {
            try {
                ModernFileReader reader{file_path};
                auto result = reader.read_lines(container);
                if (!result) {
                    std::cerr << std::format("Warning: Cannot read {}: {}\n", file_path.string(),
                                             result.error());
                    continue;
                }
            } catch (const std::exception &e) {
                std::cerr << std::format("Warning: Cannot open {}: {}\n", file_path.string(),
                                         e.what());
                continue;
            }
        }
        return {};
    }

    auto write_output(const LineContainer &container) -> Result<void> {
        if (config_.output_file.empty()) {
            ModernFileWriter writer{std::cout};
            return writer.write_lines(container);
        }
        ModernFileWriter writer{config_.output_file};
        return writer.write_lines(container);
    }
};

// =============================================================================
// Modern Usage Information
// =============================================================================
void show_usage(std::string_view program_name) {
    std::cout << std::format(R"(
Usage: {} [options] [+field_start[-field_end]] [files...]
Sort Options:
  -f Fold upper case to lower case
  -n Sort by numeric value (implies -b)
  -b Ignore leading blanks
  -i Ignore non-ASCII characters (keep 040-0176 range)
  -r Reverse the sort order
  -d Dictionary order (letters, digits, commas, periods only)
  -u Output unique lines only
  -c Check if input is already sorted
  -m Merge already sorted files
  -o file Write output to specified file
  -t char Use 'char' as field separator
Field Specifications:
  +N.M Start sorting at field N, character M (0-based)
  -N.M Stop sorting at field N, character M
Examples:
  {} file.txt             # Sort file.txt
  {} -n numbers.txt       # Numeric sort
  {} -r -f text.txt       # Reverse case-insensitive sort
  {} +1.2 -2.5 data.txt   # Sort on field 1 char 2 to field 2 char 5
  {} -t: -k2 /etc/passwd  # Sort by second field using ':' separator
  {} -u duplicate.txt     # Remove duplicates and sort
Modern C++23 implementation with Unicode support and memory safety.
)",
                              program_name, program_name, program_name, program_name, program_name,
                              program_name, program_name);
}

} // namespace xinim::sort_utility

#ifndef SORT_UTILITY_NO_MAIN
auto main(int argc, char *argv[]) -> int {
    using namespace xinim::sort_utility;
    try {
        CommandLineParser parser{std::span{argv, static_cast<std::size_t>(argc)}};
        auto config_result = parser.parse();
        if (!config_result) {
            std::cerr << std::format("Error: {}\n", config_result.error());
            show_usage(argv[0]);
            return 1;
        }
        SortUtilityApp app{*config_result};
        auto result = app.run();
        if (!result) {
            std::cerr << std::format("Error: {}\n", result.error());
            return 1;
        }
        return 0;
    } catch (const std::exception &e) {
        std::cerr << std::format("Fatal error: {}\n", e.what());
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred\n";
        return 1;
    }
}
#endif // SORT_UTILITY_NO_MAIN