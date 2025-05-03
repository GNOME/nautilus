/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-date-utilities.h"

#include "nautilus-global-preferences.h"

#include <gdesktop-enums.h>
#include <glib.h>
#include <locale.h>
#include <time.h>

#include <unicode/udatpg.h>
#include <unicode/ureldatefmt.h>

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

G_DEFINE_AUTOPTR_CLEANUP_FUNC (UDateFormat, udat_close);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (UDateTimePatternGenerator, udatpg_close);
G_DEFINE_AUTOPTR_CLEANUP_FUNC (URelativeDateTimeFormatter, ureldatefmt_close);
G_DEFINE_AUTO_CLEANUP_FREE_FUNC (UCalendar, ucal_close, NULL);

static char *
date_to_str (GDateTime *timestamp,
             gboolean   use_short_format,
             gboolean   detailed_date)
{
    static const char *icu_locale = NULL;
    UErrorCode status = U_ZERO_ERROR;
    g_autoptr (UDateTimePatternGenerator) pattern_gen = NULL;
    g_autofree char *formatted_utf8 = NULL;

    if (icu_locale == NULL)
    {
        gchar *locale = setlocale (LC_TIME, NULL);

        if (locale != NULL)
        {
            /* Convert locale format (e.g., en_US.UTF-8) to ICU format (en_US) */
            char *dot = strchr (locale, '.');
            if (dot != NULL)
            {
                *dot = '\0';
            }
            icu_locale = locale;
        }
        else
        {
            /* Fallback to default locale if setlocale returns NULL */
            icu_locale = uloc_getDefault ();
        }
    }

    pattern_gen = udatpg_open (icu_locale, &status);

    do
    {
        g_autoptr (UDateFormat) formatter = NULL;
        UChar time_pattern[64];
        guint32 time_pattern_len;

        if (use_short_format && !detailed_date)
        {
            /* Re-use local time zone, because every time a new local time zone is
             * created, GLib needs to check if the time zone file has changed */
            GTimeZone *local_tz = g_date_time_get_timezone (timestamp);
            g_autoptr (GDateTime) now = g_date_time_new_now (local_tz);
            g_autoptr (GDateTime) today_midnight = g_date_time_new (local_tz,
                                                                    g_date_time_get_year (now),
                                                                    g_date_time_get_month (now),
                                                                    g_date_time_get_day_of_month (now),
                                                                    0, 0, 0);
            g_autoptr (GDateTime) date_midnight = g_date_time_new (local_tz,
                                                                   g_date_time_get_year (timestamp),
                                                                   g_date_time_get_month (timestamp),
                                                                   g_date_time_get_day_of_month (timestamp),
                                                                   0, 0, 0);
            GTimeSpan midnight_difference = g_date_time_difference (today_midnight, date_midnight);
            double relative_value = (double) g_date_time_difference (now, timestamp) / G_TIME_SPAN_DAY;
            gboolean add_time = FALSE;
            URelativeDateTimeUnit unit;
            g_autoptr (URelativeDateTimeFormatter) relative_formatter = NULL;
            UChar relative_date_buf[128];

            if (llabs (midnight_difference) < 2 * G_TIME_SPAN_DAY)
            {
                /* Use explicit day boundary, and show time on
                 * Today/yesterday/tomorrow. */
                relative_value = midnight_difference / G_TIME_SPAN_DAY;
                add_time = TRUE;
            }

            if (relative_value < 7.0)
            {
                unit = UDAT_REL_UNIT_DAY;
            }
            else if (relative_value < 31)
            {
                unit = UDAT_REL_UNIT_WEEK;
                relative_value /= 7.0;
            }
            else if (relative_value < 365)
            {
                unit = UDAT_REL_UNIT_MONTH;
                relative_value /= 31.0;
            }
            else
            {
                unit = UDAT_REL_UNIT_YEAR;
                relative_value /= 365.0;
            }

            relative_formatter = ureldatefmt_open (icu_locale, NULL,
                                                   UDAT_STYLE_LONG, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE,
                                                   &status);

            if (!U_SUCCESS (status))
            {
                break;
            }

            gint32 relative_date_len = ureldatefmt_format (relative_formatter,
                                                           (int) -relative_value, unit,
                                                           relative_date_buf, G_N_ELEMENTS (relative_date_buf),
                                                           &status);

            if (!U_SUCCESS (status))
            {
                break;
            }

            if (add_time)
            {
                const UChar *time_skeleton = use_24_hour ? u"Hm" : u"hm";

                time_pattern_len = udatpg_getBestPatternWithOptions (pattern_gen,
                                                                     time_skeleton, -1,
                                                                     UDATPG_MATCH_NO_OPTIONS,
                                                                     time_pattern, G_N_ELEMENTS (time_pattern),
                                                                     &status);

                if (!U_SUCCESS (status))
                {
                    break;
                }

                formatter = udat_open (UDAT_PATTERN, UDAT_NONE, icu_locale,
                                       0, -1,
                                       time_pattern, time_pattern_len,
                                       &status);

                if (!U_SUCCESS (status))
                {
                    break;
                }

                UChar time_buffer[256];
                UChar combined_buffer[512];
                gint32 time_len = udat_format (formatter,
                                               g_date_time_to_unix (timestamp) * 1000,
                                               time_buffer, G_N_ELEMENTS (time_buffer),
                                               NULL,
                                               &status);

                if (!U_SUCCESS (status))
                {
                    break;
                }

                gint32 combinedLength = ureldatefmt_combineDateAndTime (relative_formatter,
                                                                        relative_date_buf, relative_date_len,
                                                                        time_buffer, time_len,
                                                                        combined_buffer, G_N_ELEMENTS (combined_buffer),
                                                                        &status);

                if (!U_SUCCESS (status))
                {
                    break;
                }

                formatted_utf8 = g_utf16_to_utf8 (combined_buffer, combinedLength, NULL, NULL, NULL);
            }
            else
            {
                formatted_utf8 = g_utf16_to_utf8 (relative_date_buf, relative_date_len, NULL, NULL, NULL);
            }
        }
        else /* Long format or detailed short format */
        {
            const UChar *datetime_skeleton = use_short_format
                                             ? (use_24_hour ? u"yMd, hm" : u"yMd, Hm")
                                             : (use_24_hour ? u"yMd, hms" : u"yMd, Hms");
            g_auto (UCalendar) gregorian_cal = ucal_open (0, -1, icu_locale, UCAL_GREGORIAN, &status);

            if (!U_SUCCESS (status))
            {
                break;
            }

            time_pattern_len = udatpg_getBestPatternWithOptions (pattern_gen,
                                                                 datetime_skeleton, -1,
                                                                 UDATPG_MATCH_NO_OPTIONS,
                                                                 time_pattern, G_N_ELEMENTS (time_pattern),
                                                                 &status);

            if (!U_SUCCESS (status))
            {
                break;
            }

            formatter = udat_open (UDAT_PATTERN, UDAT_PATTERN, icu_locale,
                                   0, -1,
                                   time_pattern, time_pattern_len,
                                   &status);

            if (!U_SUCCESS (status))
            {
                break;
            }

            UChar result_buffer[512];
            gint64 timestamp_ms = g_date_time_to_unix (timestamp) * 1000;
            guint32 result_buffer_len;

            /* Force Gregorian calendar */
            udat_setCalendar (formatter, gregorian_cal);

            result_buffer_len = udat_format (formatter, timestamp_ms,
                                             result_buffer, sizeof (result_buffer) / sizeof (result_buffer[0]),
                                             NULL,
                                             &status);

            if (!U_SUCCESS (status))
            {
                break;
            }

            formatted_utf8 = g_utf16_to_utf8 (result_buffer, result_buffer_len, NULL, NULL, NULL);
        }
    }
    while (false);

    if (!U_SUCCESS (status))
    {
        g_warning ("ICU failed create a formatted date: %s", u_errorName (status));
    }

    /* Fallback if ICU formatting failed */
    if (formatted_utf8 == NULL)
    {
        formatted_utf8 = g_date_time_format (timestamp, "%x %X");
    }

    return g_steal_pointer (&formatted_utf8);
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
