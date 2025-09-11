// Pure C++23 implementation of POSIX 'ps' utility
// Process Status - No libc, only libc++

#include <algorithm>
#include <charconv>
#include <chrono>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <ranges>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
namespace chrono = std::chrono;

namespace {
    struct ProcessInfo {
        int pid;
        int ppid;
        int uid;
        std::string user;
        std::string state;
        std::string command;
        size_t vsize;  // Virtual memory size
        size_t rss;    // Resident set size
        double cpu_percent;
        double mem_percent;
        chrono::seconds cpu_time;
        std::string tty;
        chrono::system_clock::time_point start_time;
    };
    
    struct Options {
        bool all_users = false;     // -a
        bool all_processes = false; // -e, -A
        bool full_format = false;   // -f
        bool long_format = false;   // -l
        bool user_format = false;   // -u
        bool show_threads = false;  // -H
        bool no_header = false;     // --no-headers
        std::vector<int> pids;      // -p
        std::vector<std::string> users; // -u
    };
    
    // Parse /proc/[pid]/stat file (Linux-specific, but using C++23)
    [[nodiscard]] std::expected<ProcessInfo, std::string>
    parse_proc_stat(int pid) {
        ProcessInfo info;
        info.pid = pid;
        
        auto stat_path = fs::path("/proc") / std::to_string(pid) / "stat";
        if (!fs::exists(stat_path)) {
            return std::unexpected("process not found");
        }
        
        std::ifstream stat_file(stat_path);
        if (!stat_file) {
            return std::unexpected("cannot read stat file");
        }
        
        std::string line;
        std::getline(stat_file, line);
        
        // Parse the stat line (format is complex)
        // Find command in parentheses
        auto paren_start = line.find('(');
        auto paren_end = line.rfind(')');
        if (paren_start != std::string::npos && paren_end != std::string::npos) {
            info.command = line.substr(paren_start + 1, paren_end - paren_start - 1);
        }
        
        // Parse fields after command
        std::istringstream iss(line.substr(paren_end + 2));
        char state;
        iss >> state;
        
        switch (state) {
            case 'R': info.state = "R"; break;  // Running
            case 'S': info.state = "S"; break;  // Sleeping
            case 'D': info.state = "D"; break;  // Disk sleep
            case 'Z': info.state = "Z"; break;  // Zombie
            case 'T': info.state = "T"; break;  // Stopped
            case 'W': info.state = "W"; break;  // Paging
            default: info.state = "?"; break;
        }
        
        iss >> info.ppid;
        
        // Skip to utime and stime (fields 14 and 15)
        for (int i = 0; i < 10; ++i) {
            int dummy;
            iss >> dummy;
        }
        
        unsigned long utime, stime;
        iss >> utime >> stime;
        
        // Convert jiffies to seconds (assuming 100 Hz)
        info.cpu_time = chrono::seconds((utime + stime) / 100);
        
        // Skip to vsize and rss (fields 23 and 24)
        for (int i = 0; i < 6; ++i) {
            long dummy;
            iss >> dummy;
        }
        
        iss >> info.vsize >> info.rss;
        info.rss *= 4;  // Convert pages to KB (assuming 4KB pages)
        
        return info;
    }
    
    // Parse /proc/[pid]/status for additional info
    [[nodiscard]] void enhance_proc_info(ProcessInfo& info) {
        auto status_path = fs::path("/proc") / std::to_string(info.pid) / "status";
        
        std::ifstream status_file(status_path);
        if (!status_file) return;
        
        std::string line;
        while (std::getline(status_file, line)) {
            if (line.starts_with("Uid:")) {
                std::istringstream iss(line.substr(4));
                iss >> info.uid;
                
                // Try to get username from /etc/passwd
                std::ifstream passwd("/etc/passwd");
                std::string passwd_line;
                while (std::getline(passwd, passwd_line)) {
                    if (auto parts = passwd_line | std::views::split(':'); 
                        std::ranges::distance(parts) >= 3) {
                        auto it = parts.begin();
                        std::string_view name(*it);
                        std::advance(it, 2);
                        std::string_view uid_str(*it);
                        
                        int uid_val;
                        auto result = std::from_chars(
                            uid_str.data(), 
                            uid_str.data() + uid_str.size(),
                            uid_val
                        );
                        
                        if (result.ec == std::errc{} && uid_val == info.uid) {
                            info.user = std::string(name);
                            break;
                        }
                    }
                }
                
                if (info.user.empty()) {
                    info.user = std::to_string(info.uid);
                }
            }
        }
    }
    
    // Get terminal name for process
    [[nodiscard]] std::string get_tty(int pid) {
        auto fd_path = fs::path("/proc") / std::to_string(pid) / "fd" / "0";
        
        try {
            if (fs::is_symlink(fd_path)) {
                auto target = fs::read_symlink(fd_path);
                auto target_str = target.string();
                
                if (target_str.starts_with("/dev/pts/")) {
                    return target_str.substr(5);  // Remove "/dev/"
                } else if (target_str.starts_with("/dev/")) {
                    return target_str.substr(5);
                }
            }
        } catch (...) {
            // Ignore errors
        }
        
        return "?";
    }
    
