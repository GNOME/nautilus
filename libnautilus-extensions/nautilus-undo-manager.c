/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NautilusUndoManager - Undo/Redo transaction manager.
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
#include "nautilus-undo-manager.h"

#include <eel/eel-gtk-macros.h>
#include <eel/eel-gtk-extensions.h>
#include <gtk/gtksignal.h>
#include <bonobo/bonobo-main.h>
#include <libnautilus/nautilus-bonobo-workarounds.h>
#include <libnautilus/nautilus-undo-private.h>
#include "nautilus-undo-context.h"

struct NautilusUndoManagerDetails {
	Nautilus_Undo_Transaction transaction;

	/* These are used to tell undo from redo. */
	gboolean current_transaction_is_redo;
	gboolean new_transaction_is_redo;

	/* These are used only so that we can complain if we get more
	 * than one transaction inside undo.
	 */
	gboolean undo_in_progress;
        int num_transactions_during_undo;
};

enum {
	CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

typedef struct {
	POA_Nautilus_Undo_Manager servant;
	NautilusUndoManager *bonobo_object;
} UndoManagerServant;

typedef struct {
#ifdef UIH
	BonoboUIHandler *handler;
#endif /* UIH */
	char *path;
	char *no_undo_menu_item_label;
	char *no_undo_menu_item_hint;
} UndoMenuHandlerConnection;

/* GtkObject */
static void nautilus_undo_manager_initialize_class (NautilusUndoManagerClass  *class);
static void nautilus_undo_manager_initialize       (NautilusUndoManager       *item);
static void destroy                                (GtkObject                 *object);

/* CORBA/Bonobo */
static void corba_append                           (PortableServer_Servant     servant,
						    Nautilus_Undo_Transaction  transaction,
						    CORBA_Environment         *ev);
static void corba_forget                           (PortableServer_Servant     servant,
						    Nautilus_Undo_Transaction  transaction,
						    CORBA_Environment         *ev);
static void corba_undo                             (PortableServer_Servant     servant,
						    CORBA_Environment         *ev);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusUndoManager, nautilus_undo_manager, BONOBO_OBJECT_TYPE)

static PortableServer_ServantBase__epv base_epv;
static POA_Nautilus_Undo_Manager__epv epv = {
	NULL,
	&corba_append,
	&corba_forget,
	&corba_undo,
};
static POA_Nautilus_Undo_Manager__vepv vepv = {
	&base_epv,
	NULL,
	&epv
};

static void
destroy_servant (BonoboObject *object,
		 UndoManagerServant *servant)
{
	PortableServer_ObjectId *object_id;
	CORBA_Environment ev;

  	CORBA_exception_init (&ev);

	/* Deactivate the object. */
  	object_id = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
  	PortableServer_POA_deactivate_object (bonobo_poa (), object_id, &ev);
  	CORBA_free (object_id);

	/* Disconnect the object from the servant. */
  	object->servant = NULL;

	/* Free the servant. */
	POA_Nautilus_Undo_Manager__fini (servant, &ev);
  	g_free (servant);

  	CORBA_exception_free (&ev);
}

static Nautilus_Undo_Manager
create_servant (NautilusUndoManager *bonobo_object,
		CORBA_Environment *ev)
{
	UndoManagerServant *servant;

	/* Create the servant. */
	servant = g_new0 (UndoManagerServant, 1);
	servant->servant.vepv = &vepv;
  	servant->bonobo_object = bonobo_object;
  	POA_Nautilus_Undo_Manager__init ((PortableServer_Servant) servant, ev);

	/* Set up code so we will destroy the servant when the bonobo_object is destroyed. */
  	gtk_signal_connect (GTK_OBJECT (bonobo_object), "destroy",
			    destroy_servant, servant);

	/* Activate the servant to create the CORBA object. */
	return bonobo_object_activate_servant (BONOBO_OBJECT (bonobo_object), servant);
}

static void
release_transaction (NautilusUndoManager *manager)
{
	Nautilus_Undo_Transaction transaction;

	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);

	transaction = manager->details->transaction;
	manager->details->transaction = CORBA_OBJECT_NIL;
	if (!CORBA_Object_is_nil (transaction, &ev)) {
		bonobo_object_release_unref (transaction, &ev);
	}

	CORBA_exception_free (&ev);
}

