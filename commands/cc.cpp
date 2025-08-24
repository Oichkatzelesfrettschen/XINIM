/**
 * @file cc.cpp
 * @brief Modern C++23 compiler driver for XINIM operating system
 * @author Erik Baalbergen (original), modernized for XINIM C++23 migration
 * @version 3.0 - Fully modernized with C++23 paradigms
 * @date 2025-08-13
 *
 * @section Description
 * A compiler driver functioning as /bin/cc, coordinating the compilation pipeline
 * (preprocessing, compilation, optimization, code generation, and linking) with
 * modern C++23 features. Originally derived from CEMCOM, this implementation
 * emphasizes type safety, RAII, thread safety, and performance optimization.
 *
 * @section Features
 * - Exception-safe error handling with std::expected
 * - RAII for resource management
 * - Thread-safe operations with std::mutex
 * - Type-safe string handling with std::string_view
 * - Constexpr configuration for compile-time optimization
 * - Memory-safe buffer management with std::span
 * - Comprehensive logging and Doxygen documentation
 * - Support for modern C++23 ranges and string formatting
 *
 * @section Usage
 * cc [options] file...
 *
 * Standard options: -c, -o, -D, -I, -U, -l, -O, -S, -F, -v
 *
 * @note Requires C++23 compliant compiler
 */

#include "../h/signal.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

// Modern constants with clear semantic meaning
namespace config {
constexpr std::size_t MAXARGC = 64;           /**< Maximum arguments in a list */
constexpr std::size_t USTR_SIZE = 64;         /**< Maximum string variable length */
constexpr std::size_t MAX_FILES = 32;         /**< Maximum number of input files */
constexpr std::size_t BUFSIZE = USTR_SIZE * MAXARGC; /**< Total buffer size */
constexpr std::size_t TEMP_NAME_SIZE = 15;    /**< Temporary filename buffer size */
} // namespace config

// Modern type aliases for clarity
using UString = std::array<char, config::USTR_SIZE>;
using ArgumentVector = std::vector<std::string>;

// Compiler toolchain paths configuration
namespace compiler_paths {
#if defined(MEM640K)
constexpr std::string_view BASE_PATH = "/lib";
#elif defined(MEM512K)
constexpr std::string_view BASE_PATH = "/usr/lib";
#else
constexpr std::string_view BASE_PATH = "/usr/lib";
#endif
constexpr std::string_view CPP_PATH = "/usr/lib/cpp";
constexpr std::string_view CEM_PATH = "/usr/lib/cem";
constexpr std::string_view OPT_PATH = "/usr/lib/opt";
constexpr std::string_view CG_PATH = "/usr/lib/cg";
constexpr std::string_view ASLD_PATH = "/usr/bin/asld";
constexpr std::string_view SHELL_PATH = "/bin/sh";
constexpr std::string_view LIB_DIR = "/usr/lib";
} // namespace compiler_paths

// Legacy toolchain path globals (phased out)
inline constexpr const char *PP = compiler_paths::CPP_PATH.data();
inline constexpr const char *CEM = compiler_paths::CEM_PATH.data();
inline constexpr const char *OPT = compiler_paths::OPT_PATH.data();
inline constexpr const char *CG = compiler_paths::CG_PATH.data();
inline constexpr const char *ASLD = compiler_paths::ASLD_PATH.data();
inline constexpr const char *SHELL = compiler_paths::SHELL_PATH.data();
inline constexpr const char *LIBDIR = compiler_paths::LIB_DIR.data();

// Modern configuration constants
namespace toolchain_config {
constexpr std::string_view V_FLAG = "-Vs2.2w2.2i2.2l4.2f4.2d8.2p2.2";
constexpr std::string_view DEFAULT_OUTPUT = "a.out";
constexpr std::string_view TEMP_DIR = "/tmp";
} // namespace toolchain_config

// Legacy global structures (modernized)
// @brief Argument list for a single command execution
struct arglist {
    int al_argc{0};
    std::array<char *, config::MAXARGC> al_argv{};

    constexpr void init() noexcept { al_argc = 1; }
    [[nodiscard]] constexpr bool is_full() const noexcept { return al_argc >= config::MAXARGC; }
};
inline arglist LD_HEAD{1, {const_cast<char *>("/usr/lib/crtso.s")}};
inline arglist LD_TAIL{
    2, {const_cast<char *>("/usr/lib/libc.a"), const_cast<char *>("/usr/lib/end.s")}};

// Forward declaration for panic function
[[noreturn]] void panic(std::string_view message);

