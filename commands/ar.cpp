/* ar - archiver		Author: Michiel Huisjes */

/*
 * Usage: ar [adprtvx] archive [file] ...
 *	  v: verbose
 *	  x: extract
 *	  a: append
 *	  r: replace (append when not in archive)
 *	  d: delete
 *	  t: print contents of archive
 *	  p: print named files
 */

#include "signal.hpp"
#include "stat.hpp"
#include <array>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* Modern constants and helpers */
constexpr int MAGIC_NUMBER = 0177545;

// Forward declarations
static void get(int argc, char *argv[]);
static void error(bool quit, const char *str1, const char *str2);
static char *ar_basename(char *path);
static int open_archive(const char *name, int mode);
static MEMBER *get_member();
static long swap(union swabber *sw_ptr);
static ssize_t safe_write(int fd, const void *buffer, size_t size);

/* Determine if a number is odd */
[[nodiscard]] constexpr bool odd(int nr) { return nr & 0x01; }

/* Round a number up to the next even value */
[[nodiscard]] constexpr int even(int nr) { return odd(nr) ? nr + 1 : nr; }

union swabber {
    struct sw {
        short mem_1;
        short mem_2;
    } mem;
    long joined;
} swapped;

long swap();

// Changed from anonymous struct typedef to named struct
struct MEMBER {
    char m_name[14];
    short m_time_1;
    short m_time_2;
    char m_uid;
    char m_gid;
    short m_mode;
    short m_size_1;
    short m_size_2;
};

// Modern type aliases for better clarity
using BOOL = bool;
constexpr bool FALSE = false;
constexpr bool TRUE = true;

// File operation modes
constexpr int READ = 0;
constexpr int APPEND = 2;
constexpr int CREATE = 1;

// Modern null pointer constants (avoid macro conflicts)
constexpr MEMBER *NIL_MEM = nullptr;
constexpr long *NIL_LONG = nullptr;

// Buffer size constants
constexpr std::size_t IO_SIZE = 10 * 1024;
constexpr std::size_t BLOCK_SIZE = 1024;

namespace {
/**
     * @brief Index into the @ref terminal buffer.
     *
     * Tracks the number of characters
 * currently stored in the buffer
     * awaiting output.
     */
std::size_t terminal_index{0};
} // namespace

/**
 * @brief Flush the terminal output buffer.
 *
 * Writes the buffered data to standard output
 * using ::safe_write
 * and resets the internal index used by ::print.
 */
inline void flush();

/**
 * @brief Compare two strings for equality (first 14 characters)
 * @param str1 First string to compare
 * @param str2 Second string to compare
 * @return true if strings are equal, false otherwise
 */
[[nodiscard]] inline bool equal(const char *str1, const char *str2) {
    return !std::strncmp(str1, str2, 14);
}

// Global operation flags - consider refactoring into a struct
bool verbose{false};
bool app_fl{false};
bool ex_fl{false};
bool show_fl{false};
bool pr_fl{false};
bool rep_fl{false};
bool del_fl{false};

// File descriptor and archive metadata
int ar_fd{-1};
long mem_time{0};
long mem_size{0};

// I/O buffers with proper initialization
std::array<char, IO_SIZE> io_buffer{};
std::array<char, BLOCK_SIZE> terminal{};

// Temporary archive path template
char temp_arch[] = "/tmp/ar.XXXXX";

/**
 * @brief Display usage information and exit
 */
[[noreturn]] static void usage() { error(true, "Usage: ar [adprtxv] archive [file] ...", nullptr); }

/**
 * @brief Display error message and optionally exit
 * @param quit If true, clean up and exit with error code
 * @param str1 Primary error message
 * @param str2 Optional secondary error message
 */
[[noreturn]] static void error(bool quit, const char *str1, const char *str2) {
    const auto len1 = std::strlen(str1);
    write(2, str1, len1);

    if (str2 != nullptr) {
        const auto len2 = std::strlen(str2);
        write(2, str2, len2);
    }

    write(2, "\n", 1);

    if (quit) {
        std::unlink(temp_arch);
        std::exit(1);
    }
}

/**
 * @brief Extract basename from a file path
 * @param path File path to process
 * @return Pointer to the basename portion of the path
 */
[[nodiscard]] static char *ar_basename(char *path) {
    char *ptr = path;
    char *last = nullptr;

    while (*ptr != '\0') {
        if (*ptr == '/') {
            last = ptr;
        }
        ptr++;
    }

    if (last == nullptr) {
        return path;
    }

    if (*(last + 1) == '\0') {
        *last = '\0';
        return ar_basename(path);
    }

    return last + 1;
}

