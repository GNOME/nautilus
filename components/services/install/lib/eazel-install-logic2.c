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
 *          Robey Pointer <robey@eazel.com>
 */

#include "eazel-install-logic2.h"
#include "eazel-install-public.h"
#include "eazel-install-private.h"

#include <libtrilobite/trilobite-core-utils.h>
#include <libtrilobite/trilobite-md5-tools.h>

#include <libnautilus-extensions/nautilus-debug.h>

#ifndef EAZEL_INSTALL_NO_CORBA
#include <libtrilobite/libtrilobite.h>
#else
#include <libtrilobite/libtrilobite-service.h>
#include <libtrilobite/trilobite-root-helper.h>
#endif

/*
  0x1 enables post check_dependencies treedumps
  0x2 enables post dedupe tree dumps
  0x4 enables random spewing
  0x8 enables dumping the final tree
 */
#define EI2_DEBUG 0xff
#define PATCH_FOR_SOFTCAT_BUG 1
#define MUST_HAVE PACKAGE_FILL_NO_DIRS_IN_PROVIDES

enum {
	DEPENDENCY_OK = 1,
	DEPENDENCY_NOT_OK = 2
};

/***********************************************************************************/
/* 
   this is a debug thing that'll dump the memory addresses of all packages
   in trees, and thereby let you manually check whether the dedupe worked or not.
*/
#if EI2_DEBUG & 0x10
static void
dump_tree_helper (GList *packages, char *indent, GList *path) 
{
	GList *iterator;
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = NULL;
		char *tmp;
		char *name;

		if (IS_PACKAGEDATA (iterator->data)) {
			pack = PACKAGEDATA (iterator->data);
		} else {
			pack = (PACKAGEDEPENDENCY (iterator->data))->package;
		}
		
		name = packagedata_get_readable_name (pack);
		trilobite_debug ("%s%p (%s) %s%s", 
				 indent, 
				 pack, 
				 name, 
				 pack->fillflag & MUST_HAVE ? "filled":"not_filled",
				 pack->status == PACKAGE_CANNOT_OPEN ? " but failed" : "");
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

/* This is code to check the md5 of downloaded packages */


gboolean
check_md5_on_files (EazelInstall *service, 
		    GList *packages)
{
	gboolean result = TRUE;
	GList *iterator;
	GList *flat_packages;

	result = eazel_install_lock_tmp_dir (service);

	if (!result) {
		g_warning (_("Failed to lock the downloaded file"));
		return FALSE;
	}

	flat_packages = flatten_packagedata_dependency_tree (packages);

	for (iterator = flat_packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		
		if (pack->md5) {
			char pmd5[16];
			char md5[16];

			trilobite_md5_get_digest_from_file (pack->filename, md5);
			trilobite_md5_get_digest_from_md5_string (pack->md5, pmd5);

			if (memcmp (pmd5, md5, 16) != 0) {
				g_warning (_("MD5 mismatch, package %s may be compromised"), pack->name);
				trilobite_debug ("read md5 from file %s", pack->filename);
				trilobite_debug ("for package %s version %s", pack->name, pack->version);
				eazel_install_emit_md5_check_failed (service, 
								     pack, 
								     trilobite_md5_get_string_from_md5_digest (md5));
				result = FALSE;
			} else {
				trilobite_debug ("md5 match on %s", pack->name);
			}
		} else {
			trilobite_debug ("No MD5 available for %s", pack->name);
		}
	}	
	g_list_free (flat_packages);

	return result;
}

/***********************************************************************************/

/* This is code to removed failed trees from the tree */

void prune_failed_packages (EazelInstall *service, GList **packages);
void prune_failed_packages_helper (EazelInstall *service, PackageData *root, 
				   PackageData *pack, GList *packages, GList **result);

void 
prune_failed_packages_helper (EazelInstall *service, 
			      PackageData *root, 
			      PackageData *pack, 
			      GList *packages,
			      GList **result)
{
	GList *iterator;

#if EI2_DEBUG & 0x4
	trilobite_debug ("entering subpruner %p %s %s", 
			 pack, pack->name, 
			 packagedata_status_enum_to_str (pack->status));
#endif

	if (pack->status != PACKAGE_PARTLY_RESOLVED) {
#if EI2_DEBUG & 0x4
		trilobite_debug ("subpruner kill root %s because of %s", root->name, pack->name);
#endif
		(*result) = g_list_prepend (*result, root);
	} else {
		for (iterator = pack->depends; iterator; iterator = g_list_next (iterator)) {
			PackageDependency *dep = PACKAGEDEPENDENCY (iterator->data);
			prune_failed_packages_helper (service, root, dep->package, packages, result);
		}
	}
}

void 
prune_failed_packages (EazelInstall *service, 
		       GList **packages)
{
	GList *iterator;
	GList *result = NULL;

	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = PACKAGEDATA (iterator->data);
		prune_failed_packages_helper (service, pack, pack, *packages, &result);
	}

	for (iterator = result; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = PACKAGEDATA (iterator->data);

#if EI2_DEBUG & 0x4
		trilobite_debug ("removing %p %s", pack, pack->name);
#endif
		(*packages) = g_list_remove (*packages, pack);
		eazel_install_emit_install_failed (service, pack);
		gtk_object_unref (GTK_OBJECT (pack));
	}
	if (g_list_length (*packages) == 0) {
#if EI2_DEBUG & 0x4
		trilobite_debug ("packages is empty after prune");
#endif		
	}
	g_list_free (result);

#if EI2_DEBUG & 0x2
	dump_tree (*packages);
#endif
}


