/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/* tail - print the end of a file */

/**
 * @file tail.cpp
 * @brief Print the end of a file using C++ iostreams.
 */

#include "stdio.hpp"
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <span>

#define TRUE 1
#define FALSE 0
#define BLANK ' '
#define TAB '\t'
#define NEWL '\n'

int lines, chars;

static int stoi(char *s);
static void tail(std::istream &in, int goal);
[[noreturn]] static void done(int n);

/**
 * @brief Entry point processing command-line arguments.
 */
int main(int argc, char *argv[]) {
    char *s;
    std::ifstream input;
    int count;
    argc--;
    argv++;
    lines = TRUE;
    chars = FALSE;
    count = -10;

    if (argc == 0) {
        tail(std::cin, count);
        done(0);
    }

    s = *argv;
    if (*s == '-' || *s == '+') {
        s++;
        if (*s >= '0' && *s <= '9') {
            count = stoi(*argv);
            s++;
            while (*s >= '0' && *s <= '9')
                s++;
        }
        if (*s == 'c') {
            chars = TRUE;
            lines = FALSE;
        } else if (*s != 'l' && *s != NULL) {
            std::cerr << "tail: unknown option " << *s << "\n";
            argc = 0;
        }
        argc--;
        argv++;
    }

    if (argc < 0) {
        std::cerr << "Usage: tail [+/-[number][lc]] [files]\n";
        done(1);
    }

    if (argc == 0)
        tail(std::cin, count);

    else {
        input.open(*argv);
        if (!input.is_open()) {
            std::cerr << "tail: can't open " << *argv << "\n";
            done(1);
        }
        tail(input, count);
        input.close();
    }

    done(0);
    return 0; // Unreachable
}

/* stoi - convert string to integer */

/**
 * @brief Convert a string to integer respecting sign.
 */
static int stoi(char *s) {
    int n, sign;

    while (*s == BLANK || *s == NEWL || *s == TAB)
        s++;

    sign = 1;
    if (*s == '+')
        s++;
    else if (*s == '-') {
        sign = -1;
        s++;
    }
    for (n = 0; *s >= '0' && *s <= '9'; s++)
        n = 10 * n + *s - '0';
    return (sign * n);
}

/* tail - print 'count' lines/chars */

#define BUF_SIZE 4098

/**
 * @brief Print the last part of the input stream.
 *
 * @param in    Input stream.
 * @param goal  Positive to skip first N, negative to print last N.
 */
static void tail(std::istream &in, int goal) {
    int c, count = 0;
    std::array<char, BUF_SIZE> cbuf{};
    std::span<char> buf{cbuf};
    char *start = buf.data();
    char *finish = buf.data();
    char *end = buf.data() + buf.size() - 1;
    auto incr = [&](char *&p) { p = (p >= end) ? buf.data() : p + 1; };

    if (goal > 0) { /* skip */
        if (lines)  /* lines */
            while ((c = in.get()) != EOF) {
                if (c == NEWL)
                    count++;
                if (count >= goal)
                    break;
            }
        else /* chars */
            while (in.get() != EOF) {
                count++;
                if (count >= goal)
                    break;
            }
        if (count >= goal)
            while ((c = in.get()) != EOF)
                std::cout.put(static_cast<char>(c));
    } else { /* tail */
        goal = -goal;
        while ((c = in.get()) != EOF) {
            *finish = static_cast<char>(c);
            incr(finish);

            if (start == finish)
                incr(start);
            if (!lines || c == NEWL)
                count++;

            if (count > goal) {
                count = goal;
                if (lines)
                    while (*start != NEWL)
                        incr(start);
                incr(start);
            }
        }

        while (start != finish) {
            std::cout.put(*start);
            incr(start);
        }
    }
}

/**
 * @brief Terminate the program, flushing stdio buffers.
 */
[[noreturn]] static void done(int n) {
    _cleanup(); /* flush stdio's internal buffers */
    std::exit(n);
}
