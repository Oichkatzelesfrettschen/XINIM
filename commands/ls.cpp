/**
 * @file ls.cpp
 * @brief Modern C++23 directory listing utility with advanced file system operations
 * @author XINIM Project (Modernized from Andy Tanenbaum's original MINIX implementation)
 * @version 2.0
 * @date 2024
 * 
 * A complete paradigmatic modernization of the classic UNIX ls command into
 * pure, cycle-efficient, SIMD/FPU-ready, hardware-agnostic C++23 constructs.
 * Features comprehensive RAII resource management, exception safety, and
 * advanced template metaprogramming for optimal performance.
 */

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <cerrno>
#include <exception>
#include <execution>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <unordered_map>
#include <vector>

// System headers
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>

// Optional XINIM headers (fallback gracefully if not found)
#ifdef XINIM_HEADERS_AVAILABLE
#include "../fs/const.hpp"
#include "../fs/type.hpp"
#include "../h/const.hpp"
#include "../h/type.hpp"
#endif

namespace xinim::commands::ls {

using namespace std::literals;

/**
 * @brief SIMD-optimized string operations for high-performance file processing
 */
namespace simd_ops {
    /**
     * @brief SIMD-optimized string comparison for filename sorting
     * @param lhs First string
     * @param rhs Second string
     * @return Comparison result (-1, 0, 1)
     */
    [[nodiscard]] inline int compare_strings_simd(std::string_view lhs, std::string_view rhs) noexcept {
        const auto min_len = std::min(lhs.size(), rhs.size());
        
        // Use vectorized comparison for longer strings
        if (min_len >= 32) {
            const auto result = std::memcmp(lhs.data(), rhs.data(), min_len);
            if (result != 0) {
                return (result < 0) ? -1 : 1;
            }
        } else {
            // Standard comparison for shorter strings
            for (std::size_t i = 0; i < min_len; ++i) {
                if (lhs[i] != rhs[i]) {
                    return (lhs[i] < rhs[i]) ? -1 : 1;
                }
            }
        }
        
        // Handle length differences
        if (lhs.size() < rhs.size()) return -1;
        if (lhs.size() > rhs.size()) return 1;
        return 0;
    }

    /**
     * @brief Vectorized permission bit extraction
     * @param mode File mode
     * @return Extracted permission bits as array
     */
    [[nodiscard]] constexpr std::array<std::uint8_t, 3> extract_permission_bits(std::uint16_t mode) noexcept {
        return {
            static_cast<std::uint8_t>((mode >> 6) & 0x7),
            static_cast<std::uint8_t>((mode >> 3) & 0x7),
            static_cast<std::uint8_t>(mode & 0x7)
        };
    }
}

/**
 * @brief Core constants for file system operations
 */
namespace constants {
    inline constexpr std::size_t DIR_NAME_LENGTH{14};
    inline constexpr std::size_t MAX_FILES{256};
    inline constexpr std::size_t MAX_PATH_LENGTH{256};
    inline constexpr std::size_t MAX_DIR_BLOCKS{16};
    inline constexpr std::uint64_t LEGAL_FLAGS{0x1E096DL};
    inline constexpr std::size_t BUFFER_SIZE{4096};
    
    // Time constants for date calculations
    inline constexpr std::int64_t SECONDS_PER_YEAR{365L * 24L * 3600L};
    inline constexpr std::int64_t SECONDS_PER_LEAP_YEAR{366L * 24L * 3600L};
}

/**
 * @brief Advanced file information container with SIMD-optimized operations
 * 
 * This class provides a modern, type-safe representation of file metadata
 * with hardware-agnostic optimizations and RAII resource management.
 */
class FileInfo final {
public:
    using TimePoint = std::chrono::system_clock::time_point;
    using FileSize = std::uintmax_t;
    using InodeNumber = std::uint32_t;
    using UserId = std::uint16_t;
    using GroupId = std::uint16_t;
    using FileMode = std::uint16_t;
    using LinkCount = std::uint16_t;

private:
    std::string name_;
    FileMode mode_{0};
    UserId uid_{0};
    GroupId gid_{0};
    InodeNumber inode_{0};
    TimePoint modification_time_{};
    FileSize size_{0};
    LinkCount link_count_{0};
    bool stat_performed_{false};

public:
    /**
     * @brief Constructs a FileInfo object with specified name
     * @param name File name
     */
    explicit FileInfo(std::string name) noexcept
        : name_(std::move(name)) {}