/***********************************************************************************/

/* Code to check already installed packages */

EazelInstallStatus
eazel_install_check_existing_packages (EazelInstall *service, 
				       PackageData *pack)
{
	GList *existing_packages;
	EazelInstallStatus result;

	result = EAZEL_INSTALL_STATUS_NEW_PACKAGE;
	/* query for existing package of same name */
	existing_packages = eazel_package_system_query (service->private->package_system,
							service->private->cur_root,
							pack->name,
							EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
							PACKAGE_FILL_NO_DIRS_IN_PROVIDES);
	if (existing_packages) {
		/* Get the existing package, set it's modify flag and add it */
		GList *iterator;
		if (g_list_length (existing_packages)>1) {
			trilobite_debug ("there are %d existing packages called %s",
					 g_list_length (existing_packages),
					 pack->name);
			trilobite_debug ("*********************************************************");
			trilobite_debug ("This is a bad bad case, see bug 3511");
			trilobite_debug ("To circumvent this problem, as root, execute this command");
			trilobite_debug ("(which is dangerous by the way....)");
			trilobite_debug ("rpm -e --nodeps `rpm -q %s`", pack->name);
			trilobite_debug ("Or wait for the author to fix bug 3511");
			/* FIXME bugzilla.eazel.com 3511
			   g_assert_not_reached ();
			*/
		}
		for (iterator = existing_packages; iterator; iterator = g_list_next (iterator)) {
			PackageData *existing_package = (PackageData*)iterator->data;
			int res;

			if (g_list_find_custom (pack->modifies, 
						existing_package->name,
						(GCompareFunc)eazel_install_package_name_compare)) {
#if EI2_DEBUG & 0x4
				trilobite_debug ("%s already marked as modified", existing_package->name);
#endif
				packagedata_destroy (existing_package, TRUE);
				existing_package = NULL;
				continue;
			}

			g_assert (pack->version);
			g_assert (existing_package->version);

			res = eazel_package_system_compare_version (service->private->package_system,
								    pack->version, 
								    existing_package->version);
			
			/* check against minor version */
			if (res==0) {
#if EI2_DEBUG & 0x4
				trilobite_debug ("versions are equal, comparing minors");
#endif
				if (pack->minor && existing_package->minor) {
#if EI2_DEBUG & 0x4
					trilobite_debug ("minors are %s and %s (installed)", 
							 pack->minor, existing_package->minor);
#endif
					res = eazel_package_system_compare_version (service->private->package_system,
										    pack->minor, existing_package->minor);
				} else if (!pack->minor && existing_package->minor) {
					/* If the given packages does not have a minor,
					   but the installed has, assume we're updated */
					res = 1;
				}
			}

			/* Set the modify_status flag */
			if (res == 0) {
				existing_package->modify_status = PACKAGE_MOD_UNTOUCHED;
			} else if (res > 0) {
				existing_package->modify_status = PACKAGE_MOD_UPGRADED;
			} else {
				existing_package->modify_status = PACKAGE_MOD_DOWNGRADED;
			}

			/* Calc the result */
			if (res == 0) {
				result = EAZEL_INSTALL_STATUS_QUO;
			} else if (res > 0) {
				result = EAZEL_INSTALL_STATUS_UPGRADES;
			} else {
				result = EAZEL_INSTALL_STATUS_DOWNGRADES;
			}
		
#if EI2_DEBUG & 0x4
			/* Debug output */ 
			switch (result) {
			case EAZEL_INSTALL_STATUS_QUO: {
				if (pack->minor && existing_package->minor) {
					trilobite_debug (_("%s-%s version %s-%s already installed"), 
							 pack->name, pack->minor, 
							 existing_package->version, existing_package->minor);
				} else {
					trilobite_debug (_("%s version %s already installed"), 
							 pack->name, 
							 existing_package->version);
				}
			} 
			break;
			case EAZEL_INSTALL_STATUS_UPGRADES: {
				/* This is certainly ugly as helll */
				if (pack->minor && existing_package->minor) {
					trilobite_debug (_("%s upgrades from version %s-%s to %s-%s"),
							 pack->name, 
							 existing_package->version, existing_package->minor, 
							 pack->version, pack->minor);
				} else {
					trilobite_debug (_("%s upgrades from version %s-%s to %s-%s"),
							 pack->name, 
							 existing_package->version, existing_package->minor, 
							 pack->version, pack->minor);
				}
			}
			break;
			case EAZEL_INSTALL_STATUS_DOWNGRADES: {
				if (pack->minor && existing_package->minor) {
					trilobite_debug (_("%s downgrades from version %s-%s to %s-%s"),
							 pack->name, 
							 existing_package->version, existing_package->minor, 
							 pack->version, pack->minor);
				} else {
					trilobite_debug (_("%s downgrades from version %s to %s"),
							 pack->name, 
							 existing_package->version, 
							 pack->version);
				}
			}
			break;
			default:
				break;
			}
#endif				
			/* Set modifies list */
			if (result != EAZEL_INSTALL_STATUS_QUO) {
#if EI2_DEBUG & 0x4
				trilobite_debug ("%p %s modifies %p %s",
						 pack, pack->name,
						 existing_package, existing_package->name);
#endif		

				packagedata_add_pack_to_modifies (pack, existing_package);
				existing_package->status = PACKAGE_RESOLVED;
			} else {
				pack->status = PACKAGE_ALREADY_INSTALLED;
			}
			//gtk_object_unref (GTK_OBJECT (existing_package));
		}

		/* Free the list structure from _simple_query */
		g_list_free (existing_packages);
	} else {
#if EI2_DEBUG & 0x4
		if (pack->minor) {
			trilobite_debug (_("%s installs version %s-%s"), 
					 pack->name, 
					 pack->version,
					 pack->minor);
		} else {
			trilobite_debug (_("%s installs version %s"), 
					 pack->name, 
					 pack->version);
		}
#endif
	}

	return result;
}

