// Constexpr Date Utility - Compile-Time Date/Time Calculations
// Revolutionary C++23 implementation with compile-time computation

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <format>
#include <iostream>
#include <span>
#include <string>
#include <string_view>

using namespace std::chrono;
using namespace std::string_literals;

// ═══════════════════════════════════════════════════════════════════════════
// COMPILE-TIME DATE/TIME COMPUTATION ENGINE
// ═══════════════════════════════════════════════════════════════════════════

namespace constexpr_date {

// Constexpr string for compile-time string processing
template<std::size_t N>
struct constexpr_string {
    std::array<char, N> data{};
    std::size_t len = 0;
    
    constexpr constexpr_string() = default;
    
    constexpr constexpr_string(const char (&str)[N]) {
        std::copy_n(str, N-1, data.begin());
        len = N - 1;
    }
    
    constexpr constexpr_string(std::string_view sv) {
        auto copy_len = std::min(N-1, sv.size());
        std::copy_n(sv.begin(), copy_len, data.begin());
        len = copy_len;
    }
    
    constexpr std::string_view view() const noexcept {
        return {data.data(), len};
    }
    
    constexpr char operator[](std::size_t i) const noexcept {
        return i < len ? data[i] : '\0';
    }
    
    constexpr std::size_t size() const noexcept { return len; }
    
    constexpr bool empty() const noexcept { return len == 0; }
    
    // Constexpr string concatenation
    template<std::size_t M>
    constexpr auto operator+(const constexpr_string<M>& other) const {
        constexpr_string<N + M - 1> result;
        std::copy_n(data.begin(), len, result.data.begin());
        std::copy_n(other.data.begin(), other.len, result.data.begin() + len);
        result.len = len + other.len;
        return result;
    }
};

// Deduction guide
template<std::size_t N>
constexpr_string(const char (&)[N]) -> constexpr_string<N>;

// Constexpr date representation
struct constexpr_date {
    std::int32_t year = 1970;
    std::uint8_t month = 1;   // 1-12
    std::uint8_t day = 1;     // 1-31
    
    constexpr constexpr_date() = default;
    constexpr constexpr_date(std::int32_t y, std::uint8_t m, std::uint8_t d) 
        : year(y), month(m), day(d) {}
    
    // Check if year is leap year
    constexpr bool is_leap_year() const noexcept {
        return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
    }
    
    // Get days in month
    constexpr std::uint8_t days_in_month() const noexcept {
        constexpr std::array<std::uint8_t, 12> days = {
            31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
        };
        
        if (month == 2 && is_leap_year()) {
            return 29;
        }
        return days[month - 1];
    }
    
    // Convert to days since epoch (1970-01-01)
    constexpr std::int64_t to_days_since_epoch() const noexcept {
        std::int64_t days = 0;
        
        // Add days for complete years
        for (std::int32_t y = 1970; y < year; ++y) {
            days += (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0) ? 366 : 365;
        }
        
        // Add days for complete months in current year
        for (std::uint8_t m = 1; m < month; ++m) {
            constexpr std::array<std::uint8_t, 12> month_days = {
                31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
            };
            days += month_days[m - 1];
            if (m == 2 && is_leap_year()) {
                ++days;
            }
        }
        
        // Add remaining days
        days += day - 1;
        
        return days;
    }
    
    // Get day of week (0 = Sunday, 6 = Saturday)
    constexpr std::uint8_t day_of_week() const noexcept {
        // 1970-01-01 was a Thursday (4)
        return (to_days_since_epoch() + 4) % 7;
    }
    
    // Get day of year (1-366)
    constexpr std::uint16_t day_of_year() const noexcept {
        std::uint16_t days = day;
        
        for (std::uint8_t m = 1; m < month; ++m) {
            constexpr std::array<std::uint8_t, 12> month_days = {
                31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
            };
            days += month_days[m - 1];
            if (m == 2 && is_leap_year()) {
                ++days;
            }
        }
        
        return days;
    }
    
