/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

/*
  If compiled with EAZEL_INSTALL_NO_CORBA, 
  the object should _NOT_ use Bonobo, OAF, ORBIT
  and whatnot. This is to facilite the statically linked nautilus bootstrap thingy
 */

#ifndef EAZEL_INSTALL_PUBLIC_H
#define EAZEL_INSTALL_PUBLIC_H

#include <libgnome/gnome-defs.h>
#ifndef EAZEL_INSTALL_NO_CORBA
#include "bonobo.h"
#include "trilobite-eazel-install.h"
#endif /*  EAZEL_INSTALL_NO_CORBA */

#include "eazel-package-system-types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TYPE_EAZEL_INSTALL           (eazel_install_get_type ())
#define EAZEL_INSTALL(obj)           (GTK_CHECK_CAST ((obj), TYPE_EAZEL_INSTALL, EazelInstall))
#define EAZEL_INSTALL_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TYPE_EAZEL_INSTALL, EazelInstallClass))
#define EAZEL_IS_INSTALL(obj)        (GTK_CHECK_TYPE ((obj), TYPE_EAZEL_INSTALL))
#define EAZEL_IS_INSTALL_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_EAZEL_INSTALL))

typedef enum {
	EAZEL_INSTALL_NOTHING = 0,
	EAZEL_INSTALL_INSTALL_OK = 1<<0,
	EAZEL_INSTALL_UNINSTALL_OK = 1<<1,
	EAZEL_INSTALL_REVERSION_OK = 1<<2
} EazelInstallOperationStatus;

typedef enum {
	EAZEL_INSTALL_STATUS_NEW_PACKAGE,
	EAZEL_INSTALL_STATUS_UPGRADES,
	EAZEL_INSTALL_STATUS_DOWNGRADES,
	EAZEL_INSTALL_STATUS_QUO
} EazelInstallStatus;
	
typedef struct _EazelInstall EazelInstall;
typedef struct _EazelInstallClass EazelInstallClass;

struct _EazelInstallClass 
{
#ifdef EAZEL_INSTALL_NO_CORBA	
	GtkObjectClass parent_class;
#else 
	BonoboObjectClass parent_class;
#endif /* EAZEL_INSTALL_NO_CORBA */
	/* signal prototypes */
	void (*download_progress) (EazelInstall *service, const PackageData *package, int amount, int total);
	void (*file_conflict_check) (EazelInstall *service, const PackageData *package);
	void (*file_uniqueness_check) (EazelInstall *service, const PackageData *package);
	void (*feature_consistency_check) (EazelInstall *service, const PackageData *package);

	gboolean (*preflight_check) (EazelInstall *service, 
				     GList *packages,
				     int total_bytes, 
				     int total_packages);

	void (*install_progress)  (EazelInstall *service, 
				   const PackageData *pack, 
				   int package_num, int num_packages, 
				   int package_size_completed, int package_size_total,
				   int total_size_completed, int total_size);
	void (*dependency_check) (EazelInstall *service, const PackageData *pack, const PackageData *needed);
	/* 
	   if the set URLType is PROTOCOL_HTTP, info is a HTTPError struc 
	*/
	void (*download_failed) (EazelInstall *service, const PackageData *package);
	/*
	  if RPM_FAIL is RPM_SRC_NOT_SUPPORTED, info is NULL
	                 RPM_DEP_FAIL, info is a GSList of required packages (PackageData objects)
			 RPM_NOT_AN_RPM, info is NULL
	*/
	void (*md5_check_failed) (EazelInstall *service, const PackageData *pd, const char *actual_md5);
	void (*install_failed) (EazelInstall *service, const PackageData *pd);
	void (*uninstall_failed) (EazelInstall *service, const PackageData *pd);

	gboolean (*delete_files) (EazelInstall *service);

	void (*done) (EazelInstall *service, gboolean result);
#ifndef EAZEL_INSTALL_NO_CORBA
	gpointer servant_vepv;
#endif /* EAZEL_INSTALL_NO_CORBA */
};

typedef struct _EazelInstallPrivate EazelInstallPrivate;