/***********************************************************************************/

/* Info collector */

typedef enum {
	NO_SOFTCAT_HIT = 0,
	SOFTCAT_HIT_OK  = 1,
	PACKAGE_SKIPPED = 2
} GetSoftCatResult;

void get_package_info (EazelInstall *service, GList *packages);
GetSoftCatResult get_softcat_info (EazelInstall *service, PackageData **pack);

GetSoftCatResult
get_softcat_info (EazelInstall *service, 
		  PackageData **package)
{
	GetSoftCatResult result = NO_SOFTCAT_HIT;

	g_assert (*package);
	g_assert (IS_PACKAGEDATA (*package));
	g_assert (service);
	g_assert (EAZEL_IS_INSTALL (service));

	if ((*package)->fillflag & MUST_HAVE) {
		/* Package is already filled */
		result = SOFTCAT_HIT_OK;
	} else {
		if ((*package)->filename) {
			if (g_file_test ((*package)->filename, G_FILE_TEST_ISFILE) &&
			    access ((*package)->filename, R_OK)==0) {
				(*package) = eazel_package_system_load_package (service->private->package_system,
										*package,
										(*package)->filename,
										MUST_HAVE);
				result = SOFTCAT_HIT_OK;
				
			}
		} else {
			EazelSoftCatError err;
			
			/* if package->version is set, we want get_info to get the exact 
			   version */
			err = eazel_softcat_get_info (service->private->softcat,
						      *package,
						      EAZEL_SOFTCAT_SENSE_EQ,
						      MUST_HAVE);

			if ((err==EAZEL_SOFTCAT_SUCCESS) && ((*package)->fillflag & MUST_HAVE)) {
				result = SOFTCAT_HIT_OK;
				(*package)->status = PACKAGE_PARTLY_RESOLVED;
			} else {
				result = NO_SOFTCAT_HIT;
				(*package)->status = PACKAGE_CANNOT_OPEN;
			}
		}
	}
	if (result != NO_SOFTCAT_HIT) {
		PackageData *p1;

		p1 = g_hash_table_lookup (service->private->dedupe_hash, (*package)->eazel_id);

		if (p1) {
			gtk_object_ref (GTK_OBJECT (p1));
			gtk_object_unref (GTK_OBJECT (*package)); 
			(*package) = p1;
		} else {
			EazelInstallStatus status;

			status = eazel_install_check_existing_packages (service, *package);
			switch (status) {
			case EAZEL_INSTALL_STATUS_NEW_PACKAGE:
			case EAZEL_INSTALL_STATUS_UPGRADES:
				gtk_object_ref (GTK_OBJECT (*package));
				g_hash_table_insert (service->private->dedupe_hash, 
						     (*package)->eazel_id, 
						     *package);
				break;
			case EAZEL_INSTALL_STATUS_DOWNGRADES:
			case EAZEL_INSTALL_STATUS_QUO:
				gtk_object_unref (GTK_OBJECT (*package));
				result = PACKAGE_SKIPPED;
				break;
			}
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
	
        switch (get_softcat_info (service, &package)) {
	case NO_SOFTCAT_HIT:
		trilobite_debug ("Could not get info from SoftwareCatalog ");
		break;
	case PACKAGE_SKIPPED:
#if EI2_DEBUG & 0x4
		trilobite_debug ("Package skipped");
#endif
		break;
	default:
#if EI2_DEBUG & 0x4
		trilobite_debug ("Softcat check ok");
#endif 		
		break;
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
			gtk_object_ref (GTK_OBJECT (p1));
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
	char *key;
	int previous_check_state;

	g_assert (dep);
	g_assert (IS_PACKAGEDEPENDENCY (dep));
	g_assert (service);
	g_assert (EAZEL_IS_INSTALL (service));

	if (dep->version != NULL) {
		char *sense_str = eazel_softcat_sense_flags_to_string (dep->sense);
#if EI2_DEBUG & 0x4
		trilobite_debug ("is_satisfied? %p %s %s %s", dep->package, dep->package->name, sense_str, dep->version);
#endif
		key = g_strdup_printf ("%s-%s-%s", dep->package->eazel_id, sense_str, dep->version);
		g_free (sense_str);
	} else {
#if EI2_DEBUG & 0x4
		trilobite_debug ("is_satisfied? %p %s", dep->package, dep->package->name);
#endif
		key = g_strdup (dep->package->eazel_id);
	}

	previous_check_state = GPOINTER_TO_INT (g_hash_table_lookup (service->private->dep_ok_hash, key));
	switch (previous_check_state) {
	case DEPENDENCY_OK: {
#if EI2_DEBUG & 0x4
		trilobite_debug ("\t--> dep hash ok");
#endif
		return TRUE;
		break;
	}
	case DEPENDENCY_NOT_OK: {
#if EI2_DEBUG & 0x4
		trilobite_debug ("\t--> dep hash failed");
#endif
		return FALSE;
		break;
	}
	default: {
		gboolean result = FALSE;
		
		if (dep->version) {
			if (eazel_package_system_is_installed (service->private->package_system,
							       service->private->cur_root,
							       dep->package->name,
							       dep->version,
							       NULL,
							       dep->sense)) {
#if EI2_DEBUG & 0x4
				trilobite_debug ("\t--> installed");
#endif
				result = TRUE;
			}
		} else {
			if (is_satisfied_features (service, dep->package->features)) {
#if EI2_DEBUG & 0x4
				trilobite_debug ("\t--> feature satisfied");
#endif
				result = TRUE;
			}
		}
		if (result) {
			g_hash_table_insert (service->private->dep_ok_hash, 
					     key,
					     GINT_TO_POINTER (DEPENDENCY_OK));
			return TRUE;
		} else {
#if EI2_DEBUG & 0x4
			trilobite_debug ("\t--> feature not satisfied");
#endif
			g_hash_table_insert (service->private->dep_ok_hash, 
					     key,
					     GINT_TO_POINTER (DEPENDENCY_NOT_OK));
			
			return FALSE;
		}
	}
	}
}

gboolean
is_satisfied_features (EazelInstall *service, 
		       GList *features)
{
	gboolean result = TRUE;
	GList *iterator;

	g_assert (service);
	g_assert (EAZEL_IS_INSTALL (service));

	trilobite_debug ("is_satisfied_features %d features", g_list_length (features));

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
		g_list_foreach (query_result, (GFunc)gtk_object_unref, NULL); 
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
		if (is_satisfied (service, dep)) {
			remove = g_list_prepend (remove, dep);
		} else {
			eazel_install_emit_dependency_check (service, package, dep);
		}
	}

	for (iterator = remove; iterator; iterator = g_list_next (iterator)) {
		PackageDependency *dep = PACKAGEDEPENDENCY (iterator->data);
		package->depends = g_list_remove (package->depends, dep);
		trilobite_debug ("removing %p %s from %p %s", dep->package, dep->package->name, package, package->name);
		packagedependency_destroy (dep);
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

void do_file_conflict_check (EazelInstall *service, GList **packages, GList **extra_packages);
void check_no_two_packages_has_same_file (EazelInstall *service, GList *packages);
void check_feature_consistency (EazelInstall *service, GList *packages);
void check_conflicts_against_already_installed_packages (EazelInstall *service, GList *packages);
void check_tree_for_conflicts (EazelInstall *service, GList **packages, GList **extra_packages);

static void
check_tree_helper (EazelInstall *service, 
		   PackageData *pack,
		   GList **extra_packages)
{
	GList *iterator;

#if EI2_DEBUG & 0x4
	trilobite_debug ("-> check_tree_for_conflicts_helper");
#endif
	if (pack->status == PACKAGE_FILE_CONFLICT) {
#if EI2_DEBUG & 0x4
		trilobite_debug ("reviving %s", pack->name);
#endif
		pack->status = PACKAGE_PARTLY_RESOLVED;
		for (iterator = pack->breaks; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack_broken = PACKAGEDATA (iterator->data);
			/* reset pack_broken to some sane values */
			pack_broken->status = PACKAGE_UNKNOWN_STATUS;
			pack_broken->fillflag = PACKAGE_FILL_INVALID;
			g_free (pack_broken->version);
			pack_broken->version = NULL;
			g_free (pack_broken->minor);
			pack_broken->minor = NULL;
			(*extra_packages) = g_list_prepend (*extra_packages, pack_broken);
		}
		g_list_free (pack->breaks);
		pack->breaks = NULL;
	}
	
	for (iterator = pack->depends; iterator; iterator = g_list_next (iterator)) {
		PackageDependency *dep = PACKAGEDEPENDENCY (iterator->data);
		check_tree_helper (service, dep->package, extra_packages);
	}
#if EI2_DEBUG & 0x4
	trilobite_debug ("<- check_tree_for_conflicts_helper");
#endif
}

/* 
   Scan the tree for file conflicts. Add the broken packages to extra_packages
   and reset the status flag on the offending package. Since the prune_failed_packages
   is called at every step, we won't risk resetting a borked package to ok.
 */
void 
check_tree_for_conflicts (EazelInstall *service, 
			  GList **packages, 
			  GList **extra_packages)
{
	GList *iterator;
#if EI2_DEBUG & 0x4
	trilobite_debug ("-> check_tree_for_conflicts");
#endif
	for (iterator = g_list_first (*packages); iterator != NULL; iterator = g_list_next (iterator)) {
		PackageData *pack = PACKAGEDATA (iterator->data);
		check_tree_helper (service, pack, extra_packages);
	}
#if EI2_DEBUG & 0x4
	trilobite_debug ("<- check_tree_for_conflicts");
#endif
}

/* make sure none of the packages we're installing will share files

   add code to check that no two packages provides the same file 

	hash<char*, PackageData> Hfile;

	for each p in packages {
		for each f in p->provides {
			if p'=Hfile[f] {	
				# f is provided by p'
				whine (about p' conflicting with p over file f)
				set: p breaks p'
			} else {
				Hfile[f] = p;
			}
		}
	}

   (rough draft by robey:)
*/
void 
check_no_two_packages_has_same_file (EazelInstall *service, 
				     GList *packages)
{
	GHashTable *file_table;		/* filename(char *) -> package(PackageData *) */
	GList *broken_packages;		/* (PackageData *) packages known to have conflicts already */
	GList *iter, *iter_file;
	PackageData *pack, *pack_other;
	char *filename;
	GList *flat_packages;

	broken_packages = NULL;
	flat_packages = flatten_packagedata_dependency_tree (packages);

	if (eazel_install_get_force (service) ||
	    eazel_install_get_ignore_file_conflicts (service) ||
	    (g_list_length (flat_packages) == 1)) {
		trilobite_debug ("(not performing duplicate file check)");
		g_list_free (flat_packages);
		return;
	}

	file_table = g_hash_table_new (g_str_hash, g_str_equal);

#if EI2_DEBUG & 0x4
	trilobite_debug ("-> no_two_packages conflict check begins (%d packages)", g_list_length (flat_packages));
#endif
	
	for (iter = g_list_first (flat_packages); iter != NULL; iter = g_list_next (iter)) {
		gboolean reported_yet = FALSE;
		int other_conflicts = 0;

		pack = PACKAGEDATA (iter->data);

#if EI2_DEBUG & 0x4
		trilobite_debug ("checking %s", pack->name);
#endif
		for (iter_file = g_list_first (pack->provides); iter_file != NULL; iter_file = g_list_next (iter_file)) {
			filename = (char *)(iter_file->data);

			/* FIXME: bugzilla.eazel.com 5720
			   this is a patch to circumvent unwanted behaviour.
			   Softcat doens't strip directories when giving NO_DIRS_IN_PROVIDES as fillflag,
			   till it does, I use this check */
			if (g_file_test (filename, G_FILE_TEST_ISDIR)) {
				continue;
			}

			pack_other = g_hash_table_lookup (file_table, filename);
			if (pack_other != NULL) {
				/* Dang, this file is provided by both 'pack' and 'pack_other' */
				/* Only report it once in the debug log or we'll spam to eternity on some
				 * large broken packages... */
				if (! reported_yet) {
					trilobite_debug ("file conflict 1: %s from package %s is also in %s",
							 filename, pack->name, pack_other->name);
					reported_yet = TRUE;
				} else {
					other_conflicts++;
				}

				/* 'pack' broke 'pack_other', but only if 'pack_other' isn't already broken */
				if (! g_list_find (broken_packages, pack_other)) {
					broken_packages = g_list_prepend (broken_packages, pack_other);
					packagedata_add_pack_to_breaks (pack, pack_other);
					pack->status = PACKAGE_FILE_CONFLICT;
					pack_other->status = PACKAGE_FILE_CONFLICT;
				}
			} else {
				/* file is okay */
				g_hash_table_insert (file_table, filename, pack);
			}
		}
		if (other_conflicts) {
			trilobite_debug ("(there were %d other conflicts)", other_conflicts);
		}
	}

	/* let's free all this crap, unlike last time (cough cough) :) */
	g_list_free (broken_packages);
	g_list_free (flat_packages);
	g_hash_table_destroy (file_table);	

#if EI2_DEBUG & 0x4
	trilobite_debug ("<- no_two_packages conflict");
#endif
}

/* determine if package 'pack' is being upgraded in the 'packages' list,
 * and that upgrade no longer needs 'filename'
 */
static gboolean
package_is_upgrading_and_doesnt_need_file (PackageData *pack, GList *packages, const char *filename)
{
	PackageData *pack_upgrade;
	GList *item, *item2;

	item = g_list_find_custom (packages, pack->name,
				   (GCompareFunc)eazel_install_package_name_compare);
	if (item != NULL) {
		pack_upgrade = PACKAGEDATA (item->data);
		item2 = g_list_find_custom (pack_upgrade->provides, (char *)filename, (GCompareFunc)strcmp);
		if (item2 == NULL) {
			/* package IS in the package list, and does NOT provide this file anymore */
			return TRUE;
		}
	}
	return FALSE;
}

/* check if any of our packages contain files that would conflict with packages already installed.

   Add code to do conflict checking against already installed packages.

	for each p in packages {
		for each f in p->provides {
			L = query (PROVIDES, f)
			L -= p->modifies	# dont care about packages we're modifying
			if L.size > 0 {
				# someone else might own f
				foreach p' in L {
					next if p'->name == p->name
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
	}

    (rough draft by robey:)
*/
void 
check_conflicts_against_already_installed_packages (EazelInstall *service, 
						    GList *packages)
{
	GList *owners;
	GList *iter, *iter_file, *iter_pack;
	PackageData *pack, *pack_owner;
	char *filename;
	GList *flat_packages;

	if (eazel_install_get_force (service) ||
	    eazel_install_get_ignore_file_conflicts (service)) {
		trilobite_debug ("(not performing file conflict check)");
		return;
	}

	flat_packages = flatten_packagedata_dependency_tree (packages);

#if EI2_DEBUG & 0x4
	trilobite_debug ("-> file conflict check begins (%d packages)", g_list_length (flat_packages));
#endif

	for (iter = g_list_first (flat_packages); iter != NULL; iter = g_list_next (iter)) {
		pack = PACKAGEDATA (iter->data);

		if (pack->conflicts_checked) {
			continue;
		}

#if EI2_DEBUG & 0x4
		trilobite_debug ("checking %s", pack->name);
#endif

		pack->conflicts_checked = TRUE;
		for (iter_file = g_list_first (pack->provides); iter_file != NULL; iter_file = g_list_next (iter_file)) {
			filename = (char *)(iter_file->data);

			/* FIXME: bugzilla.eazel.com 5720
			   this is a patch to circumvent unwanted behaviour.
			   Softcat doens't strip directories when giving NO_DIRS_IN_PROVIDES as fillflag,
			   till it does, I use this check */
			if (g_file_test (filename, G_FILE_TEST_ISDIR)) {
				continue;
			}

			owners = eazel_package_system_query (service->private->package_system,
							     service->private->cur_root,
							     filename,
							     EAZEL_PACKAGE_SYSTEM_QUERY_OWNS,
							     PACKAGE_FILL_NO_DIRS_IN_PROVIDES);
			/* No need to check packages that we modify */	
			packagedata_list_prune (&owners, pack->modifies, TRUE, TRUE);

			for (iter_pack = g_list_first (owners); iter_pack != NULL; iter_pack = g_list_next (iter_pack)) {
				pack_owner = (PackageData *)(iter_pack->data);

				if (strcmp (pack_owner->name, pack->name) == 0) {
					/* can't conflict with self */
					gtk_object_unref (GTK_OBJECT (pack_owner));
					continue;
				}

				trilobite_debug ("file conflict 2: %s from %s conflicts with %p %s",
						 filename, pack->name, pack_owner, pack_owner->name);
				if (package_is_upgrading_and_doesnt_need_file (pack_owner, 
									       flat_packages, filename)) {
					/* the owner of this file is a package that we're upgrading, and the
					 * new version no longer has this file, so everything's okay. */
					trilobite_debug ("...but it's okay, we're upgrading %s and it ditched that file",
							 pack_owner->name);
				} else {
					/* boo */
					pack->status = PACKAGE_FILE_CONFLICT;
					/* did we already mark this in ->breaks ? */
					if (g_list_find_custom (pack->breaks, pack_owner->name,
						     (GCompareFunc)eazel_install_package_name_compare)==NULL) {
						pack_owner->status = PACKAGE_FILE_CONFLICT;
						packagedata_add_pack_to_breaks (pack, pack_owner);
					}
				}
				gtk_object_unref (GTK_OBJECT (pack_owner));
			}
			g_list_free (owners);
		}
	}

#if EI2_DEBUG & 0x4
	trilobite_debug ("<- file conflict check ends");
#endif
	g_list_free (flat_packages);
}

/* FIXME: bugzilla.eazel.com 5277 (still need to test this)
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
void 
check_feature_consistency (EazelInstall *service, 
			   GList *packages)
{
	GHashTable *feature_hash;
	GList *iterator;
	GList *flat_packages;

	flat_packages = flatten_packagedata_dependency_tree (packages);
#if EI2_DEBUG & 0x4
	trilobite_debug ("-> check_feature_consistency begins, %d packages", g_list_length (flat_packages));
#endif
	feature_hash = g_hash_table_new ((GHashFunc)g_str_hash,
					 (GCompareFunc)g_str_equal);

	for (iterator = flat_packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = PACKAGEDATA (iterator->data);
		GList *feature_it;

		for (feature_it = pack->features; feature_it; feature_it = g_list_next (feature_it)) {
			char *feature = (char*)feature_it->data;
			g_hash_table_insert (feature_hash, feature, pack);
#if EI2_DEBUG & 0x4
			trilobite_debug ("%s provides feature %s", pack->name, feature);
#endif
		}
	}

	for (iterator = flat_packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = PACKAGEDATA (iterator->data);
		GList *modify_it;

#if EI2_DEBUG & 0x4
		trilobite_debug ("checking %s", pack->name);
#endif

		for (modify_it = pack->modifies; modify_it; modify_it = g_list_next (modify_it)) {
			PackageData *pack_modified = PACKAGEDATA (modify_it->data);
			GList *feature_it;
#if EI2_DEBUG & 0x4
			trilobite_debug ("%s modifies %s", pack->name, pack_modified->name);
#endif

			for (feature_it = pack_modified->features; feature_it; feature_it = g_list_next (feature_it)) {
				char *feature = (char*)feature_it->data;

				if (g_hash_table_lookup (feature_hash, feature)) {
					/* Since feature was in hash, it is still provided by someone */
#if EI2_DEBUG & 0x4
					trilobite_debug ("feature %s is ok", feature);
#endif
				} else {
					GList *required_by;
					GList *break_it;
					/* Feature is lost */
#if EI2_DEBUG & 0x4
					trilobite_debug ("feature %s is lost", feature);
#endif
					required_by = eazel_package_system_query (service->private->package_system,
										  service->private->cur_root,
										  feature,
										  EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES,
										  PACKAGE_FILL_NO_DIRS_IN_PROVIDES);
					for (break_it = required_by; break_it; break_it = g_list_next (break_it)) {
						PackageData *pack_broken = PACKAGEDATA (break_it->data);

#if EI2_DEBUG & 0x4
						trilobite_debug ("%p %s is broken by %p %s modifying %p %s", 
								 pack_broken, pack_broken->name,
								 pack, pack->name,
								 pack_modified, pack_modified->name);
#endif
						/* FIXME 5277 and 5721 more here 
						   For me to finish this, I need softcat to emit
						   features in the xml
						 */
						pack_broken->status = PACKAGE_DEPENDENCY_FAIL;
						pack->status = PACKAGE_BREAKS_DEPENDENCY;
						packagedata_add_pack_to_breaks (pack, pack_broken);
						gtk_object_unref (GTK_OBJECT (pack_broken));
					}
					g_list_free (required_by);
				}
			}
		}
	}

	g_list_free (flat_packages);
	g_hash_table_destroy (feature_hash);

