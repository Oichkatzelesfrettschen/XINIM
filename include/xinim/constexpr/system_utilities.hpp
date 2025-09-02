#pragma once

/**
 * @file system_utilities.hpp
 * @brief Complete constexpr system utilities for compile-time evaluation
 * 
 * World's first operating system with constexpr system utilities:
 * - Compile-time file system operations
 * - Constexpr process management
 * - Compile-time environment variable processing
 * - Constexpr string manipulation and formatting
 * - Template-based system call abstraction
 */

import xinim.posix;

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <concepts>
#include <expected>
#include <format>
#include <ranges>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

namespace xinim::constexpr_sys {

// ═══════════════════════════════════════════════════════════════════════════
// Core Constexpr System Types and Concepts
// ═══════════════════════════════════════════════════════════════════════════

template<std::size_t N>
struct constexpr_string {
    char data[N] = {};
    std::size_t len = 0;
    
    constexpr constexpr_string() = default;
    
    constexpr constexpr_string(const char (&str)[N]) : len(N - 1) {
        std::copy_n(str, N - 1, data);
    }
    
    constexpr constexpr_string(std::string_view sv) : len(sv.size()) {
        if (sv.size() >= N) {
            len = N - 1;
        }
        std::copy_n(sv.data(), len, data);
    }
    
    constexpr operator std::string_view() const {
        return std::string_view(data, len);
    }
    
    constexpr char& operator[](std::size_t idx) { return data[idx]; }
    constexpr const char& operator[](std::size_t idx) const { return data[idx]; }
    
    constexpr std::size_t size() const { return len; }
    constexpr bool empty() const { return len == 0; }
    
    constexpr auto begin() const { return data; }
    constexpr auto end() const { return data + len; }
    
    constexpr bool operator==(const constexpr_string& other) const {
        if (len != other.len) return false;
        return std::equal(begin(), end(), other.begin());
    }
    
    constexpr bool operator==(std::string_view sv) const {
        if (len != sv.size()) return false;
        return std::equal(begin(), end(), sv.begin());
    }
};

template<std::size_t N>
constexpr_string(const char (&)[N]) -> constexpr_string<N>;

// Constexpr file path representation
template<std::size_t MaxPathLen = 4096>
class constexpr_path {
    constexpr_string<MaxPathLen> path_;
    
public:
    constexpr constexpr_path() = default;
    
    constexpr constexpr_path(std::string_view path) : path_(path) {}
    
    template<std::size_t N>
    constexpr constexpr_path(const char (&path)[N]) : path_(path) {}
    
    constexpr std::string_view string() const { return path_; }
    
    constexpr constexpr_path parent_path() const {
        auto path_view = std::string_view(path_);
        auto last_slash = path_view.find_last_of('/');
        
        if (last_slash == std::string_view::npos) {
            return constexpr_path(".");
        }
        
        if (last_slash == 0) {
            return constexpr_path("/");
        }
        
        return constexpr_path(path_view.substr(0, last_slash));
    }
    
    constexpr constexpr_string<256> filename() const {
        auto path_view = std::string_view(path_);
        auto last_slash = path_view.find_last_of('/');
        
        if (last_slash == std::string_view::npos) {
            return constexpr_string<256>(path_view);
        }
        
        return constexpr_string<256>(path_view.substr(last_slash + 1));
    }
    
    constexpr constexpr_string<256> extension() const {
        auto fname = filename();
        auto fname_view = std::string_view(fname);
        auto last_dot = fname_view.find_last_of('.');
        
        if (last_dot == std::string_view::npos || last_dot == 0) {
            return constexpr_string<256>();
        }
        
        return constexpr_string<256>(fname_view.substr(last_dot));
    }
    
