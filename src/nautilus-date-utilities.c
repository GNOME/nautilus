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


G_DEFINE_AUTO_CLEANUP_FREE_FUNC (locale_t, freelocale, (locale_t) 0)

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
    const char *time_locale = setlocale (LC_TIME, NULL);
    locale_t current_locale = uselocale ((locale_t) 0);
    g_auto (locale_t) forced_locale = (locale_t) 0;
    const gchar *format;

    /* We are going to pick a translatable string which defines a time format,
     * which is then used to obtain a final string for the current time.
     *
     * The time locale might be different from the language we pick translations
     * from; so, in order to avoid chimeric results (with some particles in one
     * language and other particles in another language), we need to temporarily
     * force translations to be obtained from the language corresponding to the
     * time locale. The current locale settings are saved to be restored later.
     */
    forced_locale = newlocale (LC_MESSAGES_MASK, time_locale, duplocale (current_locale));
    uselocale (forced_locale);

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
        if (time_difference < G_TIME_SPAN_DAY)
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
        else if (time_difference < 2 * G_TIME_SPAN_DAY)
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

    /* Restore locale settings */
    uselocale (current_locale);

    g_autofree gchar *formatted = g_date_time_format (timestamp, format);

    /* Replace ":" with ratio. Replacement is done afterward because g_date_time_format
     * may fail with utf8 chars in some locales */
    GString *time_label = g_string_new_take (g_steal_pointer (&formatted));
    g_string_replace (time_label, ":", "∶", 0);

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
