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

#ifndef STANDALONE
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include <libtrilobite/libtrilobite.h>
#include "trilobite-eazel-install.h"
#endif /* STANDALONE */

#include "eazel-install-public.h"
#include "eazel-install-private.h"

#include "eazel-install-metadata.h"
#include "eazel-install-protocols.h"
#include "eazel-install-rpm-glue.c"

enum {
	DOWNLOAD_PROGRESS,
	INSTALL_PROGRESS,
	LAST_SIGNAL
};

enum {
	ARG_0,
	ARG_VERBOSE,
	ARG_SILENT,
	ARG_DEBUG,
	ARG_TEST,	
	ARG_FORCE,
	ARG_DEPEND,
	ARG_UPDATE,
	ARG_UNINSTALL,
	ARG_DOWNGRADE,
	ARG_PROTOCOL,
	ARG_TMP_DIR,
	ARG_RPMRC_FILE,
	ARG_HOSTNAME,
	ARG_RPM_STORAGE_PATH,
	ARG_PACKAGE_LIST_STORAGE_PATH,
	ARG_PACKAGE_LIST,
	ARG_PORT_NUMBER
};

/* The signal array, used for building the signal bindings */

static guint signals[LAST_SIGNAL] = { 0 };

/* This is the parent class pointer */

#ifdef STANDALONE
static GtkObjectClass *trilobite_eazel_install_parent_class;
#else
static BonoboObjectClass *trilobite_eazel_install_parent_class;
#endif /* STANDALONE */
/* prototypes */

#ifndef STANDALONE

/*****************************************
  Corba stuff
*****************************************/

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

typedef struct {
	POA_Trilobite_Eazel_Install poa;
	TrilobiteEazelInstall *object;
} impl_POA_Trilobite_Eazel_Install;

static void 
impl_Trilobite_Eazel_Install_new_packages(impl_POA_Trilobite_Eazel_Install *servant,
					  const Trilobite_Eazel_InstallCallback cb,
					  CORBA_Environment * ev) 
{
	servant->object->callback = cb;

	return;
}

POA_Trilobite_Eazel_Install__epv* 
trilobite_eazel_install_get_epv () 
{
	POA_Trilobite_Eazel_Install__epv *epv;

	epv = g_new0 (POA_Trilobite_Eazel_Install__epv, 1);
	epv->new_packages = (gpointer)&impl_Trilobite_Eazel_Install_new_packages;
	return epv;
};

#endif /* STANDALONE */

/*****************************************
  GTK+ object stuff
*****************************************/

void
trilobite_eazel_install_destroy (GtkObject *object)
{
	TrilobiteEazelInstall *service;

	g_return_if_fail (object != NULL);
	g_return_if_fail (TRILOBITE_EAZEL_INSTALL (object));

	service = TRILOBITE_EAZEL_INSTALL (object);

	/* FIXME 
	   implement this properly */
	g_message ("in trilobite_eazel_install_destroy");
}