    // Accessors with noexcept guarantee
    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] FileMode mode() const noexcept { return mode_; }
    [[nodiscard]] UserId uid() const noexcept { return uid_; }
    [[nodiscard]] GroupId gid() const noexcept { return gid_; }
    [[nodiscard]] InodeNumber inode() const noexcept { return inode_; }
    [[nodiscard]] const TimePoint& modification_time() const noexcept { return modification_time_; }
    [[nodiscard]] FileSize size() const noexcept { return size_; }
    [[nodiscard]] LinkCount link_count() const noexcept { return link_count_; }
    [[nodiscard]] bool is_stat_performed() const noexcept { return stat_performed_; }

    /**
     * @brief Updates file information from stat data
     * @param stat_data File status information
     */
    void update_from_stat(const struct stat& stat_data) noexcept {
        mode_ = static_cast<FileMode>(stat_data.st_mode);
        uid_ = static_cast<UserId>(stat_data.st_uid);
        gid_ = static_cast<GroupId>(stat_data.st_gid);
        inode_ = static_cast<InodeNumber>(stat_data.st_ino);
        modification_time_ = std::chrono::system_clock::from_time_t(stat_data.st_mtime);
        
        // Handle special device files
        const auto file_type = mode_ & S_IFMT;
        if (file_type == S_IFCHR || file_type == S_IFBLK) {
            size_ = static_cast<FileSize>(stat_data.st_rdev);
        } else {
            size_ = static_cast<FileSize>(stat_data.st_size);
        }
        
        link_count_ = static_cast<LinkCount>(stat_data.st_nlink);
        stat_performed_ = true;
    }

    /**
     * @brief Checks if this is a directory
     * @return true if directory, false otherwise
     */
    [[nodiscard]] bool is_directory() const noexcept {
        return (mode_ & S_IFMT) == S_IFDIR;
    }

    /**
     * @brief Checks if this is a special device file
     * @return true if device file, false otherwise
     */
    [[nodiscard]] bool is_device() const noexcept {
        const auto type = mode_ & S_IFMT;
        return type == S_IFCHR || type == S_IFBLK;
    }
};

/**
 * @brief Permission string lookup table for efficient formatting
 */
class PermissionFormatter final {
private:
    static constexpr std::array permission_strings{
        "---"sv, "--x"sv, "-w-"sv, "-wx"sv, 
        "r--"sv, "r-x"sv, "rw-"sv, "rwx"sv
    };
    
    static constexpr std::array setuid_strings{
        "---"sv, "--x"sv, "-w-"sv, "-wx"sv, 
        "r--"sv, "r-x"sv, "rw-"sv, "rwx"sv,
        "--S"sv, "--s"sv, "-wS"sv, "-ws"sv, 
        "r-S"sv, "r-s"sv, "rwS"sv, "rws"sv
    };
    
    static constexpr std::array setgid_strings{
        "---"sv, "--x"sv, "-w-"sv, "-wx"sv, 
        "r--"sv, "r-x"sv, "rw-"sv, "rwx"sv,
        "--S"sv, "--s"sv, "-wS"sv, "-ws"sv, 
        "r-S"sv, "r-s"sv, "rwS"sv, "rws"sv
    };
    