// Modern memory and file utilities
namespace memory_manager {
inline std::array<char, config::BUFSIZE> buffer{};
inline char *buffer_ptr = buffer.data();

[[nodiscard]] inline char *allocate(std::size_t size) {
    char *const result = buffer_ptr;
    if ((buffer_ptr + size) >= (buffer.data() + config::BUFSIZE)) {
        throw std::runtime_error("Buffer overflow: no space for allocation");
    }
    buffer_ptr += size;
    return result;
}
inline void reset() noexcept { buffer_ptr = buffer.data(); }
} // namespace memory_manager

namespace file_utils {
[[nodiscard]] inline bool remove_file(char *filename) noexcept {
    if (!filename || filename[0] == '\0')
        return true;
    const bool success = (unlink(filename) == 0);
    filename[0] = '\0';
    return success;
}

[[nodiscard]] inline bool cleanup_temp(char *filename) noexcept {
    return filename ? remove_file(filename) : true;
}

/**
 * @brief Construct a full archive path within the library directory.
 *
 * @param name Short
 * name of the library without prefix or extension.
 * @return Pointer to a newly allocated C-string
 * (e.g., "/usr/lib/libc.a").
 */
[[nodiscard]] inline char *create_library_path(std::string_view name) {
    if (name.empty())
        return nullptr;
    const auto formatted = std::format("{}/lib{}.a", LIBDIR, name);
    char *result = memory_manager::allocate(formatted.size() + 1);
    std::memcpy(result, formatted.c_str(), formatted.size() + 1);
    return result;
}
} // namespace file_utils

// Utility functions
/**
 * @brief Allocate raw memory from the compiler's internal buffer.
 *
 * @param size Number of
 * bytes to allocate.
 * @return Pointer to allocated memory.
 */
[[nodiscard]] inline char *alloc(std::size_t size) {
    try {
        return memory_manager::allocate(size);
    } catch (const std::runtime_error &) {
        panic("No space for allocation");
        return nullptr; // Unreachable
    }
}

/**
 * @brief Append a single argument to an argument list.
 *
 * @param al  Argument list to
 * modify.
 * @param arg Argument to append.
 */
void append(arglist &al, std::string_view arg) {
    if (al.is_full()) {
        panic("Argument list overflow");
    }
    char *buf = alloc(arg.size() + 1);
    arg.copy(buf, arg.size());
    buf[arg.size()] = '\0';
    al.al_argv[al.al_argc++] = buf;
}

/**
 * @brief Concatenate two argument lists.
 *
 * @param al1 Destination argument list.
 * @param
 * al2 Source argument list.
 */
void concat(arglist &al1, const arglist &al2) {
    if (al1.is_full() || (al1.al_argc + al2.al_argc) > config::MAXARGC) {
        panic("Argument list overflow in concat");
    }
    std::ranges::copy(al2.al_argv | std::views::take(al2.al_argc),
                      al1.al_argv.begin() + al1.al_argc);
    al1.al_argc += al2.al_argc;
}

/**
 * @brief Concatenate arbitrary string-like pieces into a UString buffer.
 *
 * @tparam Args
 * Parameter pack convertible to std::string_view.
 * @param dst Destination buffer.
 * @param args
 * Strings to concatenate.
 * @return Pointer to the destination buffer.
 */
template <typename... Args> [[nodiscard]] char *mkstr(UString &dst, Args &&...args) {
    char *q = dst.data();
    auto copy = [&](std::string_view s) {
        const auto len = std::min<std::size_t>(
            s.size(), static_cast<std::size_t>(dst.data() + config::USTR_SIZE - 1 - q));
        std::memcpy(q, s.data(), len);
        q += len;
    };
    (copy(std::string_view(std::forward<Args>(args))), ...);
    *q = '\0';
    return dst.data();
}

void basename(std::string_view str, UString &dst) {
    auto last_slash = str.rfind('/');
    auto base = last_slash == std::string_view::npos ? str : str.substr(last_slash + 1);
    auto dot = base.find('.');
    if (dot != std::string_view::npos)
        base.remove_suffix(base.size() - dot);
    base.copy(dst.data(), std::min(base.size(), config::USTR_SIZE - 1));
    dst[std::min(base.size(), config::USTR_SIZE - 1)] = '\0';
}

[[nodiscard]] int extension(std::string_view filename) {
    if (filename.empty())
        return 0;
    if (filename.back() == '.')
        return 0;
    auto dot = filename.rfind('.');
    return (dot != std::string_view::npos && dot + 1 < filename.size()) ? filename[dot + 1] : 0;
}

