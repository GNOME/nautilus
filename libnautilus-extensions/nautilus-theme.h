/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-theme.h: theme framework with xml-based theme definition files
 
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
  
   Authors: Andy Hertzfeld <andy@eazel.com>
*/

#ifndef NAUTILUS_THEME_H
#define NAUTILUS_THEME_H


#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnomevfs/gnome-vfs-types.h>

/* A callback which can be invoked for each available theme. */
typedef void (*NautilusThemeCallback) (const char *name,
				       const char *path,
				       const char *display_name,
				       const char *description,
				       GdkPixbuf *preview_pixbuf,
				       gboolean builtin,
				       gpointer callback_data);

/* The result of a theme install operation. */
typedef enum
{
	/* Theme installed OK */
	NAUTILUS_THEME_INSTALL_OK,

	/* The given path is not a vaild theme directory */
	NAUTILUS_THEME_INSTALL_NOT_A_THEME_DIRECTORY,

	/* Failed to create the user themes directory ~/.nautilus/themes */
	NAUTILUS_THEME_INSTALL_FAILED_USER_THEMES_DIRECTORY_CREATION,

	/* Failed to install the theme */
	NAUTILUS_THEME_INSTALL_FAILED
} NautilusThemeInstallResult;

/* get and set the current theme */
char                      *nautilus_theme_get_theme                 (void);
void                       nautilus_theme_set_theme                 (const char            *new_theme);


/* fetch data from the current theme */
char                      *nautilus_theme_get_theme_data            (const char            *resource_name,
								     const char            *property_name);

/* fetch data from the specified theme */
char                      *nautilus_theme_get_theme_data_from_theme (const char            *resource_name,
								     const char            *property_name,
								     const char            *theme_name);

/* given the current theme, get the path name of an image with the passed-in name */
char                      *nautilus_theme_get_image_path            (const char            *image_name);

/* like get_image_path, put use the passed in theme instead of the current one */
char                      *nautilus_theme_get_image_path_from_theme (const char            *image_name,
								     const char            *theme_name);

/* create a pixbuf to represent the theme */
GdkPixbuf                 *nautilus_theme_make_preview_pixbuf       (const char            *theme_name);

/* Return the directory where user themes are stored */
char                      *nautilus_theme_get_user_themes_directory (void);

/* Invoke the given callback for each theme available to Nautilus */
void                       nautilus_theme_for_each_theme            (NautilusThemeCallback  callback,
								     gpointer               callback_data);

/* Remove a user theme from Nautilus. */
GnomeVFSResult             nautilus_theme_remove_user_theme         (const char            *theme_to_remove_name);

/* Install the theme found at the given path (if valid). */
NautilusThemeInstallResult nautilus_theme_install_user_theme        (const char            *theme_to_install_path);

#endif /* NAUTILUS_THEME_H */