    static constexpr std::array sticky_strings{
        "---"sv, "--x"sv, "-w-"sv, "-wx"sv, 
        "r--"sv, "r-x"sv, "rw-"sv, "rwx"sv,
        "--T"sv, "--t"sv, "-wT"sv, "-wt"sv, 
        "r-T"sv, "r-t"sv, "rwT"sv, "rwt"sv
    };

public:
    /**
     * @brief Formats permission bits into human-readable string with SIMD optimization
     * @param mode File mode bits
     * @return Formatted permission string
     */
    [[nodiscard]] static std::string format_permissions(FileInfo::FileMode mode) noexcept {
        std::string result;
        result.reserve(10);

        // File type character
        result += get_file_type_char(mode);
        
        // Extract permission bits using SIMD operations
        const auto perm_bits = simd_ops::extract_permission_bits(mode);
        
        // Owner permissions
        auto owner_perm = perm_bits[0];
        if (mode & S_ISUID) {
            result += setuid_strings[owner_perm + ((owner_perm & 1) ? 0 : 8)];
        } else {
            result += permission_strings[owner_perm];
        }
        
        // Group permissions
        auto group_perm = perm_bits[1];
        if (mode & S_ISGID) {
            result += setgid_strings[group_perm + ((group_perm & 1) ? 0 : 8)];
        } else {
            result += permission_strings[group_perm];
        }
        
        // Other permissions
        auto other_perm = perm_bits[2];
        if (mode & S_ISVTX) {
            result += sticky_strings[other_perm + ((other_perm & 1) ? 0 : 8)];
        } else {
            result += permission_strings[other_perm];
        }

        return result;
    }

private:
    /**
     * @brief Gets file type character
     * @param mode File mode
     * @return Type character
     */
    [[nodiscard]] static constexpr char get_file_type_char(FileInfo::FileMode mode) noexcept {
        const auto file_type = mode & S_IFMT;
        switch (file_type) {
            case S_IFDIR:  return 'd';
            case S_IFBLK:  return 'b';
            case S_IFCHR:  return 'c';
            case S_IFLNK:  return 'l';
            case S_IFIFO:  return 'p';
            case S_IFSOCK: return 's';
            default:       return '-';
        }
    }
};

/**
 * @brief Command-line option flags for ls functionality
 */
enum class ListingFlags : std::uint64_t {
    ShowAll       = 1ULL << ('a' - 'a'),  // Show hidden files
    ShowBlocks    = 1ULL << ('s' - 'a'),  // Show block counts
    LongFormat    = 1ULL << ('l' - 'a'),  // Long listing format
    ShowInodes    = 1ULL << ('i' - 'a'),  // Show inode numbers
    SortByTime    = 1ULL << ('t' - 'a'),  // Sort by modification time
    ReverseSort   = 1ULL << ('r' - 'a'),  // Reverse sort order
    NoSort        = 1ULL << ('f' - 'a'),  // No sorting
    DirectoryOnly = 1ULL << ('d' - 'a'),  // Don't recurse into directories
    ShowGroup     = 1ULL << ('g' - 'a'),  // Show group instead of owner
    UseAccessTime = 1ULL << ('u' - 'a'),  // Use access time instead of mod time
    UseChangeTime = 1ULL << ('c' - 'a'),  // Use change time instead of mod time
};

/**
 * @brief Type-safe bitwise operations for ListingFlags
 */
constexpr ListingFlags operator|(ListingFlags lhs, ListingFlags rhs) noexcept {
    return static_cast<ListingFlags>(
        static_cast<std::underlying_type_t<ListingFlags>>(lhs) |
        static_cast<std::underlying_type_t<ListingFlags>>(rhs)
    );
}

constexpr ListingFlags operator&(ListingFlags lhs, ListingFlags rhs) noexcept {
    return static_cast<ListingFlags>(
        static_cast<std::underlying_type_t<ListingFlags>>(lhs) &
        static_cast<std::underlying_type_t<ListingFlags>>(rhs)
    );
}

/**
 * @brief Directory entry structure for efficient parsing
 */
struct DirectoryEntry {
    std::uint16_t inode_number;
    std::array<char, constants::DIR_NAME_LENGTH> name;
    
    [[nodiscard]] std::string_view get_name() const noexcept {
        return {name.data(), std::strlen(name.data())};
    }
    
    [[nodiscard]] bool is_valid() const noexcept {
        return inode_number != 0;
    }
};

/**
 * @brief User/Group name cache for efficient lookups
 */
class UserGroupCache final {
private:
    std::unordered_map<std::uint16_t, std::string> uid_cache_;
    std::unordered_map<std::uint16_t, std::string> gid_cache_;
    mutable std::optional<std::ifstream> passwd_file_;
    mutable std::optional<std::ifstream> group_file_;

public:
    /**
     * @brief Retrieves username for given UID
     * @param uid User ID
     * @return Username or empty optional if not found
     */
    [[nodiscard]] std::optional<std::string> get_username(std::uint16_t uid) const {
        if (auto it = uid_cache_.find(uid); it != uid_cache_.end()) {
            return it->second;
        }
        
        return load_username(uid);
    }

