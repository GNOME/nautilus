/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-undo-types.h - Data structures used by undo/redo
 *
 * Copyright (C) 2007-2011 Amos Brocco
 * Copyright (C) 2010 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Amos Brocco <amos.brocco@gmail.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#ifndef __NAUTILUS_FILE_UNDO_TYPES_H__
#define __NAUTILUS_FILE_UNDO_TYPES_H__

#include <glib.h>
#include <gio/gio.h>

typedef struct _NautilusFileUndoManager NautilusFileUndoManager;
typedef void (* NautilusFileUndoFinishCallback) (gpointer data);

typedef enum {
	NAUTILUS_FILE_UNDO_COPY,
	NAUTILUS_FILE_UNDO_DUPLICATE,
	NAUTILUS_FILE_UNDO_MOVE,
	NAUTILUS_FILE_UNDO_RENAME,
	NAUTILUS_FILE_UNDO_CREATE_EMPTY_FILE,
	NAUTILUS_FILE_UNDO_CREATE_FILE_FROM_TEMPLATE,
	NAUTILUS_FILE_UNDO_CREATE_FOLDER,
	NAUTILUS_FILE_UNDO_MOVE_TO_TRASH,
	NAUTILUS_FILE_UNDO_RESTORE_FROM_TRASH,
	NAUTILUS_FILE_UNDO_CREATE_LINK,
	NAUTILUS_FILE_UNDO_RECURSIVE_SET_PERMISSIONS,
	NAUTILUS_FILE_UNDO_SET_PERMISSIONS,
	NAUTILUS_FILE_UNDO_CHANGE_GROUP,
	NAUTILUS_FILE_UNDO_CHANGE_OWNER,
	NAUTILUS_FILE_UNDO_NUM_TYPES,
} NautilusFileUndoDataType;

typedef struct {
	const char *undo_label;
	const char *undo_description;
	const char *redo_label;
	const char *redo_description;
} NautilusFileUndoMenuData;

typedef struct _NautilusFileUndoData NautilusFileUndoData;

struct _NautilusFileUndoData
{
	NautilusFileUndoDataType type;

	void (* undo_func) (NautilusFileUndoData *data);
	void (* redo_func) (NautilusFileUndoData *data);

	NautilusFileUndoFinishCallback callback;
	gpointer callback_user_data;

	NautilusFileUndoManager *manager;
	guint is_valid : 1;
	guint locked : 1;	/* True if the action is being undone/redone */
	guint freed : 1;	/* True if the action must be freed after undo/redo */
	guint count;		/* Number of items */

	void    (* strings_func) (NautilusFileUndoData *data,
				  guint count,
				  gchar **labels,
				  gchar **descriptions);

	gchar *undo_label;
	gchar *redo_label;
	gchar *undo_description;
	gchar *redo_description;
	
	void (* finalize_func) (NautilusFileUndoData *data);
};

typedef struct 
{
	NautilusFileUndoData base_data;
	/* Copy / Move / Duplicate / Link / Restore from trash stuff */
	GFile *src_dir;
	GFile *dest_dir;
	GList *sources;	     /* Relative to src_dir */
	GList *destinations; /* Relative to dest_dir */
} NautilusFileUndoDataExt;

typedef struct
{
	NautilusFileUndoData base_data;
	/* Create new file/folder stuff/set permissions */
	char *template;
	GFile *target_file;
} NautilusFileUndoDataCreate;

typedef struct
{
	NautilusFileUndoData base_data;
	/* Rename stuff */
	GFile *old_file;
	GFile *new_file;
} NautilusFileUndoDataRename;

typedef struct
{
	NautilusFileUndoData base_data;
	/* Trash stuff */
	GHashTable *trashed;
} NautilusFileUndoDataTrash;

typedef struct
{
	NautilusFileUndoData base_data;
	/* Recursive change permissions stuff */
	GFile *dest_dir;
	GHashTable *original_permissions;
	guint32 dir_mask;
	guint32 dir_permissions;
	guint32 file_mask;
	guint32 file_permissions;
} NautilusFileUndoDataRecursivePermissions;

typedef struct
{
	NautilusFileUndoData base_data;
	/* Single file change permissions stuff */
	GFile *target_file;
	guint32 current_permissions;
	guint32 new_permissions;
} NautilusFileUndoDataPermissions;

typedef struct
{
	NautilusFileUndoData base_data;
	/* Group and Owner change stuff */
	GFile *target_file;
	char *original_ownership;
	char *new_ownership;
} NautilusFileUndoDataOwnership;

struct _NautilusFileUndoManagerPrivate
{
	GQueue *stack;

	/* Used to protect access to stack (because of async file ops) */
	GMutex mutex;

	guint undo_levels;
	guint index;
	guint undo_redo_flag : 1;
};

#endif /* __NAUTILUS_FILE_UNDO_TYPES_H__ */
