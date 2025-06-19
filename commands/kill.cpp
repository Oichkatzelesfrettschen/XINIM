/**
 * @file kill.cpp
 * @brief Send signals to processes.
 * @author Adri Koppes (original author)
 * @date 2023-10-28 (modernization)
 *
 * @copyright Copyright (c) 2023, The XINIM Project. All rights reserved.
 *
 * This program is a C++23 modernization of the classic `kill` utility.
 * It sends signals to processes specified by process ID (PID).
 * The modern implementation uses proper signal handling, type-safe
 * argument parsing, and comprehensive error reporting.
 *
 * Usage: kill [-signal] pid...
 *   -signal  Signal number to send (default: SIGTERM)
 */

#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <charconv>
#include <system_error>
#include <unordered_map>

// POSIX includes for signal handling
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>

namespace {

/**
 * @brief Kill options structure.
 */
struct KillOptions {
    int signal_number = SIGTERM;
    std::vector<pid_t> pids;
};

/**
 * @brief Signal name to number mapping.
 */
const std::unordered_map<std::string_view, int> signal_names = {
    {"HUP", SIGHUP},    {"INT", SIGINT},    {"QUIT", SIGQUIT},
    {"ILL", SIGILL},    {"TRAP", SIGTRAP},  {"ABRT", SIGABRT},
    {"IOT", SIGIOT},    {"BUS", SIGBUS},    {"FPE", SIGFPE},
    {"KILL", SIGKILL},  {"USR1", SIGUSR1},  {"SEGV", SIGSEGV},
    {"USR2", SIGUSR2},  {"PIPE", SIGPIPE},  {"ALRM", SIGALRM},
    {"TERM", SIGTERM},  {"CHLD", SIGCHLD},  {"CONT", SIGCONT},
    {"STOP", SIGSTOP},  {"TSTP", SIGTSTP},  {"TTIN", SIGTTIN},
    {"TTOU", SIGTTOU},  {"URG", SIGURG},    {"XCPU", SIGXCPU},
    {"XFSZ", SIGXFSZ},  {"VTALRM", SIGVTALRM}, {"PROF", SIGPROF},
    {"WINCH", SIGWINCH}, {"IO", SIGIO},     {"PWR", SIGPWR},
    {"SYS", SIGSYS}
};

/**
 * @brief Signal number to name mapping.
 */
const std::unordered_map<int, std::string_view> signal_numbers = {
    {SIGHUP, "HUP"},    {SIGINT, "INT"},    {SIGQUIT, "QUIT"},
    {SIGILL, "ILL"},    {SIGTRAP, "TRAP"},  {SIGABRT, "ABRT"},
    {SIGIOT, "IOT"},    {SIGBUS, "BUS"},    {SIGFPE, "FPE"},
    {SIGKILL, "KILL"},  {SIGUSR1, "USR1"},  {SIGSEGV, "SEGV"},
    {SIGUSR2, "USR2"},  {SIGPIPE, "PIPE"},  {SIGALRM, "ALRM"},
    {SIGTERM, "TERM"},  {SIGCHLD, "CHLD"},  {SIGCONT, "CONT"},
    {SIGSTOP, "STOP"},  {SIGTSTP, "TSTP"},  {SIGTTIN, "TTIN"},
    {SIGTTOU, "TTOU"},  {SIGURG, "URG"},    {SIGXCPU, "XCPU"},
    {SIGXFSZ, "XFSZ"},  {SIGVTALRM, "VTALRM"}, {SIGPROF, "PROF"},
    {SIGWINCH, "WINCH"}, {SIGIO, "IO"},     {SIGPWR, "PWR"},
    {SIGSYS, "SYS"}
};

/**
 * @brief Parse signal specification (number or name).
 */
int parse_signal(std::string_view signal_spec) {
    // Try parsing as number first
    int signal_num;
    auto [ptr, ec] = std::from_chars(signal_spec.data(), 
                                   signal_spec.data() + signal_spec.size(), signal_num);
    
    if (ec == std::errc() && ptr == signal_spec.data() + signal_spec.size()) {
        if (signal_num > 0 && signal_num < NSIG) {
            return signal_num;
        }
        throw std::runtime_error("Invalid signal number: " + std::string(signal_spec));
    }
    
    // Try parsing as signal name
    std::string upper_signal(signal_spec);
    std::transform(upper_signal.begin(), upper_signal.end(), upper_signal.begin(), ::toupper);
    
    // Remove SIG prefix if present
    if (upper_signal.starts_with("SIG")) {
        upper_signal = upper_signal.substr(3);
    }
    
    auto it = signal_names.find(upper_signal);
    if (it != signal_names.end()) {
        return it->second;
    }
    
    throw std::runtime_error("Invalid signal: " + std::string(signal_spec));
}

/**
 * @brief Parse process ID.
 */
pid_t parse_pid(std::string_view pid_spec) {
    // Special case for "0"
    if (pid_spec == "0") {
        return 0;
    }
    
    pid_t pid;
    auto [ptr, ec] = std::from_chars(pid_spec.data(), 
                                   pid_spec.data() + pid_spec.size(), pid);
    
    if (ec != std::errc() || ptr != pid_spec.data() + pid_spec.size()) {
        throw std::runtime_error("Invalid process ID: " + std::string(pid_spec));
    }
    
    if (pid < 0) {
        throw std::runtime_error("Process ID cannot be negative: " + std::string(pid_spec));
    }
    
    return pid;
}

/**
 * @brief Parse command line arguments.
 */
KillOptions parse_arguments(int argc, char* argv[]) {
    KillOptions opts;
    int i = 1;
    
    // Check for -l flag (list signals)
    if (i < argc && std::string_view(argv[i]) == "-l") {
        std::cout << "Available signals:" << std::endl;
        for (const auto& [sig_num, sig_name] : signal_numbers) {
            std::cout << std::setw(2) << sig_num << ") SIG" << sig_name << std::endl;
        }
        std::exit(0);
    }
    
    // Parse signal option
    if (i < argc && argv[i][0] == '-' && argv[i][1] != '\0') {
        std::string_view signal_arg(argv[i] + 1);
        opts.signal_number = parse_signal(signal_arg);
        ++i;
    }
    
    // Parse PIDs
    if (i >= argc) {
        throw std::runtime_error("At least one process ID required");
    }
    
    while (i < argc) {
        opts.pids.push_back(parse_pid(argv[i]));
        ++i;
    }
    
    return opts;
}

/**
 * @brief Print usage information.
 */
void print_usage() {
    std::cerr << "Usage: kill [-signal] pid..." << std::endl;
    std::cerr << "       kill -l" << std::endl;
    std::cerr << "  -signal  Signal number or name to send (default: TERM)" << std::endl;
    std::cerr << "  -l       List available signals" << std::endl;
}

} // namespace

/**
 * @brief Main entry point for the kill command.
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char* argv[]) {
    try {
        KillOptions options = parse_arguments(argc, argv);
        
        bool any_errors = false;
        
        for (pid_t pid : options.pids) {
            if (::kill(pid, options.signal_number) != 0) {
                std::cerr << "kill: (" << pid << ") - " << std::strerror(errno) << std::endl;
                any_errors = true;
            }
        }
        
        return any_errors ? 1 : 0;
        
    } catch (const std::exception& e) {
        std::cerr << "kill: " << e.what() << std::endl;
        print_usage();
        return 1;
    }
}
