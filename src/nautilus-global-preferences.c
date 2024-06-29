/* nautilus-global-preferences.c - Nautilus specific preference keys and
 *                                  functions.
 *
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  The Gnome Library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The Gnome Library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with the Gnome Library; see the file COPYING.LIB.  If not,
 *  see <http://www.gnu.org/licenses/>.
 *
 *  Authors: Ramiro Estrugo <ramiro@eazel.com>
 */

#include <config.h>
#include "nautilus-global-preferences.h"

#include "nautilus-file-utilities.h"
#include "nautilus-file.h"
#include "src/nautilus-files-view.h"
#include <eel/eel-stock-dialogs.h>
#include <glib/gi18n.h>

GSettings *nautilus_preferences;
GSettings *nautilus_compression_preferences;
GSettings *nautilus_icon_view_preferences;
GSettings *nautilus_list_view_preferences;
GSettings *nautilus_window_state;
GSettings *gtk_filechooser_preferences;
GSettings *gnome_lockdown_preferences;
GSettings *gnome_interface_preferences;
GSettings *gnome_privacy_preferences;
GSettings *localsearch_preferences;

void
nautilus_global_preferences_init (void)
{
    static gboolean initialized = FALSE;

    if (initialized)
    {
        return;
    }

    initialized = TRUE;

    nautilus_preferences = g_settings_new ("org.gnome.nautilus.preferences");
    nautilus_compression_preferences = g_settings_new ("org.gnome.nautilus.compression");
    nautilus_window_state = g_settings_new ("org.gnome.nautilus.window-state");
    nautilus_icon_view_preferences = g_settings_new ("org.gnome.nautilus.icon-view");
    nautilus_list_view_preferences = g_settings_new ("org.gnome.nautilus.list-view");
    /* Some settings such as show hidden files are shared between Nautilus and GTK file chooser */
    gtk_filechooser_preferences = g_settings_new_with_path ("org.gtk.gtk4.Settings.FileChooser",
                                                            "/org/gtk/gtk4/settings/file-chooser/");
    gnome_lockdown_preferences = g_settings_new ("org.gnome.desktop.lockdown");
    gnome_interface_preferences = g_settings_new ("org.gnome.desktop.interface");
    gnome_privacy_preferences = g_settings_new ("org.gnome.desktop.privacy");
    localsearch_preferences = g_settings_new ("org.freedesktop.Tracker3.Miner.Files");
}
