/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-directory-notify.h: Nautilus directory notify calls.
 
   Copyright (C) 2000, 2001 Eazel, Inc.
  
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

#include <glib.h>
#include <gdk/gdk.h>
#include <libnautilus-private/nautilus-file.h>

typedef struct {
	char *from_uri;
	char *to_uri;
} URIPair;

typedef struct {
	char *uri;
	gboolean set;
	GdkPoint point;
} NautilusFileChangesQueuePosition;

/* Almost-public change notification calls */
void nautilus_directory_notify_files_added       (GList        *uris);
void nautilus_directory_notify_files_changed     (GList        *uris);
void nautilus_directory_notify_files_moved       (GList        *uri_pairs);
void nautilus_directory_notify_files_removed     (GList        *uris);
void nautilus_directory_schedule_metadata_copy   (GList        *uri_pairs);
void nautilus_directory_schedule_metadata_move   (GList        *uri_pairs);
void nautilus_directory_schedule_metadata_remove (GList        *uris);
void nautilus_directory_schedule_position_set    (GList        *position_setting_list);

/* Change notification hack.
 * This is called when code modifies the file and it needs to trigger
 * a notification. Eventually this should become private, but for now
 * it needs to be used for code like the thumbnail generation.
 */
void nautilus_file_changed                       (NautilusFile *file);
