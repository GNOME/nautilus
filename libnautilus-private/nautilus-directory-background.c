/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
   nautilus-directory-background.c: Helper for the background of a widget
                                    that is viewing a particular directory.
 
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
  
   Author: Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "nautilus-directory-background.h"

#include <gtk/gtksignal.h>
#include "nautilus-background.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-metadata.h"
#include "nautilus-string.h"
#include "nautilus-theme.h"
#include "libnautilus-extensions/nautilus-gdk-extensions.h"

static void background_changed_callback (NautilusBackground *background,
                                         NautilusDirectory  *directory);
static void directory_changed_callback  (NautilusDirectory  *directory,
                                         NautilusBackground *background);
static void background_reset_callback   (NautilusBackground *background,
                                         NautilusDirectory  *directory);

void
static nautilus_directory_background_set_desktop (NautilusBackground *background)
{
	gtk_object_set_data (GTK_OBJECT (background), "desktop", (void *) -1); 
}

static gboolean
nautilus_directory_background_is_desktop (NautilusBackground *background)
{
	return gtk_object_get_data (GTK_OBJECT (background), "desktop") != NULL;
}


/* utility routine to handle mapping local image files in themes to a uri */
static char*
theme_image_path_to_uri (char *image_file)
{
	char *image_path;
	char *image_uri;

	if (image_file != NULL && !nautilus_istr_has_prefix (image_file, "file://")) {
		
		if (nautilus_str_has_prefix (image_file, "./")) {
			image_path = nautilus_theme_get_image_path (image_file + 2);
		} else {
			image_path = g_strdup_printf ("%s/%s", NAUTILUS_DATADIR, image_file);
		}
		
		g_assert (g_file_exists (image_path));
		
		image_uri = nautilus_get_uri_from_local_path (image_path);
		g_free (image_path);
	} else {
		image_uri = g_strdup (image_file);
	}

	return image_uri;
}

/* FIXME combine mode (image over gradient) does not work for the GNOME desktop.
 * None of our themes currently specify this for the desktop.
 */
 
static void
nautilus_directory_background_get_default_settings (gboolean is_desktop,
						  char **color, char **image, gboolean *combine)
{
	char *theme_source;
	char *combine_str;
	char *image_local_path;

	theme_source = is_desktop ? "desktop" : "directory";
	
	*color = nautilus_theme_get_theme_data (theme_source, NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_COLOR);

	image_local_path = nautilus_theme_get_theme_data (theme_source, NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_IMAGE);
	*image = theme_image_path_to_uri (image_local_path);
	g_free (image_local_path);

	combine_str = nautilus_theme_get_theme_data (theme_source, "COMBINE");
	*combine = combine_str != NULL;
	g_free (combine_str);	
}

static gboolean
nautilus_gnome_config_string_match_no_case (const char *path, const char *test_value)
{
	char *value;
	gboolean result;
	value = gnome_config_get_string (path);
	result = !nautilus_strcasecmp (value, test_value);
	g_free (value);
	return result;
}

/* This enum is from gnome-source/control-center/capplets/background-properties/render-background.h */
enum {
	WALLPAPER_TILED,
	WALLPAPER_CENTERED,
	WALLPAPER_SCALED,
	WALLPAPER_SCALED_KEEP,
	WALLPAPER_EMBOSSED
};

