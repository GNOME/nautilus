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
static void 	impl_Nautilus_Undo_Manager__append 	(impl_POA_Nautilus_Undo_Manager  *servant,
							 const Nautilus_Undo_Transaction transaction,
							 CORBA_Environment *ev);
static void 	impl_Nautilus_Undo_Manager__forget 	(impl_POA_Nautilus_Undo_Manager  *servant,
							 const Nautilus_Undo_Transaction transaction,
							 CORBA_Environment *ev);
static void 	impl_Nautilus_Undo_Manager__undo 	(impl_POA_Nautilus_Undo_Manager  *servant,
							 CORBA_Environment *ev);


static void 		nautilus_undo_manager_add_transaction 	(NautilusUndoManager *manager, 
							 	 Nautilus_Undo_Transaction transaction);
static void 		nautilus_undo_manager_undo		(NautilusUndoManager *manager);

NAUTILUS_DEFINE_CLASS_BOILERPLATE(NautilusUndoManager, nautilus_undo_manager, BONOBO_OBJECT_TYPE)

POA_Nautilus_Undo_Manager__epv libnautilus_Nautilus_Undo_Manager_epv =
{
	NULL,			/* _private */
	(gpointer) &impl_Nautilus_Undo_Manager__append,
	(gpointer) &impl_Nautilus_Undo_Manager__forget,
	(gpointer) &impl_Nautilus_Undo_Manager__undo,
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

  	servant->gtk_object = manager;

  	retval = bonobo_object_activate_servant (BONOBO_OBJECT (manager), servant);

  	gtk_signal_connect (GTK_OBJECT (manager), "destroy", GTK_SIGNAL_FUNC (impl_Nautilus_Undo_Manager__destroy), servant);

  	return retval;
}


static void
impl_Nautilus_Undo_Manager__append (impl_POA_Nautilus_Undo_Manager *servant,
					      const Nautilus_Undo_Transaction undo_transaction,
					      CORBA_Environment *ev)
{
	NautilusUndoManager *manager;

	g_assert (NAUTILUS_IS_UNDO_MANAGER (servant->gtk_object));
	manager = NAUTILUS_UNDO_MANAGER (servant->gtk_object);

	Bonobo_Unknown_ref (undo_transaction, ev);
	nautilus_undo_manager_add_transaction (manager, undo_transaction);
}

static void
impl_Nautilus_Undo_Manager__forget (impl_POA_Nautilus_Undo_Manager *servant,
			            const Nautilus_Undo_Transaction transaction,
				    CORBA_Environment *ev)
{
	NautilusUndoManager *manager;

	g_assert (NAUTILUS_IS_UNDO_MANAGER (servant->gtk_object));
	manager = NAUTILUS_UNDO_MANAGER (servant->gtk_object);
}

