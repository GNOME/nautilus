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
 * Authors: Eskil Heyn Olsen  <eskil@eazel.com>
 */

#include "eazel-install-logic2.h"
#include "eazel-install-public.h"
#include "eazel-install-private.h"

#include <libtrilobite/trilobite-core-utils.h>

#include <libnautilus-extensions/nautilus-debug.h>

/*
  0x1 enables post check_dependencies treedumps
  0x2 enables post dedupe tree dumps
  0x4 enables random spewing
  0x8 enables dumping the final tree
 */
#define EI2_DEBUG 0xff
#define PATCH_FOR_SOFTCAT_BUG 1
#define MUST_HAVE PACKAGE_FILL_NO_DIRS_IN_PROVIDES

/* 
   this is a debug thing that'll dump the memory addresses of all packages
   in trees, and thereby let you manually check whether the dedupe worked or not.
*/
#if EI2_DEBUG
static void
dump_tree_helper (GList *packages, char *indent, GList *path) 
{
	GList *iterator;
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack;
		char *tmp;
		char *name;

		if (IS_PACKAGEDATA (iterator->data)) {
			pack = PACKAGEDATA (iterator->data);
		} else {
			pack = (PACKAGEDEPENDENCY (iterator->data))->package;
		}

		name = packagedata_get_readable_name (pack);
		trilobite_debug ("%s%p (%s) %s", 
			   indent, 
			   pack, 
			   name, 
			   pack->fillflag & MUST_HAVE ? "yar":"arh");
		tmp = g_strdup_printf ("%s  ", indent);
		if (g_list_find_custom (path, name, (GCompareFunc)strcmp)) {
			trilobite_debug ("%s ... recurses ...", indent);
		} else {
			path = g_list_prepend (path, name);
			dump_tree_helper (pack->depends, tmp, path);
		}
		path = g_list_remove (path, name);
		g_free (name);
		g_free (tmp);
	}
}

static void
dump_tree (GList *packages) {
	dump_tree_helper (packages, "", NULL);
}
#endif


/***********************************************************************************/

/* Info collector */

void get_package_info (EazelInstall *service, GList *packages);
gboolean get_softcat_info (EazelInstall *service, PackageData **pack);


gboolean 
get_softcat_info (EazelInstall *service, 
		  PackageData **package)
{
	gboolean result = FALSE;

	g_assert (*package);
	g_assert (IS_PACKAGEDATA (*package));
	g_assert (service);
	g_assert (EAZEL_IS_INSTALL (service));

	if ((*package)->fillflag & MUST_HAVE) {
		/* Package is already filled */
		result = TRUE;
	} else {
		if ((*package)->filename) {
			if (g_file_test ((*package)->filename, G_FILE_TEST_ISFILE) &&
			    access ((*package)->filename, R_OK)==0) {
				(*package) = eazel_package_system_load_package (service->private->package_system,
										*package,
										(*package)->filename,
										MUST_HAVE);
				result = TRUE;
				
			}
		} else {
			EazelSoftCatError err;
			
			/* if package->version is set, we want get_info to get the exact 
			   version */
			eazel_softcat_set_authn_flag (service->private->softcat, FALSE);
			err = eazel_softcat_get_info (service->private->softcat,
						      *package,
						      EAZEL_SOFTCAT_SENSE_EQ,
						      MUST_HAVE);

			if ((err==EAZEL_SOFTCAT_SUCCESS) && ((*package)->fillflag & MUST_HAVE)) {
				result = TRUE;
			} else {
				result = FALSE;
			}
		}
	}
	if (result) {
		PackageData *p1;

		p1 = g_hash_table_lookup (service->private->dedupe_hash, (*package)->eazel_id);

		if (p1) {
			gtk_object_ref (GTK_OBJECT (p1));
			gtk_object_unref (GTK_OBJECT (*package)); 
			(*package) = p1;
		} else {
			g_hash_table_insert (service->private->dedupe_hash, 
					     (*package)->eazel_id, 
					     *package);
		}
	}
	return result;
}

