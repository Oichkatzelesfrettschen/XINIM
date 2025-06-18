/*<<< WORK-IN-PROGRESS MODERNIZATION HEADER
  This repository is a work in progress to reproduce the
  original MINIX simplicity on modern 32-bit and 64-bit
  ARM and x86/x86_64 hardware using C++23.
>>>*/

/*
 * Pr - print files
 *
 * Author: Michiel Huisjes.
 *
 * Usage: pr [+page] [-columns] [-h header] [-w with] [-l length] [-nt] [files]
 *        -t : Do not print the 5 line header and trailer at the page.
 *	  -n : Turn on line numbering.
 *        +page    : Start printing at page n.
 *        -columns : Print files in n-colums.
 *        -l length: Take the length of the page to be n instead of 66
 *        -h header: Take next argument as page header.
 *        -w with  : Take the width of the page to be n instead of default 72
 */

#include "stdio.hpp"
#include <memory>

char *colbuf;

/// Default page length in lines.
constexpr int kDefaultLength = 66;

/// Default page width in characters.
constexpr int kDefaultWidth = 72;

/// Alias for boolean values used by legacy code.
using Bool = bool;

#define NIL_PTR ((char *)0)

char *header;
Bool no_header;
Bool number;
short columns;
short cwidth;
short start_page = 1;
short width = kDefaultWidth;
short length = kDefaultLength;

char output[1024];
FILE *fopen();

/**
 * @brief Program entry point.
 *
 * Parses command line arguments and prints the specified files with
 * optional pagination and formatting.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit status code.
 */
int main(int argc, char *argv[]) {
    std::unique_ptr<FILE, decltype(&fclose)> file(nullptr, fclose);
    char *ptr;
    int index = 1;

    while (argc > index) {
        ptr = argv[index++];
        if (*ptr == '+') {
            start_page = atoi(++ptr);
            continue;
        }
        if (*ptr != '-') {
            index--;
            break;
        }
        if (*++ptr >= '0' && *ptr <= '9') {
            columns = atoi(ptr);
            continue;
        }
        while (*ptr)
            switch (*ptr++) {
            case 't':
                no_header = true;
                break;
            case 'n':
                number = true;
                break;
            case 'h':
                header = argv[index++];
                break;
            case 'w':
                width = atoi(ptr);
                *ptr = '\0';
                break;
            case 'l':
                length = atoi(ptr);
                *ptr = '\0';
                break;
            default:
                fprintf(stderr,
                        "Usage: %s [+page] [-columns] [-h header] [-w<with>] [-l<length>] [-nt] "
                        "[files]\n",
                        argv[0]);
                exit(1);
            }
    }

    if (!no_header)
        length -= 10;

    if (length <= 0)
        length = kDefaultLength;

    if (columns) {
        cwidth = width / columns + 1;
        if (columns > width) {
            fprintf(stderr, "Too many columns for pagewidth.\n");
            exit(1);
        }
        if ((colbuf = (char *)sbrk(cwidth * columns * length)) < 0) {
            fprintf(stderr,
                    "No memory available for a page of %d x %d. Use chmem to allocate more.\n",
                    length, cwidth);
            exit(1);
        }
    }

    setbuf(stdout, output);

    if (argc == index) {
        header = "";
        print(stdin);
    }

    while (index != argc) {
        file.reset(fopen(argv[index], "r"));
        if (!file) {
            fprintf(stderr, "Cannot open %s\n", argv[index++]);
            continue;
        }
        header = argv[index];
        if (columns)
            format(file);
        else
            print(file);
        file.reset();
        index++;
    }

    if (columns) {
        if (brk(colbuf) < 0) {
            fprintf(stderr, "Can't reset memory!\n");
            exit(1);
        }
    }

    (void)fflush(stdout);
    exit(0);
}

/**
 * @brief Skip a number of lines in a file.
 *
 * @param lines Number of lines to skip.
 * @param filep File pointer to read from.
 * @return Last character read or EOF.
 */
static int skip_page(int lines, FILE *filep) {
    int c{};
    do {
        while ((c = getc(filep)) != '\n' && c != EOF)
            ;
        --lines;
    } while (lines && c != EOF);
    return c;
}

/**
 * @brief Format and print a file using the configured column layout.
 *
 * @param filep File to read from.
 */