/**
 * @brief Open or create an archive file
 * @param name Archive file name
 * @param mode File access mode (READ, APPEND, CREATE)
 * @return File descriptor for the opened archive
 */
[[nodiscard]] static int open_archive(const char *name, int mode) {
    constexpr unsigned short magic = MAGIC_NUMBER;
    int fd{-1};

    if (mode == CREATE) {
        fd = ::creat(name, 0644);
        if (fd < 0) {
            error(true, "Cannot create ", name);
        }

        const auto bytes_written = ::write(fd, &magic, sizeof(magic));
        if (bytes_written != sizeof(magic)) {
            error(true, "Failed to write magic number to ", name);
        }
        return fd;
    }

    fd = ::open(name, mode);
    if (fd < 0) {
        if (mode == APPEND) {
            ::close(open_archive(name, CREATE));
            error(false, "ar: creating ", name);
            return open_archive(name, APPEND);
        }
        error(true, "Cannot open ", name);
    }

    // Reset to beginning and validate magic number
    ::lseek(fd, 0L, SEEK_SET);

    unsigned short read_magic{0};
    const auto bytes_read = ::read(fd, &read_magic, sizeof(read_magic));

    if (bytes_read != sizeof(read_magic) || read_magic != magic) {
        error(true, name, " is not in ar format.");
    }

    return fd;
}

/**
 * @brief Signal handler for cleanup on interrupt
 */
static void cleanup_handler(int sig) {
    static_cast<void>(sig); // Suppress unused parameter warning
    ::unlink(temp_arch);
    std::exit(2);
}

/**
 * @brief Safe write wrapper for archive operations
 * @param fd File descriptor to write to
 * @param buffer Data buffer to write
 * @param size Number of bytes to write
 * @return Number of bytes written, or -1 on error
 */
[[nodiscard]] static ssize_t safe_write(int fd, const void *buffer, size_t size) {
    const auto bytes_written = ::write(fd, buffer, size);
    if (bytes_written != static_cast<ssize_t>(size)) {
        error(true, "Write operation failed", nullptr);
    }
    return bytes_written;
}

/**
 * @brief Main entry point for the archive utility
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return Exit status (0 for success, non-zero for error)
 *
 * Parses command line flags and executes the appropriate archive operation.
 * Supported operations: append (a), delete (d), print (p), replace (r),
 * show contents (t), verbose (v), extract (x).
 */
int main(int argc, char *argv[]) {
    // Validate minimum arguments
    if (argc < 3) {
        usage();
    }

    // Parse command flags from argv[1]
    const std::string_view flags{argv[1]};

    // Process each flag character
    for (const char flag : flags) {
        switch (flag) {
        case 't':
            show_fl = true;
            break;
        case 'v':
            verbose = true;
            break;
        case 'x':
            ex_fl = true;
            break;
        case 'a':
            app_fl = true;
            break;
        case 'p':
            pr_fl = true;
            break;
        case 'd':
            del_fl = true;
            break;
        case 'r':
            rep_fl = true;
            break;
        default:
            usage();
        }
    }

    // Ensure exactly one operation is specified
    const int operation_count = static_cast<int>(app_fl) + static_cast<int>(ex_fl) +
                                static_cast<int>(del_fl) + static_cast<int>(rep_fl) +
                                static_cast<int>(show_fl) + static_cast<int>(pr_fl);

    if (operation_count != 1) {
        usage();
    }

    // Generate temporary archive name for operations that need it
    if (rep_fl || del_fl) {
        const int pid = getpid();
        constexpr int base_offset = 8;
        std::sprintf(&temp_arch[base_offset], "%05d", pid);
    } // Set up signal handler for cleanup
    std::signal(SIGINT, cleanup_handler);

    // Execute the archive operation
    get(argc, argv);

    return 0;
}

/**
 * @brief Read the next member header from the archive
 * @return Pointer to member structure, or nullptr if end of archive
 *
 * Reads a MEMBER structure from the archive file descriptor and validates
 * the read operation. Updates global mem_time and mem_size variables.
 */
[[nodiscard]] static MEMBER *get_member() {
    static MEMBER member{};

    const auto bytes_read = read(ar_fd, &member, sizeof(MEMBER));

    if (bytes_read == 0) {
        return nullptr; // End of archive
    }

    if (bytes_read != sizeof(MEMBER)) {
        error(true, "Corrupted archive.", nullptr);
    }

    // Update global time and size from member data
    mem_time = swap(&(member.m_time_1));
    mem_size = swap(&(member.m_size_1));

    return &member;
}

