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
#include <errno.h>
#include <gnome-xml/parser.h>

#include <gtk/gtksignal.h>

#include "eazel-install-public.h"
#include "eazel-install-private.h"
#include "eazel-install-query.h"
#include "eazel-install-xml-package-list.h"

#ifndef EAZEL_INSTALL_SLIM
#include <rpm/rpmmacro.h>
#endif 

#ifndef EAZEL_INSTALL_NO_CORBA
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include <libtrilobite/libtrilobite.h>
#include "trilobite-eazel-install.h"
#include "eazel-install-corba-types.h"
#else
#include <libtrilobite/trilobite-core-utils.h>
#include <libtrilobite/trilobite-i18n.h>
#endif /* EAZEL_INSTALL_NO_CORBA */

#include "eazel-install-metadata.h"
#include "eazel-install-protocols.h"
#include "eazel-install-logic2.h"
#include "eazel-package-system-types.h"

#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <dirent.h>
#include <string.h>
#include <time.h>

enum {
	FILE_CONFLICT_CHECK,
	FILE_UNIQUENESS_CHECK,
	FEATURE_CONSISTENCY_CHECK,

	DOWNLOAD_PROGRESS,
	PREFLIGHT_CHECK,
	SAVE_TRANSACTION,
	INSTALL_PROGRESS,
	UNINSTALL_PROGRESS,
	DOWNLOAD_FAILED,
	MD5_CHECK_FAILED,
	INSTALL_FAILED,
	UNINSTALL_FAILED,
	DEPENDENCY_CHECK,
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
	ARG_UPGRADE,
	ARG_UNINSTALL,
	ARG_DOWNGRADE,
	ARG_PROTOCOL,
	ARG_TMP_DIR,
	ARG_RPMRC_FILE,
	ARG_SERVER,
	ARG_PACKAGE_LIST_STORAGE_PATH,
	ARG_PACKAGE_LIST,
	ARG_ROOT_DIRS,
	ARG_SERVER_PORT,
	ARG_TRANSACTION_DIR,
	ARG_CGI_PATH,
	ARG_SSL_RENAME,
	ARG_IGNORE_FILE_CONFLICTS,
	ARG_EAZEL_AUTH,
	ARG_EI2
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
void eazel_install_emit_file_conflict_check_default (EazelInstall *service, 
						     const PackageData *package);
void eazel_install_emit_file_uniqueness_check_default (EazelInstall *service, 
						       const PackageData *package);
void eazel_install_emit_feature_consistency_check_default (EazelInstall *service, 
							   const PackageData *package);
void eazel_install_emit_install_progress_default (EazelInstall *service,
						  const PackageData *pack,
						  int, int, int, int, int, int);
void eazel_install_emit_uninstall_progress_default (EazelInstall *service,
						    const PackageData *pack,
						    int, int, int, int, int, int);
void  eazel_install_emit_download_progress_default (EazelInstall *service,
						    const PackageData *package,
						    int amount,
						    int total);
gboolean  eazel_install_emit_preflight_check_default (EazelInstall *service, 
						      GList *packages,
						      int total_bytes,
						      int total_packages);
gboolean  eazel_install_emit_save_transaction_default (EazelInstall *service, 
						      GList *packages);
void  eazel_install_emit_download_failed_default (EazelInstall *service,
						  const PackageData *package);
void eazel_install_emit_md5_check_failed_default (EazelInstall *service,
						  const PackageData *pack,
						  const char *actual_md5);
void eazel_install_emit_install_failed_default (EazelInstall *service,
						const PackageData *pack);
void eazel_install_emit_uninstall_failed_default (EazelInstall *service,
						  const PackageData *pack);
void eazel_install_emit_dependency_check_default (EazelInstall *service,
						  const PackageData *pack,
						  const PackageData *needs);
void eazel_install_emit_done_default (EazelInstall *service, 
				      gboolean result);

#ifdef EAZEL_INSTALL_NO_CORBA
static void eazel_install_log (const char *domain,
			       GLogLevelFlags flags,
			       const char *message,
			       EazelInstall *service);
#endif

#ifndef EAZEL_INSTALL_NO_CORBA

/*****************************************
  Corba stuff
*****************************************/

static PortableServer_ServantBase__epv base_epv = { NULL, NULL, NULL };

#endif /* EAZEL_INSTALL_NO_CORBA */

/*****************************************
  GTK+ object stuff
*****************************************/

static void
eazel_install_finalize (GtkObject *object)
{
	EazelInstall *service;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_IS_INSTALL (object));

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

	trilobite_debug ("into eazel_install_finalize");

	g_hash_table_destroy (service->private->name_to_package_hash);
	g_free (service->private->logfilename);

	g_list_foreach (service->private->downloaded_files, (GFunc)g_free, NULL);
	g_list_free (service->private->downloaded_files);

	g_list_foreach (service->private->root_dirs, (GFunc)g_free, NULL);
	g_list_free (service->private->root_dirs);

	g_list_free (service->private->transaction);
	g_list_free (service->private->failed_packages);
	
	g_free (service->private->transaction_dir);
	g_free (service->private->cur_root);

	transferoptions_destroy (service->private->topts);
	installoptions_destroy (service->private->iopts);
	eazel_softcat_unref (GTK_OBJECT (service->private->softcat));

	if (GTK_OBJECT_CLASS (eazel_install_parent_class)->finalize) {
		GTK_OBJECT_CLASS (eazel_install_parent_class)->finalize (object);
	}
	trilobite_debug ("out eazel_install_finalize");
}

void
eazel_install_unref (GtkObject *object) 
{
	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_IS_INSTALL (object));
#ifndef EAZEL_INSTALL_SLIM
	bonobo_object_unref (BONOBO_OBJECT (object));
#else
	gtk_object_unref (object);
#endif
}

static gboolean
eazel_install_start_signal (EazelPackageSystem *system,
			    EazelPackageSystemOperation op,
			    const PackageData *pack,
			    EazelInstall *service)
{
	service->private->infoblock[2]++;
	switch (op) {
	case EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL:
		eazel_install_emit_install_progress (service, 
						     pack,
						     service->private->infoblock[2], service->private->infoblock[3],
						     0, pack->bytesize,
						     service->private->infoblock[4], service->private->infoblock[5]);				     
		break;
	case EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL:
		eazel_install_emit_uninstall_progress (service, 
						       pack,
						       service->private->infoblock[2], service->private->infoblock[3],
						       0, pack->bytesize,
						       service->private->infoblock[4], service->private->infoblock[5]);				     
		break;
	default:
		break;
	}
	return TRUE;
}

static gboolean
eazel_install_end_signal (EazelPackageSystem *system,
			  EazelPackageSystemOperation op,
			  const PackageData *pack,
			  EazelInstall *service)
{
	switch (op) {
	case EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL:
		eazel_install_emit_install_progress (service, 
						     pack,
						     service->private->infoblock[2], service->private->infoblock[3],
						     pack->bytesize, pack->bytesize,
						     service->private->infoblock[4], service->private->infoblock[5]);				     
		break;
	case EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL:
		eazel_install_emit_uninstall_progress (service, 
						       pack,
						       service->private->infoblock[2], service->private->infoblock[3],
						       pack->bytesize, pack->bytesize,
						       service->private->infoblock[4], service->private->infoblock[5]);				     
		break;
	default:
		break;
	}
	if (pack->toplevel) {
		if (g_list_find (service->private->failed_packages, (PackageData*)pack) == NULL) {
			g_message ("Adding %s to transaction", pack->name);
			service->private->transaction = g_list_prepend (service->private->transaction,
									(PackageData*)pack);
		}
	}
	return TRUE;
}

