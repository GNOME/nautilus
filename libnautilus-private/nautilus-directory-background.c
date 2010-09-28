/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
   nautilus-directory-background.c: Helper for the background of a widget
                                    that is viewing a particular location.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#include <config.h>
#include "nautilus-directory-background.h"

#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-background.h>
#include "nautilus-global-preferences.h"
#include <gtk/gtk.h>
#include <string.h>

static void initialize_background_from_settings (EelBackground *background);

#define BG_PREFERENCES_DRAW_BACKGROUND    "/desktop/gnome/background/draw_background"
#define BG_PREFERENCES_PRIMARY_COLOR      "/desktop/gnome/background/primary_color"
#define BG_PREFERENCES_SECONDARY_COLOR    "/desktop/gnome/background/secondary_color"
#define BG_PREFERENCES_COLOR_SHADING_TYPE "/desktop/gnome/background/color_shading_type"
#define BG_PREFERENCES_PICTURE_OPTIONS    "/desktop/gnome/background/picture_options"
#define BG_PREFERENCES_PICTURE_OPACITY    "/desktop/gnome/background/picture_opacity"
#define BG_PREFERENCES_PICTURE_FILENAME   "/desktop/gnome/background/picture_filename"

static void
read_color (GConfClient *client, const char *key, GdkColor *color)
{
        gchar *tmp;
        
        tmp = gconf_client_get_string (client, key, NULL);

        if (tmp != NULL) {
                if (!gdk_color_parse (tmp, color))
                        gdk_color_parse ("black", color);
		g_free (tmp);
        }
        else {
                gdk_color_parse ("black", color);
        }
}

static void
nautilus_file_background_read_desktop_settings (char **color,
                                                char **image,
                                                EelBackgroundImagePlacement *placement)
{
        GConfClient *client;
        gboolean enabled;
        GdkColor primary, secondary;
        gchar *tmp, *filename;
	char	*end_color;
	char	*start_color;
	gboolean use_gradient;
	gboolean is_horizontal;

	filename = NULL;

        client = gconf_client_get_default ();

        /* Get the image filename */
        enabled = gconf_client_get_bool (client, BG_PREFERENCES_DRAW_BACKGROUND, NULL);
        if (enabled) {
                tmp = gconf_client_get_string (client, BG_PREFERENCES_PICTURE_FILENAME, NULL);
                if (tmp != NULL) {
                        if (g_utf8_validate (tmp, -1, NULL) && g_file_test (tmp, G_FILE_TEST_EXISTS)) {
                                filename = g_strdup (tmp);
                        }
                        else {
                                filename = g_filename_from_utf8 (tmp, -1, NULL, NULL, NULL);
                        }
                }
                g_free (tmp);

                if (filename != NULL && filename[0] != '\0') {
                        *image = g_filename_to_uri (filename, NULL, NULL);
                }
                else {
                        *image = NULL;
                }
                g_free (filename);
        }
        else {
                *image = NULL;
        }
        
        /* Get the placement */
        tmp = gconf_client_get_string (client, BG_PREFERENCES_PICTURE_OPTIONS, NULL);
        if (tmp != NULL) {
                if (strcmp (tmp, "wallpaper") == 0) {
                        *placement = EEL_BACKGROUND_TILED;
                }
                else if (strcmp (tmp, "centered") == 0) {
                        *placement = EEL_BACKGROUND_CENTERED;
                }
                else if (strcmp (tmp, "scaled") == 0) {
                        *placement = EEL_BACKGROUND_SCALED_ASPECT;
                }
                else if (strcmp (tmp, "stretched") == 0) {
                        *placement = EEL_BACKGROUND_SCALED;
                }
                else if (strcmp (tmp, "zoom") == 0) {
                        *placement = EEL_BACKGROUND_ZOOM;
                }
                else if (strcmp (tmp, "spanned") == 0) {
                        *placement = EEL_BACKGROUND_SPANNED;
                }
                else if (strcmp (tmp, "none") == 0) {
                        g_free (*image);
                        
                        *placement = EEL_BACKGROUND_CENTERED;
                        *image = NULL;
                }
                else {
                        *placement = EEL_BACKGROUND_CENTERED;
                }
        }
        else {
                *placement = EEL_BACKGROUND_CENTERED;
        }
        g_free (tmp);
                
        /* Get the color */
        tmp = gconf_client_get_string (client, BG_PREFERENCES_COLOR_SHADING_TYPE, NULL);
        if (tmp != NULL) {
                if (strcmp (tmp, "solid") == 0) {
                        use_gradient = FALSE;
                        is_horizontal = FALSE;
                }
                else if (strcmp (tmp, "vertical-gradient") == 0) {
                        use_gradient = TRUE;
                        is_horizontal = FALSE;
                }
                else if (strcmp (tmp, "horizontal-gradient") == 0) {
                        use_gradient = TRUE;
                        is_horizontal = TRUE;
                }
                else {
                        use_gradient = FALSE;
                        is_horizontal = FALSE;
                }
        }
        else {
                use_gradient = FALSE;
                is_horizontal = FALSE;
        }
        g_free (tmp);
        
        read_color (client, BG_PREFERENCES_PRIMARY_COLOR, &primary);
        read_color (client, BG_PREFERENCES_SECONDARY_COLOR, &secondary);

	start_color   = eel_gdk_rgb_to_color_spec (eel_gdk_color_to_rgb (&primary));
        end_color     = eel_gdk_rgb_to_color_spec (eel_gdk_color_to_rgb (&secondary));

	if (use_gradient) {
		*color = eel_gradient_new (start_color, end_color, is_horizontal);
	} else {
		*color = g_strdup (start_color);
	}

	g_free (start_color);
	g_free (end_color);
}

