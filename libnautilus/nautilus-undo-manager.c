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

#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <glib.h>
#include <string.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-control.h>

#include "nautilus-undo-manager-private.h"

#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>


/* Gloabl instance of undo manager */
Nautilus_Undo_Manager global_undo_manager;

enum {
	UNDO_TRANSACTION_OCCURRED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

typedef struct {
	POA_Nautilus_Undo_Manager servant;
	NautilusUndoManager *bonobo_object;
} impl_POA_Nautilus_Undo_Manager;

/* GtkObject */
static void   nautilus_undo_manager_initialize_class (NautilusUndoManagerClass        *class);
static void   nautilus_undo_manager_initialize       (NautilusUndoManager             *item);
static void   destroy                                (GtkObject                       *object);
static void   free_undo_manager_list                 (GList                           *list);
static GList *prune_undo_manager_list                (GList                           *list,
						      int 			       items);
/* CORBA/Bonobo */
static void   impl_Nautilus_Undo_Manager__append     (PortableServer_Servant           servant,
						      const Nautilus_Undo_Transaction  transaction,
						      CORBA_Environment               *ev);
static void   impl_Nautilus_Undo_Manager__forget     (PortableServer_Servant           servant,
						      const Nautilus_Undo_Transaction  transaction,
						      CORBA_Environment               *ev);
static void   impl_Nautilus_Undo_Manager__undo       (PortableServer_Servant           servant,
						      CORBA_Environment               *ev);
static void   nautilus_undo_manager_add_transaction  (NautilusUndoManager             *manager,
						      Nautilus_Undo_Transaction        transaction);
static void   nautilus_undo_manager_undo             (NautilusUndoManager             *manager);

NAUTILUS_DEFINE_CLASS_BOILERPLATE(NautilusUndoManager, nautilus_undo_manager, BONOBO_OBJECT_TYPE)

POA_Nautilus_Undo_Manager__epv libnautilus_Nautilus_Undo_Manager_epv =
{
	NULL,			/* _private */
	&impl_Nautilus_Undo_Manager__append,
	&impl_Nautilus_Undo_Manager__forget,
	&impl_Nautilus_Undo_Manager__undo,
};

static PortableServer_ServantBase__epv base_epv;
static POA_Nautilus_Undo_Manager__vepv impl_Nautilus_Undo_Manager_vepv =
{
	&base_epv,
	NULL,
	&libnautilus_Nautilus_Undo_Manager_epv
};

static void
impl_Nautilus_Undo_Manager__destroy (BonoboObject *obj, impl_POA_Nautilus_Undo_Manager *servant)
{
	PortableServer_ObjectId *objid;
	CORBA_Environment ev;
	void (*servant_destroy_func) (PortableServer_Servant servant, CORBA_Environment *ev);

  	CORBA_exception_init (&ev);

  	servant_destroy_func = NAUTILUS_UNDO_MANAGER_CLASS (GTK_OBJECT (servant->bonobo_object)->klass)->servant_destroy_func;
  	objid = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
  	PortableServer_POA_deactivate_object (bonobo_poa (), objid, &ev);
  	CORBA_free (objid);
  	obj->servant = NULL;

  	servant_destroy_func ((PortableServer_Servant) servant, &ev);
  	g_free (servant);
  	CORBA_exception_free (&ev);
}

static Nautilus_Undo_Manager
impl_Nautilus_Undo_Manager__create (NautilusUndoManager *manager, CORBA_Environment * ev)
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

  	servant->bonobo_object = manager;

  	retval = bonobo_object_activate_servant (BONOBO_OBJECT (manager), servant);

  	gtk_signal_connect (GTK_OBJECT (manager), "destroy", GTK_SIGNAL_FUNC (impl_Nautilus_Undo_Manager__destroy), servant);

  	return retval;
}

static void
impl_Nautilus_Undo_Manager__append (PortableServer_Servant servant,
				    const Nautilus_Undo_Transaction undo_transaction,
				    CORBA_Environment *ev)
{
	NautilusUndoManager *manager;

	manager = ((impl_POA_Nautilus_Undo_Manager *) servant)->bonobo_object;
	g_assert (NAUTILUS_IS_UNDO_MANAGER (manager));

	nautilus_undo_manager_add_transaction (manager, undo_transaction);
}

static void
impl_Nautilus_Undo_Manager__forget (PortableServer_Servant servant,
			            const Nautilus_Undo_Transaction transaction,
				    CORBA_Environment *ev)
{
	NautilusUndoManager *manager;

	manager = ((impl_POA_Nautilus_Undo_Manager *) servant)->bonobo_object;
	g_assert (NAUTILUS_IS_UNDO_MANAGER (manager));
	
	/* FIXME: Need to implement this. */
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

/* Object initialization function for the NautilusUndoManager */
static void 
nautilus_undo_manager_initialize (NautilusUndoManager *manager)
{
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);

	manager->details = g_new0 (NautilusUndoManagerDetails, 1);

	/* Create empty lists */
	manager->details->undo_list = NULL;
	manager->details->redo_list = NULL;

	/* Default to no redo functionality */
	manager->details->enable_redo = FALSE;

	/* Set queue depth to a single level */
	manager->details->queue_depth = 1;
	
	bonobo_object_construct (BONOBO_OBJECT (manager), impl_Nautilus_Undo_Manager__create (manager, &ev));

  	CORBA_exception_free (&ev);
}


