/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-undo-operations.h - Manages undo/redo of file operations
 *
 * Copyright (C) 2007-2011 Amos Brocco
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Amos Brocco <amos.brocco@gmail.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NAUTILUS_FILE_UNDO_OPERATIONS_H__
#define __NAUTILUS_FILE_UNDO_OPERATIONS_H__

#include <glib.h>
#include <libnautilus-private/nautilus-file-undo-types.h>

NautilusFileUndoData *nautilus_file_undo_data_new (NautilusFileUndoDataType type,
                                                                   gint items_count);
void nautilus_file_undo_data_free (NautilusFileUndoData *action);

void nautilus_file_undo_data_set_src_dir (NautilusFileUndoData *data, 
                                                  GFile *src);
void nautilus_file_undo_data_set_dest_dir (NautilusFileUndoData *data, 
						   GFile *dest);

void nautilus_file_undo_data_add_origin_target_pair (NautilusFileUndoData *data, 
							     GFile *origin, 
							     GFile *target);
void nautilus_file_undo_data_set_create_data (NautilusFileUndoData *data, 
						      GFile *target_file,
						      const char *template_uri);
void nautilus_file_undo_data_set_rename_information (NautilusFileUndoData *data, 
							     GFile *old_file, 
							     GFile *new_file);
void nautilus_file_undo_data_add_trashed_file (NautilusFileUndoData *data, 
						       GFile *file, 
						       guint64 mtime);
void nautilus_file_undo_data_add_file_permissions (NautilusFileUndoData *data, 
							   GFile *file, 
							   guint32 permission);
void nautilus_file_undo_data_set_recursive_permissions (NautilusFileUndoData *data, 
								guint32 file_permissions, 
								guint32 file_mask, 
								guint32 dir_permissions, 
								guint32 dir_mask);
void nautilus_file_undo_data_set_recursive_permissions_dest_dir (NautilusFileUndoData *data,
                                GFile *dest);
void nautilus_file_undo_data_set_file_permissions (NautilusFileUndoData *data, 
							   GFile *file,
							   guint32 current_permissions,
							   guint32 new_permissions);
void nautilus_file_undo_data_set_owner_change_information (NautilusFileUndoData *data, 
								   GFile *file,
								   const char *current_user, 
								   const char *new_user);
void nautilus_file_undo_data_set_group_change_information (NautilusFileUndoData *data, 
								   GFile *file,
								   const char *current_group, 
								   const char *new_group);

#endif /* __NAUTILUS_FILE_UNDO_OPERATIONS_H__ */