static void
trilobite_eazel_install_set_arg (GtkObject *object,
				 GtkArg    *arg,
				 guint      arg_id)
{
	TrilobiteEazelInstall *service;

	g_assert (object != NULL);
	g_assert (TRILOBITE_IS_EAZEL_INSTALL (object));

	service = TRILOBITE_EAZEL_INSTALL (object);

	switch (arg_id) {
	case ARG_VERBOSE:
		trilobite_eazel_install_set_verbose (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_SILENT:
		trilobite_eazel_install_set_silent (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_TEST:
		trilobite_eazel_install_set_test (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_FORCE:
		trilobite_eazel_install_set_force (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_DEPEND:
		trilobite_eazel_install_set_depend (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_UPDATE:
		trilobite_eazel_install_set_update (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_UNINSTALL:
		trilobite_eazel_install_set_uninstall (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_DOWNGRADE:
		trilobite_eazel_install_set_downgrade (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_PROTOCOL:
		trilobite_eazel_install_set_protocol (service, GTK_VALUE_ENUM(*arg));
		break;
	case ARG_TMP_DIR:
		trilobite_eazel_install_set_tmp_dir (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_RPMRC_FILE:
		trilobite_eazel_install_set_rpmrc_file (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_HOSTNAME:
		trilobite_eazel_install_set_hostname (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_RPM_STORAGE_PATH:
		trilobite_eazel_install_set_rpm_storage_path (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_PACKAGE_LIST_STORAGE_PATH:
		trilobite_eazel_install_set_package_list_storage_path (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_PACKAGE_LIST:
		trilobite_eazel_install_set_package_list (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_PORT_NUMBER:
		trilobite_eazel_install_set_port_number (service, GTK_VALUE_UINT(*arg));
		break;
	}
}

static void
trilobite_eazel_install_class_initialize (TrilobiteEazelInstallClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->destroy = (void(*)(GtkObject*))trilobite_eazel_install_destroy;
	object_class->set_arg = trilobite_eazel_install_set_arg;
	
#ifdef STANDALONE
	trilobite_eazel_install_parent_class = gtk_type_class (gtk_object_get_type ());
#else
	trilobite_eazel_install_parent_class = gtk_type_class (bonobo_object_get_type ());
	klass->servant_vepv = g_new0 (POA_Trilobite_Eazel_Install__vepv,1);
	((POA_Trilobite_Eazel_Install__vepv*)klass->servant_vepv)->_base_epv = &base_epv; 
	((POA_Trilobite_Eazel_Install__vepv*)klass->servant_vepv)->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	((POA_Trilobite_Eazel_Install__vepv*)klass->servant_vepv)->Trilobite_Eazel_Install_epv = trilobite_eazel_install_get_epv ();
#endif /* STANDALONE */

	signals[DOWNLOAD_PROGRESS] = 
		gtk_signal_new ("download_progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (TrilobiteEazelInstallClass, download_progress),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE,1,GTK_TYPE_POINTER);
	signals[INSTALL_PROGRESS] = 
		gtk_signal_new ("install_progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (TrilobiteEazelInstallClass, install_progress),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE,0);
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	gtk_object_add_arg_type ("TrilobiteEazelInstall::verbose",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_VERBOSE);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::silent",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_SILENT);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::debug",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_DEBUG);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::test",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_TEST);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::force",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_FORCE);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::depend",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_DEPEND);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::update",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_UPDATE);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::uninstall",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_UNINSTALL);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::downgrade",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_DOWNGRADE);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::protocol",
				 GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE,
				 ARG_PROTOCOL);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::tmp_dir",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_TMP_DIR);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::rpmrc_file",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_RPMRC_FILE);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::hostname",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_HOSTNAME);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::rpm_storage_path",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_RPM_STORAGE_PATH);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::package_list_storage_path",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_PACKAGE_LIST_STORAGE_PATH);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::package_list",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_PACKAGE_LIST);
	gtk_object_add_arg_type ("TrilobiteEazelInstall::port_number",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_PORT_NUMBER);
}

#ifndef STANDALONE
static Trilobite_Eazel_Install
trilobite_eazel_install_create_corba_object (BonoboObject *service) {
	impl_POA_Trilobite_Eazel_Install *servant;
	CORBA_Environment ev;

	g_assert (service != NULL);
	g_assert (TRILOBITE_IS_EAZEL_INSTALL (service));
	
	CORBA_exception_init (&ev);
	
	servant = (impl_POA_Trilobite_Eazel_Install*)g_new0 (PortableServer_Servant,1);
	((POA_Trilobite_Eazel_Install*) servant)->vepv = TRILOBITE_EAZEL_INSTALL_CLASS ( GTK_OBJECT (service)->klass)->servant_vepv;
	POA_Trilobite_Eazel_Install__init (servant, &ev);
	ORBIT_OBJECT_KEY (((POA_Trilobite_Eazel_Install*)servant)->_private)->object = NULL;

	if (ev._major != CORBA_NO_EXCEPTION) {
		g_warning ("Cannot instantiate Trilobite_Eazel_Install corba object");
		g_free (servant);
		CORBA_exception_free (&ev);		
		return CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);		

	/* Return the bonobo activation of the servant */
	return (Trilobite_Eazel_Install) bonobo_object_activate_servant (service, servant);
}
#endif /* STANDALONE */

static void
trilobite_eazel_install_initialize (TrilobiteEazelInstall *service) {
#ifndef STANDALONE
	Trilobite_Eazel_Install corba_service;
#endif /* STANDALONE */

	/* g_message ("in trilobite_eazel_install_initialize"); */

	g_assert (service != NULL);
	g_assert (TRILOBITE_IS_EAZEL_INSTALL (service));

#ifndef STANDALONE
	corba_service = trilobite_eazel_install_create_corba_object (BONOBO_OBJECT (service));

	/* This sets the bonobo structures in service using the corba object */
	if (!bonobo_object_construct (BONOBO_OBJECT (service), corba_service)) {
		g_warning ("bonobo_object_construct failed");
	}	
#endif /* STANDALONE */

	service->private = g_new0 (TrilobiteEazelInstallPrivate,1);
	service->private->topts = g_new0 (TransferOptions, 1);
	service->private->iopts = g_new0 (InstallOptions, 1);
}

GtkType
trilobite_eazel_install_get_type() {
	static GtkType trilobite_service_type = 0;

	/* g_message ("into trilobite_eazel_install_get_type");  */

	/* First time it's called ? */
	if (!trilobite_service_type)
	{
		static const GtkTypeInfo trilobite_service_info =
		{
			"TrilobiteEazelInstall",
			sizeof (TrilobiteEazelInstall),
			sizeof (TrilobiteEazelInstallClass),
			(GtkClassInitFunc) trilobite_eazel_install_class_initialize,
			(GtkObjectInitFunc) trilobite_eazel_install_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

#ifdef STANDALONE
		trilobite_service_type = gtk_type_unique (gtk_object_get_type (), &trilobite_service_info);
#else
		trilobite_service_type = gtk_type_unique (bonobo_object_get_type (), &trilobite_service_info);
#endif /* STANDALONE */
	}

	return trilobite_service_type;
}

TrilobiteEazelInstall*
trilobite_eazel_install_new()
{
	TrilobiteEazelInstall *service;

	service = TRILOBITE_EAZEL_INSTALL (gtk_object_new (TRILOBITE_TYPE_EAZEL_INSTALL, NULL));
	
	return service;
}

TrilobiteEazelInstall*
trilobite_eazel_install_new_with_config (const char *config_file)
{
	TrilobiteEazelInstall *service;
	TransferOptions *topts;
	InstallOptions *iopts;

	iopts = init_default_install_configuration (config_file);
	topts = init_default_transfer_configuration (config_file);

	if (iopts==NULL || topts==NULL) {
		return NULL;
	}

	service = TRILOBITE_EAZEL_INSTALL (gtk_object_new (TRILOBITE_TYPE_EAZEL_INSTALL,
							   "verbose", iopts->mode_verbose,
							   "silent", iopts->mode_silent,
							   "debug", iopts->mode_debug,
							   "test", iopts->mode_test,
							   "force", iopts->mode_force,
							   "depend", iopts->mode_depend,
							   "update", iopts->mode_update,
							   "uninstall", iopts->mode_uninstall,
							   "downgrade", iopts->mode_downgrade,
							   "protocol", iopts->protocol,
							   "tmp_dir", topts->tmp_dir,
							   "rpmrc_file", topts->rpmrc_file,
							   "hostname", topts->hostname,
							   "rpm_storage_path", topts->rpm_storage_path,
							   "package_list_storage_path", topts->pkg_list_storage_path,
							   "package_list", iopts->pkg_list,
							   "port_number", topts->port_number,
							   NULL));

	/* FIXME: bugzilla.eazel.com 982:
	   topts and iopts are leaked at this point. There needs
	   to be a set of _destroy methods in eazel-install-metadata.c */

	return service;
}

static void
create_temporary_directory (const char* tmpdir) {

	int retval;

	g_print (_("Creating temporary download directory ...\n"));

	retval = mkdir (tmpdir, 0755);
	if (retval < 0) {
		if (errno != EEXIST) {
			g_error (_("*** Could not create temporary directory! ***\n"));
		}
	}
} /* end create_temporary_directory */

static void
fetch_remote_package_list (const char* pkg_list, TransferOptions* topts) {

	gboolean retval;
	char* url;

	g_print (_("Getting package-list.xml from remote server ...\n"));

	url = g_strdup_printf ("http://%s%s", topts->hostname,
                                topts->pkg_list_storage_path);

	retval = http_fetch_remote_file (url, pkg_list);

	if (retval == FALSE) {
		g_free (url);
		g_error ("*** Unable to retrieve package-list.xml! ***\n");
	}
	g_free (url);
} /* end fetch_remote_package_list */

void 
trilobite_eazel_install_new_packages (TrilobiteEazelInstall *service)
{
	SANITY (service);

	g_message ("trilobite_eazel_install_new_packages");

	if (!g_file_exists (service->private->topts->tmp_dir)) {
		create_temporary_directory (service->private->topts->tmp_dir);
	}
	
	if (service->private->iopts->protocol == PROTOCOL_HTTP) {
		fetch_remote_package_list (service->private->iopts->pkg_list, service->private->topts);
	}
	if (install_new_packages (service->private->iopts, service->private->topts)==FALSE) {
		g_warning ("*** Install failed");
	} 
}


/* Welcome to define madness. These are all the get/set methods. There is nothing of
 interest beyond this point, except for a fucking big dragon*/

ei_mutator_impl (verbose, gboolean, iopts, mode_verbose);
ei_mutator_impl (silent, gboolean, iopts, mode_silent);
ei_mutator_impl (debug, gboolean, iopts, mode_debug);
ei_mutator_impl (test, gboolean, iopts, mode_test);
ei_mutator_impl (force, gboolean, iopts, mode_force);
ei_mutator_impl (depend, gboolean, iopts, mode_depend);
ei_mutator_impl (update, gboolean, iopts, mode_update);
ei_mutator_impl (uninstall, gboolean, iopts, mode_uninstall);
ei_mutator_impl (downgrade, gboolean, iopts, mode_downgrade);
ei_mutator_impl (protocol, URLType, iopts, protocol);
ei_mutator_impl_string (tmp_dir, char*, topts, tmp_dir);
ei_mutator_impl_string (rpmrc_file, char*, topts, rpmrc_file);
ei_mutator_impl_string (hostname, char*, topts, hostname);
ei_mutator_impl_string (rpm_storage_path, char*, topts, rpm_storage_path);
ei_mutator_impl_string (package_list_storage_path, char*, topts, pkg_list_storage_path);
ei_mutator_impl_string (package_list, char*, iopts, pkg_list);
ei_mutator_impl (port_number, guint, topts, port_number);

ei_access_impl (verbose, gboolean, iopts, mode_verbose, FALSE);
ei_access_impl (silent, gboolean, iopts, mode_silent, FALSE);
ei_access_impl (debug, gboolean, iopts, mode_debug, FALSE);
ei_access_impl (test, gboolean, iopts, mode_test, FALSE);
ei_access_impl (force, gboolean, iopts, mode_force, FALSE);
ei_access_impl (depend, gboolean, iopts, mode_depend, FALSE);
ei_access_impl (update, gboolean, iopts, mode_update, FALSE);
ei_access_impl (uninstall, gboolean, iopts, mode_uninstall, FALSE);
ei_access_impl (downgrade, gboolean, iopts, mode_downgrade, FALSE);
ei_access_impl (protocol, URLType , iopts, protocol, PROTOCOL_LOCAL);
ei_access_impl (tmp_dir, const char*, topts, tmp_dir, NULL);
ei_access_impl (rpmrc_file, const char*, topts, rpmrc_file, NULL);
ei_access_impl (hostname, const char*, topts, hostname, NULL);
ei_access_impl (rpm_storage_path, const char*, topts, rpm_storage_path, NULL);
ei_access_impl (package_list_storage_path, const char*, topts, pkg_list_storage_path, NULL);
ei_access_impl (package_list, const char*, iopts, pkg_list, NULL);
ei_access_impl (port_number, guint, topts, port_number, 0);
