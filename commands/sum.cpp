/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* sum - checksum a file		Author: Martin C. Atkins */

/*
 *	This program was written by:
 *		Martin C. Atkins,
 *		University of York,
 *		Heslington,
 *		York. Y01 5DD
 *		England
 *	and is released into the public domain, on the condition
 *	that this comment is always included without alteration.
 */

#include <cstdlib>  // exit
#include <cstring>  // strcmp
#include <fcntl.h>  // open
#include <unistd.h> // read, close

// Size of the buffer used when reading files
constexpr int kBufSize = 512;

static void error(const char *s, const char *f);
static void sum(int fd, const char *fname);
static void putd(int number, int fw, int zeros);

// Return code from main
int rc = 0;

// Default argument when no file is specified
char *defargv[] = {"-", nullptr};

// Program entry point
int main(int argc, char *argv[]) {
    int fd;

    if (*++argv == 0)
        argv = defargv;
    for (; *argv; ++argv) {
        if (argv[0][0] == '-' && argv[0][1] == '\0') {
            fd = 0; // read from stdin
        } else {
            fd = open(*argv, O_RDONLY);
        }

        if (fd == -1) {
            error("can't open ", *argv);
            rc = 1;
            continue;
        }
        sum(fd, (argc > 2) ? *argv : nullptr);
        if (fd != 0) {
            close(fd);
        }
    }
    exit(rc);
}

static void error(const char *s, const char *f) {
    std_err("sum: ");
    std_err(s);

    if (f) {
        std_err(f);
    }
    std_err("\n");
}

static void sum(int fd, const char *fname) {
    char buf[kBufSize];
    int i;
    int n;
    int size = 0;
    unsigned crc = 0;
    unsigned tmp;

    while ((n = read(fd, buf, kBufSize)) > 0) {
        for (i = 0; i < n; i++) {
            crc = (crc >> 1) + ((crc & 1) ? 0x8000 : 0);
            tmp = buf[i] & 0377;
            crc += tmp;
            crc &= 0xffff;
            size++;
        }
    }

    if (n < 0) {
        if (fname) {
            error("read error on ", fname);
        } else {
            error("read error", nullptr);
        }
        rc = 1;
        return;
    }
    putd(crc, 5, 1);
    putd((size + kBufSize - 1) / kBufSize, 6, 0);
    if (fname) {
        prints(" %s", fname);
    }
    prints("\n");
}

static void putd(int number, int fw, int zeros) {
    /* Put a decimal number, in a field width, to stdout. */

    char buf[10];
    int n;
    unsigned num;

    num = static_cast<unsigned>(number);
    for (n = 0; n < fw; ++n) {
        if (num || n == 0) {
            buf[fw - n - 1] = '0' + num % 10;
            num /= 10;
        } else {
            buf[fw - n - 1] = zeros ? '0' : ' ';
        }
    }
    buf[fw] = 0;
    prints("%s", buf);
}
