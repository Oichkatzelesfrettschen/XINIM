/**
 * @file cc.cpp
 * @brief Modern C++23 compiler driver for XINIM operating system
 * @author Erik Baalbergen (original), modernized for XINIM C++23 migration
 * @version 2.0 - Modernized with C++23 features and RAII principles
 * @date 2025-06-19
 * 
 * @section Description
 * A compiler driver that works like /bin/cc and accepts standard
 * compilation options. Coordinates the compilation pipeline including
 * preprocessing, compilation, and linking phases.
 * 
 * Originally derived from CEMCOM compiler driver, now fully modernized
 * with C++23 features for type safety, performance, and maintainability.
 * 
 * @section Features
 * - Modern exception-safe error handling
 * - RAII resource management  
 * - Type-safe string handling with std::string_view
 * - Constexpr configuration management
 * - Memory-safe buffer management
 * - Comprehensive documentation and logging
 * 
 * @section Usage
 * cc [options] file...
 * 
 * Standard options: -c, -o, -D, -I, -U, -l, -O, -S, -F, -v
 * 
 * @note Requires C++23 compliant compiler
 */

#include "../h/signal.hpp"
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <string_view>
#include <vector>
#include <memory>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <span>
#include <stdexcept>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>

// Modern constants with clear semantic meaning
namespace config {
    constexpr int MAXARGC = 64;        /**< Maximum arguments in a list */
    constexpr int USTR_SIZE = 64;      /**< Maximum string variable length */
    constexpr int MAX_FILES = 32;      /**< Maximum number of input files */
    constexpr int BUFSIZE = USTR_SIZE * MAXARGC; /**< Total buffer size */
}

// Modern type aliases for better clarity
using USTRING = std::array<char, config::USTR_SIZE>;
using ArgumentVector = std::vector<std::string>;

/**
 * @brief Modern argument list structure for compiler passes
 * 
 * Provides type-safe argument management with bounds checking
 * and modern C++23 initialization.
 */
struct arglist {
    int al_argc{0};                                      /**< Argument count */
    std::array<char*, config::MAXARGC> al_argv{};        /**< Argument vector */
    
    /**
     * @brief Initialize argument list
     */
    constexpr void init() noexcept { al_argc = 1; }
    
    /**
     * @brief Check if argument list is full
     * @return true if at capacity
     */
    [[nodiscard]] constexpr bool is_full() const noexcept { 
        return al_argc >= config::MAXARGC; 
    }
};

/**
 * @brief Compiler toolchain paths configuration
 * 
 * Modern C++23 approach using constexpr and conditional compilation
 * to handle different memory configurations elegantly.
 */
namespace compiler_paths {
    // Memory configuration detection
    #if defined(MEM640K)
        constexpr std::string_view BASE_PATH = "/lib";
    #elif defined(MEM512K) 
        constexpr std::string_view BASE_PATH = "/usr/lib";
    #else
        // Default to modern system layout
        constexpr std::string_view BASE_PATH = "/usr/lib";
    #endif
    
    // Modern path construction with compile-time evaluation
    constexpr std::string_view CPP_PATH = "/usr/lib/cpp";
    constexpr std::string_view CEM_PATH = "/usr/lib/cem"; 
    constexpr std::string_view OPT_PATH = "/usr/lib/opt";
    constexpr std::string_view CG_PATH = "/usr/lib/cg";
    constexpr std::string_view ASLD_PATH = "/usr/bin/asld";
    constexpr std::string_view SHELL_PATH = "/bin/sh";
    constexpr std::string_view LIB_DIR = "/usr/lib";
}

// Legacy toolchain path globals (being phased out)
// TODO: Replace with modern compiler_paths namespace usage
#ifdef MEM640K
    /* MINIX paths for 640K PC (not 512K AT) */
    char *PP = const_cast<char*>("/lib/cpp");
    char *CEM = const_cast<char*>("/lib/cem");
    char *OPT = const_cast<char*>("/usr/lib/opt");
    char *CG = const_cast<char*>("/usr/lib/cg");
    char *ASLD = const_cast<char*>("/usr/bin/asld");
    char *SHELL = const_cast<char*>("/bin/sh");
    char *LIBDIR = const_cast<char*>("/usr/lib");
