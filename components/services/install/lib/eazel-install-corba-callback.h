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

#ifndef EAZEL_INSTALL_CORBA_CALLBACK_H
#define EAZEL_INSTALL_CORBA_CALLBACK_H 

#include <libgnome/gnome-defs.h>
#include "bonobo.h"
#include "trilobite-eazel-install.h"

#include "eazel-install-types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TYPE_EAZEL_INSTALL_CALLBACK           (eazel_install_callback_get_type ())
#define EAZEL_INSTALL_CALLBACK(obj)           (GTK_CHECK_CAST ((obj), TYPE_EAZEL_INSTALL_CALLBACK, EazelInstallCallback))
#define EAZEL_INSTALL_CALLBACK_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TYPE_EAZEL_INSTALL_CALLBACK, EazelInstallCallbackClass))
#define IS_EAZEL_INSTALL_CALLBACK(obj)        (GTK_CHECK_TYPE ((obj), TYPE_EAZEL_INSTALL_CALLBACK))
#define IS_EAZEL_INSTALL_CALLBACK_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_EAZEL_INSTALL_CALLBACK))

typedef struct _EazelInstallCallback EazelInstallCallback;
typedef struct _EazelInstallCallbackClass EazelInstallCallbackClass;

struct _EazelInstallCallbackClass 
{
	BonoboObjectClass parent_class;

	/* Called during the download of a file */
	void (*download_progress) (EazelInstallCallback *service, const char *name, int amount, int total);

	/* Called after download and before (un)install_progress */
	void (*preflight_check) (EazelInstallCallback *service, int total_size, int num_packages);

	/* Called during install of a package */
	void (*install_progress)  (EazelInstallCallback *service, 
				   const PackageData *pack, 
				   int package_num, int num_packages, 
				   int package_size_completed, int package_size_total,
				   int total_size_completed, int total_size);
	/* Called during uninstall of a package */
	void (*uninstall_progress)  (EazelInstallCallback *service, const PackageData *pack, int amount, int total);

	/* Called when a dependency check is being resolved */
	void (*dependency_check) (EazelInstallCallback *service, const PackageData *package, const PackageData *needed );

	/* Called when a file could not be downloaded */
	void (*download_failed) (EazelInstallCallback *service, char *name);
	/* Called when a package install request fails, eg. for dependency reasons.
	   pd->soft_depends and pd->breaks can be traversed to see why the package
	   failed */
	void (*install_failed) (EazelInstallCallback *service, PackageData *pd);
	/* Same as install_failed... */
	void (*uninstall_failed) (EazelInstallCallback *service, PackageData *pd);

	/* Called after installation to determine if the RPM files should be deleted */
	gboolean (*delete_files) (EazelInstallCallback *service);

	/* Called when the operation is complete */
	void (*done) ();

	gpointer servant_vepv;
};

struct _EazelInstallCallback
{
	BonoboObject parent;
	Trilobite_Eazel_InstallCallback cb;

	BonoboObjectClient *installservice_bonobo;
	Trilobite_Eazel_Install installservice_corba;
};

/* Create a new eazel-install-callback object */
EazelInstallCallback          *eazel_install_callback_new (void);
/* Destroy the eazel-install-callback object */
void                           eazel_install_callback_destroy    (GtkObject *object);

/* Request the installation of a set of packages in categories.
   If categories = NULL, the service tries to fetch the packagelist
   from the server. See the trilobite-eazel-install.idl file for
   the function to specify packagelist name */
void eazel_install_callback_install_packages (EazelInstallCallback *service, 
					      GList *categories,
					      CORBA_Environment *ev);

/* Request the uninstallation of a set of packages */
void eazel_install_callback_uninstall_packages (EazelInstallCallback *service, 
						  GList *categories,
						  CORBA_Environment *ev);

GList* eazel_install_callback_simple_query (EazelInstallCallback *service, 
					    const char* query,
					    CORBA_Environment *ev);

void eazel_install_callback_revert_transaction (EazelInstallCallback *service, 
						const char *xmlfile,
						CORBA_Environment *ev);

/* Stuff */
GtkType                                   eazel_install_callback_get_type   (void);
POA_Trilobite_Eazel_InstallCallback__epv *eazel_install_callback_get_epv (void);
Trilobite_Eazel_InstallCallback           eazel_install_callback_create_corba_object (BonoboObject *service);
Trilobite_Eazel_Install                   eazel_install_callback_corba_objref (EazelInstallCallback *service);
BonoboObjectClient                       *eazel_install_callback_bonobo (EazelInstallCallback *service);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EAZEL_INSTALL_CORBA_CALLBACK_H */
