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
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <glib.h>
#include <string.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-control.h>

#include "nautilus-undo-manager.h"
#include "nautilus-undo-manager-private.h"

#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include "nautilus-undo-manager.h"

enum {
	UNDO_TRANSACTION_OCCURED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];


typedef struct {
  POA_Nautilus_Undo_Manager servant;
  gpointer bonobo_object;

  NautilusUndoManager *gtk_object;
} impl_POA_Nautilus_Undo_Manager;

/* GtkObject */
static void     nautilus_undo_manager_initialize_class  (NautilusUndoManagerClass  *class);
static void     nautilus_undo_manager_initialize        (NautilusUndoManager       *item);
static void	destroy 			   	(GtkObject *object);

static GList	*free_undo_manager_list_data 		(GList *list);
static GList	*prune_undo_manager_list 		(GList *list, gint items);

/* CORBA/Bonobo */

static void 		impl_Nautilus_Undo_Manager__append 	(impl_POA_Nautilus_Undo_Manager  *servant,
								   	const CORBA_char 	 *name,
								   	 CORBA_Environment       *ev);
static void 		impl_Nautilus_Undo_Manager__forget 	(impl_POA_Nautilus_Undo_Manager  *servant,
								   	 CORBA_Environment       *ev);

NAUTILUS_DEFINE_CLASS_BOILERPLATE(NautilusUndoManager, nautilus_undo_manager, BONOBO_OBJECT_TYPE)

POA_Nautilus_Undo_Manager__epv libnautilus_Nautilus_Undo_Manager_epv =
{
	NULL,			/* _private */
	(gpointer) &impl_Nautilus_Undo_Manager__append,
	(gpointer) &impl_Nautilus_Undo_Manager__forget,
};

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

static POA_Nautilus_Undo_Manager__vepv impl_Nautilus_Undo_Manager_vepv =
{
	&base_epv,
	NULL,
	&libnautilus_Nautilus_Undo_Manager_epv
};


static void
impl_Nautilus_Undo_Manager__destroy(BonoboObject *obj, impl_POA_Nautilus_Undo_Manager *servant)
{
	PortableServer_ObjectId *objid;
	CORBA_Environment ev;
	void (*servant_destroy_func) (PortableServer_Servant servant, CORBA_Environment *ev);

  	CORBA_exception_init(&ev);

  	servant_destroy_func = NAUTILUS_UNDO_MANAGER_CLASS (GTK_OBJECT (servant->gtk_object)->klass)->servant_destroy_func;
  	objid = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
  	PortableServer_POA_deactivate_object (bonobo_poa (), objid, &ev);
  	CORBA_free (objid);
  	obj->servant = NULL;

  	servant_destroy_func ((PortableServer_Servant) servant, &ev);
  	g_free (servant);
  	CORBA_exception_free(&ev);
}

static Nautilus_Undo_Manager
impl_Nautilus_Undo_Manager__create(NautilusUndoManager *manager, CORBA_Environment * ev)
{
	Nautilus_Undo_Manager retval;
	impl_POA_Nautilus_Undo_Manager *servant;
	void (*servant_init_func) (PortableServer_Servant servant, CORBA_Environment *ev);

	NautilusUndoManagerClass *undo_class = NAUTILUS_UNDO_MANAGER_CLASS (GTK_OBJECT(manager)->klass);

	servant_init_func = undo_class->servant_init_func;
	servant = g_new0 (impl_POA_Nautilus_Undo_Manager, 1);
	servant->servant.vepv = undo_class->vepv;
	if (!servant->servant.vepv->Bonobo_Unknown_epv)
		servant->servant.vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
  	servant_init_func ((PortableServer_Servant) servant, ev);

  	servant->gtk_object = manager;

  	retval = bonobo_object_activate_servant (BONOBO_OBJECT (manager), servant);

  	gtk_signal_connect (GTK_OBJECT (manager), "destroy", GTK_SIGNAL_FUNC (impl_Nautilus_Undo_Manager__destroy), servant);

  	return retval;
}

static void
impl_Nautilus_Undo_Manager__append (impl_POA_Nautilus_Undo_Manager *servant,
					      const CORBA_char              *name,
					      CORBA_Environment             *ev)
{ 
}