#if EI2_DEBUG & 0x4
	trilobite_debug ("<- check_feature_consistency");
#endif
}

void
do_file_conflict_check (EazelInstall *service, 
			GList **packages,
			GList **extra_packages)
{
	check_no_two_packages_has_same_file (service, *packages);
	prune_failed_packages (service, packages);

	check_conflicts_against_already_installed_packages (service, *packages);
	check_tree_for_conflicts (service, packages, extra_packages);

	if (extra_packages == NULL) {
		check_feature_consistency (service, *packages);
		prune_failed_packages (service, packages);
	}
}


/***********************************************************************************/

/* This is the main dependency check function */

void do_dep_check_internal (EazelInstall *service, GList *packages);
void do_dep_check (EazelInstall *service, GList **packages);

void
do_dep_check_internal (EazelInstall *service,
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

	/* Build the K list, consisting of packages without must_have set... */
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
	        GList *diterator;
		for (diterator = PACKAGEDATA(iterator->data)->depends; diterator; diterator = g_list_next (diterator)) {
			PackageDependency *dep = PACKAGEDEPENDENCY (diterator->data);
			if (~dep->package->fillflag & MUST_HAVE) {
				K = g_list_prepend (K, dep->package);
			}
		}
	}

	/* ... and process them */
	if (K) {
		do_dep_check_internal (service, K);
	}
}