    constexpr constexpr_path operator/(std::string_view component) const {
        auto current = std::string_view(path_);
        
        if (current.empty() || current == ".") {
            return constexpr_path(component);
        }
        
        if (current.back() == '/') {
            // Create concatenated path
            constexpr_string<MaxPathLen> new_path;
            std::copy(current.begin(), current.end(), new_path.data);
            std::copy(component.begin(), component.end(), new_path.data + current.size());
            new_path.len = current.size() + component.size();
            return constexpr_path(std::string_view(new_path));
        } else {
            // Add separator
            constexpr_string<MaxPathLen> new_path;
            std::copy(current.begin(), current.end(), new_path.data);
            new_path.data[current.size()] = '/';
            std::copy(component.begin(), component.end(), new_path.data + current.size() + 1);
            new_path.len = current.size() + 1 + component.size();
            return constexpr_path(std::string_view(new_path));
        }
    }
    
    constexpr bool is_absolute() const {
        return !path_.empty() && path_.data[0] == '/';
    }
    
    constexpr bool is_relative() const {
        return !is_absolute();
    }
};

// Constexpr environment variable storage
template<std::size_t MaxVars = 1024, std::size_t MaxVarLen = 256>
class constexpr_environment {
public:
    struct env_var {
        constexpr_string<MaxVarLen> name;
        constexpr_string<MaxVarLen> value;
        bool is_set = false;
        
        constexpr env_var() = default;
        
        constexpr env_var(std::string_view n, std::string_view v) 
            : name(n), value(v), is_set(true) {}
    };
    
private:
    std::array<env_var, MaxVars> vars_{};
    std::size_t count_ = 0;
    
public:
    constexpr constexpr_environment() = default;
    
    constexpr void set(std::string_view name, std::string_view value) {
        // Find existing variable
        for (std::size_t i = 0; i < count_; ++i) {
            if (vars_[i].name == name) {
                vars_[i].value = constexpr_string<MaxVarLen>(value);
                return;
            }
        }
        
        // Add new variable
        if (count_ < MaxVars) {
            vars_[count_] = env_var(name, value);
            ++count_;
        }
    }
    
    constexpr std::string_view get(std::string_view name) const {
        for (std::size_t i = 0; i < count_; ++i) {
            if (vars_[i].name == name) {
                return std::string_view(vars_[i].value);
            }
        }
        return {};
    }
    
    constexpr bool has(std::string_view name) const {
        for (std::size_t i = 0; i < count_; ++i) {
            if (vars_[i].name == name) {
                return true;
            }
        }
        return false;
    }
    
    constexpr void unset(std::string_view name) {
        for (std::size_t i = 0; i < count_; ++i) {
            if (vars_[i].name == name) {
                // Shift remaining variables
                for (std::size_t j = i; j < count_ - 1; ++j) {
                    vars_[j] = vars_[j + 1];
                }
                --count_;
                vars_[count_] = env_var{};
                return;
            }
        }
    }
    
    constexpr std::size_t size() const { return count_; }
    