static void
nautilus_directory_background_read_desktop_settings (char **color, char **image, gboolean *combine)
{
	gboolean use_image;
	char*	 image_local_path;
	int	 image_alignment;
	char*	 default_image_file;
	
	char	*end_color;
	char	*start_color;
	char	*default_color;
	gboolean use_gradient;
	gboolean is_horizontal;
	gboolean no_start_color;
	gboolean no_end_color;

	nautilus_directory_background_get_default_settings (TRUE, &default_color, &default_image_file, combine);
	/* note - value of combine comes from the theme */

	use_image        = !nautilus_gnome_config_string_match_no_case ("/Background/Default/wallpaper=none", "none");
	image_local_path = gnome_config_get_string ("/Background/Default/wallpaper");
	image_alignment  = gnome_config_get_int ("/Background/Default/wallpaperAlign=0");

	end_color     = gnome_config_get_string_with_default ("/Background/Default/color2", &no_end_color);
	start_color   = gnome_config_get_string_with_default ("/Background/Default/color1", &no_start_color);
	use_gradient  = !nautilus_gnome_config_string_match_no_case ("/Background/Default/simple=solid", "solid");
	is_horizontal = !nautilus_gnome_config_string_match_no_case ("/Background/Default/gradient=vertical", "vertical");

	if (use_image) {
		*image = nautilus_get_uri_from_local_path (image_local_path);
		 switch (image_alignment)
		 {
			case WALLPAPER_TILED:
			case WALLPAPER_CENTERED:
			case WALLPAPER_SCALED:
			case WALLPAPER_SCALED_KEEP:
			case WALLPAPER_EMBOSSED:
			/* FIXME need to fix nautilus_background to handle image alignment
			 */
		 }
	} else {
		*image = NULL;
	}

	if (use_gradient) {
		if (no_start_color || no_end_color) {
			*color = g_strdup (default_color);
		} else {
			*color = nautilus_gradient_new (start_color, end_color , is_horizontal);
		}
	} else {
		if (no_start_color) {
			*color = g_strdup (default_color);
		} else {
			*color = g_strdup (start_color);
		}
	}

	g_free(start_color);
	g_free(end_color);
	g_free(default_color);
	
	g_free(image_local_path);
	g_free(default_image_file);	
}

static void
nautilus_directory_background_write_desktop_settings (char *color, char *image, gboolean combine)
{
	char *end_color;
	char *start_color;
	char *image_local_path;

	if (color != NULL) {
		start_color = nautilus_gradient_get_start_color_spec (color);
		gnome_config_set_string ("/Background/Default/color1", start_color);		
		g_free (start_color);

		/* if color is not a gradient, this ends up writing same as start_color */
		end_color = nautilus_gradient_get_end_color_spec (color);
		gnome_config_set_string ("/Background/Default/color2", end_color);		
		g_free (end_color);

		gnome_config_set_string ("/Background/Default/simple", nautilus_gradient_is_gradient (color) ? "gradient" : "solid");
		gnome_config_set_string ("/Background/Default/gradient", nautilus_gradient_is_horizontal (color) ? "vertical" : "horizontal");
	}

	if (image != NULL) {
		image_local_path = nautilus_get_local_path_from_uri (image);
		gnome_config_set_string ("/Background/Default/wallpaper", image_local_path);
		g_free (image_local_path);
		/* FIXME need to fix nautilus_background to handle image alignment
		 * and write out the proper value here.
		 */
		gnome_config_set_int ("/Background/Default/wallpaperAlign", WALLPAPER_TILED);
	} else {
		gnome_config_set_string ("/Background/Default/wallpaper", "none");
	}

	gnome_config_sync ();
	
	/* FIXME
	 * Try to trick GNOME into re-reading the settings.
	 */
}

static void
nautilus_directory_background_write_desktop_default_settings ()
{
	char *color;
	char *image;
	gboolean combine;
	nautilus_directory_background_get_default_settings (TRUE, &color, &image, &combine);
	nautilus_directory_background_write_desktop_settings (color, image, combine);
}	

/* return true if the background is not in the default state */
gboolean
nautilus_directory_background_is_set (NautilusBackground *background)
{
	char *color;
	char *image;
	char *default_color;
	char *default_image;
	
	gboolean is_set;
	gboolean combine;
	gboolean default_combine;

	color = nautilus_background_get_color (background);
	image = nautilus_background_get_tile_image_uri (background);
	default_combine = nautilus_background_get_combine_mode (background);
	nautilus_directory_background_get_default_settings (nautilus_directory_background_is_desktop (background), &default_color, &default_image, &combine);

	is_set = !nautilus_strcmp (color, default_color) ||
		 !nautilus_strcmp (image, default_image) ||
		 combine != default_combine;

	g_free (color);
	g_free (image);
	g_free (default_color);
	g_free (default_image);
	
	return is_set;
}

