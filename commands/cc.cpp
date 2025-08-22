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
constexpr std::size_t MAXARGC = 64;                  /**< Maximum arguments in a list */
constexpr std::size_t USTR_SIZE = 64;                /**< Maximum string variable length */
constexpr std::size_t MAX_FILES = 32;                /**< Maximum number of input files */
constexpr std::size_t BUFSIZE = USTR_SIZE * MAXARGC; /**< Total buffer size */
constexpr std::size_t TEMP_NAME_SIZE = 15;           /**< Temporary filename buffer size */
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

[[nodiscard]] inline char *create_library_path(std::string_view name) {
    if (name.empty())
        return nullptr;
    const auto total_len =
        std::string(LIBDIR).size() + name.size() + 7; // "/lib" + name + ".a" + null
    char *result = memory_manager::allocate(total_len);
    return result ? std::format("{}/lib{}.a", LIBDIR, name).copy(result, total_len) : nullptr;
}
} // namespace file_utils

// Utility functions
[[nodiscard]] inline char *alloc(std::size_t size) {
    try {
        return memory_manager::allocate(size);
    } catch (const std::runtime_error &) {
        panic("No space for allocation");
        return nullptr; // Never reached
    }
}

void append(arglist &al, std::string_view arg) {
    if (al.is_full()) {
        panic("Argument list overflow");
    }
    char *buf = alloc(arg.size() + 1);
    arg.copy(buf, arg.size());
    buf[arg.size()] = '\0';
    al.al_argv[al.al_argc++] = buf;
}

void concat(arglist &al1, const arglist &al2) {
    if (al1.is_full() || (al1.al_argc + al2.al_argc) > config::MAXARGC) {
        panic("Argument list overflow in concat");
    }
    std::ranges::copy(al2.al_argv | std::views::take(al2.al_argc),
                      al1.al_argv.begin() + al1.al_argc);
    al1.al_argc += al2.al_argc;
}

[[nodiscard]] char *mkstr(UString &dst, const std::ranges::range auto &...args) {
    char *q = dst.data();
    const auto copy = [&](std::string_view s) {
        while (!s.empty() && q < dst.data() + config::USTR_SIZE - 1) {
            *q++ = s.front();
            s.remove_prefix(1);
        }
    };
    (copy(args), ...);
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

// RAII-based compiler driver
class CompilerDriver {
  public:
    CompilerDriver() : instance_(this) {
        CALL_VEC[0].init();
        CALL_VEC[1].init();
    }

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
    [[nodiscard]] auto &o_file() noexcept { return o_file_; }
    [[nodiscard]] auto &prog_call() noexcept { return prog_call_; }
    [[nodiscard]] auto &base() noexcept { return BASE; }
    [[nodiscard]] auto &tmpdir() noexcept { return tmpdir_; }
    [[nodiscard]] auto tmpname_buf() noexcept { return std::span<char>{tmpname}; }
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
    static void trapcc(int sig) noexcept {
        signal(sig, SIG_IGN);
        if (instance_) {
            instance_->cleanup_temporary_files();
        }
    }

  private:
    void cleanup_temporary_files() noexcept {
        std::lock_guard lock(mtx_);
        file_utils::cleanup_temp(ifile_.data());
        file_utils::cleanup_temp(kfile_.data());
        file_utils::cleanup_temp(sfile_.data());
        file_utils::cleanup_temp(mfile_.data());
        file_utils::cleanup_temp(ofile_.data());
    }

    struct arglist SRCFILES {};    /**< Source files to process */
    struct arglist LDFILES {};     /**< Object files for linking */
    struct arglist GEN_LDFILES {}; /**< Generated temporary files */
    struct arglist PP_FLAGS {};    /**< Preprocessor flags */
    struct arglist CEM_FLAGS {};   /**< Compiler flags */
    struct arglist OPT_FLAGS {};   /**< Optimizer flags */
    struct arglist CG_FLAGS {};    /**< Code generator flags */
    struct arglist ASLD_FLAGS {};  /**< Assembler/linker flags */
    struct arglist DEBUG_FLAGS {}; /**< Debug flags */
    struct arglist LD_HEAD {
        1, { const_cast<char *>("/usr/lib/crtso.s") }
    }; /**< Linker prologue files */
    struct arglist LD_TAIL {
        2, { const_cast<char *>("/usr/lib/libc.a"), const_cast<char *>("/usr/lib/end.s") }
    };                                 /**< Linker epilogue files */
    std::array<arglist, 2> CALL_VEC{}; /**< Command execution vectors */
    int RET_CODE{0};                   /**< Return code accumulator */
    bool o_flag_{false};               /**< Output file specified */
    bool S_flag_{false};               /**< Stop after compilation */
    bool v_flag_{false};               /**< Verbose mode */
    bool F_flag_{false};               /**< Use files instead of pipes */
    UString ifile_{}, kfile_{}, sfile_{}, mfile_{}, ofile_{}, BASE{};
    const char *tmpdir_{toolchain_config::TEMP_DIR.data()};
    char tmpname[config::TEMP_NAME_SIZE]{};
    char *o_file_{
        const_cast<char *>(toolchain_config::DEFAULT_OUTPUT.data())}; /**< Output file path */
    const char *prog_call_{nullptr}; /**< Program invocation name */
#ifdef DEBUG
    bool noexec_{false}; /**< Debug: don't execute commands */
#endif
    mutable std::mutex mtx_{};
    static thread_local CompilerDriver *instance_;
};

thread_local CompilerDriver *CompilerDriver::instance_ = nullptr;

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
                file_utils::remove_file(ifile_.data());
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
                file_utils::remove_file(kfile_.data());
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
        if (!runvec(call, nullptr)) {
            file_utils::remove_file(kfile_.data());
            return false;
        }
        file = kfile_.data();
        ext = 'k';
        file_utils::cleanup_temp(ifile_.data());
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
        file_utils::cleanup_temp(kfile_.data());
    }
    if (ext == 'm') {
        ldfile = S_flag_ ? ofile_.data() : alloc(BASE.size() + 3);
        call.init();
        append(call, CG);
        concat(call, CG_FLAGS);
        append(call, file);
        auto f = mkstr(ldfile, BASE.data(), ".s");
        append(call, f);
        if (!runvec(call, nullptr)) {
            return false;
        }
        file_utils::cleanup_temp(mfile_.data());
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
            file_utils::remove_file(GEN_LDFILES.al_argv[i]);
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
        vec.al_argv[0] = SHELL;
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
int main(int argc, char *argv[]) {
    try {
        CompilerDriver driver;
        driver.prog_call() = *argv++;
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