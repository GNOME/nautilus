/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-undo.c - public interface for objects that implement
 *                   undoable actions -- works across components
 *
 * Copyright (C) 2000 Eazel, Inc.
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
 *
 * Author: Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "nautilus-undo.h"

#include "nautilus-undo-private.h"
#include "nautilus-undo-transaction.h"
#include <gtk/gtksignal.h>
#include <gtk/gtkwindow.h>
#include <libgnomeui/gnome-canvas.h>

#define NAUTILUS_UNDO_MANAGER_DATA "Nautilus undo manager"

/* Register a simple undo action by calling nautilus_undo_register_full. */
void
nautilus_undo_register (GtkObject *target,
			NautilusUndoCallback callback,
			gpointer callback_data,
			GDestroyNotify callback_data_destroy_notify,
			const char *operation_name,
			const char *undo_menu_item_label,
			const char *undo_menu_item_hint,
			const char *redo_menu_item_label,
			const char *redo_menu_item_hint)
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
				     undo_menu_item_label,
				     undo_menu_item_hint,
				     redo_menu_item_label,
				     redo_menu_item_hint);
}

/* Register an undo action. */
void
nautilus_undo_register_full (GList *atoms,
			     GtkObject *undo_manager_search_start_object,
			     const char *operation_name,
			     const char *undo_menu_item_label,
			     const char *undo_menu_item_hint,
			     const char *redo_menu_item_label,
			     const char *redo_menu_item_hint)
{
	NautilusUndoTransaction *transaction;
	GList *p;

	g_return_if_fail (atoms != NULL);
	g_return_if_fail (GTK_IS_OBJECT (undo_manager_search_start_object));

	/* Create an undo transaction */
	transaction = nautilus_undo_transaction_new (operation_name,
						     undo_menu_item_label,
						     undo_menu_item_hint, 
						     redo_menu_item_label,
						     redo_menu_item_hint);
	for (p = atoms; p != NULL; p = p->next) {
		nautilus_undo_transaction_add_atom (transaction, p->data);
	}
	nautilus_undo_transaction_add_to_undo_manager
		(transaction,
		 nautilus_undo_get_undo_manager (undo_manager_search_start_object));

	/* Now we are done with the transaction.
	 * If the undo manager is holding it, then this will not destroy it.
	 */
	bonobo_object_unref (BONOBO_OBJECT (transaction));
}

/* Cover for forgetting about all undo relating to a particular target. */
void
nautilus_undo_unregister (GtkObject *target)
{
	/* Perhaps this should also unregister all children if called on a
	 * GtkContainer? That might be handy.
	 */
	nautilus_undo_transaction_unregister_object (target);
}

void
nautilus_undo (GtkObject *undo_manager_search_start_object)
{
	Nautilus_Undo_Manager manager;
	CORBA_Environment ev;

	g_return_if_fail (GTK_IS_OBJECT (undo_manager_search_start_object));

	CORBA_exception_init (&ev);

	manager = nautilus_undo_get_undo_manager (undo_manager_search_start_object);
	if (!CORBA_Object_is_nil (manager, &ev)) {
		Nautilus_Undo_Manager_undo (manager, &ev);
	}

	CORBA_exception_free (&ev);
}