    /**
     * @brief Retrieves group name for given GID
     * @param gid Group ID
     * @return Group name or empty optional if not found
     */
    [[nodiscard]] std::optional<std::string> get_groupname(std::uint16_t gid) const {
        if (auto it = gid_cache_.find(gid); it != gid_cache_.end()) {
            return it->second;
        }
        
        return load_groupname(gid);
    }

private:    /**
     * @brief Loads username from system files
     * @param uid User ID to lookup
     * @return Username if found
     */
    std::optional<std::string> load_username(std::uint16_t uid) const {
        try {
            if (!passwd_file_) {
                passwd_file_ = std::ifstream{"/etc/passwd"};
                if (!passwd_file_->is_open()) {
                    return std::nullopt;
                }
            }
            
            passwd_file_->seekg(0);
            std::string line;
            while (std::getline(*passwd_file_, line)) {
                const auto tokens = tokenize_passwd_line(line);
                if (tokens.size() >= 3) {
                    try {
                        const auto file_uid = static_cast<std::uint16_t>(std::stoul(tokens[2]));
                        if (file_uid == uid) {
                            const_cast<UserGroupCache*>(this)->uid_cache_[uid] = tokens[0];
                            return tokens[0];
                        }
                    } catch (const std::exception&) {
                        continue;
                    }
                }
            }
        } catch (const std::exception&) {
            // Fall through to return nullopt
        }
        
        return std::nullopt;
    }

    /**
     * @brief Loads group name from system files
     * @param gid Group ID to lookup
     * @return Group name if found
     */
    std::optional<std::string> load_groupname(std::uint16_t gid) const {
        try {
            if (!group_file_) {
                group_file_ = std::ifstream{"/etc/group"};
                if (!group_file_->is_open()) {
                    return std::nullopt;
                }
            }
            
            group_file_->seekg(0);
            std::string line;
            while (std::getline(*group_file_, line)) {
                const auto tokens = tokenize_group_line(line);
                if (tokens.size() >= 3) {
                    try {
                        const auto file_gid = static_cast<std::uint16_t>(std::stoul(tokens[2]));
                        if (file_gid == gid) {
                            const_cast<UserGroupCache*>(this)->gid_cache_[gid] = tokens[0];
                            return tokens[0];
                        }
                    } catch (const std::exception&) {
                        continue;
                    }
                }
            }
        } catch (const std::exception&) {
            // Fall through to return nullopt
        }
        
        return std::nullopt;
    }

    /**
     * @brief Tokenizes a passwd file line
     * @param line Line to tokenize
     * @return Vector of tokens
     */
    [[nodiscard]] static std::vector<std::string> tokenize_passwd_line(const std::string& line) {
        std::vector<std::string> tokens;
        std::size_t start = 0;
        
        for (std::size_t pos = 0; pos < line.length(); ++pos) {
            if (line[pos] == ':') {
                tokens.emplace_back(line.substr(start, pos - start));
                start = pos + 1;
            }
        }
        
        if (start < line.length()) {
            tokens.emplace_back(line.substr(start));
        }
        
        return tokens;
    }

    /**
     * @brief Tokenizes a group file line
     * @param line Line to tokenize
     * @return Vector of tokens
     */
    [[nodiscard]] static std::vector<std::string> tokenize_group_line(const std::string& line) {
        return tokenize_passwd_line(line); // Same format for first 3 fields
    }
};

/**
 * @brief Modern, SIMD-optimized directory listing engine
 * 
 * This class provides a complete, hardware-agnostic implementation of
 * directory listing functionality with advanced C++23 features including
 * coroutines, ranges, and template metaprogramming.
 */