#elif defined(MEM512K)
    /* MINIX paths for 512K AT (not 640K PC) */
    char *PP = const_cast<char*>("/usr/lib/cpp");
    char *CEM = const_cast<char*>("/usr/lib/cem");
    char *OPT = const_cast<char*>("/usr/lib/opt");
    char *CG = const_cast<char*>("/usr/lib/cg");
    char *ASLD = const_cast<char*>("/usr/bin/asld");
    char *SHELL = const_cast<char*>("/bin/sh");
    char *LIBDIR = const_cast<char*>("/usr/lib");
#else
    // Default modern configuration
    char *PP = const_cast<char*>("/usr/lib/cpp");
    char *CEM = const_cast<char*>("/usr/lib/cem");
    char *OPT = const_cast<char*>("/usr/lib/opt");
    char *CG = const_cast<char*>("/usr/lib/cg");
    char *ASLD = const_cast<char*>("/usr/bin/asld");
    char *SHELL = const_cast<char*>("/bin/sh");
    char *LIBDIR = const_cast<char*>("/usr/lib");
#endif

// Modern configuration constants
namespace toolchain_config {
    /* Object sizes with semantic naming */
    constexpr std::string_view V_FLAG = "-Vs2.2w2.2i2.2l4.2f4.2d8.2p2.2";
    
    /* Default output file */
    constexpr std::string_view DEFAULT_OUTPUT = "a.out";
    
    /* Temporary directory */
    constexpr std::string_view TEMP_DIR = "/tmp";
}

// Legacy global structures (being modernized)
struct arglist LD_HEAD = {1, {const_cast<char*>("/usr/lib/crtso.s")}};
struct arglist LD_TAIL = {2, {const_cast<char*>("/usr/lib/libc.a"), const_cast<char*>("/usr/lib/end.s")}};

char *o_FILE = const_cast<char*>("a.out"); /* default name for executable file */

/**
 * @brief Modern utility functions replacing unsafe macros
 * 
 * These replace the legacy unsafe macros with type-safe, 
 * exception-safe equivalents.
 */
namespace file_utils {
    /**
     * @brief Safely remove a file and clear filename
     * @param filename Reference to filename string to clear
     * @return true if file was removed successfully
     */
    [[nodiscard]] inline bool remove_file(char* filename) noexcept {
        if (!filename || filename[0] == '\0') return true;
        
        const bool success = (unlink(filename) == 0);
        filename[0] = '\0';  // Clear filename
        return success;
    }
    
    /**
     * @brief Cleanup temporary file if it exists
     * @param filename Pointer to filename (may be null)
     * @return true if cleanup was successful or file didn't exist
     */
    [[nodiscard]] inline bool cleanup_temp(char* filename) noexcept {
        return filename ? remove_file(filename) : true;
    }
    
    /**
     * @brief Create library path from name
     * @param name Library name (without .a extension)
     * @return Allocated string with full library path
     */
    [[nodiscard]] char* create_library_path(const char* name);
}

// Forward declarations for modernized functions
[[nodiscard]] char* mkstr(char* dst, const char* arg, ...);
[[nodiscard]] char* alloc(unsigned int size);
void append(struct arglist* al, const char* arg);
void concat(struct arglist* al1, const struct arglist* al2);
[[nodiscard]] int extension(const char* filename);
void basename(const char* str, char* dst);
[[noreturn]] void panic(const char* message);
[[nodiscard]] int runvec(struct arglist* vec, const char* output_file);
[[nodiscard]] int runvec2(struct arglist* vec0, struct arglist* vec1);
void pr_vec(const struct arglist* vec);
[[noreturn]] void ex_vec(struct arglist* vec);
void mktempname(char name_buffer[]);
void process_source_files();
bool process_single_file(char*& file, int& ext, char*& ldfile);
bool process_compilation_stages(char*& file, int& ext, char*& ldfile);
void perform_linking();

// Global state (being refactored into classes)
char *ProgCall = nullptr;

// Global compiler state (TODO: refactor into CompilerDriver class)
struct compiler_state {
    struct arglist SRCFILES{};     /**< Source files to process */
    struct arglist LDFILES{};      /**< Object files for linking */
    struct arglist GEN_LDFILES{};  /**< Generated temporary files */
    