static void
impl_Nautilus_Undo_Manager__forget (impl_POA_Nautilus_Undo_Manager *servant,
					    CORBA_Environment             *ev)
{
}


/* nautilus_undo_manager_new */
NautilusUndoManager *
nautilus_undo_manager_new (void)
{
	NautilusUndoManager *manager;
	
	manager = gtk_type_new (nautilus_undo_manager_get_type ());

	return manager;
}


/* Object initialization function for the NautilusUndoManager */
static void 
nautilus_undo_manager_initialize (NautilusUndoManager *manager)
{
	CORBA_Environment ev;	
	CORBA_exception_init(&ev);

	manager->details = g_new0 (NautilusUndoManagerDetails, 1);

	/* Create empty lists */
	manager->details->undo_list = NULL;
	manager->details->redo_list = NULL;

	/* Default to no redo functionality */
	manager->details->enable_redo = FALSE;

	/* Set queue depth to a single level */
	manager->details->queue_depth = 1;
	
	/* No transaction is in progress */
	manager->details->transaction_in_progress = FALSE;

	bonobo_object_construct (BONOBO_OBJECT (manager), impl_Nautilus_Undo_Manager__create (manager, &ev));

  	CORBA_exception_free(&ev);
}


/* Class initialization function for the NautilusUndoable item. */
static void
nautilus_undo_manager_initialize_class (NautilusUndoManagerClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = destroy;

	klass->servant_init_func = POA_Nautilus_Undo_Manager__init;
	klass->servant_destroy_func = POA_Nautilus_Undo_Manager__fini;
	klass->vepv = &impl_Nautilus_Undo_Manager_vepv;

	/* Setup signals */
	signals[UNDO_TRANSACTION_OCCURED]
		= gtk_signal_new ("undo_transaction_occurred",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusUndoManagerClass,
						     undo_transaction_occurred),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);	
}

/* nautilus_undo_manager_begin_transaction */
NautilusUndoTransactionInProgress * 
nautilus_undo_manager_begin_transaction (GtkObject *object, const gchar *name)
{
	NautilusUndoManager *manager;
	NautilusUndoTransactionInProgress *tip;
	
	/* Locate undo manager. */
	/* FIXME: We can't get a pointer to the actual undo manager, so this
	 * needs to work through the CORBA interface to the undo manager.
	 */
	manager = nautilus_get_undo_manager (object);
	g_assert (manager != NULL);
	
	/* We aren't handling nested transactions currently */
	if (manager->details->transaction_in_progress) {
		g_warning("NautilusUndoManager does not handle nested transactions. End previous transaction first.");
		return NULL;
	}

	/* Create NautilusUndoTransactionInProgress */
	tip = g_new (NautilusUndoTransactionInProgress, 1);
	g_assert(tip);
	
	tip->manager = manager;
	
	/* Create new transaction */
	tip->transaction = nautilus_undo_transaction_new(name);

	tip->manager->details->transaction_in_progress = TRUE;

	return tip;
}

/* nautilus_undo_manager_end_transaction */
void 
nautilus_undo_manager_end_transaction (NautilusUndoTransactionInProgress *tip)
{
	/* Verify a transaction is in progress */
	if (!tip->manager->details->transaction_in_progress) {
		g_warning("NautilusUndoManager has no current transaction. Begin a transaction first.");
		return;
	}

	tip->manager->details->transaction_in_progress = FALSE;

	/* Commit current transaction to undo list */	
	nautilus_undo_manager_add_transaction (tip->manager, tip->transaction);

	/* Fire off signal informing that an undo transaction has occurred */
	gtk_signal_emit (GTK_OBJECT (tip->manager), signals[UNDO_TRANSACTION_OCCURED]);
}