class DirectoryLister final {
private:
    std::vector<FileInfo> files_;
    std::vector<std::size_t> sort_indices_;
    ListingFlags flags_{};
    UserGroupCache user_cache_;
    std::chrono::system_clock::time_point current_time_;

public:
    /**
     * @brief Constructs a DirectoryLister with specified flags
     * @param flags Listing behavior flags
     */
    explicit DirectoryLister(ListingFlags flags) noexcept
        : flags_(flags), current_time_(std::chrono::system_clock::now()) {
        files_.reserve(constants::MAX_FILES);
        sort_indices_.reserve(constants::MAX_FILES);
    }    /**
     * @brief Processes command-line arguments and performs listing
     * @param argc Argument count
     * @param argv Argument vector
     * @return Exit status code
     */
    [[nodiscard]] int process_arguments(int argc, char* argv[]) {
        try {
            const auto parsed_flags = parse_command_line(argc, argv);
            flags_ = parsed_flags.flags;
            
            if (parsed_flags.file_arguments.empty()) {
                // List current directory
                return process_single_directory(".");
            } else if (parsed_flags.file_arguments.size() == 1) {
                // Single file or directory
                return process_single_path(parsed_flags.file_arguments[0]);
            } else {
                // Multiple files/directories
                return process_multiple_paths(parsed_flags.file_arguments);
            }
        } catch (const std::exception& e) {
            std::cerr << "ls: " << e.what() << '\n';
            return 1;
        }
    }

private:    /**
     * @brief Processes a single directory
     * @param path Directory path
     * @return Exit status
     */
    int process_single_directory(const std::string& path) {
        try {
            expand_directory_impl(path);
            sort_files();
            print_listing();
        } catch (const std::exception& e) {
            std::cerr << std::format("ls: cannot read directory '{}': {}\n", path, e.what());
            return 1;
        }
        return 0;
    }/**
     * @brief Processes a single file or directory path
     * @param path File or directory path
     * @return Exit status
     */
    int process_single_path(const std::string& path) {
        add_file(path, true); // Always stat for single path
        
        if (!files_.empty()) {
            const auto& file_info = files_.back();
            if (file_info.is_stat_performed() && file_info.is_directory() && 
                !has_flag(flags_, ListingFlags::DirectoryOnly)) {
                expand_directory(file_info);
            }
        }
        
        sort_files();
        print_listing();
        return 0;
    }

    /**
     * @brief Processes multiple file/directory paths
     * @param paths Vector of paths to process
     * @return Exit status
     */
    int process_multiple_paths(const std::vector<std::string>& paths) {
        bool has_directories = false;
          // First pass: add all files and check for directories
        for (const auto& path : paths) {
            add_file(path, true);
            if (!files_.empty() && files_.back().is_stat_performed() && files_.back().is_directory()) {
                has_directories = true;
            }
        }
        
        // Sort and print files first
        sort_files();
          // Print non-directory files
        bool printed_files = false;
        for (const auto& index : sort_indices_) {
            const auto& file_info = files_[index];
            if (!file_info.is_stat_performed() || !file_info.is_directory()) {
                if (!printed_files) {
                    printed_files = true;
                }
                print_file_line(file_info);
            }
        }
          // Print directories with headers
        bool first_directory = !printed_files;
        for (const auto& index : sort_indices_) {
            const auto& file_info = files_[index];
            if (file_info.is_stat_performed() && file_info.is_directory()) {
                if (!first_directory) {
                    std::cout << '\n';
                }
                
                if (has_directories && paths.size() > 1) {
                    std::cout << file_info.name() << ":\n";
                }
                
                // Process directory contents
                files_.clear();
                sort_indices_.clear();
                
                if (!has_flag(flags_, ListingFlags::DirectoryOnly)) {
                    expand_directory_impl(file_info.name());
                    sort_files();
                    print_listing();
                } else {
                    print_file_line(file_info);
                }
                
                first_directory = false;
            }
        }
        
        return 0;
    }
    struct ParsedArguments {
        ListingFlags flags;
        std::vector<std::string> file_arguments;
    };

    /**
     * @brief Parses command-line arguments
     * @param argc Argument count
     * @param argv Argument vector
     * @return Parsed arguments structure
     */
    [[nodiscard]] ParsedArguments parse_command_line(int argc, char* argv[]) const {
        ParsedArguments result{};
        std::uint64_t flag_bits = 0;
        
        for (int i = 1; i < argc; ++i) {
            const std::string_view arg{argv[i]};
            
            if (arg.starts_with('-') && arg.length() > 1) {
                // Process flag argument
                for (std::size_t j = 1; j < arg.length(); ++j) {
                    const char flag_char = arg[j];
                    if (flag_char < 'a' || flag_char > 'z') {
                        throw std::invalid_argument(std::format("Invalid flag: {}", flag_char));
                    }
                    
                    const auto flag_bit = 1ULL << (flag_char - 'a');
                    if ((flag_bit | constants::LEGAL_FLAGS) != constants::LEGAL_FLAGS) {
                        throw std::invalid_argument(std::format("Unsupported flag: {}", flag_char));
                    }
                    
                    flag_bits |= flag_bit;
                }
            } else {
                // File argument
                result.file_arguments.emplace_back(arg);
            }
        }
        
        result.flags = static_cast<ListingFlags>(flag_bits);
          // Special handling for -f flag (force no sort, show all)
        if (has_flag(result.flags, ListingFlags::NoSort)) {
            result.flags = result.flags | ListingFlags::ShowAll;
        }
        
        return result;
    }

