/*
 * Copyright 2016, Data61, CSIRO (ABN 41 687 119 230)
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <time.h>
#include <stdio.h>

/*! @file
    @brief Supports printing the current time in a human readable form
*/

char *weekdays[7] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
char *months[12] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};

/*! @brief Returns the day as a string */
char * day_to_string(int day)
{
    return weekdays[day - 1];
}

/*! @brief Returns the month as a string */
char * month_to_string(int month)
{
    return months[month];
}

/*! @brief Formats the time string */
char * refos_print_time (struct tm *tm)
{
    static char buf[47];
    snprintf(buf, 47, "%s, %s%3d %.2d:%.2d:%.2d %d\n",
		    day_to_string(tm->tm_wday),
		    month_to_string(tm->tm_mon),
		    tm->tm_mday, tm->tm_hour,
            tm->tm_min, tm->tm_sec,
            1900 + tm->tm_year);
    return buf;
}