static gboolean
eazel_install_progress_signal (EazelPackageSystem *system,
			       EazelPackageSystemOperation op,
			       const PackageData *pack,
			       unsigned long *info,
			       EazelInstall *service)
{
	service->private->infoblock[4] = info[4];
	if ((info[0] != 0) && (info[0] != info[1])) {
		switch (op) {
		case EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL:
			eazel_install_emit_install_progress (service, 
							     pack,
							     service->private->infoblock[2], service->private->infoblock[3],
							     info[0], pack->bytesize,
							     info[4], info[5]);
			break;
		case EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL:
			eazel_install_emit_uninstall_progress (service, 
							       pack,
							       service->private->infoblock[2], service->private->infoblock[3],
							       1, pack->bytesize,
							       info[4], info[5]);
			break;
		default:
			break;
		}
	}
	return TRUE;
}

static gboolean
eazel_install_failed_signal (EazelPackageSystem *system,
			     EazelPackageSystemOperation op,
			     const PackageData *pack,
			     EazelInstall *service)
{
	trilobite_debug ("*** %s failed", pack->name);

	service->private->failed_packages = g_list_prepend (service->private->failed_packages, 
							    (PackageData*)pack);

	if (pack->toplevel) {
		trilobite_debug ("emiting failed for %s", pack->name);
		if (op==EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL) {
			eazel_install_emit_install_failed (service, pack);
		} else if (op==EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL) {
			eazel_install_emit_uninstall_failed (service, pack);
		}
	}
	return TRUE;
}