static void 	
impl_Nautilus_Undo_Manager__undo (impl_POA_Nautilus_Undo_Manager  *servant, CORBA_Environment *ev)
{
	NautilusUndoManager *manager;

	g_assert (NAUTILUS_IS_UNDO_MANAGER (servant->gtk_object));
	manager = NAUTILUS_UNDO_MANAGER (servant->gtk_object);

	nautilus_undo_manager_undo (manager);
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
	
	bonobo_object_construct (BONOBO_OBJECT (manager), impl_Nautilus_Undo_Manager__create (manager, &ev));

  	CORBA_exception_free(&ev);
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


/* nautilus_undo_manager_undo_last_transaction */
static void 
nautilus_undo_manager_undo (NautilusUndoManager *manager)
{
	GList *list;
	CORBA_Object undo_transaction;
	CORBA_Environment ev;

  	CORBA_exception_init(&ev);

	/* Verify we have a transaction to be undone */
	if (manager->details->undo_list == NULL) {
		g_warning("NautilusUndoManager has no transaction to be undone.");
		return;
	}

	/* Pop last transaction off undo list */
	list = g_list_last(manager->details->undo_list);
	g_assert(list);
	undo_transaction = list->data;
	manager->details->undo_list = g_list_remove (manager->details->undo_list, list->data);

	/* Undo transaction */
	Nautilus_Undo_Transaction_undo (undo_transaction, &ev);
	
	/* Place transaction into redo list */
	if (manager->details->enable_redo) {
		/* nautilus_undo_manager_add_redo_transaction (undo_transaction); */
	} else {
		/* Purge transaction */
		Bonobo_Unknown_unref (undo_transaction, &ev);
		CORBA_Object_release (undo_transaction, &ev);
	}

	/* Fire off signal informing that an undo transaction has occurred */
	gtk_signal_emit (GTK_OBJECT (manager), signals[UNDO_TRANSACTION_OCCURED]);

	CORBA_exception_free(&ev);
}

#if 0
/* nautilus_undo_manager_redo_last_undone_transaction */
static void 
nautilus_undo_manager_redo (NautilusUndoManager *manager)
{		
	GList *list;
	CORBA_Object redo_transaction;
	CORBA_Object undo_transaction;
	CORBA_Environment ev;

  	CORBA_exception_init(&ev);

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
	list = g_list_last (manager->details->redo_list);
	g_assert(list);
	redo_transaction = list->data;

	Nautilus_Undo_Transaction_undo (redo_transaction, &ev);

	/* Place transaction into undo list */
	undo_transaction = bonobo_object_corba_objref (BONOBO_OBJECT (redo_transaction));
	Bonobo_Unknown_ref (undo_transaction, &ev);
	nautilus_undo_manager_add_transaction (manager, undo_transaction);

	CORBA_exception_free(&ev);
}
#endif

/* nautilus_undo_manager_add_undo_transaction */
static void 
nautilus_undo_manager_add_transaction (NautilusUndoManager *manager, Nautilus_Undo_Transaction transaction)
{
	int length;
	
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
	Nautilus_Undo_Manager manager;	
	gboolean success;
/*
	GList *list;
	int index, length;
	Nautilus_Undo_Transaction transaction;
*/	
	CORBA_Environment ev;

  	CORBA_exception_init(&ev);

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
					
				Bonobo_Unknown_unref (transaction, &ev);
				CORBA_Object_release (transaction, &ev);
			
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

				Bonobo_Unknown_unref (transaction, &ev);
				CORBA_Object_release (transaction, &ev);

				index--;
				success = TRUE;
			}
		}
	}
#endif
	
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
	Nautilus_Undo_Transaction transaction;
	CORBA_Environment ev;

	CORBA_exception_init(&ev);

	length = g_list_length (list);		
	for (index = 0; index < length; index++) {
		list = g_list_last (list);
		if (list) {
			transaction = list->data;
			list = g_list_remove (list, transaction);

			Bonobo_Unknown_unref (transaction, &ev);
			CORBA_Object_release (transaction, &ev);
		}
	}

	CORBA_exception_free(&ev);

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
	Nautilus_Undo_Transaction transaction;
	CORBA_Environment ev;

	CORBA_exception_init(&ev);

	for (index = 0; index < items; index++) {
		list = g_list_first (list);
		if (list) {
			transaction = list->data;
			list = g_list_remove (list, transaction);

			Bonobo_Unknown_unref (transaction, &ev);
			CORBA_Object_release (transaction, &ev);
		}
	}

	CORBA_exception_free(&ev);

	return list;
}

/* FIXME: This should return a Nautilus_Undo_Manager in the long run.
 * And it's more likely that we'll want this in the transaction code
 * than in here so it will probably be moved.
 */
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
			      Nautilus_Undo_Manager manager)
{
	g_return_if_fail (GTK_IS_OBJECT (object));

	if (manager == NULL) {
		gtk_object_remove_data (object, "Nautilus undo");
	}

	/* FIXME:  Do we need to ref here? */
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




/*
 * Locates an undo manger for this bonobo control which is supplied by an interface on the control frame
 */

void
nautilus_undo_setup_bonobo_control (BonoboControl *control)
{
	/*
	NautilusUndoContext *undo_context;
	Nautilus_Application app;
	Nautilus_ViewWindow window;
	Nautilus_Undo_Manager manager;

	g_message ("nautilus_view_frame_ensure_view_frame");

	g_assert (view->private->view_frame);
	window = Nautilus_ViewFrame__get_main_window (view->private->view_frame, &ev);
	g_assert (window);

	app = Nautilus_ViewWindow__get_application (window, &ev);
	g_assert (app);

	manager = bonobo_object_query_interface (BONOBO_OBJECT (app), "IDL:Nautilus/Undo/Manager:1.0");
	g_assert (manager);
	 
	undo_context = nautilus_undo_context_new (manager);
	*/
}

/*
static void set_frame ()
{

}
*/