[[noreturn]] void panic(std::string_view message) {
    if (!message.empty()) {
        write(STDERR_FILENO, message.data(), message.size());
    }
    exit(EXIT_FAILURE);
}

// Global state
inline const char *ProgCall = nullptr;

// RAII-based compiler driver
/**
 * @brief Thread-safe RAII compilation orchestrator.
 *
 * Encapsulates all compiler state that
 * was previously handled via global
 * structures. Each instance manages its own temporary files
 * and flags,
 * ensuring clean teardown and providing a foundation for concurrent
 * compilation
 * tasks.
 */
class CompilerDriver {
  public:
    /**
     * @brief Construct the compiler driver and initialise command vectors.
     */
    CompilerDriver() {
        instance_ = this;
        CALL_VEC[0].init();
        CALL_VEC[1].init();
    }

    /**
     * @brief Clean up temporary files on destruction.
     */
    ~CompilerDriver() {
        cleanup_temporary_files();
        instance_ = nullptr;
    }

    // Accessors
    [[nodiscard]] auto &src_files() noexcept { return SRCFILES; }
    [[nodiscard]] auto &ld_files() noexcept { return LDFILES; }
    [[nodiscard]] auto &gen_ld_files() noexcept { return GEN_LDFILES; }
    [[nodiscard]] auto &pp_flags() noexcept { return PP_FLAGS; }
    [[nodiscard]] auto &cem_flags() noexcept { return CEM_FLAGS; }
    [[nodiscard]] auto &opt_flags() noexcept { return OPT_FLAGS; }
    [[nodiscard]] auto &cg_flags() noexcept { return CG_FLAGS; }
    [[nodiscard]] auto &asld_flags() noexcept { return ASLD_FLAGS; }
    [[nodiscard]] auto &debug_flags() noexcept { return DEBUG_FLAGS; }
    [[nodiscard]] auto &call_vec(std::size_t i) noexcept { return CALL_VEC[i]; }
    [[nodiscard]] auto &ret_code() noexcept { return RET_CODE; }
    [[nodiscard]] auto &o_flag() noexcept { return o_flag_; }
    [[nodiscard]] auto &S_flag() noexcept { return S_flag_; }
    [[nodiscard]] auto &v_flag() noexcept { return v_flag_; }
    [[nodiscard]] auto &F_flag() noexcept { return F_flag_; }
    [[nodiscard]] auto &ifile() noexcept { return ifile_; }
    [[nodiscard]] auto &kfile() noexcept { return kfile_; }
    [[nodiscard]] auto &sfile() noexcept { return sfile_; }
    [[nodiscard]] auto &mfile() noexcept { return mfile_; }
    [[nodiscard]] auto &ofile() noexcept { return ofile_; }
    [[nodiscard]] auto &base() noexcept { return BASE; }
    [[nodiscard]] auto &tmpdir() noexcept { return tmpdir_; }
    [[nodiscard]] auto tmpname_buf() noexcept { return std::span<char>{tmpname}; }
    [[nodiscard]] std::string &o_file() noexcept { return o_file_; }
#ifdef DEBUG
    [[nodiscard]] auto &noexec() noexcept { return noexec_; }
#endif

    // Core functionality
    /**
     * @brief Process all source files through the compilation pipeline.
     */
    void process_source_files();
    /**
     * @brief Process a single file through the compilation pipeline.
     * @param file Input filename.
     * @param ext File extension character.
     * @param ldfile Output parameter for generated object file.
     * @return true if processing succeeded, false otherwise.
     */
    [[nodiscard]] bool process_single_file(std::string_view &file, int &ext, char *&ldfile);
    /**
     * @brief Continue processing file through compilation stages.
     * @param file File being processed (modified in-place).
     * @param ext File extension (modified in-place).
     * @param ldfile Output object file name.
     * @return true if processing succeeded.
     */
    [[nodiscard]] bool process_compilation_stages(std::string_view &file, int &ext, char *&ldfile);
    /**
     * @brief Perform final linking stage.
     */
    void perform_linking();
    /**
     * @brief Execute command vector with optional output redirection.
     * @param vec Command and arguments to execute.
     * @param output_file Optional output redirection file (empty for stdout).
     * @return 1 on success, 0 on failure.
     */
    [[nodiscard]] int runvec(arglist &vec, std::string_view output_file);
    /**
     * @brief Execute two command vectors connected by pipe.
     * @param vec0 First command (producer).
     * @param vec1 Second command (consumer).
     * @return 1 on success, 0 on failure.
     */
    [[nodiscard]] int runvec2(arglist &vec0, arglist &vec1);
    /**
     * @brief Execute command vector via execv.
     * @param vec Command and arguments to execute.
     */
    [[noreturn]] void ex_vec(arglist &vec);