static void
eazel_install_set_arg (GtkObject *object,
				 GtkArg    *arg,
				 guint      arg_id)
{
	EazelInstall *service;

	g_assert (object != NULL);
	g_assert (EAZEL_IS_INSTALL (object));

	service = EAZEL_INSTALL (object);

	switch (arg_id) {
	case ARG_EI2:
		eazel_install_set_ei2 (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_VERBOSE:
		eazel_install_set_verbose (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_IGNORE_FILE_CONFLICTS:
		eazel_install_set_ignore_file_conflicts (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_SSL_RENAME:
		eazel_install_set_ssl_rename (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_SILENT:
		eazel_install_set_silent (service, GTK_VALUE_BOOL(*arg));
		break;
	case ARG_DEBUG:
		eazel_install_set_debug (service, GTK_VALUE_BOOL (*arg));
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
	case ARG_UPGRADE:
		eazel_install_set_upgrade (service, GTK_VALUE_BOOL(*arg));
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
	case ARG_PACKAGE_LIST_STORAGE_PATH:
		eazel_install_set_package_list_storage_path (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_PACKAGE_LIST:
		eazel_install_set_package_list (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_SERVER_PORT:
		eazel_install_set_server_port (service, GTK_VALUE_UINT(*arg));
		break;
	case ARG_ROOT_DIRS:
		eazel_install_set_root_dirs (service, (GList*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_TRANSACTION_DIR:
		eazel_install_set_transaction_dir (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_CGI_PATH:
		eazel_install_set_cgi_path (service, (char*)GTK_VALUE_POINTER(*arg));
		break;
	case ARG_EAZEL_AUTH:
		eazel_install_set_eazel_auth (service, GTK_VALUE_BOOL (*arg));
		break;
	}
}

static void
eazel_install_class_initialize (EazelInstallClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->finalize = eazel_install_finalize;
	object_class->set_arg = eazel_install_set_arg;
	
#ifdef EAZEL_INSTALL_NO_CORBA
	eazel_install_parent_class = gtk_type_class (gtk_object_get_type ());
#else
	eazel_install_parent_class = gtk_type_class (bonobo_object_get_type ());
	klass->servant_vepv = g_new0 (POA_GNOME_Trilobite_Eazel_Install__vepv,1);
	((POA_GNOME_Trilobite_Eazel_Install__vepv*)klass->servant_vepv)->_base_epv = &base_epv; 
	((POA_GNOME_Trilobite_Eazel_Install__vepv*)klass->servant_vepv)->Bonobo_Unknown_epv = bonobo_object_get_epv ();
	((POA_GNOME_Trilobite_Eazel_Install__vepv*)klass->servant_vepv)->GNOME_Trilobite_Eazel_Install_epv = eazel_install_get_epv ();
#endif /* EAZEL_INSTALL_NO_CORBA */

	signals[FILE_CONFLICT_CHECK] = 
		gtk_signal_new ("file_conflict_check",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, file_conflict_check),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	signals[FILE_UNIQUENESS_CHECK] = 
		gtk_signal_new ("file_uniqueness_check",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, file_uniqueness_check),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	signals[FEATURE_CONSISTENCY_CHECK] = 
		gtk_signal_new ("feature_consistency_check",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, feature_consistency_check),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);

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
				gtk_marshal_BOOL__POINTER_INT_INT,
				GTK_TYPE_BOOL, 3, GTK_TYPE_POINTER, GTK_TYPE_INT, GTK_TYPE_INT);
	signals[SAVE_TRANSACTION] = 
		gtk_signal_new ("save_transaction",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, save_transaction),
				gtk_marshal_BOOL__POINTER,
				GTK_TYPE_BOOL, 1, GTK_TYPE_POINTER);
	signals[INSTALL_PROGRESS] = 
		gtk_signal_new ("install_progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, install_progress),
				eazel_install_gtk_marshal_NONE__POINTER_INT_INT_INT_INT_INT_INT,
				GTK_TYPE_NONE, 7, GTK_TYPE_POINTER, 
				GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT, 
				GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT);
	signals[UNINSTALL_PROGRESS] = 
		gtk_signal_new ("uninstall_progress",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, uninstall_progress),
				eazel_install_gtk_marshal_NONE__POINTER_INT_INT_INT_INT_INT_INT,
				GTK_TYPE_NONE, 7, GTK_TYPE_POINTER, 
				GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT, 
				GTK_TYPE_INT, GTK_TYPE_INT, GTK_TYPE_INT);
	signals[DOWNLOAD_FAILED] = 
		gtk_signal_new ("download_failed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, download_failed),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1, GTK_TYPE_POINTER);
	signals[MD5_CHECK_FAILED] = 
		gtk_signal_new ("md5_check_failed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, md5_check_failed),
				gtk_marshal_NONE__POINTER_POINTER,
				GTK_TYPE_NONE, 2, GTK_TYPE_POINTER, GTK_TYPE_POINTER);
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
	signals[DONE] = 
		gtk_signal_new ("done",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (EazelInstallClass, done),
				gtk_marshal_NONE__BOOL,
				GTK_TYPE_NONE, 1, GTK_TYPE_BOOL);
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

#ifdef EAZEL_INSTALL_NO_CORBA
	klass->file_conflict_check = NULL;
	klass->file_uniqueness_check = NULL;
	klass->feature_consistency_check = NULL;
	klass->install_progress = NULL;
	klass->uninstall_progress = NULL;
	klass->download_progress = NULL;
	klass->download_failed = NULL;
	klass->md5_check_failed = NULL;
	klass->install_failed = NULL;
	klass->uninstall_failed = NULL;
	klass->dependency_check = NULL;
	klass->preflight_check = NULL;
	klass->save_transaction = NULL;
#else
	klass->file_conflict_check = eazel_install_emit_file_conflict_check_default;
	klass->file_uniqueness_check = eazel_install_emit_file_uniqueness_check_default;
	klass->feature_consistency_check = eazel_install_emit_feature_consistency_check_default;
	klass->install_progress = eazel_install_emit_install_progress_default;
	klass->uninstall_progress = eazel_install_emit_uninstall_progress_default;
	klass->download_progress = eazel_install_emit_download_progress_default;
	klass->download_failed = eazel_install_emit_download_failed_default;
	klass->md5_check_failed = eazel_install_emit_md5_check_failed_default;
	klass->install_failed = eazel_install_emit_install_failed_default;
	klass->uninstall_failed = eazel_install_emit_uninstall_failed_default;
	klass->dependency_check = eazel_install_emit_dependency_check_default;
	klass->preflight_check = eazel_install_emit_preflight_check_default;
	klass->save_transaction = eazel_install_emit_save_transaction_default;
#endif
	klass->done = eazel_install_emit_done_default;

	gtk_object_add_arg_type ("EazelInstall::ei2",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_VERBOSE);
	gtk_object_add_arg_type ("EazelInstall::verbose",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_VERBOSE);
	gtk_object_add_arg_type ("EazelInstall::ignore_file_conflicts",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_IGNORE_FILE_CONFLICTS);
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
	gtk_object_add_arg_type ("EazelInstall::upgrade",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_UPGRADE);
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
	gtk_object_add_arg_type ("EazelInstall::ssl_rename",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_SSL_RENAME);
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
				 ARG_ROOT_DIRS);
	gtk_object_add_arg_type ("EazelInstall::transaction_dir",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_TRANSACTION_DIR);
	gtk_object_add_arg_type ("EazelInstall::server_port",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_SERVER_PORT);
	gtk_object_add_arg_type ("EazelInstall::cgi_path",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_CGI_PATH);
	gtk_object_add_arg_type ("EazelInstall::eazel_auth",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_EAZEL_AUTH);
}

static void
eazel_install_initialize (EazelInstall *service) {
	GList *list;
#ifndef EAZEL_INSTALL_NO_CORBA
	GNOME_Trilobite_Eazel_Install corba_service;
#endif /* EAZEL_INSTALL_NO_CORBA */

	g_assert (service != NULL);
	g_assert (EAZEL_IS_INSTALL (service));

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
	service->private->root_dirs = NULL;
	service->private->cur_root = NULL;
	if (g_get_home_dir ()) {
		service->private->transaction_dir = g_strdup_printf ("%s/.nautilus/transactions", g_get_home_dir () );
	} else {
		service->private->transaction_dir = NULL;
		g_message (_("Transactions are not stored, could not find home dir"));
	}
	service->private->logfile = NULL;
	service->private->logfilename = NULL;
	service->private->log_to_stderr = FALSE;
	service->private->name_to_package_hash = g_hash_table_new ((GHashFunc)g_str_hash,
								   (GCompareFunc)g_str_equal);
	service->private->dedupe_hash = g_hash_table_new ((GHashFunc)g_str_hash,
							  (GCompareFunc)g_str_equal);
	service->private->dep_ok_hash = g_hash_table_new ((GHashFunc)g_str_hash,
							  (GCompareFunc)g_str_equal);
	service->private->downloaded_files = NULL;
	service->private->transaction = NULL;
	service->private->failed_packages = NULL;
	service->private->revert = FALSE;
	service->private->ssl_rename = FALSE;
	service->private->ignore_file_conflicts = FALSE;

	service->private->softcat = eazel_softcat_new ();
	eazel_softcat_set_packages_pr_query (service->private->softcat, 50);

	eazel_install_set_rpmrc_file (service, "/usr/lib/rpm/rpmrc");

	/* when running as part of trilobite, don't catch logs */
#ifdef EAZEL_INSTALL_NO_CORBA
	g_log_set_handler (G_LOG_DOMAIN,
			   G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_WARNING | G_LOG_LEVEL_ERROR, 
			   (GLogFunc)eazel_install_log, 
			   service);
#endif

	trilobite_debug (_("Transactions are stored in %s"), service->private->transaction_dir);

	list = NULL;
#ifndef EAZEL_INSTALL_SLIM
	if (eazel_install_configure_use_local_db ()) {
		char *tmp = NULL;

		tmp = g_strdup_printf ("%s/.nautilus/packagedb/", g_get_home_dir ());
		list = g_list_prepend (list, g_strdup (g_get_home_dir ()));
		list = g_list_prepend (list, tmp);
	}
#endif

	service->private->package_system = eazel_package_system_new (list);
	eazel_package_system_set_debug (service->private->package_system, 
					EAZEL_PACKAGE_SYSTEM_DEBUG_FAIL);
	gtk_signal_connect (GTK_OBJECT (service->private->package_system),
			    "start",
			    (GtkSignalFunc)eazel_install_start_signal,
			    service);
	gtk_signal_connect (GTK_OBJECT (service->private->package_system),
			    "progress",
			    (GtkSignalFunc)eazel_install_progress_signal,
			    service);
	gtk_signal_connect (GTK_OBJECT (service->private->package_system),
			    "failed",
			    (GtkSignalFunc)eazel_install_failed_signal,
			    service);
	gtk_signal_connect (GTK_OBJECT (service->private->package_system),
			    "end",
			    (GtkSignalFunc)eazel_install_end_signal,
			    service);
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

EazelInstall *
eazel_install_new (void)
{
	EazelInstall *service;

	service = EAZEL_INSTALL (gtk_object_new (TYPE_EAZEL_INSTALL, NULL));
	gtk_object_ref (GTK_OBJECT (service));
	gtk_object_sink (GTK_OBJECT (service));
	
	return service;
}

EazelInstall *
eazel_install_new_with_config (void)
{
	EazelInstall *service;
	TransferOptions *topts;
	InstallOptions *iopts;

	iopts = init_default_install_configuration ();
	topts = init_default_transfer_configuration ();

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
						 "upgrade", iopts->mode_update,
						 "uninstall", iopts->mode_uninstall,
						 "downgrade", iopts->mode_downgrade,
						 "protocol", iopts->protocol,
						 "tmp_dir", topts->tmp_dir,
						 "transaction_dir", iopts->transaction_dir,
						 "rpmrc_file", topts->rpmrc_file,
						 "package_list_storage_path", topts->pkg_list_storage_path,
						 NULL));
	gtk_object_ref (GTK_OBJECT (service));
	gtk_object_sink (GTK_OBJECT (service));

	eazel_install_configure_softcat (service->private->softcat);

	transferoptions_destroy (topts);
	installoptions_destroy (iopts);

	return service;
}


/*****************************************
  install stuff
*****************************************/

/* if there's an older tmpdir left over from a previous attempt, use it */
static char *
find_old_tmpdir (const char *prefix)
{
	DIR *dirfd;
	struct dirent *file;
	char *parent_dir, *base_prefix;
	char *old_tmpdir;
	char *p;
	struct stat statbuf;

	/* find parent dir of prefix */
	parent_dir = g_strdup (prefix);
	p = strrchr (parent_dir, '/');
	if (p == NULL) {
		g_free (parent_dir);
		parent_dir = g_strdup ("/");
		base_prefix = g_strdup (prefix);
	} else {
		base_prefix = g_strdup (p+1);
		*p = '\0';
	}

	old_tmpdir = NULL;
	dirfd = opendir (parent_dir);
	if (dirfd != NULL) {
		while ((file = readdir (dirfd)) != NULL) {
			if ((old_tmpdir == NULL) && (strlen (file->d_name) > strlen (base_prefix)) &&
			    (strncmp (file->d_name, base_prefix, strlen (base_prefix)) == 0)) {
				old_tmpdir = g_strdup_printf ("%s/%s", parent_dir, file->d_name);
				if ((lstat (old_tmpdir, &statbuf) == 0) &&
				    ((statbuf.st_mode & 0077) == 0) &&
				    (statbuf.st_mode & S_IFDIR) &&
				    ((statbuf.st_mode & S_IFLNK) != S_IFLNK) &&
				    (statbuf.st_nlink == 2) &&
				    (statbuf.st_uid == getuid ())) {
					/* acceptable */
					trilobite_debug ("found an old tmpdir: %s", old_tmpdir);
				} else {
					g_free (old_tmpdir);
					old_tmpdir = NULL;
				}
			}
		}
	}
	closedir (dirfd);

	g_free (parent_dir);
	g_free (base_prefix);

	return old_tmpdir;
}

static int
create_temporary_directory (EazelInstall *service) 
{
	int result = 0;
	char *tmpdir = g_strdup (eazel_install_get_tmp_dir (service));
	struct stat statbuf;
	struct passwd *pwentry;
	char *prefix;
	int tries;

	if (tmpdir != NULL) {
		if (lstat (tmpdir, &statbuf) == 0) {
			if ((statbuf.st_mode & S_IFDIR) &&
			    ((statbuf.st_mode & S_IFLNK) != S_IFLNK) &&
			    (statbuf.st_nlink == 2) &&
			    (statbuf.st_uid == getuid ()) &&
			    (chmod (tmpdir, 0700) == 0)) {
				/* acceptable existing directory */
			} else {
				/* unacceptable: throw it away and start over */
				g_warning ("Sinister-looking temporary directory %s (ignoring)", tmpdir);
				g_free (tmpdir);
				tmpdir = NULL;
			}
		} else {
			/* doesn't even exist: try to create */
			if (mkdir (tmpdir, 0700) == 0) {
				/* A-OK! */
			} else {
				g_warning ("Unable to create temporary directory %s (%s)", tmpdir, strerror (errno));
				g_free (tmpdir);
				tmpdir = NULL;
			}
		}
	}

#define RANDCHAR ('A' + (rand () % 23))
	if (tmpdir == NULL) {
		pwentry = getpwuid (getuid ());
		prefix = g_strdup_printf ("/tmp/nautilus-installer-%s.",
					  (pwentry == NULL) ? "unknown" : pwentry->pw_name);
		tmpdir = find_old_tmpdir (prefix);
		if (tmpdir == NULL) {
			trilobite_debug ("No acceptable temporary directory set; creating one...");

			srand (time (NULL));
			for (tries = 0; tries < 50; tries++) {
				tmpdir = g_strdup_printf ("%s%c%c%c%c%c%c%d", prefix,
							  RANDCHAR, RANDCHAR, RANDCHAR, RANDCHAR,
							  RANDCHAR, RANDCHAR, (rand () % 1000));
				/* it's important that the mkdir here be atomic */
				if (mkdir (tmpdir, 0700) == 0) {
					trilobite_debug ("Created temporary directory \"%s\"", tmpdir);
					break;
				}
				g_free (tmpdir);
				tmpdir = NULL;
			}
			if (tries >= 50) {
				g_warning ("Unable to create temporary directory (%s)", strerror (errno));
			}
		}
		g_free (prefix);
	}

	if (tmpdir != NULL) {
		eazel_install_set_tmp_dir (service, tmpdir);
		result = 1;
	}

	g_free (tmpdir);
	return result;
} /* end create_temporary_directory */

gboolean
eazel_install_fetch_remote_package_list (EazelInstall *service) 
{
	gboolean retval;
	char *url;
	char *destination;
	
	EAZEL_INSTALL_SANITY_VAL(service, FALSE);

	trilobite_debug (_("Getting package list from remote server ...\n"));

	url = g_strdup_printf ("http://%s:%d%s%s", 
			       eazel_install_get_server (service),
			       eazel_install_get_server_port (service),
			       eazel_install_get_package_list_storage_path (service)[0]=='/'?"":"/",
			       eazel_install_get_package_list_storage_path (service));
	destination = g_strdup (eazel_install_get_package_list (service));
	
	retval = eazel_install_fetch_file (service, 
					   url, 
					   "package list", 
					   destination,
					   NULL);

	if (!retval) {
		g_warning (_("Unable to retrieve package-list.xml!\n"));
	}

	g_free (destination);
	g_free (url);
	return retval;
} 

#ifdef EAZEL_INSTALL_NO_CORBA
static void 
eazel_install_log (const char *domain,
		   GLogLevelFlags flags,
		   const char *message,
		   EazelInstall *service)
{
	char *format;

	EAZEL_INSTALL_SANITY (service);

	if (flags & G_LOG_LEVEL_DEBUG) {
		format = "d: %s\n";
	} else if (flags & G_LOG_LEVEL_MESSAGE) {
		format = " : %s\n";
	} else if (flags & G_LOG_LEVEL_WARNING) {
		format = "w: %s\n";
	} else if (flags & G_LOG_LEVEL_ERROR) {
		format = "E: %s\n";
	} else {
		format = "?: %s\n";
	}

	if ((flags & G_LOG_LEVEL_DEBUG) && ! service->private->iopts->mode_debug) {
		/* don't log debug stuff to stderr unless debug mode is on */
		return;
	}

	if (service->private->logfile != NULL) {
		fprintf (service->private->logfile, format, message);
		fflush (service->private->logfile);
	}
	if (service->private->log_to_stderr || (service->private->logfile == NULL)) {
		fprintf (stderr, format, message);
		fflush (stderr);
	}
}
#endif

void
eazel_install_set_log (EazelInstall *service,
		       FILE *logfp)
{
	if (service->private->logfile != NULL) {
		fclose (service->private->logfile);
	}
	service->private->logfile = logfp;
}	

void 
eazel_install_open_log (EazelInstall *service,
			const char *fname)
{
	FILE *fp;

	EAZEL_INSTALL_SANITY (service);

	fp = fopen (fname, "wt");
	if (fp != NULL) {
		eazel_install_set_log (service, fp);
		if (service->private->logfilename) {
			g_free (service->private->logfilename);
		}
		service->private->logfilename = g_strdup (fname);
	} else {
		g_warning (_("Cannot write to file %s, using default log handler"), fname);
	}
}

void
eazel_install_log_to_stderr (EazelInstall *service,
			     gboolean log_to_stderr)
{
	service->private->log_to_stderr = log_to_stderr;
}

gboolean
eazel_install_failed_because_of_disk_full (EazelInstall *service)
{
	return service->private->disk_full;
}

static gboolean
eazel_install_alter_mode_on_temp (EazelInstall *service,
				  mode_t mode)
{
	GList *iterator;
	gboolean result = TRUE;

	EAZEL_INSTALL_SANITY_VAL (service, FALSE);

	trilobite_debug ("locking dir to 0%o", mode);

	/* First set mode 400 on all files */
	if (chmod (eazel_install_get_tmp_dir (service), mode + 0100) != 0) {
		trilobite_debug ("cannot change %s to 0%o", eazel_install_get_tmp_dir (service), 
				 mode + 0100);
		result = FALSE;
	}

	for (iterator = service->private->downloaded_files; iterator; iterator = g_list_next (iterator)) {
		char *filename = (char*)iterator->data;
		if (filename) {
			if (chmod (filename, mode) != 0) {
				trilobite_debug ("cannot change %s to 0%o", filename, mode);
				result = FALSE;
			}
		}
	}
	
	trilobite_debug ("locking done");

	return result;
}

gboolean 
eazel_install_lock_tmp_dir (EazelInstall *service)
{
	return eazel_install_alter_mode_on_temp (service, 0400);
}

gboolean 
eazel_install_unlock_tmp_dir (EazelInstall *service)
{
	return eazel_install_alter_mode_on_temp (service, 0600);
}

void
eazel_install_delete_downloads (EazelInstall *service)
{
	GList *iterator;
	char *filename;

	EAZEL_INSTALL_SANITY (service);

	if (service->private->downloaded_files) {
		trilobite_debug ("deleting the package files:");
		for (iterator = g_list_first (service->private->downloaded_files); iterator != NULL;
		     iterator = g_list_next (iterator)) {
			filename = (char*)iterator->data;
			trilobite_debug ("deleting file '%s'", filename);
			if (unlink (filename) != 0) {
				g_warning ("unable to delete file %s !", filename);
			}
		}

		/* don't try to delete them again later */
		g_list_foreach (service->private->downloaded_files, (GFunc)g_free, NULL);
		g_list_free (service->private->downloaded_files);
		service->private->downloaded_files = NULL;
	}
	if (rmdir (eazel_install_get_tmp_dir (service)) != 0) {
		g_warning ("unable to delete directory %s !", 
			   eazel_install_get_tmp_dir (service));
	}
}

void 
eazel_install_install_packages (EazelInstall *service, 
				GList *categories,
				const char *root)
{
	EazelInstallOperationStatus result;
	EAZEL_INSTALL_SANITY (service);

	trilobite_debug ("eazel_install_install_packages (..., %d cats, %s)", 
			 g_list_length (categories),
			 root);
	trilobite_debug ("eazel_install_install_packages (upgrade = %d, downgrade = %d, force = %d)",
			 eazel_install_get_upgrade (service),
			 eazel_install_get_downgrade (service),
			 eazel_install_get_force (service));

#ifndef EAZEL_INSTALL_SLIM
	/* we're about to call g_main_iteraton sometimes, so grab a ref on ourself to avoid vanishing. */
	bonobo_object_ref (BONOBO_OBJECT (service));
#endif /* EAZEL_INSTALL_SLIM */

	if (create_temporary_directory (service)) {
		if (categories == NULL && eazel_install_get_package_list (service) == NULL) {
			char *tmp;
			tmp = g_strdup_printf ("%s/package-list.xml", eazel_install_get_tmp_dir (service));
			eazel_install_set_package_list (service, tmp);
			unlink (tmp);	/* in case one was sitting around from last time */
			g_free (tmp);
			
			eazel_install_fetch_remote_package_list (service);
		}
		
		g_free (service->private->cur_root);
		service->private->cur_root = g_strdup (root?root:DEFAULT_RPM_DB_ROOT);
		eazel_install_set_uninstall (service, FALSE);
		
		result = install_packages (service, categories);
		
		if (result == EAZEL_INSTALL_NOTHING) {
			g_warning (_("Install failed"));
		} 
		
		trilobite_debug ("service->private->downloaded_files = %p", 
				 (unsigned int)service->private->downloaded_files);
		
		g_free (service->private->cur_root);
		service->private->cur_root = NULL;
	} else {
		result = EAZEL_INSTALL_NOTHING;
	}

	eazel_install_emit_done (service, result & EAZEL_INSTALL_INSTALL_OK);
		
#ifndef EAZEL_INSTALL_SLIM
	bonobo_object_unref (BONOBO_OBJECT (service));
#endif /* EAZEL_INSTALL_SLIM */
}

void 
eazel_install_uninstall_packages (EazelInstall *service, GList *categories, const char *root)
{
	EazelInstallOperationStatus result;
	EAZEL_INSTALL_SANITY (service);

	trilobite_debug ("eazel_install_uninstall_packages (..., %d cats, %s)", 
			 g_list_length (categories),
			 root);

	g_free (service->private->cur_root);
	service->private->cur_root = g_strdup (root?root:DEFAULT_RPM_DB_ROOT);
	eazel_install_set_uninstall (service, TRUE);

	if (categories == NULL && eazel_install_get_package_list (service) == NULL) {
		char *tmp;
		tmp = g_strdup_printf ("%s/package-list.xml", eazel_install_get_tmp_dir (service));
		eazel_install_set_package_list (service, tmp);
		unlink (tmp);
		g_free (tmp);

		eazel_install_fetch_remote_package_list (service);
	}

	result = uninstall_packages (service, categories);

	if (result == EAZEL_INSTALL_NOTHING) {
		g_warning (_("Uninstall failed"));
	} 

	eazel_install_emit_done (service, result & EAZEL_INSTALL_UNINSTALL_OK);
}

void 
eazel_install_revert_transaction_from_xmlstring (EazelInstall *service, 
						 const char *xml, 
						 int size,
						 const char *root)
{
	GList *packages;
	EazelInstallOperationStatus result;

	g_free (service->private->cur_root);
	service->private->cur_root = g_strdup (root?root:DEFAULT_RPM_DB_ROOT);

	packages = parse_memory_transaction_file (xml, size);

	service->private->revert = TRUE;

	if (create_temporary_directory (service)) {
		result = revert_transaction (service, packages);
	} else {
		result = EAZEL_INSTALL_NOTHING;
	} 
	service->private->revert = FALSE;

	eazel_install_emit_done (service, result & EAZEL_INSTALL_REVERSION_OK);
}


void 
eazel_install_revert_transaction_from_file (EazelInstall *service, 
					    const char *filename,
					    const char *root)
{
	xmlDocPtr doc;
	xmlChar *mem;
	int size;
	
	doc = xmlParseFile (filename);
	xmlDocDumpMemory (doc, &mem, &size);
	eazel_install_revert_transaction_from_xmlstring (service, mem, size, root);
	g_free (mem);
	xmlFreeDoc (doc);
}

GList*
eazel_install_query_package_system (EazelInstall *service,
				    const char *query, 
				    int flags,
				    const char *root)
{
	GList *result;
	g_message ("eazel_install_query_package_system (...,%s,...)", query);

	g_free (service->private->cur_root);
	service->private->cur_root = g_strdup (root);

	result = eazel_package_system_query (service->private->package_system,
					     root,
					     (const gpointer)query,
					     EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
					     PACKAGE_FILL_EVERYTHING);
	return result;
}

void
eazel_install_add_repository (EazelInstall *service, const char *dir)
{
	service->private->local_repositories = g_list_prepend (service->private->local_repositories, g_strdup (dir));
}

static void
eazel_install_do_transaction_save_report_helper (xmlNodePtr node,
						     GList *packages)
{
	GList *iterator;

	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack;
		char *tmp;
		pack = (PackageData*)iterator->data;
		switch (pack->modify_status) {
		case PACKAGE_MOD_INSTALLED:			
			tmp = g_strdup_printf ("Installed %s", pack->name);
			xmlNewChild (node, NULL, "DESCRIPTION", tmp);
			g_free (tmp);
			break;
		case PACKAGE_MOD_UNINSTALLED:			
			tmp = g_strdup_printf ("Uninstalled %s", pack->name);
			xmlNewChild (node, NULL, "DESCRIPTION", tmp);
			g_free (tmp);
			break;
		default:
			break;
		}
		if (pack->modifies) {
			eazel_install_do_transaction_save_report_helper (node, pack->modifies);
		}
	}
}

static gboolean 
eazel_install_is_dir (const char *path)
{
	struct stat statbuf;

	stat (path, &statbuf);

	return S_ISDIR (statbuf.st_mode);
}
		      


void
eazel_install_save_transaction_report (EazelInstall *service) 
{
	FILE *outfile;
	xmlDocPtr doc;
	xmlNodePtr node, root;
	char *name = NULL;

	if (eazel_install_get_transaction_dir (service) == NULL) {
		g_warning ("Transaction directory not set, not storing transaction report");
	}

	/* Ensure the transaction dir is present */
	if (! eazel_install_is_dir (eazel_install_get_transaction_dir (service))) {
		int retval;
		retval = mkdir (eazel_install_get_transaction_dir (service), 0755);		       
		if (retval < 0) {
			if (errno != EEXIST) {
				g_warning (_("Could not create transaction directory (%s)! ***\n"), 
					   eazel_install_get_transaction_dir (service));
				return;
			}
		}
	}

	/* Create xml */
	doc = xmlNewDoc ("1.0");
	root = node = xmlNewNode (NULL, "TRANSACTION");
	xmlDocSetRootElement (doc, node);

	/* Make a unique name */
	name = g_strdup_printf ("%s/transaction.%lu", eazel_install_get_transaction_dir (service),
				(unsigned long) time (NULL));
	while (!access (name, F_OK)) {
		g_free (name);
		sleep (1);
		name = g_strdup_printf ("%s/transaction.%lu", 
					eazel_install_get_transaction_dir (service), 
					(unsigned long) time (NULL));
	}

	trilobite_debug (_("Writing transaction to %s"), name);
	
	/* Open and save */
	outfile = fopen (name, "w");
	xmlAddChild (root, eazel_install_packagelist_to_xml (service->private->transaction, FALSE));
	node = xmlAddChild (node, xmlNewNode (NULL, "DESCRIPTIONS"));

	{
		char *tmp;
		tmp = g_strdup_printf ("%lu", (unsigned long) time (NULL));		
		xmlNewChild (node, NULL, "DATE", tmp);
		g_free (tmp);
	}

	eazel_install_do_transaction_save_report_helper (node, service->private->transaction);

	xmlDocDump (outfile, doc);
	
	fclose (outfile);
	g_free (name);
}

void
eazel_install_init_transaction (EazelInstall *service) 
{
	/* Null the list of files met in the transaction */
	g_list_free (service->private->transaction);
	service->private->transaction = NULL;

	/* Null the list of files met in the transaction */
	g_list_free (service->private->failed_packages);
	service->private->failed_packages = NULL;
}

/************************************************
  Signal emitters and default handlers.
  The default handlers check for the existance of
  a corba callback, and if true, do the callback
**************************************************/
					 
void 
eazel_install_emit_file_conflict_check (EazelInstall *service, 
					const PackageData *pack)
{
	EAZEL_INSTALL_SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[FILE_CONFLICT_CHECK], pack);
}

void 
eazel_install_emit_file_conflict_check_default (EazelInstall *service, 
						const PackageData *pack)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	EAZEL_INSTALL_SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		GNOME_Trilobite_Eazel_PackageDataStruct *package;
		package = corba_packagedatastruct_from_packagedata (pack);
		GNOME_Trilobite_Eazel_InstallCallback_file_conflict_check (service->callback, 
									   package, 
									   &ev);
		CORBA_free (package);
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
}

void 
eazel_install_emit_file_uniqueness_check (EazelInstall *service, 
					const PackageData *pack)
{
	EAZEL_INSTALL_SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[FILE_UNIQUENESS_CHECK], pack);
}

