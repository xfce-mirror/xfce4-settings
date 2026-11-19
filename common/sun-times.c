/*
 *  Copyright (c) 2026 Grigory Tuboltsev <gtubolcev@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * Sunrise and sunset for the rules of the theme schedule.
 *
 * The location is that of the system time zone: the zone tables of tzdata
 * give the coordinates of its main city, which is close enough for switching
 * a theme. The times are computed with the formulas of the NOAA Solar
 * Calculator, see https://gml.noaa.gov/grad/solcalc/calcdetails.html
 */

#include "sun-times.h"

#include <glib/gstdio.h>
#include <math.h>

/* where tzdata is if $TZDIR is not set, as for GLib */
#define DEFAULT_ZONEINFO_DIRECTORY "/usr/share/zoneinfo"

#define RADIANS(degrees) ((degrees) * G_PI / 180.0)
#define DEGREES(radians) ((radians) * 180.0 / G_PI)



/* parses the time of a rule at sunrise or sunset: "sunrise", "sunset", or
 * one of these followed by an offset in minutes, e.g. "sunset+30" or
 * "sunrise-15"; FALSE for anything else, e.g. a time of the day like "21:00" */
gboolean
sun_times_parse (const gchar *str,
                 SunTimesEvent *event,
                 gint *offset_minutes)
{
    gint64 offset = 0;
    gchar *end;

    if (str == NULL)
        return FALSE;

    if (g_str_has_prefix (str, "sunrise"))
    {
        *event = SUN_TIMES_SUNRISE;
        str += strlen ("sunrise");
    }
    else if (g_str_has_prefix (str, "sunset"))
    {
        *event = SUN_TIMES_SUNSET;
        str += strlen ("sunset");
    }
    else
        return FALSE;

    if (*str != '\0')
    {
        if ((*str != '+' && *str != '-') || !g_ascii_isdigit (str[1]))
            return FALSE;

        offset = g_ascii_strtoll (str, &end, 10);
        if (*end != '\0' || ABS (offset) > SUN_TIMES_MAX_OFFSET_MINUTES)
            return FALSE;
    }

    *offset_minutes = offset;

    return TRUE;
}



/* the time of a rule as sun_times_parse() reads it */
gchar *
sun_times_to_string (SunTimesEvent event,
                     gint offset_minutes)
{
    const gchar *name = event == SUN_TIMES_SUNRISE ? "sunrise" : "sunset";

    if (offset_minutes == 0)
        return g_strdup (name);

    return g_strdup_printf ("%s%+d", name, offset_minutes);
}



/* the number written with the @n_digits digits at @str */
static gint
sun_times_read_digits (const gchar *str,
                       gint n_digits)
{
    gint number = 0;

    for (gint i = 0; i < n_digits; i++)
        number = number * 10 + g_ascii_digit_value (str[i]);

    return number;
}



/* reads a coordinate of the zone tables in degrees: a sign, then the degrees
 * with @n_degree_digits digits (2 for a latitude, 3 for a longitude), the
 * minutes and optionally the seconds, e.g. "+5230" or "-0734515" (ISO 6709);
 * @end is set to the character after it */
static gboolean
sun_times_parse_coordinate (const gchar *str,
                            gint n_degree_digits,
                            gdouble *degrees,
                            const gchar **end)
{
    const gchar *digits = str + 1;
    gint n_digits = 0;
    gint seconds = 0;

    if (*str != '+' && *str != '-')
        return FALSE;

    while (g_ascii_isdigit (digits[n_digits]))
        n_digits++;

    if (n_digits == n_degree_digits + 4)
        seconds = sun_times_read_digits (digits + n_degree_digits + 2, 2);
    else if (n_digits != n_degree_digits + 2)
        return FALSE;

    *degrees = sun_times_read_digits (digits, n_degree_digits)
               + sun_times_read_digits (digits + n_degree_digits, 2) / 60.0
               + seconds / 3600.0;
    if (*str == '-')
        *degrees = -*degrees;
    *end = digits + n_digits;

    return TRUE;
}



/* whether @path is the file of @file_info, also through a symbolic or hard link */
static gboolean
sun_times_is_same_file (const gchar *path,
                        const GStatBuf *file_info)
{
    GStatBuf path_info;

    return g_stat (path, &path_info) == 0
           && path_info.st_dev == file_info->st_dev
           && path_info.st_ino == file_info->st_ino;
}



/* looks up the coordinates of the time zone called @zone, e.g.
 * "Europe/Berlin", or, if @zone is NULL, of the one whose file in @directory
 * is @zone_file, in the zone tables of tzdata in @directory */
