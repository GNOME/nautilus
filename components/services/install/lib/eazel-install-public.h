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

#ifndef TRILOBITE_EAZEL_INSTALL_PUBLIC_H
#define TRILOBITE_EAZEL_INSTALL_PUBLIC_H 

#include <libgnome/gnome-defs.h>
#ifndef STANDALONE
#include "trilobite-eazel-install.h"
#endif /*  STANDALONE */

#include "eazel-install-types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TRILOBITE_TYPE_EAZEL_INSTALL           (trilobite_eazel_install_get_type ())
#define TRILOBITE_EAZEL_INSTALL(obj)           (GTK_CHECK_CAST ((obj), TRILOBITE_TYPE_EAZEL_INSTALL, TrilobiteEazelInstall))
#define TRILOBITE_EAZEL_INSTALL_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TRILOBITE_TYPE_EAZEL_INSTALL, TrilobiteEazelInstallClass))
#define TRILOBITE_IS_EAZEL_INSTALL(obj)        (GTK_CHECK_TYPE ((obj), TRILOBITE_TYPE_EAZEL_INSTALL))
#define TRILOBITE_IS_EAZEL_INSTALL_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((obj), TRILOBITE_TYPE_EAZEL_INSTALL))

typedef struct _TrilobiteEazelInstall TrilobiteEazelInstall;
typedef struct _TrilobiteEazelInstallClass TrilobiteEazelInstallClass;

struct _TrilobiteEazelInstallClass 
{
#ifdef STANDALONE	
	GtkObjectClass parent_class;
#else 
	BonoboObjectClass parent_class;
#endif /* STANDALONE */
	/* signal prototypes */
	void (*download_progress) (char *file, int amount, int total);
	void (*install_progress)  (char *name, int amount, int total);

#ifndef STANDALONE
	gpointer servant_vepv;
#endif /* STANDALONE */
};

typedef struct _TrilobiteEazelInstallPrivate TrilobiteEazelInstallPrivate;

struct _TrilobiteEazelInstall
{
#ifdef STANDALONE	
	GtkObject parent;
#else 
	BonoboObject parent;
	Trilobite_Eazel_InstallCallback callback;
#endif /* STANDALONE */
	TrilobiteEazelInstallPrivate *private;
};

TrilobiteEazelInstall         *trilobite_eazel_install_new (void);
TrilobiteEazelInstall         *trilobite_eazel_install_new_with_config (const char *config_file);
GtkType                        trilobite_eazel_install_get_type   (void);
void                           trilobite_eazel_install_destroy    (GtkObject *object);

#ifndef STANDALONE
POA_Trilobite_Eazel_Install__epv *trilobite_eazel_install_get_epv (void);
#endif /* STANDALONE */


void trilobite_eazel_install_fetch_pockage_list (TrilobiteEazelInstall *service);
void trilobite_eazel_install_new_packages (TrilobiteEazelInstall *service);

/******************************************************************************/
/* Beware, from hereonafter, it's #def madness, to make the get/set functions */

#define SANITY_VAL(name, ret)\
	g_return_val_if_fail (name != NULL, ret); \
	g_return_val_if_fail (TRILOBITE_IS_EAZEL_INSTALL (name), ret); \
	g_assert (name->private != NULL); \
	g_assert (name->private->iopts != NULL); \
	g_assert (name->private->topts != NULL) 

#define SANITY(name)\
	g_return_if_fail (name != NULL); \
	g_return_if_fail (TRILOBITE_IS_EAZEL_INSTALL (name)); \
	g_assert (name->private != NULL); \
	g_assert (name->private->iopts != NULL); \
	g_assert (name->private->topts != NULL) 


#define ei_access_decl(name, type) \
type trilobite_eazel_install_get_##name (TrilobiteEazelInstall *service)

#define ei_access_impl(name, type, str, var, defl) \
type trilobite_eazel_install_get_##name (TrilobiteEazelInstall *service) { \
        SANITY_VAL (service, defl); \
	return service->private->##str##->var; \
}

#define ei_mutator_decl(name, type) \
void trilobite_eazel_install_set_##name (TrilobiteEazelInstall *service, \
                                         type name)

#define ei_mutator_impl(name, type, str, var) \
void trilobite_eazel_install_set_##name (TrilobiteEazelInstall *service, \
                                         type name) { \
        SANITY (service); \
	service->private->str->var = name; \
}

#define ei_mutator_impl_string(name, type, str, var) \
void trilobite_eazel_install_set_##name (TrilobiteEazelInstall *service, \
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

#endif /* TRILOBITE_EAZEL_INSTALL_PUBLIC_H */