/* Class initialization function for the NautilusUndoManager. */
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
	signals[UNDO_TRANSACTION_OCCURRED]
		= gtk_signal_new ("undo_transaction_occurred",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusUndoManagerClass,
						     undo_transaction_occurred),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);	
}

static void 
nautilus_undo_manager_undo (NautilusUndoManager *manager)
{
	GList *last_in_list;
	CORBA_Object undo_transaction;
	CORBA_Environment ev;

  	CORBA_exception_init (&ev);

	/* Verify we have a transaction to be undone */
	if (manager->details->undo_list == NULL) {
		g_warning ("NautilusUndoManager has no transaction to be undone.");
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
		/* FIXME: Implement redo. */
		/* nautilus_undo_manager_add_redo_transaction (undo_transaction); */
	} else {
		/* Purge transaction */
		Nautilus_Undo_Transaction_unref (undo_transaction, &ev);
	}

	/* Fire off signal informing that an undo transaction has occurred */
	gtk_signal_emit (GTK_OBJECT (manager),
			 signals[UNDO_TRANSACTION_OCCURRED]);

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

	CORBA_exception_init(&ev);
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
	gtk_signal_emit (GTK_OBJECT (manager),
			 signals[UNDO_TRANSACTION_OCCURRED]);

	CORBA_exception_free(&ev);
}

/* nautilus_undo_manager_unregister_object
 * 
 * Remove any transaction with object as target from
 * the undo and redo queues
 */
gboolean 
nautilus_undo_manager_unregister_object (GtkObject *object)
{
	Nautilus_Undo_Manager manager;	
	gboolean success;
/*
	GList *list;
	int index, length;
	Nautilus_Undo_Transaction transaction;
	CORBA_Environment ev;

  	CORBA_exception_init (&ev);
*/	

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
	/* FIXME: We need a way to do this */
#if 0
	length = g_list_length (manager->details->undo_list);
	for (index = 0; index < length; index++) {
		list = g_list_nth (manager->details->undo_list, index);
		if (list) {
			transaction = list->data;
			if (nautilus_undo_transaction_contains_object (transaction, object)) {
				manager->details->undo_list =
					g_list_remove (manager->details->undo_list, transaction);
					
				Nautilus_Undo_Transaction_unref (transaction, &ev);
			
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

				Nautilus_Undo_Transaction_unref (transaction, &ev);

				index--;
				success = TRUE;
			}
		}
	}
#endif
	
	if (success) {
		/* Fire off signal informing that a transaction has occurred */
		gtk_signal_emit (GTK_OBJECT (manager),
				 signals[UNDO_TRANSACTION_OCCURRED]);
	}
	
	return success;
}

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
	gtk_signal_emit (GTK_OBJECT (manager),
			 signals[UNDO_TRANSACTION_OCCURRED]);
}

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

Nautilus_Undo_Manager
nautilus_get_undo_manager (GtkObject *start_object)
{
	Nautilus_Undo_Manager manager;
	GtkWidget *parent;
	GtkWindow *transient_parent;

	if (start_object == NULL) {
		return NULL;
	}

	g_return_val_if_fail (GTK_IS_OBJECT (start_object), NULL);

	/* Check for an undo manager right here. */
	manager = gtk_object_get_data (start_object, "Nautilus undo");
	if (manager != NULL) {
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
			if (transient_parent != NULL) {
				manager = nautilus_get_undo_manager (GTK_OBJECT (transient_parent));
				if (manager != NULL) {
					return manager;
				}
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

static void
undo_manager_unref (gpointer manager)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	Nautilus_Undo_Manager_unref (manager, &ev);
	CORBA_exception_free (&ev);
}

void
nautilus_attach_undo_manager (GtkObject *object,
			      Nautilus_Undo_Manager manager)
{
	CORBA_Environment ev;

	g_return_if_fail (GTK_IS_OBJECT (object));

	if (manager == NULL) {
		gtk_object_remove_data (object, "Nautilus undo");
		return;
	}

	CORBA_exception_init (&ev);
	Nautilus_Undo_Manager_ref (manager, &ev);
	CORBA_exception_free (&ev);
	gtk_object_set_data_full
		(object, "Nautilus undo",
		 manager, undo_manager_unref);
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

	/* Find the undo manager. */
	control_frame = bonobo_control_get_control_frame (control);
	if (control_frame != CORBA_OBJECT_NIL) {
		CORBA_exception_init (&ev);
		undo_context = Bonobo_Control_query_interface
			(control_frame, "IDL:Nautilus/Undo/Context:1.0", &ev);
		if (undo_context != CORBA_OBJECT_NIL) {
			manager = Nautilus_Undo_Context__get_undo_manager (undo_context, &ev);
		}
		CORBA_exception_free (&ev);
	}

	/* Attach the undo manager to the widget, or detach the old one. */
	widget = bonobo_control_get_widget (control);
	nautilus_attach_undo_manager (GTK_OBJECT (widget), manager);
}

void
nautilus_undo_set_up_bonobo_control (BonoboControl *control)
{
	g_return_if_fail (BONOBO_IS_CONTROL (control));

	set_up_bonobo_control (control);
	gtk_signal_connect (GTK_OBJECT (control), "set_frame",
			    GTK_SIGNAL_FUNC (set_up_bonobo_control), NULL);
}


void
nautilus_undo_manager_stash_global_undo (Nautilus_Undo_Manager undo_manager)
{
	global_undo_manager = undo_manager;
}

Nautilus_Undo_Manager
nautilus_undo_manager_get_global_undo (void)
{
	return global_undo_manager;
}


