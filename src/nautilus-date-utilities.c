/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-date-utilities.h"

#include "nautilus-global-preferences.h"

#include <gdesktop-enums.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <time.h>

#include <unicode/udat.h>
#include <unicode/ureldatefmt.h>
#include <unicode/unum.h>
#include <unicode/ustring.h>

static gboolean use_24_hour;
static gboolean use_detailed_date_format;

static void
clock_format_changed_callback (gpointer)
{
    gint clock_format = g_settings_get_enum (gnome_interface_preferences, "clock-format");
    use_24_hour = (clock_format == G_DESKTOP_CLOCK_FORMAT_24H);
}

static void
date_format_changed_callback (gpointer)
{
    NautilusDateTimeFormat format = g_settings_get_enum (nautilus_preferences,
                                                         NAUTILUS_PREFERENCES_DATE_TIME_FORMAT);
    use_detailed_date_format = (format == NAUTILUS_DATE_TIME_FORMAT_DETAILED);
}

void
nautilus_date_setup_preferences (void)
{
    clock_format_changed_callback (NULL);
    g_signal_connect_swapped (gnome_interface_preferences,
                              "changed::clock-format",
                              G_CALLBACK (clock_format_changed_callback),
                              NULL);

    date_format_changed_callback (NULL);
    g_signal_connect_swapped (nautilus_preferences,
                              "changed::" NAUTILUS_PREFERENCES_DATE_TIME_FORMAT,
                              G_CALLBACK (date_format_changed_callback),
                              NULL);
}

