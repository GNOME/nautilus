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
#include "eazel-package-system-skeleton.h"
#include "eazel-package-system-private.h"
#include <libtrilobite/trilobite-core-utils.h>

EazelPackageSystem* eazel_package_system_implementation (GList*);

/* This is the parent class pointer */
static EazelPackageSystemClass *eazel_package_system_skeleton_parent_class;

static PackageData*
eazel_package_system_skeleton_load_package (EazelPackageSystemSkeleton *system,
					    PackageData *in_package,
					    const char *filename,
					    int detail_level)
{
	PackageData *result = NULL;
	g_assert (system != NULL);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_SKELETON (system));
	trilobite_debug ("eazel_package_system_skeleton_load_package");
	/* Code Here */
	return result;
}

static GList*               
eazel_package_system_skeleton_query (EazelPackageSystemSkeleton *system,
				     const char *dbpath,
				     const gpointer key,
				     EazelPackageSystemQueryEnum flag,
				     int detail_level)
{
	GList *result = NULL;
	g_assert (system != NULL);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_SKELETON (system));
	trilobite_debug ("eazel_package_system_skeleton_query");
	/* Code Here */
	return result;
}

static void                 
eazel_package_system_skeleton_install (EazelPackageSystemSkeleton *system, 
				       const char *dbpath,
				       GList* packages,
				       unsigned long flags)
{
	g_assert (system != NULL);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_SKELETON (system));
	trilobite_debug ("eazel_package_system_skeleton_install");
	/* Code Here */
}

static void                 
eazel_package_system_skeleton_uninstall (EazelPackageSystemSkeleton *system, 
					 const char *dbpath,
					 GList* packages,
					 unsigned long flags)
{
	g_assert (system != NULL);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_SKELETON (system));
	trilobite_debug ("eazel_package_system_skeleton_uninstall");
	/* Code Here */
}

static void                 
eazel_package_system_skeleton_verify (EazelPackageSystemSkeleton *system, 
				      GList* packages)
{
	g_assert (system != NULL);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_SKELETON (system));
	trilobite_debug ("eazel_package_system_skeleton_verify");
	/* Code Here */
}

static int
eazel_package_system_skeleton_compare_version (EazelPackageSystem *system,
					       const char *a,
					       const char *b)
{
	g_assert (system != NULL);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_SKELETON (system));
	trilobite_debug ("eazel_package_system_skeleton_compare_version");
	/* Code Here */
	return 0;
}


/*****************************************
  GTK+ object stuff
*****************************************/

static void
eazel_package_system_skeleton_finalize (GtkObject *object)
{
	EazelPackageSystemSkeleton *system;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_PACKAGE_SYSTEM_SKELETON (object));

	system = EAZEL_PACKAGE_SYSTEM_SKELETON (object);

	if (GTK_OBJECT_CLASS (eazel_package_system_skeleton_parent_class)->finalize) {
		GTK_OBJECT_CLASS (eazel_package_system_skeleton_parent_class)->finalize (object);
	}
}

static void
eazel_package_system_skeleton_class_initialize (EazelPackageSystemSkeletonClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->finalize = eazel_package_system_skeleton_finalize;
	
	eazel_package_system_skeleton_parent_class = gtk_type_class (eazel_package_system_get_type ());
}

static void
eazel_package_system_skeleton_initialize (EazelPackageSystemSkeleton *system) {
	g_assert (system != NULL);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_SKELETON (system));
}

GtkType
eazel_package_system_skeleton_get_type() {
	static GtkType system_type = 0;

	/* First time it's called ? */
	if (!system_type)
	{
		static const GtkTypeInfo system_info =
		{
			"EazelPackageSystemSkeleton",
			sizeof (EazelPackageSystemSkeleton),
			sizeof (EazelPackageSystemSkeletonClass),
			(GtkClassInitFunc) eazel_package_system_skeleton_class_initialize,
			(GtkObjectInitFunc) eazel_package_system_skeleton_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		system_type = gtk_type_unique (eazel_package_system_get_type (), &system_info);
	}

	return system_type;
}

EazelPackageSystemSkeleton *
eazel_package_system_skeleton_new (GList *dbpaths) 
{
	EazelPackageSystemSkeleton *system;

	system = EAZEL_PACKAGE_SYSTEM_SKELETON (gtk_object_new (TYPE_EAZEL_PACKAGE_SYSTEM_SKELETON, NULL));

	gtk_object_ref (GTK_OBJECT (system));
	gtk_object_sink (GTK_OBJECT (system));

	return system;
}

EazelPackageSystem*
eazel_package_system_implementation (GList *dbpaths)
{
	EazelPackageSystem *result;

	trilobite_debug ("eazel_package_system_implementation (skeleton)");

	result = EAZEL_PACKAGE_SYSTEM (eazel_package_system_skeleton_new (dbpaths));
	
	result->private->load_package
		= (EazelPackageSytemLoadPackageFunc)eazel_package_system_skeleton_load_package;

	result->private->query
		= (EazelPackageSytemQueryFunc)eazel_package_system_skeleton_query;

	result->private->install
		= (EazelPackageSytemInstallFunc)eazel_package_system_skeleton_install;

	result->private->uninstall
		= (EazelPackageSytemUninstallFunc)eazel_package_system_skeleton_uninstall;

	result->private->verify
		= (EazelPackageSytemVerifyFunc)eazel_package_system_skeleton_verify;

	result->private->compare_version
		= (EazelPackageSystemCompareVersionFunc)eazel_package_system_skeleton_compare_version;

	return result;
}