static void 
get_package_info_foreach (PackageData *package,
			  EazelInstall *service)
{
	g_assert (service);
	g_assert (EAZEL_IS_INSTALL (service));
	g_assert (package);
	g_assert (IS_PACKAGEDATA (package));

	if (get_softcat_info (service, &package)==FALSE) {
		trilobite_debug ("Could not get info from SoftwareCatalog ");
	}
}

void
get_package_info (EazelInstall *service, 
		  GList *packages)
{
	g_assert (service);
	g_assert (EAZEL_IS_INSTALL (service));

	g_list_foreach (packages, (GFunc)get_package_info_foreach, service);
}

/***********************************************************************************/

/* This is the dedupe magic, which given a tree, links dupes (same eazel-id) to
   point to the same PackageData* object and unrefs the dupes */

void dedupe_foreach (PackageData *pack, EazelInstall *service);
void dedupe_foreach_depends (PackageDependency *d, EazelInstall *service);
void dedupe (EazelInstall *service, GList *packages);

void
dedupe_foreach_depends (PackageDependency *d,
			 EazelInstall *service) 
{
	PackageData *p1;

	g_assert (d);
	g_assert (IS_PACKAGEDEPENDENCY (d));
	g_assert (service);
	g_assert (EAZEL_IS_INSTALL (service));
	
	p1 = d->package;

	if (~p1->fillflag & MUST_HAVE) {
		PackageData *p11;
		p11 = g_hash_table_lookup (service->private->dedupe_hash, p1->eazel_id);
		if (p11) {
			gtk_object_ref (GTK_OBJECT (p11));
			gtk_object_unref (GTK_OBJECT (p1)); 
			d->package = p11;
		} else {
			g_hash_table_insert (service->private->dedupe_hash, p1->eazel_id, p1);
			dedupe_foreach (p1, service);
		}
	}
}

void
dedupe_foreach (PackageData *package,
		 EazelInstall *service)
{
	g_assert (package);
	g_assert (IS_PACKAGEDATA (package));
	g_assert (service);
	g_assert (EAZEL_IS_INSTALL (service));

	g_list_foreach (package->depends, (GFunc)dedupe_foreach_depends, service);
} 

void
dedupe (EazelInstall *service,
	 GList *packages)
{
	g_assert (service);
	g_assert (EAZEL_IS_INSTALL (service));

	g_list_foreach (packages, (GFunc)dedupe_foreach, service);
#if EI2_DEBUG & 0x2
	trilobite_debug ("''yar'' is piratese for yes, ''arh'' for no");
	dump_tree (packages);
#endif
}

/***********************************************************************************/

/* This is the code to remove already satisfied dependencies */

void check_dependencies (EazelInstall *service, GList *packages);
void check_dependencies_foreach (PackageData *package, EazelInstall *service);
gboolean is_satisfied (EazelInstall *service, PackageDependency *dep);
gboolean is_satisfied_features (EazelInstall *service, GList *features);

gboolean
is_satisfied (EazelInstall *service, 
	      PackageDependency *dep)
{
	g_assert (dep);
	g_assert (IS_PACKAGEDEPENDENCY (dep));
	g_assert (service);
	g_assert (EAZEL_IS_INSTALL (service));

#if EI2_DEBUG & 0x4
	trilobite_debug ("is_satisfied %p %s", dep->package, dep->package->name);
#endif

	if (g_hash_table_lookup (service->private->dep_ok_hash, dep->package->eazel_id)) {
		return TRUE;
	} else {
		gboolean result = FALSE;

		if (dep->version) {
			if (eazel_package_system_is_installed (service->private->package_system,
							       service->private->cur_root,
							       dep->package->name,
							       dep->version,
							       NULL,
							       dep->sense)) {
				result = TRUE;
			}
		} else {
			if (is_satisfied_features (service, dep->package->features)) {
				result = TRUE;
			}
		}
		if (result) {
			g_hash_table_insert (service->private->dep_ok_hash, 
					     dep->package->eazel_id,
					     dep->package);
			return TRUE;
		}
	}
	return FALSE;
}