void 
eazel_install_emit_file_uniqueness_check_default (EazelInstall *service, 
						  const PackageData *pack)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	EAZEL_INSTALL_SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		GNOME_Trilobite_Eazel_PackageDataStruct *package;
		package = corba_packagedatastruct_from_packagedata (pack);
		GNOME_Trilobite_Eazel_InstallCallback_file_uniqueness_check (service->callback, 
									     package, 
									     &ev);
		CORBA_free (package);
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
}

void 
eazel_install_emit_feature_consistency_check (EazelInstall *service, 
					      const PackageData *pack)
{
	EAZEL_INSTALL_SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[FEATURE_CONSISTENCY_CHECK], pack);
}


void 
eazel_install_emit_feature_consistency_check_default (EazelInstall *service, 
						      const PackageData *pack)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	EAZEL_INSTALL_SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		GNOME_Trilobite_Eazel_PackageDataStruct *package;
		package = corba_packagedatastruct_from_packagedata (pack);
		GNOME_Trilobite_Eazel_InstallCallback_feature_consistency_check (service->callback, 
										 package, 
										 &ev);
		CORBA_free (package);
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
}

void 
eazel_install_emit_install_progress (EazelInstall *service, 
				     const PackageData *pack,
				     int package_num, int num_packages, 
				     int package_size_completed, int package_size_total,
				     int total_size_completed, int total_size)
{
	EAZEL_INSTALL_SANITY(service);
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
	EAZEL_INSTALL_SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		GNOME_Trilobite_Eazel_PackageDataStruct *package;
		package = corba_packagedatastruct_from_packagedata (pack);
		GNOME_Trilobite_Eazel_InstallCallback_install_progress (service->callback, 
								  package, 
								  package_num, num_packages,
								  package_size_completed, package_size_total,
								  total_size_completed, total_size,
								  &ev);
		CORBA_free (package);
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

void 
eazel_install_emit_uninstall_progress (EazelInstall *service, 
				       const PackageData *pack,
				       int package_num, int num_packages, 
				       int package_size_completed, int package_size_total,
				       int total_size_completed, int total_size)
{
	EAZEL_INSTALL_SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[UNINSTALL_PROGRESS], 
			 pack,
			 package_num, num_packages,
			 package_size_completed, package_size_total,
			 total_size_completed, total_size);

}