/**
 * @brief Swap byte order for cross-platform compatibility
 * @param sw_ptr Pointer to swabber union containing the data to swap
 * @return The swapped long value
 */
[[nodiscard]] static long swap(union swabber *sw_ptr) {
    swapped.mem.mem_1 = (sw_ptr->mem).mem_2;
    swapped.mem.mem_2 = (sw_ptr->mem).mem_1;
    return swapped.joined;
}

/**
 * @brief Process the archive based on command line options.
 *
 * Iterates over each archive
 * member and dispatches actions such as
 * extraction, deletion, replacement, appending, or listing
 * according
 * to the flags supplied in @p argv.
 *
 * @param argc Number of command line
 * arguments.
 * @param argv Argument vector containing the operation flags and file names.
 * Iterates over each archive member and dispatches actions such as extraction, deletion, replacement, appending, or listing according to the flags supplied in @p argv.
 *
 * @param argc Number of command line arguments.
 * @param argv Argument vector containing the operation flags and file names.
 */
static void get(int argc, char *argv[]) {
    register MEMBER *member;
    int i = 0;
    int temp_fd, read_chars;

    ar_fd = open_archive(argv[2], (show_fl || pr_fl) ? READ : APPEND);
    if (rep_fl || del_fl)
        temp_fd = open_archive(temp_arch, CREATE);
    while ((member = get_member()) != NIL_MEM) {
        if (argc > 3) {
            for (i = 3; i < argc; i++) {
                if (equal(basename(argv[i]), member->m_name))
                    break;
            }
            if (i == argc || app_fl) {
                if (rep_fl || del_fl) {
                    mwrite(temp_fd, member, sizeof(MEMBER));
                    copy_member(member, ar_fd, temp_fd);
                } else {
                    if (app_fl && i != argc) {
                        print(argv[i]);
                        print(": already in archive.\n");
                        argv[i] = "";
                    }
                    (void)lseek(ar_fd, even(mem_size), 1);
                }
                continue;
            }
        }
        if (ex_fl || pr_fl)
            extract(member);
        else {
            if (rep_fl)
                add(argv[i], temp_fd, 'r');
            else if (show_fl) {
                if (verbose) {
                    print_mode(member->m_mode);
                    if (member->m_uid < 10)
                        print(" ");
                    litoa(0, (long)member->m_uid);
                    print("/");
                    litoa(0, (long)member->m_gid);
                    litoa(8, mem_size);
                    date(mem_time);
                }
                p_name(member->m_name);
                print("\n");
            } else if (del_fl)
                show('d', member->m_name);
            (void)lseek(ar_fd, even(mem_size), 1);
        }
        argv[i] = "";
    }

    if (argc > 3) {
        for (i = 3; i < argc; i++)
            if (argv[i][0] != '\0') {
                if (app_fl)
                    add(argv[i], ar_fd, 'a');
                else if (rep_fl)
                    add(argv[i], temp_fd, 'a');
                else {
                    print(argv[i]);
                    print(": not found\n");
                }
            }
    }

    flush();

    if (rep_fl || del_fl) {
        signal(SIGINT, SIG_IGN);
        (void)close(ar_fd);
        (void)close(temp_fd);
        ar_fd = open_archive(argv[2], CREATE);
        temp_fd = open_archive(temp_arch, APPEND);
        while ((read_chars = read(temp_fd, io_buffer.data(), IO_SIZE)) > 0)
            mwrite(ar_fd, io_buffer.data(), read_chars);
        (void)close(temp_fd);
        (void)unlink(temp_arch);
    }
    (void)close(ar_fd);
}

/**
 * @brief Add or replace a file within the archive.
 *
 * Reads the specified file from the host
 * filesystem and writes its
 * metadata and contents into the archive referenced by @p fd.
 *
 *
 * @param name Path to the source file being archived.
 * @param fd File descriptor of the archive
 * being written.
 * @param mess Character indicating operation context ('a' for append,
 * 'r' for
 * replace).
 */
