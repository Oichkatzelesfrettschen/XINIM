/* ar - archiver		Author: Michiel Huisjes */

/*
 * Usage: ar [adprtvx] archive [file] ...
 *	  v: verbose
 *	  x: extract
 *	  a: append
 *	  r: replace (append when not in archive)
 *	  d: delete
 *	  t: print contents of archive
 *	  p: print named files
 */

#include "signal.hpp"
#include "stat.hpp"
#include <array>
#include <cstring>
#include <string>
#include <string_view>

/* Modern constants and helpers */
constexpr int MAGIC_NUMBER = 0177545;

/* Determine if a number is odd */
[[nodiscard]] constexpr bool odd(int nr) { return nr & 0x01; }

/* Round a number up to the next even value */
[[nodiscard]] constexpr int even(int nr) { return odd(nr) ? nr + 1 : nr; }


union swabber {
    struct sw {
        short mem_1;
        short mem_2;
    } mem;
    long joined;
} swapped;

long swap();

// Changed from anonymous struct typedef to named struct
struct MEMBER {
    char m_name[14];
    short m_time_1;
    short m_time_2;
    char m_uid;
    char m_gid;
    short m_mode;
    short m_size_1;
    short m_size_2;
};

using BOOL = bool;
constexpr bool FALSE = false;
constexpr bool TRUE = true;

constexpr int READ = 0;
constexpr int APPEND = 2;
constexpr int CREATE = 1;

constexpr char *NIL_PTR = nullptr;
constexpr MEMBER *NIL_MEM = nullptr;
constexpr long *NIL_LONG = nullptr;

constexpr std::size_t IO_SIZE = 10 * 1024;
constexpr std::size_t BLOCK_SIZE = 1024;

inline void flush() { print(NIL_PTR); }

inline bool equal(const char *str1, const char *str2) {
    return !std::strncmp(str1, str2, 14);
}

BOOL verbose;
BOOL app_fl;
BOOL ex_fl;
BOOL show_fl;
BOOL pr_fl;
BOOL rep_fl;
BOOL del_fl;

int ar_fd;
long mem_time, mem_size;

std::array<char, IO_SIZE> io_buffer{};  // Buffer used for I/O operations
std::array<char, BLOCK_SIZE> terminal{}; // Temporary terminal buffer

char temp_arch[] = "/tmp/ar.XXXXX";

static void usage(void) { error(TRUE, "Usage: ar [adprtxv] archive [file] ...", NIL_PTR); }

static void error(BOOL quit, char *str1, char *str2) {
    write(2, str1, strlen(str1));
    if (str2 != NIL_PTR)
        write(2, str2, strlen(str2));
    write(2, "\n", 1);
    if (quit) {
        (void)unlink(temp_arch);
        exit(1);
    }
}

static char *basename(char *path) {
    register char *ptr = path;
    register char *last = NIL_PTR;

    while (*ptr != '\0') {
        if (*ptr == '/')
            last = ptr;
        ptr++;
    }
    if (last == NIL_PTR)
        return path;
    if (*(last + 1) == '\0') {
        *last = '\0';
        return basename(path);
    }
    return last + 1;
}

static int open_archive(char *name, int mode) {
    unsigned short magic = 0;
    int fd;

    if (mode == CREATE) {
        if ((fd = creat(name, 0644)) < 0)
            error(TRUE, "Cannot creat ", name);
        magic = MAGIC_NUMBER;
        mwrite(fd, &magic, sizeof(magic));
        return fd;
    }

    if ((fd = open(name, mode)) < 0) {
        if (mode == APPEND) {
            (void)close(open_archive(name, CREATE));
            error(FALSE, "ar: creating ", name);
            return open_archive(name, APPEND);
        }
        error(TRUE, "Cannot open ", name);
    }
    (void)lseek(fd, 0L, 0);
    (void)read(fd, &magic, sizeof(magic));
    if (magic != MAGIC_NUMBER)
        error(TRUE, name, " is not in ar format.");

    return fd;
}

static void catch (void) {
    (void)unlink(temp_arch);
    exit(2);
}

int main(int argc, char* argv[]) {
    // Parse command flags from argv[1]
    std::string_view flags = argv[1];
    int pow, pid;
    char* ptr = nullptr; // scratch pointer

    if (argc < 3)
        usage();

    for (char flag : flags) {
        switch (flag) {
        case 't':
            show_fl = TRUE;
            break;
        case 'v':
            verbose = TRUE;
            break;
        case 'x':
            ex_fl = TRUE;
            break;
        case 'a':
            app_fl = TRUE;
            break;
        case 'p':
            pr_fl = TRUE;
            break;
        case 'd':
            del_fl = TRUE;
            break;
        case 'r':
            rep_fl = TRUE;
            break;
        default:
            usage();
        }
    }

    if (app_fl + ex_fl + del_fl + rep_fl + show_fl + pr_fl != 1)
        usage();

    if (rep_fl || del_fl) {
        ptr = &temp_arch[8];
        pid = getpid();
        pow = 10000;

        while (pow != 0) {
            *ptr++ = (pid / pow) + '0';
            pid %= pow;
            pow /= 10;
        }
    }

    signal(SIGINT, catch);
    get(argc, argv);

    exit(0);
}

