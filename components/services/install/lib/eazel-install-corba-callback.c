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
#include "eazel-install-corba-callback.h"
#include "eazel-install-corba-types.h"
#include "eazel-install-xml-package-list.h"
/*
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include <libtrilobite/libtrilobite.h>
#include "trilobite-eazel-install.h"
*/

#define OAF_ID "OAFIID:trilobite_eazel_install_service:8ff6e815-1992-437c-9771-d932db3b4a17"

enum {
	DOWNLOAD_PROGRESS,
	PREFLIGHT_CHECK,
	INSTALL_PROGRESS,
	UNINSTALL_PROGRESS,

	DOWNLOAD_FAILED,
	INSTALL_FAILED,
	UNINSTALL_FAILED,

	DEPENDENCY_CHECK,
	DELETE_FILES,

	DONE,
	
	LAST_SIGNAL
};

/* The signal array, used for building the signal bindings */

static guint signals[LAST_SIGNAL] = { 0 };

static BonoboObjectClass *eazel_install_callback_parent_class;

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

typedef struct {
	POA_Trilobite_Eazel_InstallCallback poa;
	EazelInstallCallback *object;
} impl_POA_Trilobite_Eazel_InstallCallback;

static void
impl_download_progress (impl_POA_Trilobite_Eazel_InstallCallback *servant,
			const char *name,
			const CORBA_long amount,
			const CORBA_long total,
			CORBA_Environment * ev)
{
	gtk_signal_emit (GTK_OBJECT (servant->object), signals[DOWNLOAD_PROGRESS], name, amount, total);
}

static void
impl_preflight_check (impl_POA_Trilobite_Eazel_InstallCallback *servant,
		      const CORBA_long total_bytes,
		      const CORBA_long total_packages,
		      CORBA_Environment * ev)
{
	gtk_signal_emit (GTK_OBJECT (servant->object), signals[PREFLIGHT_CHECK], total_bytes, total_packages);
}

static void
impl_download_failed (impl_POA_Trilobite_Eazel_InstallCallback *servant,
		      const char *name,
		      CORBA_Environment * ev)
{
	gtk_signal_emit (GTK_OBJECT (servant->object), signals[DOWNLOAD_FAILED], name);
}

static void 
impl_dep_check (impl_POA_Trilobite_Eazel_InstallCallback *servant,
		const Trilobite_Eazel_PackageDataStruct *corbapack,
		const Trilobite_Eazel_PackageDataStruct *corbaneeds, 
		CORBA_Environment * ev)
{
	PackageData *pack, *needs;
	pack = packagedata_from_corba_packagedatastruct (*corbapack);
	needs = packagedata_from_corba_packagedatastruct (*corbaneeds);
	gtk_signal_emit (GTK_OBJECT (servant->object), signals[DEPENDENCY_CHECK], pack, needs);
}

static void 
impl_install_progress (impl_POA_Trilobite_Eazel_InstallCallback *servant,
		       const Trilobite_Eazel_PackageDataStruct *corbapack,
		       const CORBA_long package_num, const CORBA_long num_packages, 
		       const CORBA_long package_size_completed, const CORBA_long package_size_total,
		       const CORBA_long total_size_completed, const CORBA_long total_size,
		       CORBA_Environment * ev) 
{
	PackageData *pack;
	pack = packagedata_from_corba_packagedatastruct (*corbapack);
	gtk_signal_emit (GTK_OBJECT (servant->object), signals[INSTALL_PROGRESS], 
			 pack,
			 package_num, num_packages,
			 package_size_completed, package_size_total,
			 total_size_completed, total_size);
}

static void 
impl_uninstall_progress (impl_POA_Trilobite_Eazel_InstallCallback *servant,
			 const Trilobite_Eazel_PackageDataStruct *corbapack,
			 const CORBA_long amount,
			 const CORBA_long total,
			 CORBA_Environment * ev)
{
	PackageData *pack;
	pack = packagedata_from_corba_packagedatastruct (*corbapack);
	gtk_signal_emit (GTK_OBJECT (servant->object), signals[UNINSTALL_PROGRESS], pack, amount, total);
}

