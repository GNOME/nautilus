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

	GList *install_categories;
	GList *force_categories;
	GList *force_remove_categories;

	GList *must_have_categories;
	GList *implicit_must_have;
	GList *dont_show;

	GList *failure_info;		/* GList<char *> */
	char *tmpdir;

	gboolean debug, output;
	gboolean test;

	unsigned long total_bytes_downloaded;
	unsigned long last_KB;

	/* if all errors during an install are because we would break other packages, we
	 * can try upgrading those packages.  this is the list of packages to try.
	 */
	gboolean all_errors_are_recoverable;
	GList *additional_packages;
	gboolean successful;

	GList *attempted_updates; /* This is a list of packages we tried to update,
				   Before we add, check this list. */

	/* once we've got this, we know mystery errors were caused by rpm (this is kind of a hack) */
	gboolean got_dep_check;
};

typedef enum {
	MUST_UPDATE,      /* package is in the way, update or remove */
	FORCE_BOTH,       /* two packages are fighting it out, install both ? */	
	REMOVE            /* Can't be helped, get rid of that wart */
} RepairEnum;

typedef struct {
	RepairEnum t;
	union {
		struct {
			PackageData *pack;
		} in_the_way;
		struct {
			PackageData *pack_1;
			PackageData *pack_2;
		} force_both;
		struct {
			PackageData *pack;
		} remove;
	} u;
} RepairCase;

GtkType            eazel_installer_get_type(void);
EazelInstaller    *eazel_installer_new   (void);
void               eazel_installer_unref (GtkObject *object);
void               eazel_installer_do_install (EazelInstaller *installer,
					       GList *categories,
					       gboolean force,
					       gboolean remove);

#define DEBUG

#ifdef DEBUG
#define LOG_DEBUG(x) do { if (installer_debug) { printf x; fflush (stdout); } } while (0)
#else
#define LOG_DEBUG(x)
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EAZEL_INSTALLER_PUBLIC_H */


