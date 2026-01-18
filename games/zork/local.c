/* local.c -- dungeon functions which need local definition */

#include "funcs.h"
#include "glue.h"

/* This function should return TRUE_ if it's OK for people to play the
 * game, FALSE_ otherwise.  If you have a working <time.h> library,
 * you can define NONBUSINESS to disallow play Monday to Friday, 9-5
 * (this is only checked at the start of the game, though).  For more
 * complex control you will have to write your own version of this
 * function.
 */

logical protected()
{
#ifndef NONBUSINESS
    return TRUE_;

#else

    // read goldfish rtc
    int result;
    uint64_t utc_time, seconds_in_week, seconds_in_day;
    if ((result = time(&utc_time)) < 0) {
        printf("Error getting time, letting user play\n");
        return TRUE_;
    }
    utc_time /= NANOSECONDS_PER_SECOND;

    seconds_in_week = utc_time % SECONDS_PER_WEEK;
    seconds_in_day = utc_time % SECONDS_PER_DAY;
    
    // 2:00 - 3:20 Tu/Th
    if ((seconds_in_week/SECONDS_PER_DAY == 0 || seconds_in_week/SECONDS_PER_DAY == 4) &&
        (seconds_in_day >= (14 * SECONDS_PER_HOUR) && seconds_in_day < ((15 * SECONDS_PER_HOUR) + (20 * SECONDS_PER_MINUTE)))) {
        return FALSE_;
    }

    return TRUE_;

#endif

}

#ifdef ALLOW_GDT

/* This function should return TRUE_ if the user is allowed to invoke the
 * game debugging tool by typing "gdt".  This isn't very useful without
 * the source code, and it's mainly for people trying to debug the game.
 * You can define WIZARDID to specify a user id on a UNIX system.  On a
 * non AMOS, non unix system this function will have to be changed if
 * you want to use gdt.
 */

#ifndef WIZARDID
#define WIZARDID (0)
#endif

logical wizard()
{
    return FALSE_;
}

#endif