    // Add days to date
    constexpr constexpr_date operator+(std::int64_t days) const noexcept {
        constexpr_date result = *this;
        
        if (days >= 0) {
            while (days > 0) {
                std::uint8_t days_in_current_month = result.days_in_month();
                std::uint8_t days_left_in_month = days_in_current_month - result.day + 1;
                
                if (days >= days_left_in_month) {
                    days -= days_left_in_month;
                    result.day = 1;
                    ++result.month;
                    if (result.month > 12) {
                        result.month = 1;
                        ++result.year;
                    }
                } else {
                    result.day += static_cast<std::uint8_t>(days);
                    days = 0;
                }
            }
        } else {
            // Handle negative days (subtract)
            days = -days;
            while (days > 0) {
                if (days >= result.day) {
                    days -= result.day;
                    --result.month;
                    if (result.month == 0) {
                        result.month = 12;
                        --result.year;
                    }
                    result.day = result.days_in_month();
                } else {
                    result.day -= static_cast<std::uint8_t>(days);
                    days = 0;
                }
            }
        }
        
        return result;
    }
    
    // Subtract dates to get difference in days
    constexpr std::int64_t operator-(const constexpr_date& other) const noexcept {
        return to_days_since_epoch() - other.to_days_since_epoch();
    }
    
    // Format date as string
    template<std::size_t N = 32>
    constexpr auto format(std::string_view fmt = "%Y-%m-%d") const -> constexpr_string<N> {
        constexpr_string<N> result;
        std::size_t pos = 0;
        
        for (std::size_t i = 0; i < fmt.size() && pos < N - 1; ++i) {
            if (fmt[i] == '%' && i + 1 < fmt.size()) {
                switch (fmt[i + 1]) {
                    case 'Y': {  // 4-digit year
                        auto year_str = to_constexpr_string<5>(year);
                        for (char c : year_str.view()) {
                            if (pos < N - 1) result.data[pos++] = c;
                        }
                        ++i;  // Skip format specifier
                        break;
                    }
                    case 'm': {  // Month as 2-digit number
                        if (month < 10 && pos < N - 1) result.data[pos++] = '0';
                        auto month_str = to_constexpr_string<3>(month);
                        for (char c : month_str.view()) {
                            if (pos < N - 1) result.data[pos++] = c;
                        }
                        ++i;
                        break;
                    }
                    case 'd': {  // Day as 2-digit number
                        if (day < 10 && pos < N - 1) result.data[pos++] = '0';
                        auto day_str = to_constexpr_string<3>(day);
                        for (char c : day_str.view()) {
                            if (pos < N - 1) result.data[pos++] = c;
                        }
                        ++i;
                        break;
                    }
                    case 'j': {  // Day of year
                        auto doy = day_of_year();
                        if (doy < 100 && pos < N - 1) result.data[pos++] = '0';
                        if (doy < 10 && pos < N - 1) result.data[pos++] = '0';
                        auto doy_str = to_constexpr_string<4>(doy);
                        for (char c : doy_str.view()) {
                            if (pos < N - 1) result.data[pos++] = c;
                        }
                        ++i;
                        break;
                    }
                    case 'w': {  // Day of week
                        auto dow = day_of_week();
                        if (pos < N - 1) result.data[pos++] = '0' + dow;
                        ++i;
                        break;
                    }
                    default:
                        if (pos < N - 1) result.data[pos++] = fmt[i];
                        break;
                }
            } else {
                if (pos < N - 1) result.data[pos++] = fmt[i];
            }
        }
        
        result.len = pos;
        return result;
    }
    
private:
    // Convert integer to constexpr string
    template<std::size_t N>
    constexpr auto to_constexpr_string(std::int64_t value) const -> constexpr_string<N> {
        constexpr_string<N> result;
        
        if (value == 0) {
            result.data[0] = '0';
            result.len = 1;
            return result;
        }
        
        bool negative = value < 0;
        if (negative) value = -value;
        
        std::size_t pos = 0;
        while (value > 0 && pos < N - 1) {
            result.data[pos++] = '0' + (value % 10);
            value /= 10;
        }
        
        if (negative && pos < N - 1) {
            result.data[pos++] = '-';
        }
        
        // Reverse the string
        for (std::size_t i = 0; i < pos / 2; ++i) {
            std::swap(result.data[i], result.data[pos - 1 - i]);
        }
        
        result.len = pos;
        return result;
    }
};

// Constexpr time representation
struct constexpr_time {
    std::uint8_t hour = 0;    // 0-23
    std::uint8_t minute = 0;  // 0-59
    std::uint8_t second = 0;  // 0-59
    std::uint32_t nanosecond = 0;
    
