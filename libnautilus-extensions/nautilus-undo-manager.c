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
  POA_Nautilus_UndoManager servant;
  gpointer bonobo_object;

  NautilusUndoManager *gtk_object;
} impl_POA_Nautilus_UndoManager;


/* GtkObject */
static void     nautilus_undo_manager_initialize_class  (NautilusUndoManagerClass  *class);
static void     nautilus_undo_manager_initialize        (NautilusUndoManager       *item);
static void	destroy 			   	(GtkObject *object);

static GList	*free_undo_manager_list_data 		(GList *list);
static GList	*prune_undo_manager_list 		(GList *list, gint items);

/* CORBA/Bonobo */

static CORBA_boolean 	impl_Nautilus_UndoManager__begin_transaction 	(impl_POA_Nautilus_UndoManager  *servant,
								   	 const CORBA_char 		*name,
								   	 CORBA_Environment              *ev);
static void 		impl_Nautilus_UndoManager__end_transaction 	(impl_POA_Nautilus_UndoManager  *servant,
								   	 CORBA_Environment              *ev);

NAUTILUS_DEFINE_CLASS_BOILERPLATE(NautilusUndoManager, nautilus_undo_manager, BONOBO_OBJECT_TYPE)

POA_Nautilus_UndoManager__epv libnautilus_extensions_Nautilus_UndoManager_epv =
{
	NULL,			/* _private */
	(gpointer) &impl_Nautilus_UndoManager__begin_transaction,
	(gpointer) &impl_Nautilus_UndoManager__end_transaction,
};

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

static POA_Nautilus_UndoManager__vepv impl_Nautilus_UndoManager_vepv =
{
	&base_epv,
	NULL,
	&libnautilus_extensions_Nautilus_UndoManager_epv
};


static void
impl_Nautilus_UndoManager__destroy(BonoboObject *obj, impl_POA_Nautilus_UndoManager *servant)
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

static Nautilus_UndoManager
impl_Nautilus_UndoManager__create(NautilusUndoManager *manager, CORBA_Environment * ev)
{
	Nautilus_UndoManager retval;
	impl_POA_Nautilus_UndoManager *servant;
	void (*servant_init_func) (PortableServer_Servant servant, CORBA_Environment *ev);

	NautilusUndoManagerClass *undo_class = NAUTILUS_UNDO_MANAGER_CLASS (GTK_OBJECT(manager)->klass);

	servant_init_func = undo_class->servant_init_func;
	servant = g_new0 (impl_POA_Nautilus_UndoManager, 1);
	servant->servant.vepv = undo_class->vepv;
	if (!servant->servant.vepv->Bonobo_Unknown_epv)
		servant->servant.vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
  	servant_init_func ((PortableServer_Servant) servant, ev);

  	servant->gtk_object = manager;

  	retval = bonobo_object_activate_servant (BONOBO_OBJECT (manager), servant);

  	gtk_signal_connect (GTK_OBJECT (manager), "destroy", GTK_SIGNAL_FUNC (impl_Nautilus_UndoManager__destroy), servant);

  	return retval;
}

static CORBA_boolean
impl_Nautilus_UndoManager__begin_transaction (impl_POA_Nautilus_UndoManager *servant,
					      const CORBA_char              *name,
					      CORBA_Environment             *ev)
{ 
	return nautilus_undo_manager_begin_transaction (name);
}

static void
impl_Nautilus_UndoManager__end_transaction (impl_POA_Nautilus_UndoManager *servant,
					    CORBA_Environment             *ev)
{
	nautilus_undo_manager_end_transaction ();
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

	/* Init transaction to none */
	manager->details->transaction = NULL;

	/* Create empty lists */
	manager->details->undo_list = NULL;
	manager->details->redo_list = NULL;

	/* Default to no redo functionality */
	manager->details->enable_redo = FALSE;

	/* Set queue depth to a single level */
	manager->details->queue_depth = 1;
	
	/* No transaction is in progress */
	manager->details->transaction_in_progress = FALSE;

	bonobo_object_construct (BONOBO_OBJECT (manager), impl_Nautilus_UndoManager__create (manager, &ev));

  	CORBA_exception_free(&ev);
}