void 
eazel_install_emit_uninstall_progress_default (EazelInstall *service, 
					       const PackageData *pack,
					       int package_num, int num_packages, 
					       int package_size_completed, int package_size_total,
					       int total_size_completed, int total_size)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	EAZEL_INSTALL_SANITY(service);

	if (service->callback != CORBA_OBJECT_NIL) {
		GNOME_Trilobite_Eazel_PackageDataStruct *package;
		package = corba_packagedatastruct_from_packagedata (pack);
		GNOME_Trilobite_Eazel_InstallCallback_uninstall_progress (service->callback, 
									  package, 
									  package_num, num_packages,
									  package_size_completed, package_size_total,
									  total_size_completed, total_size,
									  &ev);
		CORBA_free (package);
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

void 
eazel_install_emit_download_progress (EazelInstall *service, 
				      const PackageData *pack,
				      int amount, 
				      int total)
{
	EAZEL_INSTALL_SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[DOWNLOAD_PROGRESS], pack, amount, total);
}

void 
eazel_install_emit_download_progress_default (EazelInstall *service, 
					      const PackageData *pack,
					      int amount, 
					      int total)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	EAZEL_INSTALL_SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		GNOME_Trilobite_Eazel_PackageDataStruct *package;
		package = corba_packagedatastruct_from_packagedata (pack);
		GNOME_Trilobite_Eazel_InstallCallback_download_progress (service->callback, package, amount, total, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			/* user has aborted us and gone home -- tell VFS to STOP! */
			service->private->cancel_download = TRUE;
		}
		CORBA_free (package);
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

