/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
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
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

#include <config.h>
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include <ghttp.h>

#include <libgnomevfs/gnome-vfs.h>

#define DEBUG(X...) g_print("eazel-inventory-service: " X)


#include <gconf/gconf-client.h>

#include <libtrilobite/libtrilobite.h>
#include <libtrilobite/libtrilobite-service.h>

#include <libnautilus-extensions/nautilus-file-utilities.h>

/* FIXME: crack */
/*#include <libammonite/libammonite.h>*/
#include <libtrilobite/libammonite.h>

#include "eazel-inventory-utils.h"

#include "eazel-inventory-service.h"
#include "eazel-inventory-service-interface.h"

#define KEY_GCONF_EAZEL_INVENTORY_ENABLED "/apps/eazel-trilobite/inventory/enabled"

#define EAZEL_INVENTORY_UPLOAD_URI "eazel-services:/inventory/upload"

#define UPLOAD_POST_PREFIX "_inventory.xml="

/* FIXME: hook gconf signals so that if the values are changed externally we contact the server... */


/* This is the parent class pointer */

static BonoboObjectClass *eazel_inventory_service_parent_class;

/*****************************************
  Corba stuff
*****************************************/

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

/*
  This is POA_Trilobite_Eazel_Inventory structure we will use,
  as it will let us access the EazelInventoryService object in 
  the corba methods
 */
typedef struct {
	POA_Trilobite_Eazel_Inventory poa;
	EazelInventoryService *object;
} impl_POA_Trilobite_Eazel_Inventory;

struct EazelInventoryServiceDetails {
	GConfClient *gconf_client;
};

static gboolean
get_enabled (EazelInventoryService *service) {
	return gconf_client_get_bool (service->details->gconf_client, KEY_GCONF_EAZEL_INVENTORY_ENABLED, NULL);
	/* FIXME: handle gconf errors */
}

static CORBA_boolean
impl_Trilobite_Eazel_Inventory__get_enabled (PortableServer_Servant servant,
					     CORBA_Environment     *ev) 
{
	impl_POA_Trilobite_Eazel_Inventory *service; 

	service = (impl_POA_Trilobite_Eazel_Inventory *) servant;
	return get_enabled (service->object);
}

static void
impl_Trilobite_Eazel_Inventory__set_enabled (PortableServer_Servant servant,
					     CORBA_boolean          enabled,
					     CORBA_Environment     *ev) 
{
	impl_POA_Trilobite_Eazel_Inventory *service; 

	service = (impl_POA_Trilobite_Eazel_Inventory *) servant;
                                             
	gconf_client_set_bool (service->object->details->gconf_client, KEY_GCONF_EAZEL_INVENTORY_ENABLED, enabled, NULL);
	/* FIXME: handle gconf errors */
}




static CORBA_char *
impl_Trilobite_Eazel_Inventory__get_machine_id (PortableServer_Servant servant,
						CORBA_Environment     *ev) 
{
	gchar *g_machine_name;
	CORBA_char *c_machine_name;
	impl_POA_Trilobite_Eazel_Inventory *service; 

	service = (impl_POA_Trilobite_Eazel_Inventory *) servant;

	g_machine_name = ammonite_get_machine_id ();

	c_machine_name = CORBA_string_dup (g_machine_name);

	g_free (g_machine_name);
	return c_machine_name;
}


