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

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <gtk/gtksignal.h>
#include <bonobo/bonobo-main.h>
#include <libnautilus/nautilus-undo-private.h>
#include "nautilus-undo-context.h"

struct NautilusUndoManagerDetails {
	GList *undo_list;
	GList *redo_list;
	gboolean enable_redo;
	gint queue_depth;
};

enum {
	CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

typedef struct {
	POA_Nautilus_Undo_Manager servant;
	NautilusUndoManager *bonobo_object;
} impl_POA_Nautilus_Undo_Manager;

typedef struct {
	BonoboUIHandler *handler;
	char *path;
	char *no_undo_menu_item_label;
	char *no_undo_menu_item_hint;
} UndoMenuHandlerConnection;

/* GtkObject */
static void   nautilus_undo_manager_initialize_class   (NautilusUndoManagerClass  *class);
static void   nautilus_undo_manager_initialize         (NautilusUndoManager       *item);
static void   destroy                                  (GtkObject                 *object);
static void   free_undo_manager_list                   (GList                     *list);
static GList *prune_undo_manager_list                  (GList                     *list,
							int                        items);
/* CORBA/Bonobo */
static void   impl_Nautilus_Undo_Manager__append       (PortableServer_Servant     servant,
							Nautilus_Undo_Transaction  transaction,
							CORBA_Environment         *ev);
static void   impl_Nautilus_Undo_Manager__forget       (PortableServer_Servant     servant,
							Nautilus_Undo_Transaction  transaction,
							CORBA_Environment         *ev);
static void   impl_Nautilus_Undo_Manager__undo         (PortableServer_Servant     servant,
							CORBA_Environment         *ev);
static void   nautilus_undo_manager_add_transaction    (NautilusUndoManager       *manager,
							Nautilus_Undo_Transaction  transaction);
static void   nautilus_undo_manager_forget_transaction (NautilusUndoManager       *manager,
							Nautilus_Undo_Transaction  transaction);

NAUTILUS_DEFINE_CLASS_BOILERPLATE(NautilusUndoManager, nautilus_undo_manager, BONOBO_OBJECT_TYPE)

static POA_Nautilus_Undo_Manager__epv libnautilus_Nautilus_Undo_Manager_epv =
{
	NULL,
	&impl_Nautilus_Undo_Manager__append,
	&impl_Nautilus_Undo_Manager__forget,
	&impl_Nautilus_Undo_Manager__undo,
};
static PortableServer_ServantBase__epv base_epv;
static POA_Nautilus_Undo_Manager__vepv vepv =
{
	&base_epv,
	NULL,
	&libnautilus_Nautilus_Undo_Manager_epv
};

static void
impl_Nautilus_Undo_Manager__destroy (BonoboObject *object,
				     impl_POA_Nautilus_Undo_Manager *servant)
{
	PortableServer_ObjectId *object_id;
	CORBA_Environment ev;

  	CORBA_exception_init (&ev);

  	object_id = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
  	PortableServer_POA_deactivate_object (bonobo_poa (), object_id, &ev);
  	CORBA_free (object_id);
  	object->servant = NULL;

	POA_Nautilus_Undo_Manager__fini (servant, &ev);
  	g_free (servant);

  	CORBA_exception_free (&ev);
}

static Nautilus_Undo_Manager
impl_Nautilus_Undo_Manager__create (NautilusUndoManager *bonobo_object,
				    CORBA_Environment *ev)
{
	impl_POA_Nautilus_Undo_Manager *servant;

	servant = g_new0 (impl_POA_Nautilus_Undo_Manager, 1);

	vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();
	servant->servant.vepv = &vepv;
  	POA_Nautilus_Undo_Manager__init ((PortableServer_Servant) servant, ev);

  	gtk_signal_connect (GTK_OBJECT (bonobo_object), "destroy",
			    GTK_SIGNAL_FUNC (impl_Nautilus_Undo_Manager__destroy),
			    servant);

  	servant->bonobo_object = bonobo_object;
	return bonobo_object_activate_servant (BONOBO_OBJECT (bonobo_object), servant);
}

static void
impl_Nautilus_Undo_Manager__append (PortableServer_Servant servant,
				    Nautilus_Undo_Transaction undo_transaction,
				    CORBA_Environment *ev)
{
	NautilusUndoManager *manager;

	manager = ((impl_POA_Nautilus_Undo_Manager *) servant)->bonobo_object;
	g_assert (NAUTILUS_IS_UNDO_MANAGER (manager));

	nautilus_undo_manager_add_transaction (manager, undo_transaction);
}

static void
impl_Nautilus_Undo_Manager__forget (PortableServer_Servant servant,
			            Nautilus_Undo_Transaction transaction,
				    CORBA_Environment *ev)
{
	NautilusUndoManager *manager;

	manager = ((impl_POA_Nautilus_Undo_Manager *) servant)->bonobo_object;
	g_assert (NAUTILUS_IS_UNDO_MANAGER (manager));

	nautilus_undo_manager_forget_transaction (manager, transaction);
}

static void 	
impl_Nautilus_Undo_Manager__undo (PortableServer_Servant servant,
				  CORBA_Environment *ev)
{
	NautilusUndoManager *manager;

	manager = ((impl_POA_Nautilus_Undo_Manager *) servant)->bonobo_object;
	g_assert (NAUTILUS_IS_UNDO_MANAGER (manager));
	
	nautilus_undo_manager_undo (manager);
}

NautilusUndoManager *
nautilus_undo_manager_new (void)
{
	return gtk_type_new (nautilus_undo_manager_get_type ());
}

static void
nautilus_undo_manager_initialize (NautilusUndoManager *manager)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);

	manager->details = g_new0 (NautilusUndoManagerDetails, 1);

	/* Set queue depth to a single level */
	manager->details->queue_depth = 1;
	
	bonobo_object_construct (BONOBO_OBJECT (manager),
				 impl_Nautilus_Undo_Manager__create (manager, &ev));

  	CORBA_exception_free (&ev);
}