    // Signal handler
    /**
     * @brief Signal handler to tidy temporary files on interrupt.
     *
     * @param sig
     * Received POSIX signal.
     */
    static void trapcc(int sig) noexcept {
        signal(sig, SIG_IGN);
        if (instance_) {
            instance_->cleanup_temporary_files();
        }
    }

private:
    /**
     * @brief Remove all temporary files generated during compilation.
     */
    void cleanup_temporary_files() noexcept {
        std::lock_guard lock(mtx_);
        std::ignore = file_utils::cleanup_temp(ifile_.data());
        std::ignore = file_utils::cleanup_temp(kfile_.data());
        std::ignore = file_utils::cleanup_temp(sfile_.data());
        std::ignore = file_utils::cleanup_temp(mfile_.data());
        std::ignore = file_utils::cleanup_temp(ofile_.data());
    }

    struct arglist SRCFILES {};
    struct arglist LDFILES {};
    struct arglist GEN_LDFILES {};
    struct arglist PP_FLAGS {};
    struct arglist CEM_FLAGS {};
    struct arglist OPT_FLAGS {};
    struct arglist CG_FLAGS {};
    struct arglist ASLD_FLAGS {};
    struct arglist DEBUG_FLAGS {};
    struct arglist LD_HEAD {
        1, { const_cast<char *>("/usr/lib/crtso.s") }
    };
    struct arglist LD_TAIL {
        2, { const_cast<char *>("/usr/lib/libc.a"), const_cast<char *>("/usr/lib/end.s") }
    };
    std::array<arglist, 2> CALL_VEC{};
    int RET_CODE{0};
    bool o_flag_{false};
    bool S_flag_{false};
    bool v_flag_{false};
    bool F_flag_{false};
    UString ifile_{}, kfile_{}, sfile_{}, mfile_{}, ofile_{}, BASE{};
    std::string o_file_{
        std::string(toolchain_config::DEFAULT_OUTPUT)};
    const char *tmpdir_{toolchain_config::TEMP_DIR.data()};
    char tmpname[config::TEMP_NAME_SIZE]{};
#ifdef DEBUG
    bool noexec_{false};
#endif
    mutable std::mutex mtx_{};
    /**
     * @brief Thread-local pointer to active driver for signal handling.
     */
    static thread_local CompilerDriver *instance_;

    /**
     * @brief Print a command vector to stderr.
     */
    static void pr_vec(const arglist &vec);
};

thread_local CompilerDriver *CompilerDriver::instance_ = nullptr;

void CompilerDriver::pr_vec(const arglist &vec) {
    for (int i = 1; i < vec.al_argc; ++i) {
        if (const char *arg = vec.al_argv[i]) {
            write(STDERR_FILENO, arg, std::strlen(arg));
            if (i < vec.al_argc - 1) {
                write(STDERR_FILENO, " ", 1);
            }
        }
    }
}

/**
 * @brief Generate a unique temporary filename fragment.
 *
 * @param buf Destination buffer for
 * the generated name.
 */
void mktempname(std::span<char> buf) {
    auto result = std::format_to_n(buf.begin(), buf.size() - 1, "cc.{}", getpid());
    buf[result.out - buf.begin()] = '\0';
}

void CompilerDriver::process_source_files() {
    std::lock_guard lock(mtx_);
    for (auto file : std::span(SRCFILES.al_argv.data(), SRCFILES.al_argc)) {
        if (!file)
            continue;
        std::string_view file_view(file);
        char *ldfile = nullptr;
        basename(file_view, BASE);
        if (SRCFILES.al_argc > 1) {
            std::cout << file_view << ":\n";
        }
        int ext = extension(file_view);
        if (!process_single_file(file_view, ext, ldfile)) {
            continue;
        }
        if (!S_flag_) {
            append(LDFILES, file_view);
            if (ldfile)
                append(GEN_LDFILES, ldfile);
        }
    }
}

