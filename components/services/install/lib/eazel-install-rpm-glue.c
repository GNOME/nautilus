/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 * Copyright (C) 1998-1999 James Henstridge
 * Copyright (C) 1998 Red Hat Software, Inc.
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
 * Authors: J Shane Culpepper <pepper@eazel.com>
 *          Eskil Heyn Olsen  <eskil@eazel.com>
 */

#include "eazel-install-rpm-glue.h"
#include "eazel-install-public.h"
#include "eazel-install-private.h"
#include "eazel-install-logic.h"

#include <libtrilobite/trilobite-core-utils.h>
#include <libtrilobite/trilobite-md5-tools.h>

#include <rpm/rpmlib.h>
#include <string.h>
#include <time.h>

#include <ctype.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef EAZEL_INSTALL_SLIM
#include <sys/wait.h>
#endif

#include "eazel-package-system.h"

/* WAAAAAAHH! */
#include "eazel-package-system-rpm3.h"
#include "eazel-package-system-rpm3-private.h"

PackageData* packagedata_new_from_rpm_conflict (struct rpmDependencyConflict);
PackageData* packagedata_new_from_rpm_conflict_reversed (struct rpmDependencyConflict);

PackageData*
packagedata_new_from_rpm_conflict (struct rpmDependencyConflict conflict) 
{
	PackageData *result;
	
	result = packagedata_new ();

	result->name = g_strdup (conflict.needsName);
	result->version = (conflict.needsVersion && (strlen (conflict.needsVersion) > 1)) ? g_strdup (conflict.needsVersion) : NULL;
	return result;
}

PackageData*
packagedata_new_from_rpm_conflict_reversed (struct rpmDependencyConflict conflict) 
{
	PackageData *result;
	
	result = packagedata_new ();

	result->name = g_strdup (conflict.byName);
	result->version = (conflict.byVersion && (strlen (conflict.byVersion) > 1)) ? g_strdup (conflict.byVersion) : NULL;
	return result;
}

#if 0
/*
  Adds the headers to the package system set
 */
static void
eazel_install_add_to_rpm_set (EazelInstall *service,
			      rpmTransactionSet set, 
			      GList **packages,
			      GList **failed)

{
	GList *iterator;
	GList *tmp_failed;
	int interface_flags;

	g_assert (packages!=NULL);
	g_assert (*packages!=NULL);

	tmp_failed = NULL;

	interface_flags = 0;
	if (eazel_install_get_update (service)) {
		interface_flags |= INSTALL_UPGRADE;
	}

	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack;
		int err;

		pack = (PackageData*)iterator->data;
		if (!eazel_install_get_uninstall (service)) {
			g_assert (pack->packsys_struc);
			err = rpmtransAddPackage (set,
						  ((Header) pack->packsys_struc),
						  NULL, 
						  NULL,
						  interface_flags, 
						  NULL);
			if (err!=0) {
				trilobite_debug ("rpmtransAddPackage (..., %s, ...) = %d", pack->name, err);
				if (pack->source_package) {
					pack->status = PACKAGE_SOURCE_NOT_SUPPORTED;
				} else {
					pack->status = PACKAGE_INVALID;
				}
				/* We cannot remove the thing immediately from packages, as
				   we're iterating it, so add to a tmp list and nuke later */
				tmp_failed = g_list_prepend (tmp_failed, pack);
			}
			/* just flailing around here (robey) 
			if (pack->soft_depends) {
				eazel_install_add_to_rpm_set (service, set, &pack->soft_depends, failed);
			}
			*/
		} else {
			g_assert_not_reached ();
		}
	}
	/* Remove all failed from packages, and add them to failed */
	if (tmp_failed) {
		for (iterator = tmp_failed; iterator; iterator = g_list_next (iterator)) {
			if (failed) {
				(*failed) = g_list_prepend (*failed, iterator->data);
			}
			(*packages) = g_list_remove (*packages, iterator->data);
		}
		g_list_free (tmp_failed);
	}
}

/* Given a filename, it find the 
   package that has a package in modifies that
   provides the given file. */

