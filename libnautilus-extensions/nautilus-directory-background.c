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
#include "nautilus-metadata.h"

static void background_changed_callback (NautilusBackground *background,
                                         NautilusDirectory  *directory);
static void directory_changed_callback  (NautilusDirectory  *directory,
                                         NautilusBackground *background);
static void background_reset_callback   (NautilusBackground *background,
                                         NautilusDirectory  *directory);

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
        
        /* Block the other handler while we are writing metadata so it doesn't
         * try to change the background.
         */
        gtk_signal_handler_block_by_func (GTK_OBJECT (directory),
                                          directory_changed_callback,
                                          background);

        /* Update metadata based on color. */
	color = nautilus_background_get_color (background);
	nautilus_directory_set_metadata (directory,
					 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_COLOR,
					 NULL,
					 color);
	g_free (color);

        /* Update metadata based on tile image. */
	image = nautilus_background_get_tile_image_uri (background);
	nautilus_directory_set_metadata (directory,
					 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_IMAGE,
					 NULL,
					 image);
	g_free (image);

        /* Unblock the handler. */
        gtk_signal_handler_unblock_by_func (GTK_OBJECT (directory),
                                            directory_changed_callback,
                                            background);
}

/* handle the directory changed signal */
static void
directory_changed_callback (NautilusDirectory *directory,
                            NautilusBackground *background)
{
        char *color, *image;

        g_assert (NAUTILUS_IS_DIRECTORY (directory));
        g_assert (NAUTILUS_IS_BACKGROUND (background));
        g_assert (gtk_object_get_data (GTK_OBJECT (background), "nautilus_background_directory")
                  == directory);

        /* Block the other handler while we are responding to changes
         * in the metadata so it doesn't try to change the metadata.
         */
        gtk_signal_handler_block_by_func (GTK_OBJECT (background),
                                          background_changed_callback,
                                          directory);
        
        /* Update color based on metadata. */
	color = nautilus_directory_get_metadata (directory,
                                                 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_COLOR,
                                                 NULL);
	nautilus_background_set_color (background, color);
	g_free (color);
        
        /* Update tile image based on metadata. */
	image = nautilus_directory_get_metadata (directory,
                                                 NAUTILUS_METADATA_KEY_DIRECTORY_BACKGROUND_IMAGE,
                                                 NULL);
	nautilus_background_set_tile_image_uri (background, image);
	g_free (image);

        /* Unblock the handler. */
        gtk_signal_handler_unblock_by_func (GTK_OBJECT (background),
                                            background_changed_callback,
                                            directory);
}

/* handle the background reset signal.  Eventually, fetch the defaults from the theme,
   but for now, just use NULL */
static void
background_reset_callback (NautilusBackground *background,
                           NautilusDirectory *directory)
{
	nautilus_background_set_color (background, NULL);
	nautilus_background_set_tile_image_uri (background, NULL);
	gtk_signal_emit_stop_by_name (GTK_OBJECT (background),
				      "reset");
}

/* handle the background destroyed signal */
static void
background_destroyed_callback (NautilusBackground *background,
                               NautilusDirectory *directory)
{
        gtk_signal_disconnect_by_func (GTK_OBJECT (directory),
                                       GTK_SIGNAL_FUNC (directory_changed_callback),
                                       background);
}

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
        }

        /* Update the background based on the directory metadata. */
        directory_changed_callback (directory, background);
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