static void add(char *name, int fd, char mess) {
    static MEMBER member;
    register int read_chars;
    struct stat status;
    int src_fd;

    if (stat(name, &status) < 0) {
        error(FALSE, "Cannot find ", name);
        return;
    } else if ((src_fd = open(name, 0)) < 0) {
        error(FALSE, "Cannot open ", name);
        return;
    }

    strcpy(member.m_name, basename(name));
    member.m_uid = status.st_uid;
    member.m_gid = status.st_gid;
    member.m_mode = status.st_mode & 07777;
    (void)swap(&(status.st_mtime));
    member.m_time_1 = swapped.mem.mem_1;
    member.m_time_2 = swapped.mem.mem_2;
    (void)swap(&(status.st_size));
    member.m_size_1 = swapped.mem.mem_1;
    member.m_size_2 = swapped.mem.mem_2;
    mwrite(fd, &member, sizeof(MEMBER));
    while ((read_chars = read(src_fd, io_buffer.data(), IO_SIZE)) > 0)
        mwrite(fd, io_buffer.data(), read_chars);

    if (odd(status.st_size))
        mwrite(fd, io_buffer.data(), 1);

    if (verbose)
        show(mess, name);
    (void)close(src_fd);
}

/**
 * @brief Extract a member from the archive.
 *
 * Creates the output file (unless printing to
 * standard output) and
 * copies the archived contents into it, restoring the recorded
 *
 * permissions.
 *
 * @param member Pointer to the archive member metadata.
 */
static void extract(MEMBER *member) {
    int fd = 1;

    if (pr_fl == FALSE && (fd = creat(member->m_name, 0644)) < 0) {
        error(FALSE, "Cannot create ", member->m_name);
        return;
    }

    if (verbose && pr_fl == FALSE)
        show('x', member->m_name);

    copy_member(member, ar_fd, fd);

    if (fd != 1)
        (void)close(fd);
    (void)chmod(member->m_name, member->m_mode);
}

/**
 * @brief Copy a member's contents between file descriptors.
 *
 * Performs buffered I/O from @p
 * from to @p to for the number of bytes
 * indicated by the global @ref mem_size variable.
 *
 *
 * @param member Pointer to the member being copied (used for error messages).
 * @param from Source
 * file descriptor.
 * @param to Destination file descriptor.
 */
static void copy_member(MEMBER *member, int from, int to) {
    register int rest;
    BOOL is_odd = odd(mem_size) ? TRUE : FALSE;

    do {
        rest = mem_size > (long)IO_SIZE ? IO_SIZE : (int)mem_size;
        if (read(from, io_buffer.data(), rest) != rest)
            error(TRUE, "Read error on ", member->m_name);
        mwrite(to, io_buffer.data(), rest);
        mem_size -= (long)rest;
    } while (mem_size != 0L);

    if (is_odd) {
        (void)lseek(from, 1L, 1);
        if (rep_fl || del_fl)
            (void)lseek(to, 1L, 1);
    }
}

/**
 * @brief Write buffered output.
 *
 * When called with @c nullptr the buffer contents are
 * flushed
 * immediately using ::safe_write. Otherwise the characters are
 * appended to the buffer
 * and written out once the buffer becomes
 * full.
 *
 * @param str Null-terminated string or @c
 * nullptr to flush the buffer.
 */
static void print(char *str) {
    if (str == NIL_PTR) {
        safe_write(1, terminal.data(), terminal_index);
        terminal_index = 0;
        return;
    }

    while (*str != '\0') {
        terminal[terminal_index++] = *str++;
        if (terminal_index == BLOCK_SIZE) {
            flush();
        }
    }
}
/**
 * @brief Flush buffered output to the terminal.
 *
 * Writes the accumulated characters using
 * @ref safe_write and
 * resets the @c terminal_index so that subsequent calls to
 * ::print begin
 * writing at the start of the buffer.
 */
inline void flush() {
    if (terminal_index != 0) {
        safe_write(1, terminal.data(), terminal_index);
        terminal_index = 0;
    }
}

/**
 * @brief Print a symbolic representation of POSIX file modes.
 *
 * Outputs a string such as
 * ``rwxr-xr-x`` to the archive's terminal
 * buffer.
 *
 * @param mode File mode bits to
 * represent.
 */
static void print_mode(int mode) {
    static char mode_buf[11];
    register int tmp = mode;
    int i;

    mode_buf[9] = ' ';
    for (i = 0; i < 3; i++) {
        mode_buf[i * 3] = (tmp & S_IREAD) ? 'r' : '-';
        mode_buf[i * 3 + 1] = (tmp & S_IWRITE) ? 'w' : '-';
        mode_buf[i * 3 + 2] = (tmp & S_IEXEC) ? 'x' : '-';
        tmp <<= 3;
    }
    if (mode & S_ISUID)
        mode_buf[2] = 's';
    if (mode & S_ISGID)
        mode_buf[5] = 's';
    print(mode_buf);
}

