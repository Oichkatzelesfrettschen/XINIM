/**
 * @file ar.cpp
 * @brief Modern C++23 archiver utility for XINIM operating system
 * @author Michiel Huisjes (original), modernized for XINIM C++23 migration
 * @version 2.0 - Modernized with C++23 features and RAII principles
 * @date 2025-08-13
 *
 * @section Description
 * A modernized archive utility that manages archive files, supporting operations
 * like append, delete, replace, extract, print, and list contents. Originally
 * developed by Michiel Huisjes, this version leverages C++23 features for type
 * safety, RAII, thread safety, and improved performance.
 *
 * @section Features
 * - RAII-based resource management for file descriptors
 * - Type-safe string handling with std::string_view
 * - Thread-safe operations with std::mutex
 * - Constexpr constants for compile-time optimization
 * - Exception-safe error handling
 * - Comprehensive Doxygen documentation
 * - Support for C++23 ranges and string formatting
 *
 * @section Usage
 * ar [adprtxv] archive [file] ...
 * - a: append
 * - d: delete
 * - p: print named files
 * - r: replace (append when not in archive)
 * - t: print contents of archive
 * - x: extract
 * - v: verbose
 *
 * @note Requires C++23 compliant compiler
 */

#include "signal.hpp"
#include "stat.hpp"
#include <array>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

/// POSIX file descriptor constants wrapped as constexpr values.
namespace descriptors {
constexpr int STDIN = STDIN_FILENO;
constexpr int STDOUT = STDOUT_FILENO;
constexpr int STDERR = STDERR_FILENO;
} // namespace descriptors

// Modern constants
namespace config {
constexpr uint16_t MAGIC_NUMBER = 0177545;
constexpr std::size_t IO_SIZE = 10 * 1024;
constexpr std::size_t BLOCK_SIZE = 1024;
constexpr std::size_t NAME_SIZE = 14;
constexpr std::size_t TEMP_NAME_SIZE = 15;
} // namespace config

// Modern type aliases
using UString = std::array<char, config::NAME_SIZE + 1>;
using Buffer = std::array<char, config::IO_SIZE>;
using TerminalBuffer = std::array<char, config::BLOCK_SIZE>;

/**
 * @brief File operation modes for archive manipulation.
 */
enum class FileMode { READ, CREATE, APPEND };

/**
 * @brief Header metadata for an individual archive member.
 */
struct Member {
    UString m_name{};   ///< Null-terminated member name.
    int16_t m_time_1{}; ///< Timestamp high word.
    int16_t m_time_2{}; ///< Timestamp low word.
    uint8_t m_uid{};    ///< Owning user identifier.
    uint8_t m_gid{};    ///< Owning group identifier.
    int16_t m_mode{};   ///< POSIX file mode bits.
    int16_t m_size_1{}; ///< File size high word.
    int16_t m_size_2{}; ///< File size low word.
};

/**
 * @brief RAII-based archiver handling archive operations.
 */
class Archiver {
  public:
    /** @brief Construct a new Archiver instance. */
    Archiver() = default;
    /**
     * @brief Destroy the Archiver, closing open descriptors and removing the temporary archive.
     */
    ~Archiver() {
        std::lock_guard lock(mtx_);
        if (ar_fd_ != -1)
            ::close(ar_fd_);
        if (temp_fd_ != -1)
            ::close(temp_fd_);
        std::filesystem::remove(temp_arch_);
    }

    /**
     * @brief Parse command line options and execute the requested archive operation.
     *
     * @param argc Argument count.
     * @param argv Argument vector.
     */
    void process(int argc, char *argv[]);

    /**
     * @brief Handle interrupt signals to ensure temporary artifacts are removed.
     *
     * @param sig Signal number triggering cleanup.
     */
    static void cleanup_handler(int sig) noexcept {
        std::lock_guard lock(instance_->mtx_);
        std::filesystem::remove(instance_->temp_arch_);
        std::exit(2);
    }

  private:
    // Core operations
    void add(std::string_view name, int fd, char mess);
    void extract(const Member &member);
    void copy_member(const Member &member, int from, int to);
    [[nodiscard]] int open_archive(std::string_view name, FileMode mode);
    [[nodiscard]] Member *get_member();

