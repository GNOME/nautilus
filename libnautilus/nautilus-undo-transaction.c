/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NautilusUndoTransaction - An object for an undoable transcation.
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

#include <string.h>
#include <glib.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-control.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>

#include "nautilus-undo-transaction.h"

typedef struct {
  POA_Nautilus_Undo_Transaction servant;
  gpointer bonobo_object;

  NautilusUndoTransaction *undo_transaction;
} impl_POA_Nautilus_Undo_Transaction;

/* GtkObject */
static void     nautilus_undo_transaction_initialize_class  	(NautilusUndoTransactionClass  *class);
static void     nautilus_undo_transaction_initialize        	(NautilusUndoTransaction       *item);
static void	nautilus_undo_transaction_destroy 	    	(GtkObject *object);
static void 	nautilus_undo_transaction_undo 		  	(NautilusUndoTransaction *transaction);


/* CORBA/Bonobo */
static Nautilus_Undo_MenuItem *impl_Nautilus_Undo_Transaction__get_undo_description 	(impl_POA_Nautilus_Undo_Transaction  *servant,
							 					 CORBA_Environment *ev);
static Nautilus_Undo_MenuItem *impl_Nautilus_Undo_Transaction__get_redo_description 	(impl_POA_Nautilus_Undo_Transaction  *servant,
							 					 CORBA_Environment *ev);
static CORBA_char *impl_Nautilus_Undo_Transaction__get_base_description 			(impl_POA_Nautilus_Undo_Transaction  *servant,
							 					 CORBA_Environment *ev);
static void impl_Nautilus_Undo_Transaction__undo 						(impl_POA_Nautilus_Undo_Transaction  *servant,
							 					 CORBA_Environment *ev);
							 					 

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusUndoTransaction, nautilus_undo_transaction, BONOBO_OBJECT_TYPE)

POA_Nautilus_Undo_Transaction__epv libnautilus_Nautilus_Undo_Transaction_epv =
{
	NULL,			/* _private */

	(gpointer) &impl_Nautilus_Undo_Transaction__get_undo_description,
	(gpointer) &impl_Nautilus_Undo_Transaction__get_redo_description,
	(gpointer) &impl_Nautilus_Undo_Transaction__get_base_description,
	(gpointer) &impl_Nautilus_Undo_Transaction__undo
};

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

static POA_Nautilus_Undo_Transaction__vepv impl_Nautilus_Undo_Transaction_vepv =
{
	&base_epv,
	NULL,
	&libnautilus_Nautilus_Undo_Transaction_epv
};


static void
impl_Nautilus_Undo_Transaction__destroy(BonoboObject *obj, impl_POA_Nautilus_Undo_Transaction *servant)
{
	PortableServer_ObjectId *objid;
	CORBA_Environment ev;
	void (*servant_destroy_func) (PortableServer_Servant servant, CORBA_Environment *ev);

  	CORBA_exception_init (&ev);

  	servant_destroy_func = NAUTILUS_UNDO_TRANSACTION_CLASS (GTK_OBJECT (servant->undo_transaction)->klass)->servant_destroy_func;
  	objid = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
  	PortableServer_POA_deactivate_object (bonobo_poa (), objid, &ev);
  	CORBA_free (objid);
  	obj->servant = NULL;

  	servant_destroy_func ((PortableServer_Servant) servant, &ev);
  	g_free (servant);
  	CORBA_exception_free(&ev);
}


static Nautilus_Undo_Transaction
impl_Nautilus_Undo_Transaction__create (NautilusUndoTransaction *transaction, CORBA_Environment *ev)
{
	Nautilus_Undo_Transaction retval;
	impl_POA_Nautilus_Undo_Transaction *servant;
	void (*servant_init_func) (PortableServer_Servant servant, CORBA_Environment *ev);

	NautilusUndoTransactionClass *transaction_class = NAUTILUS_UNDO_TRANSACTION_CLASS (GTK_OBJECT (transaction)->klass);

	servant_init_func = transaction_class->servant_init_func;
	servant = g_new0 (impl_POA_Nautilus_Undo_Transaction, 1);
	servant->servant.vepv = transaction_class->vepv;
	if (!servant->servant.vepv->Bonobo_Unknown_epv)
		servant->servant.vepv->Bonobo_Unknown_epv = bonobo_object_get_epv ();
  	servant_init_func ((PortableServer_Servant) servant, ev);

  	servant->undo_transaction = transaction;

  	retval = bonobo_object_activate_servant (BONOBO_OBJECT (transaction), servant);

  	gtk_signal_connect (GTK_OBJECT (transaction), "destroy", GTK_SIGNAL_FUNC (impl_Nautilus_Undo_Transaction__destroy), servant);

  	return retval;
}


