#include "fat_utils.hpp"
#include <cassert>
#include <chrono>
int main() {
    using namespace std::chrono;
    uint16_t date = 0x56EE; // 2023-07-14
    uint16_t time = 0x6DAF; // 13:45:30
    auto tp = fat_datetime_to_timepoint(date, time);
    auto expected = sys_days{year{2023} / 7 / 14} + hours{13} + minutes{45} + seconds{30};
    assert(tp == expected);
    return 0;
}