/* handle the background changed signal */
static void
background_changed_callback (NautilusBackground *background,
                             NautilusDirectory *directory)
{
  	char *color, *image;
        
        g_assert (NAUTILUS_IS_BACKGROUND (background));
        g_assert (NAUTILUS_IS_DIRECTORY (directory));
        g_assert (gtk_object_get_data (GTK_OBJECT (background), "nautilus_background_directory")
                  == directory);
        

	color = nautilus_background_get_color (background);
	image = nautilus_background_get_tile_image_uri (background);

	if (nautilus_directory_background_is_desktop (background)) {
		nautilus_directory_background_write_desktop_settings (color, image, nautilus_background_get_combine_mode (background));
	} else {
	        /* Block the other handler while we are writing metadata so it doesn't
	         * try to change the background.
	         */
	        gtk_signal_handler_block_by_func (GTK_OBJECT (directory),
	                                          directory_changed_callback,
	                                          background);

		nautilus_directory_set_metadata (directory,
						 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_COLOR,
						 NULL,
						 color);

		nautilus_directory_set_metadata (directory,
						 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_IMAGE,
						 NULL,
						 image);
						 
	        /* Unblock the handler. */
	        gtk_signal_handler_unblock_by_func (GTK_OBJECT (directory),
	                                            directory_changed_callback,
	                                            background);
	}

	g_free (color);
	g_free (image);
}

/* handle the directory changed signal */
static void
directory_changed_callback (NautilusDirectory *directory,
                            NautilusBackground *background)
{
        char *color, *image;
	gboolean combine;
	
        g_assert (NAUTILUS_IS_DIRECTORY (directory));
        g_assert (NAUTILUS_IS_BACKGROUND (background));
        g_assert (gtk_object_get_data (GTK_OBJECT (background), "nautilus_background_directory")
                  == directory);

	if (nautilus_directory_background_is_desktop (background)) {
		nautilus_directory_background_read_desktop_settings (&color, &image, &combine);
	} else {
		color = nautilus_directory_get_metadata (directory,
	                                                 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_COLOR,
	                                                 NULL);
		image = nautilus_directory_get_metadata (directory,
	                                                 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_IMAGE,
	                                                 NULL);
		combine = FALSE; /* only from theme, at least for now */

		/* if there's none, read the default from the theme */
		if (color == NULL && image == NULL) {
			nautilus_directory_background_get_default_settings (FALSE, &color, &image, &combine);	
		}
	}

        /* Block the other handler while we are responding to changes
         * in the metadata so it doesn't try to change the metadata.
         */
        gtk_signal_handler_block_by_func (GTK_OBJECT (background),
                                          background_changed_callback,
                                          directory);

	nautilus_background_set_color (background, color);     
	nautilus_background_set_tile_image_uri (background, image);
        nautilus_background_set_combine_mode (background, combine);
	
	/* Unblock the handler. */
	gtk_signal_handler_unblock_by_func (GTK_OBJECT (background),
                                        background_changed_callback,
                                        directory);
	
	g_free (color);
	g_free (image);
}

/* handle the theme changing */
static void
nautilus_directory_background_theme_changed (gpointer user_data)
{
	NautilusDirectory *directory;
	NautilusBackground *background;

	background = NAUTILUS_BACKGROUND (user_data);
	directory = gtk_object_get_data (GTK_OBJECT (background),
					"nautilus_background_directory");
	if (directory) {
		directory_changed_callback (directory, background);
	}
}

/* handle the background reset signal by setting values from the current theme */
static void
background_reset_callback (NautilusBackground *background,
                           NautilusDirectory *directory)
{
	if (nautilus_directory_background_is_desktop (background)) {
		nautilus_directory_background_write_desktop_default_settings ();
	} else {
	        /* Block the other handler while we are writing metadata so it doesn't
	         * try to change the background.
	         */
	        gtk_signal_handler_block_by_func (GTK_OBJECT (directory),
	                                          directory_changed_callback,
	                                          background);

		/* reset the metadata */
		nautilus_directory_set_metadata (directory,
						 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_COLOR,
						 NULL,
						 NULL);

		nautilus_directory_set_metadata (directory,
						 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_IMAGE,
						 NULL,
						 NULL);
	        /* Unblock the handler. */
	        gtk_signal_handler_unblock_by_func (GTK_OBJECT (directory),
	                                            directory_changed_callback,
	                                            background);
	}

	directory_changed_callback (directory, background);

	/* We don't want the default reset handler running.
	 * It will set color and tile_image_uri  to NULL.
	 */
	gtk_signal_emit_stop_by_name (GTK_OBJECT (background), "reset");
}

