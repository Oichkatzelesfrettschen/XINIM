/**
 * @file xinim_terminal.c
 * @brief XINIM terminal integration for mksh
 * 
 * Provides terminal I/O, line editing, and terminal control for mksh shell.
 */

#include <termios.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

/* Terminal state */
static struct termios saved_termios;
static int terminal_saved = 0;

/**
 * @brief Get terminal attributes
 */
int xinim_tcgetattr(int fd, struct termios *termios_p) {
    /* Call XINIM ioctl for terminal attributes */
    return ioctl(fd, TCGETS, termios_p);
}

/**
 * @brief Set terminal attributes
 */
int xinim_tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    int cmd;
    
    switch (optional_actions) {
    case TCSANOW:
        cmd = TCSETS;
        break;
    case TCSADRAIN:
        cmd = TCSETSW;
        break;
    case TCSAFLUSH:
        cmd = TCSETSF;
        break;
    default:
        errno = EINVAL;
        return -1;
    }
    
    return ioctl(fd, cmd, termios_p);
}

/**
 * @brief Enter raw mode for shell input
 */
int xinim_terminal_raw_mode(int fd) {
    struct termios raw;
    
    if (!terminal_saved) {
        if (xinim_tcgetattr(fd, &saved_termios) < 0)
            return -1;
        terminal_saved = 1;
    }
    
    raw = saved_termios;
    
    /* Input modes - disable canonical mode, echo, signals */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    
    /* Output modes - disable post-processing */
    raw.c_oflag &= ~(OPOST);
    
    /* Control modes - set 8-bit chars */
    raw.c_cflag |= (CS8);
    
    /* Local modes - disable echo, canonical mode, extended functions, signals */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    
    /* Control characters - minimum read is 1 byte */
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    
    return xinim_tcsetattr(fd, TCSAFLUSH, &raw);
}

/**
 * @brief Restore terminal to cooked mode
 */
int xinim_terminal_cooked_mode(int fd) {
    if (!terminal_saved)
        return 0;
    
    return xinim_tcsetattr(fd, TCSAFLUSH, &saved_termios);
}

/**
 * @brief Get terminal window size
 */
int xinim_get_window_size(int fd, int *rows, int *cols) {
    struct winsize ws;
    
    if (ioctl(fd, TIOCGWINSZ, &ws) < 0) {
        *rows = 24;  /* Default */
        *cols = 80;
        return -1;
    }
    
    *rows = ws.ws_row;
    *cols = ws.ws_col;
    return 0;
}

/**
 * @brief Check if file descriptor is a terminal
 */
int xinim_isatty(int fd) {
    struct termios t;
    return xinim_tcgetattr(fd, &t) == 0;
}

/**
 * @brief Get controlling terminal name
 */
char *xinim_ttyname(int fd) {
    static char tty_name[32];
    
    if (!xinim_isatty(fd)) {
        errno = ENOTTY;
        return NULL;
    }
    
    /* XINIM terminal naming convention */
    snprintf(tty_name, sizeof(tty_name), "/dev/tty%d", fd);
    return tty_name;
}

/**
 * @brief Read a single character from terminal
 */
int xinim_terminal_getchar(int fd) {
    unsigned char c;
    ssize_t n;
    
    n = read(fd, &c, 1);
    if (n <= 0)
        return -1;
    
    return c;
}

/**
 * @brief Write string to terminal
 */
ssize_t xinim_terminal_write(int fd, const char *str) {
    return write(fd, str, strlen(str));
}

/**
 * @brief Clear terminal screen
 */
void xinim_terminal_clear(int fd) {
    xinim_terminal_write(fd, "\x1b[2J");     /* Clear screen */
    xinim_terminal_write(fd, "\x1b[H");      /* Move cursor to home */
}

/**
 * @brief Move cursor to position
 */
void xinim_terminal_move_cursor(int fd, int row, int col) {
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", row + 1, col + 1);
    xinim_terminal_write(fd, buf);
}

/**
 * @brief Set terminal title
 */
void xinim_terminal_set_title(int fd, const char *title) {
    char buf[256];
    snprintf(buf, sizeof(buf), "\x1b]0;%s\x07", title);
    xinim_terminal_write(fd, buf);
}

/* POSIX compatibility wrappers */

int tcgetattr(int fd, struct termios *termios_p) {
    return xinim_tcgetattr(fd, termios_p);
}

int tcsetattr(int fd, int optional_actions, const struct termios *termios_p) {
    return xinim_tcsetattr(fd, optional_actions, termios_p);
}

int isatty(int fd) {
    return xinim_isatty(fd);
}

char *ttyname(int fd) {
    return xinim_ttyname(fd);
}