/* nautilus_undo_manager_undo_last_transaction */
void 
nautilus_undo_manager_undo (NautilusUndoManager *manager)
{
	GList *list;
	NautilusUndoTransaction *undo_transaction;
	
	/* Verify we have a transaction to be undone */
	if (manager->details->undo_list == NULL) {
		g_warning("NautilusUndoManager has no transaction to be undone.");
		return;
	}

	/* Pop last transaction off undo list */
	list = g_list_last(manager->details->undo_list);
	g_assert(list);
	undo_transaction = list->data;
	manager->details->undo_list = g_list_remove(manager->details->undo_list, list->data);

	/* Undo transaction */
	nautilus_undo_transaction_undo(undo_transaction);
	
	/* Place transaction into redo list */
	if (manager->details->enable_redo) {
		/* nautilus_undo_manager_add_redo_transaction (undo_transaction); */
	} else {
		/* Purge transaction */
		nautilus_undo_transaction_destroy(undo_transaction);
	}

	/* Fire off signal informing that an undo transaction has occurred */
	gtk_signal_emit (GTK_OBJECT (manager), signals[UNDO_TRANSACTION_OCCURED]);
}

/* nautilus_undo_manager_redo_last_undone_transaction */
void 
nautilus_undo_manager_redo (NautilusUndoManager *manager)
{		
	GList *list;
	NautilusUndoTransaction *redo_transaction;

	/* Are we allowing redo operations? */
	if (manager->details->enable_redo) {
		g_warning("NautilusUndoManager is not configure to allow redo operations.");
		return;
	}
		
	/* Verify we have a transaction to be redone */
	if (manager->details->redo_list == NULL) {
		g_warning("NautilusUndoManager has no transaction to be redone.");
		return;
	}

	/* Pop last transaction off redo list */
	list = g_list_last(manager->details->redo_list);
	g_assert(list);
	redo_transaction = list->data;

	nautilus_undo_transaction_undo(redo_transaction);

	/* Place transaction into undo list */
	nautilus_undo_manager_add_transaction (manager, redo_transaction);
}

/* nautilus_undo_manager_add_undo_transaction */
void 
nautilus_undo_manager_add_transaction (NautilusUndoManager *manager, NautilusUndoTransaction *transaction)
{
	int length;
	
	/* Verify a transaction is not in progress */
	if (manager->details->transaction_in_progress) {
		g_warning("NautilusUndoManager cannot add a transaction while one is in progress.");
		return;
	}

	g_return_if_fail (transaction != NULL);

	/* Check and see if we are over our queue limit */
	length = g_list_length (manager->details->undo_list);
			
	if (length >= manager->details->queue_depth) {
		manager->details->undo_list = prune_undo_manager_list (
							  manager->details->undo_list, 
							  (length - manager->details->queue_depth) + 1);
	}
	
	/* Add transaction to undo list */
	manager->details->undo_list = g_list_append(manager->details->undo_list, transaction);

	/* Fire off signal informing that an undo transaction has occurred */
	gtk_signal_emit (GTK_OBJECT (manager), signals[UNDO_TRANSACTION_OCCURED]);
}


/* nautilus_undo_manager_unregister_object
 * 
 * Remove any transaction with object as target from
 * the undo and redo queues
 */

gboolean 
nautilus_undo_manager_unregister_object (GtkObject *object)
{
	NautilusUndoManager *manager;
	NautilusUndoTransaction *transaction;
	gboolean success;
	GList *list;
	int index, length;

	/* FIXME: We can't get a pointer to the actual undo manager, so this
	 * needs to work through the CORBA interface to the undo manager.
	 * Also there's no reason to assume that this object will have the
	 * right one.
	 */
	manager = nautilus_get_undo_manager (object);
	if (manager == NULL) {
		return FALSE;
	}

	success = FALSE;

	/* Check undo list */
	length = g_list_length (manager->details->undo_list);
	for (index = 0; index < length; index++) {
		list = g_list_nth (manager->details->undo_list, index);
		if (list) {
			transaction = list->data;
			if (nautilus_undo_transaction_contains_object (transaction, object)) {
				manager->details->undo_list =
					g_list_remove (manager->details->undo_list, transaction);
				nautilus_undo_transaction_destroy (transaction);
				index--;
				success = TRUE;
			}
		}
	}

	/* Check redo list */
	length = g_list_length (manager->details->redo_list);
	for (index = 0; index < length; index++) {
		list = g_list_nth (manager->details->redo_list, index);
		if (list) {
			transaction = list->data;
			if (nautilus_undo_transaction_contains_object(transaction, object)) {
				manager->details->redo_list =
					g_list_remove (manager->details->redo_list, transaction);
				nautilus_undo_transaction_destroy(transaction);
				index--;
				success = TRUE;
			}
		}
	}

	if (success) {
		/* Fire off signal informing that a transaction has occurred */
		gtk_signal_emit (GTK_OBJECT (manager), signals[UNDO_TRANSACTION_OCCURED]);
	}
	
	return success;
}