struct _EazelInstall
{
#ifdef EAZEL_INSTALL_NO_CORBA	
	GtkObject parent;
#else 
	BonoboObject parent;
	GNOME_Trilobite_Eazel_InstallCallback callback;
#endif /* EAZEL_INSTALL_NO_CORBA */
	EazelInstallPrivate *private;
};

EazelInstall                  *eazel_install_new (void);
EazelInstall                  *eazel_install_new_with_config (void);
GtkType                        eazel_install_get_type   (void);
void                           eazel_install_unref       (GtkObject *object);

#ifndef EAZEL_INSTALL_NO_CORBA
POA_GNOME_Trilobite_Eazel_Install__epv *eazel_install_get_epv (void);
GNOME_Trilobite_Eazel_Install eazel_install_create_corba_object (BonoboObject *service);
#endif /* EAZEL_INSTALL_NO_CORBA */

void eazel_install_set_log			  (EazelInstall *service,
						   FILE *logfp);
void eazel_install_open_log                       (EazelInstall *service,
						   const char *fname);
void eazel_install_log_to_stderr		  (EazelInstall *service,
						   gboolean log_to_stderr);
gboolean eazel_install_failed_because_of_disk_full (EazelInstall *service);
void eazel_install_add_repository		  (EazelInstall *service, const char *dir);

/* This sets mode 500 on tmpdir and all files in
   private->downloaded_files */
gboolean eazel_install_lock_tmp_dir               (EazelInstall *service);
/* This sets mode 700 on tmpdir and downloaded files */
gboolean eazel_install_unlock_tmp_dir               (EazelInstall *service);

void eazel_install_emit_file_conflict_check       (EazelInstall *service,
						   const PackageData *pack);
void eazel_install_emit_file_uniqueness_check     (EazelInstall *service,
						   const PackageData *pack);
void eazel_install_emit_feature_consistency_check (EazelInstall *service,
						   const PackageData *pack);
void eazel_install_emit_install_progress          (EazelInstall *service, 
						   const PackageData *pack,
						   int package_num, int num_packages, 
						   int package_size_completed, int package_size_total,
						   int total_size_completed, int total_size);
void eazel_install_emit_download_progress         (EazelInstall *service,
						   const PackageData *package,
						   int amount, 
						   int total);
gboolean eazel_install_emit_preflight_check     (EazelInstall *service, 
						 GList *packages);
void eazel_install_emit_download_failed           (EazelInstall *service, 
						   const PackageData *package);
void eazel_install_emit_md5_check_failed          (EazelInstall *service, 
						   const PackageData *pd,
						   const char *actual_md5);
void eazel_install_emit_install_failed            (EazelInstall *service, 
						   const PackageData *pd);
void eazel_install_emit_uninstall_failed          (EazelInstall *service, 
						   const PackageData *pd);
void eazel_install_emit_dependency_check          (EazelInstall *service, 
						   const PackageData *package, 
						   const PackageDependency *needed);
void eazel_install_emit_dependency_check_pre_ei2  (EazelInstall *service, 
						   const PackageData *package, 
						   const PackageData *needed);
void eazel_install_emit_done                      (EazelInstall *service, 
						   gboolean result);

/* This is in flux */

gboolean eazel_install_fetch_remote_package_list (EazelInstall *service);

void eazel_install_install_packages (EazelInstall *service, 
				     GList *categories, 
				     const char *root);

void eazel_install_uninstall_packages (EazelInstall *service, 
				       GList *categories, 
				       const char *root);

GList* eazel_install_query_package_system (EazelInstall *service,
					   const char *query, 
					   int flags,
					   const char *root);

void eazel_install_revert_transaction_from_xmlstring (EazelInstall *service, 
						      const char *xml, 
						      int size,
						      const char *root);

void eazel_install_revert_transaction_from_file (EazelInstall *service, 
						 const char *filename,
						 const char *root);

void eazel_install_delete_downloads (EazelInstall *service);


/******************************************************************************/
/* Beware, from hereonafter, it's #def madness, to make the get/set functions */

#define EAZEL_INSTALL_SANITY_VAL(name, ret)\
	g_return_val_if_fail (name != NULL, ret); \
	g_return_val_if_fail (EAZEL_IS_INSTALL (name), ret); \
	g_return_val_if_fail (name->private->softcat != NULL, ret); \
	g_return_val_if_fail (EAZEL_IS_SOFTCAT (name->private->softcat), ret); \
	g_assert (name->private != NULL); \
	g_assert (name->private->iopts != NULL); \
	g_assert (name->private->topts != NULL) 