    // Get all process IDs
    [[nodiscard]] std::vector<int> get_all_pids() {
        std::vector<int> pids;
        
        for (const auto& entry : fs::directory_iterator("/proc")) {
            if (entry.is_directory()) {
                auto name = entry.path().filename().string();
                if (std::ranges::all_of(name, [](char c) { 
                    return c >= '0' && c <= '9'; 
                })) {
                    int pid;
                    auto result = std::from_chars(
                        name.data(), 
                        name.data() + name.size(),
                        pid
                    );
                    if (result.ec == std::errc{}) {
                        pids.push_back(pid);
                    }
                }
            }
        }
        
        std::ranges::sort(pids);
        return pids;
    }
    
    // Format time duration
    [[nodiscard]] std::string format_time(chrono::seconds secs) {
        auto hours = chrono::duration_cast<chrono::hours>(secs);
        secs -= hours;
        auto minutes = chrono::duration_cast<chrono::minutes>(secs);
        secs -= minutes;
        
        return std::format("{:02}:{:02}:{:02}", 
                          hours.count(), 
                          minutes.count(), 
                          secs.count());
    }
    
    // Print process info in various formats
    void print_header(const Options& opts) {
        if (opts.no_header) return;
        
        if (opts.full_format) {
            std::cout << "UID        PID  PPID  C STIME TTY          TIME CMD\n";
        } else if (opts.long_format) {
            std::cout << "F S   UID   PID  PPID  C PRI  NI ADDR SZ WCHAN  TTY          TIME CMD\n";
        } else if (opts.user_format) {
            std::cout << "USER       PID %CPU %MEM    VSZ   RSS TTY      STAT START   TIME COMMAND\n";
        } else {
            std::cout << "  PID TTY          TIME CMD\n";
        }
    }
    
    void print_process(const ProcessInfo& info, const Options& opts) {
        if (opts.full_format) {
            std::cout << std::format("{:<10} {:5} {:5}  0 00:00 {:<8} {} {}\n",
                                    info.user,
                                    info.pid,
                                    info.ppid,
                                    info.tty,
                                    format_time(info.cpu_time),
                                    info.command);
        } else if (opts.user_format) {
            std::cout << std::format("{:<10} {:5} {:4.1f} {:4.1f} {:7} {:6} {:<8} {:<4} {:5} {} {}\n",
                                    info.user,
                                    info.pid,
                                    info.cpu_percent,
                                    info.mem_percent,
                                    info.vsize / 1024,  // Convert to KB
                                    info.rss,
                                    info.tty,
                                    info.state,
                                    "00:00",  // START time
                                    format_time(info.cpu_time),
                                    info.command);
        } else {
            std::cout << std::format("{:5} {:<8} {} {}\n",
                                    info.pid,
                                    info.tty,
                                    format_time(info.cpu_time),
                                    info.command);
        }
    }
}

int main(int argc, char* argv[]) {
    Options opts;
    
    // Parse arguments using C++23 span
    std::span<char*> args(argv + 1, argc - 1);
    
    for (const auto& arg_ptr : args) {
        std::string_view arg(arg_ptr);
        
        if (arg == "-a" || arg == "--all") {
            opts.all_users = true;
        } else if (arg == "-A" || arg == "-e" || arg == "--all-processes") {
            opts.all_processes = true;
        } else if (arg == "-f" || arg == "--full") {
            opts.full_format = true;
        } else if (arg == "-l" || arg == "--long") {
            opts.long_format = true;
        } else if (arg == "-u" || arg == "--user-oriented") {
            opts.user_format = true;
        } else if (arg == "-H" || arg == "--show-threads") {
            opts.show_threads = true;
        } else if (arg == "--no-headers") {
            opts.no_header = true;
        } else if (arg == "--help") {
            std::cout << "Usage: ps [OPTIONS]\n";
            std::cout << "Report process status\n\n";
            std::cout << "  -a              all with tty, except session leaders\n";
            std::cout << "  -A, -e          all processes\n";
            std::cout << "  -f              full-format listing\n";
            std::cout << "  -l              long format\n";
            std::cout << "  -u              user-oriented format\n";
            std::cout << "  --no-headers    do not print header\n";
            return 0;
        }
    }
    
    // Get process list
    auto pids = get_all_pids();
    std::vector<ProcessInfo> processes;
    
    // Collect process information
    for (int pid : pids) {
        auto result = parse_proc_stat(pid);
        if (result) {
            auto info = result.value();
            enhance_proc_info(info);
            info.tty = get_tty(pid);
            
            // Filter based on options
            bool include = opts.all_processes;
            
            if (!include && opts.all_users && info.tty != "?") {
                include = true;
            }
            
            if (!include && !opts.all_users && !opts.all_processes) {
                // Default: only current user's processes with tty
                // For simplicity, show all for now
                include = true;
            }
            
            if (include) {
                processes.push_back(info);
            }
        }
    }
    
    // Print results
    print_header(opts);
    for (const auto& proc : processes) {
        print_process(proc, opts);
    }
    
    return 0;
}