static unsigned long
eazel_install_get_size_increasement (EazelInstall *service, 
				     GList *packages)
{
	const GList *iterator;
	unsigned long result = 0;
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		GList *modit;
		result += pack->bytesize;
		if (pack->modifies) {
			for (modit = pack->modifies; modit; modit = g_list_next (modit)) {
				PackageData *modpack = (PackageData*)modit->data;
				result -= modpack->bytesize;
			}
			
		}
	}
	return result;
}

gboolean
eazel_install_emit_preflight_check (EazelInstall *service, 
				    GList *packages)
{
	unsigned long size_packages, num_packages;
	gboolean result;
	GList *flat_packages;

	EAZEL_INSTALL_SANITY_VAL(service, FALSE);

	flat_packages = flatten_packagedata_dependency_tree (packages);
	size_packages = eazel_install_get_size_increasement (service, flat_packages);
	num_packages = g_list_length (flat_packages);
	g_list_free (flat_packages);

#if 0
	/* FIXME: ARGH ARGH ARGH, this circumvents ORBit's buffersize error */
	if (num_packages > 50) {
		GList *foo = NULL;

		trilobite_debug ("hest hest hest");

		foo = g_list_prepend (foo, packages->data);
		gtk_signal_emit (GTK_OBJECT (service), 
				 signals[PREFLIGHT_CHECK], 
				 foo,
				 size_packages,
				 num_packages,
				 &result);
		return result;
		
	} 
#endif

	gtk_signal_emit (GTK_OBJECT (service), 
			 signals[PREFLIGHT_CHECK], 
			 packages,
			 size_packages,
			 num_packages,
			 &result);

	return result;
}

gboolean
eazel_install_emit_preflight_check_default (EazelInstall *service, 
					    GList *packages,
					    int total_bytes, 
					    int total_packages)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_boolean result = FALSE;
	GNOME_Trilobite_Eazel_PackageDataStructList *package_tree;
	GNOME_Trilobite_Eazel_Operation corba_op;

	CORBA_exception_init (&ev);
	EAZEL_INSTALL_SANITY_VAL (service, FALSE);

	if (service->private->revert) {
		corba_op = GNOME_Trilobite_Eazel_OPERATION_REVERT;
	} else if (eazel_install_get_uninstall (service)==TRUE) {
		corba_op = GNOME_Trilobite_Eazel_OPERATION_UNINSTALL;		
	} else {
		corba_op = GNOME_Trilobite_Eazel_OPERATION_INSTALL;
	} 

	if (service->callback != CORBA_OBJECT_NIL) {
		package_tree = corba_packagedatastructlist_from_packagedata_tree (packages);
		result = GNOME_Trilobite_Eazel_InstallCallback_preflight_check (service->callback, 
										corba_op,
										package_tree,
										total_bytes,
										total_packages, 
										&ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			/* abort on corba failure */
			result = FALSE;
		}
		CORBA_free (package_tree);
	} 
	CORBA_exception_free (&ev);
	return (gboolean)result;
