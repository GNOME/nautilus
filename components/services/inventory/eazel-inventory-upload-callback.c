/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2001 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */


#include <config.h>
#include "eazel-inventory-upload-callback.h"

#include "eazel-inventory-service-interface.h"
#include "eazel-inventory.h"

#include <bonobo/bonobo-main.h>
#include <gtk/gtksignal.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>

struct EazelInventoryUploadCallbackDetails {
	EazelInventory *inventory;
	EazelInventoryDoneCallback callback;
	gpointer callback_data;
};

typedef struct {
	POA_Trilobite_Eazel_InventoryUploadCallback  servant;
	EazelInventoryUploadCallback                *bonobo_object;
}  impl_POA_Trilobite_Eazel_InventoryUploadCallback;



static void
impl_Trilobite_Eazel_InventoryUploadCallback_done_uploading (PortableServer_Servant servant,
							     CORBA_boolean          succeeded,
							     CORBA_Environment     *ev);

static void
impl_Trilobite_Eazel_InventoryUploadCallback__destroy       (BonoboObject                    *object, 
							     PortableServer_Servant           servant);

static Trilobite_Eazel_InventoryUploadCallback
impl_Trilobite_Eazel_InventoryUploadCallback__create        (EazelInventoryUploadCallback    *bonobo_object,
							     CORBA_Environment               *ev);


POA_Trilobite_Eazel_InventoryUploadCallback__epv impl_Trilobite_Eazel_InventoryUploadCallback_epv =
{
	NULL,
	&impl_Trilobite_Eazel_InventoryUploadCallback_done_uploading
};


static PortableServer_ServantBase__epv base_epv;

static POA_Trilobite_Eazel_InventoryUploadCallback__vepv impl_Trilobite_Eazel_InventoryUploadCallback_vepv =
{
	&base_epv,
	NULL,
	&impl_Trilobite_Eazel_InventoryUploadCallback_epv
};



static void eazel_inventory_upload_callback_initialize_class (EazelInventoryUploadCallbackClass *klass);
static void eazel_inventory_upload_callback_initialize       (EazelInventoryUploadCallback      *server);
static void eazel_inventory_upload_callback_destroy          (GtkObject                      *object);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (EazelInventoryUploadCallback,
				   eazel_inventory_upload_callback,
				   BONOBO_OBJECT_TYPE)


     
static void
eazel_inventory_upload_callback_initialize_class (EazelInventoryUploadCallbackClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = eazel_inventory_upload_callback_destroy;
}

static void
eazel_inventory_upload_callback_initialize (EazelInventoryUploadCallback *server)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	
	server->details = g_new0 (EazelInventoryUploadCallbackDetails, 1);
	
	bonobo_object_construct
		(BONOBO_OBJECT (server),
		 impl_Trilobite_Eazel_InventoryUploadCallback__create (server, &ev));
	
	CORBA_exception_free (&ev);
}

static void
eazel_inventory_upload_callback_destroy (GtkObject *object)
{
	EazelInventoryUploadCallback *callback;

	callback = EAZEL_INVENTORY_UPLOAD_CALLBACK (object);
	g_free (callback->details);

	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}


EazelInventoryUploadCallback *
eazel_inventory_upload_callback_new (EazelInventory *inventory,
				     EazelInventoryDoneCallback callback,
				     gpointer callback_data)
{
	EazelInventoryUploadCallback *upload_callback;

	upload_callback = EAZEL_INVENTORY_UPLOAD_CALLBACK 
		(gtk_object_new (EAZEL_TYPE_INVENTORY_UPLOAD_CALLBACK, NULL));

	upload_callback->details->inventory = inventory;
	upload_callback->details->callback = callback;
	upload_callback->details->callback_data = callback_data;
	gtk_object_ref (GTK_OBJECT (upload_callback)); 
	gtk_object_sink (GTK_OBJECT (upload_callback)); 

	return upload_callback;
}





static void
impl_Trilobite_Eazel_InventoryUploadCallback_done_uploading (PortableServer_Servant servant,
							     CORBA_boolean succeeded,
							     CORBA_Environment *ev)
{
	impl_POA_Trilobite_Eazel_InventoryUploadCallback *callback_servant;
	
	callback_servant = (impl_POA_Trilobite_Eazel_InventoryUploadCallback *) servant;

	(*callback_servant->bonobo_object->details->callback)
		(callback_servant->bonobo_object->details->inventory, 
		 succeeded, 
		 callback_servant->bonobo_object->details->callback_data);

	gtk_object_unref (GTK_OBJECT (callback_servant->bonobo_object));
}

static void
impl_Trilobite_Eazel_InventoryUploadCallback__destroy (BonoboObject *object, 
						       PortableServer_Servant servant)
{
	PortableServer_ObjectId *object_id;
	CORBA_Environment ev;
	
	CORBA_exception_init (&ev);
	
	object_id = PortableServer_POA_servant_to_id (bonobo_poa (), servant, &ev);
	PortableServer_POA_deactivate_object (bonobo_poa (), object_id, &ev);
	CORBA_free (object_id);

	object->servant = NULL;
	
	POA_Trilobite_Eazel_InventoryUploadCallback__fini (servant, &ev);
	g_free (servant);

	CORBA_exception_free (&ev);
}

static Trilobite_Eazel_InventoryUploadCallback
impl_Trilobite_Eazel_InventoryUploadCallback__create (EazelInventoryUploadCallback *bonobo_object,
						      CORBA_Environment            *ev)
{
	impl_POA_Trilobite_Eazel_InventoryUploadCallback *servant;
	
	impl_Trilobite_Eazel_InventoryUploadCallback_vepv.Bonobo_Unknown_epv = bonobo_object_get_epv ();

	servant = g_new0 (impl_POA_Trilobite_Eazel_InventoryUploadCallback, 1);
	servant->servant.vepv = &impl_Trilobite_Eazel_InventoryUploadCallback_vepv;
	POA_Trilobite_Eazel_InventoryUploadCallback__init ((PortableServer_Servant) servant, ev);

	gtk_signal_connect (GTK_OBJECT (bonobo_object), "destroy",
			    GTK_SIGNAL_FUNC (impl_Trilobite_Eazel_InventoryUploadCallback__destroy), servant);
	
	servant->bonobo_object = bonobo_object;
	return bonobo_object_activate_servant (BONOBO_OBJECT (bonobo_object), servant);
}
