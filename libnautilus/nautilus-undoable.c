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

#include <config.h>
#include "nautilus-undoable.h"

#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <glib.h>

#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include "nautilus-undo-manager.h"

enum {
	SAVE_UNDO_SNAPSHOT,
	RESTORE_FROM_UNDO_SNAPSHOT,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];


/* GtkObject */
static void     nautilus_undoable_initialize_class (NautilusUndoableClass  *class);
static void     nautilus_undoable_initialize       (NautilusUndoable       *item);
static void	destroy 			   (GtkObject *object);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusUndoable, nautilus_undoable, GTK_TYPE_OBJECT)


/* Class initialization function for the NautilusUndoable item. */
static void
nautilus_undoable_initialize_class (NautilusUndoableClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);

	object_class->destroy = destroy;

	signals[SAVE_UNDO_SNAPSHOT]
		= gtk_signal_new ("save_undo_snapshot",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusUndoableClass,
						     save_undo_snapshot),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_POINTER);

	signals[RESTORE_FROM_UNDO_SNAPSHOT]		
		= gtk_signal_new ("restore_from_undo_snapshot",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusUndoableClass,
						     restore_from_undo_snapshot),
				  gtk_marshal_NONE__POINTER,
				  GTK_TYPE_NONE, 1,
				  GTK_TYPE_POINTER);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

/* Object initialization function for the NautilusUndoable. */
static void
nautilus_undoable_initialize (NautilusUndoable *undoable)
{
	g_datalist_init(&undoable->undo_data);
}


GtkObject *
nautilus_undoable_new (void)
{
	GtkObject *new;

	new = gtk_type_new (nautilus_undoable_get_type ());

	return new;
}



/* destroy */
static void
destroy (GtkObject *object)
{
	NautilusUndoable *undoable;
	
	g_return_if_fail (NAUTILUS_IS_UNDOABLE (object));

	undoable = NAUTILUS_UNDOABLE(object);
	
	g_datalist_clear(&undoable->undo_data);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


/* nautilus_undoable_save_undo_snapshot */
void 
nautilus_undoable_save_undo_snapshot (NautilusUndoTransaction *transaction, GtkObject *target, 
				      GtkSignalFunc save_func, GtkSignalFunc restore_func)
{
	gboolean result;
	NautilusUndoable *undoable;
	
	/* Init Undoable */
	undoable = NAUTILUS_UNDOABLE(nautilus_undoable_new());

	/* Set target */
	undoable->undo_target_class = target;
	
	/* Connect signals to target object */
	gtk_signal_connect_while_alive (GTK_OBJECT (undoable), "save_undo_snapshot", save_func, target, target);
	gtk_signal_connect_while_alive (GTK_OBJECT (undoable), "restore_from_undo_snapshot", restore_func, target, target);

	/* Add undoable to current transaction */
	result = nautilus_undo_transaction_add_undoable (transaction, undoable);

	/* Fire SAVE_UNDO_SNAPSHOT signal */
	gtk_signal_emit (GTK_OBJECT (undoable),
		 signals[SAVE_UNDO_SNAPSHOT],
		 undoable);
}


/* nautilus_undoable_restore_from_undo_snapshot */
void 
nautilus_undoable_restore_from_undo_snapshot (NautilusUndoable *undoable)
{
	gtk_signal_emit (GTK_OBJECT (undoable), 
		 signals[RESTORE_FROM_UNDO_SNAPSHOT],
		 undoable);
}