static void
impl_Trilobite_Eazel_Inventory_upload (PortableServer_Servant servant,
				       Trilobite_Eazel_InventoryUploadCallback callback,
				       CORBA_Environment *caller_ev) 
{
	AmmoniteError error;
	char *url, *partial_url;
	gboolean do_upload;
        ghttp_request* request;
	ghttp_status status;
	GnomeVFSResult result;
	int file_size;
	char *file_contents;
	char *file_contents_good;
	char *escaped;
	char *body;
	char *path;
	CORBA_Environment ev;
	impl_POA_Trilobite_Eazel_Inventory *service; 

	service = (impl_POA_Trilobite_Eazel_Inventory *) servant;

	CORBA_exception_init (&ev);


	if (!get_enabled (service->object)) {
		/* g_print ("not enabled\n"); */
		if (! CORBA_Object_is_nil (callback, &ev)) {
			Trilobite_Eazel_InventoryUploadCallback_done_uploading (callback, CORBA_FALSE, &ev);
		}
		CORBA_exception_free (&ev);
		return;
	}

	do_upload = eazel_gather_inventory ();

	if (!do_upload) {
		if (! CORBA_Object_is_nil (callback, &ev)) {
			Trilobite_Eazel_InventoryUploadCallback_done_uploading (callback, CORBA_TRUE, &ev);
		}
		CORBA_exception_free (&ev);
		return;
	}

	/* TODO: store new MD5 */

	partial_url = NULL;
	error = ammonite_http_url_for_eazel_url (EAZEL_INVENTORY_UPLOAD_URI, &partial_url);

	/* FIXME: CRAAAAAACK */

	if (error != ERR_Success) {
#if 0
		switch (error) {
		case ERR_UserNotLoggedIn:
			g_print (_("User isn't logged into ammonite yet.\n"));
			break;
		case ERR_BadURL:
			g_print (_("The supplied URL was bad.\n"));
			break;
		case ERR_CORBA:
			g_print (_("A CORBA error occured.\n"));
			break;
		default:
			g_print (_("Ammonite returned an error translating the url.\n"));
		}
#endif

		if (! CORBA_Object_is_nil (callback, &ev)) {
			Trilobite_Eazel_InventoryUploadCallback_done_uploading (callback, CORBA_FALSE, &ev);
		}
		CORBA_exception_free (&ev);
		return;
	}


	url = g_strconcat ("http", partial_url, NULL);
	g_print ("the URI is: %s\n", url);

	request = ghttp_request_new();
	if (!request) {
                /* g_warning (_("Could not create an http request !")); */
		if (! CORBA_Object_is_nil (callback, &ev)) {
			Trilobite_Eazel_InventoryUploadCallback_done_uploading (callback, CORBA_FALSE, &ev);
		}
		CORBA_exception_free (&ev);
		return;
	}

        if (ghttp_set_uri (request, url) != 0) {
                /* g_warning (_("Invalid uri !")); */
		if (! CORBA_Object_is_nil (callback, &ev)) {
			Trilobite_Eazel_InventoryUploadCallback_done_uploading (callback, CORBA_FALSE, &ev);
		}
		CORBA_exception_free (&ev);
		return;
        }

	if (ghttp_set_type (request, ghttp_type_post) != 0) {
		/* g_warning (_("Can't post !")); */
		if (! CORBA_Object_is_nil (callback, &ev)) {
			Trilobite_Eazel_InventoryUploadCallback_done_uploading (callback, CORBA_FALSE, &ev);
		}
		CORBA_exception_free (&ev);
		return;
	}

        ghttp_set_header (request, http_hdr_Connection, "close");
        ghttp_set_header (request, http_hdr_User_Agent, trilobite_get_useragent_string (NULL));
        ghttp_set_header (request, http_hdr_Content_Type, "application/x-www-form-urlencoded");
	g_print("about to read file\n");

	path = eazel_inventory_local_path ();	
	result = nautilus_read_entire_file (path, &file_size, &file_contents);
	g_free (path);
	if (result != GNOME_VFS_OK) {
		/* g_warning(_("can't open tempory file hell\n")); */
		if (! CORBA_Object_is_nil (callback, &ev)) {
			Trilobite_Eazel_InventoryUploadCallback_done_uploading (callback, CORBA_FALSE, &ev);
		}
		CORBA_exception_free (&ev);
		return;
	}

	file_contents_good = g_malloc (file_size+1);
	memcpy (file_contents_good, file_contents, file_size);
	file_contents_good[file_size] = '\0';
	g_free (file_contents);
	file_contents = file_contents_good;

	g_print("read file\n");

	escaped = gnome_vfs_escape_string (file_contents);

	body = g_strconcat (UPLOAD_POST_PREFIX, escaped, NULL);

	g_free (file_contents);
	g_free (escaped);

	if (ghttp_set_body (request, body, strlen(body)) != 0) {
		/* g_warning (_("Can't set body !")); */
		g_free (body);
		ghttp_close (request);
		if (! CORBA_Object_is_nil (callback, &ev)) {
			Trilobite_Eazel_InventoryUploadCallback_done_uploading (callback, CORBA_FALSE, &ev);
		}
		CORBA_exception_free (&ev);
		return;
	}

	if (ghttp_prepare (request) != 0) {
                /* g_warning (_("Could not prepare http request !")); */
		g_free (body);
		ghttp_close (request);
		if (! CORBA_Object_is_nil (callback, &ev)) {
			Trilobite_Eazel_InventoryUploadCallback_done_uploading (callback, CORBA_FALSE, &ev);
		}
		CORBA_exception_free (&ev);
		return;
        }

	
	status = ghttp_process (request);

	if (status != ghttp_done) {
		/* g_print ("an error occured uploading the inventory: %s\n",
		   ghttp_get_error (request)); */
		g_free (body);
		ghttp_close (request);
		if (! CORBA_Object_is_nil (callback, &ev)) {
			Trilobite_Eazel_InventoryUploadCallback_done_uploading (callback, CORBA_FALSE, &ev);
		}
		CORBA_exception_free (&ev);
		return;
	}

	g_free (body);
	ghttp_close (request);

	/* store the new MD5 */
	eazel_inventory_update_md5 ();

	if (! CORBA_Object_is_nil (callback, &ev)) {
		Trilobite_Eazel_InventoryUploadCallback_done_uploading (callback, CORBA_TRUE, &ev);
	}
}