#else /* EAZEL_INSTALL_NO_CORBA */
	return TRUE;
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

gboolean
eazel_install_emit_save_transaction (EazelInstall *service, 
				     GList *packages)
{
	gboolean result;

	EAZEL_INSTALL_SANITY_VAL(service, FALSE);

#if 0
	/* FIXME: ARGH ARGH ARGH, this circumvents ORBit's buffersize error */
	if (g_list_length (packages) > 50) {
		GList *foo = NULL;

		trilobite_debug ("hest hest hest");

		foo = g_list_prepend (foo, packages->data);
		gtk_signal_emit (GTK_OBJECT (service), 
				 signals[SAVE_TRANSACTION], 
				 foo,
				 &result);
		return result;
		
	} 
#endif

	gtk_signal_emit (GTK_OBJECT (service), 
			 signals[SAVE_TRANSACTION], 
			 packages,
			 &result);

	return result;
}

gboolean
eazel_install_emit_save_transaction_default (EazelInstall *service, 
					     GList *packages)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_boolean result = FALSE;
	GNOME_Trilobite_Eazel_PackageDataStructList *package_tree;
	GNOME_Trilobite_Eazel_Operation corba_op;

	CORBA_exception_init (&ev);
	EAZEL_INSTALL_SANITY_VAL (service, FALSE);

	if (service->private->revert) {
		corba_op = GNOME_Trilobite_Eazel_OPERATION_REVERT;
	} else if (eazel_install_get_uninstall (service)==TRUE) {
		corba_op = GNOME_Trilobite_Eazel_OPERATION_UNINSTALL;		
	} else {
		corba_op = GNOME_Trilobite_Eazel_OPERATION_INSTALL;
	} 

	if (service->callback != CORBA_OBJECT_NIL) {
		package_tree = corba_packagedatastructlist_from_packagedata_tree (packages);
		result = GNOME_Trilobite_Eazel_InstallCallback_save_transaction (service->callback, 
										 corba_op,
										 package_tree,
										 &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			/* abort on corba failure */
			result = FALSE;
		}
		CORBA_free (package_tree);
	} 
	CORBA_exception_free (&ev);
	return (gboolean)result;
#else /* EAZEL_INSTALL_NO_CORBA */
	return FALSE;
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

void 
eazel_install_emit_download_failed (EazelInstall *service, 
				    const PackageData *pack)
{
	EAZEL_INSTALL_SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[DOWNLOAD_FAILED], pack);
}

void 
eazel_install_emit_download_failed_default (EazelInstall *service, 
					    const PackageData *pack)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	EAZEL_INSTALL_SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		GNOME_Trilobite_Eazel_PackageDataStruct *package;
		package = corba_packagedatastruct_from_packagedata (pack);
		GNOME_Trilobite_Eazel_InstallCallback_download_failed (service->callback, package, &ev);
		if (ev._major != CORBA_NO_EXCEPTION) {
			/* user has aborted us and gone home -- tell VFS to STOP! */
			service->private->cancel_download = TRUE;
		}
		CORBA_free (package);
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

void 
eazel_install_emit_md5_check_failed (EazelInstall *service, 
				     const PackageData *pd,
				     const char *actual_md5)
{
	EAZEL_INSTALL_SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[MD5_CHECK_FAILED], pd, actual_md5);
}

void 
eazel_install_emit_md5_check_failed_default (EazelInstall *service, 
					     const PackageData *pack,
					     const char *actual_md5)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	EAZEL_INSTALL_SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		GNOME_Trilobite_Eazel_PackageDataStruct *corbapack;

		corbapack = corba_packagedatastruct_from_packagedata (pack);
		
		GNOME_Trilobite_Eazel_InstallCallback_md5_check_failed (service->callback, 
								  corbapack, 
								  actual_md5, 
								  &ev);	
		CORBA_free (corbapack);
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

void 
eazel_install_emit_install_failed (EazelInstall *service, 
				   const PackageData *pd)
{
	EAZEL_INSTALL_SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[INSTALL_FAILED], pd);
}