/**
 * @brief Print a long integer using buffered output.
 *
 * The number is converted to decimal
 * and written via ::print with
 * optional padding spaces.
 *
 * @param pad Minimum field width to
 * pad.
 * @param number Value to convert and output.
 */
static void litoa(int pad, long number) {
    static char num_buf[11];
    register long digit;
    register long pow = 1000000000L;
    int digit_seen = FALSE;
    int i;

    for (i = 0; i < 10; i++) {
        digit = number / pow;
        if (digit == 0L && digit_seen == FALSE && i != 9)
            num_buf[i] = ' ';
        else {
            num_buf[i] = '0' + (char)digit;
            number -= digit * pow;
            digit_seen = TRUE;
        }
        pow /= 10L;
    }

    for (i = 0; num_buf[i] == ' ' && i + pad < 11; i++)
        ;
    print(&num_buf[i]);
}

/**
 * @brief Write bytes to a file descriptor with error checking.
 *
 * Invokes ::write and
 * terminates the program if the write fails.
 *
 * @param fd Destination file descriptor.
 * @param
 * address Buffer containing the data to write.
 * @param bytes Number of bytes to write from the
 * buffer.
 */
static void mwrite(int fd, char *address, int bytes) {
    if (write(fd, address, bytes) != bytes)
        error(TRUE, "Write error.", NIL_PTR);
}

/**
 * @brief Emit a one-line status message for a file operation.
 *
 * Prints messages such as "x
 * - filename" to standard output.
 *
 * @param c Operation character (e.g., 'x' for extract).
 *
 * @param name Name of the file being operated on.
 */
static void show(char c, char *name) {
    write(1, &c, 1);
    write(1, " - ", 3);
    write(1, name, strlen(name));
    write(1, "\n", 1);
}

/**
 * @brief Print a member name from an archive header.
 *
 * The name stored in the archive
 * header is not null terminated; this
 * function copies it into a temporary buffer before output.

 * *
 * @param mem_name Pointer to the 14-character member name field.
 */
static void p_name(char *mem_name) {
    register int i = 0;
    char name[15];

    for (i = 0; i < 14 && *mem_name; i++)
        name[i] = *mem_name++;

    name[i] = '\0';
    print(name);
}

#define MINUTE 60L
#define HOUR (60L * MINUTE)
#define DAY (24L * HOUR)
#define YEAR (365L * DAY)
#define LYEAR (366L * DAY)

int mo[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

char *moname[] = {" Jan ", " Feb ", " Mar ", " Apr ", " May ", " Jun ",
                  " Jul ", " Aug ", " Sep ", " Oct ", " Nov ", " Dec "};

/**
 * @brief Print a timestamp in ``Mon dd hh:mm`` or ``Mon dd  yyyy`` format.
 *
 * The
 * representation matches traditional UNIX ``ls`` output and is
 * valid for years 1970--2099.
 *
 *
 * @param t Epoch time to format.
 */
static void date(long t) {
    int i, year, day, month, hour, minute;
    long length, time(), original;

    year = 1970;
    original = t;
    while (t > 0) {
        length = (year % 4 == 0 ? LYEAR : YEAR);
        if (t < length)
            break;
        t -= length;
        year++;
    }

    /* Year has now been determined.  Now the rest. */
    day = (int)(t / DAY);
    t -= (long)day * DAY;
    hour = (int)(t / HOUR);
    t -= (long)hour * HOUR;
    minute = (int)(t / MINUTE);

    /* Determine the month and day of the month. */
    mo[1] = (year % 4 == 0 ? 29 : 28);
    month = 0;
    i = 0;
    while (day >= mo[i]) {
        month++;
        day -= mo[i];
        i++;
    }

    /* At this point, 'year', 'month', 'day', 'hour', 'minute'  ok */
    print(moname[month]);
    day++;
    if (day < 10)
        print(" ");
    litoa(0, (long)day);
    print(" ");
    if (time(NIL_LONG) - original >= YEAR / 2L)
        litoa(1, (long)year);
    else {
        if (hour < 10)
            print("0");
        litoa(0, (long)hour);
        print(":");
        if (minute < 10)
            print("0");
        litoa(0, (long)minute);
    }
    print(" ");
}