    // Utility functions
    [[nodiscard]] static std::string_view ar_basename(std::string_view path);
    [[nodiscard]] static bool equal(std::string_view str1, std::string_view str2);
    [[noreturn]] static void usage();
    [[noreturn]] static void error(bool quit, std::string_view str1, std::string_view str2 = "");
    static void print(std::string_view str);
    static void flush();
    static void show(char c, std::string_view name);
    static void p_name(std::string_view mem_name);
    static void print_mode(int mode);
    static void litoa(int pad, long number);
    static void date(long t);
    [[nodiscard]] static long swap(int16_t &mem_1, int16_t &mem_2);
    [[nodiscard]] static ssize_t safe_write(int fd, const void *buffer, std::size_t size);
    void mktempname();

    // State
    int ar_fd_{-1};
    int temp_fd_{-1};
    long mem_time_{0};
    long mem_size_{0};
    bool verbose_{false};
    bool app_fl_{false};
    bool ex_fl_{false};
    bool show_fl_{false};
    bool pr_fl_{false};
    bool rep_fl_{false};
    bool del_fl_{false};
    Buffer io_buffer_{};
    TerminalBuffer terminal_{};
    std::size_t terminal_index_{0};
    UString temp_arch_{"/tmp/ar.XXXXX"};
    mutable std::mutex mtx_{};
    static thread_local Archiver *instance_;
};

thread_local Archiver *Archiver::instance_ = nullptr;

// Utility functions
/**
 * @brief Check whether a number is odd.
 *
 * @param nr Number to test.
 * @return True if @p nr is odd.
 */
[[nodiscard]] constexpr bool odd(int nr) { return nr & 0x01; }
/**
 * @brief Return the nearest even number greater than or equal to the input.
 *
 * @param nr Number to adjust.
 * @return Even counterpart of @p nr.
 */
[[nodiscard]] constexpr int even(int nr) { return odd(nr) ? nr + 1 : nr; }

/**
 * @brief Display usage information and terminate execution.
 */
void Archiver::usage() { error(true, "Usage: ar [adprtxv] archive [file] ..."); }

/**
 * @brief Report an error message and optionally exit.
 *
 * @param quit Whether to terminate
 * the program after reporting.
 * @param str1 Primary error description.
 * @param str2 Optional
 * secondary detail.
 */
void Archiver::error(bool quit, std::string_view str1, std::string_view str2) {
    std::lock_guard lock(mtx_);
    write(descriptors::STDERR, str1.data(), str1.size());
    if (!str2.empty()) {
        write(descriptors::STDERR, str2.data(), str2.size());
    }
    write(descriptors::STDERR, "\n", 1);
    if (quit) {
        std::filesystem::remove(instance_->temp_arch_);
        std::exit(1);
    }
}

/**
 * @brief Extract basename component from a path.
 *
 * @param path Path string to process.
 *
 * @return Basename portion of @p path.
 */
std::string_view Archiver::ar_basename(std::string_view path) {
    auto last_slash = path.rfind('/');
    if (last_slash == std::string_view::npos)
        return path;
    if (path[last_slash + 1] == '\0') {
        return ar_basename(path.substr(0, last_slash));
    }
    return path.substr(last_slash + 1);
}

/**
 * @brief Compare two archive member names for equality.
 *
 * @param str1 First name to
 * compare.
 * @param str2 Second name to compare.
 * @return True if the names match up to @c
 * config::NAME_SIZE characters.
 */
bool Archiver::equal(std::string_view str1, std::string_view str2) {
    return str1.substr(0, config::NAME_SIZE) == str2.substr(0, config::NAME_SIZE);
}

/**
 * @brief Open an archive file in the specified mode.
 *
 * @param name Archive filename.
 *
 * @param mode Desired open mode.
 * @return File descriptor for the opened archive.
 *
 * @throws
 * std::runtime_error If the archive cannot be opened or is invalid.
 */
