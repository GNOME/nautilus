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

#ifndef EAZEL_INSTALLER_PUBLIC_H
#define EAZEL_INSTALLER_PUBLIC_H

#include <eazel-install-public.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TYPE_EAZEL_INSTALLER           (eazel_installer_get_type ())
#define EAZEL_INSTALLER(obj)           (GTK_CHECK_CAST ((obj), TYPE_EAZEL_INSTALLER, EazelInstaller))
#define EAZEL_INSTALLER_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass),  \
							      TYPE_EAZEL_INSTALLER, EazelInstallerClass))
#define IS_EAZEL_INSTALLER(obj)        (GTK_CHECK_TYPE ((obj), TYPE_EAZEL_INSTALLER))
#define IS_EAZEL_INSTALLER_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_EAZEL_INSTALLER))

typedef struct _EazelInstaller EazelInstaller;
typedef struct _EazelInstallerClass EazelInstallerClass;

struct _EazelInstallerClass 
{
	GtkObjectClass parent_class;
};

struct _EazelInstaller
{
	GtkObject parent;

	GnomeDruid *druid;
	GnomeDruidPage *back_page;
	GnomeDruidPage *finish_good;
	GnomeDruidPage *finish_evil;
	GtkWidget *window;

	EazelInstall *service;

	GList *categories;

	GHashTable *category_deps;
	GList *must_have_categories;
	GList *implicit_must_have;
	GList *dont_show;

	char *failure_info;

	gboolean debug, output;
	gboolean test;
};

GtkType            eazel_installer_get_type(void);
EazelInstaller    *eazel_installer_new   (void);
void               eazel_installer_unref (GtkObject *object);
void               eazel_installer_do_install (EazelInstaller *installer,
					       GList *categories);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EAZEL_INSTALLER_PUBLIC_H */