static gboolean
sun_times_lookup_zone (const gchar *directory,
                       const gchar *zone,
                       const GStatBuf *zone_file,
                       gdouble *latitude,
                       gdouble *longitude)
{
    /* zone1970.tab is the current table, zone.tab still lists some zones
     * that are only links in zone1970.tab, like Europe/Amsterdam */
    const gchar *tables[] = { "zone1970.tab", "zone.tab" };
    gboolean found = FALSE;

    for (guint t = 0; t < G_N_ELEMENTS (tables) && !found; t++)
    {
        gchar *path = g_build_filename (directory, tables[t], NULL);
        gchar *contents = NULL;
        gchar **lines;

        if (!g_file_get_contents (path, &contents, NULL, NULL))
        {
            g_free (path);
            continue;
        }

        /* each line: country codes, coordinates, zone name, comment */
        lines = g_strsplit (contents, "\n", -1);
        for (guint i = 0; lines[i] != NULL && !found; i++)
        {
            gchar **columns = g_strsplit (lines[i], "\t", 4);
            gboolean is_zone = FALSE;
            const gchar *end;

            if (lines[i][0] != '#' && g_strv_length (columns) >= 3)
            {
                if (zone != NULL)
                    is_zone = g_strcmp0 (columns[2], zone) == 0;
                else
                {
                    gchar *zone_path = g_build_filename (directory, columns[2], NULL);

                    is_zone = sun_times_is_same_file (zone_path, zone_file);
                    g_free (zone_path);
                }
            }

            /* the coordinates are a latitude and a longitude without a space
             * between them, e.g. "+5230+01322" */
            if (is_zone)
                found = sun_times_parse_coordinate (columns[1], 2, latitude, &end)
                        && sun_times_parse_coordinate (end, 3, longitude, &end)
                        && *end == '\0';

            g_strfreev (columns);
        }

        g_strfreev (lines);
        g_free (contents);
        g_free (path);
    }

    return found;
}



/* finds the coordinates of the system time zone; FALSE if it has none, e.g.
 * if it is UTC */
static gboolean
sun_times_get_location (gdouble *latitude,
                        gdouble *longitude)
{
    /* the zone tables are read again only when the system time zone changes */
    static gchar *cached_zone = NULL;
    static gboolean cached_found = FALSE;
    static gdouble cached_latitude, cached_longitude;
    GTimeZone *tz;
    const gchar *zone;

    tz = g_time_zone_new_local ();

    /* the identifier is the name of the zone, e.g. "Europe/Berlin", or a file
     * if $TZ names one, e.g. ":/etc/localtime" */
    zone = g_time_zone_get_identifier (tz);
    if (zone[0] == ':')
        zone++;

    if (g_strcmp0 (zone, cached_zone) != 0)
    {
        const gchar *directory = g_getenv ("TZDIR");
        GStatBuf zone_file;
        gchar *path;

        if (directory == NULL || directory[0] == '\0')
            directory = DEFAULT_ZONEINFO_DIRECTORY;

        /* the tables have only the current names: an old one like Europe/Kiev,
         * or a file like /etc/localtime, is a link to the file of a current one */
        path = g_path_is_absolute (zone) ? g_strdup (zone) : g_build_filename (directory, zone, NULL);
        cached_found = sun_times_lookup_zone (directory, zone, NULL, &cached_latitude, &cached_longitude)
                       || (g_stat (path, &zone_file) == 0
                           && sun_times_lookup_zone (directory, NULL, &zone_file, &cached_latitude, &cached_longitude));
        g_free (path);

        g_free (cached_zone);
        cached_zone = g_strdup (zone);
    }

    g_time_zone_unref (tz);
    *latitude = cached_latitude;
    *longitude = cached_longitude;

    return cached_found;
}



/* whether the sunrise and sunset can be computed, i.e. the system time zone
 * has a location */
gboolean
sun_times_has_location (void)
{
    gdouble latitude, longitude;

    return sun_times_get_location (&latitude, &longitude);
}



/* computes the sunrise and sunset of the date of @day at the place @latitude,
 * @longitude, in minutes after 00:00 UTC of that date: before 0 or after
 * 24 * 60 if they are on the day before or after in UTC; FALSE if the sun
 * does not rise or set that day (polar day or night) */
