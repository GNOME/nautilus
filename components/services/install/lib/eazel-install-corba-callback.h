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
#define IS_EAZEL_INSTALL_CALLBACK_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((obj), TYPE_EAZEL_INSTALL_CALLBACK))

typedef struct _EazelInstallCallback EazelInstallCallback;
typedef struct _EazelInstallCallbackClass EazelInstallCallbackClass;

struct _EazelInstallCallbackClass 
{
	BonoboObjectClass parent_class;

	/* signal prototypes */
	void (*download_progress) (EazelInstallCallback *service, const char *name, int amount, int total);
	void (*install_progress)  (EazelInstallCallback *service, const PackageData *pack, int amount, int total);
	void (*uninstall_progress)  (EazelInstallCallback *service, const PackageData *pack, int amount, int total);

	void (*dependency_check) (EazelInstallCallback *service, const PackageData *package, const PackageData *needed );

	void (*download_failed) (EazelInstallCallback *service, char *name);
	void (*install_failed) (EazelInstallCallback *service, PackageData *pd);
	void (*uninstall_failed) (EazelInstallCallback *service, PackageData *pd);

	void (*done) ();

	gpointer servant_vepv;
};

struct _EazelInstallCallback
{
	BonoboObject parent;
	Trilobite_Eazel_InstallCallback cb;
};

EazelInstallCallback          *eazel_install_callback_new (void);
GtkType                        eazel_install_callback_get_type   (void);
void                           eazel_install_callback_destroy    (GtkObject *object);

POA_Trilobite_Eazel_InstallCallback__epv *eazel_install_callback_get_epv (void);
Trilobite_Eazel_InstallCallback           eazel_install_callback_create_corba_object (BonoboObject *service);

Trilobite_Eazel_InstallCallback eazel_install_callback_corba (EazelInstallCallback *service);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EAZEL_INSTALL_CORBA_CALLBACK_H */
