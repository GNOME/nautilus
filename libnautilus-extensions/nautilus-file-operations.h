/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-operations: execute file operations.

   Copyright (C) 1999, 2000 Free Software Foundation
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
   
   Authors: Ettore Perazzoli <ettore@gnu.org>,
            Pavel Cisler <pavel@eazel.com>
*/

#ifndef NAUTILUS_FILE_OPERATIONS_H
#define NAUTILUS_FILE_OPERATIONS_H

#include <gtk/gtkwidget.h>
#include <libgnomevfs/gnome-vfs-types.h>

typedef void (* NautilusCopyCallback)      (GHashTable *debuting_uris,
					    gpointer    callback_data);
typedef void (* NautilusNewFolderCallback) (const char *new_folder_uri,
					    gpointer    callback_data);

/* FIXME: int copy_action should be an enum */

void  nautilus_file_operations_copy_move               (const GList               *item_uris,
							GArray            	  *target_item_points,
							const char                *target_dir_uri,
							GdkDragAction              copy_action,
							GtkWidget                 *parent_view,
							NautilusCopyCallback       done_callback,
							gpointer                   done_callback_data);
void  nautilus_file_operations_empty_trash             (GtkWidget                 *parent_view);
void  nautilus_file_operations_new_folder              (GtkWidget                 *parent_view,
							const char                *parent_dir_uri,
							NautilusNewFolderCallback  done_callback,
							gpointer                   done_callback_data);
void  nautilus_file_operations_delete                  (const GList               *item_uris,
							GtkWidget                 *parent_view);

#endif /* NAUTILUS_FILE_OPERATIONS_H */
