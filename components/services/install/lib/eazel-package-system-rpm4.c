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
#include "eazel-package-system-rpm3-private.h"
#include "eazel-package-system-rpm4.h"
#include "eazel-package-system-private.h"
#include <libtrilobite/trilobite-core-utils.h>

#include <rpm/rpmlib.h>

#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libtrilobite/trilobite-root-helper.h>

#define DEFAULT_DB_PATH "/var/lib/rpm"
#define DEFAULT_ROOT "/"

EazelPackageSystem* eazel_package_system_implementation (GList*);

/* This is the parent class pointer */
static EazelPackageSystemRpm3Class *eazel_package_system_rpm4_parent_class;

static void
eazel_package_system_rpm4_query_foreach (char *dbpath,
					 rpmdb db,
					 struct RpmQueryPiggyBag *pig)
{
	rpmdbMatchIterator rpm_iterator;

	switch (pig->flag) {
	case EAZEL_PACKAGE_SYSTEM_QUERY_OWNS:		
		rpm_iterator = rpmdbInitIterator (db, RPMTAG_BASENAMES, pig->key, strlen (pig->key));
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_PROVIDES:		
		rpm_iterator = rpmdbInitIterator (db, RPMTAG_PROVIDENAME, pig->key, strlen (pig->key));
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES:
		rpm_iterator = rpmdbInitIterator (db, RPMDBI_LABEL, pig->key, strlen (pig->key));
		/* FIXME: do pruning calls here */
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES:
	case EAZEL_PACKAGE_SYSTEM_QUERY_SUBSTR:
		break;
	default:
		g_warning ("Unknown query");
	}
}


/*****************************************
  GTK+ object stuff
*****************************************/

static void
eazel_package_system_rpm4_finalize (GtkObject *object)
{
	EazelPackageSystemRpm4 *system;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_PACKAGE_SYSTEM_RPM4 (object));

	system = EAZEL_PACKAGE_SYSTEM_RPM4 (object);

	if (GTK_OBJECT_CLASS (eazel_package_system_rpm4_parent_class)->finalize) {
		GTK_OBJECT_CLASS (eazel_package_system_rpm4_parent_class)->finalize (object);
	}
}

static void
eazel_package_system_rpm4_class_initialize (EazelPackageSystemRpm4Class *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->finalize = eazel_package_system_rpm4_finalize;
	
	eazel_package_system_rpm4_parent_class = gtk_type_class (eazel_package_system_rpm3_get_type ());
}

static void
eazel_package_system_rpm4_initialize (EazelPackageSystemRpm4 *system) {
	g_assert (system != NULL);
	g_assert (IS_EAZEL_PACKAGE_SYSTEM_RPM4 (system));
	
	EAZEL_PACKAGE_SYSTEM_RPM3 (system)->private->query_foreach = 
		(EazelPackageSystemRpmQueryForeach)eazel_package_system_rpm4_query_foreach;	
}

GtkType
eazel_package_system_rpm4_get_type() {
	static GtkType system_type = 0;

	/* First time it's called ? */
	if (!system_type)
	{
		static const GtkTypeInfo system_info =
		{
			"EazelPackageSystemRpm4",
			sizeof (EazelPackageSystemRpm4),
			sizeof (EazelPackageSystemRpm4Class),
			(GtkClassInitFunc) eazel_package_system_rpm4_class_initialize,
			(GtkObjectInitFunc) eazel_package_system_rpm4_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		system_type = gtk_type_unique (eazel_package_system_rpm3_get_type (), &system_info);
	}

	return system_type;
}

EazelPackageSystemRpm4 *
eazel_package_system_rpm4_new (GList *dbpaths) 
{
	EazelPackageSystemRpm4 *system;

	g_return_val_if_fail (dbpaths, NULL);

	system = EAZEL_PACKAGE_SYSTEM_RPM4 (gtk_object_new (TYPE_EAZEL_PACKAGE_SYSTEM_RPM4, NULL));

	gtk_object_ref (GTK_OBJECT (system));
	gtk_object_sink (GTK_OBJECT (system));

	return system;
}

EazelPackageSystem*
eazel_package_system_implementation (GList *dbpaths)
{
	EazelPackageSystem *result;
	GList *tdbpaths = dbpaths;

	g_message ("Eazel Package System - rpm4");

	tdbpaths = g_list_prepend (tdbpaths, g_strdup (DEFAULT_ROOT));
	tdbpaths = g_list_prepend (tdbpaths, g_strdup (DEFAULT_DB_PATH));
	result = EAZEL_PACKAGE_SYSTEM (eazel_package_system_rpm4_new (tdbpaths));
	
	result->private->load_package = (EazelPackageSytemLoadPackageFunc)eazel_package_system_rpm3_load_package;
	result->private->query = (EazelPackageSytemQueryFunc)eazel_package_system_rpm3_query;
	result->private->install = (EazelPackageSytemInstallFunc)eazel_package_system_rpm3_install;
	result->private->uninstall = (EazelPackageSytemUninstallFunc)eazel_package_system_rpm3_uninstall;
	result->private->verify = (EazelPackageSytemVerifyFunc)eazel_package_system_rpm3_verify;

	return result;
}