Nautilus_Undo_Manager
nautilus_undo_get_undo_manager (GtkObject *start_object)
{
	Nautilus_Undo_Manager manager;
	GtkWidget *parent;
	GtkWindow *transient_parent;

	if (start_object == NULL) {
		return CORBA_OBJECT_NIL;
	}

	g_return_val_if_fail (GTK_IS_OBJECT (start_object), NULL);

	/* Check for an undo manager right here. */
	manager = gtk_object_get_data (start_object, NAUTILUS_UNDO_MANAGER_DATA);
	if (manager != NULL) {
		return manager;
	}

	/* Check for undo manager up the parent chain. */
	if (GTK_IS_WIDGET (start_object)) {
		parent = GTK_WIDGET (start_object)->parent;
		if (parent != NULL) {
			manager = nautilus_undo_get_undo_manager (GTK_OBJECT (parent));
			if (manager != NULL) {
				return manager;
			}
		}

		/* Check for undo manager in our window's parent. */
		if (GTK_IS_WINDOW (start_object)) {
			transient_parent = GTK_WINDOW (start_object)->transient_parent;
			if (transient_parent != NULL) {
				manager = nautilus_undo_get_undo_manager (GTK_OBJECT (transient_parent));
				if (manager != NULL) {
					return manager;
				}
			}
		}
	}
	
	/* In the case of a canvas item, try the canvas. */
	if (GNOME_IS_CANVAS_ITEM (start_object)) {
		manager = nautilus_undo_get_undo_manager (GTK_OBJECT (GNOME_CANVAS_ITEM (start_object)->canvas));
		if (manager != NULL) {
			return manager;
		}
	}
	
	/* Found nothing. I can live with that. */
	return CORBA_OBJECT_NIL;
}

static void
undo_manager_unref_cover (gpointer manager)
{
	bonobo_object_release_unref (manager, NULL);
}

void
nautilus_undo_attach_undo_manager (GtkObject *object,
				   Nautilus_Undo_Manager manager)
{
	g_return_if_fail (GTK_IS_OBJECT (object));

	if (manager == NULL) {
		gtk_object_remove_data (object, NAUTILUS_UNDO_MANAGER_DATA);
	} else {
		bonobo_object_dup_ref (manager, NULL);
		gtk_object_set_data_full
			(object, NAUTILUS_UNDO_MANAGER_DATA,
			 manager, undo_manager_unref_cover);
	}
}

/* Copy a reference to the undo manager fromone object to another. */
void
nautilus_undo_share_undo_manager (GtkObject *destination_object,
				  GtkObject *source_object)
{
	Nautilus_Undo_Manager manager;
	CORBA_Environment ev;

	manager = nautilus_undo_get_undo_manager (source_object);

	nautilus_undo_attach_undo_manager
		(destination_object, manager);

	CORBA_exception_init (&ev);
	CORBA_Object_release (manager, &ev);
	CORBA_exception_free (&ev);
}

/* Locates an undo manager for this bonobo control.
 * The undo manager is supplied by an interface on
 * the control frame. The put that undo manager on
 * the Bonobo control's widget.
 */
static void
set_up_bonobo_control (BonoboControl *control)
{
	Nautilus_Undo_Manager manager;
	Bonobo_ControlFrame control_frame;
	CORBA_Environment ev;
	Nautilus_Undo_Context undo_context;
	GtkWidget *widget;

	g_assert (BONOBO_IS_CONTROL (control));

	manager = CORBA_OBJECT_NIL;

	CORBA_exception_init (&ev);

	/* Find the undo manager. */
	control_frame = bonobo_control_get_control_frame (control);
	if (!CORBA_Object_is_nil (control_frame, &ev)) {
		undo_context = Bonobo_Control_queryInterface
			(control_frame, "IDL:Nautilus/Undo/Context:1.0", &ev);
		if (!CORBA_Object_is_nil (undo_context, &ev)) {
			manager = Nautilus_Undo_Context__get_undo_manager (undo_context, &ev);
			Bonobo_Control_unref (undo_context, &ev);
		}
		CORBA_Object_release (undo_context, &ev);
	}
	CORBA_Object_release (control_frame, &ev);

	/* Attach the undo manager to the widget, or detach the old one. */
	widget = bonobo_control_get_widget (control);
	nautilus_undo_attach_undo_manager (GTK_OBJECT (widget), manager);
	CORBA_Object_release (manager, &ev);

	CORBA_exception_free (&ev);
}

void
nautilus_undo_set_up_bonobo_control (BonoboControl *control)
{
	g_return_if_fail (BONOBO_IS_CONTROL (control));

	set_up_bonobo_control (control);
	gtk_signal_connect (GTK_OBJECT (control), "set_frame",
			    GTK_SIGNAL_FUNC (set_up_bonobo_control), NULL);
}
