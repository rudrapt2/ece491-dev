#ifdef STUDENT
    // YOUR CODE HERE
#else

#include "../syscall.h"
#include "../string.h"
#include "../shell.h"
#include "../error.h"

#define RTC_PATH "/dev/rtc"

#define NSEC_PER_SEC (1000000000UL)
#define NSEC_PER_MIN (60UL * NSEC_PER_SEC)
#define NSEC_PER_HR (60UL * NSEC_PER_MIN)
#define NSEC_PER_DAY (24UL * NSEC_PER_HR)

#define DAYS_SINCE_1970(t) (t / NSEC_PER_DAY)
#define HOUR(t) ((t%NSEC_PER_DAY) / NSEC_PER_HR)
#define MINUTE(t) ((t%NSEC_PER_HR) / NSEC_PER_MIN)
#define SEC(t) ((t%NSEC_PER_MIN) / NSEC_PER_SEC)

#define LEAP_YEAR(y) (((y)%4 == 0 && (y)%100 != 0) || ((y)%400 == 0))

const char* months[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};


void main (int argc, char** argv)
{
    unsigned long time;
    int dev_fd;
    int day, month, year;
    int month_days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31
    };

    
    dev_fd = _open(-1, RTC_PATH);

    if (dev_fd < 0) {
        printf("%s: could not open %s (%s)\n", 
            argv[0], RTC_PATH, error_desc(dev_fd));
        return;
    }

    _read(dev_fd, &time, sizeof(time));

    day = DAYS_SINCE_1970(time);
    year = 1970;
    month = 0;

    while (day >= (LEAP_YEAR(year) ? 366 : 365)) {
        day -= LEAP_YEAR(year) ? 366 : 365;
        year++;
    }

    if (LEAP_YEAR(year)) month_days[1] = 29;

    while (day >= month_days[month]) {
        day -= month_days[month];
        month++;
    }
    
    dprintf(STDOUT, "%02lu %s %02lu %02lu:%02lu:%02lu GMT\n", 
        day+1, months[month], year,
        HOUR(time), MINUTE(time), SEC(time));
}

#endif