/*
 * Copyright (C) 2012 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <adwaita.h>
#include <string.h>

#include "nautilus-dbus-launcher.h"
#include "nautilus-file-operations.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-location-banner.h"
#include "nautilus-enum-types.h"
#include "nautilus-scheme.h"
#include "nautilus-trash-monitor.h"

#define FILE_SHARING_SCHEMA_ID "org.gnome.desktop.file-sharing"

typedef enum
{
    NAUTILUS_LOCATION_BANNER_NONE,
    NAUTILUS_LOCATION_BANNER_SCRIPTS,
    NAUTILUS_LOCATION_BANNER_SHARING,
    NAUTILUS_LOCATION_BANNER_TEMPLATES,
    NAUTILUS_LOCATION_BANNER_TRASH,
    NAUTILUS_LOCATION_BANNER_TRASH_AUTO_EMPTIED,
} NautilusLocationBannerMode;

static void set_mode (AdwBanner                 *banner,
                      NautilusLocationBannerMode mode);

static void
on_sharing_clicked (AdwBanner *banner)
{
    GtkWindow *window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (banner)));
    GVariant *parameters = g_variant_new_parsed (
        "('launch-panel', [<('sharing', @av [])>], @a{sv} {})");

    nautilus_dbus_launcher_call (nautilus_dbus_launcher_get (),
                                 NAUTILUS_DBUS_LAUNCHER_SETTINGS,
                                 "Activate",
                                 parameters, window);
}

static void
on_template_clicked (AdwBanner *banner)
{
    GtkWindow *window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (banner)));
    g_autoptr (GtkUriLauncher) launcher = gtk_uri_launcher_new ("help:gnome-help/files-templates");
    gtk_uri_launcher_launch (launcher, window, NULL, NULL, NULL);
}

static void
on_trash_auto_emptied_clicked (AdwBanner *banner)
{
    GtkWindow *window = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (banner)));
    const gchar *parameters = "('launch-panel', [<('privacy', [<'usage'>])>], @a{sv} {})";

    nautilus_dbus_launcher_call (nautilus_dbus_launcher_get (),
                                 NAUTILUS_DBUS_LAUNCHER_SETTINGS,
                                 "Activate",
                                 g_variant_new_parsed (parameters),
                                 window);
}

static void
on_trash_clear_clicked (AdwBanner *banner)
{
    GtkWidget *window = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (banner)));

    nautilus_file_operations_empty_trash (window, TRUE, NULL);
}

static gchar *
parse_old_files_age_preferences_value (void)
{
    guint old_files_age = g_settings_get_uint (gnome_privacy_preferences, "old-files-age");

    switch (old_files_age)
    {
        case 0:
        {
            return g_strdup (_("Files in the trash are permanently deleted after 1 hour"));
        }

        default:
        {
            return g_strdup_printf (ngettext ("Files in the trash are permanently deleted after %d day",
                                              "Files in the trash are permanently deleted after %d days",
                                              old_files_age),
                                    old_files_age);
        }
    }
}

static void
set_auto_emptied_message (AdwBanner *banner)
{
    g_autofree gchar *message = parse_old_files_age_preferences_value ();

    adw_banner_set_title (banner, message);
}

static void
on_remove_old_trash_files_changed (GSettings *settings,
                                   gchar     *key,
                                   gpointer   callback_data)
{
    AdwBanner *banner = ADW_BANNER (callback_data);
    gboolean auto_emptied = g_settings_get_boolean (settings, key);

    set_mode (banner, auto_emptied ? NAUTILUS_LOCATION_BANNER_TRASH_AUTO_EMPTIED :
                                     NAUTILUS_LOCATION_BANNER_TRASH);
}

static gboolean
is_scripts_location (GFile *location)
{
    g_autofree char *scripts_path = nautilus_get_scripts_directory_path ();
    g_autoptr (GFile) scripts_file = g_file_new_for_path (scripts_path);

    return g_file_equal (location, scripts_file);
}

static NautilusLocationBannerMode
get_mode_for_location (GFile *location)
{
    g_autoptr (NautilusFile) file = NULL;

    if (location == NULL)
    {
        return NAUTILUS_LOCATION_BANNER_NONE;
    }

    file = nautilus_file_get (location);

    if (nautilus_should_use_templates_directory () &&
        nautilus_file_is_user_special_directory (file, G_USER_DIRECTORY_TEMPLATES))
    {
        return NAUTILUS_LOCATION_BANNER_TEMPLATES;
    }
    else if (is_scripts_location (location))
    {
        return NAUTILUS_LOCATION_BANNER_SCRIPTS;
    }
    else if (check_schema_available (FILE_SHARING_SCHEMA_ID) && nautilus_file_is_public_share_folder (file))
    {
        return NAUTILUS_LOCATION_BANNER_SHARING;
    }
    else if (nautilus_file_is_in_trash (file))
    {
        gboolean auto_emptied = g_settings_get_boolean (gnome_privacy_preferences, "remove-old-trash-files");
        return (auto_emptied ?
                NAUTILUS_LOCATION_BANNER_TRASH_AUTO_EMPTIED :
                NAUTILUS_LOCATION_BANNER_TRASH);
    }
    else
    {
        return NAUTILUS_LOCATION_BANNER_NONE;
    }
}

static void
update_trash_banner_visibility (AdwBanner *banner)
{
    adw_banner_set_revealed (banner, !nautilus_trash_monitor_is_empty ());
}

static void
set_mode (AdwBanner                  *banner,
          NautilusLocationBannerMode  mode)
{
    const char *button_label = NULL;
    GCallback callback = NULL;
    gboolean banner_is_revealed = TRUE;

    /* Reset signal handlers. */
    g_signal_handlers_disconnect_by_data (banner, &nautilus_location_banner_load);
    g_signal_handlers_disconnect_by_data (gnome_privacy_preferences, banner);
    g_signal_handlers_disconnect_by_data (nautilus_trash_monitor_get (), banner);

    switch (mode)
    {
        case NAUTILUS_LOCATION_BANNER_NONE:
        {
            adw_banner_set_revealed (banner, FALSE);
            return;
        }
        break;

        case NAUTILUS_LOCATION_BANNER_SCRIPTS:
        {
            adw_banner_set_title (banner, _("Executable files in this folder will appear in the Scripts menu"));
        }
        break;

        case NAUTILUS_LOCATION_BANNER_SHARING:
        {
            adw_banner_set_title (banner, _("Turn on File Sharing to share the contents of this folder over the network"));
            button_label = _("Sharing Settings");
            callback = G_CALLBACK (on_sharing_clicked);
        }
        break;

        case NAUTILUS_LOCATION_BANNER_TEMPLATES:
        {
            adw_banner_set_title (banner, _("Put files in this folder to use them as templates for new documents"));
            button_label = _("_Learn More");
            callback = G_CALLBACK (on_template_clicked);
        }
        break;

        case NAUTILUS_LOCATION_BANNER_TRASH:
        {
            adw_banner_set_title (banner, "");
            if (nautilus_trash_monitor_is_empty ())
            {
                banner_is_revealed = FALSE;
            }

            button_label = _("_Empty Trash…");
            callback = G_CALLBACK (on_trash_clear_clicked);

            g_signal_connect_swapped (nautilus_trash_monitor_get (),
                                      "trash-state-changed",
                                      G_CALLBACK (update_trash_banner_visibility),
                                      banner);
            g_signal_connect_object (gnome_privacy_preferences,
                                     "changed::remove-old-trash-files",
                                     G_CALLBACK (on_remove_old_trash_files_changed),
                                     banner, 0);
        }
        break;

        case NAUTILUS_LOCATION_BANNER_TRASH_AUTO_EMPTIED:
        {
            set_auto_emptied_message (banner);
            button_label = _("_Trash Settings");
            callback = G_CALLBACK (on_trash_auto_emptied_clicked);

            g_signal_connect_object (gnome_privacy_preferences,
                                     "changed::old-files-age",
                                     G_CALLBACK (set_auto_emptied_message),
                                     banner, G_CONNECT_SWAPPED);
            g_signal_connect_object (gnome_privacy_preferences,
                                     "changed::remove-old-trash-files",
                                     G_CALLBACK (on_remove_old_trash_files_changed),
                                     banner, 0);
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
    }

    adw_banner_set_button_label (banner, button_label);
    adw_banner_set_revealed (banner, banner_is_revealed);

    if (callback != NULL)
    {
        g_signal_connect (banner, "button-clicked", callback, &nautilus_location_banner_load);
    }
}

void
nautilus_location_banner_load (AdwBanner *banner,
                               GFile     *location)
{
    set_mode (banner, get_mode_for_location (location));
}
