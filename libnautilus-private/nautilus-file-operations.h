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

#include <gdk/gdkdnd.h>
#include <gtk/gtkwidget.h>
#include <gio/gio.h>

typedef void (* NautilusCopyCallback)      (GHashTable *debuting_uris,
					    gpointer    callback_data);
typedef void (* NautilusCreateCallback)    (GFile      *new_file,
					    gpointer    callback_data);
typedef void (* NautilusOpCallback)        (gpointer    callback_data);
typedef void (* NautilusDeleteCallback)    (GHashTable *debuting_uris,
					    gboolean    user_cancel,
					    gpointer    callback_data);

/* FIXME: int copy_action should be an enum */

void nautilus_file_operations_copy_move   (const GList               *item_uris,
					   GArray                    *relative_item_points,
					   const char                *target_dir_uri,
					   GdkDragAction              copy_action,
					   GtkWidget                 *parent_view,
					   NautilusCopyCallback       done_callback,
					   gpointer                   done_callback_data);
void nautilus_file_operations_empty_trash (GtkWidget                 *parent_view);
void nautilus_file_operations_new_folder  (GtkWidget                 *parent_view,
					   GdkPoint                  *target_point,
					   const char                *parent_dir_uri,
					   NautilusCreateCallback     done_callback,
					   gpointer                   done_callback_data);
void nautilus_file_operations_new_file    (GtkWidget                 *parent_view,
					   GdkPoint                  *target_point,
					   const char                *parent_dir,
					   const char                *target_filename,
					   const char                *initial_contents,
					   NautilusCreateCallback     done_callback,
					   gpointer                   data);
void nautilus_file_operations_new_file_from_template (GtkWidget               *parent_view,
					   	      GdkPoint                *target_point,
						      const char              *parent_dir,
						      const char              *target_filename,
						      const char              *template_uri,
						      NautilusCreateCallback   done_callback,
						      gpointer                 data);

void nautilus_file_operations_delete          (GList                  *files,
					       GtkWindow              *parent_window,
					       NautilusDeleteCallback  done_callback,
					       gpointer                done_callback_data);
void nautilus_file_operations_trash_or_delete (GList                  *files,
					       GtkWindow              *parent_window,
					       NautilusDeleteCallback  done_callback,
					       gpointer                done_callback_data);

void nautilus_file_set_permissions_recursive (const char                     *directory,
					      guint32                         file_permissions,
					      guint32                         file_mask,
					      guint32                         folder_permissions,
					      guint32                         folder_mask,
					      NautilusOpCallback              callback,
					      gpointer                        callback_data);

void nautilus_file_operations_unmount_mount (GtkWindow                      *parent_window,
					     GMount                         *mount,
					     gboolean                        eject,
					     gboolean                        check_trash);
void nautilus_file_operations_mount_volume  (GtkWindow                      *parent_window,
					     GVolume                        *volume,
					     gboolean                        allow_autorun);
								

void nautilus_file_operations_copy      (GList                *files,
					 GArray               *relative_item_points,
					 GFile                *target_dir,
					 GtkWindow            *parent_window,
					 NautilusCopyCallback  done_callback,
					 gpointer              done_callback_data);
void nautilus_file_operations_move      (GList                *files,
					 GArray               *relative_item_points,
					 GFile                *target_dir,
					 GtkWindow            *parent_window,
					 NautilusCopyCallback  done_callback,
					 gpointer              done_callback_data);
void nautilus_file_operations_duplicate (GList                *files,
					 GArray               *relative_item_points,
					 GtkWindow            *parent_window,
					 NautilusCopyCallback  done_callback,
					 gpointer              done_callback_data);
void nautilus_file_operations_link      (GList                *files,
					 GArray               *relative_item_points,
					 GFile                *target_dir,
					 GtkWindow            *parent_window,
					 NautilusCopyCallback  done_callback,
					 gpointer              done_callback_data);


#endif /* NAUTILUS_FILE_OPERATIONS_H */