static char *
date_to_str (GDateTime *timestamp,
             gboolean   use_short_format,
             gboolean   detailed_date)
{
    GTimeZone *local_tz = g_date_time_get_timezone(timestamp);
    gint64 timestamp_ms = g_date_time_to_unix(timestamp) * 1000;
    g_autofree char *locale = NULL;
    const char *icu_locale = NULL;
    UErrorCode status = U_ZERO_ERROR;
    UDateFormat *formatter = NULL;
    URelativeDateTimeFormatter *relative_formatter = NULL;
    g_autofree UChar *formatted_uchar = NULL;
    g_autofree char *formatted_utf8 = NULL;
    int32_t formatted_len = 0;

    /* Get the current locale for ICU */
    locale = g_strdup(setlocale(LC_TIME, NULL));
    if (locale != NULL)
    {
        /* Convert locale format (e.g., en_US.UTF-8) to ICU format (en_US) */
        char *dot = strchr(locale, '.');
        if (dot != NULL)
        {
            *dot = '\0';
        }
        icu_locale = locale;
    }
    else
    {
        /* Fallback to default locale if setlocale returns NULL */
        icu_locale = uloc_getDefault();
    }

    if (use_short_format && !detailed_date)
    {
        gint64 now_ms = g_date_time_to_unix(g_date_time_new_now(local_tz)) * 1000;
        URelativeDateTimeUnit unit;
        double relative_value = (double)(timestamp_ms - now_ms) / (1000.0 * 60 * 60 * 24);

        /* Use relative formatting for today/yesterday */
        if (relative_value > -2.0 && relative_value < 1.0)
        {
            relative_formatter = ureldatefmt_open(icu_locale, NULL, UDAT_STYLE_SHORT, UDAT_STYLE_NONE, &status);
            if (U_SUCCESS(status))
            {
                /* Determine if it's today or yesterday based on the beginning of the day */
                g_autoptr(GDateTime) now_dt = g_date_time_new_from_unix_local(now_ms / 1000);
                g_autoptr(GDateTime) timestamp_dt = g_date_time_new_from_unix_local(timestamp_ms / 1000);
                gint now_day = g_date_time_get_day_of_year(now_dt);
                gint timestamp_day = g_date_time_get_day_of_year(timestamp_dt);
                gint now_year = g_date_time_get_year(now_dt);
                gint timestamp_year = g_date_time_get_year(timestamp_dt);

                double day_diff = 0.0;
                if (now_year == timestamp_year)
                {
                    day_diff = (double)(timestamp_day - now_day);
                }
                else if (timestamp_year == now_year - 1 && timestamp_day >= g_date_time_get_days_in_year(timestamp_year) - 1 && now_day == 1)
                {
                    // Handle year boundary for yesterday
                    day_diff = -1.0;
                }
                else
                {
                    // Further than yesterday, use absolute date below
                    day_diff = -2.0; // Sentinel value
                }

                if (day_diff == 0.0)
                {
                    unit = UDAT_REL_UNIT_DAY;
                    relative_value = 0.0;
                }
                else if (day_diff == -1.0)
                {
                    unit = UDAT_REL_UNIT_DAY;
                    relative_value = -1.0;
                }
                else
                {
                    /* Not today or yesterday, fall through to absolute date below */
                    ureldatefmt_close(relative_formatter);
                    relative_formatter = NULL;
                }

                if (relative_formatter)
                {
                    /* Format relative date ("Today" / "Yesterday") */
                    g_autofree UChar *relative_part_uchar = NULL;
                    int32_t relative_part_len = ureldatefmt_format(relative_formatter, relative_value, unit, NULL, 0, &status);
                    if (status == U_BUFFER_OVERFLOW_ERROR)
                    {
                        status = U_ZERO_ERROR;
                        relative_part_uchar = g_new(UChar, relative_part_len + 1);
                        ureldatefmt_format(relative_formatter, relative_value, unit, relative_part_uchar, relative_part_len + 1, &status);
                    }
                    ureldatefmt_close(relative_formatter);

                    /* Format time part */
                    formatter = udat_open(UDAT_PATTERN, use_24_hour ? "HH:mm" : "h:mm a", -1, icu_locale, NULL, 0, &status);
                    if (U_SUCCESS(status))
                    {
                        int32_t time_part_len = udat_format(formatter, timestamp_ms, NULL, 0, NULL, &status);
                        if (status == U_BUFFER_OVERFLOW_ERROR)
                        {
                            status = U_ZERO_ERROR;
                            g_autofree UChar *time_part_uchar = g_new(UChar, time_part_len + 1);
                            udat_format(formatter, timestamp_ms, time_part_uchar, time_part_len + 1, NULL, &status);

                            if (U_SUCCESS(status) && relative_part_uchar != NULL)
                            {
                                /* Combine relative date and time (assuming "Date Time" pattern) */
                                formatted_len = relative_part_len + 1 + time_part_len; /* +1 for space */
                                formatted_uchar = g_new(UChar, formatted_len + 1);
                                u_strcpy(formatted_uchar, relative_part_uchar);
                                u_strcat(formatted_uchar, (UChar[]){' ', 0}); /* Add space */
                                u_strcat(formatted_uchar, time_part_uchar);
                            }
                        }
                        udat_close(formatter);
                        formatter = NULL; /* Ensure it's not closed again */
                    }
                }
            }
            else
            {
                /* Handle error opening relative date formatter if necessary */
                g_warning("ICU failed to open relative date formatter: %s", u_errorName(status));
            }
            status = U_ZERO_ERROR; /* Reset status for potential fallback */
        }

        /* Fallback or if not today/yesterday: Use absolute date format */
        if (formatted_uchar == NULL)
        {
            const char *skeleton = "yMMMd"; /* e.g., "Feb 3, 2015" */
            formatter = udat_open(UDAT_PATTERN, NULL, 0, icu_locale, NULL, -1, &status);
            if (U_SUCCESS(status))
            {
                UChar pattern[100];
                int32_t pattern_len = udat_toPatternRelativeDate(formatter, skeleton, -1, pattern, G_N_ELEMENTS(pattern), &status);
                if (U_SUCCESS(status))
                {
                    udat_applyPattern(formatter, FALSE, pattern, pattern_len);
                }
                else
                {
                    g_warning("ICU failed to get pattern for skeleton %s: %s", skeleton, u_errorName(status));
                    udat_applyPattern(formatter, FALSE, (UChar *)L"MMM d, y", -1); // Fallback pattern
                }
            }
            else
            {
                g_warning("ICU failed to open date formatter: %s", u_errorName(status));
            }
        }
    }
    else /* Long format or detailed short format */
    {
        const char *date_skeleton;
        const char *time_skeleton;

        if (use_short_format && detailed_date)
        {
            /* e.g., "12/31/2023 11:59 PM" */
            date_skeleton = "yMd";
            time_skeleton = use_24_hour ? "HHmm" : "hhmm a";
        }
        else /* Long format */
        {
            /* e.g., "February 3, 2015 09:04:00 PM" */
            date_skeleton = "yMMMMd";
            time_skeleton = use_24_hour ? "HHmmss" : "hhmmss a";
        }

        formatter = udat_open(UDAT_PATTERN, NULL, 0, icu_locale, NULL, -1, &status);
        if (U_SUCCESS(status))
        {
            UChar pattern[100];
            int32_t pattern_len = udat_toPatternRelativeDateTime(formatter, date_skeleton, -1, time_skeleton, -1, pattern, G_N_ELEMENTS(pattern), &status);

            if (U_SUCCESS(status))
            {
                udat_applyPattern(formatter, FALSE, pattern, pattern_len);
            }
            else
            {
                g_warning("ICU failed to get pattern for skeletons %s %s: %s", date_skeleton, time_skeleton, u_errorName(status));
                // Construct a fallback pattern based on requested style
                const UChar *fallback_pattern = use_short_format ? (use_24_hour ? L"M/d/y HH:mm" : L"M/d/y h:mm a")
                                                                 : (use_24_hour ? L"MMMM d, y HH:mm:ss" : L"MMMM d, y h:mm:ss a");
                udat_applyPattern(formatter, FALSE, fallback_pattern, -1);
            }
        }
        else
        {
            g_warning("ICU failed to open date/time formatter: %s", u_errorName(status));
        }
    }

    /* Format the date if a formatter was successfully created and not already formatted (relative case) */
    if (formatter != NULL && formatted_uchar == NULL)
    {
        status = U_ZERO_ERROR;
        formatted_len = udat_format(formatter, timestamp_ms, NULL, 0, NULL, &status);
        if (status == U_BUFFER_OVERFLOW_ERROR)
        {
            status = U_ZERO_ERROR;
            formatted_uchar = g_new(UChar, formatted_len + 1);
            udat_format(formatter, timestamp_ms, formatted_uchar, formatted_len + 1, NULL, &status);
        }
        if (!U_SUCCESS(status))
        {
            g_warning("ICU failed to format date: %s", u_errorName(status));
            g_free(formatted_uchar);
            formatted_uchar = NULL;
            formatted_len = 0;
        }
        udat_close(formatter);
    }

    /* Convert UChar* result to UTF-8 char* */
    if (formatted_uchar != NULL)
    {
        status = U_ZERO_ERROR;
        int32_t utf8_len = 0;
        u_strToUTF8(NULL, 0, &utf8_len, formatted_uchar, formatted_len, &status);
        if (status == U_BUFFER_OVERFLOW_ERROR)
        {
            status = U_ZERO_ERROR;
            formatted_utf8 = g_new(char, utf8_len + 1);
            u_strToUTF8(formatted_utf8, utf8_len + 1, NULL, formatted_uchar, formatted_len, &status);
        }
        if (!U_SUCCESS(status))
        {
            g_warning("ICU failed to convert formatted date to UTF-8: %s", u_errorName(status));
            g_free(formatted_utf8);
            formatted_utf8 = NULL;
        }
    }

    /* Fallback if ICU formatting failed */
    if (formatted_utf8 == NULL)
    {
        const char *fallback_format;
        if (use_short_format && !detailed_date)
        {
            fallback_format = use_24_hour ? "%x %H:%M" : "%x %I:%M %p"; // Use locale default short date
        }
        else if (use_short_format && detailed_date)
        {
            fallback_format = use_24_hour ? "%x %H:%M" : "%x %I:%M %p"; // Use locale default short date + time
        }
        else
        {
            fallback_format = use_24_hour ? "%c" : "%x %I:%M:%S %p"; // Use locale default long date + time
        }
        formatted_utf8 = g_date_time_format(timestamp, fallback_format);
        if (!formatted_utf8)
        {
            /* Ultimate fallback */
            formatted_utf8 = g_strdup("Invalid Date");
        }
    }

    /* Replace ":" with ratio. */
    GString *time_label = g_string_new(formatted_utf8);
    g_string_replace (time_label, ":", "∶", 0);
    g_free(formatted_utf8); // Free the intermediate string

    return g_string_free_and_steal (time_label);
}

char *
nautilus_date_to_str (GDateTime *timestamp,
                      gboolean   use_short_format)
{
    return date_to_str (timestamp, use_short_format, use_detailed_date_format);
}

char *
nautilus_date_preview_detailed_format (GDateTime *timestamp,
                                       gboolean   use_detailed)
{
    return date_to_str (timestamp, TRUE, use_detailed);
}
