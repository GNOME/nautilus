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
eazel_package_system_rpm4_query_impl (EazelPackageSystemRpm4 *system,
				      const char *dbpath,
				      const char *key,
				      EazelPackageSystemQueryEnum flag,
				      int detail_level,
				      GList **result)
{
	rpmdb db;
	rpmdbMatchIterator rpm_iterator = NULL;

	db = g_hash_table_lookup (EAZEL_PACKAGE_SYSTEM_RPM3 (system)->private->dbs, dbpath);
	if (db==NULL) {
		fail (system, "query could not access db in %s", dbpath);
		return;
	}

	switch (flag) {
	case EAZEL_PACKAGE_SYSTEM_QUERY_OWNS:		
		rpm_iterator = rpmdbInitIterator (db, RPMTAG_BASENAMES, key, 0);
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_PROVIDES:		
		rpm_iterator = rpmdbInitIterator (db, RPMTAG_PROVIDENAME, key, 0);
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES:
		rpm_iterator = rpmdbInitIterator (db, RPMDBI_LABEL, key, 0);
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	info (system, "rpm_iterator = 0x%p", rpm_iterator);
		
	if (rpm_iterator) {
		Header hd;
		PackageData *pack;

		info (system, "%d hits", rpmdbGetIteratorCount (rpm_iterator));
		
		hd = rpmdbNextIterator (rpm_iterator);
		
		pack = packagedata_new ();
		eazel_package_system_rpm3_packagedata_fill_from_header (EAZEL_PACKAGE_SYSTEM_RPM3 (system), 
									pack, 
									hd, 
									detail_level);
		g_free (pack->install_root);
		pack->install_root = g_strdup (dbpath);
		if (g_list_find_custom (*result, 
					pack, 
					(GCompareFunc)eazel_install_package_compare)!=NULL) {
			info (system, "%s already in set", pack->name);
			packagedata_destroy (pack, TRUE);
		} else {
			(*result) = g_list_prepend (*result, pack);
		}
		rpmdbFreeIterator (rpm_iterator);
	}
}

static void
eazel_package_system_rpm4_query_foreach (const char *dbpath,
					 rpmdb db,
					 struct RpmQueryPiggyBag *pig)
{

	switch (pig->flag) {
	case EAZEL_PACKAGE_SYSTEM_QUERY_OWNS:		
	case EAZEL_PACKAGE_SYSTEM_QUERY_PROVIDES:		
	case EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES:
		eazel_package_system_rpm4_query_impl (EAZEL_PACKAGE_SYSTEM_RPM4 (pig->system),
						      dbpath,
						      pig->key,
						      pig->flag,
						      pig->detail_level,
						      pig->result);
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES:
		g_assert_not_reached ();
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_SUBSTR:
		g_assert_not_reached ();
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

	eazel_package_system_rpm3_create_dbs (EAZEL_PACKAGE_SYSTEM_RPM3 (system), 
					      dbpaths);

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