/* nautilus_undo_manager_can_undo */
gboolean 
nautilus_undo_manager_can_undo (NautilusUndoManager *manager)
{	
	if (manager != NULL) {
		return (g_list_length (manager->details->undo_list) > 0);
	} else {
		return FALSE;
	}
}

/* nautilus_undo_manager_can_redo */
gboolean 
nautilus_undo_manager_can_redo (NautilusUndoManager *manager)
{
	if (manager->details->enable_redo) {
		if (manager != NULL) {		
			return (g_list_length (manager->details->redo_list) > 0);
		} else {
			return FALSE;
		}
	} else {
		return FALSE;
	}
}

/* get_current_transaction_name
 * 
 * Return transaction name up to max_length characters in name.  If name is NULL, return 
 * length of transaction name.  Return -1 if transaction name is NULL or there is no
 * current undo transaction.
 */

static const gchar *
get_current_transaction_name (GList *list)
{
	NautilusUndoTransaction *transaction;
	const gchar *transaction_name;
		
	/* Check for NULL list */
	if (list == NULL) {
		g_warning("Unable to get current transaction name due to NULL undo list.");
		return NULL;
	}

	/* Check for empty list */
	if (g_list_length(list) <= 0) {
		g_warning("No transaction to get current undo transaction name from.");
		return NULL;
	}

	/* Get last transaction from list */
	list = g_list_last(list);
	transaction = list->data;
	if (transaction == NULL) {
		g_warning("Unable to get current transaction name due to NULL transaction in list.");
		return NULL;
	}
	
	/* Check for valid transaction name */
	transaction_name = nautilus_undo_transaction_get_name(transaction);
	if ( transaction_name == NULL) {
		g_warning("Current transaction name is NULL.");
		return NULL;
	}

	return transaction_name;
}


/* nautilus_undo_manager_get_current_undo_transaction_name
 * 
 * Return transaction name up to max_length characters in name.  If name is NULL, return 
 * length of transaction name.  Return -1 if transaction name is NULL or there is no
 * current undo transaction.
 */
 
const gchar * 
nautilus_undo_manager_get_current_undo_transaction_name (NautilusUndoManager *manager)
{
	return get_current_transaction_name (manager->details->undo_list);
}

/* nautilus_undo_manager_get_current_undo_transaction_name
 * 
 * Return transaction name up to max_length characters in name.  If name is NULL, return 
 * length of transaction name.  Return -1 if transaction name is NULL or there is no
 * current undo transaction.
 */
 
const gchar * 
nautilus_undo_manager_get_current_redo_transaction_name (NautilusUndoManager *manager)
{
	return get_current_transaction_name (manager->details->redo_list);
}