void 
eazel_install_emit_install_failed_default (EazelInstall *service, 
					   const PackageData *pack)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	GNOME_Trilobite_Eazel_PackageDataStructList *package_tree;
	GList *list;

	EAZEL_INSTALL_SANITY (service);
	CORBA_exception_init (&ev);
	if (service->callback != CORBA_OBJECT_NIL) {
		list = g_list_prepend (NULL, (void *)pack);
		package_tree = corba_packagedatastructlist_from_packagedata_tree (list);
		g_list_free (list);

		GNOME_Trilobite_Eazel_InstallCallback_install_failed (service->callback, package_tree, &ev);
		CORBA_free (package_tree);
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

void 
eazel_install_emit_uninstall_failed (EazelInstall *service, 
				     const PackageData *pd)
{
	EAZEL_INSTALL_SANITY(service);
	gtk_signal_emit (GTK_OBJECT (service), signals[UNINSTALL_FAILED], pd);
}

void 
eazel_install_emit_uninstall_failed_default (EazelInstall *service, 
					     const PackageData *pack)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	GNOME_Trilobite_Eazel_PackageDataStructList *package_tree;
	GList *list;

	EAZEL_INSTALL_SANITY (service);
	CORBA_exception_init (&ev);
	if (service->callback != CORBA_OBJECT_NIL) {
		list = g_list_prepend (NULL, (void *)pack);
		package_tree = corba_packagedatastructlist_from_packagedata_tree (list);
		g_list_free (list);

		GNOME_Trilobite_Eazel_InstallCallback_uninstall_failed (service->callback, package_tree, &ev);
		CORBA_free (package_tree);
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

void 
eazel_install_emit_dependency_check (EazelInstall *service, 
				     const PackageData *package,
				     const PackageDependency *needs)
{
	PackageData *needed_package;

	EAZEL_INSTALL_SANITY(service);

	needed_package = packagedata_copy (needs->package, FALSE);
	if (needs->version) {
		g_free (needed_package->version);
		needed_package->version = NULL;
		g_free (needed_package->minor);
		needed_package->minor = NULL;
		needed_package->version = g_strdup (needs->version);
	} 
	gtk_signal_emit (GTK_OBJECT (service), signals[DEPENDENCY_CHECK], package, needed_package);
	gtk_object_unref (GTK_OBJECT (needed_package));
}

void 
eazel_install_emit_dependency_check_pre_ei2 (EazelInstall *service, 
					     const PackageData *package,
					     const PackageData *needs)
{
	EAZEL_INSTALL_SANITY(service);
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
	EAZEL_INSTALL_SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		GNOME_Trilobite_Eazel_PackageDataStruct *corbapack;
		GNOME_Trilobite_Eazel_PackageDataStruct *corbaneeds;

		corbapack = corba_packagedatastruct_from_packagedata (pack);
		corbaneeds = corba_packagedatastruct_from_packagedata (needs);

		/* An old hack that we removed when we fixed bug 3460.
		 * We should delete it some day.
		 */
		/*
		if (needs->name == NULL && needs->provides) {
			CORBA_free (corbaneeds->name);
			corbaneeds->name = CORBA_string_dup (needs->provides->data);
		}
		*/

		GNOME_Trilobite_Eazel_InstallCallback_dependency_check (service->callback, 
								  corbapack, 
								  corbaneeds, &ev);	
		CORBA_free (corbapack);
		CORBA_free (corbaneeds);
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 

void 
eazel_install_emit_done (EazelInstall *service, gboolean result)
{
	EAZEL_INSTALL_SANITY(service);

	trilobite_debug ("emit_done (result = %s)", result ? "TRUE" : "FALSE");
	gtk_signal_emit (GTK_OBJECT (service), signals[DONE], result);
}

void 
eazel_install_emit_done_default (EazelInstall *service, gboolean result)
{
#ifndef EAZEL_INSTALL_NO_CORBA
	CORBA_Environment ev;
	CORBA_exception_init (&ev);
	EAZEL_INSTALL_SANITY(service);
	if (service->callback != CORBA_OBJECT_NIL) {
		GNOME_Trilobite_Eazel_InstallCallback_done (service->callback, result, &ev);	
	} 
	CORBA_exception_free (&ev);
#endif /* EAZEL_INSTALL_NO_CORBA */
} 


/* Welcome to define madness. These are all the get/set methods. There is nothing of
 * interest beyond this point, except for this dragon:
                              _ _
                       _     //` `\
                   _,-"\%   // /``\`\
              ~^~ >__^  |% // /  } `\`\
                     )  )%// / }  } }`\`\
                    /  (%/'/.\_/\_/\_/\`/
                   (    '         `-._`
                    \   ,     (  \   _`-.__.-;%>
                   /_`\ \      `\ \." `-..-'`
                  ``` /_/`"-=-'`/_/
             jgs     ```       ```
*/


static void 
string_list_copy (GList **in, 
		  const GList *strings) {
	GList *iterator;
	const GList *iterator_c;

	for (iterator = *in; iterator; iterator=iterator->next) {
		g_free (iterator->data);
	}
	g_list_free (*in);
		
	for (iterator_c = strings; iterator_c; iterator_c = iterator_c->next) {
		(*in) = g_list_prepend (*in, g_strdup ((char*)iterator_c->data));
	}
}

/* wrapper functions for softcat */
char *
eazel_install_get_server (EazelInstall *service) {
	EAZEL_INSTALL_SANITY_VAL (service, NULL);
	return g_strdup (eazel_softcat_get_server_host (service->private->softcat));
}

guint
eazel_install_get_server_port (EazelInstall *service) {
	EAZEL_INSTALL_SANITY_VAL (service, 0);
	return (guint)eazel_softcat_get_server_port (service->private->softcat);
}

char *
eazel_install_get_username (EazelInstall *service) {
	const char *username;
	EAZEL_INSTALL_SANITY_VAL (service, NULL);
	eazel_softcat_get_authn (service->private->softcat, &username);
	return g_strdup (username);
}

gboolean
eazel_install_get_eazel_auth (EazelInstall *service) {
	EAZEL_INSTALL_SANITY_VAL (service, FALSE);
	return eazel_softcat_get_authn (service->private->softcat, NULL);
}

char *
eazel_install_get_cgi_path (EazelInstall *service) {
	EAZEL_INSTALL_SANITY_VAL (service, NULL);
	return g_strdup (eazel_softcat_get_cgi_path (service->private->softcat));
}

void
eazel_install_set_server (EazelInstall *service, const char *server)
{
	EAZEL_INSTALL_SANITY (service);
	eazel_softcat_set_server_host (service->private->softcat, server);
}

void
eazel_install_set_server_port (EazelInstall *service, guint port)
{
	EAZEL_INSTALL_SANITY (service);
	eazel_softcat_set_server_port (service->private->softcat, (int)port);
}

void
eazel_install_set_username (EazelInstall *service, const char *username)
{
	EAZEL_INSTALL_SANITY (service);
	eazel_softcat_set_username (service->private->softcat, username);
}

void
eazel_install_set_eazel_auth (EazelInstall *service, gboolean auth)
{
	EAZEL_INSTALL_SANITY (service);
	eazel_softcat_set_authn_flag (service->private->softcat, auth);
}

void
eazel_install_set_cgi_path (EazelInstall *service, const char *cgi_path)
{
	EAZEL_INSTALL_SANITY (service);
	eazel_softcat_set_cgi_path (service->private->softcat, cgi_path);
}

void eazel_install_set_debug (EazelInstall *service, gboolean debug) {
	EAZEL_INSTALL_SANITY (service);
	service->private->iopts->mode_debug = debug;
	if (debug) {
#ifdef ESKIL
		eazel_package_system_set_debug (service->private->package_system, 
						EAZEL_PACKAGE_SYSTEM_DEBUG_VERBOSE);
#else
		eazel_package_system_set_debug (service->private->package_system, 
						EAZEL_PACKAGE_SYSTEM_DEBUG_FAIL);
#endif
	} else {
		eazel_package_system_set_debug (service->private->package_system, 
						EAZEL_PACKAGE_SYSTEM_DEBUG_FAIL);
	}
}

ei_mutator_impl (verbose, gboolean, iopts->mode_verbose);
ei_mutator_impl (silent, gboolean, iopts->mode_silent);
ei_mutator_impl (test, gboolean, iopts->mode_test);
ei_mutator_impl (force, gboolean, iopts->mode_force);
ei_mutator_impl (depend, gboolean, iopts->mode_depend);
ei_mutator_impl (upgrade, gboolean, iopts->mode_update);
ei_mutator_impl (uninstall, gboolean, iopts->mode_uninstall);
ei_mutator_impl (downgrade, gboolean, iopts->mode_downgrade);
ei_mutator_impl (protocol, URLType, iopts->protocol);
ei_mutator_impl_copy (tmp_dir, char*, topts->tmp_dir, g_strdup);
ei_mutator_impl_copy (rpmrc_file, char*, topts->rpmrc_file, g_strdup);
ei_mutator_impl_copy (package_list_storage_path, char*, topts->pkg_list_storage_path, g_strdup);
ei_mutator_impl_copy (package_list, char*, iopts->pkg_list, g_strdup);
ei_mutator_impl_copy (transaction_dir, char*, transaction_dir, g_strdup);
ei_mutator_impl (ssl_rename, gboolean, ssl_rename);
ei_mutator_impl (ignore_file_conflicts, gboolean, ignore_file_conflicts);
ei_mutator_impl (ei2, gboolean, ei2);

ei_access_impl (verbose, gboolean, iopts->mode_verbose, FALSE);
ei_access_impl (silent, gboolean, iopts->mode_silent, FALSE);
ei_access_impl (debug, gboolean, iopts->mode_debug, FALSE);
ei_access_impl (test, gboolean, iopts->mode_test, FALSE);
ei_access_impl (force, gboolean, iopts->mode_force, FALSE);
ei_access_impl (depend, gboolean, iopts->mode_depend, FALSE);
ei_access_impl (upgrade, gboolean, iopts->mode_update, FALSE);
ei_access_impl (uninstall, gboolean, iopts->mode_uninstall, FALSE);
ei_access_impl (downgrade, gboolean, iopts->mode_downgrade, FALSE);
ei_access_impl (protocol, URLType , iopts->protocol, PROTOCOL_LOCAL);
ei_access_impl (tmp_dir, char*, topts->tmp_dir, NULL);
ei_access_impl (rpmrc_file, char*, topts->rpmrc_file, NULL);
ei_access_impl (package_list_storage_path, char*, topts->pkg_list_storage_path, NULL);
ei_access_impl (package_list, char*, iopts->pkg_list, NULL);
ei_access_impl (transaction_dir, char*, transaction_dir, NULL);
ei_access_impl (root_dirs, GList*, root_dirs, NULL);
ei_access_impl (ssl_rename, gboolean, ssl_rename, FALSE);
ei_access_impl (ignore_file_conflicts, gboolean, ignore_file_conflicts, FALSE);
ei_access_impl (ei2, gboolean, ei2, FALSE);

void eazel_install_set_root_dirs (EazelInstall *service,
				  const GList *new_roots) 
{
	string_list_copy (&service->private->root_dirs, new_roots);
}