static void format(FILE *filep) {
    int c = '\0';
    int index, lines, i;
    int page_number = 0;
    int maxcol = columns;

    do {
        /* Check printing of page */
        page_number++;

        if (page_number < start_page && c != EOF) {
            c = skip_page(columns * length, filep);
            continue;
        }

        if (c == EOF)
            return;

        lines = columns * length;
        index = 0;
        do {
            for (i = 0; i < cwidth - 1; i++) {
                if ((c = getc(filep)) == '\n' || c == EOF)
                    break;
                colbuf[index++] = c;
            }
            /* First char is EOF */
            if (i == 0 && lines == columns * length && c == EOF)
                return;
            while (c != '\n' && c != EOF)
                c = getc(filep);
            colbuf[index++] = '\0';
            while (++i < cwidth)
                index++;
            lines--;
            if (c == EOF) {
                maxcol = columns - lines / length;
                while (lines--)
                    for (i = 0; i < cwidth; i++)
                        colbuf[index++] = '\0';
            }
        } while (c != EOF && lines);
        print_page(colbuf, page_number, maxcol);
    } while (c != EOF);
}

/**
 * @brief Print a single formatted page.
 *
 * @param buf Buffer containing formatted lines.
 * @param pagenr Current page number.
 * @param maxcol Number of columns in the page.
 */
static void print_page(char buf[], int pagenr, int maxcol) {
    int linenr = (pagenr - 1) * length + 1;
    int pad, i, j, start;

    if (!no_header)
        out_header(pagenr);
    for (i = 0; i < length; i++) {
        if (number)
            printf("%d\t", linenr++);
        for (j = 0; j < maxcol; j++) {
            start = (i + j * length) * cwidth;
            for (pad = 0; pad < cwidth - 1 && buf[start + pad]; pad++)
                putchar(buf[start + pad]);
            if (j < maxcol - 1) /* Do not pad last column */
                while (pad++ < cwidth - 1)
                    putchar(' ');
        }
        putchar('\n');
    }
    if (!no_header)
        printf("\n\n\n\n\n");
}

/**
 * @brief Print a file without column formatting.
 *
 * @param filep File to read from.
 */
static void print(FILE *filep) {
    int c = '\0';
    int page_number = 0;
    int linenr = 1;
    int lines;

    do {
        /* Check printing of page */
        page_number++;
        if (page_number < start_page && c != EOF) {
            c = skip_page(length, filep);
            continue;
        }

        if (c == EOF)
            return;

        if (page_number == start_page)
            c = getc(filep);

        /* Print the page */
        lines = length;
        if (!no_header)
            out_header(page_number);
        while (lines && c != EOF) {
            if (number)
                printf("%d\t", linenr++);
            do {
                putchar(c);
            } while ((c = getc(filep)) != '\n' && c != EOF);
            putchar('\n');
            lines--;
            c = getc(filep);
        }
        if (lines == length)
            return;
        if (!no_header)
            printf("\n\n\n\n\n");
        /* End of file */
    } while (c != EOF);

    /* Fill last page */
    if (page_number >= start_page) {
        while (lines--)
            putchar('\n');
    }
}

/**
 * @brief Print the standard page header.
 *
 * @param page Page number to display.
 */
static void out_header(int page) {
    extern long time();
    long t;

    (void)time(&t);
    print_time(t);
    printf("  %s   Page %d\n\n\n", header, page);
}

/// Number of seconds per minute.
constexpr long kMinute = 60L;

/// Number of seconds per hour.
constexpr long kHour = 60L * kMinute;

/// Number of seconds in a day.
constexpr long kDay = 24L * kHour;

/// Number of seconds in a common year.
constexpr long kYear = 365L * kDay;

/// Number of seconds in a leap year.
constexpr long kLeapYear = 366L * kDay;

int mo[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

char *moname[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/**
 * @brief Print the current date in a human readable form.
 *
 * This implementation only works for years 1970-2099.
 *
 * @param t Unix timestamp in seconds.
 */
static void print_time(long t) {
    int i, year, day, month, hour, minute;
    long length, time(), original;

    year = 1970;
    original = t;
    while (t > 0) {
        length = (year % 4 == 0 ? kLeapYear : kYear);
        if (t < length)
            break;
        t -= length;
        year++;
    }

    /* Year has now been determined.  Now the rest. */
    day = static_cast<int>(t / kDay);
    t -= static_cast<long>(day) * kDay;
    hour = static_cast<int>(t / kHour);
    t -= static_cast<long>(hour) * kHour;
    minute = static_cast<int>(t / kMinute);

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
    printf("\n\n%s %d %0d:%0d %d", moname[month], day + 1, hour + 1, minute, year);
}
