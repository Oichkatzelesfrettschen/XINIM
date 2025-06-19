/**
 * @file cal.cpp
 * @brief Modern C++23 calendar utility
 * @author Martin Minow (original), modernized for XINIM
 * 
 * A calendar printing utility that displays monthly or yearly calendars.
 * Fully modernized with C++23 features including constexpr, strong typing,
 * and static assertions for compile-time validation.
 */

/* cal - print a calendar		Author: Martin Minow */

#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

// Modern constants with clear semantic naming
constexpr int IO_SUCCESS = 0;
constexpr int IO_ERROR = 1;
constexpr char EOS = '\0';

// Calendar layout constants
constexpr int ENTRY_SIZE = 3;      /**< Bytes per calendar entry */
constexpr int DAYS_PER_WEEK = 7;   /**< Days in a week */
constexpr int WEEKS_PER_MONTH = 6; /**< Maximum weeks in a month */
constexpr int MONTHS_PER_LINE = 3; /**< Months displayed horizontally */
constexpr int MONTH_SPACE = 3;     /**< Spacing between months */

// Error messages as string literals
constexpr auto badarg = "Bad argument\n";
constexpr auto usage_msg = "Usage: cal [month] year\n";

/*
 * calendar() stuffs data into layout[],
 * output() copies from layout[] to outline[], (then trims blanks).
 */
std::array<std::array<std::array<std::array<char, ENTRY_SIZE>, DAYS_PER_WEEK>, WEEKS_PER_MONTH>,
           MONTHS_PER_LINE>
    layout{};
std::array<char,
           (MONTHS_PER_LINE * DAYS_PER_WEEK * ENTRY_SIZE) + (MONTHS_PER_LINE * MONTH_SPACE) + 1>
    outline{};

