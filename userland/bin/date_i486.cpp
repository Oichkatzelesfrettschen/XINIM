// date -- print date and time (POSIX.1)
// Cleanroom C++23 implementation per IEEE Std 1003.1.
// Outputs: YYYY-MM-DD HH:MM:SS UTC

#include <sys/time.h>
#include <unistd.h>

namespace {

void write_all(int fd, const char* buf, int len) {
    while (len > 0) {
        auto w = write(fd, buf, static_cast<unsigned>(len));
        if (w <= 0) return;
        buf += w;
        len -= static_cast<int>(w);
    }
}

void write_str(int fd, const char* s) {
    int n = 0;
    while (s[n] != '\0') ++n;
    write_all(fd, s, n);
}

void write_padded(int fd, unsigned long v, int width) {
    char buf[20];
    int pos = 0;
    if (v == 0) { buf[pos++] = '0'; }
    else {
        while (v > 0) {
            buf[pos++] = static_cast<char>('0' + v % 10);
            v /= 10;
        }
    }
    // Leading zeros
    for (int i = pos; i < width; ++i)
        write_all(fd, "0", 1);
    // Digits in reverse
    for (int i = pos - 1; i >= 0; --i)
        write_all(fd, &buf[i], 1);
}

bool is_leap(unsigned long y) {
    return (y % 4 == 0 && (y % 100 != 0 || y % 400 == 0));
}

} // namespace

int main() {
    struct timeval tv{};
    if (gettimeofday(&tv, nullptr) != 0) {
        write_str(2, "date: gettimeofday failed\n");
        return 1;
    }

    auto t = static_cast<unsigned long>(tv.tv_sec);
    unsigned long days = t / 86400;
    unsigned long daytime = t % 86400;
    unsigned long hour = daytime / 3600;
    unsigned long min = (daytime % 3600) / 60;
    unsigned long sec = daytime % 60;

    // Compute year from days since 1970-01-01
    unsigned long year = 1970;
    for (;;) {
        unsigned long yd = is_leap(year) ? 366 : 365;
        if (days < yd) break;
        days -= yd;
        ++year;
    }

    // Compute month and day
    constexpr unsigned short mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    unsigned long month = 0;
    for (month = 0; month < 12; ++month) {
        unsigned long md = mdays[month] + (month == 1 && is_leap(year) ? 1 : 0);
        if (days < md) break;
        days -= md;
    }

    write_padded(1, year, 4);
    write_str(1, "-");
    write_padded(1, month + 1, 2);
    write_str(1, "-");
    write_padded(1, days + 1, 2);
    write_str(1, " ");
    write_padded(1, hour, 2);
    write_str(1, ":");
    write_padded(1, min, 2);
    write_str(1, ":");
    write_padded(1, sec, 2);
    write_str(1, " UTC\n");
    return 0;
}