static Nautilus_Undo_MenuItem *impl_Nautilus_Undo_Transaction__get_undo_description 	(impl_POA_Nautilus_Undo_Transaction  *servant,
							 					 CORBA_Environment *ev)
{
	return NULL;
}
							 				
static Nautilus_Undo_MenuItem *impl_Nautilus_Undo_Transaction__get_redo_description 	(impl_POA_Nautilus_Undo_Transaction  *servant,
							 					 CORBA_Environment *ev)
{
	return NULL;
}
							 					 
static CORBA_char *impl_Nautilus_Undo_Transaction__get_base_description (impl_POA_Nautilus_Undo_Transaction  *servant,
							 		 CORBA_Environment *ev)
{
	return NULL;
}
							 					 
static void impl_Nautilus_Undo_Transaction__undo (impl_POA_Nautilus_Undo_Transaction  *servant, CORBA_Environment *ev)
{
	nautilus_undo_transaction_undo (servant->undo_transaction);
}							 					 

/* nautilus_undo_transaction_new */
NautilusUndoTransaction *
nautilus_undo_transaction_new (const gchar *name)
{
	NautilusUndoTransaction *transaction;

	transaction = gtk_type_new (nautilus_undo_transaction_get_type ());
	
	transaction->name = g_strdup(name);

	return transaction;
}


/* Object initialization function for NautilusUndoTransaction */
static void 
nautilus_undo_transaction_initialize (NautilusUndoTransaction *transaction)
{
	CORBA_Environment ev;	
	CORBA_exception_init(&ev);

	/* Create empty lists */
	transaction->transaction_list = NULL;

	bonobo_object_construct (BONOBO_OBJECT (transaction), 
				 impl_Nautilus_Undo_Transaction__create (transaction, &ev));

  	CORBA_exception_free(&ev);
}

/* nautilus_undo_transaction_destroy */
static void
nautilus_undo_transaction_destroy (GtkObject *object)
{
	NautilusUndoTransaction *transaction;

	g_return_if_fail (NAUTILUS_IS_UNDO_TRANSACTION (object));

	transaction = NAUTILUS_UNDO_TRANSACTION (object);
	
	/* Empty list */
	g_list_free(transaction->transaction_list);

	g_free(transaction->name);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

/* Class initialization function for the NautilusUndoable item. */
static void
nautilus_undo_transaction_initialize_class (NautilusUndoTransactionClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);

	object_class->destroy = nautilus_undo_transaction_destroy;

	klass->servant_init_func = POA_Nautilus_Undo_Transaction__init;
	klass->servant_destroy_func = POA_Nautilus_Undo_Transaction__fini;
	klass->vepv = &impl_Nautilus_Undo_Transaction_vepv;
}

/* nautilus_undo_transaction_add_undoable */
gboolean 
nautilus_undo_transaction_add_undoable	(NautilusUndoTransaction *transaction, 
						 NautilusUndoable *undoable)
{	
	if (transaction == NULL) {
		g_warning("Cannot add undoable to a NULL transaction");
		return FALSE;
	}

	if (undoable == NULL) {
		g_warning("Cannot add a NULL undoable to a transaction");
		return FALSE;
	}

	transaction->transaction_list = g_list_append(transaction->transaction_list, undoable);
	
	return TRUE;
}



/* nautilus_undo_transaction_undo
 * Parse transaction and send undo signals to undoable objects stored in transaction 
 */

void 
nautilus_undo_transaction_undo (NautilusUndoTransaction *transaction)
{
	NautilusUndoable *undoable;
	guint index;
	
	if (transaction == NULL) {
		return;
	}
		
	for ( index = 0; index < g_list_length(transaction->transaction_list); index++) {

		/* Get pointer to undoable */
		undoable = g_list_nth_data(transaction->transaction_list, index);

		/* Send object restore from undo signal */
		if (undoable != NULL)
			nautilus_undoable_restore_from_undo_snapshot (undoable);
	}
}


/* nautilus_undo_transaction_contains_object 
 * 
 * Return name of requested transaction 
 */
const gchar *
nautilus_undo_transaction_get_name (NautilusUndoTransaction *transaction)
{
	g_return_val_if_fail(transaction != NULL, NULL);
	
	return transaction->name;
}


/* nautilus_undo_transaction_contains_object 
 * 
 * Return TRUE if object is contained by transaction
 */
gboolean		
nautilus_undo_transaction_contains_object (NautilusUndoTransaction *transaction, 
					   GtkObject *object)
{
	NautilusUndoable *undoable;
	gint index;

	g_return_val_if_fail(transaction != NULL, FALSE);
		
	for ( index = 0; index < g_list_length(transaction->transaction_list); index++) {

		/* Get pointer to undoable */
		undoable = g_list_nth_data(transaction->transaction_list, index);
		
		/* Check and see if we have a target object match */
		if (undoable != NULL && undoable->undo_target_class == object) {
			return TRUE;
		}
	}

	return FALSE;
}