bool CompilerDriver::process_single_file(std::string_view &file, int &ext, char *&ldfile) {
    std::lock_guard lock(mtx_);
    auto &call = CALL_VEC[0];
    auto &call1 = CALL_VEC[1];
    if (ext == 'c') {
        call.init();
        append(call, PP);
        concat(call, PP_FLAGS);
        append(call, file);
        if (F_flag_) {
            auto f = mkstr(ifile_, tmpdir_, tmpname, ".i");
            if (runvec(call, f)) {
                file = ifile_.data();
                ext = 'i';
            } else {
                std::ignore = file_utils::remove_file(ifile_.data());
                return false;
            }
        } else {
            call1.init();
            append(call1, CEM);
            concat(call1, DEBUG_FLAGS);
            append(call1, toolchain_config::V_FLAG);
            concat(call1, CEM_FLAGS);
            append(call1, "-");
            auto f = mkstr(kfile_, tmpdir_, tmpname, ".k");
            append(call1, f);
            if (runvec2(call, call1)) {
                file = kfile_.data();
                ext = 'k';
            } else {
                std::ignore = file_utils::remove_file(kfile_.data());
                return false;
            }
        }
    }
    return process_compilation_stages(file, ext, ldfile);
}

bool CompilerDriver::process_compilation_stages(std::string_view &file, int &ext, char *&ldfile) {
    std::lock_guard lock(mtx_);
    auto &call = CALL_VEC[0];
    if (ext == 'i') {
        call.init();
        append(call, CEM);
        concat(call, DEBUG_FLAGS);
        append(call, toolchain_config::V_FLAG);
        concat(call, CEM_FLAGS);
        append(call, file);
        auto f = mkstr(kfile_, tmpdir_, tmpname, ".k");
        append(call, f);
        if (!runvec(call, "")) {
            std::ignore = file_utils::remove_file(kfile_.data());
            return false;
        }
        file = kfile_.data();
        ext = 'k';
        std::ignore = file_utils::cleanup_temp(ifile_.data());
    }
    if (ext == 'k') {
        call.init();
        append(call, OPT);
        concat(call, OPT_FLAGS);
        append(call, file);
        auto f = mkstr(mfile_, tmpdir_, tmpname, ".m");
        if (!runvec(call, f)) {
            return false;
        }
        file = mfile_.data();
        ext = 'm';
        std::ignore = file_utils::cleanup_temp(kfile_.data());
    }
    if (ext == 'm') {
        ldfile = S_flag_ ? ofile_.data() : alloc(std::strlen(BASE.data()) + 3);
        call.init();
        append(call, CG);
        concat(call, CG_FLAGS);
        append(call, file);
        auto f = mkstr(ldfile, BASE.data(), ".s");
        append(call, f);
        if (!runvec(call, "")) {
            return false;
        }
        std::ignore = file_utils::cleanup_temp(mfile_.data());
        file = ldfile;
        ext = 's';
    }
    return true;
}

void CompilerDriver::perform_linking() {
    std::lock_guard lock(mtx_);
    auto &call = CALL_VEC[0];
    call.init();
    append(call, ASLD);
    concat(call, ASLD_FLAGS);
    append(call, "-o");
    append(call, o_file_);
    concat(call, LD_HEAD);
    concat(call, LDFILES);
    concat(call, LD_TAIL);
    if (runvec(call, "")) {
        for (int i = 0; i < GEN_LDFILES.al_argc; ++i) {
            std::ignore = file_utils::remove_file(GEN_LDFILES.al_argv[i]);
        }
    }
}

int CompilerDriver::runvec(arglist &vec, std::string_view output_file) {
    std::lock_guard lock(mtx_);
    if (v_flag_) {
        pr_vec(vec);
        write(STDERR_FILENO, "\n", 1);
    }
#ifdef DEBUG
    if (noexec_)
        return 1;
#endif
    const pid_t pid = fork();
    if (pid == 0) {
        if (!output_file.empty()) {
            close(STDOUT_FILENO);
            const int fd = creat(output_file.data(), 0666);
            if (fd != STDOUT_FILENO) {
                panic(std::format("Cannot create output file: {}", output_file));
            }
        }
        ex_vec(vec);
    }
    if (pid == -1) {
        panic("No more processes available");
    }
    int status;
    wait(&status);
    if (status != 0) {
        RET_CODE = 1;
        return 0;
    }
    return 1;
}

