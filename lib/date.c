/*  $Id$
**
**  Date parsing and conversion routines.
**
**  Provides various date parsing and conversion routines, including
**  generating Date headers for posted articles.  Note that the parsedate
**  parser is separate from this file.
*/

#include "config.h"
#include "clibrary.h"
#include <ctype.h>
#include <time.h>

#include "libinn.h"

/* Do not translate these names.  RFC 822 by way of RFC 1036 requires that
   weekday and month names *not* be translated.  This is why we use static
   tables instead of strftime, to avoid locale interference.  */
static const char WEEKDAY[7][4] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static const char MONTH[12][4] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct",
    "Nov", "Dec"
};

/* The number of days in each month. */
static const int MONTHDAYS[] = {
    31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

/* Whether a given year is a leap year. */
#define ISLEAP(year) \
    (((year) % 4) == 0 && (((year) % 100) != 0 || ((year) % 400) == 0))

/* The maximum length of the date specification, not including the time zone
   comment.  Used to make sure the buffer is large enough. */
#define DATE_LENGTH     32


/*
**  Given a time as a time_t, return the offset in seconds of the local time
**  zone from UTC at that time (adding the offset to UTC time yields local
**  time).  If the second argument is true, the time represents the current
**  time and in that circumstance we can assume that timezone/altzone are
**  correct.  (We can't for arbitrary times in the past.)
*/
static long
local_tz_offset(time_t date, bool current UNUSED)
{
    struct tm *tm;
#if !HAVE_TM_GMTOFF
    struct tm local, gmt;
    long offset;
#endif

    tm = localtime(&date);

#if !HAVE_TM_GMTOFF && HAVE_VAR_TIMEZONE
    if (current)
        return (tm->tm_isdst > 0) ? -altzone : -timezone;
#endif

#if HAVE_TM_GMTOFF
    return tm->tm_gmtoff;
#else
    /* We don't have any easy returnable value, so we call both localtime
       and gmtime and calculate the difference.  Assume that local time is
       never more than 24 hours away from UTC and ignore seconds. */
    local = *tm;
    tm = gmtime(&date);
    gmt = *tm;
    offset = local.tm_yday - gmt.tm_yday;
    if (offset < -1) {
        /* Local time is in the next year. */
        offset = 24;
    } else if (offset > 1) {
        /* Local time is in the previous year. */
        offset = -24;
    } else {
        offset *= 24;
    }
    offset += local.tm_hour - gmt.tm_hour;
    offset *= 60;
    offset += local.tm_min - gmt.tm_min;
    return offset * 60;
#endif /* !HAVE_TM_GMTOFF */
}


/*
**  Given a time_t, a flag saying whether to use local time, a buffer, and
**  the length of the buffer, write the contents of a valid RFC 822 / RFC
**  1036 Date header into the buffer (provided it's long enough).  Returns
**  true on success, false if the buffer is too long.  Use sprintf rather
**  than strftime to be absolutely certain that locales don't result in the
**  wrong output.  If the time is -1, obtain and use the current time.
*/
bool
makedate(time_t date, bool local, char *buff, size_t buflen)
{
    time_t realdate;
    struct tm *tmp_tm;
    struct tm tm;
    long tz_offset;
    int tz_hour_offset, tz_min_offset, tz_sign;
    size_t date_length;
    const char *tz_name;

    /* Make sure the buffer is large enough. */
    if (buflen < DATE_LENGTH + 1)
        return false;

    /* Get the current time if the provided time is -1. */
    realdate = (date == (time_t) -1) ? time(NULL) : date;

    /* RFC 822 says the timezone offset is given as [+-]HHMM, so we have to
       separate the offset into a sign, hours, and minutes.  Dividing the
       offset by 36 looks like it works, but will fail for any offset that
       isn't an even number of hours, and there are half-hour timezones. */
    if (local) {
        tmp_tm = localtime(&realdate);
        tm = *tmp_tm;
        tz_offset = local_tz_offset(realdate, date == (time_t) -1);
        tz_sign = (tz_offset < 0) ? -1 : 1;
        tz_offset *= tz_sign;
        tz_hour_offset = tz_offset / 3600;
        tz_min_offset = (tz_offset % 3600) / 60;
    } else {
        tmp_tm = gmtime(&realdate);
        tm = *tmp_tm;
        tz_sign = 1;
        tz_hour_offset = 0;
        tz_min_offset = 0;
    }

    /* tz_min_offset cannot be larger than 60 (by basic mathematics).  If
       through some insane circumtances, tz_hour_offset would be larger,
       reject the time as invalid rather than generate an invalid date. */
    if (tz_hour_offset > 24)
        return false;

    /* Generate the actual date string, sans the trailing time zone comment
       but with the day of the week and the seconds (both of which are
       optional in the standard). */
    snprintf(buff, buflen, "%3.3s, %d %3.3s %d %02d:%02d:%02d %c%02d%02d",
             &WEEKDAY[tm.tm_wday][0], tm.tm_mday, &MONTH[tm.tm_mon][0],
             1900 + tm.tm_year, tm.tm_hour, tm.tm_min, tm.tm_sec,
             (tz_sign > 0) ? '+' : '-', tz_hour_offset, tz_min_offset);
    date_length = strlen(buff);

    /* Now, get a pointer to the time zone abbreviation, and if there is
       enough room in the buffer, add it to the end of the date string as a
       comment. */
    if (!local) {
        tz_name = "UTC";
    } else {
#if HAVE_TM_ZONE
        tz_name = tm.tm_zone;
#elif HAVE_VAR_TZNAME
        tz_name = tzname[(tm.tm_isdst > 0) ? 1 : 0];
#else
        tz_name = NULL;
#endif
    }
    if (tz_name != NULL && date_length + 4 + strlen(tz_name) <= buflen) {
        sprintf(buff + date_length, " (%s)", tz_name);
    }
    return true;
}


/*
**  Given a struct tm representing a calendar time in UTC, convert it to
**  seconds since epoch.  Returns (time_t) -1 if the time is not
**  convertable.  Note that this function does not canonicalize the provided
**  struct tm, nor does it allow out of range values or years before 1970.
*/
static time_t
mktime_utc(const struct tm *tm)
{
    time_t result = 0;
    int i;

    /* We do allow some ill-formed dates, but we don't do anything special
       with them and our callers really shouldn't pass them to us.  Do
       explicitly disallow the ones that would cause invalid array accesses
       or other algorithm problems. */
    if (tm->tm_mon < 0 || tm->tm_mon > 11 || tm->tm_year < 70)
        return (time_t) -1;

    /* Convert to a time_t. */
    for (i = 1970; i < tm->tm_year + 1900; i++)
        result += 365 + ISLEAP(i);
    for (i = 0; i < tm->tm_mon; i++)
        result += MONTHDAYS[i];
    if (tm->tm_mon > 1 && ISLEAP(tm->tm_year + 1900))
        result++;
    result = 24 * (result + tm->tm_mday - 1) + tm->tm_hour;
    result = 60 * result + tm->tm_min;
    result = 60 * result + tm->tm_sec;
    return result;
}


/*
**  Parse a date in the format used in NNTP commands such as NEWGROUPS and
**  NEWNEWS.  The first argument is a string of the form YYYYMMDD and the
**  second a string of the form HHMMSS.  The third argument is a boolean
**  flag saying whether the date is specified in local time; if false, the
**  date is assumed to be in UTC.  Returns the time_t corresponding to the
**  given date and time or (time_t) -1 in the event of an error.
*/
time_t
parsedate_nntp(const char *date, const char *hour, bool local)
{
    const char *p;
    size_t datelen;
    time_t now, result;
    struct tm tm;
    struct tm *current;
    int century;

    /* Accept YYMMDD and YYYYMMDD.  The first is what RFC 977 requires.  The
       second is what the revision of RFC 977 will require. */
    datelen = strlen(date);
    if ((datelen != 6 && datelen != 8) || strlen(hour) != 6)
        return (time_t) -1;
    for (p = date; *p; p++)
        if (!CTYPE(isdigit, *p))
            return (time_t) -1;
    for (p = hour; *p; p++)
        if (!CTYPE(isdigit, *p))
            return (time_t) -1;

    /* Parse the date into a struct tm, skipping over the century part of
       the year, if any.  We'll deal with it in a moment. */
    tm.tm_isdst = -1;
    p = date + datelen - 6;
    tm.tm_year = (p[0] - '0') * 10 + p[1] - '0';
    tm.tm_mon  = (p[2] - '0') * 10 + p[3] - '0' - 1;
    tm.tm_mday = (p[4] - '0') * 10 + p[5] - '0';
    p = hour;
    tm.tm_hour = (p[0] - '0') * 10 + p[1] - '0';
    tm.tm_min  = (p[2] - '0') * 10 + p[3] - '0';
    tm.tm_sec  = (p[4] - '0') * 10 + p[5] - '0';

    /* Do the range checks we can without knowing the year. */
    if (tm.tm_sec > 60 || tm.tm_min > 59 || tm.tm_hour > 23)
        return (time_t) -1;
    if (tm.tm_mday < 1 || tm.tm_mon < 0 || tm.tm_mon > 11)
        return (time_t) -1;

    /* Four-digit years are the easy case.

       For two-digit years, RFC 977 says "The closest century is assumed as
       part of the year (i.e., 86 specifies 1986, 30 specifies 2030, 99 is
       1999, 00 is 2000)."  draft-ietf-nntpext-base-10.txt simplifies this
       considerably and is what we implement:

         If the first two digits of the year are not specified, the year is
         to be taken from the current century if YY is smaller than or equal
         to the current year, otherwise the year is from the previous
         century.

       This implementation assumes "current year" means the last two digits
       of the current year.  Note that this algorithm interacts poorly with
       clients with a slightly fast clock around the turn of a century, as
       it may send 00 for the year when the year on the server is still xx99
       and have it taken to be 99 years in the past.  But 2000 has come and
       gone, and by 2100 news clients *really* should have started using UTC
       for everything like the new draft recommends. */
    if (datelen == 8) {
        tm.tm_year += (date[0] - '0') * 1000 + (date[1] - '0') * 100;
        tm.tm_year -= 1900;
    } else {
        now = time(NULL);
        current = local ? localtime(&now) : gmtime(&now);
        century = current->tm_year / 100;
        if (tm.tm_year > current->tm_year % 100) century--;
        tm.tm_year += century * 100;
    }

    /* Range check the day of the month, now that we know the year. */
    if (tm.tm_mday > MONTHDAYS[tm.tm_mon]
        && (tm.tm_mon != 1 || tm.tm_mday > 29
            || !ISLEAP(tm.tm_year + 1900)))
        return (time_t) -1;

    /* We can't handle years before 1970. */
    if (tm.tm_year < 70)
        return (time_t) -1;

    /* tm contains the broken-down date; convert it to a time_t.  mktime
       assumes the supplied struct tm is in the local time zone; if given a
       time in UTC, use our own routine instead. */
    result = local ? mktime(&tm) : mktime_utc(&tm);
    return result;
}