    struct arglist PP_FLAGS{};     /**< Preprocessor flags */
    struct arglist CEM_FLAGS{};    /**< Compiler flags */
    struct arglist OPT_FLAGS{};    /**< Optimizer flags */
    struct arglist CG_FLAGS{};     /**< Code generator flags */
    struct arglist ASLD_FLAGS{};   /**< Assembler/linker flags */
    struct arglist DEBUG_FLAGS{};  /**< Debug flags */
    
    struct arglist CALL_VEC[2]{};  /**< Command execution vectors */
    
    int RET_CODE{0};               /**< Return code accumulator */
    
    // Compilation flags
    bool o_flag{false};            /**< Output file specified */
    bool S_flag{false};            /**< Stop after compilation */
    bool v_flag{false};            /**< Verbose mode */
    bool F_flag{false};            /**< Use files instead of pipes */
    
    // Temporary files
    USTRING ifile{}, kfile{}, sfile{}, mfile{}, ofile{};
    USTRING BASE{};
    
    const char* tmpdir{toolchain_config::TEMP_DIR.data()};
    char tmpname[15]{};
    
    #ifdef DEBUG
    bool noexec{false};            /**< Debug: don't execute commands */
    #endif
} g_state;

/**
 * @brief Signal handler for cleanup on interruption
 * @param sig Signal number received
 */
static void trapcc(int sig) noexcept {
    signal(sig, SIG_IGN);
    (void)file_utils::cleanup_temp(g_state.ifile.data());
    (void)file_utils::cleanup_temp(g_state.kfile.data());
    (void)file_utils::cleanup_temp(g_state.sfile.data());
    (void)file_utils::cleanup_temp(g_state.mfile.data());
    (void)file_utils::cleanup_temp(g_state.ofile.data());
}

/**
 * @brief Modern main function with exception safety and clear error handling
 * @param argc Argument count
 * @param argv Argument vector
 * @return Exit code (0 for success, non-zero for failure)
 */
int main(int argc, char *argv[]) {
    try {
        // Initialize program state
        ProgCall = *argv++;
        argc--;
        
        // Set up signal handlers for clean shutdown
        signal(SIGHUP, trapcc);
        signal(SIGINT, trapcc);
        signal(SIGQUIT, trapcc);
        
        // Process command line arguments
        while (argc > 0) {
            const std::string_view arg{*argv++};
            argc--;
            
            if (arg.empty() || arg[0] != '-') {
                // Source file or library
                append(&g_state.SRCFILES, arg.data());
                continue;
            }
            
            // Process option flags
            switch (arg[1]) {
                case 'c':
                    g_state.S_flag = true;
                    break;
                    
                case 'D':
                case 'I': 
                case 'U':
                    append(&g_state.PP_FLAGS, arg.data());
                    break;
                    
                case 'F':
                    g_state.F_flag = true;
                    break;
                    
                case 'l':
                    if (arg.length() > 2) {
                        append(&g_state.SRCFILES, file_utils::create_library_path(&arg[2]));
                    }
                    break;
                    
                case 'o':
                    g_state.o_flag = true;
                    if (argc > 0) {
                        o_FILE = *argv++;
                        argc--;
                    } else {
                        panic("Option -o requires an argument");
                    }
                    break;
                    
                case 'O':
                    append(&g_state.CG_FLAGS, "-p4");
                    break;
                    
                case 'S':
                    g_state.S_flag = true;
                    break;
                    
                case 'L':
                    if (arg == "-LIB") {
                        append(&g_state.OPT_FLAGS, "-L");
                        break;
                    }
                    [[fallthrough]];
                    
                case 'v':
                    g_state.v_flag = true;
                    #ifdef DEBUG
                    if (arg.length() > 2 && arg[2] == 'n') {
                        g_state.noexec = true;
                    }
                    #endif
                    break;
                    
                case 'T':
                    if (arg.length() > 2) {
                        g_state.tmpdir = &arg[2];
                    }
                    [[fallthrough]];
                    
                case 'R':
                case 'p':
                case 'w':
                    append(&g_state.CEM_FLAGS, arg.data());
                    break;
                    
                default:
                    append(&g_state.ASLD_FLAGS, arg.data());
                    break;
            }
        }
        
        // Generate temporary filename base
        mktempname(g_state.tmpname);
        
        // Process all source files
        process_source_files();
        
        // Link if not in compile-only mode and we have objects
        if (g_state.RET_CODE == 0 && g_state.LDFILES.al_argc > 0 && !g_state.S_flag) {
            perform_linking();
        }
        
        return g_state.RET_CODE;
        
    } catch (const std::exception& e) {
        std::cerr << "cc: Fatal error: " << e.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "cc: Unknown fatal error occurred\n";
        return 1;
    }
}