int Archiver::open_archive(std::string_view name, FileMode mode) {
    std::lock_guard lock(mtx_);
    int fd = -1;
    if (mode == FileMode::CREATE) {
        fd = ::creat(name.data(), 0644);
        if (fd < 0) {
            error(true, std::format("Cannot create {}", name));
        }
        safe_write(fd, &config::MAGIC_NUMBER, sizeof(config::MAGIC_NUMBER));
        return fd;
    }
    fd = ::open(name.data(), static_cast<int>(mode));
    if (fd < 0) {
        if (mode == FileMode::APPEND) {
            ::close(open_archive(name, FileMode::CREATE));
            error(false, std::format("ar: creating {}", name));
            return open_archive(name, FileMode::APPEND);
        }
        error(true, std::format("Cannot open {}", name));
    }
    ::lseek(fd, 0L, SEEK_SET);
    uint16_t read_magic{0};
    if (::read(fd, &read_magic, sizeof(read_magic)) != sizeof(read_magic) ||
        read_magic != config::MAGIC_NUMBER) {
        error(true, std::format("{} is not in ar format", name));
    }
    return fd;
}

/**
 * @brief Retrieve the next member header from the archive.
 *
 * Updates internal metadata such
 * as timestamp and size using the
 * retrieved header.
 *
 * @return Pointer to a static Member
 * instance representing the next
 * archive entry, or nullptr when the end of the archive
 * is
 * reached.
 */
Member *Archiver::get_member() {
    std::lock_guard lock(mtx_);
    static Member member{};
    if (::read(ar_fd_, &member, sizeof(Member)) == 0) {
        return nullptr;
    }
    if (static_cast<std::size_t>(::read(ar_fd_, &member, sizeof(Member))) != sizeof(Member)) {
        error(true, "Corrupted archive");
    }
    mem_time_ = swap(member.m_time_1, member.m_time_2);
    mem_size_ = swap(member.m_size_1, member.m_size_2);
    return &member;
}

/**
 * @brief Swap two 16-bit words and return the combined 32-bit value.
 *
 * @param mem_1 First
 * word (will contain @p mem_2).
 * @param mem_2 Second word (will contain @p mem_1).
 * @return
 * Joined 32-bit integer.
 */
long Archiver::swap(int16_t &mem_1, int16_t &mem_2) {
    union Swapper {
        struct {
            int16_t mem_1;
            int16_t mem_2;
        } mem;
        long joined;
    } swapped;
    swapped.mem.mem_1 = mem_2;
    swapped.mem.mem_2 = mem_1;
    return swapped.joined;
}

/**
 * @brief Robust wrapper around ::write that aborts on partial writes.
 *
 * @param fd
 * Destination file descriptor.
 * @param buffer Pointer to the data to write.
 * @param size Number
 * of bytes to be written.
 * @return      Number of bytes successfully written.
 *
 * @note This
 * function invokes ::error and terminates the program
 * if the requested number of bytes
 * cannot be written.
 */
ssize_t Archiver::safe_write(int fd, const void *buffer, std::size_t size) {
    std::lock_guard lock(mtx_);
    const auto bytes_written = ::write(fd, buffer, size);
    if (bytes_written != static_cast<ssize_t>(size)) {
        error(true, "Write operation failed");
    }
    return bytes_written;
}

/**
 * @brief Append text to the terminal buffer for batched output.
 *
 * @param str Text to
 * write.
 */
void Archiver::print(std::string_view str) {
    std::lock_guard lock(mtx_);
    for (char c : str) {
        terminal_[terminal_index_++] = c;
        if (terminal_index_ == config::BLOCK_SIZE) {
            flush();
        }
    }
}

/**
 * @brief Flush the internal terminal buffer to standard output.
 *
 * Writes any accumulated
 * output in the terminal buffer to
 * descriptors::STDOUT and resets the buffer index.
 */
void Archiver::flush() {
    std::lock_guard lock(mtx_);
    if (terminal_index_ != 0) {
        safe_write(descriptors::STDOUT, terminal_.data(), terminal_index_);
        terminal_index_ = 0;
    }
}

/**
 * @brief Display an operation character and member name.
 *
 * @param c Operation indicator
 * (e.g., 'd' for delete).
 * @param name Archive member name.
 */
