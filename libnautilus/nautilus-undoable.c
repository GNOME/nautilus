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

/* Register a simple undo action by calling nautilus_undo_register_full. */
void
nautilus_undo_register (GtkObject *target,
			NautilusUndoCallback callback,
			gpointer callback_data,
			GDestroyNotify callback_data_destroy_notify,
			const char *operation_name,
			const char *undo_menu_item_name,
			const char *undo_menu_item_description,
			const char *redo_menu_item_name,
			const char *redo_menu_item_description)
{
	NautilusUndoAtom atom;
	GList single_atom_list;

	g_return_if_fail (GTK_IS_OBJECT (target));
	g_return_if_fail (callback != NULL);

	/* Make an atom. */
	atom.target = target;
	atom.callback = callback;
	atom.callback_data = callback_data;
	atom.callback_data_destroy_notify = callback_data_destroy_notify;

	/* Make a single-atom list. */
	single_atom_list.data = &atom;
	single_atom_list.next = NULL;
	single_atom_list.prev = NULL;

	/* Call the full version of the registration function,
	 * using the undo target as the place to search for the
	 * undo manager.
	 */
	nautilus_undo_register_full (&single_atom_list,
				     target,
				     operation_name,
				     undo_menu_item_name,
				     undo_menu_item_description,
				     redo_menu_item_name,
				     redo_menu_item_description);
}

static void
undo_atom_destroy_callback_data (NautilusUndoAtom *atom)
{
	if (atom->callback_data_destroy_notify != NULL) {
		(* atom->callback_data_destroy_notify) (atom->callback_data);
	}
}

static void
undo_atom_destroy (NautilusUndoAtom *atom)
{
	undo_atom_destroy_callback_data (atom);
	g_free (atom);
}

static void
undo_atom_destroy_notify_cover (gpointer data)
{
	undo_atom_destroy (data);
}

static void
undo_atom_destroy_callback_data_g_func_cover (gpointer data, gpointer callback_data)
{
	undo_atom_destroy_callback_data (data);
}

/* This is a temporary hack to make things work with NautilusUndoable. */
static NautilusUndoAtom *atom_global_hack;

/* The saving has already been done.
 * We just use the save callback as a way to connect the atom to the
 * undoable object that's created.
 */
static void
save_callback (NautilusUndoable *undoable)
{
	gtk_object_set_data_full (GTK_OBJECT (undoable),
				  "Nautilus undo atom",
				  atom_global_hack,
				  undo_atom_destroy_notify_cover);
}

static void
restore_callback (NautilusUndoable *undoable)
{
	NautilusUndoAtom *atom;

	atom = gtk_object_get_data (GTK_OBJECT (undoable),
				    "Nautilus undo atom");
	(* atom->callback) (atom->target, atom->callback_data);
}

/* Register an undo action. */
void
nautilus_undo_register_full (GList *atoms,
			     GtkObject *undo_manager_search_start_object,
			     const char *operation_name,
			     const char *undo_menu_item_name,
			     const char *undo_menu_item_description,
			     const char *redo_menu_item_name,
			     const char *redo_menu_item_description)
{
	NautilusUndoManager *manager;
	NautilusUndoTransaction *transaction;
	NautilusUndoAtom *atom;
	GList *p;

	g_return_if_fail (atoms != NULL);
	g_return_if_fail (GTK_IS_OBJECT (undo_manager_search_start_object));

	/* Note that this is just a hack in terms of the existing stuff.
	 * A lot of things could be simplified and we can probably get rid of
	 * NautilusUndoable entirely (maybe replace it with NautilusUndoAtom).
	 */

	manager = nautilus_get_undo_manager (undo_manager_search_start_object);
	if (manager == NULL) {
		g_list_foreach (atoms, undo_atom_destroy_callback_data_g_func_cover, NULL);
		return;
	}

	transaction = nautilus_undo_manager_begin_transaction
		(manager, operation_name);
	for (p = atoms; p != NULL; p = p->next) {
		atom = p->data;

		atom_global_hack = g_memdup (atom, sizeof (*atom));
		nautilus_undoable_save_undo_snapshot
			(transaction, atom->target,
			 GTK_SIGNAL_FUNC (save_callback),
			 GTK_SIGNAL_FUNC (restore_callback));

		/* Connect a signal handler so this object will unregister
		 * itself when it's destroyed.
		 */
		gtk_signal_connect
			(atom->target, "destroy",
			 GTK_SIGNAL_FUNC (nautilus_undo_unregister), NULL);
	}
	nautilus_undo_manager_end_transaction (manager, transaction);
}

/* Cover for forgetting about all undo relating to a particular target. */
void
nautilus_undo_unregister (GtkObject *target)
{
	g_return_if_fail (GTK_IS_OBJECT (target));

	/* Right now we just call the "real" code over in the undo manager.
	 * FIXME: We will have to figure out which transactions this affects
	 * and remove ourselves from them. It's not clear how to do that.
	 */
	nautilus_undo_manager_unregister_object (target);

	/* Perhaps this should also unregister all children if called on a
	 * GtkContainer? That might be handy.
	 */
}

void
nautilus_undo (GtkObject *undo_manager_search_start_object)
{
	NautilusUndoManager *manager;

	g_return_if_fail (GTK_IS_OBJECT (undo_manager_search_start_object));

	/* FIXME: This has to use the CORBA undo so it can work even
	 * if it's in a different process from the undo manager. That
	 * will be clear anyway, since the undo manager returned by
	 * nautilus_get_undo_manager will have to be a CORBA one.
	 */
	manager = nautilus_get_undo_manager (undo_manager_search_start_object);
	if (manager != NULL) {
		nautilus_undo_manager_undo (manager);
	}
}