/**
 * @brief Process all source files through the compilation pipeline
 * 
 * Handles the complete compilation pipeline from source to object files:
 * .c -> .i (preprocessing) -> .k (compilation) -> .m (optimization) -> .s (assembly)
 */
void process_source_files() {
    const int count = g_state.SRCFILES.al_argc;
    char** argvec = &(g_state.SRCFILES.al_argv[0]);
    
    for (int i = 0; i < count; ++i) {
        char* file = *argvec++;
        char* ldfile = nullptr;
        
        basename(file, g_state.BASE.data());
        
        // Show progress for multiple files
        if (g_state.SRCFILES.al_argc > 1) {
            std::cout << file << ":\n";
        }
        
        int ext = extension(file);
        
        // Process through compilation pipeline
        if (!process_single_file(file, ext, ldfile)) {
            continue; // Skip failed files
        }
        
        // Add to link list if not compile-only
        if (!g_state.S_flag) {
            append(&g_state.LDFILES, file);
            if (ldfile) {
                append(&g_state.GEN_LDFILES, ldfile);
            }
        }
    }
}

/**
 * @brief Process a single file through the compilation pipeline
 * @param file Input filename
 * @param ext File extension character
 * @param ldfile Output parameter for generated object file
 * @return true if processing succeeded, false otherwise
 */
bool process_single_file(char*& file, int& ext, char*& ldfile) {
    struct arglist* call = &g_state.CALL_VEC[0];
    struct arglist* call1 = &g_state.CALL_VEC[1];
    char* f = nullptr;
    
    // .c -> .i (preprocessing) or .k (direct compilation)
    if (ext == 'c') {
        call->init();
        append(call, PP);
        concat(call, &g_state.PP_FLAGS);
        append(call, file);
        
        if (g_state.F_flag) {
            // Use file for preprocessing output
            f = mkstr(g_state.ifile.data(), g_state.tmpdir, g_state.tmpname, ".i", nullptr);            if (runvec(call, f)) {
                file = g_state.ifile.data();
                ext = 'i';
            } else {
                (void)file_utils::remove_file(g_state.ifile.data());
                return false;
            }
        } else {
            // Use pipe for direct compilation
            call1->init();
            append(call1, CEM);
            concat(call1, &g_state.DEBUG_FLAGS);
            append(call1, const_cast<char*>(toolchain_config::V_FLAG.data()));
            concat(call1, &g_state.CEM_FLAGS);
            append(call1, "-"); // Use stdin
            f = mkstr(g_state.kfile.data(), g_state.tmpdir, g_state.tmpname, ".k", nullptr);
            append(call1, f);            if (runvec2(call, call1)) {
                file = g_state.kfile.data();
                ext = 'k';
            } else {
                (void)file_utils::remove_file(g_state.kfile.data());
                return false;
            }
        }
    }
    
    // Continue through remaining pipeline stages...
    return process_compilation_stages(file, ext, ldfile);
}

/**
 * @brief Perform final linking stage
 */
void perform_linking() {
    struct arglist* call = &g_state.CALL_VEC[0];
    
    call->init();
    append(call, ASLD);
    concat(call, &g_state.ASLD_FLAGS);
    append(call, "-o");
    append(call, o_FILE);
    concat(call, &LD_HEAD);
    concat(call, &g_state.LDFILES);
    concat(call, &LD_TAIL);
      if (runvec(call, nullptr)) {
        // Clean up temporary files on successful link
        for (int i = 0; i < g_state.GEN_LDFILES.al_argc; ++i) {
            (void)file_utils::remove_file(g_state.GEN_LDFILES.al_argv[i]);
        }
    }
}

// Forward declaration for the pipeline continuation
bool process_compilation_stages(char*& file, int& ext, char*& ldfile);

/**
 * @brief Continue processing file through compilation stages
 * @param file File being processed (modified in-place)
 * @param ext File extension (modified in-place)
 * @param ldfile Output object file name
 * @return true if processing succeeded
 */