static void 
impl_install_failed (impl_POA_Trilobite_Eazel_InstallCallback *servant,
		     const CORBA_char *xmlcorbapack,
		     CORBA_Environment * ev)
{
	GList *categories;
	PackageData *pack;
	categories = parse_memory_xml_package_list ((char*)xmlcorbapack, strlen (xmlcorbapack));
	if (categories==NULL) {
		g_warning ("install_failed called with error in xml.");
		g_warning ("XML is = \n%s", xmlcorbapack);
	} else {
		CategoryData *cat;
		cat = (CategoryData*)categories->data;
		if (cat->packages==NULL) {
			g_warning ("install_failed called with error in xml.");
			g_warning ("XML is = \n%s", xmlcorbapack);
		} else {
			PackageData *pack;
			pack = (PackageData*)cat->packages->data;
			gtk_signal_emit (GTK_OBJECT (servant->object), signals[INSTALL_FAILED], pack);
		}
	}
}

static void 
impl_uninstall_failed (impl_POA_Trilobite_Eazel_InstallCallback *servant,
		       const CORBA_char *xmlcorbapack,
		       CORBA_Environment * ev)
{
	GList *categories;
	PackageData *pack;
	categories = parse_memory_xml_package_list ((char*)xmlcorbapack, strlen (xmlcorbapack));
	if (categories==NULL) {
		g_warning ("install_failed called with error in xml.");
		g_warning ("XML is = \n%s", xmlcorbapack);
	} else {
		CategoryData *cat;
		cat = (CategoryData*)categories->data;
		if (cat->packages==NULL) {
			g_warning ("install_failed called with error in xml.");
			g_warning ("XML is = \n%s", xmlcorbapack);
		} else {
			PackageData *pack;
			pack = (PackageData*)cat->packages->data;
			gtk_signal_emit (GTK_OBJECT (servant->object), signals[INSTALL_FAILED], pack);
		}
	}
}

static CORBA_boolean
impl_delete_files (impl_POA_Trilobite_Eazel_InstallCallback *servant,
		   CORBA_Environment *ev)
{
	gboolean result;

	gtk_signal_emit (GTK_OBJECT (servant->object), signals[DELETE_FILES], &result);
	return (CORBA_boolean)result;
}

static void 
impl_done (impl_POA_Trilobite_Eazel_InstallCallback *servant, 
	   CORBA_Environment * ev)
{
	gtk_signal_emit (GTK_OBJECT (servant->object), signals[DONE]);
}

POA_Trilobite_Eazel_InstallCallback__epv* 
eazel_install_callback_get_epv () 
{
	POA_Trilobite_Eazel_InstallCallback__epv *epv;

	epv = g_new0 (POA_Trilobite_Eazel_InstallCallback__epv, 1);
	epv->download_progress   = (gpointer)&impl_download_progress;
	epv->preflight_check     = (gpointer)&impl_preflight_check;
	epv->dependency_check    = (gpointer)&impl_dep_check;
	epv->install_progress    = (gpointer)&impl_install_progress;
	epv->uninstall_progress  = (gpointer)&impl_uninstall_progress;
	epv->install_failed      = (gpointer)&impl_install_failed;
	epv->download_failed     = (gpointer)&impl_download_failed;
	epv->uninstall_failed    = (gpointer)&impl_uninstall_failed;
	epv->delete_files	 = (gpointer)&impl_delete_files;
	epv->done                = (gpointer)&impl_done;

	return epv;
};

Trilobite_Eazel_InstallCallback
eazel_install_callback_create_corba_object (BonoboObject *service) {
	impl_POA_Trilobite_Eazel_InstallCallback *servant;
	CORBA_Environment ev;

	g_assert (service != NULL);
	g_assert (IS_EAZEL_INSTALL_CALLBACK (service));
	
	CORBA_exception_init (&ev);
	
	servant = g_new0 (impl_POA_Trilobite_Eazel_InstallCallback,1);
	servant->object = EAZEL_INSTALL_CALLBACK (service);

	((POA_Trilobite_Eazel_InstallCallback*) servant)->vepv = 
		EAZEL_INSTALL_CALLBACK_CLASS ( GTK_OBJECT (service)->klass)->servant_vepv;
	POA_Trilobite_Eazel_InstallCallback__init (servant, &ev);
	ORBIT_OBJECT_KEY (((POA_Trilobite_Eazel_InstallCallback*)servant)->_private)->object = NULL;

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot instantiate Eazel_InstallCallback corba object");
		g_free (servant);
		CORBA_exception_free (&ev);		
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);		

	/* Return the bonobo activation of the servant */
	return (Trilobite_Eazel_InstallCallback) bonobo_object_activate_servant (service, servant);
}


