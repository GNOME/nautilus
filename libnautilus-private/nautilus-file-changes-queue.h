/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.
  
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

   Author: Pavel Cisler <pavel@eazel.com>  
*/

#ifndef NAUTILUS_FILE_CHANGES_QUEUE_H
#define NAUTILUS_FILE_CHANGES_QUEUE_H

#include <gdk/gdktypes.h>

void nautilus_file_changes_queue_file_added               (const char *uri);
void nautilus_file_changes_queue_file_changed             (const char *uri);
void nautilus_file_changes_queue_file_removed             (const char *uri);
void nautilus_file_changes_queue_file_moved               (const char *from_uri,
							   const char *to_uri);
void nautilus_file_changes_queue_schedule_metadata_copy   (const char *from_uri,
							   const char *to_uri);
void nautilus_file_changes_queue_schedule_metadata_move   (const char *from_uri,
							   const char *to_uri);
void nautilus_file_changes_queue_schedule_metadata_remove (const char *uri);
void nautilus_file_changes_queue_schedule_position_set    (const char *uri,
							   GdkPoint    point);
void nautilus_file_changes_queue_schedule_position_remove (const char *uri);

void nautilus_file_changes_consume_changes                (gboolean    consume_all);

#endif /* NAUTILUS_FILE_CHANGES_QUEUE_H */