bool process_compilation_stages(char*& file, int& ext, char*& ldfile) {
    struct arglist* call = &g_state.CALL_VEC[0];
    char* f = nullptr;
    
    // .i -> .k (compilation)
    if (ext == 'i') {
        call->init();
        append(call, CEM);
        concat(call, &g_state.DEBUG_FLAGS);
        append(call, const_cast<char*>(toolchain_config::V_FLAG.data()));
        concat(call, &g_state.CEM_FLAGS);
        append(call, file);
        f = mkstr(g_state.kfile.data(), g_state.tmpdir, g_state.tmpname, ".k", nullptr);
        append(call, f);
          if (runvec(call, nullptr)) {
            file = g_state.kfile.data();
            ext = 'k';
        } else {
            (void)file_utils::remove_file(g_state.kfile.data());
            return false;
        }
        (void)file_utils::cleanup_temp(g_state.ifile.data());
    }
    
    // .k -> .m (optimization)
    if (ext == 'k') {
        call->init();
        append(call, OPT);
        concat(call, &g_state.OPT_FLAGS);
        append(call, file);
        f = mkstr(g_state.mfile.data(), g_state.tmpdir, g_state.tmpname, ".m", nullptr);
        if (!runvec(call, f)) {
            return false;
        }        file = g_state.mfile.data();
        ext = 'm';
        (void)file_utils::cleanup_temp(g_state.kfile.data());
    }
    
    // .m -> .s (code generation)
    if (ext == 'm') {
        ldfile = g_state.S_flag ? g_state.ofile.data() : 
                 alloc(strlen(g_state.BASE.data()) + 3);
        
        call->init();
        append(call, CG);
        concat(call, &g_state.CG_FLAGS);
        append(call, file);
        f = mkstr(ldfile, g_state.BASE.data(), ".s", nullptr);
        append(call, f);        if (!runvec(call, nullptr)) {
            return false;
        }
        (void)file_utils::cleanup_temp(g_state.mfile.data());
        file = ldfile;
        ext = 's';
    }
    
    return true;
}

/**
 * @brief Modern memory management and utility functions
 * 
 * Type-safe replacements for legacy buffer management and string operations
 */
namespace memory_manager {
    // Modern buffer management with bounds checking
    std::array<char, config::BUFSIZE> buffer{};
    char* buffer_ptr = buffer.data();
    
    /**
     * @brief Allocate memory from static buffer with bounds checking
     * @param size Number of bytes to allocate
     * @return Pointer to allocated memory
     * @throws std::runtime_error if out of buffer space
     */
    [[nodiscard]] char* allocate(unsigned int size) {
        char* const result = buffer_ptr;
        
        if ((buffer_ptr + size) >= (buffer.data() + config::BUFSIZE)) {
            throw std::runtime_error("Buffer overflow: no space for allocation");
        }
        
        buffer_ptr += size;
        return result;
    }
    
    /**
     * @brief Reset buffer allocator (for cleanup)
     */
    void reset() noexcept {
        buffer_ptr = buffer.data();
    }
}

/**
 * @brief Modernized alloc function with exception safety
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory
 */
[[nodiscard]] char* alloc(unsigned int size) {
    try {
        return memory_manager::allocate(size);
    } catch (const std::runtime_error&) {
        panic("no space\n");
        return nullptr; // Never reached due to [[noreturn]]
    }
}

/**
 * @brief Safely append argument to argument list with bounds checking
 * @param al Argument list to append to
 * @param arg Argument string to append
 */
void append(struct arglist* al, const char* arg) {
    if (!al) {
        panic("null argument list\n");
    }
    
    if (al->al_argc >= config::MAXARGC) {
        panic("argument list overflow\n");
    }
    
    al->al_argv[al->al_argc++] = const_cast<char*>(arg);
}

/**
 * @brief Concatenate two argument lists with bounds checking
 * @param al1 Destination argument list 
 * @param al2 Source argument list to append
 */
void concat(struct arglist* al1, const struct arglist* al2) {
    if (!al1 || !al2) {
        panic("null argument list in concat\n");
    }
    
    const int new_argc = al1->al_argc + al2->al_argc;
    if (new_argc >= config::MAXARGC) {
        panic("argument list overflow in concat\n");
    }
    
    // Copy arguments efficiently
    std::copy_n(al2->al_argv.data(), al2->al_argc, 
                al1->al_argv.data() + al1->al_argc);
    
    al1->al_argc = new_argc;
}