void
do_dep_check (EazelInstall *service,
	      GList **packages)
{
	/* Shift to the internal. We want to do a prune_failed_packages
	   after completion, but since the do_dep_check algorithm is 
	   recursive, we call an internal function here. */
	do_dep_check_internal (service, *packages);
	prune_failed_packages (service, packages);
}

/***********************************************************************************/

/* This is the download stuff */

void download_packages (EazelInstall *service, GList *packages);

void 
download_packages (EazelInstall *service, 
		   GList *packages)
{
	GList *flat_packages;
	GList *iterator;
	
	flat_packages = flatten_packagedata_dependency_tree (packages);
#if EI2_DEBUG & 0x4
	trilobite_debug ("downloading %d packages", g_list_length (packages));
#endif
	for (iterator = flat_packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = PACKAGEDATA (iterator->data);
		eazel_install_fetch_package (service, pack);
	}

	g_list_free (flat_packages);
}

/***********************************************************************************/

static gboolean
clean_up_dedupe_hash (const char *id, PackageData *pack)
{
	gtk_object_unref (GTK_OBJECT (pack));
	return TRUE;
}

static gboolean
clean_up_dep_ok_hash (char *key, gpointer unused)
{
	g_free (key);
	return TRUE;
}

unsigned long
eazel_install_get_total_size_of_packages (EazelInstall *service,
					  const GList *packages)
{
	const GList *iterator;
	unsigned long result = 0;
	for (iterator = packages; iterator; glist_step (iterator)) {
		PackageData *pack;

		pack = (PackageData*)iterator->data;
		result += pack->bytesize;
	}
	return result;
}