void Archiver::show(char c, std::string_view name) {
    std::lock_guard lock(mtx_);
    write(descriptors::STDOUT, &c, 1);
    write(descriptors::STDOUT, " - ", 3);
    write(descriptors::STDOUT, name.data(), name.size());
    write(descriptors::STDOUT, "\n", 1);
}

/**
 * @brief Print an archive member name padded to fixed width.
 *
 * @param mem_name Name
 * extracted from archive header.
 */
void Archiver::p_name(std::string_view mem_name) {
    UString name{};
    std::ranges::copy_n(mem_name.begin(), std::min(mem_name.size(), config::NAME_SIZE),
                        name.begin());
    print(name.data());
}

/**
 * @brief Emit symbolic POSIX permission bits for a file mode.
 *
 * @param mode Mode bits to
 * display.
 */
void Archiver::print_mode(int mode) {
    std::array<char, 11> mode_buf{' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    int tmp = mode;
    for (int i = 0; i < 3; ++i) {
        mode_buf[i * 3] = (tmp & S_IREAD) ? 'r' : '-';
        mode_buf[i * 3 + 1] = (tmp & S_IWRITE) ? 'w' : '-';
        mode_buf[i * 3 + 2] = (tmp & S_IEXEC) ? 'x' : '-';
        tmp <<= 3;
    }
    if (mode & S_ISUID)
        mode_buf[2] = 's';
    if (mode & S_ISGID)
        mode_buf[5] = 's';
    print(std::string_view(mode_buf.data(), mode_buf.size()));
}

/**
 * @brief Print a long integer with optional padding.
 *
 * @param pad Minimum field width.
 *
 * @param number Value to output.
 */
void Archiver::litoa(int pad, long number) {
    std::array<char, 11> num_buf{};
    long pow = 1'000'000'000L;
    bool digit_seen = false;
    for (int i = 0; i < 10; ++i) {
        auto digit = number / pow;
        if (digit == 0 && !digit_seen && i != 9) {
            num_buf[i] = ' ';
        } else {
            num_buf[i] = '0' + static_cast<char>(digit);
            number -= digit * pow;
            digit_seen = true;
        }
        pow /= 10;
    }
    auto start = std::ranges::find_if_not(num_buf, [](char c) { return c == ' '; });
    print(std::string_view(start, num_buf.end()));
}

/**
 * @brief Print a timestamp in a human-readable form.
 *
 * @param t Seconds since the Unix
 * epoch.
 */
void Archiver::date(long t) {
    constexpr int mo[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    constexpr std::string_view moname[] = {" Jan ", " Feb ", " Mar ", " Apr ", " May ", " Jun ",
                                           " Jul ", " Aug ", " Sep ", " Oct ", " Nov ", " Dec "};
    int year = 1970;
    long original = t;
    while (t > 0) {
        long length = (year % 4 == 0 ? 366L * 24 * 3600 : 365L * 24 * 3600);
        if (t < length)
            break;
        t -= length;
        ++year;
    }
    int day = static_cast<int>(t / (24 * 3600));
    t -= static_cast<long>(day) * 24 * 3600;
    int hour = static_cast<int>(t / 3600);
    t -= static_cast<long>(hour) * 3600;
    int minute = static_cast<int>(t / 60);
    int month = 0;
    auto mo_adj = mo;
    mo_adj[1] = (year % 4 == 0 ? 29 : 28);
    for (int i = 0; day >= mo_adj[i]; ++i) {
        ++month;
        day -= mo_adj[i];
    }
    print(moname[month]);
    ++day;
    if (day < 10)
        print(" ");
    litoa(0, day);
    print(" ");
    if (time(nullptr) - original >= (365L * 24 * 3600) / 2) {
        litoa(1, year);
    } else {
        if (hour < 10)
            print("0");
        litoa(0, hour);
        print(":");
        if (minute < 10)
            print("0");
        litoa(0, minute);
    }
    print(" ");
}

/**
 * @brief Create a unique temporary archive filename.
 */
void Archiver::mktempname() {
    std::lock_guard lock(mtx_);
    auto result =
        std::format_to_n(temp_arch_.begin(), temp_arch_.size() - 1, "/tmp/ar.{:05d}", getpid());
    temp_arch_[result.out - temp_arch_.begin()] = '\0';
}

/**
 * @brief Add a file to an archive.
 *
 * @param name Source filename.
 * @param fd Target file
 * descriptor.
 * @param mess Operation indicator character.
 */
void Archiver::add(std::string_view name, int fd, char mess) {
    std::lock_guard lock(mtx_);
    struct stat status;
    if (stat(name.data(), &status) < 0) {
        error(false, std::format("Cannot find {}", name));
        return;
    }
    int src_fd = ::open(name.data(), O_RDONLY);
    if (src_fd < 0) {
        error(false, std::format("Cannot open {}", name));
        return;
    }
    Member member{};
    auto base = ar_basename(name);
    base.copy(member.m_name.data(), std::min(base.size(), config::NAME_SIZE));
    member.m_uid = status.st_uid;
    member.m_gid = status.st_gid;
    member.m_mode = status.st_mode & 07777;
    swap(status.st_mtime);
    member.m_time_1 = swap(status.st_mtime).mem_1;
    member.m_time_2 = swap(status.st_mtime).mem_2;
    swap(status.st_size);
    member.m_size_1 = swap(status.st_size).mem_1;
    member.m_size_2 = swap(status.st_size).mem_2;
    safe_write(fd, &member, sizeof(Member));
    int read_chars;
    while ((read_chars = ::read(src_fd, io_buffer_.data(), config::IO_SIZE)) > 0) {
        safe_write(fd, io_buffer_.data(), read_chars);
    }
    if (odd(status.st_size)) {
        safe_write(fd, io_buffer_.data(), 1);
    }
    if (verbose_)
        show(mess, name);
    ::close(src_fd);
}

/**
 * @brief Extract an archive member to the filesystem.
 *
 * @param member Member metadata to
 * extract.
 */
void Archiver::extract(const Member &member) {
    std::lock_guard lock(mtx_);
    int fd = descriptors::STDOUT;
    if (!pr_fl_) {
        fd = ::creat(member.m_name.data(), 0644);
        if (fd < 0) {
            error(false, std::format("Cannot create {}", member.m_name.data()));
            return;
        }
    }
    if (verbose_ && !pr_fl_)
        show('x', member.m_name.data());
    copy_member(member, ar_fd_, fd);
    if (fd != descriptors::STDOUT) {
        ::close(fd);
        ::chmod(member.m_name.data(), member.m_mode);
    }
}

/**
 * @brief Copy member data between file descriptors.
 *
 * @param member Member metadata
 * describing size.
 * @param from Source descriptor.
 * @param to Destination descriptor.
 */
void Archiver::copy_member(const Member &member, int from, int to) {
    std::lock_guard lock(mtx_);
    long rest = mem_size_;
    bool is_odd = odd(mem_size_);
    while (rest > 0) {
        int chunk =
            rest > static_cast<long>(config::IO_SIZE) ? config::IO_SIZE : static_cast<int>(rest);
        if (::read(from, io_buffer_.data(), chunk) != chunk) {
            error(true, std::format("Read error on {}", member.m_name.data()));
        }
        safe_write(to, io_buffer_.data(), chunk);
        rest -= chunk;
    }
    if (is_odd) {
        ::lseek(from, 1L, SEEK_CUR);
        if (rep_fl_ || del_fl_)
            ::lseek(to, 1L, SEEK_CUR);
    }
}

/**
 * @brief Parse command line arguments and perform archive operations.
 *
 * @param argc
 * Argument count.
 * @param argv Argument vector.
 */
void Archiver::process(int argc, char *argv[]) {
    if (argc < 3)
        usage();
    std::string_view flags{argv[1]};
    for (char flag : flags) {
        switch (flag) {
        case 't':
            show_fl_ = true;
            break;
        case 'v':
            verbose_ = true;
            break;
        case 'x':
            ex_fl_ = true;
            break;
        case 'a':
            app_fl_ = true;
            break;
        case 'p':
            pr_fl_ = true;
            break;
        case 'd':
            del_fl_ = true;
            break;
        case 'r':
            rep_fl_ = true;
            break;
        default:
            usage();
        }
    }
    if (static_cast<int>(app_fl_) + static_cast<int>(ex_fl_) + static_cast<int>(del_fl_) +
            static_cast<int>(rep_fl_) + static_cast<int>(show_fl_) + static_cast<int>(pr_fl_) !=
        1) {
        usage();
    }
    if (rep_fl_ || del_fl_)
        mktempname();
    std::signal(SIGINT, cleanup_handler);
    instance_ = this;

    ar_fd_ = open_archive(argv[2], (show_fl_ || pr_fl_) ? FileMode::READ : FileMode::APPEND);
    if (rep_fl_ || del_fl_) {
        temp_fd_ = open_archive(temp_arch_.data(), FileMode::CREATE);
    }

    std::vector<std::string_view> files(argv + 3, argv + argc);
    Member *member;
    while ((member = get_member()) != nullptr) {
        bool matched = false;
        if (!files.empty()) {
            for (auto &file : files) {
                if (file.empty())
                    continue;
                if (equal(ar_basename(file), member->m_name)) {
                    matched = true;
                    break;
                }
            }
            if (!matched || app_fl_) {
                if (rep_fl_ || del_fl_) {
                    safe_write(temp_fd_, member, sizeof(Member));
                    copy_member(*member, ar_fd_, temp_fd_);
                } else {
                    if (app_fl_ && matched) {
                        print(
                            std::format("{}: already in archive.\n", files[&file - files.data()]));
                        files[&file - files.data()] = "";
                    }
                    ::lseek(ar_fd_, even(mem_size_), SEEK_CUR);
                }
                continue;
            }
        }
        if (ex_fl_ || pr_fl_) {
            extract(*member);
            files[&file - files.data()] = "";
        } else if (show_fl_) {
            if (verbose_) {
                print_mode(member->m_mode);
                if (member->m_uid < 10)
                    print(" ");
                litoa(0, member->m_uid);
                print("/");
                litoa(0, member->m_gid);
                litoa(8, mem_size_);
                date(mem_time_);
            }
            p_name(member->m_name.data());
            print("\n");
            ::lseek(ar_fd_, even(mem_size_), SEEK_CUR);
        } else if (del_fl_) {
            show('d', member->m_name.data());
            ::lseek(ar_fd_, even(mem_size_), SEEK_CUR);
        }
    }

    for (auto &file : files) {
        if (!file.empty()) {
            if (app_fl_) {
                add(file, ar_fd_, 'a');
            } else if (rep_fl_) {
                add(file, temp_fd_, 'a');
            } else {
                print(std::format("{}: not found\n", file));
            }
        }
    }

    flush();
    if (rep_fl_ || del_fl_) {
        std::signal(SIGINT, SIG_IGN);
        ::close(ar_fd_);
        ::close(temp_fd_);
        ar_fd_ = open_archive(argv[2], FileMode::CREATE);
        temp_fd_ = open_archive(temp_arch_.data(), FileMode::APPEND);
        int read_chars;
        while ((read_chars = ::read(temp_fd_, io_buffer_.data(), config::IO_SIZE)) > 0) {
            safe_write(ar_fd_, io_buffer_.data(), read_chars);
        }
        ::close(temp_fd_);
        std::filesystem::remove(temp_arch_);
    }
    ::close(ar_fd_);
}

/**
 * @brief Entry point for the ar utility.
 * @param argc Number of command-line arguments as per C++23 [basic.start.main].
 * @param argv Array of command-line argument strings.
 * @return Exit status as specified by C++23 [basic.start.main].
 */
int main(int argc, char *argv[]) {
    try {
        Archiver archiver;
        archiver.process(argc, argv);
        return 0;
    } catch (const std::exception &e) {
        std::cerr << std::format("ar: Fatal error: {}\n", e.what());
        return 1;
    } catch (...) {
        std::cerr << "ar: Unknown fatal error occurred\n";
        return 1;
    }
}