static void
corba_append (PortableServer_Servant servant,
	      Nautilus_Undo_Transaction transaction,
	      CORBA_Environment *ev)
{
	NautilusUndoManager *manager;
	Nautilus_Undo_Transaction duplicate_transaction;

	manager = ((UndoManagerServant *) servant)->bonobo_object;
	g_assert (NAUTILUS_IS_UNDO_MANAGER (manager));

	/* Check, complain, and ignore the passed-in transaction if we
	 * get more than one within a single undo operation. The single
	 * transaction we get during the undo operation is supposed to
	 * be the one for redoing the undo (or re-undoing the redo).
	 */
	if (manager->details->undo_in_progress) {
		manager->details->num_transactions_during_undo += 1;
		g_return_if_fail (manager->details->num_transactions_during_undo == 1);		
	}
	
	g_return_if_fail (!CORBA_Object_is_nil (transaction, ev));

	/* Keep a copy of this transaction (dump the old one). */
	duplicate_transaction = CORBA_Object_duplicate (transaction, ev);
	Nautilus_Undo_Transaction_ref (duplicate_transaction, ev);
	release_transaction (manager);
	manager->details->transaction = duplicate_transaction;
	manager->details->current_transaction_is_redo =
		manager->details->new_transaction_is_redo;
	
	/* Fire off signal indicating that the undo state has changed. */
	gtk_signal_emit (GTK_OBJECT (manager), signals[CHANGED]);
}

static void
corba_forget (PortableServer_Servant servant,
	      Nautilus_Undo_Transaction transaction,
	      CORBA_Environment *ev)
{
	NautilusUndoManager *manager;

	manager = ((UndoManagerServant *) servant)->bonobo_object;
	g_assert (NAUTILUS_IS_UNDO_MANAGER (manager));

	/* Nothing to forget unless the item we are passed is the
	 * transaction we are currently holding.
	 */
	if (!CORBA_Object_is_equivalent (manager->details->transaction, transaction, ev)) {
		return;
	}

	/* Get rid of the transaction we are holding on to. */
	release_transaction (manager);
	
	/* Fire off signal indicating that the undo state has changed. */
	gtk_signal_emit (GTK_OBJECT (manager), signals[CHANGED]);
}

static void
corba_undo (PortableServer_Servant servant,
	    CORBA_Environment *ev)
{
	NautilusUndoManager *manager;

	manager = ((UndoManagerServant *) servant)->bonobo_object;
	g_assert (NAUTILUS_IS_UNDO_MANAGER (manager));
	
	nautilus_undo_manager_undo (manager);
}

NautilusUndoManager *
nautilus_undo_manager_new (void)
{
	return NAUTILUS_UNDO_MANAGER (gtk_object_new (nautilus_undo_manager_get_type (), NULL));
}

static void
nautilus_undo_manager_initialize (NautilusUndoManager *manager)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);

	manager->details = g_new0 (NautilusUndoManagerDetails, 1);

	bonobo_object_construct (BONOBO_OBJECT (manager),
				 create_servant (manager, &ev));

  	CORBA_exception_free (&ev);
}

static void
nautilus_undo_manager_initialize_class (NautilusUndoManagerClass *klass)
{
	GtkObjectClass *object_class;

	vepv.Bonobo_Unknown_epv = nautilus_bonobo_object_get_epv ();

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = destroy;

	signals[CHANGED] = gtk_signal_new
		("changed",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusUndoManagerClass,
				    changed),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);	
}

void
nautilus_undo_manager_undo (NautilusUndoManager *manager)
{
	CORBA_Environment ev;
	Nautilus_Undo_Transaction transaction;

	g_return_if_fail (NAUTILUS_IS_UNDO_MANAGER (manager));

  	CORBA_exception_init (&ev);

	transaction = manager->details->transaction;
	manager->details->transaction = CORBA_OBJECT_NIL;
	if (!CORBA_Object_is_nil (transaction, &ev)) {
		/* Perform the undo. New transactions that come in
		 * during an undo are redo transactions. New
		 * transactions that come in during a redo are undo
		 * transactions. Transactions that come in outside
		 * are always undo and never redo.
		 */
		manager->details->new_transaction_is_redo =
			!manager->details->current_transaction_is_redo;
		manager->details->undo_in_progress = TRUE;
		manager->details->num_transactions_during_undo = 0;
		Nautilus_Undo_Transaction_undo (transaction, &ev);
		manager->details->undo_in_progress = FALSE;
		manager->details->new_transaction_is_redo = FALSE;

		/* Let go of the transaction. */
		bonobo_object_release_unref (transaction, &ev);

		/* Fire off signal indicating the undo state has changed. */
		gtk_signal_emit (GTK_OBJECT (manager), signals[CHANGED]);
	}

	CORBA_exception_free (&ev);
}