static MEMBER *get_member(void) {
    static MEMBER member;
    register int ret;

    if ((ret = read(ar_fd, &member, sizeof(MEMBER))) == 0)
        return NIL_MEM;
    if (ret != sizeof(MEMBER))
        error(TRUE, "Corrupted archive.", NIL_PTR);
    mem_time = swap(&(member.m_time_1));
    mem_size = swap(&(member.m_size_1));
    return &member;
}

static long swap(union swabber *sw_ptr) {
    swapped.mem.mem_1 = (sw_ptr->mem).mem_2;
    swapped.mem.mem_2 = (sw_ptr->mem).mem_1;

    return swapped.joined;
}

static void get(int argc, char *argv[]) {
    register MEMBER *member;
    int i = 0;
    int temp_fd, read_chars;

    ar_fd = open_archive(argv[2], (show_fl || pr_fl) ? READ : APPEND);
    if (rep_fl || del_fl)
        temp_fd = open_archive(temp_arch, CREATE);
    while ((member = get_member()) != NIL_MEM) {
        if (argc > 3) {
            for (i = 3; i < argc; i++) {
                if (equal(basename(argv[i]), member->m_name))
                    break;
            }
            if (i == argc || app_fl) {
                if (rep_fl || del_fl) {
                    mwrite(temp_fd, member, sizeof(MEMBER));
                    copy_member(member, ar_fd, temp_fd);
                } else {
                    if (app_fl && i != argc) {
                        print(argv[i]);
                        print(": already in archive.\n");
                        argv[i] = "";
                    }
                    (void)lseek(ar_fd, even(mem_size), 1);
                }
                continue;
            }
        }
        if (ex_fl || pr_fl)
            extract(member);
        else {
            if (rep_fl)
                add(argv[i], temp_fd, 'r');
            else if (show_fl) {
                if (verbose) {
                    print_mode(member->m_mode);
                    if (member->m_uid < 10)
                        print(" ");
                    litoa(0, (long)member->m_uid);
                    print("/");
                    litoa(0, (long)member->m_gid);
                    litoa(8, mem_size);
                    date(mem_time);
                }
                p_name(member->m_name);
                print("\n");
            } else if (del_fl)
                show('d', member->m_name);
            (void)lseek(ar_fd, even(mem_size), 1);
        }
        argv[i] = "";
    }

    if (argc > 3) {
        for (i = 3; i < argc; i++)
            if (argv[i][0] != '\0') {
                if (app_fl)
                    add(argv[i], ar_fd, 'a');
                else if (rep_fl)
                    add(argv[i], temp_fd, 'a');
                else {
                    print(argv[i]);
                    print(": not found\n");
                }
            }
    }

    flush();

    if (rep_fl || del_fl) {
        signal(SIGINT, SIG_IGN);
        (void)close(ar_fd);
        (void)close(temp_fd);
        ar_fd = open_archive(argv[2], CREATE);
        temp_fd = open_archive(temp_arch, APPEND);
        while ((read_chars = read(temp_fd, io_buffer.data(), IO_SIZE)) > 0)
            mwrite(ar_fd, io_buffer.data(), read_chars);
        (void)close(temp_fd);
        (void)unlink(temp_arch);
    }
    (void)close(ar_fd);
}

static void add(char *name, int fd, char mess) {
    static MEMBER member;
    register int read_chars;
    struct stat status;
    int src_fd;

    if (stat(name, &status) < 0) {
        error(FALSE, "Cannot find ", name);
        return;
    } else if ((src_fd = open(name, 0)) < 0) {
        error(FALSE, "Cannot open ", name);
        return;
    }

    strcpy(member.m_name, basename(name));
    member.m_uid = status.st_uid;
    member.m_gid = status.st_gid;
    member.m_mode = status.st_mode & 07777;
    (void)swap(&(status.st_mtime));
    member.m_time_1 = swapped.mem.mem_1;
    member.m_time_2 = swapped.mem.mem_2;
    (void)swap(&(status.st_size));
    member.m_size_1 = swapped.mem.mem_1;
    member.m_size_2 = swapped.mem.mem_2;
    mwrite(fd, &member, sizeof(MEMBER));
    while ((read_chars = read(src_fd, io_buffer.data(), IO_SIZE)) > 0)
        mwrite(fd, io_buffer.data(), read_chars);

    if (odd(status.st_size))
        mwrite(fd, io_buffer.data(), 1);

    if (verbose)
        show(mess, name);
    (void)close(src_fd);
}