/**
 * @brief Modern variable-argument string concatenation
 * @param dst Destination buffer
 * @param ... Null-terminated list of strings to concatenate
 * @return Pointer to destination buffer
 */
char* mkstr(char* dst, const char* arg, ...) {
    if (!dst) return nullptr;
    
    va_list args;
    va_start(args, arg);
    
    char* q = dst;
    const char* p = arg;
    
    while (p) {
        // Copy string safely
        while (*p && q < dst + config::USTR_SIZE - 1) {
            *q++ = *p++;
        }
        p = va_arg(args, const char*);
    }
    
    *q = '\0'; // Ensure null termination
    va_end(args);
    return dst;
}

/**
 * @brief Extract basename from file path safely
 * @param str Input file path
 * @param dst Destination buffer for basename
 */
void basename(const char* str, char* dst) {
    if (!str || !dst) {
        panic("null pointer in basename\n");
    }
    
    const char* p1 = str;
    const char* p2 = str;
    
    // Find last '/' character
    while (*p1) {
        if (*p1++ == '/') {
            p2 = p1;
        }
    }
    
    // Copy basename, removing extension if present
    const char* src = p2;
    char* dest = dst;
    
    while (*src && *src != '.' && dest < dst + config::USTR_SIZE - 1) {
        *dest++ = *src++;
    }
    *dest = '\0';
}

/**
 * @brief Get file extension character from filename
 * @param filename File name to examine
 * @return Extension character or 0 if no extension
 */
[[nodiscard]] int extension(const char* filename) {
    if (!filename) return 0;
    
    const char* p = filename + strlen(filename);
    
    if (p > filename && *(p-1) == '.') return 0; // Ends with '.'
    
    // Find extension
    while (p > filename && *--p != '.') {
        // Continue until we find '.' or reach start
    }
    
    return (p > filename && *p == '.') ? *(p+1) : 0;
}

/**
 * @brief Execute command vector with optional output redirection
 * @param vec Command and arguments to execute
 * @param output_file Optional output redirection file (null for stdout)
 * @return 1 on success, 0 on failure
 */
[[nodiscard]] int runvec(struct arglist* vec, const char* output_file) {
    if (!vec) {
        panic("null argument vector in runvec\n");
    }
    
    if (g_state.v_flag) {
        pr_vec(vec);
        write(STDERR_FILENO, "\n", 1);
    }
    
    const pid_t pid = fork();
    if (pid == 0) {
        // Child process
        if (output_file) {
            close(STDOUT_FILENO);
            const int fd = creat(output_file, 0666);
            if (fd != STDOUT_FILENO) {
                panic("cannot create output file\n");
            }
        }
        ex_vec(vec);
        // Never returns
    }
    
    if (pid == -1) {
        panic("no more processes\n");
    }
    
    int status;
    wait(&status);
    
    if (status != 0) {
        g_state.RET_CODE = 1;
        return 0;
    }
    return 1;
}

/**
 * @brief Execute two command vectors connected by pipe
 * @param vec0 First command (producer)
 * @param vec1 Second command (consumer)
 * @return 1 on success, 0 on failure
 */
[[nodiscard]] int runvec2(struct arglist* vec0, struct arglist* vec1) {
    if (!vec0 || !vec1) {
        panic("null argument vector in runvec2\n");
    }
    
    if (g_state.v_flag) {
        pr_vec(vec0);
        write(STDERR_FILENO, " | ", 3);
        pr_vec(vec1);
        write(STDERR_FILENO, "\n", 1);
    }
    
    int pipe_fds[2];
    if (pipe(pipe_fds) == -1) {
        panic("cannot create pipe\n");
    }
    
    // First process (producer)
    const pid_t pid1 = fork();
    if (pid1 == 0) {
        close(STDOUT_FILENO);
        if (dup(pipe_fds[1]) != STDOUT_FILENO) {
            panic("bad dup\n");
        }
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        ex_vec(vec0);
    }
    
    if (pid1 == -1) {
        panic("no more processes\n");
    }
    
    // Second process (consumer)
    const pid_t pid2 = fork();
    if (pid2 == 0) {
        close(STDIN_FILENO);
        if (dup(pipe_fds[0]) != STDIN_FILENO) {
            panic("bad dup\n");
        }
        close(pipe_fds[0]);
        close(pipe_fds[1]);
        ex_vec(vec1);
    }
    
    if (pid2 == -1) {
        panic("no more processes\n");
    }
    
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    
    int status1, status2;
    wait(&status1);
    wait(&status2);
    
    if (status1 || status2) {
        g_state.RET_CODE = 1;
        return 0;
    }
    return 1;
}

