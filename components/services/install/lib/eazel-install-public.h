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
  If compiled with STANDALONE, 
  the object should _NOT_ use Bonobo, OAF, ORBIT
  and whatnot. This is to facilite the statically linked nautilus bootstrap thingy
 */

#ifndef EAZEL_INSTALL_PUBLIC_H
#define EAZEL_INSTALL_PUBLIC_H 

#include <libgnome/gnome-defs.h>
#ifndef STANDALONE
#include "bonobo.h"
#include "trilobite-eazel-install.h"
#endif /*  STANDALONE */

#include "eazel-install-types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TYPE_EAZEL_INSTALL           (eazel_install_get_type ())
#define EAZEL_INSTALL(obj)           (GTK_CHECK_CAST ((obj), TYPE_EAZEL_INSTALL, EazelInstall))
#define EAZEL_INSTALL_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TYPE_EAZEL_INSTALL, EazelInstallClass))
#define IS_EAZEL_INSTALL(obj)        (GTK_CHECK_TYPE ((obj), TYPE_EAZEL_INSTALL))
#define IS_EAZEL_INSTALL_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((obj), TYPE_EAZEL_INSTALL))

typedef struct _EazelInstall EazelInstall;
typedef struct _EazelInstallClass EazelInstallClass;

struct _EazelInstallClass 
{
#ifdef STANDALONE	
	GtkObjectClass parent_class;
#else 
	BonoboObjectClass parent_class;
#endif /* STANDALONE */
	/* signal prototypes */
	void (*download_progress) (char *file, int amount, int total);
	void (*install_progress)  (char *name, int amount, int total);

	/* 
	   if the set URLType is PROTOCOL_HTTP, info is a HTTPError struc 
	*/
	void (*download_failed) (char *name, gpointer info);
	/*
	  if RPM_FAIL is RPM_SRC_NOT_SUPPORTED, info is NULL
	                 RPM_DEP_FAIL, info is a GSList of required packages (PackageData objects)
			 RPM_NOT_AN_RPM, info is NULL
	*/
	void (*install_failed) (PackageData *pd, RPM_FAIL code, gpointer info);
	void (*uninstall_failed) (PackageData *pd);
#ifndef STANDALONE
	gpointer servant_vepv;
#endif /* STANDALONE */
};

typedef struct _EazelInstallPrivate EazelInstallPrivate;

struct _EazelInstall
{
#ifdef STANDALONE	
	GtkObject parent;
#else 
	BonoboObject parent;
	Trilobite_Eazel_InstallCallback callback;
#endif /* STANDALONE */
	EazelInstallPrivate *private;
};

EazelInstall                  *eazel_install_new (void);
EazelInstall                  *eazel_install_new_with_config (const char *config_file);
GtkType                        eazel_install_get_type   (void);
void                           eazel_install_destroy    (GtkObject *object);

#ifndef STANDALONE
POA_Trilobite_Eazel_Install__epv *eazel_install_get_epv (void);
#endif /* STANDALONE */

void eazel_install_open_log                       (EazelInstall *service,
						   const char *fname);

void eazel_install_emit_install_progress          (EazelInstall *service, 
						   const char *name,
						   int amount, 
						   int total);
void eazel_install_emit_download_progress         (EazelInstall *service, 
						   const char *name,
						   int amount, 
						   int total);
void eazel_install_emit_download_failed           (EazelInstall *service, 
						   const char *name,
						   const gpointer info);
void eazel_install_emit_install_failed            (EazelInstall *service, 
						   const PackageData *pd,
						   RPM_FAIL code,
						   const gpointer info);
void eazel_install_emit_uninstall_failed          (EazelInstall *service, 
						   const PackageData *pd);

/* This is in flux */
void eazel_install_fetch_pockage_list (EazelInstall *service);
void eazel_install_new_packages (EazelInstall *service);
void eazel_install_uninstall (EazelInstall *service);

/******************************************************************************/
/* Beware, from hereonafter, it's #def madness, to make the get/set functions */

#define SANITY_VAL(name, ret)\
	g_return_val_if_fail (name != NULL, ret); \
	g_return_val_if_fail (IS_EAZEL_INSTALL (name), ret); \
	g_assert (name->private != NULL); \
	g_assert (name->private->iopts != NULL); \
	g_assert (name->private->topts != NULL) 

#define SANITY(name)\
	g_return_if_fail (name != NULL); \
	g_return_if_fail (IS_EAZEL_INSTALL (name)); \
	g_assert (name->private != NULL); \
	g_assert (name->private->iopts != NULL); \
	g_assert (name->private->topts != NULL) 


#define ei_access_decl(name, type) \
type eazel_install_get_##name (EazelInstall *service)

#define ei_access_impl(name, type, str, var, defl) \
type eazel_install_get_##name (EazelInstall *service) { \
        SANITY_VAL (service, defl); \
	return service->private->##str##->var; \
}

#define ei_mutator_decl(name, type) \
void eazel_install_set_##name (EazelInstall *service, \
                                         type name)

#define ei_mutator_impl(name, type, str, var) \
void eazel_install_set_##name (EazelInstall *service, \
                                         type name) { \
        SANITY (service); \
	service->private->str->var = name; \
}

#define ei_mutator_impl_string(name, type, str, var) \
void eazel_install_set_##name (EazelInstall *service, \
                                         type name) { \
        SANITY (service); \
        g_free (service->private->str->var); \
	service->private->str->var = g_strdup ( name ); \
}

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
ei_mutator_decl (hostname, char*);
ei_mutator_decl (rpm_storage_path, char*);
ei_mutator_decl (package_list_storage_path, char*);
ei_mutator_decl (package_list, char*);
ei_mutator_decl (port_number, guint);

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
ei_access_decl (tmp_dir, const char*);
ei_access_decl (rpmrc_file, const char*);
ei_access_decl (hostname, const char*);
ei_access_decl (rpm_storage_path, const char*);
ei_access_decl (package_list_storage_path, const char*);
ei_access_decl (package_list, const char*);
ei_access_decl (port_number, guint);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EAZEL_INSTALL_PUBLIC_H */
