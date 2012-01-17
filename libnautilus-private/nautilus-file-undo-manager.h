/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-undo-manager.h - Manages the undo/redo stack
 *
 * Copyright (C) 2007-2011 Amos Brocco
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
 * Author: Amos Brocco <amos.brocco@gmail.com>
 */

#ifndef __NAUTILUS_FILE_UNDO_MANAGER_H__
#define __NAUTILUS_FILE_UNDO_MANAGER_H__

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <gio/gio.h>

#include <libnautilus-private/nautilus-file-undo-types.h>

typedef struct _NautilusFileUndoManagerClass NautilusFileUndoManagerClass;
typedef struct _NautilusFileUndoManagerPrivate NautilusFileUndoManagerPrivate;

#define NAUTILUS_TYPE_FILE_UNDO_MANAGER\
	(nautilus_file_undo_manager_get_type())
#define NAUTILUS_FILE_UNDO_MANAGER(object)\
	(G_TYPE_CHECK_INSTANCE_CAST((object), NAUTILUS_TYPE_FILE_UNDO_MANAGER,\
				    NautilusFileUndoManager))
#define NAUTILUS_FILE_UNDO_MANAGER_CLASS(klass)\
	(G_TYPE_CHECK_CLASS_CAST((klass), NAUTILUS_TYPE_FILE_UNDO_MANAGER,\
				 NautilusFileUndoManagerClass))
#define NAUTILUS_IS_FILE_UNDO_MANAGER(object)\
	(G_TYPE_CHECK_INSTANCE_TYPE((object), NAUTILUS_TYPE_FILE_UNDO_MANAGER))
#define NAUTILUS_IS_FILE_UNDO_MANAGER_CLASS(klass)\
	(G_TYPE_CHECK_CLASS_TYPE((klass), NAUTILUS_TYPE_FILE_UNDO_MANAGER))
#define NAUTILUS_FILE_UNDO_MANAGER_GET_CLASS(object)\
	(G_TYPE_INSTANCE_GET_CLASS((object), NAUTILUS_TYPE_FILE_UNDO_MANAGER,\
				   NautilusFileUndoManagerClass))

struct _NautilusFileUndoManager {
	GObject parent_instance;

	/* < private > */
	NautilusFileUndoManagerPrivate* priv;
};

struct _NautilusFileUndoManagerClass {
	GObjectClass parent_class;
};


GType nautilus_file_undo_manager_get_type (void) G_GNUC_CONST;

NautilusFileUndoManager * nautilus_file_undo_manager_get (void);

void nautilus_file_undo_manager_add_action (NautilusFileUndoManager *manager,
					     NautilusFileUndoData *action);
void nautilus_file_undo_manager_undo (NautilusFileUndoManager *manager,
				       NautilusFileUndoFinishCallback callback,
				       gpointer user_data);
void nautilus_file_undo_manager_redo (NautilusFileUndoManager *manager, 
				       NautilusFileUndoFinishCallback callback,
				       gpointer user_data);

gboolean nautilus_file_undo_manager_is_undo_redo (NautilusFileUndoManager *manager);
void nautilus_file_undo_manager_trash_has_emptied (NautilusFileUndoManager *manager);
void nautilus_file_undo_manager_request_menu_update (NautilusFileUndoManager        *manager);
guint64 nautilus_file_undo_manager_get_file_modification_time (GFile *file);

NautilusFileUndoMenuData * nautilus_file_undo_manager_get_menu_data (NautilusFileUndoManager *self);
void nautilus_file_undo_menu_data_free (NautilusFileUndoMenuData *data);

#endif /* __NAUTILUS_FILE_UNDO_MANAGER_H__ */