/*
  This creates the epv for the object.
  Basically you just have to alloc a structure of the
  appropriate type (POA_Trilobite_Eazel_Inventory__epv in 
  this case), and set the pointers for the method implementations.
 */
POA_Trilobite_Eazel_Inventory__epv* 
eazel_inventory_service_get_epv() 
{
	POA_Trilobite_Eazel_Inventory__epv *epv;

	epv = g_new0 (POA_Trilobite_Eazel_Inventory__epv, 1);

	epv->_get_enabled           = &impl_Trilobite_Eazel_Inventory__get_enabled;
	epv->_set_enabled           = &impl_Trilobite_Eazel_Inventory__set_enabled;
	epv->_get_machine_id        = &impl_Trilobite_Eazel_Inventory__get_machine_id;
	epv->upload                 = &impl_Trilobite_Eazel_Inventory_upload;
		
	return epv;
}

/*****************************************
  GTK+ object stuff
*****************************************/

/* This is the object finalize. It should clean up any
 data allocated by the object, and if possible, call 
the parent finalize */
static void
eazel_inventory_service_finalize (GtkObject *object)
{
	EazelInventoryService *service;

	g_message ("in eazel_inventory_service_finalize");

	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_INVENTORY_SERVICE (object));

	service = EAZEL_INVENTORY_SERVICE (object);

	gtk_object_unref (GTK_OBJECT (service->details->gconf_client));
	g_free (service->details);

	/* Call parents destroy */
	if (GTK_OBJECT_CLASS (eazel_inventory_service_parent_class)->finalize) {
		GTK_OBJECT_CLASS (eazel_inventory_service_parent_class)->finalize (object);
	}

	g_message ("out eazel_inventory_service_finalize");
}

void eazel_inventory_service_unref (GtkObject *object) 
{
	g_message ("eazel_inventory_service_unref");
	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_INVENTORY_SERVICE (object));
	bonobo_object_unref (BONOBO_OBJECT (object));
}

