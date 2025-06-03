/*
 * Copyright (C) 2023 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#define G_LOG_DOMAIN "nautilus-date-utilities"

#include "nautilus-date-utilities.h"

#include "nautilus-global-preferences.h"

#include <gdesktop-enums.h>
#include <glib.h>
#include <glib/gi18n.h>
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

static gchar *
posix_locale_to_icu (const gchar *posix_locale)
{
    if (strpbrk (posix_locale, "_.@") == NULL)
    {
        return g_strdup (posix_locale);
    }

    GString *icu_id = g_string_new (posix_locale);
    gchar *dot_ptr = strchr (icu_id->str, '.');

    /* Remove the encoding, which comes prefixed with a dot */
    if (dot_ptr != NULL)
    {
        g_string_erase (icu_id, dot_ptr - icu_id->str, strcspn (dot_ptr, "_@"));
    }

    /* This is for currency only so disregard it. */
    g_string_replace (icu_id, "@euro", "", -1);

    /* These are the respective variants for libicu */
    g_string_replace (icu_id, "@latin", "@Latn", -1);
    g_string_replace (icu_id, "@cyrillic", "@Cyrl", -1);
    g_string_replace (icu_id, "@devanagari", "@Deva", -1);

    gchar *underscore_ptr = strchr (icu_id->str, '_');
    gchar *at_ptr = strchr (icu_id->str, '@');

    /* Put variants after first part, e.g. sr_RS@Cyrl to sr@Cyrl_RS */
    if (at_ptr != NULL &&
        underscore_ptr != NULL &&
        underscore_ptr < at_ptr)
    {
        gsize at_segment_len = strcspn (at_ptr, "_");
        g_autofree gchar *at_segment = g_strndup (at_ptr, at_segment_len);

        g_string_erase (icu_id, at_ptr - icu_id->str, at_segment_len);
        g_string_insert (icu_id, underscore_ptr - icu_id->str, at_segment);
    }

    g_string_replace (icu_id, "@", "_", -1);

    return g_string_free_and_steal (icu_id);
}

/* We initialize these only sparingly to increase performance. */
static char *icu_locale;
static URelativeDateTimeFormatter *relative_formatter;
static UCalendar gregorian_cal;
static UDateFormat *time_12_hour_formatter;
static UDateFormat *time_24_hour_formatter;
static UDateFormat *datetime_short_12_hour_formatter;
static UDateFormat *datetime_short_24_hour_formatter;
static UDateFormat *datetime_detailed_12_hour_formatter;
static UDateFormat *datetime_detailed_24_hour_formatter;

static void
ensure_icu_locale (void)
{
    /* The time locale might be different from the locale we pick translations
     * from; i.e. different locales for LC_MESSAGES and LC_TIME. So we need to
     * to derive ICU locale from the time locale.
     */
    if (G_UNLIKELY (icu_locale == NULL))
    {
        UErrorCode status = U_ZERO_ERROR;
        g_autofree char *time_locale = posix_locale_to_icu (setlocale (LC_TIME, NULL));
        gchar buffer[64] = {0};
        guint32 size = uloc_canonicalize (time_locale, buffer, G_N_ELEMENTS (buffer), &status);

        if (U_FAILURE (status))
        {
            g_warning ("ICU failed to canonicalize locale: %s", u_errorName (status));

            icu_locale = g_strdup (time_locale);
        }
        else
        {
            icu_locale = g_strndup (buffer, size);
        }

        g_debug ("ICU date locale: %s", icu_locale);
    }
}

