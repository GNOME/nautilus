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

#include "eazel-install-public.h"
#include "eazel-install-private.h"
#include "eazel-install-xml-package-list.h"

#ifndef EAZEL_INSTALL_NO_CORBA
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include <libtrilobite/libtrilobite.h>
#include "trilobite-eazel-install.h"
#include "eazel-install-corba-types.h"
#endif /* EAZEL_INSTALL_NO_CORBA */

#include "eazel-install-metadata.h"
#include "eazel-install-protocols.h"
#include "eazel-install-rpm-glue.h"
#include "eazel-install-types.h"

enum {
	DOWNLOAD_PROGRESS,
	PREFLIGHT_CHECK,
	INSTALL_PROGRESS,
	DOWNLOAD_FAILED,
	INSTALL_FAILED,
	UNINSTALL_FAILED,
	DEPENDENCY_CHECK,
	DELETE_FILES,
	DONE,	
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
	ARG_SERVER,
	ARG_RPM_STORAGE_PATH,
	ARG_PACKAGE_LIST_STORAGE_PATH,
	ARG_PACKAGE_LIST,
	ARG_ROOT_DIR,
	ARG_PACKAGE_SYSTEM,
	ARG_SERVER_PORT,
	ARG_TRANSACTION_DIR
};

/* The signal array, used for building the signal bindings */

static guint signals[LAST_SIGNAL] = { 0 };

/* This is the parent class pointer */

#ifdef EAZEL_INSTALL_NO_CORBA
static GtkObjectClass *eazel_install_parent_class;
#else
static BonoboObjectClass *eazel_install_parent_class;
#endif /* EAZEL_INSTALL_NO_CORBA */


/* prototypes */

void eazel_install_emit_install_progress_default (EazelInstall *service,
						  const PackageData *pack,
						  int, int, int, int, int, int);
void  eazel_install_emit_download_progress_default (EazelInstall *service, 
						    const char *name,
						  int amount,
						  int total);
void  eazel_install_emit_preflight_check_default (EazelInstall *service, 
						  int total_bytes,
						  int total_packages);
void  eazel_install_emit_download_failed_default (EazelInstall *service, 
						  const char *name);
void eazel_install_emit_install_failed_default (EazelInstall *service,
						const PackageData *pack);
void eazel_install_emit_uninstall_failed_default (EazelInstall *service,
						  const PackageData *pack);
void eazel_install_emit_dependency_check_default (EazelInstall *service,
						  const PackageData *pack,
						  const PackageData *needs);
gboolean eazel_install_emit_delete_files_default (EazelInstall *service);
void eazel_install_emit_done_default (EazelInstall *service);

#ifndef EAZEL_INSTALL_NO_CORBA

/*****************************************
  Corba stuff
*****************************************/

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

static CORBA_char*
xml_from_packagedata (const PackageData *pack) {
	xmlDocPtr doc;
	xmlNodePtr node;
	xmlChar *mem;
	CORBA_char *result;
	int size;

	doc = xmlNewDoc ("1.0");

	node = xmlNewDocNode (doc, NULL, "CATEGORIES", NULL);
	xmlDocSetRootElement (doc, node);
	node = xmlAddChild (node, xmlNewNode (NULL, "CATEGORY"));
	xmlSetProp (node, "name", "failed");
	
	node = xmlAddChild (node, xmlNewNode (NULL, "PACKAGES"));

	xmlAddChild (node, eazel_install_packagedata_to_xml (pack, NULL, NULL));

	xmlDocDumpMemory (doc, &mem, &size);
	result = CORBA_string_dup (mem);
	free (mem);
	xmlFreeDoc (doc);

	return result;
}

#endif /* EAZEL_INSTALL_NO_CORBA */

/*****************************************
  GTK+ object stuff
*****************************************/

void
eazel_install_destroy (GtkObject *object)
{
	EazelInstall *service;

	g_message ("in eazel_install_destroy");

	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_INSTALL (object));

	service = EAZEL_INSTALL (object);

#ifndef EAZEL_INSTALL_NO_CORBA
	{
		CORBA_Environment ev;
		CORBA_exception_init (&ev);
		if (service->callback != CORBA_OBJECT_NIL) {
			CORBA_Object_release (service->callback, &ev);
		}
		CORBA_exception_free (&ev);
	}