/* destroy */
static void
destroy (GtkObject *object)
{
	NautilusUndoManager *manager;

	g_return_if_fail (NAUTILUS_IS_UNDO_MANAGER (object));

	manager = NAUTILUS_UNDO_MANAGER (object);
	
	/* Clear lists */
	manager->details->undo_list = free_undo_manager_list_data (manager->details->undo_list);
	manager->details->redo_list = free_undo_manager_list_data (manager->details->redo_list);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


/* nautilus_undo_manager_enable_redo
 * 
 * Enable or disable redo functionality
 */ 

void
nautilus_undo_manager_enable_redo (NautilusUndoManager *manager, gboolean value)
{
	g_return_if_fail (manager != NULL);

	manager->details->enable_redo = value;

	/* Flush and free redo queue */
	g_list_free (manager->details->redo_list);
}

/* nautilus_undo_manager_enable_redo
 * 
 * Enable or disable redo functionality
 */ 
void		
nautilus_undo_manager_set_queue_depth (NautilusUndoManager *manager, gint depth)
{
	int length;
	
	/* Don't allow negative or zero queue depth */
	if (depth <= 0) {
		g_warning ("NautilusUndoManager doesn not allow negative or zero length queue.");
		return;
	}
	
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
	gtk_signal_emit (GTK_OBJECT (manager), signals[UNDO_TRANSACTION_OCCURED]);
}

/* free_undo_manager_list_data
 * 
 * Clear undo data from list
 */

static GList *
free_undo_manager_list_data (GList *list)
{
	int length, index;
	NautilusUndoTransaction *transaction;

	length = g_list_length (list);		
	for (index = 0; index < length; index++) {
		list = g_list_last (list);
		if (list) {
			transaction = list->data;
			list = g_list_remove (list, transaction);
			nautilus_undo_transaction_destroy (transaction);
		}
	}

	return list;
}


/* prune_undo_manager_list_data
 * 
 * Prune n items from start of list 
 */

static GList *
prune_undo_manager_list (GList *list, gint items)
{
	gint index;
	NautilusUndoTransaction *transaction;
	
	for (index = 0; index < items; index++) {
		list = g_list_first (list);
		if (list) {
			transaction = list->data;
			list = g_list_remove (list, transaction);
			nautilus_undo_transaction_destroy (transaction);
		}
	}

	return list;
}

/* FIXME: This should return a Nautilus_Undo_Manager in the long run.
 * And it's more likely that we'll want this in the transaction code
 * than in here so it will probably be moved.
 */
NautilusUndoManager *
nautilus_get_undo_manager (GtkObject *start_object)
{
	NautilusUndoManager *manager;
	GtkWidget *parent;
	GtkWindow *transient_parent;

	if (start_object == NULL) {
		return NULL;
	}

	g_return_val_if_fail (GTK_IS_OBJECT (start_object), NULL);

	/* Check for an undo manager right here. */
	manager = gtk_object_get_data (start_object, "Nautilus undo");
	if (manager != NULL) {
		g_assert (NAUTILUS_IS_UNDO_MANAGER (manager));
		return manager;
	}

	/* Check for undo manager up the parent chain. */
	if (GTK_IS_WIDGET (start_object)) {
		parent = GTK_WIDGET (start_object)->parent;
		if (parent != NULL) {
			manager = nautilus_get_undo_manager (GTK_OBJECT (parent));
			if (manager != NULL) {
				return manager;
			}
		}

		/* Check for undo manager in our window's parent. */
		if (GTK_IS_WINDOW (start_object)) {
			transient_parent = GTK_WINDOW (start_object)->transient_parent;
			manager = nautilus_get_undo_manager (GTK_OBJECT (transient_parent));
			if (manager != NULL) {
				return manager;
			}
		}
	}

	/* In the case of a canvas item, try the canvas. */
	if (GNOME_IS_CANVAS_ITEM (start_object)) {
		manager = nautilus_get_undo_manager (GTK_OBJECT (GNOME_CANVAS_ITEM (start_object)->canvas));
		if (manager != NULL) {
			return manager;
		}
	}
		
	/* Found nothing. I can live with that. */
	return NULL;
}

void
nautilus_attach_undo_manager (GtkObject *object,
			      NautilusUndoManager *manager)
{
	g_return_if_fail (GTK_IS_OBJECT (object));

	if (manager == NULL) {
		gtk_object_remove_data (object, "Nautilus undo");
	}

	g_return_if_fail (NAUTILUS_IS_UNDO_MANAGER (manager));

	bonobo_object_ref (BONOBO_OBJECT (manager));
	gtk_object_set_data_full
		(object, "Nautilus undo",
		 manager, (GtkDestroyNotify) bonobo_object_unref);
}

/* This is useful because nautilus_get_undo_manager will be
 * private one day.
 */
void
nautilus_share_undo_manager (GtkObject *destination_object,
			     GtkObject *source_object)
{
	nautilus_attach_undo_manager
		(destination_object,
		 nautilus_get_undo_manager (source_object));
}
