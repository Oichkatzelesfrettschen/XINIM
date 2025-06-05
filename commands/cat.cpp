/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++17.
>>>*/

/* cat - concatenates files  		Author: Andy Tanenbaum */

#include <cerrno>
#include <array>
#include <string_view>
#include "blocksiz.hpp"
#include "stat.hpp"

constexpr std::size_t BUF_SIZE = 512; /* size of the output buffer */
int unbuffered;                       /* non-zero for unbuffered operation */
std::array<char, BUF_SIZE> buffer{};  /* output buffer */
char *next = buffer.data();           /* next free byte in buffer */

/* Function prototypes */
static void copyfile(int fd1, int fd2);
static void flush(void);
static void quit(void);

int main(int argc, char* argv[]) {
    // Parse options and copy each file to standard output.

    int k = 1;                        // Start of file arguments
    std::string_view p{};             // Potential '-u' flag

    if (argc >= 2) {
        p = argv[1];
        if (p == "-u") {
            unbuffered = 1;
            k = 2;
        }
    }

    if (k >= argc) {
        // No files specified; default to standard input
        copyfile(0, 1);
        flush();
        return 0;
    }

    for (i = k; i < argc; i++) {
        std::string_view file = argv[i];
        int fd1;
        if (file == "-") {
            fd1 = 0; // Use standard input
        } else {
            fd1 = open(argv[i], 0);
            if (fd1 < 0) {
                std_err("cat: cannot open ");
                std_err(argv[i]);
                std_err("\n");
                continue;
            }
        }
        copyfile(fd1, 1);
        if (fd1 != 0)
            close(fd1);
    }
    flush();
    return 0;
}

static void copyfile(int fd1, int fd2) {
    /*
     * Read data from fd1 and write it to fd2.  When running in buffered
     * mode the output is collected in 'buffer' and only written when the
     * buffer is full.  In unbuffered mode each read result is written
     * immediately.
     */
    int n, j, m;
    std::array<char, BLOCK_SIZE> buf{};

    while (1) {
        n = read(fd1, buf.data(), BLOCK_SIZE);
        if (n < 0)
            quit();
        if (n == 0)
            return;
        if (unbuffered) {
            m = write(fd2, buf.data(), n);
            if (m != n)
                quit();
        } else {
            for (j = 0; j < n; j++) {
                *next++ = buf[j];
                if (next == buffer.data() + BUF_SIZE) {
                    m = write(fd2, buffer.data(), BUF_SIZE);
                    if (m != BUF_SIZE)
                        quit();
                    next = buffer.data();
                }
            }
        }
    }
}

static void flush(void) {
    /* Write any buffered output to standard output. */
    if (next != buffer.data())
        if (write(1, buffer.data(), next - buffer.data()) <= 0)
            quit();
}

static void quit(void) {
    /* Terminate the program after printing the error stored in errno. */
    perror("cat");
    exit(1);
}