static void extract(MEMBER *member) {
    int fd = 1;

    if (pr_fl == FALSE && (fd = creat(member->m_name, 0644)) < 0) {
        error(FALSE, "Cannot create ", member->m_name);
        return;
    }

    if (verbose && pr_fl == FALSE)
        show('x', member->m_name);

    copy_member(member, ar_fd, fd);

    if (fd != 1)
        (void)close(fd);
    (void)chmod(member->m_name, member->m_mode);
}

static void copy_member(MEMBER *member, int from, int to) {
    register int rest;
    BOOL is_odd = odd(mem_size) ? TRUE : FALSE;

    do {
        rest = mem_size > (long)IO_SIZE ? IO_SIZE : (int)mem_size;
        if (read(from, io_buffer.data(), rest) != rest)
            error(TRUE, "Read error on ", member->m_name);
        mwrite(to, io_buffer.data(), rest);
        mem_size -= (long)rest;
    } while (mem_size != 0L);

    if (is_odd) {
        (void)lseek(from, 1L, 1);
        if (rep_fl || del_fl)
            (void)lseek(to, 1L, 1);
    }
}

static void print(char *str) {
    static index = 0;

    if (str == NIL_PTR) {
        write(1, terminal.data(), index);
        index = 0;
        return;
    }

    while (*str != '\0') {
        terminal.data()[index++] = *str++;
        if (index == BLOCK_SIZE)
            flush();
    }
}

static void print_mode(int mode) {
    static char mode_buf[11];
    register int tmp = mode;
    int i;

    mode_buf[9] = ' ';
    for (i = 0; i < 3; i++) {
        mode_buf[i * 3] = (tmp & S_IREAD) ? 'r' : '-';
        mode_buf[i * 3 + 1] = (tmp & S_IWRITE) ? 'w' : '-';
        mode_buf[i * 3 + 2] = (tmp & S_IEXEC) ? 'x' : '-';
        tmp <<= 3;
    }
    if (mode & S_ISUID)
        mode_buf[2] = 's';
    if (mode & S_ISGID)
        mode_buf[5] = 's';
    print(mode_buf);
}

static void litoa(int pad, long number) {
    static char num_buf[11];
    register long digit;
    register long pow = 1000000000L;
    int digit_seen = FALSE;
    int i;

    for (i = 0; i < 10; i++) {
        digit = number / pow;
        if (digit == 0L && digit_seen == FALSE && i != 9)
            num_buf[i] = ' ';
        else {
            num_buf[i] = '0' + (char)digit;
            number -= digit * pow;
            digit_seen = TRUE;
        }
        pow /= 10L;
    }

    for (i = 0; num_buf[i] == ' ' && i + pad < 11; i++)
        ;
    print(&num_buf[i]);
}

static void mwrite(int fd, char *address, int bytes) {
    if (write(fd, address, bytes) != bytes)
        error(TRUE, "Write error.", NIL_PTR);
}

static void show(char c, char *name) {
    write(1, &c, 1);
    write(1, " - ", 3);
    write(1, name, strlen(name));
    write(1, "\n", 1);
}

static void p_name(char *mem_name) {
    register int i = 0;
    char name[15];

    for (i = 0; i < 14 && *mem_name; i++)
        name[i] = *mem_name++;

    name[i] = '\0';
    print(name);
}

#define MINUTE 60L
#define HOUR (60L * MINUTE)
#define DAY (24L * HOUR)
#define YEAR (365L * DAY)
#define LYEAR (366L * DAY)

int mo[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

char *moname[] = {" Jan ", " Feb ", " Mar ", " Apr ", " May ", " Jun ",
                  " Jul ", " Aug ", " Sep ", " Oct ", " Nov ", " Dec "};

/* Print the date.  This only works from 1970 to 2099. */
static void date(long t) {
    int i, year, day, month, hour, minute;
    long length, time(), original;

    year = 1970;
    original = t;
    while (t > 0) {
        length = (year % 4 == 0 ? LYEAR : YEAR);
        if (t < length)
            break;
        t -= length;
        year++;
    }

    /* Year has now been determined.  Now the rest. */
    day = (int)(t / DAY);
    t -= (long)day * DAY;
    hour = (int)(t / HOUR);
    t -= (long)hour * HOUR;
    minute = (int)(t / MINUTE);

    /* Determine the month and day of the month. */
    mo[1] = (year % 4 == 0 ? 29 : 28);
    month = 0;
    i = 0;
    while (day >= mo[i]) {
        month++;
        day -= mo[i];
        i++;
    }

    /* At this point, 'year', 'month', 'day', 'hour', 'minute'  ok */
    print(moname[month]);
    day++;
    if (day < 10)
        print(" ");
    litoa(0, (long)day);
    print(" ");
    if (time(NIL_LONG) - original >= YEAR / 2L)
        litoa(1, (long)year);
    else {
        if (hour < 10)
            print("0");
        litoa(0, (long)hour);
        print(":");
        if (minute < 10)
            print("0");
        litoa(0, (long)minute);
    }
    print(" ");
}