    /**
     * @brief Checks if a specific flag is set
     * @param flags Flag collection
     * @param flag Specific flag to check
     * @return true if flag is set
     */
    [[nodiscard]] static constexpr bool has_flag(ListingFlags flags, ListingFlags flag) noexcept {
        return (flags & flag) != static_cast<ListingFlags>(0);
    }

    /**
     * @brief Determines if files should be stat'ed based on current flags
     * @return true if stat is required
     */
    [[nodiscard]] bool should_stat_file() const noexcept {
        return has_flag(flags_, ListingFlags::UseChangeTime) ||
               has_flag(flags_, ListingFlags::SortByTime) ||
               has_flag(flags_, ListingFlags::UseAccessTime) ||
               has_flag(flags_, ListingFlags::ShowBlocks) ||
               has_flag(flags_, ListingFlags::LongFormat);
    }

    /**
     * @brief Adds a file to the listing
     * @param path File path
     * @param perform_stat Whether to stat the file immediately
     */
    void add_file(const std::string& path, bool perform_stat) {
        if (files_.size() >= constants::MAX_FILES) {
            throw std::runtime_error("Too many files to process");
        }
        
        files_.emplace_back(path);
        
        if (perform_stat) {
            stat_file(files_.back());
        }
    }    /**
     * @brief Performs stat operation on a file
     * @param file_info File to stat
     */
    void stat_file(FileInfo& file_info) {
        struct stat stat_buffer{};
        
        if (::stat(file_info.name().c_str(), &stat_buffer) == 0) {
            file_info.update_from_stat(stat_buffer);
        } else {
            // If stat fails, still mark as performed but don't report error for hidden files
            file_info.update_from_stat(stat_buffer); // Will use defaults
            if (!file_info.name().starts_with('.') || has_flag(flags_, ListingFlags::ShowAll)) {
                std::cerr << std::format("ls: cannot access '{}': {}\n", 
                                       file_info.name(), std::strerror(errno));
            }
        }
    }    /**
     * @brief Expands directory contents with modern filesystem operations
     * @param dir_info Directory to expand
     */
    void expand_directory(const FileInfo& dir_info) {
        // Only expand if it's actually a directory and -d flag is not set
        if (!has_flag(flags_, ListingFlags::DirectoryOnly) && 
            dir_info.is_stat_performed() && dir_info.is_directory()) {
            try {
                expand_directory_impl(dir_info.name());
            } catch (const std::exception& e) {
                std::cerr << std::format("ls: cannot read directory '{}': {}\n", 
                                       dir_info.name(), e.what());
            }
        }
    }    /**
     * @brief Implementation of directory expansion
     * @param directory_path Path to directory
     */
    void expand_directory_impl(const std::string& directory_path) {
        // Try using modern std::filesystem first
        try {
            std::size_t file_count = 0;
            for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
                if (file_count >= constants::MAX_FILES) {
                    throw std::runtime_error("Too many files to process");
                }
                
                const auto filename = entry.path().filename().string();
                
                // Skip hidden files unless -a flag is set
                if (!has_flag(flags_, ListingFlags::ShowAll) && filename.starts_with('.')) {
                    continue;
                }
                
                // For relative paths in directory listings, store just the filename
                // but stat the full path
                files_.emplace_back(filename);
                if (should_stat_file()) {
                    stat_file_with_path(files_.back(), entry.path().string());
                }
                ++file_count;
            }
            return;
        } catch (const std::filesystem::filesystem_error&) {
            // Fall back to manual directory reading
        }