static UErrorCode
ensure_formatters (GTimeZone *new_tz)
{
    static UErrorCode status = U_ZERO_ERROR;
    static UDateTimePatternGenerator *pattern_gen;
    static gchar *tz_id;

    if (U_FAILURE (status))
    {
        return status;
    }

    if (G_UNLIKELY (pattern_gen == NULL &&
                    relative_formatter == NULL))
    {
        ensure_icu_locale ();

        pattern_gen = udatpg_open (icu_locale, &status);

        if (U_FAILURE (status))
        {
            return status;
        }

        relative_formatter = ureldatefmt_open (icu_locale, NULL,
                                               UDAT_STYLE_LONG,
                                               UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE,
                                               &status);

        if (U_FAILURE (status))
        {
            return status;
        }
    }

    /* Detect if timezone has changed and reset the formatters. */
    if (g_set_str (&tz_id, g_time_zone_get_identifier (new_tz)))
    {
        g_clear_pointer (&gregorian_cal, ucal_close);
        g_clear_pointer (&time_12_hour_formatter, udat_close);
        g_clear_pointer (&time_24_hour_formatter, udat_close);
        g_clear_pointer (&datetime_short_12_hour_formatter, udat_close);
        g_clear_pointer (&datetime_short_24_hour_formatter, udat_close);
        g_clear_pointer (&datetime_detailed_12_hour_formatter, udat_close);
        g_clear_pointer (&datetime_detailed_24_hour_formatter, udat_close);
    }

    if (time_12_hour_formatter == NULL)
    {
        gregorian_cal = ucal_open (NULL, -1, icu_locale, UCAL_GREGORIAN, &status);

        if (U_FAILURE (status))
        {
            return status;
        }

        struct
        {
            UDateFormat **formatter;
            const UChar *skeleton;
            UDateFormatStyle time_style;
            UDateFormatStyle date_style;
        } formatter_setup[] =
        {
            /* Time formatters, 12 and 24 hours */
            {&time_12_hour_formatter, u"hm", UDAT_PATTERN, UDAT_NONE},
            {&time_24_hour_formatter, u"Hm", UDAT_PATTERN, UDAT_NONE},
            /* Datetime formatters, short and detailed, 12 and 24 hours */
            {&datetime_short_12_hour_formatter, u"yMd, hm", UDAT_PATTERN, UDAT_PATTERN},
            {&datetime_short_24_hour_formatter, u"yMd, Hm", UDAT_PATTERN, UDAT_PATTERN},
            {&datetime_detailed_12_hour_formatter, u"yMd, hms", UDAT_PATTERN, UDAT_PATTERN},
            {&datetime_detailed_24_hour_formatter, u"yMd, Hms", UDAT_PATTERN, UDAT_PATTERN},
        };

        for (uint i = 0; i < G_N_ELEMENTS (formatter_setup); i++)
        {
            UChar buffer[128];
            guint32 buffer_len;

            buffer_len = udatpg_getBestPatternWithOptions (pattern_gen,
                                                           formatter_setup[i].skeleton, -1,
                                                           UDATPG_MATCH_NO_OPTIONS,
                                                           buffer, G_N_ELEMENTS (buffer),
                                                           &status);

            if (U_FAILURE (status))
            {
                return status;
            }

            *formatter_setup[i].formatter = udat_open (formatter_setup[i].time_style,
                                                       formatter_setup[i].date_style,
                                                       icu_locale,
                                                       0, -1,
                                                       buffer, buffer_len,
                                                       &status);

            if (U_FAILURE (status))
            {
                return status;
            }

            if (formatter_setup[i].date_style != UDAT_NONE)
            {
                /* Force Gregorian calendar */
                udat_setCalendar (*formatter_setup[i].formatter, gregorian_cal);
            }
        }
    }

    return status;
}

static char *
date_to_str (GDateTime *timestamp,
             gboolean   use_short_format,
             gboolean   detailed_date)
{
    const gchar *format;

    if (use_short_format && detailed_date)
    {
        if (use_24_hour)
        {
            /* Translators: date and time in 24h format,
             * i.e. "12/31/2023 23:59" */
            /* xgettext:no-c-format */
            format = _("%m/%d/%Y %H:%M");
        }
        else
        {
            /* Translators: date and time in 12h format,
             * i.e. "12/31/2023 11:59 PM" */
            /* xgettext:no-c-format */
            format = _("%m/%d/%Y %I:%M %p");
        }
    }
    else if (use_short_format)
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
        GTimeSpan time_difference = g_date_time_difference (today_midnight, date_midnight);

        /* Show the word "Today" and time if date is on today */
        if (time_difference < G_TIME_SPAN_DAY && time_difference >= 0)
        {
            if (use_24_hour)
            {
                /* Translators: this is the word "Today" followed by
                 * a time in 24h format. i.e. "Today 23:04" */
                /* xgettext:no-c-format */
                format = _("Today %-H:%M");
            }
            else
            {
                /* Translators: this is the word Today followed by
                 * a time in 12h format. i.e. "Today 9:04 PM" */
                /* xgettext:no-c-format */
                format = _("Today %-I:%M %p");
            }
        }
        /* Show the word "Yesterday" and time if date is on yesterday */
        else if (time_difference < 2 * G_TIME_SPAN_DAY && time_difference >= 0)
        {
            if (use_24_hour)
            {
                /* Translators: this is the word Yesterday followed by
                 * a time in 24h format. i.e. "Yesterday 23:04" */
                /* xgettext:no-c-format */
                format = _("Yesterday %-H:%M");
            }
            else
            {
                /* Translators: this is the word Yesterday followed by
                 * a time in 12h format. i.e. "Yesterday 9:04 PM" */
                /* xgettext:no-c-format */
                format = _("Yesterday %-I:%M %p");
            }
        }
        else
        {
            /* Translators: this is the day of the month followed by the abbreviated
             * month name followed by the year i.e. "3 Feb 2015" */
            /* xgettext:no-c-format */
            format = _("%-e %b %Y");
        }
    }
    else
    {
        if (use_24_hour)
        {
            /* Translators: this is the day number followed by the full month
             * name followed by the year followed by a time in 24h format
             * with seconds i.e. "3 February 2015 23:04:00" */
            /* xgettext:no-c-format */
            format = _("%-e %B %Y %H:%M:%S");
        }
        else
        {
            /* Translators: this is the day number followed by the full month
             * name followed by the year followed by a time in 12h format
             * with seconds i.e. "3 February 2015 09:04:00 PM" */
            /* xgettext:no-c-format */
            format = _("%-e %B %Y %I:%M:%S %p");
        }
    }

    g_autofree gchar *formatted = g_date_time_format (timestamp, format);

    return g_steal_pointer (&formatted);
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