    constexpr constexpr_time() = default;
    constexpr constexpr_time(std::uint8_t h, std::uint8_t m, std::uint8_t s = 0, std::uint32_t ns = 0)
        : hour(h), minute(m), second(s), nanosecond(ns) {}
    
    // Convert to seconds since midnight
    constexpr std::uint64_t to_seconds_since_midnight() const noexcept {
        return hour * 3600ULL + minute * 60ULL + second;
    }
    
    // Format time as string
    template<std::size_t N = 16>
    constexpr auto format(std::string_view fmt = "%H:%M:%S") const -> constexpr_string<N> {
        constexpr_string<N> result;
        std::size_t pos = 0;
        
        for (std::size_t i = 0; i < fmt.size() && pos < N - 1; ++i) {
            if (fmt[i] == '%' && i + 1 < fmt.size()) {
                switch (fmt[i + 1]) {
                    case 'H': {  // Hour as 2-digit (24-hour)
                        if (hour < 10 && pos < N - 1) result.data[pos++] = '0';
                        auto hour_str = to_constexpr_string<3>(hour);
                        for (char c : hour_str.view()) {
                            if (pos < N - 1) result.data[pos++] = c;
                        }
                        ++i;
                        break;
                    }
                    case 'M': {  // Minute as 2-digit
                        if (minute < 10 && pos < N - 1) result.data[pos++] = '0';
                        auto minute_str = to_constexpr_string<3>(minute);
                        for (char c : minute_str.view()) {
                            if (pos < N - 1) result.data[pos++] = c;
                        }
                        ++i;
                        break;
                    }
                    case 'S': {  // Second as 2-digit
                        if (second < 10 && pos < N - 1) result.data[pos++] = '0';
                        auto second_str = to_constexpr_string<3>(second);
                        for (char c : second_str.view()) {
                            if (pos < N - 1) result.data[pos++] = c;
                        }
                        ++i;
                        break;
                    }
                    default:
                        if (pos < N - 1) result.data[pos++] = fmt[i];
                        break;
                }
            } else {
                if (pos < N - 1) result.data[pos++] = fmt[i];
            }
        }
        
        result.len = pos;
        return result;
    }
    
private:
    template<std::size_t N>
    constexpr auto to_constexpr_string(std::int64_t value) const -> constexpr_string<N> {
        constexpr_string<N> result;
        
        if (value == 0) {
            result.data[0] = '0';
            result.len = 1;
            return result;
        }
        
        std::size_t pos = 0;
        while (value > 0 && pos < N - 1) {
            result.data[pos++] = '0' + (value % 10);
            value /= 10;
        }
        
        // Reverse
        for (std::size_t i = 0; i < pos / 2; ++i) {
            std::swap(result.data[i], result.data[pos - 1 - i]);
        }
        
        result.len = pos;
        return result;
    }
};

// Combined date-time
struct constexpr_datetime {
    constexpr_date date;
    constexpr_time time;
    
    constexpr constexpr_datetime() = default;
    constexpr constexpr_datetime(const constexpr_date& d, const constexpr_time& t)
        : date(d), time(t) {}
        