static void
eazel_inventory_service_class_initialize (EazelInventoryServiceClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->finalize = eazel_inventory_service_finalize;

	eazel_inventory_service_parent_class = gtk_type_class (bonobo_object_get_type ());

	/* Here I get allocate and set up the vepv. This ensures that the
	   servant_vepv will hold the proper bindings for the corba object for
	   the eazel_inventory_service */
	klass->servant_vepv = g_new0 (POA_Trilobite_Eazel_Inventory__vepv,1);
	((POA_Trilobite_Eazel_Inventory__vepv*)klass->servant_vepv)->_base_epv = &base_epv; 
	((POA_Trilobite_Eazel_Inventory__vepv*)klass->servant_vepv)->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	((POA_Trilobite_Eazel_Inventory__vepv*)klass->servant_vepv)->Trilobite_Eazel_Inventory_epv = eazel_inventory_service_get_epv ();

}

/*
  _corba_object does all the greasy corba building and whatnot.
 */
static Trilobite_Eazel_Inventory
eazel_inventory_service_create_corba_object (BonoboObject *service) {
	impl_POA_Trilobite_Eazel_Inventory *servant;
	CORBA_Environment ev;

	g_assert (service != NULL);
	
	CORBA_exception_init (&ev);
	
	/* Allocate the POA structure, using our extended version*/
	servant = (impl_POA_Trilobite_Eazel_Inventory*)g_new0 (PortableServer_Servant,1);

	/* Set the vepv to the vepv build in eazel_inventory_service_class_initialize */
	((POA_Trilobite_Eazel_Inventory*) servant)->vepv = EAZEL_INVENTORY_SERVICE_CLASS ( GTK_OBJECT (service)->klass)->servant_vepv;

	/* Call the __init method. This is generated by the IDL compiler and
	   the name of the method depends on the name of your corba object */
	POA_Trilobite_Eazel_Inventory__init (servant, &ev);
	
	/* Magic */
	ORBIT_OBJECT_KEY (((POA_Trilobite_Eazel_Inventory*)servant)->_private)->object = NULL;

	/* Check to see if things went well */
	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot instantiate Trilobite_Eazel_Inventory corba object");
		g_free (servant);
		CORBA_exception_free (&ev);		
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);		

	/* Return the bonobo activation of the servant */
	return (Trilobite_Eazel_Inventory) bonobo_object_activate_servant (service, servant);
}

/*
  This is the EazelInventoryService instance initializer.
  Its responsibility is to create the corba object and 
  build the bonobo_object structures using the corba object.
 */
static void
eazel_inventory_service_initialize (EazelInventoryService *service) {
	Trilobite_Eazel_Inventory corba_service;

	g_assert (service != NULL);
	g_assert (EAZEL_IS_INVENTORY_SERVICE (service));

	service->details = g_new0 (EazelInventoryServiceDetails, 1);
	service->details->gconf_client = gconf_client_get_default ();

	/* This builds the corba object */
	corba_service = eazel_inventory_service_create_corba_object (BONOBO_OBJECT (service));

	/* This sets the bonobo structures in service using the corba object */
	if (!bonobo_object_construct (BONOBO_OBJECT (service), corba_service)) {
		g_warning ("bonobo_object_construct failed");
	}	
}

GtkType
eazel_inventory_service_get_type() {
	static GtkType eazel_inventory_service_type = 0;

	g_message ("into eazel_inventory_service_get_type"); 

	/* First time it's called ? */
	if (!eazel_inventory_service_type)
	{
		static const GtkTypeInfo eazel_inventory_service_info =
		{
			"EazelInventroyService",
			sizeof (EazelInventoryService),
			sizeof (EazelInventoryServiceClass),
			(GtkClassInitFunc) eazel_inventory_service_class_initialize,
			(GtkObjectInitFunc) eazel_inventory_service_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		/* Get a unique GtkType */
		eazel_inventory_service_type = gtk_type_unique (bonobo_object_get_type (), &eazel_inventory_service_info);
	}

	return eazel_inventory_service_type;
}

/*
  The _new method simply builds the service
  using gtk_object_new
*/
EazelInventoryService*
eazel_inventory_service_new ()
{
	EazelInventoryService *service;

	g_message ("in eazel_inventory_service_new");
	
	service = EAZEL_INVENTORY_SERVICE (gtk_object_new (EAZEL_TYPE_INVENTORY_SERVICE, NULL));
	
	return service;
}


