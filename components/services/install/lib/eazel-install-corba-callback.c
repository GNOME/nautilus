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
/*
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include <libtrilobite/libtrilobite.h>
#include "trilobite-eazel-install.h"
*/

enum {
	DOWNLOAD_PROGRESS,
	INSTALL_PROGRESS,
	UNINSTALL_PROGRESS,

	DOWNLOAD_FAILED,
	INSTALL_FAILED,
	UNINSTALL_FAILED,

	DEPENDENCY_CHECK,

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
impl_download_failed (impl_POA_Trilobite_Eazel_InstallCallback *servant,
		      const char *name,
		      CORBA_Environment * ev)
{
	gtk_signal_emit (GTK_OBJECT (servant->object), signals[DOWNLOAD_FAILED], name);
}

static void 
impl_dep_check (impl_POA_Trilobite_Eazel_InstallCallback *servant,
		const Trilobite_Eazel_PackageStruct *corbapack,
		const Trilobite_Eazel_PackageDataStruct *corbaneeds, 
		CORBA_Environment * ev)
{
	PackageData *pack, *needs;
	pack = packagedata_from_corba_packagestruct (corbapack);
	needs = packagedata_from_corba_packagedatastruct (*corbaneeds);
	gtk_signal_emit (GTK_OBJECT (servant->object), signals[DEPENDENCY_CHECK], pack, needs);
}

static void 
impl_install_progress (impl_POA_Trilobite_Eazel_InstallCallback *servant,
		       const Trilobite_Eazel_PackageStruct *corbapack,
		       const CORBA_long amount,
		       const CORBA_long total,
		       CORBA_Environment * ev) 
{
	PackageData *pack;
	pack = packagedata_from_corba_packagestruct (corbapack);
	gtk_signal_emit (GTK_OBJECT (servant->object), signals[INSTALL_PROGRESS], pack, amount, total);
}

static void 
impl_uninstall_progress (impl_POA_Trilobite_Eazel_InstallCallback *servant,
			 const Trilobite_Eazel_PackageStruct *corbapack,
			 const CORBA_long amount,
			 const CORBA_long total,
			 CORBA_Environment * ev)
{
	PackageData *pack;
	pack = packagedata_from_corba_packagestruct (corbapack);
	gtk_signal_emit (GTK_OBJECT (servant->object), signals[UNINSTALL_PROGRESS], pack, amount, total);
}

static void 
impl_install_failed (impl_POA_Trilobite_Eazel_InstallCallback *servant,
		     const Trilobite_Eazel_PackageStruct *corbapack,
		     CORBA_Environment * ev)
{
	PackageData *pack;
	pack = packagedata_from_corba_packagestruct (corbapack);
	gtk_signal_emit (GTK_OBJECT (servant->object), signals[INSTALL_FAILED], pack);
}

static void 
impl_uninstall_failed (impl_POA_Trilobite_Eazel_InstallCallback *servant,
		       const Trilobite_Eazel_PackageStruct *corbapack,
		       CORBA_Environment * ev)
{
	PackageData *pack;
	pack = packagedata_from_corba_packagestruct (corbapack);
	gtk_signal_emit (GTK_OBJECT (servant->object), signals[UNINSTALL_FAILED], pack);
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
	epv->dependency_check    = (gpointer)&impl_dep_check;
	epv->install_progress    = (gpointer)&impl_install_progress;
	epv->uninstall_progress  = (gpointer)&impl_uninstall_progress;
	epv->install_failed      = (gpointer)&impl_install_failed;
	epv->download_failed     = (gpointer)&impl_download_failed;
	epv->uninstall_failed    = (gpointer)&impl_uninstall_failed;
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
	
	servant = (impl_POA_Trilobite_Eazel_InstallCallback*)g_new0 (PortableServer_Servant,1);
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

	service = EAZEL_INSTALL_CALLBACK (object);

	/* FIXME bugzilla.eazel.com 1282.
	   implement this properly */
	g_message ("in eazel_install_callback_destroy");
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
	signals[INSTALL_PROGRESS] = 
		gtk_signal_new ("install_progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallCallbackClass, install_progress),
				gtk_marshal_NONE__POINTER_INT_INT,
				GTK_TYPE_NONE, 3, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);
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

	g_assert (service != NULL);
	g_assert (IS_EAZEL_INSTALL_CALLBACK (service));

	service->cb = eazel_install_callback_create_corba_object (BONOBO_OBJECT (service));

	/* This sets the bonobo structures in service using the corba object */
	if (!bonobo_object_construct (BONOBO_OBJECT (service), service->cb)) {
		g_warning ("bonobo_object_construct failed");
	}	
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
eazel_install_callback_new(Trilobite_Eazel_Install installservice)
{
	EazelInstallCallback *service;

	service = EAZEL_INSTALL_CALLBACK (gtk_object_new (TYPE_EAZEL_INSTALL_CALLBACK, NULL));

	service->installservice = installservice;

	return service;
}

Trilobite_Eazel_InstallCallback 
eazel_install_callback_corba (EazelInstallCallback *service)
{
	return service->cb;
}

void 
eazel_install_callback_install_packages (EazelInstallCallback *service, 
					 GList *categories,
					 CORBA_Environment *ev)
{
	Trilobite_Eazel_CategoryStructList *corbacats;
	corbacats = corba_category_list_from_categorydata_list (categories);
	Trilobite_Eazel_Install_install_packages (service->installservice, 
						  corbacats, 
						  eazel_install_callback_corba (service), ev);
}

GList*
eazel_install_callback_query (EazelInstallCallback *service, 
			      char *query,
			      CORBA_Environment *ev)
{
	GList *result;
	Trilobite_Eazel_PackageStructList *corbares;
	
	/* FIXME: bugzilla.eazel.com 1446 */

	corbares = Trilobite_Eazel_Install_query (service->installservice,
						  query,
						  ev);
	return result;
}
