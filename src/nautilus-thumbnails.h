/*
   nautilus-thumbnails.h: Thumbnail code for icon factory.
 
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
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
  
   Author: Andy Hertzfeld <andy@eazel.com>
*/

#pragma once

#include <gdk-pixbuf/gdk-pixbuf.h>

guint      nautilus_thumbnail_get_max_size          (void);

/* Returns NULL if there's no thumbnail yet. */
void       nautilus_create_thumbnail_async          (const gchar         *uri,
                                                     const gchar         *mime_type,
                                                     time_t               modified_time,
                                                     GCancellable        *cancellable,
                                                     GAsyncReadyCallback  callback,
                                                     gpointer             user_data);
GdkPixbuf *nautilus_create_thumbnail_finish         (GAsyncResult  *res,
                                                     GError       **error);
gboolean   nautilus_can_thumbnail                   (const gchar *uri,
                                                     const gchar *mime_type,
                                                     time_t       modified_time);
gboolean   nautilus_thumbnail_is_mimetype_limited_by_size
						    (const char *mime_type);
char *     nautilus_thumbnail_get_path_for_uri      (const char *uri);

/* Queue handling: */
void       nautilus_thumbnail_remove_from_queue     (const char   *file_uri);
void       nautilus_thumbnail_prioritize            (const char   *file_uri);