gboolean
is_satisfied_features (EazelInstall *service, 
		       GList *features)
{
	gboolean result = TRUE;
	GList *iterator;

	g_assert (service);
	g_assert (EAZEL_IS_INSTALL (service));

	for (iterator = features; iterator && result; iterator = g_list_next (iterator)) {
		GList *query_result;
		char *f = (char*)iterator->data;

		query_result = eazel_package_system_query (service->private->package_system,
							   service->private->cur_root,
							   f,
							   EAZEL_PACKAGE_SYSTEM_QUERY_PROVIDES,
							   PACKAGE_FILL_MINIMAL);
		if (g_list_length (query_result) == 0) {
			query_result = eazel_package_system_query (service->private->package_system,
								   service->private->cur_root,
								   f,
								   EAZEL_PACKAGE_SYSTEM_QUERY_OWNS,
								   PACKAGE_FILL_MINIMAL);
			if (g_list_length (query_result) == 0) {
#if EI2_DEBUG & 0x4
				trilobite_debug ("noone owns or provides %s", f);
#endif
				result = FALSE;

/* FIXME: forseti.eazel.com: 1129
   This is a workaround for a softcat bug. When bug 1129
   is closed, remove this evilness */
#if PATCH_FOR_SOFTCAT_BUG
				if (eazel_package_system_is_installed (service->private->package_system,
								       service->private->cur_root,
								       f, NULL, NULL,
								       EAZEL_SOFTCAT_SENSE_ANY)) {
					trilobite_debug ("but a package called %s exists", f);
					result = TRUE;
				}
#endif
			}
		}
		g_list_foreach (query_result, (GFunc)packagedata_destroy, GINT_TO_POINTER (TRUE)); 
		g_list_free (query_result);
	}

	return result;
}

void 
check_dependencies_foreach (PackageData *package, 
			    EazelInstall *service)
{
	GList *remove = NULL;
	GList *iterator;
	
	g_assert (service);
	g_assert (EAZEL_IS_INSTALL (service));
	g_assert (package);
	g_assert (IS_PACKAGEDATA (package));

#if EI2_DEBUG & 0x4
	trilobite_debug ("check deps for %p %s", package, package->name);
#endif

	for (iterator = package->depends; iterator; iterator = g_list_next (iterator)) {
		PackageDependency *dep = PACKAGEDEPENDENCY (iterator->data);
		eazel_install_emit_dependency_check (service, package, dep);
		if (is_satisfied (service, dep)) {
			remove = g_list_prepend (remove, dep);
		}
	}

	for (iterator = remove; iterator; iterator = g_list_next (iterator)) {
		PackageDependency *dep = PACKAGEDEPENDENCY (iterator->data);
		package->depends = g_list_remove (package->depends, dep);
		trilobite_debug ("removing %p %s from %p %s", dep->package, dep->package->name, package, package->name);
		/* packagedependency_destroy (dep, TRUE); */
	}
	g_list_free (remove);
	
}

void
check_dependencies (EazelInstall *service,
		    GList *packages) 
{
	g_assert (service);
	g_assert (EAZEL_INSTALL (service));
	
	g_list_foreach (packages, (GFunc)check_dependencies_foreach, service);

#if EI2_DEBUG & 0x1
	trilobite_debug ("post dep_check tree");
	dump_tree (packages);
#endif
}

/***********************************************************************************/

/* This is the main file conflicts check hell */

void do_file_conflict_check (EazelInstall *service, GList *packages);
void no_two_packages_with_same_file (EazelInstall *service, GList *packages);
void feature_consistency_check (EazelInstall *service, GList *packages);
void check_conflicts_with_other (EazelInstall *service, GList *packages);

void 
no_two_packages_with_same_file (EazelInstall *service, 
				GList *packages)
{
	/* FIXME: bugzilla.eazel.com 5275
	   add code to check that no two packages provides the same file 

	hash<char*, PackageData> Hfile;

	for each p in packages {
		for each f in p->provides {
			if p'=Hfile[f] {	
				# f is provided by p'
				whine (about p' conflicting with p over file f)
			} else {
				Hfile[f] = p;
			}
		}
	}
	*/

}