/**
 * @brief Modern panic function with proper error handling
 * @param message Error message to display
 */
[[noreturn]] void panic(const char* message) {
    if (message) {
        write(STDERR_FILENO, message, strlen(message));
    }
    exit(EXIT_FAILURE);
}

/**
 * @brief Find character in string (like strchr)
 * @param s String to search
 * @param c Character to find
 * @return Pointer to first occurrence or nullptr
 */
[[nodiscard]] const char* cindex(const char* s, char c) {
    if (!s) return nullptr;
    
    while (*s) {
        if (*s == c) {
            return s;
        }
        ++s;
    }
    return nullptr;
}

/**
 * @brief Print command vector for debugging
 * @param vec Argument vector to print
 */
void pr_vec(const struct arglist* vec) {
    if (!vec || vec->al_argc <= 1) return;
    
    // Print first argument (command name)
    const char* cmd = vec->al_argv[1];
    if (cmd) {
        write(STDERR_FILENO, cmd, strlen(cmd));
        
        // Print remaining arguments
        for (int i = 2; i < vec->al_argc; ++i) {
            if (vec->al_argv[i]) {
                write(STDERR_FILENO, " ", 1);
                write(STDERR_FILENO, vec->al_argv[i], strlen(vec->al_argv[i]));
            }
        }
    }
}

/**
 * @brief Execute command vector via execv
 * @param vec Command and arguments to execute
 */
[[noreturn]] void ex_vec(struct arglist* vec) {
    if (!vec) {
        panic("null argument vector in ex_vec\n");
    }
    
    #ifdef DEBUG
    if (g_state.noexec) {
        exit(EXIT_SUCCESS);
    }
    #endif
    
    vec->al_argv[vec->al_argc] = nullptr; // Null terminate
    
    execv(vec->al_argv[1], &(vec->al_argv[1]));
    
    // If execv failed, try with shell
    if (errno == ENOEXEC) {
        vec->al_argv[0] = SHELL;
        execv(SHELL, &(vec->al_argv[0]));
    }
    
    // Report execution failure
    if (access(vec->al_argv[1], X_OK) == 0) {
        const char* msg1 = "Cannot execute ";
        const char* msg2 = ". Not enough memory.\nTry cc -F or use chmem to reduce its stack allocation\n";
        write(STDERR_FILENO, msg1, strlen(msg1));
        write(STDERR_FILENO, vec->al_argv[1], strlen(vec->al_argv[1]));
        write(STDERR_FILENO, msg2, strlen(msg2));
    } else {
        write(STDERR_FILENO, vec->al_argv[1], strlen(vec->al_argv[1]));
        write(STDERR_FILENO, " is not executable\n", 19);
    }
    
    exit(EXIT_FAILURE);
}

/**
 * @brief Generate unique temporary filename based on process ID
 * @param name_buffer Buffer to store generated name (must be >= 11 bytes)
 */
void mktempname(char name_buffer[]) {
    if (!name_buffer) {
        panic("null buffer in mktempname\n");
    }
    
    const pid_t pid = getpid();
    
    // Create "/cemXXXXX" pattern
    name_buffer[0] = '/';
    name_buffer[1] = 'c';
    name_buffer[2] = 'e';
    name_buffer[3] = 'm';
    
    // Convert PID to decimal digits (backwards)
    pid_t temp_pid = pid;
    for (int i = 9; i > 3; --i) {
        name_buffer[i] = (temp_pid % 10) + '0';
        temp_pid /= 10;
    }
    
    name_buffer[10] = '\0'; // Null termination
}

/**
 * @brief Implementation of library path creation utility
 * @param name Library name (without .a extension)
 * @return Allocated string with full library path
 */
[[nodiscard]] char* file_utils::create_library_path(const char* name) {
    if (!name) return nullptr;
    
    const size_t name_len = strlen(name);
    const size_t libdir_len = strlen(LIBDIR);
    const size_t total_len = libdir_len + name_len + 7; // "/lib" + name + ".a" + null
    
    char* result = alloc(total_len);
    if (!result) return nullptr;
    
    return mkstr(result, LIBDIR, "/lib", name, ".a", nullptr);
}