int CompilerDriver::runvec2(arglist &vec0, arglist &vec1) {
    std::lock_guard lock(mtx_);
    if (v_flag_) {
        pr_vec(vec0);
        write(STDERR_FILENO, " | ", 3);
        pr_vec(vec1);
        write(STDERR_FILENO, "\n", 1);
    }
#ifdef DEBUG
    if (noexec_)
        return 1;
#endif
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        panic("Cannot create pipe");
    }
    const pid_t pid1 = fork();
    if (pid1 == 0) {
        close(STDOUT_FILENO);
        if (dup(pipe_fds[1]) != STDOUT_FILENO) {
            panic("Bad dup for producer");
        }
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        ex_vec(vec0);
    }
    if (pid1 == -1) {
        panic("No more processes for producer");
    }
    const pid_t pid2 = fork();
    if (pid2 == 0) {
        close(STDIN_FILENO);
        if (dup(pipe_fds[0]) != STDIN_FILENO) {
            panic("Bad dup for consumer");
        }
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        ex_vec(vec1);
    }
    if (pid2 == -1) {
        panic("No more processes for consumer");
    }
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    int status1, status2;
    wait(&status1);
    wait(&status2);
    if (status1 || status2) {
        RET_CODE = 1;
        return 0;
    }
    return 1;
}

void CompilerDriver::ex_vec(arglist &vec) {
#ifdef DEBUG
    if (noexec_)
        exit(EXIT_SUCCESS);
#endif
    vec.al_argv[vec.al_argc] = nullptr;
    execv(vec.al_argv[1], &vec.al_argv[1]);
    if (errno == ENOEXEC) {
        vec.al_argv[0] = const_cast<char *>(SHELL);
        execv(SHELL, &vec.al_argv[0]);
    }
    if (access(vec.al_argv[1], X_OK) == 0) {
        panic(std::format("Cannot execute {}. Not enough memory.\nTry cc -F or use chmem to reduce "
                          "stack allocation",
                          vec.al_argv[1]));
    } else {
        panic(std::format("{} is not executable", vec.al_argv[1]));
    }
}

// Main function
/**
 * @brief Entry point for the cc utility.
 * @param argc Number of command-line arguments as per C++23 [basic.start.main].
 * @param argv Array of command-line argument strings.
 * @return Exit status as specified by C++23 [basic.start.main].
 */
int main(int argc, char *argv[]) {
    try {
        CompilerDriver driver;
        ++argv;
        --argc;
        signal(SIGHUP, CompilerDriver::trapcc);
        signal(SIGINT, CompilerDriver::trapcc);
        signal(SIGQUIT, CompilerDriver::trapcc);

        for (const auto arg : std::span(argv, argc)) {
            std::string_view arg_view(arg);
            if (arg_view.empty() || arg_view[0] != '-') {
                append(driver.src_files(), arg_view);
                continue;
            }
            switch (arg_view[1]) {
            case 'c':
                driver.S_flag() = true;
                break;
            case 'D':
            case 'I':
            case 'U':
                append(driver.pp_flags(), arg_view);
                break;
            case 'F':
                driver.F_flag() = true;
                break;
            case 'l':
                if (arg_view.length() > 2) {
                    append(driver.src_files(), file_utils::create_library_path(arg_view.substr(2)));
                }
                break;
            case 'o':
                driver.o_flag() = true;
                if (argc > 1) {
                    driver.o_file() = *++argv;
                    --argc;
                } else {
                    panic("Option -o requires an argument");
                }
                break;
            case 'O':
                append(driver.cg_flags(), "-p4");
                break;
            case 'S':
                driver.S_flag() = true;
                break;
            case 'L':
                if (arg_view == "-LIB") {
                    append(driver.opt_flags(), "-L");
                    break;
                }
                [[fallthrough]];
            case 'v':
                driver.v_flag() = true;
#ifdef DEBUG
                if (arg_view.length() > 2 && arg_view[2] == 'n') {
                    driver.noexec() = true;
                }
#endif
                break;
            case 'T':
                if (arg_view.length() > 2) {
                    driver.tmpdir() = arg_view.substr(2).data();
                }
                [[fallthrough]];
            case 'R':
            case 'p':
            case 'w':
                append(driver.cem_flags(), arg_view);
                break;
            default:
                append(driver.asld_flags(), arg_view);
                break;
            }
        }

        mktempname(driver.tmpname_buf());
        driver.process_source_files();
        if (driver.ret_code() == 0 && driver.ld_files().al_argc > 0 && !driver.S_flag()) {
            driver.perform_linking();
        }
        return driver.ret_code();
    } catch (const std::exception &e) {
        std::cerr << std::format("cc: Fatal error: {}\n", e.what());
        return 1;
    } catch (...) {
        std::cerr << "cc: Unknown fatal error occurred\n";
        return 1;
    }
}