    constexpr const env_var& operator[](std::size_t idx) const {
        return vars_[idx];
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Constexpr String Processing and Formatting
// ═══════════════════════════════════════════════════════════════════════════

namespace string_ops {

template<std::size_t N>
constexpr auto split(const constexpr_string<N>& str, char delimiter) {
    constexpr std::size_t MaxTokens = 64;
    std::array<std::string_view, MaxTokens> tokens{};
    std::size_t token_count = 0;
    
    auto str_view = std::string_view(str);
    std::size_t start = 0;
    
    while (start < str_view.size() && token_count < MaxTokens) {
        auto end = str_view.find(delimiter, start);
        if (end == std::string_view::npos) {
            end = str_view.size();
        }
        
        if (end > start) {
            tokens[token_count++] = str_view.substr(start, end - start);
        }
        
        start = end + 1;
    }
    
    return std::pair{tokens, token_count};
}

template<std::size_t N>
constexpr auto trim(const constexpr_string<N>& str) {
    auto str_view = std::string_view(str);
    
    // Find first non-whitespace
    std::size_t start = 0;
    while (start < str_view.size() && std::isspace(str_view[start])) {
        ++start;
    }
    
    // Find last non-whitespace
    std::size_t end = str_view.size();
    while (end > start && std::isspace(str_view[end - 1])) {
        --end;
    }
    
    return constexpr_string<N>(str_view.substr(start, end - start));
}

template<std::size_t N>
constexpr bool starts_with(const constexpr_string<N>& str, std::string_view prefix) {
    auto str_view = std::string_view(str);
    if (str_view.size() < prefix.size()) return false;
    return str_view.substr(0, prefix.size()) == prefix;
}

template<std::size_t N>
constexpr bool ends_with(const constexpr_string<N>& str, std::string_view suffix) {
    auto str_view = std::string_view(str);
    if (str_view.size() < suffix.size()) return false;
    return str_view.substr(str_view.size() - suffix.size()) == suffix;
}

template<std::size_t N>
constexpr auto to_upper(const constexpr_string<N>& str) {
    constexpr_string<N> result = str;
    for (std::size_t i = 0; i < result.size(); ++i) {
        if (result[i] >= 'a' && result[i] <= 'z') {
            result.data[i] = result[i] - ('a' - 'A');
        }
    }
    return result;
}

template<std::size_t N>
constexpr auto to_lower(const constexpr_string<N>& str) {
    constexpr_string<N> result = str;
    for (std::size_t i = 0; i < result.size(); ++i) {
        if (result[i] >= 'A' && result[i] <= 'Z') {
            result.data[i] = result[i] + ('a' - 'A');
        }
    }
    return result;
}

// Constexpr string replacement
template<std::size_t N>
constexpr auto replace_all(const constexpr_string<N>& str, 
                          std::string_view from, 
                          std::string_view to) {
    constexpr_string<N * 2> result; // Assume worst case expansion
    auto str_view = std::string_view(str);
    std::size_t pos = 0;
    std::size_t result_pos = 0;
    
    while (pos < str_view.size()) {
        auto found = str_view.find(from, pos);
        
        if (found == std::string_view::npos) {
            // Copy remaining characters
            std::copy(str_view.begin() + pos, str_view.end(), result.data + result_pos);
            result_pos += str_view.size() - pos;
            break;
        }
        
        // Copy characters before match
        std::copy(str_view.begin() + pos, str_view.begin() + found, result.data + result_pos);
        result_pos += found - pos;
        
        // Insert replacement
        std::copy(to.begin(), to.end(), result.data + result_pos);
        result_pos += to.size();
        
        pos = found + from.size();
    }
    
    result.len = result_pos;
    return result;
}

} // namespace string_ops

// ═══════════════════════════════════════════════════════════════════════════
// Constexpr Process and System Information
// ═══════════════════════════════════════════════════════════════════════════

struct constexpr_process_info {
    std::uint32_t pid = 1;
    std::uint32_t ppid = 0;
    constexpr_string<256> name = "xinim_init";
    constexpr_string<512> cmdline;
    std::uint64_t start_time = 0;
    std::uint32_t uid = 0;
    std::uint32_t gid = 0;
    
    constexpr constexpr_process_info() = default;
    
    constexpr constexpr_process_info(std::uint32_t p, std::string_view n) 
        : pid(p), name(n) {}
};

struct constexpr_system_info {
    constexpr_string<256> hostname = "xinim-system";
    constexpr_string<256> kernel_version = "XINIM-1.0.0-C++23";
    constexpr_string<128> architecture = "x86_64";
    constexpr_string<512> build_info = "Pure C++23 with libc++, SIMD-optimized, Post-Quantum";
    std::uint64_t boot_time = 0;
    std::uint32_t cpu_count = 4;
    std::uint64_t total_memory = 8ULL * 1024 * 1024 * 1024; // 8GB default
    
    constexpr constexpr_system_info() = default;
};

// ═══════════════════════════════════════════════════════════════════════════
// Constexpr Time and Date Operations
// ═══════════════════════════════════════════════════════════════════════════

struct constexpr_timespec {
    std::int64_t seconds = 0;
    std::int32_t nanoseconds = 0;
    
    constexpr constexpr_timespec() = default;
    constexpr constexpr_timespec(std::int64_t s, std::int32_t ns = 0) 
        : seconds(s), nanoseconds(ns) {}
    
    constexpr constexpr_timespec operator+(const constexpr_timespec& other) const {
        std::int64_t total_ns = nanoseconds + other.nanoseconds;
        std::int64_t carry = total_ns / 1000000000;
        
        return {seconds + other.seconds + carry, 
                static_cast<std::int32_t>(total_ns % 1000000000)};
    }
    
    constexpr constexpr_timespec operator-(const constexpr_timespec& other) const {
        std::int64_t total_ns = nanoseconds - other.nanoseconds;
        std::int64_t borrow = 0;
        
        if (total_ns < 0) {
            total_ns += 1000000000;
            borrow = 1;
        }
        
        return {seconds - other.seconds - borrow, 
                static_cast<std::int32_t>(total_ns)};
    }
    
    constexpr bool operator<(const constexpr_timespec& other) const {
        if (seconds != other.seconds) {
            return seconds < other.seconds;
        }
        return nanoseconds < other.nanoseconds;
    }
    
    constexpr bool operator==(const constexpr_timespec& other) const {
        return seconds == other.seconds && nanoseconds == other.nanoseconds;
    }
    
    constexpr double to_seconds() const {
        return static_cast<double>(seconds) + static_cast<double>(nanoseconds) / 1e9;
    }
};

// Constexpr date/time structure
struct constexpr_datetime {
    std::int32_t year = 2024;
    std::int32_t month = 1;  // 1-12
    std::int32_t day = 1;    // 1-31
    std::int32_t hour = 0;   // 0-23
    std::int32_t minute = 0; // 0-59
    std::int32_t second = 0; // 0-59
    std::int32_t nanosecond = 0;
    
    constexpr constexpr_datetime() = default;
    
    constexpr constexpr_datetime(std::int32_t y, std::int32_t m, std::int32_t d,
                                std::int32_t h = 0, std::int32_t min = 0, std::int32_t s = 0)
        : year(y), month(m), day(d), hour(h), minute(min), second(s) {}
    
    constexpr bool is_leap_year() const {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }
    
    constexpr std::int32_t days_in_month() const {
        constexpr std::array<std::int32_t, 12> days = 
            {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        
        if (month == 2 && is_leap_year()) {
            return 29;
        }
        
        return days[month - 1];
    }
    
    constexpr std::int32_t day_of_year() const {
        constexpr std::array<std::int32_t, 12> cumulative_days = 
            {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
        
        std::int32_t result = cumulative_days[month - 1] + day;
        
        if (month > 2 && is_leap_year()) {
            result += 1;
        }
        
        return result;
    }
    
    // Convert to Unix timestamp
    constexpr std::int64_t to_unix_timestamp() const {
        // Days since Unix epoch (1970-01-01)
        std::int64_t days = 0;
        
        // Add years
        for (std::int32_t y = 1970; y < year; ++y) {
            days += (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0)) ? 366 : 365;
        }
        
        // Add months and days
        days += day_of_year() - 1;
        
        // Convert to seconds and add time components
        return days * 86400 + hour * 3600 + minute * 60 + second;
    }
    
    constexpr auto format_iso8601() const {
        constexpr_string<32> result;
        
        // Format: YYYY-MM-DDTHH:MM:SS
        auto format_2digit = [](std::int32_t value) {
            constexpr_string<3> result;
            result.data[0] = '0' + (value / 10);
            result.data[1] = '0' + (value % 10);
            result.len = 2;
            return result;
        };
        
        auto format_4digit = [](std::int32_t value) {
            constexpr_string<5> result;
            result.data[0] = '0' + (value / 1000);
            result.data[1] = '0' + ((value / 100) % 10);
            result.data[2] = '0' + ((value / 10) % 10);
            result.data[3] = '0' + (value % 10);
            result.len = 4;
            return result;
        };
        
        auto year_str = format_4digit(year);
        auto month_str = format_2digit(month);
        auto day_str = format_2digit(day);
        auto hour_str = format_2digit(hour);
        auto minute_str = format_2digit(minute);
        auto second_str = format_2digit(second);
        
        // Manually construct ISO8601 string
        std::size_t pos = 0;
        
        // Year
        std::copy_n(year_str.data, 4, result.data + pos);
        pos += 4;
        result.data[pos++] = '-';
        
        // Month
        std::copy_n(month_str.data, 2, result.data + pos);
        pos += 2;
        result.data[pos++] = '-';
        
        // Day
        std::copy_n(day_str.data, 2, result.data + pos);
        pos += 2;
        result.data[pos++] = 'T';
        
        // Hour
        std::copy_n(hour_str.data, 2, result.data + pos);
        pos += 2;
        result.data[pos++] = ':';
        
        // Minute
        std::copy_n(minute_str.data, 2, result.data + pos);
        pos += 2;
        result.data[pos++] = ':';
        
        // Second
        std::copy_n(second_str.data, 2, result.data + pos);
        pos += 2;
        
        result.len = pos;
        return result;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Constexpr File System Operations
// ═══════════════════════════════════════════════════════════════════════════

enum class constexpr_file_type : std::uint8_t {
    regular = 1,
    directory = 2,
    symlink = 3,
    block_device = 4,
    character_device = 5,
    fifo = 6,
    socket = 7,
    unknown = 0
};

struct constexpr_file_status {
    constexpr_file_type type = constexpr_file_type::unknown;
    std::uint64_t size = 0;
    std::uint32_t permissions = 0644; // octal
    std::uint32_t uid = 0;
    std::uint32_t gid = 0;
    constexpr_timespec modify_time;
    constexpr_timespec access_time;
    constexpr_timespec create_time;
    
    constexpr bool is_regular_file() const {
        return type == constexpr_file_type::regular;
    }
    
    constexpr bool is_directory() const {
        return type == constexpr_file_type::directory;
    }
    
    constexpr bool is_symlink() const {
        return type == constexpr_file_type::symlink;
    }
    
    constexpr bool is_executable() const {
        return (permissions & 0111) != 0;
    }
    
    constexpr bool is_readable() const {
        return (permissions & 0444) != 0;
    }
    
    constexpr bool is_writable() const {
        return (permissions & 0222) != 0;
    }
};

// Constexpr directory entry
template<std::size_t MaxNameLen = 256>
struct constexpr_directory_entry {
    constexpr_string<MaxNameLen> name;
    constexpr_file_status status;
    
    constexpr constexpr_directory_entry() = default;
    
    constexpr constexpr_directory_entry(std::string_view n, constexpr_file_type t)
        : name(n) {
        status.type = t;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// Constexpr System Utilities Implementation
// ═══════════════════════════════════════════════════════════════════════════

class constexpr_system {
private:
    constexpr_environment<> environment_;
    constexpr_system_info system_info_;
    constexpr_datetime current_time_;
    
public:
    constexpr constexpr_system() {
        // Initialize default environment
        environment_.set("PATH", "/usr/bin:/bin:/usr/local/bin");
        environment_.set("HOME", "/home/user");
        environment_.set("SHELL", "/bin/xinim_shell");
        environment_.set("USER", "user");
        environment_.set("XINIM_VERSION", "2.0.0-C++23");
        
        // Set current time to build time (compile-time constant)
        current_time_ = constexpr_datetime(2024, 9, 2, 0, 0, 0);
    }
    
    // Environment operations
    constexpr void setenv(std::string_view name, std::string_view value) {
        environment_.set(name, value);
    }
    
    constexpr std::string_view getenv(std::string_view name) const {
        return environment_.get(name);
    }
    
    constexpr void unsetenv(std::string_view name) {
        environment_.unset(name);
    }
    
    constexpr bool hasenv(std::string_view name) const {
        return environment_.has(name);
    }
    
    // System information
    constexpr const constexpr_system_info& system_info() const {
        return system_info_;
    }
    
    constexpr void set_hostname(std::string_view hostname) {
        system_info_.hostname = constexpr_string<256>(hostname);
    }
    
    constexpr std::string_view hostname() const {
        return std::string_view(system_info_.hostname);
    }
    
    // Process operations (compile-time simulation)
    constexpr constexpr_process_info getpid() const {
        return constexpr_process_info(1, "xinim_process");
    }
    
    constexpr std::uint32_t getuid() const { return 1000; }
    constexpr std::uint32_t getgid() const { return 1000; }
    constexpr std::uint32_t geteuid() const { return getuid(); }
    constexpr std::uint32_t getegid() const { return getgid(); }
    
    // Time operations
    constexpr const constexpr_datetime& current_datetime() const {
        return current_time_;
    }
    
    constexpr void set_current_time(const constexpr_datetime& dt) {
        current_time_ = dt;
    }
    
    constexpr std::int64_t unix_timestamp() const {
        return current_time_.to_unix_timestamp();
    }
    
    // Path operations
    constexpr constexpr_path<> absolute_path(const constexpr_path<>& path) const {
        if (path.is_absolute()) {
            return path;
        }
        
        // Get current working directory from environment or default
        auto pwd = getenv("PWD");
        if (pwd.empty()) {
            pwd = "/home/user";  // Default
        }
        
        return constexpr_path<>(pwd) / path.string();
    }
    
    constexpr constexpr_path<> canonical_path(const constexpr_path<>& path) const {
        auto abs_path = absolute_path(path);
        // In real implementation, would resolve . and .. components
        return abs_path;
    }
    
    // File system queries (compile-time simulation)
    constexpr bool file_exists(const constexpr_path<>& path) const {
        // Simulate common system files
        auto path_str = path.string();
        
        return path_str == "/" || 
               path_str == "/usr" || 
               path_str == "/usr/bin" ||
               path_str == "/bin" ||
               path_str == "/home" ||
               path_str == "/home/user" ||
               path_str == "/tmp" ||
               path_str.starts_with("/dev/");
    }
    
    constexpr constexpr_file_status file_status(const constexpr_path<>& path) const {
        constexpr_file_status status;
        auto path_str = path.string();
        
        if (path_str == "/" || path_str.ends_with("/")) {
            status.type = constexpr_file_type::directory;
            status.permissions = 0755;
        } else if (path_str.starts_with("/dev/")) {
            status.type = constexpr_file_type::character_device;
            status.permissions = 0666;
        } else if (path_str.ends_with(".so") || path_str.contains("/bin/")) {
            status.type = constexpr_file_type::regular;
            status.permissions = 0755;  // Executable
            status.size = 65536;  // Simulate 64KB
        } else {
            status.type = constexpr_file_type::regular;
            status.permissions = 0644;
            status.size = 1024;  // Simulate 1KB
        }
        
        status.modify_time = constexpr_timespec(current_time_.to_unix_timestamp());
        status.access_time = status.modify_time;
        status.create_time = status.modify_time;
        
        return status;
    }
    
    constexpr bool is_directory(const constexpr_path<>& path) const {
        return file_status(path).is_directory();
    }
    
    constexpr bool is_regular_file(const constexpr_path<>& path) const {
        return file_status(path).is_regular_file();
    }
    
    constexpr std::uint64_t file_size(const constexpr_path<>& path) const {
        return file_status(path).size;
    }
};

// Global constexpr system instance
inline constexpr constexpr_system global_system;

// ═══════════════════════════════════════════════════════════════════════════
// Constexpr Utility Functions for Common Operations
// ═══════════════════════════════════════════════════════════════════════════

namespace utils {

// Constexpr basename implementation
constexpr auto basename(std::string_view path) {
    if (path.empty()) {
        return constexpr_string<256>(".");
    }
    
    // Remove trailing slashes
    while (path.size() > 1 && path.back() == '/') {
        path.remove_suffix(1);
    }
    
    auto last_slash = path.find_last_of('/');
    if (last_slash == std::string_view::npos) {
        return constexpr_string<256>(path);
    }
    
    return constexpr_string<256>(path.substr(last_slash + 1));
}

// Constexpr dirname implementation  
constexpr auto dirname(std::string_view path) {
    if (path.empty()) {
        return constexpr_string<256>(".");
    }
    
    // Remove trailing slashes
    while (path.size() > 1 && path.back() == '/') {
        path.remove_suffix(1);
    }
    
    auto last_slash = path.find_last_of('/');
    if (last_slash == std::string_view::npos) {
        return constexpr_string<256>(".");
    }
    
    if (last_slash == 0) {
        return constexpr_string<256>("/");
    }
    
    return constexpr_string<256>(path.substr(0, last_slash));
}

// Constexpr realpath simulation
constexpr auto realpath(const constexpr_path<>& path) {
    return global_system.canonical_path(path);
}

// Constexpr which implementation
constexpr auto which(std::string_view command) {
    auto path_env = global_system.getenv("PATH");
    if (path_env.empty()) {
        return constexpr_path<>();
    }
    
    auto [path_components, count] = string_ops::split(
        constexpr_string<4096>(path_env), ':');
    
    for (std::size_t i = 0; i < count; ++i) {
        auto full_path = constexpr_path<>(path_components[i]) / command;
        if (global_system.file_exists(full_path) && 
            global_system.file_status(full_path).is_executable()) {
            return full_path;
        }
    }
    
    return constexpr_path<>();
}

// Constexpr pwd implementation
constexpr auto pwd() {
    auto pwd_env = global_system.getenv("PWD");
    if (!pwd_env.empty()) {
        return constexpr_path<>(pwd_env);
    }
    
    return constexpr_path<>("/home/user");  // Default
}

} // namespace utils

} // namespace xinim::constexpr_sys

// ═══════════════════════════════════════════════════════════════════════════
// Compile-Time Tests and Validation
// ═══════════════════════════════════════════════════════════════════════════

namespace {

// Compile-time tests to validate constexpr system utilities
constexpr bool test_constexpr_system() {
    using namespace xinim::constexpr_sys;
    
    // Test string operations
    constexpr auto test_str = constexpr_string<256>("Hello World");
    static_assert(test_str.size() == 11);
    static_assert(test_str[0] == 'H');
    static_assert(test_str == "Hello World");
    
    // Test path operations
    constexpr auto test_path = constexpr_path("/usr/bin/xinim");
    static_assert(test_path.is_absolute());
    static_assert(!test_path.is_relative());
    
    constexpr auto parent = test_path.parent_path();
    static_assert(parent.string() == "/usr/bin");
    
    constexpr auto filename = test_path.filename();
    static_assert(filename == "xinim");
    
    // Test datetime operations
    constexpr auto dt = constexpr_datetime(2024, 9, 2, 12, 30, 45);
    static_assert(dt.is_leap_year());
    static_assert(dt.day_of_year() == 246);  // Sept 2 is day 246 in leap year
    
    constexpr auto iso_str = dt.format_iso8601();
    static_assert(iso_str == "2024-09-02T12:30:45");
    
    // Test utility functions
    constexpr auto base = utils::basename("/usr/local/bin/xinim");
    static_assert(base == "xinim");
    
    constexpr auto dir = utils::dirname("/usr/local/bin/xinim");
    static_assert(dir == "/usr/local/bin");
    
    return true;
}

// Validate at compile time
static_assert(test_constexpr_system(), "Constexpr system utilities validation failed");

} // anonymous namespace