static int
eazel_install_package_modifies_provides_compare (PackageData *pack,
						 char *name)
{
	GList *ptr = NULL;
	ptr = g_list_find_custom (pack->modifies, 
				  (gpointer)name, 
				  (GCompareFunc)eazel_install_package_provides_compare);
	if (ptr) {
		trilobite_debug ("package %s-%s-%s caused harm to %s", 
				 pack->name, pack->version, pack->minor,
				 name);
		return 0;
	} 
/*
	for (ptr = pack->provides; ptr; ptr = g_list_next (ptr)) {
		g_message ("%s strcmp %s = %d", name, (char*)ptr->data, g_strcasecmp (name, (char*)ptr->data));
	}
	g_message ("package %s blows %d chunks", pack->name, g_list_length (pack->provides));
*/
	return -1;
}

static void
eazel_install_rpm_create_requirement (EazelInstall *service,
				      PackageData *pack,
				      PackageData *dep,
				      GList **requirements)
{
	g_assert (pack && dep);

	/* Check if a previous conflict solve has fixed this conflict. */
	if (g_list_find_custom (*requirements,
				dep,
				(GCompareFunc)eazel_install_requirement_dep_compare)) {
		trilobite_debug ("Already created requirement for %s", dep->name);
		packagedata_destroy (dep, FALSE);
		dep = NULL;
	} else {
		PackageRequirement *req;
		req = packagerequirement_new (pack, dep);				
		(*requirements) = g_list_prepend (*requirements, req);
				/* debug output code */
		if (dep->name) {
			trilobite_debug ("%s requires package %s", pack->name, dep->name);
		} else {
			trilobite_debug ("%s requires file %s", 
					 pack->name, 
					 (char*)dep->provides->data);
		}
	}
}
#endif