static void
execute (EazelInstall *service,
	 GList *packages,
	 EazelPackageSystemOperation op,
	 int flags) 
{
	TrilobiteRootHelper *root_helper;
	GList *flat_packages;

	flat_packages = flatten_packagedata_dependency_tree (packages);

	root_helper = gtk_object_get_data (GTK_OBJECT (service), "trilobite-root-helper");
	gtk_object_set_data (GTK_OBJECT (service->private->package_system), 
			     "trilobite-root-helper", root_helper);	

	if (eazel_install_get_test (service)) {
		flags |= EAZEL_PACKAGE_SYSTEM_OPERATION_TEST;
	}

	/* Init the hack var to emit the old style progress signals */
	service->private->infoblock [0] = 0;
	service->private->infoblock [1] = 0;
	service->private->infoblock [2] = 0;
	service->private->infoblock [3] = g_list_length (flat_packages);
	service->private->infoblock [4] = 0;
	service->private->infoblock [5] = eazel_install_get_total_size_of_packages (service, flat_packages);

	switch (op) {
	case EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL:
		eazel_package_system_install (service->private->package_system,
					      service->private->cur_root,
					      flat_packages,
					      flags);
		break;
	case EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL:
		eazel_package_system_uninstall (service->private->package_system,
						service->private->cur_root,
						flat_packages,
						flags);
		break;
	case EAZEL_PACKAGE_SYSTEM_OPERATION_VERIFY:
		eazel_package_system_verify (service->private->package_system,
					      service->private->cur_root,
					      flat_packages);
		g_assert (0);
		break;
	}

	g_list_free (flat_packages);
}

