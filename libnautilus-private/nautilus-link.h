/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-link.h: xml-based link files that control their appearance
   and behavior.

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

#ifndef NAUTILUS_LINK_H
#define NAUTILUS_LINK_H

#include <glib.h>
#include "nautilus-file.h"

/* Create a new link file */
gboolean nautilus_link_create 				(const char *directory_path, 
							 const char *name, 
							 const char *image, 
							 const char *uri);

/* given a uri, returns TRUE if it's a link file */
gboolean nautilus_link_is_link_file                	(NautilusFile *file);

gboolean nautilus_link_set_icon 			(const char  *path, 
							 const char  *icon_name);

/* returns additional text to display under the name, NULL if none */
char *   nautilus_link_get_additional_text              (const char *link_file_uri);

/* returns the image associated with a link file */
char *   nautilus_link_get_image_uri                    (const char *link_file_uri);

/* returns the link uri associated with a link file */
char *   nautilus_link_get_link_uri                     (const char *link_file_uri);
char *   nautilus_link_get_link_uri_given_file_contents (const char *link_file_contents,
							 int         link_file_size);

/* strips the suffix from the passed in string if it's a link file */
char *   nautilus_link_get_display_name                 (char       *link_file_name);

#endif /* NAUTILUS_LINK_H */
