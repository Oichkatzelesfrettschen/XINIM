/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

#pragma once

// Data structures for IOCTL terminal settings.

struct sgttyb {
    char sg_ispeed; /* input speed (not used) */
    char sg_ospeed; /* output speed (not used) */
    char sg_erase;  /* erase character */
    char sg_kill;   /* kill character */
    int sg_flags;   /* mode flags */
};

struct tchars {
    char t_intrc;  /* SIGINT char */
    char t_quitc;  /* SIGQUIT char */
    char t_startc; /* start output (initially CTRL-Q) */
    char t_stopc;  /* stop output       (initially CTRL-S) */
    char t_eofc;   /* EOF (initially CTRL-D) */
    char t_brkc;   /* input delimiter (like nl) */
};

/* Fields in sg_flags. */
enum class SgFlags : int {
    XTABS = 0006000,  /**< do tab expansion */
    RAW = 0000040,    /**< enable raw mode */
    CRMOD = 0000020,  /**< map LF to CR+LF */
    ECHO = 0000010,   /**< echo input */
    CBREAK = 0000002, /**< enable cbreak mode */
    COOKED = 0000000  /**< neither CBREAK nor RAW */
};

constexpr int TIOCGETP = ('t' << 8) | 8;  /**< get parameters */
constexpr int TIOCSETP = ('t' << 8) | 9;  /**< set parameters */
constexpr int TIOCGETC = ('t' << 8) | 18; /**< get control chars */
constexpr int TIOCSETC = ('t' << 8) | 17; /**< set control chars */
