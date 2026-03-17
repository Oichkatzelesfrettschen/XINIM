#include <sys/time.h>
#include <unistd.h>
#include <string.h>
#include <time.h>

static void write_num(int fd, unsigned long n) {
    char buf[20];
    int pos = 0;
    if (n == 0) buf[pos++] = '0';
    else while (n > 0) { buf[pos++] = (char)('0' + n % 10); n /= 10; }
    for (int i = 0; i < pos / 2; ++i) {
        char t = buf[i]; buf[i] = buf[pos-1-i]; buf[pos-1-i] = t;
    }
    write(fd, buf, pos);
}

static void write_padded(int fd, unsigned long n, int width) {
    char buf[20];
    int pos = 0;
    if (n == 0) buf[pos++] = '0';
    else while (n > 0) { buf[pos++] = (char)('0' + n % 10); n /= 10; }
    for (int i = pos; i < width; ++i) write(fd, "0", 1);
    for (int i = pos - 1; i >= 0; --i) write(fd, buf + i, 1);
}

int main(void) {
    struct timeval tv;
    if (gettimeofday(&tv, 0) != 0) {
        write(2, "date: gettimeofday failed\n", 26);
        return 1;
    }

    /* Convert epoch seconds to broken-down time (UTC) */
    unsigned long t = (unsigned long)tv.tv_sec;
    unsigned long days = t / 86400;
    unsigned long daytime = t % 86400;
    unsigned long hour = daytime / 3600;
    unsigned long min = (daytime % 3600) / 60;
    unsigned long sec = daytime % 60;

    /* Compute year, month, day from days since 1970-01-01 */
    unsigned long year = 1970;
    for (;;) {
        unsigned long yd = ((year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 366 : 365);
        if (days < yd) break;
        days -= yd;
        ++year;
    }
    int leap = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    static const unsigned short mdays[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    unsigned long month = 0;
    for (month = 0; month < 12; ++month) {
        unsigned long md = mdays[month] + (month == 1 && leap ? 1 : 0);
        if (days < md) break;
        days -= md;
    }

    write_padded(1, year, 4);
    write(1, "-", 1);
    write_padded(1, month + 1, 2);
    write(1, "-", 1);
    write_padded(1, days + 1, 2);
    write(1, " ", 1);
    write_padded(1, hour, 2);
    write(1, ":", 1);
    write_padded(1, min, 2);
    write(1, ":", 1);
    write_padded(1, sec, 2);
    write(1, " UTC\n", 5);
    (void)write_num;
    return 0;
}