    template<std::size_t N = 64>
    constexpr auto format(std::string_view fmt = "%Y-%m-%d %H:%M:%S") const -> constexpr_string<N> {
        // Simple implementation - in production would handle mixed format specifiers
        auto date_part = date.format<32>(fmt);
        auto time_part = time.format<32>(fmt);
        
        // For now, just return ISO format
        auto date_str = date.format<32>();
        constexpr_string<N> result;
        std::size_t pos = 0;
        
        // Copy date
        for (char c : date_str.view()) {
            if (pos < N - 1) result.data[pos++] = c;
        }
        
        // Add space
        if (pos < N - 1) result.data[pos++] = ' ';
        
        // Copy time
        auto time_str = time.format<16>();
        for (char c : time_str.view()) {
            if (pos < N - 1) result.data[pos++] = c;
        }
        
        result.len = pos;
        return result;
    }
};

} // namespace constexpr_date

// ═══════════════════════════════════════════════════════════════════════════
// COMPILE-TIME DATE CALCULATIONS
// ═══════════════════════════════════════════════════════════════════════════

// Compile-time date parsing and formatting
class ConstexprDateUtility {
private:
    using Date = constexpr_date::constexpr_date;
    using Time = constexpr_date::constexpr_time;
    using DateTime = constexpr_date::constexpr_datetime;
    
    // Parse simple date format at compile time
    static constexpr Date parse_date(std::string_view date_str) {
        // Simple YYYY-MM-DD parser
        if (date_str.size() != 10 || date_str[4] != '-' || date_str[7] != '-') {
            return Date{1970, 1, 1};  // Default on parse error
        }
        
        std::int32_t year = 0;
        for (std::size_t i = 0; i < 4; ++i) {
            if (date_str[i] >= '0' && date_str[i] <= '9') {
                year = year * 10 + (date_str[i] - '0');
            }
        }
        
        std::uint8_t month = 0;
        for (std::size_t i = 5; i < 7; ++i) {
            if (date_str[i] >= '0' && date_str[i] <= '9') {
                month = month * 10 + (date_str[i] - '0');
            }
        }
        
        std::uint8_t day = 0;
        for (std::size_t i = 8; i < 10; ++i) {
            if (date_str[i] >= '0' && date_str[i] <= '9') {
                day = day * 10 + (date_str[i] - '0');
            }
        }
        
        return Date{year, month, day};
    }
    
public:
    static int execute(std::span<std::string_view> args) {
        // Get current time
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto* tm = std::gmtime(&time_t);
        
        Date current_date{tm->tm_year + 1900, 
                         static_cast<std::uint8_t>(tm->tm_mon + 1), 
                         static_cast<std::uint8_t>(tm->tm_mday)};
        
        Time current_time{static_cast<std::uint8_t>(tm->tm_hour),
                         static_cast<std::uint8_t>(tm->tm_min),
                         static_cast<std::uint8_t>(tm->tm_sec)};
        
        // Parse arguments
        std::string_view format = "%a %b %d %H:%M:%S %Z %Y";  // Default date format
        bool utc = false;
        std::string_view set_date;
        
        for (std::size_t i = 0; i < args.size(); ++i) {
            std::string_view arg = args[i];
            
            if (arg == "-u" || arg == "--utc") {
                utc = true;
            } else if (arg == "-d" || arg == "--date") {
                if (i + 1 < args.size()) {
                    set_date = args[++i];
                }
            } else if (arg.starts_with("+")) {
                format = arg.substr(1);  // Remove '+'
            } else if (arg == "--help") {
                std::cout << "Usage: date [OPTION]... [+FORMAT]\n";
                std::cout << "Display the current time in FORMAT, or set the system date.\n\n";
                std::cout << "  -d, --date=STRING     display time described by STRING\n";
                std::cout << "  -u, --utc             Coordinated Universal Time (UTC)\n";
                std::cout << "\nFORMAT controls the output.  Interpreted sequences are:\n";
                std::cout << "  %Y   year (e.g., 2023)\n";
                std::cout << "  %m   month (01..12)\n";
                std::cout << "  %d   day of month (01..31)\n";
                std::cout << "  %H   hour (00..23)\n";
                std::cout << "  %M   minute (00..59)\n";
                std::cout << "  %S   second (00..59)\n";
                return 0;
            }
        }
        
        // If parsing a specific date
        Date target_date = current_date;
        if (!set_date.empty()) {
            target_date = parse_date(set_date);
        }
        
        // Compile-time calculations
        constexpr Date epoch{1970, 1, 1};
        constexpr Date y2k{2000, 1, 1};
        constexpr Date unix_end{2038, 1, 19};
        
        // Show some compile-time calculations
        if (format == "demo") {
            constexpr auto days_since_epoch = y2k - epoch;
            constexpr auto days_to_unix_end = unix_end - y2k;
            constexpr auto y2k_day_of_week = y2k.day_of_week();
            constexpr auto y2k_formatted = y2k.format();
            
            std::cout << "Compile-time calculations:\n";
            std::cout << "Y2K was " << days_since_epoch << " days after Unix epoch\n";
            std::cout << "Unix 32-bit end is " << days_to_unix_end << " days after Y2K\n";
            std::cout << "Y2K was day " << static_cast<int>(y2k_day_of_week) << " of the week\n";
            std::cout << "Y2K formatted: " << y2k_formatted.view() << "\n";
            return 0;
        }
        
        // Format current or specified date
        if (format == "%a %b %d %H:%M:%S %Z %Y") {
            // Default format - use runtime formatting
            if (utc) {
                std::cout << std::format("{:%a %b %d %H:%M:%S UTC %Y}", 
                                       std::chrono::system_clock::now()) << "\n";
            } else {
                auto local_time = std::chrono::current_zone()->to_local(
                    std::chrono::system_clock::now());
                std::cout << std::format("{:%a %b %d %H:%M:%S %Z %Y}", local_time) << "\n";
            }
        } else {
            // Custom format - use constexpr formatting
            DateTime dt{target_date, current_time};
            auto formatted = dt.format<64>(format);
            std::cout << formatted.view() << "\n";
        }
        
        return 0;
    }
};