/*****************************************
  GTK+ object stuff
*****************************************/

void
eazel_install_callback_destroy (GtkObject *object)
{
	EazelInstallCallback *service;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_INSTALL_CALLBACK (object));
	
	g_message ("in eazel_install_callback_destroy");

	service = EAZEL_INSTALL_CALLBACK (object);

	if (service->installservice_corba != CORBA_OBJECT_NIL) {
		CORBA_Environment ev;
		CORBA_exception_init (&ev);

		bonobo_object_unref (BONOBO_OBJECT (service->installservice_bonobo));
		Bonobo_Unknown_unref (service->installservice_corba, &ev); 
		CORBA_Object_release (service->installservice_corba, &ev); 

		CORBA_Object_release (service->cb, &ev);
		CORBA_exception_free (&ev);		
	}

	if (GTK_OBJECT_CLASS (eazel_install_callback_parent_class)->destroy) {
		GTK_OBJECT_CLASS (eazel_install_callback_parent_class)->destroy (object);
	}
	g_message ("out eazel_install_callback_destroy");
}

static void
eazel_install_callback_class_initialize (EazelInstallCallbackClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->destroy = (void(*)(GtkObject*))eazel_install_callback_destroy;

	eazel_install_callback_parent_class = gtk_type_class (bonobo_object_get_type ());
	klass->servant_vepv = g_new0 (POA_Trilobite_Eazel_InstallCallback__vepv,1);
	((POA_Trilobite_Eazel_InstallCallback__vepv*)klass->servant_vepv)->_base_epv = &base_epv; 
	((POA_Trilobite_Eazel_InstallCallback__vepv*)klass->servant_vepv)->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	((POA_Trilobite_Eazel_InstallCallback__vepv*)klass->servant_vepv)->Trilobite_Eazel_InstallCallback_epv = 
		eazel_install_callback_get_epv ();

	signals[DOWNLOAD_PROGRESS] = 
		gtk_signal_new ("download_progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallCallbackClass, download_progress),
				gtk_marshal_NONE__POINTER_INT_INT,
				GTK_TYPE_NONE, 3, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);
	signals[PREFLIGHT_CHECK] = 
		gtk_signal_new ("preflight_check",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallCallbackClass, preflight_check),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);
	signals[INSTALL_PROGRESS] = 
		gtk_signal_new ("install_progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallCallbackClass, install_progress),
				eazel_install_gtk_marshal_NONE__POINTER_INT_INT_INT_INT_INT_INT,
				GTK_TYPE_NONE, 7, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT);
	signals[UNINSTALL_PROGRESS] = 
		gtk_signal_new ("uninstall_progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallCallbackClass, uninstall_progress),
				gtk_marshal_NONE__POINTER_INT_INT,
				GTK_TYPE_NONE, 3, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);
	signals[DOWNLOAD_FAILED] = 
		gtk_signal_new ("download_failed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallCallbackClass, download_failed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	signals[INSTALL_FAILED] = 
		gtk_signal_new ("install_failed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallCallbackClass, install_failed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	signals[UNINSTALL_FAILED] = 
		gtk_signal_new ("uninstall_failed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallCallbackClass, uninstall_failed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	signals[DEPENDENCY_CHECK] = 
		gtk_signal_new ("dependency_check",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallCallbackClass, dependency_check),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_POINTER);
	signals[DELETE_FILES] =
		gtk_signal_new ("delete_files",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallCallbackClass, delete_files),
				gtk_marshal_BOOL__NONE,
				GTK_TYPE_BOOL, 0);
	signals[DONE] = 
		gtk_signal_new ("done",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallCallbackClass, done),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
eazel_install_callback_initialize (EazelInstallCallback *service) {
	/* g_message ("in eazel_install_callback_initialize"); */
	CORBA_Environment ev;

	g_assert (service != NULL);
	g_assert (IS_EAZEL_INSTALL_CALLBACK (service));

	CORBA_exception_init (&ev);
	service->cb = eazel_install_callback_create_corba_object (BONOBO_OBJECT (service));

	service->installservice_bonobo = bonobo_object_activate (OAF_ID, 0);
	if ( !service->installservice_bonobo) {
		g_error ("Cannot activate %s\n", OAF_ID);
	}
	if (! bonobo_object_client_has_interface (service->installservice_bonobo, "IDL:Trilobite/Service:1.0", &ev)) {
		g_error ("Object does not support IDL:Trilobite/Service:1.0");
	}
	if (! bonobo_object_client_has_interface (service->installservice_bonobo, "IDL:Trilobite/Eazel/Install:1.0", &ev)) {
		g_error ("Object does not support IDL:Trilobite/Eazel/Time:1.0");
	}
	service->installservice_corba = bonobo_object_query_interface (BONOBO_OBJECT (service->installservice_bonobo),
								       "IDL:Trilobite/Eazel/Install:1.0");

	/* This sets the bonobo structures in service using the corba object */
	if (!bonobo_object_construct (BONOBO_OBJECT (service), service->cb)) {
		g_warning ("bonobo_object_construct failed");
	}	
	CORBA_exception_free (&ev);		
}

GtkType
eazel_install_callback_get_type() {
	static GtkType service_type = 0;

	/* g_message ("into eazel_install_callback_get_type");  */

	/* First time it's called ? */
	if (!service_type)
	{
		static const GtkTypeInfo service_info =
		{
			"EazelInstallCallback",
			sizeof (EazelInstallCallback),
			sizeof (EazelInstallCallbackClass),
			(GtkClassInitFunc) eazel_install_callback_class_initialize,
			(GtkObjectInitFunc) eazel_install_callback_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};
		
		service_type = gtk_type_unique (bonobo_object_get_type (), &service_info);
	}

	return service_type;
}

EazelInstallCallback*
eazel_install_callback_new()
{
	EazelInstallCallback *service;

	service = EAZEL_INSTALL_CALLBACK (gtk_object_new (TYPE_EAZEL_INSTALL_CALLBACK, NULL));

	return service;
}

Trilobite_Eazel_Install 
eazel_install_callback_corba_objref (EazelInstallCallback *service)
{
	return service->installservice_corba;
}

BonoboObjectClient*
eazel_install_callback_bonobo (EazelInstallCallback *service)
{
	return service->installservice_bonobo;
}

void 
eazel_install_callback_install_packages (EazelInstallCallback *service, 
					 GList *categories,
					 CORBA_Environment *ev)
{
	Trilobite_Eazel_CategoryStructList *corbacats;
	corbacats = corba_category_list_from_categorydata_list (categories);
	Trilobite_Eazel_Install_install_packages (service->installservice_corba, 
						  corbacats, 
						  service->cb, 
						  ev);
}

void 
eazel_install_callback_uninstall_packages (EazelInstallCallback *service, 
					   GList *categories,
					   CORBA_Environment *ev)
{
	Trilobite_Eazel_CategoryStructList *corbacats;
	corbacats = corba_category_list_from_categorydata_list (categories);
	Trilobite_Eazel_Install_uninstall_packages (service->installservice_corba, 
						    corbacats, 
						    service->cb, 
						    ev);
}

GList*
eazel_install_callback_simple_query (EazelInstallCallback *service, 
				     const char *query,
				     CORBA_Environment *ev)
{
	GList *result;
	Trilobite_Eazel_PackageDataStructList *corbares;
	
	corbares = Trilobite_Eazel_Install_simple_query (service->installservice_corba,
							 query,
							 ev);
	result = packagedata_list_from_corba_packagedatastructlist (*corbares);
	CORBA_free (corbares); 
	
	return result;
}

void 
eazel_install_callback_revert_transaction (EazelInstallCallback *service, 
					   const char *xmlfile,
					   CORBA_Environment *ev)
{
	xmlDocPtr doc;
	xmlChar *mem;
	int size;
	CORBA_char *arg;
	
	doc = xmlParseFile (xmlfile);
	xmlDocDumpMemory (doc, &mem, &size);
	arg = CORBA_string_dup (mem);
	Trilobite_Eazel_Install_revert_transaction (service->installservice_corba, 
						    arg,
						    service->cb,
						    ev);
	CORBA_free (arg);
	g_free (mem);
	xmlFreeDoc (doc);
}
