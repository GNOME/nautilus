/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NautilusUndoable - A container for an undoable object .
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: Gene Z. Ragan <gzr@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef NAUTILUS_UNDOABLE_H
#define NAUTILUS_UNDOABLE_H

#include <libgnome/gnome-defs.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>

#define NAUTILUS_TYPE_UNDOABLE \
	(nautilus_undoable_get_type ())
#define NAUTILUS_UNDOABLE(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_UNDOABLE, NautilusUndoable))
#define NAUTILUS_UNDOABLE_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_UNDOABLE, NautilusUndoableClass))
#define NAUTILUS_IS_UNDOABLE(obj) \
        (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_UNDOABLE))
#define NAUTILUS_IS_UNDOABLE_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass),	NAUTILUS_TYPE_UNDOABLE))

typedef struct NautilusUndoable NautilusUndoable;
typedef struct NautilusUndoableClass NautilusUndoableClass;

typedef struct NautilusUndoManager NautilusUndoManager;
typedef struct NautilusUndoTransaction NautilusUndoTransaction;

struct NautilusUndoable {	
	GtkObject parent;
	NautilusUndoManager *undo_manager;
	GtkObject *undo_target_class;
	GData *undo_data;
};

struct NautilusUndoableClass {
	GtkObjectClass parent_class;
	
	void (* save_undo_snapshot) (GtkObject *object,
				     gpointer data);
	void (* restore_from_undo_snapshot) (GtkObject *object,
					     gpointer data);
};

/* GtkObject */
GtkType    nautilus_undoable_get_type                   (void);
GtkObject *nautilus_undoable_new                        (void);

/* Basic operations. */
void       nautilus_undoable_save_undo_snapshot         (NautilusUndoTransaction *transaction,
							 GtkObject               *target,
							 GtkSignalFunc            save_function,
							 GtkSignalFunc            restore_function);
void       nautilus_undoable_restore_from_undo_snapshot (NautilusUndoable        *undoable);

typedef void (* NautilusUndoCallback) (GtkObject *target, gpointer callback_data);

/* Recipe for undo of a bit of work on an object. */
typedef struct {
	GtkObject *target;
	NautilusUndoCallback callback;
	gpointer callback_data;
	GDestroyNotify callback_data_destroy_notify;
} NautilusUndoAtom;

/* Registering something that can be undone. */
void nautilus_undo_register      (GtkObject            *target,
				  NautilusUndoCallback  callback,
				  gpointer              callback_data,
				  GDestroyNotify        callback_data_destroy_notify,
				  const char           *operation_name,
				  const char           *undo_menu_item_name,
				  const char           *undo_menu_item_description,
				  const char           *redo_menu_item_name,
				  const char           *redo_menu_item_description);
void nautilus_undo_register_full (GList                *atoms,
				  GtkObject            *undo_manager_search_start_object,
				  const char           *operation_name,
				  const char           *undo_menu_item_name,
				  const char           *undo_menu_item_description,
				  const char           *redo_menu_item_name,
				  const char           *redo_menu_item_description);
void nautilus_undo_unregister    (GtkObject            *target);

/* Performing an undo explicitly. Only for use by objects "out in the field".
 * The menu bar itself uses a richer API in the undo manager.
 */
void nautilus_undo               (GtkObject            *undo_manager_search_start_object);

#endif
