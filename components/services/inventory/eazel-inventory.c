/* -*- Mode: C; tab-width: 8; indent-tabs-mode: 8; c-basic-offset: 8 -*- */

/*
 * Copyright (C) 2001 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <config.h>
#include "eazel-inventory.h"

#include "eazel-inventory-upload-callback.h"
#include "eazel-inventory-service-interface.h"
#include <eel/eel-gtk-macros.h>
#include <bonobo/bonobo-object-client.h>

#define EAZEL_INVENTORY_IID "OAFIID:trilobite_inventory_service:eaae1152-1551-43d5-a764-52274131a9d5"

struct EazelInventoryDetails {
        Trilobite_Eazel_Inventory corba_inventory;
};


static void eazel_inventory_initialize_class (EazelInventoryClass *klass);
static void eazel_inventory_initialize       (EazelInventory      *factory);
static void eazel_inventory_destroy          (GtkObject                            *object);


EEL_DEFINE_CLASS_BOILERPLATE (EazelInventory, eazel_inventory, GTK_TYPE_OBJECT)

static void
eazel_inventory_initialize_class  (EazelInventoryClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass*) klass;
	object_class->destroy = eazel_inventory_destroy;
}

static void
eazel_inventory_initialize (EazelInventory *factory)
{
	BonoboObjectClient *object_client;

	factory->details = g_new0 (EazelInventoryDetails, 1);

	object_client = bonobo_object_activate (EAZEL_INVENTORY_IID, 0);
	if (object_client != NULL) {
		factory->details->corba_inventory = bonobo_object_query_interface 
			(BONOBO_OBJECT (object_client), "IDL:Trilobite/Eazel/Inventory:1.0");
		bonobo_object_unref (BONOBO_OBJECT (object_client)); 
	}
}


static EazelInventory *global_eazel_inventory = NULL;

static void
eazel_inventory_destroy (GtkObject *object)
{
        EazelInventory *inventory;

	inventory = EAZEL_INVENTORY (object);

	if (inventory->details->corba_inventory != CORBA_OBJECT_NIL) {
		bonobo_object_release_unref (inventory->details->corba_inventory, NULL);
	}

	g_free (inventory->details);

	global_eazel_inventory = NULL;
}


EazelInventory *
eazel_inventory_get (void)
{
	if (global_eazel_inventory == NULL) {
		global_eazel_inventory = EAZEL_INVENTORY
			(gtk_object_new (EAZEL_TYPE_INVENTORY, NULL));
 		gtk_object_ref (GTK_OBJECT (global_eazel_inventory));
		gtk_object_sink (GTK_OBJECT (global_eazel_inventory));
		if (global_eazel_inventory->details->corba_inventory == CORBA_OBJECT_NIL) {
		         gtk_object_unref (GTK_OBJECT (global_eazel_inventory));
		         global_eazel_inventory = NULL;
		}
	} else {
 		gtk_object_ref (GTK_OBJECT (global_eazel_inventory));
	}



	return global_eazel_inventory;
}

gboolean
eazel_inventory_get_enabled (EazelInventory *inventory)
{
	gboolean enabled;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	enabled = Trilobite_Eazel_Inventory__get_enabled (inventory->details->corba_inventory,
							  &ev);
	CORBA_exception_free (&ev);
							  
	return enabled;
}

void
eazel_inventory_set_enabled (EazelInventory *inventory,
			     gboolean        enabled)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	Trilobite_Eazel_Inventory__set_enabled (inventory->details->corba_inventory,
						enabled,
						&ev);
	CORBA_exception_free (&ev);
}


char *
eazel_inventory_get_machine_id (EazelInventory *inventory)
{
	CORBA_char *corba_id;
	gchar *g_id;
	CORBA_Environment ev;

	CORBA_exception_init (&ev);
	corba_id = Trilobite_Eazel_Inventory__get_machine_id (inventory->details->corba_inventory,
							      &ev);
	CORBA_exception_free (&ev);
							  
	g_id = g_strdup (corba_id);
	CORBA_free (corba_id);
	
	return g_id;
}


void
eazel_inventory_upload (EazelInventory *inventory,
			EazelInventoryDoneCallback done_callback,
			gpointer callback_data)
{
	EazelInventoryUploadCallback *callback;
	Trilobite_Eazel_InventoryUploadCallback corba_callback;
	CORBA_Environment ev;

	callback = eazel_inventory_upload_callback_new (inventory,
							done_callback,
							callback_data);

	corba_callback = bonobo_object_corba_objref (BONOBO_OBJECT (callback));

	CORBA_exception_init (&ev);
	Trilobite_Eazel_Inventory_upload (inventory->details->corba_inventory,
					  corba_callback,
					  &ev);
	CORBA_Object_release (corba_callback, &ev);
	CORBA_exception_free (&ev);
}
