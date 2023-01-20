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
#include "nautilus-global-preferences.h"
#include "nautilus-special-location-bar.h"
#include "nautilus-enum-types.h"

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
    GVariant *parameters = g_variant_new_parsed (
        "('launch-panel', [<('usage', @av [])>], @a{sv} {})");

    nautilus_dbus_launcher_call (nautilus_dbus_launcher_get (),
                                 NAUTILUS_DBUS_LAUNCHER_SETTINGS,
                                 "Activate",
                                 parameters, window);
}

static gchar *
parse_old_files_age_preferences_value (void)
{
    guint old_files_age = g_settings_get_uint (gnome_privacy_preferences, "old-files-age");

    switch (old_files_age)
    {
        case 0:
        {
            return g_strdup (_("Items in Trash older than 1 hour are automatically deleted"));
        }

        default:
        {
            return g_strdup_printf (ngettext ("Items in Trash older than %d day are automatically deleted",
                                              "Items in Trash older than %d days are automatically deleted",
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
load_special_location (AdwBanner               *banner,
                       NautilusSpecialLocation  location)
{
    const char *button_label = NULL;
    GCallback callback = NULL;

    switch (location)
    {
        case NAUTILUS_SPECIAL_LOCATION_TEMPLATES:
        {
            adw_banner_set_title (banner, _("Put files in this folder to use them as templates for new documents."));
            button_label = _("_Learn More");
            callback = G_CALLBACK (on_template_clicked);
        }
        break;

        case NAUTILUS_SPECIAL_LOCATION_SCRIPTS:
        {
            adw_banner_set_title (banner, _("Executable files in this folder will appear in the Scripts menu."));
        }
        break;

        case NAUTILUS_SPECIAL_LOCATION_SHARING:
        {
            adw_banner_set_title (banner, _("Turn on File Sharing to share the contents of this folder over the network."));
            button_label = _("Sharing Settings");
            callback = G_CALLBACK (on_sharing_clicked);
        }
        break;

        case NAUTILUS_SPECIAL_LOCATION_TRASH:
        {
            set_auto_emptied_message (banner);
            button_label = _("_Settings");
            callback = G_CALLBACK (on_trash_auto_emptied_clicked);

            g_signal_connect_object (gnome_privacy_preferences,
                                     "changed::old-files-age",
                                     G_CALLBACK (set_auto_emptied_message),
                                     banner, G_CONNECT_SWAPPED);
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
    }

    adw_banner_set_button_label (banner, button_label);
    adw_banner_set_revealed (banner, TRUE);

    if (callback != NULL)
    {
        g_signal_connect (banner, "button-clicked", callback, NULL);
    }
}

GtkWidget *
nautilus_special_location_bar_new (NautilusSpecialLocation location)
{
    GtkWidget *banner = adw_banner_new ("");

    load_special_location (ADW_BANNER (banner), location);

    return banner;
}