/* This is the function to do the RPM system dependency check */
void
eazel_install_do_rpm_dependency_check (EazelInstall *service,
				       GList **packages,
				       GList **failedpackages,
				       GList **requirements)
{
	g_assert_not_reached ();
#if 0
	int iterator;
	rpmTransactionSet set;
	int num_conflicts;
	struct rpmDependencyConflict *conflicts;
	struct rpmDependencyConflict conflict;
	rpmdb db;

	trilobite_debug ("eazel_install_do_rpm_dependency_check");

	g_assert (EAZEL_PACKAGE_SYSTEM_RPM3 (service->private->package_system)->private->dbs);

	eazel_package_system_rpm3_open_dbs (EAZEL_PACKAGE_SYSTEM_RPM3 (service->private->package_system));
	db = (rpmdb)g_hash_table_lookup (EAZEL_PACKAGE_SYSTEM_RPM3 (service->private->package_system)->private->dbs,
					 service->private->cur_root);
	if (!db) {
		return;
	}

	set =  rpmtransCreateSet (db, service->private->cur_root);
	eazel_install_add_to_rpm_set (service, set, packages, failedpackages); 

	/* Reorder the packages as per. deps and do the dep check */
	rpmdepOrder (set);		
	rpmdepCheck (set, &conflicts, &num_conflicts);
	eazel_package_system_rpm3_close_dbs (EAZEL_PACKAGE_SYSTEM_RPM3 (service->private->package_system));

	/* FIXME bugzilla.eazel.com 1512 (BUG OBSOLETED, DON'T REOPEN <eskil, 20010129>)
	   This piece of code is rpm specific. It has some generic algorithm
	   for doing the dep stuff, but it's rpm entangled */

	for (iterator = 0; iterator < num_conflicts; iterator++) {
		GList *pack_entry = NULL;
		PackageData *pack = NULL;
		PackageData *dep = NULL;

		conflict = conflicts[iterator];

		/* Locate the package that caused the conflict */
		pack_entry = g_list_find_custom (*packages, 
						 (char*)conflict.byName,
						 (GCompareFunc)eazel_install_package_name_compare);

		/* first time through, only do immediate matches */
		if (pack_entry == NULL) {
			continue;
		}

		pack = (PackageData*)pack_entry->data;
		/* Does the conflict look like a file dependency ? */
		if (*conflict.needsName=='/' || strstr (conflict.needsName, ".so")) {
			g_message (_("Processing dep for %s, requires library %s"), 
				   pack->name, conflict.needsName);		
			dep = packagedata_new ();
			dep->provides = g_list_append (dep->provides, g_strdup (conflict.needsName));
		} else {
			dep = packagedata_new_from_rpm_conflict (conflict);
			dep->archtype = g_strdup (pack->archtype);
			g_message (_("Processing dep for %s, requires package %s"), 
				   pack->name, 
				   dep->name);
		}

		eazel_install_rpm_create_requirement (service, pack, dep, requirements);
	}

	/* now iterate the HARD cases, life sucks! */
	for (iterator = 0; iterator < num_conflicts; iterator++) {
		GList *pack_entry = NULL;
		PackageData *pack = NULL;
		PackageData *dep = NULL;

		conflict = conflicts[iterator];

		/* Locate the package that caused the conflict */
		pack_entry = g_list_find_custom (*packages, 
						 (char*)conflict.byName,
						 (GCompareFunc)eazel_install_package_name_compare);
		if (pack_entry == NULL) {
			/* try our brand-new list of required packages, too */
			pack_entry = g_list_find_custom (*requirements,
							 (char*)conflict.byName,
							 (GCompareFunc)eazel_install_requirement_dep_name_compare);
			if (pack_entry != NULL) {
				trilobite_debug (_("package %s is already in requirements, whew!"), conflict.byName);
			}
		}
		if (pack_entry != NULL) {
			continue;
		}

		/* If we did not find it, we're in a special case conflict */
		switch (conflict.sense) {
		case RPMDEP_SENSE_REQUIRES: {				
				/* Possibly the implest case, we're installing package A, which requires
				   B that is not installed. */
			g_message (_("%s requires %s"), 
				   conflict.byName,
				   conflict.needsName);
			pack_entry = g_list_find_custom (*packages, 
							 (gpointer)conflict.needsName,
							 (GCompareFunc)eazel_install_package_name_compare);
			if (pack_entry==NULL) {
				/* If pack_entry is null, we're in the worse case, where
				   install A causes file f to disappear, and package conflict.byName
				   needs f (conflict.needsName). So conflict does not identify which
				   package caused the conflict */
				trilobite_debug ("pack_entry==NULL, level 2");
				/* 
				   I need to find the package P in "packages" that provides
				   conflict.needsName, then fail P marking it's status as 
				   PACKAGE_BREAKS_DEPENDENCY, then create PackageData C for
				   conflict.byName, add to P's depends and mark C's status as
				   PACKAGE_DEPENDENCY_FAIL. 
				   Then then client can rerun the operation with all the C's as
				   part of the update
				*/
				pack_entry = g_list_find_custom (*packages, 
								 (gpointer)conflict.needsName,
								 (GCompareFunc)eazel_install_package_modifies_provides_compare);
				if (pack_entry == NULL) {
					trilobite_debug ("pack_entry==NULL, level 3");
					/* Kühl, we probably already moved it to 
					   failed packages */
					pack_entry = g_list_find_custom (*failedpackages, 
									 (gpointer)conflict.needsName,
									 (GCompareFunc)eazel_install_package_modifies_provides_compare); 					
					if (pack_entry == NULL) {
						trilobite_debug ("pack_entry==NULL, level 4");
						/* Still kühl, we're looking for a name... */
						pack_entry = g_list_find_custom (*failedpackages, 
										 (gpointer)conflict.needsName,
										 (GCompareFunc)eazel_install_package_name_compare); 					
						if (pack_entry == NULL) {
							/* Massive debug output before I die.... */
							int a;
							GList *ptr;
							trilobite_debug ("This was certainly unexpected v5!");
							trilobite_debug ("*********************************");
							trilobite_debug ("Cannot lookup %s for %s", 
									 conflict.needsName,
									 conflict.byName);
							trilobite_debug ("Cannot lookup %s in *packages",
									 conflict.needsName);
							trilobite_debug ("Cannot lookup %s in *failedpackages",
									 conflict.needsName);
							
							trilobite_debug ("packages = 0x%p", packages);
							trilobite_debug ("*packages = 0x%p", *packages);
							trilobite_debug ("failedpackages = 0x%p", failedpackages);
							trilobite_debug ("*failedpackages = 0x%p", *failedpackages);
							
							trilobite_debug ("g_list_length (*packages) = %d", g_list_length (*packages));
							trilobite_debug ("g_list_length (*failedpackages) = %d", g_list_length (*failedpackages));
							a = 0;
							for (ptr = *packages; ptr; glist_step (ptr)) {
								PackageData *p = (PackageData*)ptr->data;
								a++;
								trilobite_debug ("(*packages)[%d] = %s-%s-%s",
										 a,
										 p->name,
										 p->version,
										 p->minor);
							}
							a = 0;
							for (ptr = *failedpackages; ptr; glist_step (ptr)) {
								PackageData *p = (PackageData*)ptr->data;
								a++;
								trilobite_debug ("(*failedpackages)[%d] = %s-%s-%s",
										 a,
										 p->name,
										 p->version,
										 p->minor);
							}
							
							
							g_assert_not_reached ();
						}
					} 
					/* If we reach this point, the package
					   was found in *failedpackages. Otherwise,
					   we would have hit the g_assert */
					trilobite_debug ("We don't want to redo failing it");
					pack_entry = NULL;
				}
			}
			
			if (pack_entry) {
				trilobite_debug ("pack_entry found");
				/* Create a packagedata for the dependecy */
				dep = packagedata_new_from_rpm_conflict_reversed (conflict);
				pack = (PackageData*)(pack_entry->data);
				dep->archtype = g_strdup (pack->archtype);
				pack->status = PACKAGE_BREAKS_DEPENDENCY;
				dep->status = PACKAGE_DEPENDENCY_FAIL;
				g_assert (dep!=NULL);
			} else {
				trilobite_debug ("pack_enty is NULL, continueing");
				continue;
			}
			
				/* Here I check to see if I'm breaking the -devel package, if so,
				   request it. It does a pretty generic check to see
				   if dep is on the form x-z and pack is x[-y] */
			
			if (eazel_install_check_if_related_package (service, pack, dep)) {
				trilobite_debug ("check_if_related_package returned TRUE");
				g_free (dep->version);
				dep->version = g_strdup (pack->version);
			} else {
				trilobite_debug ("check_if_related_package returned FALSE");
				/* not the devel package, are we in force mode ? */
				if (!eazel_install_get_force (service)) {
					/* if not, remove the package */
					packagedata_add_pack_to_breaks (pack, dep);
					if (g_list_find (*failedpackages, pack) == NULL) {
						(*failedpackages) = g_list_prepend (*failedpackages, pack);
					}
					(*packages) = g_list_remove (*packages, pack);
				}
				continue;
			}
		}
		break;
		case RPMDEP_SENSE_CONFLICTS:
				/* This should be set if there's a file conflict,
				   but I don't think rpm ever does that...
				   Because the code below is broken, I've inserted 
				   a g_assert_not_reached (eskil, Sept 2000)
				*/
			g_assert_not_reached ();
				/* If we end here, it's a conflict is going to break something */
			g_warning (_("Package %s conflicts with %s-%s"), 
				   conflict.byName, conflict.needsName, 
				   conflict.needsVersion);
			if (g_list_find (*failedpackages, pack) == NULL) {
				(*failedpackages) = g_list_prepend (*failedpackages, pack);
			}
			(*packages) = g_list_remove (*packages, pack);
			continue;
			break;
		}
		
		eazel_install_rpm_create_requirement (service, pack, dep, requirements);
	}

	rpmdepFreeConflicts (conflicts, num_conflicts);
	rpmtransFree (set);
	trilobite_debug ("eazel_install_do_rpm_dependency_check ended with %d fails and %d requirements", 
			 g_list_length (*failedpackages),
			 g_list_length (*requirements));
#endif
}
