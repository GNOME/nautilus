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

/* get the current theme */
char                      *nautilus_theme_get_theme                 (void);


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
/* Return the directory where user themes are stored */
char                      *nautilus_theme_get_user_themes_directory (void);

#endif /* NAUTILUS_THEME_H */