static gboolean
sun_times_compute (GDateTime *day,
                   gdouble latitude,
                   gdouble longitude,
                   gdouble *sunrise_minutes_utc,
                   gdouble *sunset_minutes_utc)
{
    GDateTime *noon;
    gdouble julian_century, mean_longitude, mean_anomaly, eccentricity;
    gdouble center, apparent_longitude, obliquity, declination;
    gdouble y, equation_of_time, cos_hour_angle, hour_angle;

    /* The formulas and their constants are those of the NOAA spreadsheet, in
     * the order of its columns. They start from the time since the year 2000
     * in Julian centuries, at noon UTC: Unix time 0 is the Julian day
     * 2440587.5, and the year 2000 starts at the Julian day 2451545. */
    noon = g_date_time_new_utc (g_date_time_get_year (day), g_date_time_get_month (day),
                                g_date_time_get_day_of_month (day), 12, 0, 0);
    julian_century = (g_date_time_to_unix (noon) / 86400.0 + 2440587.5 - 2451545.0) / 36525.0;
    g_date_time_unref (noon);

    /* the position of the sun, in degrees */
    mean_longitude = fmod (280.46646 + julian_century * (36000.76983 + julian_century * 0.0003032), 360.0);
    mean_anomaly = 357.52911 + julian_century * (35999.05029 - 0.0001537 * julian_century);
    eccentricity = 0.016708634 - julian_century * (0.000042037 + 0.0000001267 * julian_century);
    center = sin (RADIANS (mean_anomaly)) * (1.914602 - julian_century * (0.004817 + 0.000014 * julian_century))
             + sin (RADIANS (2 * mean_anomaly)) * (0.019993 - 0.000101 * julian_century)
             + sin (RADIANS (3 * mean_anomaly)) * 0.000289;
    apparent_longitude = mean_longitude + center - 0.00569
                         - 0.00478 * sin (RADIANS (125.04 - 1934.136 * julian_century));
    obliquity = 23.0 + (26.0 + (21.448 - julian_century * (46.815 + julian_century * (0.00059 - julian_century * 0.001813))) / 60.0) / 60.0
                + 0.00256 * cos (RADIANS (125.04 - 1934.136 * julian_century));
    declination = asin (sin (RADIANS (obliquity)) * sin (RADIANS (apparent_longitude)));

    /* how much the solar noon differs from 12:00 at longitude 0, in minutes */
    y = tan (RADIANS (obliquity / 2)) * tan (RADIANS (obliquity / 2));
    equation_of_time = y * sin (2 * RADIANS (mean_longitude));
    equation_of_time -= 2 * eccentricity * sin (RADIANS (mean_anomaly));
    equation_of_time += 4 * eccentricity * y * sin (RADIANS (mean_anomaly)) * cos (2 * RADIANS (mean_longitude));
    equation_of_time -= 0.5 * y * y * sin (4 * RADIANS (mean_longitude));
    equation_of_time -= 1.25 * eccentricity * eccentricity * sin (2 * RADIANS (mean_anomaly));
    equation_of_time = 4 * DEGREES (equation_of_time);

    /* the upper edge of the sun at the horizon, 90.833° from the zenith
     * because of the refraction */
    cos_hour_angle = cos (RADIANS (90.833)) / (cos (RADIANS (latitude)) * cos (declination))
                     - tan (RADIANS (latitude)) * tan (declination);
    if (cos_hour_angle < -1 || cos_hour_angle > 1)
        return FALSE;
    hour_angle = DEGREES (acos (cos_hour_angle));

    /* the earth turns by one degree in 4 minutes */
    *sunrise_minutes_utc = 720 - 4 * (longitude + hour_angle) - equation_of_time;
    *sunset_minutes_utc = 720 - 4 * (longitude - hour_angle) - equation_of_time;

    return TRUE;
}



/* the local time of a rule at sunrise or sunset (see sun_times_parse()) on
 * the local day of @day, in minutes since midnight; -1 if the sun does not
 * rise or set that day, or if the system time zone has no location.
 * An offset that moves the time to the day before or after, e.g. sunset
 * plus three hours in the far north in summer, gives 00:00 or 23:59 instead,
 * so that the rule still switches once a day. */
gint
sun_times_get_minutes_since_midnight (GDateTime *day,
                                      SunTimesEvent event,
                                      gint offset_minutes)
{
    GDateTime *midnight_utc, *at, *day_start, *day_end;
    gdouble latitude, longitude, sunrise_minutes_utc, sunset_minutes_utc, minutes_utc;
    gint minutes_since_midnight;

    /* the times are computed for the local date in UTC, which is a few
     * minutes off at most where that UTC date is another day */
    if (!sun_times_get_location (&latitude, &longitude)
        || !sun_times_compute (day, latitude, longitude, &sunrise_minutes_utc, &sunset_minutes_utc))
        return -1;

    minutes_utc = (event == SUN_TIMES_SUNRISE ? sunrise_minutes_utc : sunset_minutes_utc) + offset_minutes;
    midnight_utc = g_date_time_new_utc (g_date_time_get_year (day), g_date_time_get_month (day),
                                        g_date_time_get_day_of_month (day), 0, 0, 0);
    at = g_date_time_add_seconds (midnight_utc, minutes_utc * 60);
    day_start = g_date_time_new_local (g_date_time_get_year (day), g_date_time_get_month (day),
                                       g_date_time_get_day_of_month (day), 0, 0, 0);
    day_end = g_date_time_add_days (day_start, 1);

    if (g_date_time_compare (at, day_start) < 0)
        minutes_since_midnight = 0;
    else if (g_date_time_compare (at, day_end) >= 0)
        minutes_since_midnight = 24 * 60 - 1;
    else
    {
        GDateTime *at_local = g_date_time_to_local (at);

        minutes_since_midnight = g_date_time_get_hour (at_local) * 60 + g_date_time_get_minute (at_local);
        g_date_time_unref (at_local);
    }

    g_date_time_unref (day_end);
    g_date_time_unref (day_start);
    g_date_time_unref (at);
    g_date_time_unref (midnight_utc);

    return minutes_since_midnight;
}