void 
check_conflicts_with_other (EazelInstall *service, 
			    GList *packages)
{
	/* FIXME: bugzilla.eazel.com 5276
	   Add code to do conflict checking against already installed packages.

	for each p in packages {
		for each f in p->provides {
			L = query (PROVIDES, f)
			if L.size > 0 {
				# someone else might own f
				foreach p' in L {
					if p' in packages (name compare && p->provides[f]==null) {
						# we're upgrading p' to p'' which doesn't have f, 
						# so all is well 
					} else {
						whine (p might break p' since they share f);
					}
				}
			} else {
				# f is fine, noone else owns it 
			}
		}
	*/
	
}

void 
feature_consistency_check (EazelInstall *service, 
			   GList *packages)
{
	/* FIXME: bugzilla.eazel.com 5277
	   add code to check that no features are lost, and if, that they're not required

	hash<char*, PackageData> Hfeat;

	for each p in packages {
		for each f in p->features {
			Hfeat[f] = p;
		}
	}

	for each p in packages {
		for each p' in p->modifies {
			for each f in p'->features {
				if Hfeat[f] {
					# all is well, feature is still on system 
				} else {
					# feature is lost 
					L = query (REQUIRES, f);
					if l.size > 0 {
						# and someone requires this feature 
						whine (p updates p' which breaks packages in L);
					}
				}
			}
		}
	}
	*/
}

void
do_file_conflict_check (EazelInstall *service, 
			GList *packages)
{
	no_two_packages_with_same_file (service, packages);
	check_conflicts_with_other (service, packages);
	feature_consistency_check (service, packages);
}


/***********************************************************************************/

/* This is the main dependency check function */

void do_dep_check (EazelInstall *service, GList *packages);

void
do_dep_check (EazelInstall *service,
	      GList *packages)
{
	GList *iterator;
	GList *K = NULL;

	g_assert (service);
	g_assert (EAZEL_IS_INSTALL (service));

	if (packages==NULL) {
		return;
	}

#if EI2_DEBUG & 0x1
	trilobite_debug ("do_dep_check tree");
	dump_tree (packages);
#endif

	get_package_info (service, packages);
	dedupe (service, packages);
	check_dependencies (service, packages);

	/* Build the K list, consisting of packages without must_have set */
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
	        GList *diterator;
		for (diterator = PACKAGEDATA(iterator->data)->depends; diterator; diterator = g_list_next (diterator)) {
			PackageDependency *dep = PACKAGEDEPENDENCY (diterator->data);
			if (~dep->package->fillflag & MUST_HAVE) {
				K = g_list_prepend (K, dep->package);
			}
		}
	}
	/* and process them */
	if (K) {
		do_dep_check (service, K);
	}
}

/***********************************************************************************/

/* These are the methods exposed to the the rest of the service object */

EazelInstallOperationStatus 
install_packages (EazelInstall *service, GList *categories)
{
	EazelInstallOperationStatus result = EAZEL_INSTALL_NOTHING;       	
	GList *packages;

	packages = packagedata_list_copy (categorylist_flatten_to_packagelist (categories), TRUE);
	do_dep_check (service, packages);
	do_file_conflict_check (service, packages);

#if EI2_DEBUG & 0x8
	trilobite_debug ("FINAL TREE BEGIN");
	dump_tree (packages);
	trilobite_debug ("FINAL TREE END");
#endif
	
	/* FIXME: bugzilla.eazel.com 5264, 5266, 5267
	   finish this cruft by 
	   1: traversing the tree to see if anything failed/broke (5264)
	   2 if failed, emit_failed on the root, and remove root from tree
	     3: if any broke any other, add the others to tree and recurse
	   else
	     4: traverse tree and download packages (5266)
	     5: force install... (5267)
	*/

	g_list_foreach (packages, (GFunc)packagedata_destroy, GINT_TO_POINTER (TRUE));
	return result;
}

EazelInstallOperationStatus 
uninstall_packages (EazelInstall *service, GList *categories)
{
	EazelInstallOperationStatus result = EAZEL_INSTALL_NOTHING;
	return result;
}

EazelInstallOperationStatus 
revert_transaction (EazelInstall *service, GList *packages)
{
	EazelInstallOperationStatus result = EAZEL_INSTALL_NOTHING;
	return result;
}