/* handle the background destroyed signal */
static void
background_destroyed_callback (NautilusBackground *background,
                               NautilusDirectory *directory)
{
        gtk_signal_disconnect_by_func (GTK_OBJECT (directory),
                                       GTK_SIGNAL_FUNC (directory_changed_callback),
                                       background);
        nautilus_directory_file_monitor_remove (directory, background);
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					      nautilus_directory_background_theme_changed,
					      background);
}

/* key routine that hooks up a background and directory */
void
nautilus_connect_background_to_directory_metadata (GtkWidget *widget,
                                                   NautilusDirectory *directory)
{
	NautilusBackground *background;
	gpointer old_directory;

	/* Get at the background object we'll be connecting. */
	background = nautilus_get_widget_background (widget);


	/* Check if it is already connected. */
	old_directory = gtk_object_get_data (GTK_OBJECT (background),
                                             "nautilus_background_directory");
	if (old_directory == directory) {
		return;
	}

	/* Disconnect old signal handlers. */
	if (old_directory != NULL) {
		g_assert (NAUTILUS_IS_DIRECTORY (old_directory));
		gtk_signal_disconnect_by_func (GTK_OBJECT (background),
                                               GTK_SIGNAL_FUNC (background_changed_callback),
                                               old_directory);
		gtk_signal_disconnect_by_func (GTK_OBJECT (background),
                                               GTK_SIGNAL_FUNC (background_destroyed_callback),
                                               old_directory);
		gtk_signal_disconnect_by_func (GTK_OBJECT (background),
                                               GTK_SIGNAL_FUNC (background_reset_callback),
                                               old_directory);
		gtk_signal_disconnect_by_func (GTK_OBJECT (old_directory),
                                               GTK_SIGNAL_FUNC (directory_changed_callback),
                                               background);
		nautilus_directory_file_monitor_remove (old_directory, background);
		nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					      nautilus_directory_background_theme_changed,
					      background);

	}

        /* Attach the new directory. */
        nautilus_directory_ref (directory);
        gtk_object_set_data_full (GTK_OBJECT (background),
                                  "nautilus_background_directory",
                                  directory,
                                  (GtkDestroyNotify) nautilus_directory_unref);

        /* Connect new signal handlers. */
        if (directory != NULL) {
                gtk_signal_connect (GTK_OBJECT (background),
                                    "settings_changed",
                                    GTK_SIGNAL_FUNC (background_changed_callback),
                                    directory);
                gtk_signal_connect (GTK_OBJECT (background),
                                    "destroy",
                                    GTK_SIGNAL_FUNC (background_destroyed_callback),
                                    directory);
                gtk_signal_connect (GTK_OBJECT (background),
				    "reset",
				    GTK_SIGNAL_FUNC (background_reset_callback),
				    directory);
		gtk_signal_connect (GTK_OBJECT (directory),
                                    "metadata_changed",
                                    GTK_SIGNAL_FUNC (directory_changed_callback),
                                    background);
        	
		/* arrange to receive directory metadata */
		nautilus_directory_file_monitor_add (directory,
						     background,
						     NULL, TRUE, FALSE,
						     NULL, NULL);					     
		
		/* arrange for notification when the theme changes */
		nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_THEME, nautilus_directory_background_theme_changed, background);	

	}

        /* Update the background based on the directory metadata. */
        directory_changed_callback (directory, background);
}

void
nautilus_connect_desktop_background_to_directory_metadata (GtkWidget *widget,
                                                   NautilusDirectory *directory)
{
	nautilus_directory_background_set_desktop (nautilus_get_widget_background (widget));

	/* Strictly speaking, we don't need to know about metadata changes, since
	 * the desktop setting aren't stored there. But, hooking up to metadata
	 * changes is actually a small part of what this fn does, and we do need
	 * the other stuff (hooked up to background & theme changes). Being notified
	 * of metadata changes on the directory is a waste, but won't hurt, so I don't
	 * think it's worth refactoring the fn at this point.
	 */
	nautilus_connect_background_to_directory_metadata (widget, directory);
}

void
nautilus_connect_background_to_directory_metadata_by_uri (GtkWidget *widget,
                                                          const char *uri)
{
        NautilusDirectory *directory;
        directory = nautilus_directory_get (uri);
        nautilus_connect_background_to_directory_metadata (widget, directory);
        nautilus_directory_unref (directory);
}