/***********************************************************************************/

/* These are the methods exposed to the the rest of the service object */

static void 
install_packages_helper (EazelInstall *service, 
			 GList **packages,
			 GList **extra_packages)
{
	do_dep_check (service, packages);
	do_file_conflict_check (service, packages, extra_packages);

#if EI2_DEBUG & 0x8
	trilobite_debug ("FINAL TREE BEGIN");
	dump_tree (*packages);
	trilobite_debug ("FINAL TREE END");
#endif
	
	/* FIXME: bugzilla.eazel.com 5264(fixed), 5266, 5267
	   finish this cruft by 
	   1: traversing the tree to see if anything failed/broke (5264)
	   2 if failed, emit_failed on the root, and remove root from tree
	     3: if any broke any other, add the others to tree and recurse
	   else
	     4: traverse tree and download packages (5266)
	     5: force install... (5267)
	*/
	return;
}

EazelInstallOperationStatus 
install_packages (EazelInstall *service, GList *categories)
{
	EazelInstallOperationStatus result = EAZEL_INSTALL_NOTHING;       	
	GList *packages;
	GList *extra_packages = NULL;

	packages = packagedata_list_copy (categorylist_flatten_to_packagelist (categories), TRUE);
	do {
		extra_packages = NULL;
		install_packages_helper (service, &packages, &extra_packages);
		if (extra_packages) {
			packages = g_list_concat (packages, extra_packages);
		}
	} while (extra_packages != NULL);

	if (eazel_install_emit_preflight_check (service, packages)) {
		int flags = EAZEL_PACKAGE_SYSTEM_OPERATION_UPGRADE | EAZEL_PACKAGE_SYSTEM_OPERATION_FORCE;
		gboolean go_ahead = TRUE;

		/* FIXME: bugzilla.eazel.com 5722
		   download could fail, do the download func needs to
		   be able to fail the operation... */
		download_packages (service, packages);
		if (eazel_install_get_force (service) == FALSE) {
			if (!check_md5_on_files (service, packages)) {
				go_ahead = FALSE;
			}
		}
		if (go_ahead) {
			execute (service, packages, EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL, flags);
			/* FIXME: bugzilla.eazel.com 5723
			   we need to detect if install_failed_signal was called */
			result = EAZEL_INSTALL_INSTALL_OK;
		}
	}

	g_hash_table_foreach_remove (service->private->dedupe_hash,
				     (GHRFunc)clean_up_dedupe_hash,
				     service);
	g_hash_table_foreach_remove (service->private->dep_ok_hash,
				     (GHRFunc)clean_up_dep_ok_hash,
				     service);
	g_list_foreach (packages, (GFunc)gtk_object_unref, NULL);

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