static void
destroy (GtkObject *object)
{
	NautilusUndoManager *manager;

	manager = NAUTILUS_UNDO_MANAGER (object);

	release_transaction (manager);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

void
nautilus_undo_manager_attach (NautilusUndoManager *manager, GtkObject *target)
{
	g_return_if_fail (NAUTILUS_IS_UNDO_MANAGER (manager));
	g_return_if_fail (GTK_IS_OBJECT (target));

	nautilus_undo_attach_undo_manager
		(target,
		 bonobo_object_corba_objref (BONOBO_OBJECT (manager)));
}

void
nautilus_undo_manager_add_interface (NautilusUndoManager *manager, BonoboObject *object)
{
	NautilusUndoContext *context;

	g_return_if_fail (NAUTILUS_IS_UNDO_MANAGER (manager));
	g_return_if_fail (BONOBO_IS_OBJECT (object));

	context = nautilus_undo_context_new (bonobo_object_corba_objref (BONOBO_OBJECT (manager)));
	bonobo_object_add_interface (object, BONOBO_OBJECT (context));
}

#ifdef UIH
static void
update_undo_menu_item (NautilusUndoManager *manager,
		       UndoMenuHandlerConnection *connection)
{
	CORBA_Environment ev;
	Nautilus_Undo_MenuItem *menu_item;

	g_assert (NAUTILUS_IS_UNDO_MANAGER (manager));
	g_assert (connection != NULL);
	g_assert (BONOBO_IS_UI_HANDLER (connection->handler));
	g_assert (connection->path != NULL);
	g_assert (connection->no_undo_menu_item_label != NULL);
	g_assert (connection->no_undo_menu_item_hint != NULL);
	
	CORBA_exception_init (&ev);

	if (CORBA_Object_is_nil (manager->details->transaction, &ev)) {
		menu_item = NULL;
	} else {
		if (manager->details->current_transaction_is_redo) {
			menu_item = Nautilus_Undo_Transaction__get_redo_menu_item
				(manager->details->transaction, &ev);
		} else {
			menu_item = Nautilus_Undo_Transaction__get_undo_menu_item
				(manager->details->transaction, &ev);
		}
	}

	bonobo_ui_handler_menu_set_sensitivity
		(connection->handler, connection->path,
		 menu_item != NULL);
	bonobo_ui_handler_menu_set_label
		(connection->handler, connection->path,
		 menu_item == NULL
		 ? connection->no_undo_menu_item_label
		 : menu_item->label);
	bonobo_ui_handler_menu_set_hint
		(connection->handler, connection->path,
		 menu_item == NULL
		 ? connection->no_undo_menu_item_hint
		 : menu_item->hint);
	
	CORBA_free (menu_item);
	
	CORBA_exception_free (&ev);
}

static void
undo_menu_handler_connection_free (UndoMenuHandlerConnection *connection)
{
	g_assert (connection != NULL);
	g_assert (BONOBO_IS_UI_HANDLER (connection->handler));
	g_assert (connection->path != NULL);
	g_assert (connection->no_undo_menu_item_label != NULL);
	g_assert (connection->no_undo_menu_item_hint != NULL);

	g_free (connection->path);
	g_free (connection->no_undo_menu_item_label);
	g_free (connection->no_undo_menu_item_hint);
	g_free (connection);
}

static void
undo_menu_handler_connection_free_cover (gpointer data)
{
	undo_menu_handler_connection_free (data);
}

void
nautilus_undo_manager_set_up_bonobo_ui_handler_undo_item (NautilusUndoManager *manager,
							  BonoboUIHandler *handler,
							  const char *path,
							  const char *no_undo_menu_item_label,
							  const char *no_undo_menu_item_hint)
{
	UndoMenuHandlerConnection *connection;

	connection = g_new (UndoMenuHandlerConnection, 1);
	connection->handler = handler;
	connection->path = g_strdup (path);
	connection->no_undo_menu_item_label = g_strdup (no_undo_menu_item_label);
	connection->no_undo_menu_item_hint = g_strdup (no_undo_menu_item_hint);

	/* Set initial state of menu item. */
	update_undo_menu_item (manager, connection);

	/* Update it again whenever the changed signal is emitted. */
	eel_gtk_signal_connect_full_while_alive
		(GTK_OBJECT (manager), "changed",
		 GTK_SIGNAL_FUNC (update_undo_menu_item), NULL,
		 connection, undo_menu_handler_connection_free_cover,
		 FALSE, FALSE,
		 GTK_OBJECT (handler));
}
#endif /* UIH */