#define EAZEL_INSTALL_SANITY(name)\
	g_return_if_fail (name != NULL); \
	g_return_if_fail (EAZEL_IS_INSTALL (name)); \
	g_return_if_fail (name->private->softcat != NULL); \
	g_return_if_fail (EAZEL_IS_SOFTCAT (name->private->softcat)); \
	g_assert (name->private != NULL); \
	g_assert (name->private->iopts != NULL); \
	g_assert (name->private->topts != NULL) 

#define ei_access_decl(name, type) \
type eazel_install_get_##name (EazelInstall *service)

#define ei_access_impl(name, type, var, defl) \
type eazel_install_get_##name (EazelInstall *service) { \
        EAZEL_INSTALL_SANITY_VAL (service, defl); \
	return service->private->var; \
}

#define ei_mutator_decl(name, type) \
void eazel_install_set_##name (EazelInstall *service, \
                                         const type name)

#define ei_mutator_impl(name, type,var) \
void eazel_install_set_##name (EazelInstall *service, \
                                         type name) { \
        EAZEL_INSTALL_SANITY (service); \
	service->private->var = name; \
}

#define ei_mutator_impl_copy(name, type, var, copyfunc) \
void eazel_install_set_##name (EazelInstall *service, \
                                         const type name) { \
        EAZEL_INSTALL_SANITY (service); \
        g_free (service->private->var); \
	service->private->var = copyfunc ( name ); \
}

/* When adding fields to EazelInstall object, add
   _mutator_decl here
   _access_decl here
   ARG_ in -object.c
   code in eazel_install_set_arg
   code in eazel_install_class_initialize
   _mutator_impl in -object.c
   _access_impl in -object.c
*/

ei_mutator_decl (verbose, gboolean);
ei_mutator_decl (silent, gboolean);
ei_mutator_decl (debug, gboolean);
ei_mutator_decl (test, gboolean);
ei_mutator_decl (force, gboolean);
ei_mutator_decl (depend, gboolean);
ei_mutator_decl (update, gboolean);
ei_mutator_decl (uninstall, gboolean);
ei_mutator_decl (downgrade, gboolean);
ei_mutator_decl (protocol, URLType);
ei_mutator_decl (tmp_dir, char*);
ei_mutator_decl (rpmrc_file, char*);
ei_mutator_decl (server, char*);
ei_mutator_decl (username, char*);
ei_mutator_decl (package_list_storage_path, char*);
ei_mutator_decl (package_list, char*);
ei_mutator_decl (root_dirs, GList*);
ei_mutator_decl (transaction_dir, char*);
ei_mutator_decl (server_port, guint);
ei_mutator_decl (cgi_path, char*);
ei_mutator_decl (eazel_auth, gboolean);
ei_mutator_decl (ssl_rename, gboolean);
ei_mutator_decl (ignore_file_conflicts, gboolean);
ei_mutator_decl (ei2, gboolean);

ei_access_decl (verbose, gboolean);
ei_access_decl (silent, gboolean);
ei_access_decl (debug, gboolean);
ei_access_decl (test, gboolean);
ei_access_decl (force, gboolean);
ei_access_decl (depend, gboolean);
ei_access_decl (update, gboolean);
ei_access_decl (uninstall, gboolean);
ei_access_decl (downgrade, gboolean);
ei_access_decl (protocol, URLType );
ei_access_decl (tmp_dir, char*);
ei_access_decl (rpmrc_file, char*);
ei_access_decl (server, char*);
ei_access_decl (username, char*);
ei_access_decl (package_list_storage_path, char*);
ei_access_decl (package_list, char*);
ei_access_decl (root_dirs, GList*);
ei_access_decl (transaction_dir, char*);
ei_access_decl (server_port, guint);
ei_access_decl (cgi_path, char*);
ei_access_decl (eazel_auth, gboolean);
ei_access_decl (ssl_rename, gboolean);
ei_access_decl (ignore_file_conflicts, gboolean);
ei_access_decl (ei2, gboolean);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EAZEL_INSTALL_PUBLIC_H */