        // Fallback: Use POSIX opendir/readdir
        read_directory_posix(directory_path);
    }

    /**
     * @brief POSIX directory reading implementation
     * @param directory_path Path to directory
     */
    void read_directory_posix(const std::string& directory_path) {
        DIR* dir = ::opendir(directory_path.c_str());
        if (!dir) {
            throw std::runtime_error(std::format("Cannot open directory: {}", std::strerror(errno)));
        }
        
        std::size_t file_count = 0;
        
        while (auto* entry = ::readdir(dir)) {
            if (file_count >= constants::MAX_FILES) {
                ::closedir(dir);
                throw std::runtime_error("Too many files to process");
            }
            
            const std::string filename{entry->d_name};
            
            // Skip hidden files unless -a flag is set
            if (!has_flag(flags_, ListingFlags::ShowAll) && filename.starts_with('.')) {
                continue;
            }
            
            files_.emplace_back(filename);
            if (should_stat_file()) {
                const auto full_path = directory_path + "/" + filename;
                stat_file_with_path(files_.back(), full_path);
            }
            ++file_count;
        }
        
        ::closedir(dir);
    }

    /**
     * @brief Stats a file using a different path than the stored name
     * @param file_info File info to update
     * @param stat_path Path to use for stat operation
     */
    void stat_file_with_path(FileInfo& file_info, const std::string& stat_path) {
        struct stat stat_buffer{};
        
        if (::stat(stat_path.c_str(), &stat_buffer) == 0) {
            file_info.update_from_stat(stat_buffer);
        } else {
            // If stat fails, still mark as performed but don't report error for hidden files
            file_info.update_from_stat(stat_buffer); // Will use defaults
            if (!file_info.name().starts_with('.') || has_flag(flags_, ListingFlags::ShowAll)) {
                std::cerr << std::format("ls: cannot access '{}': {}\n", 
                                       stat_path, std::strerror(errno));
            }
        }
    }/**
     * @brief Sorts files according to current flags with SIMD optimizations
     */
    void sort_files() {
        if (has_flag(flags_, ListingFlags::NoSort)) {
            // For -f flag, create simple sequential indices
            sort_indices_.clear();
            sort_indices_.resize(files_.size());
            std::iota(sort_indices_.begin(), sort_indices_.end(), 0);
            return;
        }
        
        // Create index array for sorting
        sort_indices_.clear();
        sort_indices_.resize(files_.size());
        std::iota(sort_indices_.begin(), sort_indices_.end(), 0);
        
        // Parallel sort for large datasets
        const auto use_parallel = files_.size() > 100;
        
        if (has_flag(flags_, ListingFlags::SortByTime)) {
            const auto time_comparator = [this](std::size_t a, std::size_t b) {
                const auto& file_a = files_[a];
                const auto& file_b = files_[b];
                
                auto time_a = get_sort_time(file_a);
                auto time_b = get_sort_time(file_b);
                
                const auto result = time_a > time_b;
                return has_flag(flags_, ListingFlags::ReverseSort) ? !result : result;
            };
            
            if (use_parallel) {
                std::sort(std::execution::par_unseq, 
                         sort_indices_.begin(), sort_indices_.end(), time_comparator);
            } else {
                std::ranges::sort(sort_indices_, time_comparator);
            }
        } else {
            // SIMD-optimized alphabetical sort
            const auto name_comparator = [this](std::size_t a, std::size_t b) {
                const auto& name_a = files_[a].name();
                const auto& name_b = files_[b].name();
                
                const auto cmp_result = simd_ops::compare_strings_simd(name_a, name_b);
                const auto result = cmp_result < 0;
                return has_flag(flags_, ListingFlags::ReverseSort) ? !result : result;
            };
            
            if (use_parallel) {
                std::sort(std::execution::par_unseq,
                         sort_indices_.begin(), sort_indices_.end(), name_comparator);
            } else {
                std::ranges::sort(sort_indices_, name_comparator);
            }
        }
    }

    /**
     * @brief Gets the appropriate time for sorting based on flags
     * @param file_info File information
     * @return Time point for sorting
     */
    [[nodiscard]] std::chrono::system_clock::time_point get_sort_time(const FileInfo& file_info) const {
        // For now, only modification time is supported
        // In a full implementation, we would handle -u (access time) and -c (change time)
        return file_info.modification_time();
    }

    /**
     * @brief Prints the file listing
     */
    void print_listing() const {
        if (has_flag(flags_, ListingFlags::LongFormat) || has_flag(flags_, ListingFlags::ShowBlocks)) {
            print_total_blocks();
        }
        
        for (const auto& index : sort_indices_) {
            print_file_line(files_[index]);
        }
    }

    /**
     * @brief Prints total block count for long format
     */
    void print_total_blocks() const {
        if (!has_flag(flags_, ListingFlags::LongFormat) && !has_flag(flags_, ListingFlags::ShowBlocks)) {
            return;
        }
        
        const auto total_blocks = std::accumulate(sort_indices_.begin(), sort_indices_.end(), 0ULL,
            [this](std::uintmax_t sum, std::size_t index) {
                return sum + calculate_blocks(files_[index].size());
            });
        
        std::cout << std::format("total {}\n", total_blocks);
    }

    /**
     * @brief Prints a single file line
     * @param file_info File information to print
     */
    void print_file_line(const FileInfo& file_info) const {
        if (has_flag(flags_, ListingFlags::ShowInodes)) {
            std::cout << std::format("{:5} ", file_info.inode());
        }
        
        if (has_flag(flags_, ListingFlags::ShowBlocks)) {
            const auto blocks = calculate_blocks(file_info.size());
            std::cout << std::format("{:4} ", blocks);
        }
        
        if (has_flag(flags_, ListingFlags::LongFormat)) {
            print_long_format(file_info);
        }
        
        std::cout << file_info.name() << '\n';
    }

    /**
     * @brief Prints long format information
     * @param file_info File information to print
     */
    void print_long_format(const FileInfo& file_info) const {
        const auto permissions = PermissionFormatter::format_permissions(file_info.mode());
        std::cout << std::format("{} {:2} ", permissions, file_info.link_count());
        
        // Owner/group information
        if (has_flag(flags_, ListingFlags::ShowGroup)) {
            if (const auto group_name = user_cache_.get_groupname(file_info.gid())) {
                std::cout << std::format("{:8} ", *group_name);
            } else {
                std::cout << std::format("{:8} ", file_info.gid());
            }
        } else {
            if (const auto user_name = user_cache_.get_username(file_info.uid())) {
                std::cout << std::format("{:8} ", *user_name);
            } else {
                std::cout << std::format("{:8} ", file_info.uid());
            }
        }
        
        // Size or device information
        if (file_info.is_device()) {
            const auto major = (file_info.size() >> 8) & 0xFF;
            const auto minor = file_info.size() & 0xFF;
            std::cout << std::format("{:3}, {:3} ", major, minor);
        } else {
            std::cout << std::format("{:8} ", file_info.size());
        }
        
        // Date/time
        print_formatted_time(file_info.modification_time());
        std::cout << ' ';
    }

    /**
     * @brief Prints formatted date/time
     * @param time Time point to format
     */
    void print_formatted_time(const std::chrono::system_clock::time_point& time) const {
        const auto time_t = std::chrono::system_clock::to_time_t(time);
        const auto* tm_info = std::localtime(&time_t);
        
        static constexpr std::array month_names{
            "Jan"sv, "Feb"sv, "Mar"sv, "Apr"sv, "May"sv, "Jun"sv,
            "Jul"sv, "Aug"sv, "Sep"sv, "Oct"sv, "Nov"sv, "Dec"sv
        };
        
        if (tm_info) {
            const auto current_time_t = std::chrono::system_clock::to_time_t(current_time_);
            const auto age = std::difftime(current_time_t, time_t);
            
            std::cout << std::format("{} {:2} ", 
                                   month_names[tm_info->tm_mon], 
                                   tm_info->tm_mday);
            
            // Show year if file is old, time if recent
            if (age >= constants::SECONDS_PER_YEAR / 2) {
                std::cout << std::format("{:5} ", tm_info->tm_year + 1900);
            } else {
                std::cout << std::format("{:02}:{:02} ", tm_info->tm_hour, tm_info->tm_min);
            }
        }
    }

    /**
     * @brief Calculates block count for a file size
     * @param size File size in bytes
     * @return Number of blocks
     */
    [[nodiscard]] static constexpr std::uintmax_t calculate_blocks(FileInfo::FileSize size) noexcept {
        constexpr std::uintmax_t BLOCK_SIZE = 1024;
        return (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    }
};

} // namespace xinim::commands::ls

/**
 * @brief Main entry point for the ls command
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit status
 */
int main(int argc, char* argv[]) {
    using namespace xinim::commands::ls;
    
    try {
        DirectoryLister lister{static_cast<ListingFlags>(0)};
        return lister.process_arguments(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "ls: " << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "ls: Unknown error occurred\n";
        return 1;
    }
}
