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

typedef struct NautilusUndoableClass NautilusUndoableClass;
typedef struct NautilusUndoManager NautilusUndoManager;
typedef struct NautilusUndoable NautilusUndoable;
typedef struct NautilusUndoTransaction NautilusUndoTransaction;

struct NautilusUndoable {	
	GtkObject parent;
	NautilusUndoManager *undo_manager;
	GtkObject *undo_target_class;
	GData *undo_data;
};

struct NautilusUndoableClass {
	GtkObjectClass parent_class;
	
	void (* save_undo_snapshot) (GtkObject *object, gpointer data);
	void (* restore_from_undo_snapshot) (GtkObject *object, gpointer data);
};

/* GtkObject */
GtkType	nautilus_undoable_get_type 				(void);

GtkObject 	*nautilus_undoable_new 				(void);
void		nautilus_undoable_save_undo_snapshot		(NautilusUndoTransaction *transaction, GtkObject *target, 
								 GtkSignalFunc save_func, GtkSignalFunc restore_func);
void		nautilus_undoable_restore_from_undo_snapshot	(NautilusUndoable *undoable);

#endif