static void
nautilus_undo_manager_initialize_class (NautilusUndoManagerClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = destroy;

	signals[CHANGED]
		= gtk_signal_new ("changed",
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
	GList *last_in_list;
	CORBA_Object undo_transaction;
	CORBA_Environment ev;

  	CORBA_exception_init (&ev);

	/* Verify we have a transaction to be undone */
	if (manager->details->undo_list == NULL) {
		return;
	}

	/* Pop last transaction off undo list */
	last_in_list = g_list_last (manager->details->undo_list);
	g_assert (last_in_list != NULL);
	undo_transaction = last_in_list->data;
	manager->details->undo_list = g_list_remove (manager->details->undo_list, undo_transaction);

	/* Undo transaction */
	Nautilus_Undo_Transaction_undo (undo_transaction, &ev);
	
	/* Place transaction into redo list */
	if (manager->details->enable_redo) {
		/* FIXME bugzilla.eazel.com 1290: Implement redo. */
		/* nautilus_undo_manager_add_redo_transaction (undo_transaction); */
	} else {
		/* Purge transaction */
		Nautilus_Undo_Transaction_unref (undo_transaction, &ev);
	}

	/* Fire off signal informing that an undo transaction has occurred */
	gtk_signal_emit (GTK_OBJECT (manager), signals[CHANGED]);

	CORBA_exception_free (&ev);
}

#if 0
static void
nautilus_undo_manager_redo (NautilusUndoManager *manager)
{
	GList *list;
	CORBA_Object redo_transaction;
	CORBA_Object undo_transaction;
	CORBA_Environment ev;

  	CORBA_exception_init (&ev);

	/* Are we allowing redo operations? */
	if (manager->details->enable_redo) {
		g_warning ("NautilusUndoManager is not configure to allow redo operations.");
		return;
	}
		
	/* Verify we have a transaction to be redone */
	if (manager->details->redo_list == NULL) {
		g_warning ("NautilusUndoManager has no transaction to be redone.");
		return;
	}

	/* Pop last transaction off redo list */
	list = g_list_last (manager->details->redo_list);
	g_assert(list);
	redo_transaction = list->data;

	Nautilus_Undo_Transaction_undo (redo_transaction, &ev);

	/* Place transaction into undo list */
	undo_transaction = bonobo_object_corba_objref (BONOBO_OBJECT (redo_transaction));
	nautilus_undo_manager_add_transaction (manager, undo_transaction);

	CORBA_exception_free (&ev);
}
#endif

static void 
nautilus_undo_manager_add_transaction (NautilusUndoManager *manager,
				       Nautilus_Undo_Transaction transaction)
{
	int length;
	Nautilus_Undo_Transaction duplicate_transaction;
	CORBA_Environment ev;

	g_return_if_fail (NAUTILUS_IS_UNDO_MANAGER (manager));
	g_return_if_fail (transaction != CORBA_OBJECT_NIL);

	CORBA_exception_init (&ev);

	/* Check and see if we are over our queue limit */
	length = g_list_length (manager->details->undo_list);
	if (length >= manager->details->queue_depth) {
		manager->details->undo_list = prune_undo_manager_list
			(manager->details->undo_list, 
			 (length - manager->details->queue_depth) + 1);
	}

	/* Perform refs on the transaction */
	duplicate_transaction = CORBA_Object_duplicate (transaction, &ev);
	Nautilus_Undo_Transaction_ref (duplicate_transaction, &ev);

	/* Add transaction to undo list */
	manager->details->undo_list = g_list_append (manager->details->undo_list, duplicate_transaction);
	
	/* Fire off signal informing that an undo transaction has occurred */
	gtk_signal_emit (GTK_OBJECT (manager), signals[CHANGED]);

	CORBA_exception_free (&ev);
}


static void 
nautilus_undo_manager_forget_transaction (NautilusUndoManager *manager,
					  Nautilus_Undo_Transaction transaction)
{
	GList *list;
	int index, length;
	CORBA_Environment ev;
	gboolean success;
	
	CORBA_exception_init (&ev);

	success = FALSE;

	/* Check undo list */
	length = g_list_length (manager->details->undo_list);
	for (index = 0; index < length; index++) {
		list = g_list_nth (manager->details->undo_list, index);
		if (list) {
			transaction = list->data;
				manager->details->undo_list =
					g_list_remove (manager->details->undo_list, transaction);					
				Nautilus_Undo_Transaction_unref (transaction, &ev);			
				success = TRUE;
				index--;
		}
	}

	/* Check redo list */
	length = g_list_length (manager->details->redo_list);
	for (index = 0; index < length; index++) {
		list = g_list_nth (manager->details->redo_list, index);
		if (list) {
			transaction = list->data;
				manager->details->redo_list =
					g_list_remove (manager->details->redo_list, transaction);					
				Nautilus_Undo_Transaction_unref (transaction, &ev);			
				success = TRUE;
				index--;
		}
	}

	if (success) {
		/* Fire off signal informing that a transaction has occurred */
		gtk_signal_emit (GTK_OBJECT (manager), signals[CHANGED]);
	}
	
	CORBA_exception_free (&ev);
}

#if 0

gboolean
nautilus_undo_manager_can_undo (NautilusUndoManager *manager)
{	
	return manager != NULL
		&& manager->details->undo_list != NULL;
}

gboolean
nautilus_undo_manager_can_redo (NautilusUndoManager *manager)
{
	return manager != NULL
		&& manager->details->enable_redo
		&& manager->details->redo_list != NULL;
}

#endif

static void
destroy (GtkObject *object)
{
	NautilusUndoManager *manager;

	manager = NAUTILUS_UNDO_MANAGER (object);
	
	/* Clear lists */
	free_undo_manager_list (manager->details->undo_list);
	free_undo_manager_list (manager->details->redo_list);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

#if 0

/* nautilus_undo_manager_enable_redo
 * 
 * Enable or disable redo functionality
 */ 
void
nautilus_undo_manager_enable_redo (NautilusUndoManager *manager, gboolean value)
{
	g_return_if_fail (NAUTILUS_IS_UNDO_MANAGER (manager));
	g_return_if_fail (value == FALSE || value == TRUE);

	manager->details->enable_redo = value;

	/* Flush and free redo queue */
	free_undo_manager_list (manager->details->redo_list);
	manager->details->redo_list = NULL;
}

void		
nautilus_undo_manager_set_queue_depth (NautilusUndoManager *manager, int depth)
{
	int length;
	
	g_return_if_fail (NAUTILUS_IS_UNDO_MANAGER (manager));
	g_return_if_fail (depth > 0);
	
	manager->details->queue_depth = depth;
	
	/* Prune lists */
	length = g_list_length (manager->details->undo_list);
	if (length > depth) {
		manager->details->undo_list = prune_undo_manager_list (manager->details->undo_list, 
								       length - depth);
	}
	length = g_list_length (manager->details->redo_list);
	if (length > depth) {
		manager->details->undo_list = prune_undo_manager_list (manager->details->redo_list, 
								       length - depth);
	}

	/* Fire off signal informing that an undo transaction has occurred */
	gtk_signal_emit (GTK_OBJECT (manager), signals[CHANGED]);
}

#endif

/* free_undo_manager_list
 * 
 * Clear undo data from list
 */
static void
free_undo_manager_list (GList *list)
{
	GList *p;
	Nautilus_Undo_Transaction transaction;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	for (p = list; p != NULL; p = p->next) {
		transaction = p->data;
		
		Nautilus_Undo_Transaction_unref (transaction, &ev);
	}
	CORBA_exception_free (&ev);

	g_list_free (list);
}


/* prune_undo_manager_list
 * 
 * Prune n items from start of list 
 */
static GList *
prune_undo_manager_list (GList *list, int items)
{
	int i;
	Nautilus_Undo_Transaction transaction;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	for (i = 0; i < items; i++) {
		if (list != NULL) {
			transaction = list->data;
			list = g_list_remove (list, transaction);
			Nautilus_Undo_Transaction_unref (transaction, &ev);
		}
	}
	CORBA_exception_free (&ev);

	return list;
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

	if (manager->details->undo_list == NULL) {
		menu_item = NULL;
	} else {
		menu_item = Nautilus_Undo_Transaction__get_undo_menu_item
			(g_list_last (manager->details->undo_list)->data, &ev);
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
	nautilus_gtk_signal_connect_full_while_alive
		(GTK_OBJECT (manager), "changed",
		 GTK_SIGNAL_FUNC (update_undo_menu_item), NULL,
		 connection, undo_menu_handler_connection_free_cover,
		 FALSE, FALSE,
		 GTK_OBJECT (handler));
}