/* Class initialization function for the NautilusUndoable item. */
static void
nautilus_undo_manager_initialize_class (NautilusUndoManagerClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = destroy;

	klass->servant_init_func = POA_Nautilus_UndoManager__init;
	klass->servant_destroy_func = POA_Nautilus_UndoManager__fini;
	klass->vepv = &impl_Nautilus_UndoManager_vepv;

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

/* nautilus_undo_manager_transaction_in_progress */
gboolean 
nautilus_undo_manager_transaction_in_progress (void)
{
	return global_undo_manager->details->transaction_in_progress;
}


/* nautilus_undo_manager_get_current_transaction
 * Return current undo transaction
 */
NautilusUndoTransaction *
nautilus_undo_manager_get_current_transaction (void)
{
	return global_undo_manager->details->transaction;
}


/* nautilus_undo_manager_begin_transaction */
gboolean 
nautilus_undo_manager_begin_transaction (const gchar *name)
{
	/* We aren't handling nested transactions currently */
	if (global_undo_manager->details->transaction_in_progress) {
		g_warning("NautilusUndoManager does not handle nested transactions. End previous transaction first.");
		return FALSE;
	}

	/* Create new transaction */
	global_undo_manager->details->transaction = nautilus_undo_transaction_new(name);

	global_undo_manager->details->transaction_in_progress = TRUE;

	return TRUE;
}

/* nautilus_undo_manager_end_transaction */
gboolean 
nautilus_undo_manager_end_transaction (void)
{
	/* Verify a transaction is in progress */
	if (!global_undo_manager->details->transaction_in_progress) {
		g_warning("NautilusUndoManager has no current transaction. Begin a transaction first.");
		return FALSE;
	}

	global_undo_manager->details->transaction_in_progress = FALSE;

	/* Commit current transaction to undo list */	
	nautilus_undo_manager_add_undo_transaction (global_undo_manager->details->transaction);

	/* Fire off signal informing that an undo transaction has occurred */
	gtk_signal_emit (GTK_OBJECT (global_undo_manager), signals[UNDO_TRANSACTION_OCCURED]);

	return TRUE;
}

/* nautilus_undo_manager_undo_last_transaction */
gboolean 
nautilus_undo_manager_undo_last_transaction (void)
{
	GList *list;
	NautilusUndoTransaction *undo_transaction;
	
	/* Verify we have a transaction to be undone */
	if (global_undo_manager->details->undo_list == NULL) {
		g_warning("NautilusUndoManager has no transaction to be undone.");
		return FALSE;
	}

	/* Pop last transaction off undo list */
	list = g_list_last(global_undo_manager->details->undo_list);
	g_assert(list);
	undo_transaction = list->data;
	global_undo_manager->details->undo_list = g_list_remove(global_undo_manager->details->undo_list, list->data);

	/* Undo transaction */
	nautilus_undo_transaction_undo(undo_transaction);
	
	/* Place transaction into redo list */
	if (global_undo_manager->details->enable_redo) {
		nautilus_undo_manager_add_redo_transaction (undo_transaction);
	} else {
		/* Purge transaction */
		nautilus_undo_transaction_destroy(undo_transaction);
	}

	/* Fire off signal informing that an undo transaction has occurred */
	gtk_signal_emit (GTK_OBJECT (global_undo_manager), signals[UNDO_TRANSACTION_OCCURED]);

	return TRUE;
}

/* nautilus_undo_manager_redo_last_undone_transaction */
gboolean 
nautilus_undo_manager_redo_last_undone_transaction (void)
{		
	GList *list;
	NautilusUndoTransaction *redo_transaction;

	/* Are we allowing redo operations? */
	if (global_undo_manager->details->enable_redo) {
		g_warning("NautilusUndoManager is not configure to allow redo operations.");
		return FALSE;
	}
		
	/* Verify we have a transaction to be redone */
	if (global_undo_manager->details->redo_list == NULL) {
		g_warning("NautilusUndoManager has no transaction to be redone.");
		return FALSE;
	}

	/* Pop last transaction off redo list */
	list = g_list_last(global_undo_manager->details->redo_list);
	g_assert(list);
	redo_transaction = list->data;

	nautilus_undo_transaction_undo(redo_transaction);

	/* Place transaction into undo list */
	nautilus_undo_manager_add_undo_transaction (redo_transaction);
	
	return TRUE;
}

/* nautilus_undo_manager_add_undoable_to_transaction */
gboolean 
nautilus_undo_manager_add_undoable_to_transaction (NautilusUndoable *undoable)
{
	gboolean result;

	/* Verify a transaction is in progress */
	if (!global_undo_manager->details->transaction_in_progress) {
		g_warning("NautilusUndoManager has no current transaction. Begin a transaction first.");
		return FALSE;
	}

	g_assert(global_undo_manager->details->transaction != NULL);

	result = nautilus_undo_transaction_add_undoable(global_undo_manager->details->transaction, undoable);

	return result;
}


/* nautilus_undo_manager_remove_transaction */
gboolean 
nautilus_undo_manager_remove_transaction (NautilusUndoTransaction *transaction)
{
	/* Verify a transaction is not in progress */
	if (global_undo_manager->details->transaction_in_progress) {
		g_warning("NautilusUndoManager cannot remove a transaction while one is in progress.");
		return FALSE;
	}

	g_return_val_if_fail(transaction != NULL, FALSE);
	
	/* Remove transaction from undo list */	
	global_undo_manager->details->undo_list = g_list_remove(global_undo_manager->details->undo_list, 
								transaction);

	/* Remove transaction from redo list */	
	global_undo_manager->details->redo_list = g_list_remove(global_undo_manager->details->redo_list, 
								transaction);

	/* Fire off signal informing that an undo transaction has occurred */
	gtk_signal_emit (GTK_OBJECT (global_undo_manager), signals[UNDO_TRANSACTION_OCCURED]);
	
	return TRUE;
}


/* nautilus_undo_manager_add_undo_transaction */
gboolean 
nautilus_undo_manager_add_undo_transaction (NautilusUndoTransaction *transaction)
{
	int length;
	
	/* Verify a transaction is not in progress */
	if (global_undo_manager->details->transaction_in_progress) {
		g_warning("NautilusUndoManager cannot add a transaction while one is in progress.");
		return FALSE;
	}

	g_return_val_if_fail(transaction != NULL, FALSE);

	/* Check and see if we are over our queue limit */
	length = g_list_length(global_undo_manager->details->undo_list);
			
	if (length >= global_undo_manager->details->queue_depth) {
		global_undo_manager->details->undo_list = prune_undo_manager_list (
							  global_undo_manager->details->undo_list, 
							  (length - global_undo_manager->details->queue_depth) + 1);
	}
	
	/* Add transaction to undo list */
	global_undo_manager->details->undo_list = g_list_append(global_undo_manager->details->undo_list, 
								transaction);

	/* Fire off signal informing that an undo transaction has occurred */
	gtk_signal_emit (GTK_OBJECT (global_undo_manager), signals[UNDO_TRANSACTION_OCCURED]);

	return TRUE;
}

/* nautilus_undo_manager_add_redo_transaction */
gboolean 
nautilus_undo_manager_add_redo_transaction (NautilusUndoTransaction *transaction)
{
	g_return_val_if_fail(transaction != NULL, FALSE);

	/* Verify a transaction is not in progress */
	if (global_undo_manager->details->transaction_in_progress) {
		g_warning("NautilusUndoManager cannot add a transaction while one is in progress.");
		return FALSE;
	}

	/* Make sure we allow redo */
	if (!global_undo_manager->details->enable_redo) {
		return FALSE;
	}

	/* Add transaction to undo list */	
	global_undo_manager->details->undo_list = g_list_append(global_undo_manager->details->undo_list, 
								transaction);

	/* Fire off signal informing that an redo transaction has occurred */
	gtk_signal_emit (GTK_OBJECT (global_undo_manager), signals[UNDO_TRANSACTION_OCCURED]);

	return TRUE;
}


/* nautilus_undo_manager_unregister_object
 * 
 * Remove any transaction with object as target from
 * the undo and redo queues
 */
  
gboolean 
nautilus_undo_manager_unregister_object (GtkObject *object)
{
	NautilusUndoTransaction *transaction;
	gboolean success;
	GList *list;
	int index, length;

	success = FALSE;
	
	/* Check undo list */
	length = g_list_length(global_undo_manager->details->undo_list);
	for (index = 0; index < length; index++) {
		list = g_list_nth(global_undo_manager->details->undo_list, index);
		if (list) {
			transaction = list->data;
			if (nautilus_undo_transaction_contains_object(transaction, object)) {
				global_undo_manager->details->undo_list =
					g_list_remove(global_undo_manager->details->undo_list, transaction);
				nautilus_undo_transaction_destroy(transaction);
				index--;
				success = TRUE;
			}
		}
	}

	/* Check redo list */
	length = g_list_length(global_undo_manager->details->redo_list);
	for (index = 0; index < length; index++) {
		list = g_list_nth(global_undo_manager->details->redo_list, index);
		if (list) {
			transaction = list->data;
			if (nautilus_undo_transaction_contains_object(transaction, object)) {
				global_undo_manager->details->redo_list =
					g_list_remove(global_undo_manager->details->redo_list, transaction);
				nautilus_undo_transaction_destroy(transaction);
				index--;
				success = TRUE;
			}
		}
	}

	if (success) {
		/* Fire off signal informing that a transaction has occurred */
		gtk_signal_emit (GTK_OBJECT (global_undo_manager), signals[UNDO_TRANSACTION_OCCURED]);
	}
	
	return success;
}

/* nautilus_undo_manager_can_undo */
gboolean 
nautilus_undo_manager_can_undo (void)
{	
	if (global_undo_manager != NULL) {
		return (g_list_length(global_undo_manager->details->undo_list) > 0);
	} else {
		return FALSE;
	}
}

/* nautilus_undo_manager_can_redo */
gboolean 
nautilus_undo_manager_can_redo (void)
{
	if (global_undo_manager->details->enable_redo) {
		if (global_undo_manager != NULL) {		
			return (g_list_length(global_undo_manager->details->redo_list) > 0);
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

static gint
get_current_transaction_name(GList *list, gchar *name, gint max_length)
{
	NautilusUndoTransaction *transaction;
	const gchar *transaction_name;
	gint name_length;
	
	/* Check for NULL list */
	if (list == NULL) {
		g_warning("Unable to get current transaction name due to NULL undo list.");
		return -1;
	}

	/* Check for empty list */
	if (g_list_length(list) <= 0) {
		g_warning("No transaction to get current undo transaction name from.");
		return -1;
	}

	/* Get last transaction from list */
	list = g_list_last(list);
	transaction = list->data;
	if (transaction == NULL) {
		g_warning("Unable to get current transaction name due to NULL transaction in list.");
		return -1;
	}
	
	/* Check for valid transaction name */
	transaction_name = nautilus_undo_transaction_get_name(transaction);
	if ( transaction_name == NULL) {
		g_warning("Current transaction name is NULL.");
		return -1;
	}

	/* Return length of transaction name if name argument is NULL */
	if (name == NULL) {
		return strlen(transaction_name);
	}

	/* Copy over requested amount into return name argument */
	name_length = strlen(transaction_name);
	if (name_length > max_length) {
		strncpy(name, transaction_name, max_length);
		return max_length;
	} else {
		strncpy(name, transaction_name, name_length);
		return name_length;
	}
}


/* nautilus_undo_manager_get_current_undo_transaction_name
 * 
 * Return transaction name up to max_length characters in name.  If name is NULL, return 
 * length of transaction name.  Return -1 if transaction name is NULL or there is no
 * current undo transaction.
 */
 
gint 
nautilus_undo_manager_get_current_undo_transaction_name (gchar *name, gint max_length)
{
	return get_current_transaction_name(global_undo_manager->details->undo_list, name, max_length);
}

/* nautilus_undo_manager_get_current_undo_transaction_name
 * 
 * Return transaction name up to max_length characters in name.  If name is NULL, return 
 * length of transaction name.  Return -1 if transaction name is NULL or there is no
 * current undo transaction.
 */
 
gint 
nautilus_undo_manager_get_current_redo_transaction_name (gchar *name, gint max_length)
{
	return get_current_transaction_name(global_undo_manager->details->redo_list, name, max_length);
}

/* destroy */
static void
destroy (GtkObject *object)
{
	g_return_if_fail (NAUTILUS_IS_UNDO_MANAGER (object));

	/* Clear lists */
	global_undo_manager->details->undo_list = free_undo_manager_list_data(global_undo_manager->details->undo_list);
	global_undo_manager->details->redo_list = free_undo_manager_list_data(global_undo_manager->details->redo_list);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


/* Initialize Nautilus global undo manager */
gboolean 
nautilus_undo_manager_initialize_global_manager (void)
{
	if (global_undo_manager != NULL) {
		g_warning("A global undo manager has already been created.");
		return FALSE;
	}
		
	global_undo_manager = nautilus_undo_manager_new();

	return TRUE;
}


/* Return global undo manager */
NautilusUndoManager *
nautilus_undo_manager_get_undo_manager (void)
{
	g_return_val_if_fail(global_undo_manager != NULL, NULL);
	return global_undo_manager;
}


/* nautilus_undo_manager_enable_redo
 * 
 * Enable or disable redo functionality
 */ 
gboolean
nautilus_undo_manager_enable_redo (gboolean value)
{
	g_return_val_if_fail(global_undo_manager != NULL, FALSE);

	global_undo_manager->details->enable_redo = value;

	/* Flush and free redo queue */
	g_list_free(global_undo_manager->details->redo_list);

	return TRUE;
}

/* nautilus_undo_manager_enable_redo
 * 
 * Enable or disable redo functionality
 */ 
gboolean		
nautilus_undo_manager_set_queue_depth (gint depth)
{
	int length;
	
	/* Don't allow negative or zero queue depth */
	if (depth <= 0) {
		g_warning ("NautilusUndoManager doesn not allow negative or zero length queue.");
		return FALSE;
	}
	
	global_undo_manager->details->queue_depth = depth;

	/* Prune lists */
	length = g_list_length (global_undo_manager->details->undo_list);
	if (length > depth) {
		global_undo_manager->details->undo_list = prune_undo_manager_list (global_undo_manager->details->undo_list, 
										   length - depth);
	}
	length = g_list_length (global_undo_manager->details->redo_list);
	if (length > depth) {
		global_undo_manager->details->undo_list = prune_undo_manager_list (global_undo_manager->details->redo_list, 
										   length - depth);
	}

	/* Fire off signal informing that an undo transaction has occurred */
	gtk_signal_emit (GTK_OBJECT (global_undo_manager), signals[UNDO_TRANSACTION_OCCURED]);
		
	return TRUE;	
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
	
	length = g_list_length(list);		
	for (index = 0; index < length; index++) {
		list = g_list_last(list);
		if (list) {
			transaction = list->data;
			list = g_list_remove(list, transaction);
			nautilus_undo_transaction_destroy(transaction);
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
		list = g_list_first(list);
		if (list) {
			transaction = list->data;
			list = g_list_remove(list, transaction);
			nautilus_undo_transaction_destroy(transaction);
		}
	}

	return list;
}