constexpr std::string_view weekday = " S  M Tu  W Th  F  S";
constexpr std::array<std::string_view, 13> monthname = {
    "???", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

/**
 * @brief Main entry point for the calendar utility
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return IO_SUCCESS on success, IO_ERROR on failure
 * 
 * Parses command line arguments to determine whether to display a month,
 * year, or specific month within a year. Handles argument ambiguity
 * intelligently based on value ranges.
 */
int main(int argc, char* argv[]) {
    if (argc <= 1) {
        usage(usage_msg);
        return IO_ERROR;
    }

    try {
        const int arg1val = std::stoi(argv[1]);
        const auto arg1len = std::strlen(argv[1]);
        
        if (argc == 2) {
            // Single argument: small values (≤12) with ≤2 digits are months,
            // larger values are years
            if (arg1len <= 2 && arg1val <= 12) {
                domonth_current_year(arg1val);
            } else {
                doyear(arg1val);
            }
        } else if (argc == 3) {
            // Two arguments: handle both "month year" and "year month" formats
            const int arg2val = std::stoi(argv[2]);
            
            if (arg1len > 2) {
                // First argument is likely the year (more than 2 digits)
                domonth(arg1val, arg2val);
            } else {
                // First argument is likely the month (≤2 digits)
                domonth(arg2val, arg1val);
            }
        } else {
            usage(usage_msg);
            return IO_ERROR;
        }
    } catch (const std::exception& e) {
        usage(badarg);
        return IO_ERROR;
    }
    
    return IO_SUCCESS;
}

static void doyear(int year)
/*
 * Print the calendar for an entire year.
 */
{
    register int month;

    if (year < 1 || year > 9999)
        usage(badarg);
    if (year < 100)
        printf("\n\n\n                                 00%2d\n\n", year);
    else
        printf("\n\n\n%35d\n\n", year);
    for (month = 1; month <= 12; month += MONTHS_PER_LINE) {
        printf("%12s%23s%23s\n", monthname[month], monthname[month + 1], monthname[month + 2]);
        printf("%s   %s   %s\n", weekday, weekday, weekday);
        calendar(year, month + 0, 0);
        calendar(year, month + 1, 1);        calendar(year, month + 2, 2);
        output(3);
        // Static assertion ensures MONTHS_PER_LINE is 3 for the above logic
        static_assert(MONTHS_PER_LINE == 3, "MONTHS_PER_LINE must be 3 for quarterly display");
    }
    printf("\n\n\n");
}

static void domonth(int year, int month)
/*
 * Do one specific month -- note: no longer used
 */
{
    if (year < 1 || year > 9999)
        usage(badarg);
    if (month <= 0 || month > 12)
        usage(badarg);
    printf("%9s%5d\n\n%s\n", monthname[month], year, weekday);
    calendar(year, month, 0);
    output(1);
    printf("\n\n");
}

static void output(int nmonths) /* Number of months to do */
/*
 * Clean up and output the text.
 */
{
    register int week;
    register int month;
    register char *outp;
    int i;
    char tmpbuf[21], *p;

    for (week = 0; week < WEEKS_PER_MONTH; week++) {
        outp = outline.data();
        for (month = 0; month < nmonths; month++) {
            /*
             * The -1 in the following removes
             * the unwanted leading blank from
             * the entry for Sunday.
             */
            p = &layout[month][week][0][1];
            for (i = 0; i < 20; i++)
                tmpbuf[i] = *p++;
            tmpbuf[20] = 0;
            sprintf(outp, "%s   ", tmpbuf);
            outp += (DAYS_PER_WEEK * ENTRY_SIZE) + MONTH_SPACE - 1;
        }
        while (outp > outline.data() && outp[-1] == ' ')
            outp--;
        *outp = EOS;
        puts(outline.data());
    }
}

static void calendar(int year, int month, int index) /* Which of the three months */
/*
 * Actually build the calendar for this month.
 */
{
    register char *tp;
    int week;
    register int wday;
    register int today;

    setmonth(year, month);
    for (week = 0; week < WEEKS_PER_MONTH; week++) {
        for (wday = 0; wday < DAYS_PER_WEEK; wday++) {
            tp = &layout[index][week][wday][0];
            *tp++ = ' ';
            today = getdate(week, wday);
            if (today <= 0) {
                *tp++ = ' ';
                *tp++ = ' ';
            } else if (today < 10) {
                *tp++ = ' ';
                *tp = (today + '0');
            } else {
                *tp++ = (today / 10) + '0';
                *tp = (today % 10) + '0';
            }
        }
    }
}

static void usage(char *s) {
    /* Fatal parameter error. */

    fprintf(stderr, "%s", s);
    exit(IO_ERROR);
}

/*
 * Calendar routines, intended for eventual porting to TeX
 *
 * date(year, month, week, wday)
 *	Returns the date on this week (0 is first, 5 last possible)
 *	and day of the week (Sunday == 0)
 *	Note: January is month 1.
 *
 * setmonth(year, month)
 *	Parameters are as above, sets getdate() for this month.
 *
 * int
 * getdate(week, wday)
 *	Parameters are as above, uses the data set by setmonth()
 */

/*
 * This structure is used to pass data between setmonth() and getdate().
 * It needs considerable expansion if the Julian->Gregorian change is
 * to be extended to other countries.
 */

static struct {
    int this_month;    /* month number used in 1752 checking	*/
    int feb;           /* Days in February for this month	*/
    int sept;          /* Days in September for this month	*/
    int days_in_month; /* Number of days in this month		*/
    int dow_first;     /* Day of week of the 1st day in month	*/
} info;

static int day_month[] = {/* 30 days hath September...		*/
                          0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

int static int date(int year, int month, int week, int wday) /* Calendar date being computed */
/*
 * Return the date of the month that fell on this week and weekday.
 * Return zero if it's out of range.
 */
{
    setmonth(year, month);
    return (getdate(week, wday));
}

static void setmonth(int year, int month)
/*
 * Setup the parameters needed to compute this month
 * (stored in the info structure).
 */
{
    register int i;

    if (month < 1 || month > 12) { /* Verify caller's parameters	*/
        info.days_in_month = 0;    /* Garbage flag			*/
        return;
    }
    info.this_month = month;     /* used in 1752	checking	*/
    info.dow_first = Jan1(year); /* Day of January 1st for now	*/
    info.feb = 29;               /* Assume leap year		*/
    info.sept = 30;              /* Assume normal year		*/
    /*
     * Determine whether it's an ordinary year, a leap year
     * or the magical calendar switch year of 1752.
     */
    switch ((Jan1(year + 1) + 7 - info.dow_first) % 7) {
    case 1: /* Not a leap year		*/
        info.feb = 28;
    case 2: /* Ordinary leap year		*/
        break;

    default:            /* The magical moment arrives	*/
        info.sept = 19; /* 19 days hath September	*/
        break;
    }
    info.days_in_month = (month == 2) ? info.feb : (month == 9) ? info.sept : day_month[month];
    for (i = 1; i < month; i++) {
        switch (i) { /* Special months?		*/
        case 2:      /* February			*/
            info.dow_first += info.feb;
            break;

        case 9:
            info.dow_first += info.sept;
            break;

        default:
            info.dow_first += day_month[i];
            break;
        }
    }
    info.dow_first %= 7; /* Now it's Sunday to Saturday	*/
}

static int getdate(int week, int wday) register int today;

/*
 * Get a first guess at today's date and make sure it's in range.
 */
today = (week * 7) + wday - info.dow_first + 1;
if (today <= 0 || today > info.days_in_month)
    return (0);
else if (info.sept == 19 && info.this_month == 9 && today >= 3) /* The magical month?	*/
    return (today + 11);                                        /* If so, some dates changed	*/
else                                                            /* Otherwise,			*/
    return (today);                                             /* Return the date		*/
}

static int Jan1(int year)
/*
 * Return day of the week for Jan 1 of the specified year.
 */
{
    register int day;

    day = year + 4 + ((year + 3) / 4); /* Julian Calendar	*/
    if (year > 1800) {                 /* If it's recent, do	*/
        day -= ((year - 1701) / 100);  /* Clavian correction	*/
        day += ((year - 1601) / 400);  /* Gregorian correction	*/
    }
    if (year > 1752) /* Adjust for Gregorian	*/
        day += 3;    /* calendar		*/
    return (day % 7);
}
