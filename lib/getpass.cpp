#include "../include/sgtty.hpp"
#include "../include/stdio.hpp"
#include <unistd.h>

extern "C" int ioctl(int fd, int request, ...);

static char pwdbuf[9];

/**
 * @brief Read a password from the terminal without echoing input.
 *
 * @param prompt Prompt string displayed before reading.
 * @return Pointer to an internal static buffer containing the password.
 */
char *getpass(const char *prompt) {
    int i = 0;
    struct sgttyb tty;

    fputs(prompt, stdout);
    ioctl(0, TIOCGETP, &tty);
    tty.sg_flags = 06020;
    ioctl(0, TIOCSETP, &tty);
    i = read(0, pwdbuf, 9);
    while (pwdbuf[i - 1] != '\n')
        read(0, &pwdbuf[i - 1], 1);
    pwdbuf[i - 1] = '\0';
    tty.sg_flags = 06030;
    ioctl(0, TIOCSETP, &tty);
    fputs("\n", stdout);
    return pwdbuf;
}