// ═══════════════════════════════════════════════════════════════════════════
// MAIN ENTRY POINT
// ═══════════════════════════════════════════════════════════════════════════

int main(int argc, char* argv[]) {
    std::vector<std::string_view> args(argv + 1, argv + argc);
    return ConstexprDateUtility::execute(std::span(args));
}

// ═══════════════════════════════════════════════════════════════════════════
// COMPILE-TIME VERIFICATION TESTS
// ═══════════════════════════════════════════════════════════════════════════

// Static assertions to verify compile-time computations
namespace {
    using namespace constexpr_date;
    
    // Verify leap year calculations
    static_assert(!constexpr_date{1900, 1, 1}.is_leap_year());
    static_assert(constexpr_date{2000, 1, 1}.is_leap_year());
    static_assert(constexpr_date{2004, 1, 1}.is_leap_year());
    static_assert(!constexpr_date{2100, 1, 1}.is_leap_year());
    
    // Verify date arithmetic
    static_assert((constexpr_date{2000, 1, 1} + 31).day == 1);
    static_assert((constexpr_date{2000, 1, 1} + 31).month == 2);
    
    // Verify day of week calculations (Y2K was Saturday = 6)
    static_assert(constexpr_date{2000, 1, 1}.day_of_week() == 6);
    
    // Verify formatting
    constexpr auto formatted_date = constexpr_date{2023, 12, 25}.format();
    static_assert(formatted_date.view() == "2023-12-25");
    
    // Verify time formatting
    constexpr auto formatted_time = constexpr_time{14, 30, 45}.format();
    static_assert(formatted_time.view() == "14:30:45");
    
    std::cout << "All compile-time tests passed!\n";
}