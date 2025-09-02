#pragma once
#include <chrono>
#include <cstdint>
struct DirectoryEntry;
inline std::chrono::sys_seconds fat_datetime_to_timepoint(uint16_t date, uint16_t time) {
    using namespace std::chrono;
    int y = static_cast<int>((date >> 9) & 0x7F) + 1980;
    unsigned m = static_cast<unsigned>((date >> 5) & 0x0F);
    unsigned d_ = static_cast<unsigned>(date & 0x1F);
    unsigned hour = static_cast<unsigned>((time >> 11) & 0x1F);
    unsigned minute = static_cast<unsigned>((time >> 5) & 0x3F);
    unsigned second = static_cast<unsigned>((time & 0x1F) * 2);
    auto day_point = sys_days{year{y} / month{m} / day{d_}};
    return day_point + hours{hour} + minutes{minute} + seconds{second};
}
