/**
 * @file tar_modern.cpp
 * @brief Modern C++23 Tape Archive Utility - Complete Decomposition and Rebuild
 * @author XINIM Project (Modernized from Michiel Huisjes original)
 * @version 2.0
 * @date 2025
 * 
 * Complete modernization using C++23 paradigms:
 * - RAII memory management with smart containers
 * - Type-safe enums and strong typing
 * - std::expected for error handling
 * - std::filesystem for path operations
 * - std::format for output formatting
 * - Concepts for type safety
 * - Ranges and algorithms
 * - Binary data handling with std::span
 * - Modern I/O with std::fstream
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <cstring>
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

namespace xinim::tar_utility {

// =============================================================================
// Modern Type System with C++23 Strong Typing
// =============================================================================

/// Result type for operations that can fail
template<typename T>
using Result = std::expected<T, std::string>;

/// TAR constants with proper scoping
namespace tar_constants {
    constexpr std::size_t header_size = 512;
    constexpr std::size_t name_size = 100;
    constexpr std::size_t mode_size = 8;
    constexpr std::size_t uid_size = 8;
    constexpr std::size_t gid_size = 8;
    constexpr std::size_t size_size = 12;
    constexpr std::size_t time_size = 12;
    constexpr std::size_t checksum_size = 8;
    constexpr std::size_t link_name_size = 100;
    // Note: block_boundary = 20 available if needed for future enhancements
}

/// TAR operation modes
enum class TarOperation {
    Create,   // c: Create archive
    Extract,  // x: Extract files
    List      // t: List contents
};

/// TAR file types
enum class TarFileType : char {
    RegularFile = '0',
    HardLink = '1',
    SymbolicLink = '2',
    CharDevice = '3',
    BlockDevice = '4',
    Directory = '5',
    FIFO = '6',
    Reserved = '7'
};

/// Strong type for file permissions
struct FilePermissions {
    std::uint32_t mode{0644};
    
    constexpr explicit FilePermissions(std::uint32_t perm = 0644) noexcept : mode{perm} {}
    constexpr auto operator<=>(const FilePermissions&) const = default;
    
    [[nodiscard]] constexpr std::string to_octal_string() const {
        return std::format("{:o}", mode);
    }
};

/// Strong type for user/group IDs
struct UserId {
    std::uint32_t id{0};
    
    constexpr explicit UserId(std::uint32_t user_id = 0) noexcept : id{user_id} {}
    constexpr auto operator<=>(const UserId&) const = default;
    
    [[nodiscard]] constexpr std::string to_string() const {
        return std::format("{}", id);
    }
};

struct GroupId {
    std::uint32_t id{0};
    
    constexpr explicit GroupId(std::uint32_t group_id = 0) noexcept : id{group_id} {}
    constexpr auto operator<=>(const GroupId&) const = default;
    
    [[nodiscard]] constexpr std::string to_string() const {
        return std::format("{}", id);
    }
};

/// Strong type for file sizes
struct FileSize {
    std::uint64_t bytes{0};
    
    constexpr explicit FileSize(std::uint64_t size = 0) noexcept : bytes{size} {}
    constexpr auto operator<=>(const FileSize&) const = default;
    
    [[nodiscard]] constexpr std::string to_octal_string() const {
        return std::format("{:o}", bytes);
    }
};

// =============================================================================
// Modern TAR Header with Type Safety
// =============================================================================

/// Modern TAR header structure
class TarHeader {
private:
    std::array<char, tar_constants::header_size> raw_header_{};
    
public:
    TarHeader() {
        raw_header_.fill('\0');
    }
    
    /// Set file name (truncated to fit)
    void set_name(std::string_view name) {
        auto copy_size = std::min(name.size(), tar_constants::name_size - 1);
        std::copy_n(name.begin(), copy_size, raw_header_.begin());
    }
    
    /// Set file mode
    void set_mode(FilePermissions perms) {
        auto mode_str = std::format("{:07o}", perms.mode);
        set_field(tar_constants::name_size, mode_str, tar_constants::mode_size);
    }
    
    /// Set user ID
    void set_uid(UserId uid) {
        auto uid_str = std::format("{:07o}", uid.id);
        set_field(tar_constants::name_size + tar_constants::mode_size, uid_str, tar_constants::uid_size);
    }
    
    /// Set group ID
    void set_gid(GroupId gid) {
        auto gid_str = std::format("{:07o}", gid.id);
        set_field(tar_constants::name_size + tar_constants::mode_size + tar_constants::uid_size, 
                 gid_str, tar_constants::gid_size);
    }
    
    /// Set file size
    void set_size(FileSize size) {
        auto size_str = std::format("{:011o}", size.bytes);
        set_field(tar_constants::name_size + tar_constants::mode_size + tar_constants::uid_size + tar_constants::gid_size,
                 size_str, tar_constants::size_size);
    }
    
    /// Set modification time
    void set_mtime(std::chrono::system_clock::time_point time) {
        auto time_t_val = std::chrono::system_clock::to_time_t(time);
        auto time_str = std::format("{:011o}", time_t_val);
        set_field(tar_constants::name_size + tar_constants::mode_size + tar_constants::uid_size + 
                 tar_constants::gid_size + tar_constants::size_size,
                 time_str, tar_constants::time_size);
    }
    
    /// Set file type
    void set_typeflag(TarFileType type) {
        auto offset = tar_constants::name_size + tar_constants::mode_size + tar_constants::uid_size + 
                     tar_constants::gid_size + tar_constants::size_size + tar_constants::time_size + 
                     tar_constants::checksum_size;
        raw_header_[offset] = static_cast<char>(type);
    }
    
    /// Set link name
    void set_linkname(std::string_view linkname) {
        auto offset = tar_constants::name_size + tar_constants::mode_size + tar_constants::uid_size + 
                     tar_constants::gid_size + tar_constants::size_size + tar_constants::time_size + 
                     tar_constants::checksum_size + 1;
        auto copy_size = std::min(linkname.size(), tar_constants::link_name_size - 1);
        std::copy_n(linkname.begin(), copy_size, raw_header_.begin() + offset);
    }
    
    /// Calculate and set checksum
    void calculate_checksum() {
        // Clear checksum field first
        auto checksum_offset = tar_constants::name_size + tar_constants::mode_size + tar_constants::uid_size + 
                              tar_constants::gid_size + tar_constants::size_size + tar_constants::time_size;
        std::fill_n(raw_header_.begin() + checksum_offset, tar_constants::checksum_size, ' ');
        
        // Calculate checksum
        std::uint32_t sum = 0;
        for (const auto& byte : raw_header_) {
            sum += static_cast<unsigned char>(byte);
        }
        
        // Set checksum field
        auto checksum_str = std::format("{:06o}\\0 ", sum);
        set_field(checksum_offset, checksum_str, tar_constants::checksum_size);
    }
    
    /// Get raw header data
    [[nodiscard]] auto data() const noexcept -> std::span<const char> {
        return raw_header_;
    }
    
    /// Get mutable raw header data
    [[nodiscard]] auto data() noexcept -> std::span<char> {
        return raw_header_;
    }
    
    // Getters for reading existing headers
    [[nodiscard]] auto get_name() const -> std::string {
        return extract_string(0, tar_constants::name_size);
    }
    
    [[nodiscard]] auto get_size() const -> FileSize {
        auto size_str = extract_string(tar_constants::name_size + tar_constants::mode_size + 
                                      tar_constants::uid_size + tar_constants::gid_size,
                                      tar_constants::size_size);
        try {
            return FileSize{std::stoull(size_str, nullptr, 8)};
        } catch (const std::exception&) {
            return FileSize{0};
        }
    }
    
    [[nodiscard]] auto get_typeflag() const -> TarFileType {
        auto offset = tar_constants::name_size + tar_constants::mode_size + tar_constants::uid_size + 
                     tar_constants::gid_size + tar_constants::size_size + tar_constants::time_size + 
                     tar_constants::checksum_size;
        return static_cast<TarFileType>(raw_header_[offset]);
    }
    
    /// Check if header is valid (non-zero)
    [[nodiscard]] bool is_valid() const noexcept {
        return std::any_of(raw_header_.begin(), raw_header_.end(), 
                          [](char c) { return c != '\0'; });
    }
    
private:
    /// Set field in header with proper null termination
    void set_field(std::size_t offset, std::string_view value, std::size_t field_size) {
        auto copy_size = std::min(value.size(), field_size - 1);
        std::copy_n(value.begin(), copy_size, raw_header_.begin() + offset);
        raw_header_[offset + copy_size] = '\0';
    }
    
    /// Extract string from header field
    [[nodiscard]] auto extract_string(std::size_t offset, std::size_t max_size) const -> std::string {
        auto start = raw_header_.begin() + offset;
        auto end = std::find(start, start + max_size, '\0');
        return std::string{start, end};
    }
};

// =============================================================================
// File System Abstraction with std::filesystem
// =============================================================================

/// Modern file entry with metadata
struct FileEntry {
    std::filesystem::path path;
    FileSize size{0};
    FilePermissions permissions{0644};
    UserId uid{0};
    GroupId gid{0};
    std::chrono::system_clock::time_point mtime;
    TarFileType file_type{TarFileType::RegularFile};
    std::optional<std::filesystem::path> link_target;
    
    FileEntry() = default;
    
    explicit FileEntry(const std::filesystem::path& file_path) : path{file_path} {
        if (std::filesystem::exists(file_path)) {
            populate_from_filesystem();
        }
    }
    
private:
    void populate_from_filesystem() {
        try {
            auto status = std::filesystem::status(path);
            auto file_time = std::filesystem::last_write_time(path);
            
            // Convert file time to system_clock
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                file_time - std::filesystem::file_time_type::clock::now() + 
                std::chrono::system_clock::now()
            );
            mtime = sctp;
            
            // Set file type
            switch (status.type()) {
                case std::filesystem::file_type::regular:
                    file_type = TarFileType::RegularFile;
                    size = FileSize{static_cast<std::uint64_t>(std::filesystem::file_size(path))};
                    break;
                case std::filesystem::file_type::directory:
                    file_type = TarFileType::Directory;
                    break;
                case std::filesystem::file_type::symlink:
                    file_type = TarFileType::SymbolicLink;
                    link_target = std::filesystem::read_symlink(path);
                    break;
                default:
                    file_type = TarFileType::RegularFile;
                    break;
            }
            
            // Get permissions (simplified for cross-platform compatibility)
            auto perms = status.permissions();
            std::uint32_t mode = 0;
            if ((perms & std::filesystem::perms::owner_read) != std::filesystem::perms::none) mode |= 0400;
            if ((perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none) mode |= 0200;
            if ((perms & std::filesystem::perms::owner_exec) != std::filesystem::perms::none) mode |= 0100;
            if ((perms & std::filesystem::perms::group_read) != std::filesystem::perms::none) mode |= 0040;
            if ((perms & std::filesystem::perms::group_write) != std::filesystem::perms::none) mode |= 0020;
            if ((perms & std::filesystem::perms::group_exec) != std::filesystem::perms::none) mode |= 0010;
            if ((perms & std::filesystem::perms::others_read) != std::filesystem::perms::none) mode |= 0004;
            if ((perms & std::filesystem::perms::others_write) != std::filesystem::perms::none) mode |= 0002;
            if ((perms & std::filesystem::perms::others_exec) != std::filesystem::perms::none) mode |= 0001;
            
            permissions = FilePermissions{mode};
            
        } catch (const std::exception&) {
            // Use defaults if filesystem operations fail
        }
    }
};

// =============================================================================
// TAR Archive Processing with Modern I/O
// =============================================================================

/// Modern TAR archive processor
class TarArchive {
private:
    std::filesystem::path archive_path_;
    bool verbose_{false};
    
public:
    explicit TarArchive(const std::filesystem::path& path, bool verbose = false) 
        : archive_path_{path}, verbose_{verbose} {}
    
    /// Create archive from files
    auto create_archive(std::span<const std::filesystem::path> files) -> Result<void> {
        try {
            std::ofstream archive_stream{archive_path_, std::ios::binary};
            if (!archive_stream.is_open()) {
                return std::unexpected(std::format("Cannot create archive: {}", archive_path_.string()));
            }
            
            for (const auto& file_path : files) {
                auto result = add_file_to_archive(archive_stream, file_path);
                if (!result) {
                    std::cerr << std::format("Warning: Cannot add {}: {}\\n", 
                                           file_path.string(), result.error());
                    continue;
                }
            }
            
            // Write end-of-archive markers (two zero blocks)
            std::array<char, tar_constants::header_size> zero_block{};
            archive_stream.write(zero_block.data(), zero_block.size());
            archive_stream.write(zero_block.data(), zero_block.size());
            
            return {};
        } catch (const std::exception& e) {
            return std::unexpected(std::format("Archive creation error: {}", e.what()));
        }
    }
    
    /// Extract files from archive
    auto extract_archive(const std::optional<std::vector<std::string>>& file_filter = std::nullopt) -> Result<void> {
        try {
            std::ifstream archive_stream{archive_path_, std::ios::binary};
            if (!archive_stream.is_open()) {
                return std::unexpected(std::format("Cannot open archive: {}", archive_path_.string()));
            }
            
            TarHeader header;
            while (archive_stream.read(header.data().data(), tar_constants::header_size)) {
                if (!header.is_valid()) {
                    break; // End of archive
                }
                
                auto file_name = header.get_name();
                auto file_size = header.get_size();
                // Note: file_type available if needed: auto file_type = header.get_typeflag();
                
                // Check if file matches filter
                if (file_filter && !matches_filter(*file_filter, file_name)) {
                    skip_file_data(archive_stream, file_size);
                    continue;
                }
                
                if (verbose_) {
                    std::cout << std::format("Extracting: {}\\n", file_name);
                }
                
                auto result = extract_file(archive_stream, header);
                if (!result) {
                    std::cerr << std::format("Warning: Cannot extract {}: {}\\n", 
                                           file_name, result.error());
                    skip_file_data(archive_stream, file_size);
                    continue;
                }
            }
            
            return {};
        } catch (const std::exception& e) {
            return std::unexpected(std::format("Archive extraction error: {}", e.what()));
        }
    }
    
    /// List archive contents
    auto list_archive() -> Result<void> {
        try {
            std::ifstream archive_stream{archive_path_, std::ios::binary};
            if (!archive_stream.is_open()) {
                return std::unexpected(std::format("Cannot open archive: {}", archive_path_.string()));
            }
            
            TarHeader header;
            while (archive_stream.read(header.data().data(), tar_constants::header_size)) {
                if (!header.is_valid()) {
                    break; // End of archive
                }
                
                auto file_name = header.get_name();
                auto file_size = header.get_size();
                
                std::cout << std::format("{}\\t{}\\n", file_name, file_size.bytes);
                
                // Skip file data
                skip_file_data(archive_stream, file_size);
            }
            
            return {};
        } catch (const std::exception& e) {
            return std::unexpected(std::format("Archive listing error: {}", e.what()));
        }
    }
    
private:
    /// Add single file to archive
    auto add_file_to_archive(std::ofstream& archive_stream, const std::filesystem::path& file_path) -> Result<void> {
        FileEntry entry{file_path};
        
        // Create header
        TarHeader header;
        header.set_name(file_path.string());
        header.set_mode(entry.permissions);
        header.set_uid(entry.uid);
        header.set_gid(entry.gid);
        header.set_size(entry.size);
        header.set_mtime(entry.mtime);
        header.set_typeflag(entry.file_type);
        
        if (entry.link_target) {
            header.set_linkname(entry.link_target->string());
        }
        
        header.calculate_checksum();
        
        // Write header
        archive_stream.write(header.data().data(), tar_constants::header_size);
        
        if (verbose_) {
            std::cout << std::format("Adding: {}\\n", file_path.string());
        }
        
        // Write file content for regular files
        if (entry.file_type == TarFileType::RegularFile) {
            auto result = write_file_content(archive_stream, file_path, entry.size);
            if (!result) {
                return result;
            }
        }
        
        return {};
    }
    
    /// Write file content to archive with padding
    auto write_file_content(std::ofstream& archive_stream, const std::filesystem::path& file_path, FileSize size) -> Result<void> {
        try {
            std::ifstream file_stream{file_path, std::ios::binary};
            if (!file_stream.is_open()) {
                return std::unexpected(std::format("Cannot read file: {}", file_path.string()));
            }
            
            // Copy file data
            std::array<char, 4096> buffer;
            std::uint64_t remaining = size.bytes;
            
            while (remaining > 0 && file_stream.good()) {
                auto to_read = std::min(remaining, static_cast<std::uint64_t>(buffer.size()));
                file_stream.read(buffer.data(), to_read);
                auto actually_read = file_stream.gcount();
                
                archive_stream.write(buffer.data(), actually_read);
                remaining -= actually_read;
            }
            
            // Pad to 512-byte boundary
            auto padding = (tar_constants::header_size - (size.bytes % tar_constants::header_size)) % tar_constants::header_size;
            if (padding > 0) {
                std::array<char, tar_constants::header_size> pad_buffer{};
                archive_stream.write(pad_buffer.data(), padding);
            }
            
            return {};
        } catch (const std::exception& e) {
            return std::unexpected(std::format("File write error: {}", e.what()));
        }
    }
    
    /// Extract single file from archive
    auto extract_file(std::ifstream& archive_stream, const TarHeader& header) -> Result<void> {
        auto file_name = header.get_name();
        auto file_size = header.get_size();
        auto file_type = header.get_typeflag();
        
        try {
            if (file_type == TarFileType::Directory) {
                std::filesystem::create_directories(file_name);
                return {};
            }
            
            if (file_type == TarFileType::RegularFile) {
                // Create parent directories
                auto parent_path = std::filesystem::path{file_name}.parent_path();
                if (!parent_path.empty()) {
                    std::filesystem::create_directories(parent_path);
                }
                
                // Extract file content
                std::ofstream output_file{file_name, std::ios::binary};
                if (!output_file.is_open()) {
                    return std::unexpected(std::format("Cannot create file: {}", file_name));
                }
                
                std::array<char, 4096> buffer;
                std::uint64_t remaining = file_size.bytes;
                
                while (remaining > 0 && archive_stream.good()) {
                    auto to_read = std::min(remaining, static_cast<std::uint64_t>(buffer.size()));
                    archive_stream.read(buffer.data(), to_read);
                    auto actually_read = archive_stream.gcount();
                    
                    output_file.write(buffer.data(), actually_read);
                    remaining -= actually_read;
                }
                
                // Skip padding
                auto padding = (tar_constants::header_size - (file_size.bytes % tar_constants::header_size)) % tar_constants::header_size;
                if (padding > 0) {
                    archive_stream.seekg(padding, std::ios::cur);
                }
            }
            
            return {};
        } catch (const std::exception& e) {
            return std::unexpected(std::format("File extraction error: {}", e.what()));
        }
    }
    
    /// Skip file data in archive
    void skip_file_data(std::ifstream& archive_stream, FileSize size) {
        auto total_size = size.bytes;
        auto padding = (tar_constants::header_size - (total_size % tar_constants::header_size)) % tar_constants::header_size;
        archive_stream.seekg(total_size + padding, std::ios::cur);
    }
    
    /// Check if filename matches filter
    [[nodiscard]] bool matches_filter(const std::vector<std::string>& filter, std::string_view filename) const {
        return std::any_of(filter.begin(), filter.end(),
                          [filename](const std::string& pattern) {
                              return filename == pattern;
                          });
    }
};

// =============================================================================
// Command Line Processing
// =============================================================================

/// TAR configuration
struct TarConfig {
    TarOperation operation{TarOperation::List};
    std::filesystem::path archive_file;
    std::vector<std::filesystem::path> files;
    bool verbose{false};
    
    [[nodiscard]] bool is_valid() const noexcept {
        return !archive_file.empty();
    }
};

/// Command line parser for TAR utility
class TarCommandLineParser {
private:
    std::span<char*> args_;
    TarConfig config_;
    
public:
    explicit TarCommandLineParser(std::span<char*> arguments) : args_{arguments} {}
    
    /// Parse command line arguments
    [[nodiscard]] auto parse() -> Result<TarConfig> {
        if (args_.size() < 3) {
            return std::unexpected("Usage: tar [cxt][v] archive_file [files...]");
        }
        
        // Parse operation flags
        std::string_view flags{args_[1]};
        auto operation_result = parse_operation_flags(flags);
        if (!operation_result) {
            return std::unexpected(operation_result.error());
        }
        
        // Archive file
        config_.archive_file = args_[2];
        
        // Additional files for create operation
        for (std::size_t i = 3; i < args_.size(); ++i) {
            config_.files.emplace_back(args_[i]);
        }
        
        if (!config_.is_valid()) {
            return std::unexpected("Invalid configuration");
        }
        
        return config_;
    }
    
private:
    /// Parse operation flags (cxt and v)
    auto parse_operation_flags(std::string_view flags) -> Result<void> {
        bool operation_set = false;
        
        for (char flag : flags) {
            switch (flag) {
                case 'c':
                    if (operation_set) {
                        return std::unexpected("Multiple operations specified");
                    }
                    config_.operation = TarOperation::Create;
                    operation_set = true;
                    break;
                    
                case 'x':
                    if (operation_set) {
                        return std::unexpected("Multiple operations specified");
                    }
                    config_.operation = TarOperation::Extract;
                    operation_set = true;
                    break;
                    
                case 't':
                    if (operation_set) {
                        return std::unexpected("Multiple operations specified");
                    }
                    config_.operation = TarOperation::List;
                    operation_set = true;
                    break;
                    
                case 'v':
                    config_.verbose = true;
                    break;
                    
                default:
                    return std::unexpected(std::format("Unknown flag: {}", flag));
            }
        }
        
        if (!operation_set) {
            return std::unexpected("No operation specified (use c, x, or t)");
        }
        
        return {};
    }
};

// =============================================================================
// Main Application Class
// =============================================================================

/// Main TAR utility application
class TarUtilityApp {
private:
    TarConfig config_;
    
public:
    explicit TarUtilityApp(const TarConfig& config) : config_{config} {}
    
    /// Run the TAR utility
    [[nodiscard]] auto run() -> Result<void> {
        try {
            TarArchive archive{config_.archive_file, config_.verbose};
            
            switch (config_.operation) {
                case TarOperation::Create:
                    return archive.create_archive(config_.files);
                    
                case TarOperation::Extract: {
                    std::optional<std::vector<std::string>> filter;
                    if (!config_.files.empty()) {
                        std::vector<std::string> file_names;
                        std::transform(config_.files.begin(), config_.files.end(),
                                     std::back_inserter(file_names),
                                     [](const auto& path) { return path.string(); });
                        filter = file_names;
                    }
                    return archive.extract_archive(filter);
                }
                    
                case TarOperation::List:
                    return archive.list_archive();
            }
            
            return std::unexpected("Invalid operation");
        } catch (const std::exception& e) {
            return std::unexpected(std::format("Runtime error: {}", e.what()));
        }
    }
};

// =============================================================================
// Modern Usage Information
// =============================================================================

/// Display usage information
void show_usage(std::string_view program_name) {
    std::cout << std::format(R"(
Usage: {} [operation][options] archive_file [files...]

Operations (choose one):
  c    Create new archive
  x    Extract files from archive
  t    List archive contents

Options:
  v    Verbose output

Examples:
  {} cv archive.tar file1.txt file2.txt    # Create archive
  {} xv archive.tar                         # Extract all files
  {} xv archive.tar file1.txt               # Extract specific file
  {} tv archive.tar                         # List contents

Modern C++23 implementation with type safety and RAII.
)",
        program_name, program_name, program_name, program_name, program_name
    );
}

} // namespace xinim::tar_utility

// =============================================================================
// Modern Main Function
// =============================================================================

auto main(int argc, char* argv[]) -> int {
    using namespace xinim::tar_utility;
    
    try {
        // Parse command line arguments
        TarCommandLineParser parser{std::span{argv, static_cast<std::size_t>(argc)}};
        auto config_result = parser.parse();
        
        if (!config_result) {
            std::cerr << std::format("Error: {}\\n", config_result.error());
            show_usage(argv[0]);
            return 1;
        }
        
        // Create and run the application
        TarUtilityApp app{*config_result};
        auto result = app.run();
        
        if (!result) {
            std::cerr << std::format("Error: {}\\n", result.error());
            return 1;
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << std::format("Fatal error: {}\\n", e.what());
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error occurred\\n";
        return 1;
    }
}