#endif

	g_hash_table_destroy (service->private->name_to_package_hash);
	g_free (service->private->logfilename);

	transferoptions_destroy (service->private->topts);
	installoptions_destroy (service->private->iopts);

	if (GTK_OBJECT_CLASS (eazel_install_parent_class)->destroy) {
		GTK_OBJECT_CLASS (eazel_install_parent_class)->destroy (object);
	}

	g_message ("out eazel_install_destroy");
}

static void
eazel_install_set_arg (GtkObject *object,
				 GtkArg    *arg,
				 guint      arg_id)
{
	EazelInstall *service;

	g_assert (object != NULL);
	g_assert (IS_EAZEL_INSTALL (object));

	service = EAZEL_INSTALL (object);

	switch (arg_id) {
	case ARG_VERBOSE:
		eazel_install_set_verbose (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_SILENT:
		eazel_install_set_silent (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_TEST:
		eazel_install_set_test (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_FORCE:
		eazel_install_set_force (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_DEPEND:
		eazel_install_set_depend (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_UPDATE:
		eazel_install_set_update (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_UNINSTALL:
		eazel_install_set_uninstall (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_DOWNGRADE:
		eazel_install_set_downgrade (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_PROTOCOL:
		eazel_install_set_protocol (service, GTK_VALUE_ENUM(*arg));
		break;
	case ARG_TMP_DIR:
		eazel_install_set_tmp_dir (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_RPMRC_FILE:
		eazel_install_set_rpmrc_file (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_SERVER:
		eazel_install_set_server (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_RPM_STORAGE_PATH:
		eazel_install_set_rpm_storage_path (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_PACKAGE_LIST_STORAGE_PATH:
		eazel_install_set_package_list_storage_path (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_PACKAGE_LIST:
		eazel_install_set_package_list (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_SERVER_PORT:
		eazel_install_set_server_port (service, GTK_VALUE_UINT(*arg));
		break;
	case ARG_PACKAGE_SYSTEM:
		eazel_install_set_package_system (service, GTK_VALUE_ENUM(*arg));
		break;
	case ARG_ROOT_DIR:
		eazel_install_set_root_dir (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_TRANSACTION_DIR:
		eazel_install_set_transaction_dir (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	}
}

static void
eazel_install_class_initialize (EazelInstallClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->destroy = (void(*)(GtkObject*))eazel_install_destroy;
	object_class->set_arg = eazel_install_set_arg;
	
#ifdef EAZEL_INSTALL_NO_CORBA
	eazel_install_parent_class = gtk_type_class (gtk_object_get_type ());
#else
	eazel_install_parent_class = gtk_type_class (bonobo_object_get_type ());
	klass->servant_vepv = g_new0 (POA_Trilobite_Eazel_Install__vepv,1);
	((POA_Trilobite_Eazel_Install__vepv*)klass->servant_vepv)->_base_epv = &base_epv; 
	((POA_Trilobite_Eazel_Install__vepv*)klass->servant_vepv)->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	((POA_Trilobite_Eazel_Install__vepv*)klass->servant_vepv)->Trilobite_Eazel_Install_epv = eazel_install_get_epv ();
#endif /* EAZEL_INSTALL_NO_CORBA */

	signals[DOWNLOAD_PROGRESS] = 
		gtk_signal_new ("download_progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, download_progress),
				gtk_marshal_NONE__POINTER_INT_INT,
				GTK_TYPE_NONE, 3, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);	
	signals[PREFLIGHT_CHECK] = 
		gtk_signal_new ("preflight_check",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, preflight_check),
				gtk_marshal_NONE__INT_INT,
				GTK_TYPE_NONE, 2, GTK_TYPE_INT, GTK_TYPE_INT);	
	signals[INSTALL_PROGRESS] = 
		gtk_signal_new ("install_progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, install_progress),
				eazel_install_gtk_marshal_NONE__POINTER_INT_INT_INT_INT_INT_INT,
				GTK_TYPE_NONE, 7, GTK_TYPE_POINTER, 
				GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT);
	signals[DOWNLOAD_FAILED] = 
		gtk_signal_new ("download_failed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, download_failed),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	signals[INSTALL_FAILED] = 
		gtk_signal_new ("install_failed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, install_failed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	signals[UNINSTALL_FAILED] = 
		gtk_signal_new ("uninstall_failed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, uninstall_failed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	signals[DEPENDENCY_CHECK] = 
		gtk_signal_new ("dependency_check",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, dependency_check),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_POINTER);
	signals[DELETE_FILES] =
		gtk_signal_new ("delete_files",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, delete_files),
				gtk_marshal_BOOL__NONE,
				GTK_TYPE_BOOL, 0);
	signals[DONE] = 
		gtk_signal_new ("done",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, done),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	klass->install_progress = eazel_install_emit_install_progress_default;
	klass->download_progress = eazel_install_emit_download_progress_default;
	klass->preflight_check = eazel_install_emit_preflight_check_default;
	klass->download_failed = eazel_install_emit_download_failed_default;
	klass->install_failed = eazel_install_emit_install_failed_default;
	klass->uninstall_failed = eazel_install_emit_uninstall_failed_default;
	klass->dependency_check = eazel_install_emit_dependency_check_default;
	klass->delete_files = eazel_install_emit_delete_files_default;
	klass->done = eazel_install_emit_done_default;

	gtk_object_add_arg_type ("EazelInstall::verbose",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_VERBOSE);
	gtk_object_add_arg_type ("EazelInstall::silent",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_SILENT);
	gtk_object_add_arg_type ("EazelInstall::debug",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_DEBUG);
	gtk_object_add_arg_type ("EazelInstall::test",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_TEST);
	gtk_object_add_arg_type ("EazelInstall::force",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_FORCE);
	gtk_object_add_arg_type ("EazelInstall::depend",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_DEPEND);
	gtk_object_add_arg_type ("EazelInstall::update",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_UPDATE);
	gtk_object_add_arg_type ("EazelInstall::uninstall",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_UNINSTALL);
	gtk_object_add_arg_type ("EazelInstall::downgrade",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_DOWNGRADE);
	gtk_object_add_arg_type ("EazelInstall::protocol",
				 GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE,
				 ARG_PROTOCOL);
	gtk_object_add_arg_type ("EazelInstall::tmp_dir",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_TMP_DIR);
	gtk_object_add_arg_type ("EazelInstall::rpmrc_file",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_RPMRC_FILE);
	gtk_object_add_arg_type ("EazelInstall::server",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_SERVER);
	gtk_object_add_arg_type ("EazelInstall::rpm_storage_path",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_RPM_STORAGE_PATH);
	gtk_object_add_arg_type ("EazelInstall::package_list_storage_path",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_PACKAGE_LIST_STORAGE_PATH);
	gtk_object_add_arg_type ("EazelInstall::package_list",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_PACKAGE_LIST);
	gtk_object_add_arg_type ("EazelInstall::root_dir",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_ROOT_DIR);
	gtk_object_add_arg_type ("EazelInstall::transaction_dir",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_TRANSACTION_DIR);
	gtk_object_add_arg_type ("EazelInstall::server_port",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_SERVER_PORT);
	gtk_object_add_arg_type ("EazelInstall::package_system",
				 GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE,
				 ARG_PACKAGE_SYSTEM);
}

static void
eazel_install_initialize (EazelInstall *service) {
#ifndef EAZEL_INSTALL_NO_CORBA
	Trilobite_Eazel_Install corba_service;
#endif /* EAZEL_INSTALL_NO_CORBA */

	g_assert (service != NULL);
	g_assert (IS_EAZEL_INSTALL (service));

#ifndef EAZEL_INSTALL_NO_CORBA
	corba_service = eazel_install_create_corba_object (BONOBO_OBJECT (service));

	/* This sets the bonobo structures in service using the corba object */
	if (!bonobo_object_construct (BONOBO_OBJECT (service), corba_service)) {
		g_error ("bonobo_object_construct failed");
	}	
#endif /* EAZEL_INSTALL_NO_CORBA */

	service->private = g_new0 (EazelInstallPrivate,1);
	service->private->topts = g_new0 (TransferOptions, 1);
	service->private->iopts = g_new0 (InstallOptions, 1);
	service->private->root_dir = g_strdup ("/");
	service->private->transaction_dir = g_strdup_printf ("%s/.nautilus/transactions", g_get_home_dir() );
	service->private->packsys.rpm.conflicts = NULL;
	service->private->packsys.rpm.num_conflicts = 0;
	service->private->packsys.rpm.db = NULL;
	service->private->packsys.rpm.set = NULL;
	service->private->logfile = NULL;
	service->private->logfilename = NULL;
	service->private->name_to_package_hash = g_hash_table_new ((GHashFunc)g_str_hash,
								   (GCompareFunc)g_str_equal);
	service->private->transaction = NULL;

	eazel_install_set_root_dir (service, "/");
	eazel_install_set_rpmrc_file (service, "/usr/lib/rpm/rpmrc");
}

GtkType
eazel_install_get_type() {
	static GtkType service_type = 0;

	/* First time it's called ? */
	if (!service_type)
	{
		static const GtkTypeInfo service_info =
		{
			"EazelInstall",
			sizeof (EazelInstall),
			sizeof (EazelInstallClass),
			(GtkClassInitFunc) eazel_install_class_initialize,
			(GtkObjectInitFunc) eazel_install_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

#ifdef EAZEL_INSTALL_NO_CORBA
		service_type = gtk_type_unique (gtk_object_get_type (), &service_info);
#else
		service_type = gtk_type_unique (bonobo_object_get_type (), &service_info);
#endif /* EAZEL_INSTALL_NO_CORBA */
	}

	return service_type;
}

EazelInstall*
eazel_install_new()
{
	EazelInstall *service;

	service = EAZEL_INSTALL (gtk_object_new (TYPE_EAZEL_INSTALL, NULL));
	
	return service;
}

EazelInstall*
eazel_install_new_with_config (const char *config_file)
{
	EazelInstall *service;
	TransferOptions *topts;
	InstallOptions *iopts;

	iopts = init_default_install_configuration (config_file);
	topts = init_default_transfer_configuration (config_file);

	if (iopts==NULL || topts==NULL) {
		return NULL;
	}

	service = EAZEL_INSTALL (gtk_object_new (TYPE_EAZEL_INSTALL,
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
						 "transaction_dir", iopts->transaction_dir,
						 "rpmrc_file", topts->rpmrc_file,
						 "server", topts->hostname,
						 "rpm_storage_path", topts->rpm_storage_path,
						 "package_list_storage_path", topts->pkg_list_storage_path,
						 "server_port", topts->port_number,
						 NULL));

	transferoptions_destroy (topts);
	installoptions_destroy (iopts);

	return service;
}

static void
create_temporary_directory (const char* tmpdir) 
{
	int retval;

	g_print (_("Creating temporary download directory ...\n"));

	retval = mkdir (tmpdir, 0755);
	if (retval < 0) {
		if (errno != EEXIST) {
			g_error (_("Could not create temporary directory!\n"));
		}
	}
} /* end create_temporary_directory */

static gboolean
eazel_install_fetch_remote_package_list (EazelInstall *service) 
{
	gboolean retval;
	char* url;
	
	SANITY_VAL(service, FALSE);

	g_print (_("Getting package list from remote server ...\n"));

	url = g_strdup_printf ("http://%s%s", 
			       eazel_install_get_server (service),
			       eazel_install_get_package_list_storage_path (service));
	
	retval = eazel_install_fetch_file (service, url, eazel_install_get_package_list (service));

	if (retval == FALSE) {
		g_free (url);
		g_error (_("Unable to retrieve package-list.xml!\n"));
		return FALSE;
	}
	g_free (url);
	return TRUE;
} 

static void 
eazel_install_log (const char *domain,
		   GLogLevelFlags flags,
		   const char *message,
		   EazelInstall *service)
{
	SANITY (service);
	if ( flags | G_LOG_LEVEL_MESSAGE) {
		fprintf (service->private->logfile, " : %s\n", message);
	} else if (flags | G_LOG_LEVEL_WARNING) {
		fprintf (service->private->logfile, "w: %s\n", message);
	} else if (flags | G_LOG_LEVEL_ERROR) {
		fprintf (service->private->logfile, "E: %s\n", message);
	} 
}

void 
eazel_install_open_log (EazelInstall *service,
			const char *fname)
{
	SANITY (service);

	if (service->private->logfile) {
		fclose (service->private->logfile);
	}
	service->private->logfile = fopen (fname, "wt");
	if (service->private->logfilename) {
		g_free (service->private->logfilename);
	}
	service->private->logfilename = g_strdup (fname);
	if (service->private->logfile!=NULL) {
		g_log_set_handler (G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR, 
				   (GLogFunc)eazel_install_log, 
				   service);
		return;
	} 

	g_warning (_("Cannot write to file %s, using default log handler"), fname);
}

void 
eazel_install_install_packages (EazelInstall *service, GList *categories)
{
	SANITY (service);

	if (!g_file_exists (eazel_install_get_tmp_dir (service))) {
		create_temporary_directory (eazel_install_get_tmp_dir (service));
	}
	
	if (categories == NULL && eazel_install_get_package_list (service) == NULL) {
		char *tmp;
		tmp = g_strdup_printf ("%s/package-list.xml", eazel_install_get_tmp_dir (service));
		eazel_install_set_package_list (service, tmp);
		g_free (tmp);

		switch (service->private->iopts->protocol) {
		case PROTOCOL_HTTP:
			eazel_install_fetch_remote_package_list (service);
			break;
		case PROTOCOL_FTP:
			g_error (_("ftp install not supported"));
			break;
		case PROTOCOL_LOCAL:
			break;
		}
	}
	if (install_new_packages (service, categories)==FALSE) {
		g_warning (_("Install failed"));
	}
	if (eazel_install_emit_delete_files (service)) {
		GList *top_item, *sub_item;
		CategoryData *cd;
		PackageData *top_pack, *sub_pack;

		g_message ("*** deleting the package files");
		for (top_item = g_list_first (service->private->transaction); top_item;
		     top_item = g_list_next (top_item)) {
			top_pack = (PackageData *) top_item->data;
			g_message ("*** package '%s'", (char *) top_pack->filename);
			if (unlink ((char *) top_pack->filename) != 0) {
				g_warning ("unable to delete file %s !", top_pack->filename);
			}

			for (sub_item = g_list_first (top_pack->soft_depends); sub_item;
			     sub_item = g_list_next (sub_item)) {
				sub_pack = (PackageData *) sub_item->data;
				g_message ("*** package '%s'", (char *) sub_pack->filename);
				if (unlink ((char *) sub_pack->filename) != 0) {
					g_warning ("unable to delete file %s !", (char *) sub_pack->filename);
				}
			}

			for (sub_item = g_list_first (top_pack->hard_depends); sub_item;
			     sub_item = g_list_next (sub_item)) {
				sub_pack = (PackageData *) sub_item->data;
				g_message ("*** package '%s'", (char *) sub_pack->filename);
				if (unlink ((char *) sub_pack->filename) != 0) {
					g_warning ("unable to delete file %s !", (char *) sub_pack->filename);
				}
			}
		}
	}
	eazel_install_emit_done (service);
}

void 
eazel_install_uninstall_packages (EazelInstall *service, GList *categories)
{
	SANITY (service);
	eazel_install_set_uninstall (service, TRUE);
	if (categories == NULL && eazel_install_get_package_list (service) == NULL) {
		eazel_install_set_package_list (service, "/var/eazel/services/package-list.xml");
		switch (service->private->iopts->protocol) {
		case PROTOCOL_HTTP:
			eazel_install_fetch_remote_package_list (service);
			break;
		case PROTOCOL_FTP:
			g_error (_("ftp install not supported"));
			break;
		case PROTOCOL_LOCAL:
			break;
		}
	}
	if (uninstall_packages (service, categories)==FALSE) {
		g_warning (_("Uninstall failed"));
	} 
	eazel_install_emit_done (service);
}

void eazel_install_revert_transaction_from_xmlstring (EazelInstall *service, 
						      const char *xml, 
						      int size)
{
	GList *packages;

	packages = parse_memory_transaction_file (xml, size);
	revert_transaction (service, packages);

	eazel_install_emit_done (service);
}

/************************************************
  Signal emitters and default handlers.
  The default handlers check for the existance of
  a corba callback, and if true, do the callback
**************************************************/
					 

void 
eazel_install_emit_install_progress (EazelInstall *service, 
				     const PackageData *pack,
				     int package_num, int num_packages, 
				     int package_size_completed, int package_size_total,
				     int total_size_completed, int total_size)
{
	SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[INSTALL_PROGRESS], 
			 pack,
			 package_num, num_packages,
			 package_size_completed, package_size_total,
			 total_size_completed, total_size);

}

void 
eazel_install_emit_install_progress_default (EazelInstall *service, 
					     const PackageData *pack,
					     int package_num, int num_packages, 
					     int package_size_completed, int package_size_total,
					     int total_size_completed, int total_size)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		Trilobite_Eazel_PackageDataStruct package;
		package = corba_packagedatastruct_from_packagedata (pack);
		Trilobite_Eazel_InstallCallback_install_progress (service->callback, 
								  &package, 
								  package_num, num_packages,
								  package_size_completed, package_size_total,
								  total_size_completed, total_size,
								  &ev);	
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

void 
eazel_install_emit_download_progress (EazelInstall *service, 
				      const char *name,
				      int amount, 
				      int total)
{
	SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[DOWNLOAD_PROGRESS], name, amount, total);
}

void 
eazel_install_emit_download_progress_default (EazelInstall *service, 
					      const char *name,
					      int amount, 
					      int total)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		Trilobite_Eazel_InstallCallback_download_progress (service->callback, name, amount, total, &ev);	
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

void 
eazel_install_emit_preflight_check (EazelInstall *service, 
				      int total_bytes, 
				      int total_packages)
{
	SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[PREFLIGHT_CHECK], total_bytes, total_packages);
}

void 
eazel_install_emit_preflight_check_default (EazelInstall *service, 
					      int total_bytes, 
					      int total_packages)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		Trilobite_Eazel_InstallCallback_preflight_check (service->callback, total_bytes, total_packages, &ev);	
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

void 
eazel_install_emit_download_failed (EazelInstall *service, 
				    const char *name)
{
	SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[DOWNLOAD_FAILED], name);
}

void 
eazel_install_emit_download_failed_default (EazelInstall *service, 
					    const char *name)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		Trilobite_Eazel_InstallCallback_download_failed (service->callback, name, &ev);	
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

void 
eazel_install_emit_install_failed (EazelInstall *service, 
				   const PackageData *pd)
{
	SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[INSTALL_FAILED], pd);
}

void 
eazel_install_emit_install_failed_default (EazelInstall *service, 
					   const PackageData *pack)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		CORBA_char *package;
		package = xml_from_packagedata (pack);
		Trilobite_Eazel_InstallCallback_install_failed (service->callback, package, &ev);	
		CORBA_free (package);
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

void 
eazel_install_emit_uninstall_failed (EazelInstall *service, 
				     const PackageData *pd)
{
	SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[UNINSTALL_FAILED], pd);
}

void 
eazel_install_emit_uninstall_failed_default (EazelInstall *service, 
					     const PackageData *pack)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		CORBA_char *package;
		package = xml_from_packagedata (pack);
		Trilobite_Eazel_InstallCallback_uninstall_failed (service->callback, package, &ev);	
		/* CORBA_free (package); */
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

void 
eazel_install_emit_dependency_check (EazelInstall *service, 
				     const PackageData *package,
				     const PackageData *needs)
{
	SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[DEPENDENCY_CHECK], package, needs);
}

void 
eazel_install_emit_dependency_check_default (EazelInstall *service, 
					     const PackageData *pack,
					     const PackageData *needs)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		Trilobite_Eazel_PackageDataStruct corbapack;
		Trilobite_Eazel_PackageDataStruct corbaneeds;

		corbapack = corba_packagedatastruct_from_packagedata (pack);
		corbaneeds = corba_packagedatastruct_from_packagedata (needs);

		Trilobite_Eazel_InstallCallback_dependency_check (service->callback, &corbapack, &corbaneeds, &ev);	
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

gboolean
eazel_install_emit_delete_files (EazelInstall *service)
{
	gboolean result;

	SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[DELETE_FILES], &result);
	return result;
}

gboolean
eazel_install_emit_delete_files_default (EazelInstall *service)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_boolean result = FALSE;

	CORBA_exception_init (&ev);
	SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		result = Trilobite_Eazel_InstallCallback_delete_files (service->callback, &ev);
	}
	CORBA_exception_free (&ev);
	return (gboolean)result;
#else
	return FALSE;
#endif
}

void 
eazel_install_emit_done (EazelInstall *service)
{
	SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[DONE]);
}

void 
eazel_install_emit_done_default (EazelInstall *service)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		Trilobite_Eazel_InstallCallback_done (service->callback, &ev);	
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 


/* Welcome to define madness. These are all the get/set methods. There is nothing of
 interest beyond this point, except for a fucking big dragon*/

ei_mutator_impl (verbose, gboolean, iopts->mode_verbose);
ei_mutator_impl (silent, gboolean, iopts->mode_silent);
ei_mutator_impl (debug, gboolean, iopts->mode_debug);
ei_mutator_impl (test, gboolean, iopts->mode_test);
ei_mutator_impl (force, gboolean, iopts->mode_force);
ei_mutator_impl (depend, gboolean, iopts->mode_depend);
ei_mutator_impl (update, gboolean, iopts->mode_update);
ei_mutator_impl (uninstall, gboolean, iopts->mode_uninstall);
ei_mutator_impl (downgrade, gboolean, iopts->mode_downgrade);
ei_mutator_impl (protocol, URLType, iopts->protocol);
ei_mutator_impl_copy (tmp_dir, char*, topts->tmp_dir, g_strdup);
ei_mutator_impl_copy (rpmrc_file, char*, topts->rpmrc_file, g_strdup);
ei_mutator_impl_copy (server, char*, topts->hostname, g_strdup);
ei_mutator_impl_copy (rpm_storage_path, char*, topts->rpm_storage_path, g_strdup);
ei_mutator_impl_copy (package_list_storage_path, char*, topts->pkg_list_storage_path, g_strdup);
ei_mutator_impl_copy (package_list, char*, iopts->pkg_list, g_strdup);
ei_mutator_impl_copy (root_dir, char*, root_dir, g_strdup);
ei_mutator_impl_copy (transaction_dir, char*, transaction_dir, g_strdup);
ei_mutator_impl (server_port, guint, topts->port_number);

ei_mutator_impl (install_flags, int, install_flags);
ei_mutator_impl (interface_flags, int, interface_flags);
ei_mutator_impl (problem_filters, int, problem_filters);

ei_mutator_impl (package_system, int, package_system);

ei_access_impl (verbose, gboolean, iopts->mode_verbose, FALSE);
ei_access_impl (silent, gboolean, iopts->mode_silent, FALSE);
ei_access_impl (debug, gboolean, iopts->mode_debug, FALSE);
ei_access_impl (test, gboolean, iopts->mode_test, FALSE);
ei_access_impl (force, gboolean, iopts->mode_force, FALSE);
ei_access_impl (depend, gboolean, iopts->mode_depend, FALSE);
ei_access_impl (update, gboolean, iopts->mode_update, FALSE);
ei_access_impl (uninstall, gboolean, iopts->mode_uninstall, FALSE);
ei_access_impl (downgrade, gboolean, iopts->mode_downgrade, FALSE);
ei_access_impl (protocol, URLType , iopts->protocol, PROTOCOL_LOCAL);
ei_access_impl (tmp_dir, char*, topts->tmp_dir, NULL);
ei_access_impl (rpmrc_file, char*, topts->rpmrc_file, NULL);
ei_access_impl (server, char*, topts->hostname, NULL);
ei_access_impl (rpm_storage_path, char*, topts->rpm_storage_path, NULL);
ei_access_impl (package_list_storage_path, char*, topts->pkg_list_storage_path, NULL);
ei_access_impl (package_list, char*, iopts->pkg_list, NULL);
ei_access_impl (transaction_dir, char*, transaction_dir, NULL);
ei_access_impl (root_dir, char*, root_dir, NULL);
ei_access_impl (server_port, guint, topts->port_number, 0);

ei_access_impl (install_flags, int, install_flags, 0);
ei_access_impl (interface_flags, int, interface_flags, 0);
ei_access_impl (problem_filters, int, problem_filters, 0);

ei_access_impl (package_system, int, package_system, 0);