static void
nautilus_file_background_write_desktop_default_settings (void)
{
	/* We just unset all the gconf keys so they go back to
	 * defaults
	 */
	GConfClient *client;
	GConfChangeSet *set;

	client = gconf_client_get_default ();
	set = gconf_change_set_new ();

	/* the list of keys here has to be kept in sync with libgnome
	 * schemas, which isn't the most maintainable thing ever.
	 */
 	gconf_change_set_unset (set, "/desktop/gnome/background/picture_options");
	gconf_change_set_unset (set, "/desktop/gnome/background/picture_filename");
	gconf_change_set_unset (set, "/desktop/gnome/background/picture_opacity");
	gconf_change_set_unset (set, "/desktop/gnome/background/primary_color");
	gconf_change_set_unset (set, "/desktop/gnome/background/secondary_color");
	gconf_change_set_unset (set, "/desktop/gnome/background/color_shading_type");

	/* this isn't atomic yet so it'll be a bit inefficient, but
	 * someday it might be atomic.
	 */
 	gconf_client_commit_change_set (client, set, FALSE, NULL);

	gconf_change_set_unref (set);
	
	g_object_unref (G_OBJECT (client));
}

static int
call_settings_changed (EelBackground *background)
{

        initialize_background_from_settings (background);
	g_object_set_data (G_OBJECT (background), "desktop_gconf_notification_timeout", GUINT_TO_POINTER (0));

	return FALSE;
}

static void
desktop_background_destroyed_callback (EelBackground *background, void *georgeWBush)
{
        guint notification_id;
        guint notification_timeout_id;

        notification_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (background), "desktop_gconf_notification"));
        gconf_client_notify_remove (nautilus_gconf_client, notification_id);

        notification_timeout_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (background), "desktop_gconf_notification_timeout"));
        if (notification_timeout_id != 0) {
                g_source_remove (notification_timeout_id);
        }
}

static void
desktop_background_gconf_notify_cb (GConfClient *client, guint notification_id, GConfEntry *entry, gpointer data)
{
        EelBackground *background;
        guint notification_timeout_id;

        background = EEL_BACKGROUND (data);

        notification_timeout_id = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (background), "desktop_gconf_notification_timeout"));

        if (strcmp (entry->key, "/desktop/gnome/background/stamp") == 0) {
                if (notification_timeout_id != 0) {
                        g_source_remove (notification_timeout_id);
                }

                call_settings_changed (background);
        }
        else if (notification_timeout_id == 0) {
                notification_timeout_id = g_timeout_add (300, (GSourceFunc) call_settings_changed, background);

                g_object_set_data (G_OBJECT (background), "desktop_gconf_notification_timeout", GUINT_TO_POINTER (notification_timeout_id));
        }
}

static void
nautilus_file_background_receive_gconf_changes (EelBackground *background)
{
        guint notification_id;

        notification_id = gconf_client_notify_add (nautilus_gconf_client,
                                                   "/desktop/gnome/background", desktop_background_gconf_notify_cb, background,
                                                   NULL, NULL);

        g_object_set_data (G_OBJECT (background), "desktop_gconf_notification", GUINT_TO_POINTER (notification_id));

        g_signal_connect (background, "destroy",
                          G_CALLBACK (desktop_background_destroyed_callback), NULL);
}

/* handle the background changed signal */
static void
background_changed_callback (EelBackground *background,
                             GdkDragAction  action,
                             gpointer _user_data)
{
        eel_background_save_to_gconf (background);
}

static void
initialize_background_from_settings (EelBackground *background)
{
        char *color;
        char *image;
	EelBackgroundImagePlacement placement;

        g_assert (EEL_IS_BACKGROUND (background));

        nautilus_file_background_read_desktop_settings (&color, &image, &placement);

        /* Block the other handler while we are responding to changes
         * in the metadata so it doesn't try to change the metadata.
         */
        g_signal_handlers_block_by_func
                (background,
                 G_CALLBACK (background_changed_callback),
                 NULL);

        eel_background_set_color (background, color);
        eel_background_set_image_uri (background, image);
        eel_background_set_image_placement (background, placement);

	/* Unblock the handler. */
        g_signal_handlers_unblock_by_func
                (background,
                 G_CALLBACK (background_changed_callback),
                 NULL);

	g_free (color);
	g_free (image);
}

/* handle the background reset signal by setting values from the current theme */
static void
background_reset_callback (EelBackground *background,
                           gpointer _user_data)
{
	nautilus_file_background_write_desktop_default_settings ();
	initialize_background_from_settings (background);
}

void
nautilus_connect_desktop_background_to_file_metadata (NautilusIconContainer *icon_container)
{
	EelBackground *background;

	background = eel_get_widget_background (GTK_WIDGET (icon_container));
	eel_background_set_desktop (background, GTK_WIDGET (icon_container), TRUE);

        /* Connect new signal handlers. */
        g_signal_connect_object (background, "settings_changed",
                                 G_CALLBACK (background_changed_callback), NULL, 0);
        g_signal_connect_object (background, "reset",
                                 G_CALLBACK (background_reset_callback), NULL, 0);

        /* Update the background based on the settings */
        initialize_background_from_settings (background);
	nautilus_file_background_receive_gconf_changes (background);
}
