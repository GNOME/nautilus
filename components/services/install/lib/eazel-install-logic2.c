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
#define UPDATE_MUST_HAVE PACKAGE_FILL_NO_DEPENDENCIES|PACKAGE_FILL_NO_TEXT|PACKAGE_FILL_NO_DIRS_IN_PROVIDES
#define OWNS_MUST_HAVE PACKAGE_FILL_NO_DEPENDENCIES|PACKAGE_FILL_NO_TEXT|PACKAGE_FILL_NO_DIRS_IN_PROVIDES
#define MODIFY_MUST_HAVE PACKAGE_FILL_NO_DEPENDENCIES|PACKAGE_FILL_NO_TEXT|PACKAGE_FILL_NO_DIRS_IN_PROVIDES

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
dump_tree_helper (GList *packages, char *indent, GList *path, GList **touched) 
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
		if (g_list_find (*touched, pack) != NULL) {
			trilobite_debug ("%s%p (%s) %s   [see above]",
					 indent,
					 pack,
					 name,
					 pack->eazel_id);
		} else {
			trilobite_debug ("%s%p (%s) %s %s%s %s", 
					 indent, 
					 pack, 
					 name, 
					 pack->eazel_id,
					 (pack->fillflag & MUST_HAVE) ? "filled" : "not filled",
					 (pack->status == PACKAGE_CANNOT_OPEN) ? " but failed" : "",
					 pack->toplevel ? "TOP":"");
			tmp = g_strdup_printf ("%s  ", indent);
			if (g_list_find (path, pack)) {
				trilobite_debug ("%s... %p %s recurses ..", indent, pack, pack->name);
			} else {
				path = g_list_prepend (path, pack);
				*touched = g_list_prepend (*touched, pack);
				dump_tree_helper (pack->depends, tmp, path, touched);
				path = g_list_remove (path, pack);
			}
			g_free (tmp);
		}
		g_free (name);
	}
}

static void
dump_tree (GList *packages)
{
	GList *touched = NULL;

	dump_tree_helper (packages, "", NULL, &touched);
	g_list_free (touched);
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
#if EI2_DEBUG & 0x4				
				/* get_readable_name is leaked */
				trilobite_debug ("read md5 from file %s", pack->filename);
				trilobite_debug ("for package %s", packagedata_get_readable_name (pack));
#endif
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
	eazel_install_unlock_tmp_dir (service);

	return result;
}

/***********************************************************************************/

/* This is code to removed failed trees from the tree */

void prune_failed_packages (EazelInstall *service, GList **packages);
void prune_failed_packages_helper (EazelInstall *service, PackageData *root, 
				   PackageData *pack, GList *packages, GList **path, GList **result);

void 
prune_failed_packages_helper (EazelInstall *service, 
			      PackageData *root, 
			      PackageData *pack, 
			      GList *packages,
			      GList **path,
			      GList **result)
{
	GList *iterator;

#if EI2_DEBUG & 0x4
	trilobite_debug ("entering subpruner %p %s %s", 
			 pack, pack->name, 
			 packagedata_status_enum_to_str (pack->status));
#endif

	if (g_list_find (*path, pack)) {
#if EI2_DEBUG & 0x4
		trilobite_debug ("... %p %s recurses .., softcat is probably in flux", pack, pack->name);
#endif
		return;
	}

	if (pack->status != PACKAGE_PARTLY_RESOLVED) {
#if EI2_DEBUG & 0x4
		trilobite_debug ("subpruner kill root %s because of %s", root->name, pack->name);
#endif
		if (g_list_find (*result, root)==NULL) {
			(*result) = g_list_prepend (*result, root);
		}
	} else {
		for (iterator = pack->depends; iterator; iterator = g_list_next (iterator)) {
			PackageDependency *dep = PACKAGEDEPENDENCY (iterator->data);
			(*path) = g_list_prepend (*path, pack);
			prune_failed_packages_helper (service, root, dep->package, packages, path, result);
			(*path) = g_list_remove (*path, pack);
		}
	}
}

/* If the calls to prune_failed_packages are removed, no
  install_failed signals are emitted, and the client gets the entire
  tree list.  This is for bug 5267 */

void 
prune_failed_packages (EazelInstall *service, 
		       GList **packages)
{
	GList *iterator;
	GList *result = NULL;

	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = PACKAGEDATA (iterator->data);
		GList *path = NULL;
		prune_failed_packages_helper (service, pack, pack, *packages, &path, &result);
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
		g_list_free (*packages);
		(*packages) = NULL;
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
	GList *borked_packages = NULL;
	EazelInstallStatus result;

#if EI2_DEBUG & 0x4
	trilobite_debug ("check_existing %p %s", pack, pack->name);
#endif
	result = EAZEL_INSTALL_STATUS_NEW_PACKAGE;
	/* query for existing package of same name */
	existing_packages = eazel_package_system_query (service->private->package_system,
							service->private->cur_root,
							pack->name,
							EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
							MODIFY_MUST_HAVE);
	if (existing_packages) {
		/* Get the existing package, set it's modify flag and add it */
		GList *iterator;
		PackageData *survivor = NULL;
		gboolean abort = FALSE;
		int res;

		if (g_list_length (existing_packages)>1) {
			trilobite_debug ("there are %d existing packages called %s",
					 g_list_length (existing_packages),
					 pack->name);
			/* Verify them all, to find one that is not damaged. Mark the rest
			   as invalid so the client can suggest the user should delete them */
			for (iterator = existing_packages; iterator; iterator = g_list_next (iterator)) {
				PackageData *existing_package = PACKAGEDATA (iterator->data);
				char *name = packagedata_get_readable_name (existing_package);
				GList *foo = NULL;
				
				foo = g_list_prepend (foo, existing_package);
				if (eazel_package_system_verify (service->private->package_system,
								 service->private->cur_root,
								 foo)) {
					g_message ("%s is ok", name);
					if (survivor == NULL) {
						survivor = existing_package;
					} else {						
						abort = TRUE;
					}
				} else {
					g_message ("%s is NOT ok", name);
					existing_package->status = PACKAGE_INVALID;
					/* I add the borked packages later, so they'll show up
					   earlier in the tree */
					borked_packages = g_list_prepend (borked_packages, existing_package);
				}
				g_list_free (foo);
				g_free (name);
			}
		} else {
			survivor = PACKAGEDATA (g_list_first (existing_packages)->data);
		}
		
		if (abort) {
			trilobite_debug ("*********************************************************");
			trilobite_debug ("This is a bad bad case, see bug 3511");
			trilobite_debug ("To circumvent this problem, as root, execute this command");
			trilobite_debug ("(which is dangerous by the way....)");
			trilobite_debug ("rpm -e --nodeps `rpm -q %s`", pack->name);
			
			g_list_free (borked_packages);

			/* Cancel the package, mark all the existing as invalid */			   
			pack->status = PACKAGE_CANCELLED;
			for (iterator = existing_packages; iterator; iterator = g_list_next (iterator)) {
				PackageData *existing_package = PACKAGEDATA (iterator->data);
				existing_package->status = PACKAGE_INVALID;
				packagedata_add_pack_to_modifies (pack, existing_package);
				gtk_object_unref (GTK_OBJECT (existing_package));
			}
		}

		if (abort==FALSE && survivor) {
			g_assert (pack->version);
			g_assert (survivor->version);

			res = eazel_package_system_compare_version (service->private->package_system,
								    pack->version, 
								    survivor->version);
			
			/* check against minor version */
			if (res==0) {
#if EI2_DEBUG & 0x4
				trilobite_debug ("versions are equal (%s), comparing minors", pack->version);
#endif
				if (pack->minor && survivor->minor) {
#if EI2_DEBUG & 0x4
					trilobite_debug ("minors are %s for new and %s for installed)", 
							 pack->minor, survivor->minor);
#endif
					res = eazel_package_system_compare_version (service->private->package_system,
										    pack->minor, survivor->minor);
				} else if (!pack->minor && survivor->minor) {
					/* If the given packages does not have a minor,
					   but the installed has, assume we're fine */
					/* FIXME: bugzilla.eazel.com
					   This is a patch, it should be res=1, revert when
					   softcat is updated to have revisions for all packages 
					   (post PR3) */
					res = 1;
				} else {
					/* Eh, do nothing just to be safe */
					res = 0;
				}
			}

			/* Set the modify_status flag */
			if (res == 0) {
				survivor->modify_status = PACKAGE_MOD_UNTOUCHED;
			} else if (res > 0) {
				survivor->modify_status = PACKAGE_MOD_UPGRADED;
			} else {
				survivor->modify_status = PACKAGE_MOD_DOWNGRADED;
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
				if (pack->minor && survivor->minor) {
					trilobite_debug (_("%s-%s version %s-%s already installed"), 
							 pack->name, pack->minor, 
							 survivor->version, survivor->minor);
				} else {
					trilobite_debug (_("%s version %s already installed"), 
							 pack->name, 
							 survivor->version);
				}
			} 
			break;
			case EAZEL_INSTALL_STATUS_UPGRADES: {
				/* This is certainly ugly as helll */
				if (pack->minor && survivor->minor) {
					trilobite_debug (_("%s upgrades from version %s-%s to %s-%s"),
							 pack->name, 
							 survivor->version, survivor->minor, 
							 pack->version, pack->minor);
				} else {
					trilobite_debug (_("%s upgrades from version %s-%s to %s-%s"),
							 pack->name, 
							 survivor->version, survivor->minor, 
							 pack->version, pack->minor);
				}
			}
			break;
			case EAZEL_INSTALL_STATUS_DOWNGRADES: {
				if (pack->minor && survivor->minor) {
					trilobite_debug (_("%s downgrades from version %s-%s to %s-%s"),
							 pack->name, 
							 survivor->version, survivor->minor, 
							 pack->version, pack->minor);
				} else {
					trilobite_debug (_("%s downgrades from version %s to %s"),
							 pack->name, 
							 survivor->version, 
							 pack->version);
				}
			}
			break;
			default:
				break;
			}
#endif		
/*
			if (g_list_find_custom (pack->modifies, 
						survivor->name,
						(GCompareFunc)eazel_install_package_name_compare)) {
#if EI2_DEBUG & 0x4
				trilobite_debug ("%s already marked as modified", survivor->name);
#endif
				gtk_object_unref (GTK_OBJECT (survivor));
				survivor = NULL;
				continue;
			}
*/
		
			/* Set modifies list */
			if (result != EAZEL_INSTALL_STATUS_QUO) {
#if EI2_DEBUG & 0x4
				trilobite_debug ("%p %s modifies %p %s",
						 pack, pack->name,
						 survivor, survivor->name);
#endif

				packagedata_add_pack_to_modifies (pack, survivor);
				survivor->status = PACKAGE_RESOLVED;
			} else {
				pack->status = PACKAGE_ALREADY_INSTALLED;
			}
			gtk_object_unref (GTK_OBJECT (survivor));
		}

		/* Now add the borked packages */
		for (iterator = borked_packages; iterator; iterator = g_list_next (iterator)) {
			PackageData *existing_package = PACKAGEDATA (iterator->data);
			packagedata_add_pack_to_modifies (pack, existing_package);
			gtk_object_unref (GTK_OBJECT (existing_package));

		}
		g_list_free (borked_packages);

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

	if ((*package)->fillflag == MUST_HAVE) {
		/* Package is already filled */
		result = SOFTCAT_HIT_OK;
	} else {
		if ((*package)->filename) {
			if (g_file_test ((*package)->filename, G_FILE_TEST_ISFILE) &&
			    access ((*package)->filename, R_OK)==0) {
				PackageData *loaded_package;
#if EI2_DEBUG & 0x4
				trilobite_debug ("%p %s load from disk", *package, (*package)->name);
#else
				g_message ("Loading package info from %s", (*package)->filename);
#endif		
				loaded_package = eazel_package_system_load_package (service->private->package_system,
										    *package,
										    (*package)->filename,
										    MUST_HAVE);
				if (loaded_package==NULL) {
					(*package)->status = PACKAGE_CANNOT_OPEN;
				} else {
					(*package)->status = PACKAGE_PARTLY_RESOLVED;
				}
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
		PackageData *p1 = NULL;

		g_assert ((*package)->md5);
		p1 = g_hash_table_lookup (service->private->dedupe_hash, (*package)->md5);

		if (p1) {
#if EI2_DEBUG & 0x4
			trilobite_debug ("deduping %p %s", *package, (*package)->name);
#endif		
			
			gtk_object_ref (GTK_OBJECT (p1));
			gtk_object_unref (GTK_OBJECT (*package)); 
			(*package) = p1;
		} else {
			EazelInstallStatus status;
			gboolean accept = TRUE;

			status = eazel_install_check_existing_packages (service, *package);
			switch (status) {
			case EAZEL_INSTALL_STATUS_NEW_PACKAGE:
				accept = TRUE;
				break;
			case EAZEL_INSTALL_STATUS_UPGRADES:
				if (eazel_install_get_update (service)) {
					accept = TRUE;
				} else {
					accept = FALSE;
				}
				break;
			case EAZEL_INSTALL_STATUS_DOWNGRADES:
				if (eazel_install_get_downgrade (service)) {
					accept = TRUE;
				} else {
					accept = FALSE;
				}
				break;
			case EAZEL_INSTALL_STATUS_QUO:
				accept = FALSE;
				break;
			}
			if (accept) {
				gtk_object_ref (GTK_OBJECT (*package));
				g_hash_table_insert (service->private->dedupe_hash, 
						     (*package)->md5, 
						     *package);
			} else {
				(*package)->status = PACKAGE_ALREADY_INSTALLED;
				result = PACKAGE_SKIPPED;
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

	if (p1->md5 == NULL) {
		/* Package info not received from SoftCat */
		return;
	}

	if (~p1->fillflag & MUST_HAVE) {
		PackageData *p11;

		p11 = g_hash_table_lookup (service->private->dedupe_hash, p1->md5);

		if (p11) {
			gtk_object_ref (GTK_OBJECT (p11));
			gtk_object_unref (GTK_OBJECT (p1)); 
			d->package = p11;
		} else {
			gtk_object_ref (GTK_OBJECT (p1));
			g_hash_table_insert (service->private->dedupe_hash, p1->md5, p1);			
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
gboolean is_satisfied_features (EazelInstall *service, PackageData *package);

gboolean
is_satisfied (EazelInstall *service, 
	      PackageDependency *dep)
{
	char *key;
	int previous_check_state = 0;
	char *sense_str;
	gboolean result = FALSE;

	g_assert (dep);
	g_assert (IS_PACKAGEDEPENDENCY (dep));
	g_assert (service);
	g_assert (EAZEL_IS_INSTALL (service));

	/* FIXME:
	   this checks that the package isn't already filled, but what if
	   it is, but later fails, will we loose a dependency ? */
	/* First check if we've already downloaded the package */
	if (dep->package->fillflag & MUST_HAVE) {
#if EI2_DEBUG & 0x4
		trilobite_debug ("is_satisfied? %p %s", 
				 dep->package, dep->package->name);
		trilobite_debug ("\t -> already filled, must be ok");
#else
		g_message ("\t(cached) : %s", 
			   dep->package->name);
#endif
		
		return FALSE;
	}

	/* If the dependency has a version, but no sense, something is terribly
	   wrong with the xml */
#if EI2_DEBUG & 0x4
	if (dep->version && dep->sense == 0) {
		trilobite_debug ("I'm going to die now, because softcat is making no sense");
		trilobite_debug ("Or rather, the xml returned a invalid dependency sense");
	}
#endif
	if (dep->version) { g_assert (dep->sense!=0); }

	sense_str = eazel_softcat_sense_flags_to_string (dep->sense);
#if EI2_DEBUG & 0x4
	trilobite_debug ("is_satisfied? %p %s %s %s", 
			 dep->package, dep->package->name, sense_str,
			 (dep->version != NULL ? dep->version : ""));
#endif
	key = g_strdup_printf ("%s/%s/%s", dep->package->name, sense_str,
			       (dep->version != NULL ? dep->version : ""));

	if (key != NULL && strcmp (key, "(null)//")!=0) {
		previous_check_state = GPOINTER_TO_INT (g_hash_table_lookup (service->private->dep_ok_hash, key));
	}
#if EI2_DEBUG & 0x4
	trilobite_debug ("\t--> key is %s", key);
#endif
	switch (previous_check_state) {
	case DEPENDENCY_OK: {
#if EI2_DEBUG & 0x4
		trilobite_debug ("\t--> dep hash ok");
#endif
		result = TRUE;
		break;
	}
	case DEPENDENCY_NOT_OK: {
#if EI2_DEBUG & 0x4
		trilobite_debug ("\t--> dep hash failed");
#endif
		result = FALSE;
		break;
	}
	default: {				
		if (dep->package->name && dep->version) {
			if (eazel_package_system_is_installed (service->private->package_system,
							       service->private->cur_root,
							       dep->package->name,
							       dep->version,
							       NULL,
							       dep->sense)) {
#if EI2_DEBUG & 0x4
				trilobite_debug ("\t--> installed with version");
#endif
				result = TRUE;
			}
		} else if (dep->package->name &&
			   eazel_package_system_is_installed (service->private->package_system,
							      service->private->cur_root,
							      dep->package->name,
							      NULL, 
							      NULL,
							      EAZEL_SOFTCAT_SENSE_ANY)) {
#if EI2_DEBUG & 0x4
			trilobite_debug ("\t--> installed");
#endif
			result = TRUE;
		} else {

			if (dep->package->features && is_satisfied_features (service, dep->package)) {
#if EI2_DEBUG & 0x4
				trilobite_debug ("\t--> features of package are satisfied");
#endif
				result = TRUE;
			}
		}
		if (result) {
#if EI2_DEBUG & 0x4
			trilobite_debug ("\t--> feature is satisfied");
#endif
			g_hash_table_insert (service->private->dep_ok_hash, 
					     key,
					     GINT_TO_POINTER (DEPENDENCY_OK));
		} else {
#if EI2_DEBUG & 0x4
			trilobite_debug ("\t--> feature not satisfied");
#endif
			g_hash_table_insert (service->private->dep_ok_hash, 
					     key,
					     GINT_TO_POINTER (DEPENDENCY_NOT_OK));
		}
	}
	}

#if ~EI2_DEBUG & 0x4	
	g_message ("\t%8.8s : %s %s %s", 
		   result ? "ok" : "NOT ok",
		   dep->package->name, sense_str,
		   (dep->version != NULL ? dep->version : ""));
#endif
	g_free (sense_str);
	return result;
}

gboolean
is_satisfied_features (EazelInstall *service, 
		       PackageData *package)
{
	gboolean result = TRUE;
	GList *iterator;
	GList *features;

	g_assert (service);
	g_assert (EAZEL_IS_INSTALL (service));
	g_assert (package);
	g_assert (IS_PACKAGEDATA (package));

	features = package->features;

#if EI2_DEBUG & 0x4
	trilobite_debug ("\t -> is_satisfied_features %d features", g_list_length (features));
#endif

	for (iterator = features; iterator && result; iterator = g_list_next (iterator)) {
		GList *query_result;
		char *f = (char*)iterator->data;

#if EI2_DEBUG & 0x4
		trilobite_debug ("\t -> %s", f);
#endif
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
				trilobite_debug ("\t -> noone owns or provides %s", f);
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
					trilobite_debug ("\t -> but a package called %s exists", f);
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

	/*
	  FIXME: bugzilla.eazel.com 6485
	  This is the initial fix for 6485, but I'll also need
	  to evaporate ->depends, lest the next depcheck step begins
	  depchekking the packages in ->depends, and treat them like
	  new installs which is definitely non-optimial (actually 
	  it's really bad)

	if (package->status == PACKAGE_CANCELLED) {
		return;
	}
	*/

#if EI2_DEBUG & 0x4
	trilobite_debug ("check deps for %p %s", package, package->name);
#else
	g_message ("Checking dependencies for %s", package->name);
#endif

	for (iterator = package->depends; iterator; iterator = g_list_next (iterator)) {
		PackageDependency *dep = PACKAGEDEPENDENCY (iterator->data);		

		if (dep->package->name && strcmp (dep->package->name, package->name)==0) {
			char *name_a, *name_b;

			name_a = packagedata_get_readable_name (package);
			name_b = packagedata_get_readable_name (dep->package);

			g_warning ("Possible inconsistency!");
			g_warning ("%s depends on %s", name_a, name_b);

			g_free (name_a);
			g_free (name_b);

			package->status = PACKAGE_CANCELLED;
			remove = g_list_prepend (remove, dep);
		} else {
			if (is_satisfied (service, dep)) {
				remove = g_list_prepend (remove, dep);
			} else {
				eazel_install_emit_dependency_check (service, package, dep);
			}
		}
	}

	for (iterator = remove; iterator; iterator = g_list_next (iterator)) {
		PackageDependency *dep = PACKAGEDEPENDENCY (iterator->data);
		package->depends = g_list_remove (package->depends, dep);
#if EI2_DEBUG & 0x4
		trilobite_debug ("removing %p %s from %p %s", 
				 dep->package, dep->package->name, 
				 package, package->name);
#endif
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
gboolean check_if_related_package (EazelInstall *service, PackageData *package, PackageData *dep);

/*
  This function tests wheter "package" and "dep"
  seems to be related in some way.
  This is done by checking the package->modifies list for
  elements that have same version as dep->version.
  I then compare these elements against dep->name,
  and if one matches the x-y-z vs dep->name=x-y scheme,
  I declare that "package" and "dep" are related
*/
gboolean
check_if_related_package (EazelInstall *service,
			  PackageData *package,
			  PackageData *dep)
{
	/* Pessimisn mode = HIGH */
	gboolean result = FALSE;
	GList *potiental_mates;
	char **dep_name_elements;
	
	dep_name_elements = g_strsplit (dep->name, "-", 80);

	/* First check, if package modifies a package with the same version
	   number as dep->version */
	potiental_mates = package->modifies;
	while ((potiental_mates = g_list_find_custom (potiental_mates, 
						      dep->version, 
						      (GCompareFunc)eazel_install_package_version_compare))!=NULL) {
		PackageData *modpack = (PackageData*)potiental_mates->data;
		
		if ((modpack->modify_status == PACKAGE_MOD_UPGRADED) ||
		    (modpack->modify_status == PACKAGE_MOD_DOWNGRADED)) {			
			char **mod_name_elements;
			char *dep_name_iterator;
			char *mod_name_iterator;
			int cnt = 0;

			mod_name_elements = g_strsplit (modpack->name, "-", 80);
			
			for (cnt=0; TRUE;cnt++) {
				dep_name_iterator = dep_name_elements[cnt];
				mod_name_iterator = mod_name_elements[cnt];
#if 0
				trilobite_debug ("dep name iterator = \"%s\"", dep_name_iterator);
				trilobite_debug ("mod name iterator = \"%s\"", mod_name_iterator);
#endif
				if ((dep_name_iterator==NULL) ||
				    (mod_name_iterator==NULL)) {
					break;
				}
				if ((strlen (dep_name_iterator) == strlen (mod_name_iterator)) &&
				    (strcmp (dep_name_iterator, mod_name_iterator)==0)) {
					continue;
				}
				break;
			}
			if (cnt >= 1) {
				trilobite_debug ("%s-%s seems to be a child package of %s-%s, which %s-%s updates",
						 dep->name, dep->version, 
						 modpack->name, modpack->version,
						 package->name, package->version);
				if (!result) {
					result = TRUE;
				} else {
					trilobite_debug ("but what blows is, the previous also did!!");
					g_assert_not_reached ();
				}				
			} else {
				trilobite_debug ("%s-%s is not related to %s-%s", 
						 dep->name, dep->version, 
						 package->name, package->version);
			}
			g_strfreev (mod_name_elements);			
		}
		potiental_mates = g_list_next (potiental_mates);
	}
	g_strfreev (dep_name_elements);
	return result;
}

static gboolean
check_update_for_no_more_file_conflicts (PackageFileConflict *conflict, 
					 PackageData *pack_update)
{
	GList *iterator;

	g_assert (IS_PACKAGEFILECONFLICT (conflict));
	g_assert (conflict->files);
	g_assert (g_list_length (conflict->files));

	for (iterator = conflict->files; iterator; iterator = g_list_next (iterator)) {
		char *filename = (char*)iterator->data;
		if (g_list_find_custom (pack_update->provides, 
					filename,
					(GCompareFunc)strcmp)) {
			return FALSE;
		}
	}
	return TRUE;
}

static void
check_tree_helper (EazelInstall *service, 
		   PackageData *pack,
		   GList **extra_packages,
		   GList **path)
{
	GList *iterator;
	GList *remove = NULL;

	if (g_list_find (*path, pack)) {
#if EI2_DEBUG & 0x4
		trilobite_debug ("... %p %s recurses .., softcat is probably in flux", pack, pack->name);
#endif
		return;
	}

#if EI2_DEBUG & 0x4
	trilobite_debug ("-> check_tree_for_conflicts_helper");
#endif
	if (pack->status == PACKAGE_FILE_CONFLICT) {
#if EI2_DEBUG & 0x4
		trilobite_debug ("trying to revive %s", pack->name);
#endif
		for (iterator = pack->breaks; iterator; iterator = g_list_next (iterator)) {
			PackageBreaks *breakage = PACKAGEBREAKS (iterator->data);

			if (IS_PACKAGEFILECONFLICT (breakage)) {
				PackageFileConflict *conflict = PACKAGEFILECONFLICT (breakage); 
				PackageData *pack_broken = packagebreaks_get_package (breakage);
				PackageData *pack_update = NULL;
				gboolean update_available = FALSE;

				if (check_if_related_package (service,
							      pack,
							      pack_broken)) {
					char *a, *b;
					a = packagedata_get_readable_name (pack);
					b = packagedata_get_readable_name (pack_broken);
					g_message ("%s is related to %s",
						   a, b);

					/* Create the pack_update */
					pack_update = packagedata_new ();
					pack_update->name = g_strdup (pack_broken->name);
					pack_update->version = g_strdup (pack->version);
					pack_update->distribution = pack_broken->distribution;
					pack_update->archtype = g_strdup (pack_broken->archtype);

					/* Try and get the info */
					if (eazel_softcat_get_info (service->private->softcat,
								    pack_update,
								    EAZEL_SOFTCAT_SENSE_EQ,
								    UPDATE_MUST_HAVE) 
					    != EAZEL_SOFTCAT_SUCCESS) {
						update_available = FALSE;
					        /* if we failed, delete the package object */
						gtk_object_unref (GTK_OBJECT (pack_update));
					} else {
						update_available = TRUE;
					}
					
					g_free (a);
					g_free (b);
				} else {
					update_available = 
						eazel_softcat_available_update (service->private->softcat,
										pack_broken,
										&pack_update,
										UPDATE_MUST_HAVE);
				}

				if (update_available) {
					if (check_update_for_no_more_file_conflicts (conflict, pack_update)) {
#if EI2_DEBUG & 0x4
						trilobite_debug ("adding %s to packages to be installed", 
								 pack_update->name);
#else
						g_message ("updating %s to version %s-%s solves conflict",
							   pack_update->name, pack_update->version,
							   pack_update->minor);
#endif
                                                /* ref the package and add it to the extra_packages list */
						gtk_object_ref (GTK_OBJECT (pack_update));
						(*extra_packages) = g_list_prepend (*extra_packages, 
										    pack_update);
						pack_update->status = PACKAGE_PARTLY_RESOLVED;
						remove = g_list_prepend (remove, breakage);
						/* reset pack_broken to some sane values */
						pack->status = PACKAGE_PARTLY_RESOLVED;
						pack_update->toplevel = TRUE;
					} else {
#if EI2_DEBUG & 0x4
						trilobite_debug ("%s still has conflict", pack_update->name);
#else
						g_message ("available update to %s (%s-%s) does not solves conflict",
							   pack_update->name,
							   pack_update->version, pack_update->minor);
#endif
						gtk_object_unref (GTK_OBJECT (pack_update));
					}
				} else {
					g_message (_("could not revive %s"), pack->name);
				}
			}
		}

		/* Now nuke the successfully revived PackageBreaks */
		for (iterator = remove; iterator; iterator = g_list_next (iterator)) {
			PackageBreaks *breakage = PACKAGEBREAKS (iterator->data);
			gtk_object_unref (GTK_OBJECT (breakage));
			pack->breaks = g_list_remove (pack->breaks, breakage);
		}
		g_list_free (remove);

		/* if no breaks were unrevived, null out the list */
		if (g_list_length (pack->breaks)==0) {
			g_list_free (pack->breaks);
			pack->breaks = NULL;
		}
	}
	
	for (iterator = pack->depends; iterator; iterator = g_list_next (iterator)) {
		PackageDependency *dep = PACKAGEDEPENDENCY (iterator->data);
		(*path) = g_list_prepend (*path, pack);
		check_tree_helper (service, dep->package, extra_packages, path);
		(*path) = g_list_remove (*path, pack);
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
		GList *path = NULL;
		check_tree_helper (service, pack, extra_packages, &path);
	}
#if EI2_DEBUG & 0x4
	trilobite_debug ("<- check_tree_for_conflicts");
#endif
}

static int
find_break_by_package_name (PackageBreaks *breaks, PackageData *pack)
{
	return eazel_install_package_compare (packagebreaks_get_package (breaks), 
					      pack);
}

static void
add_file_conflict (PackageData *pack, 
		   PackageData *broken,
		   char *filename)
{
	GList *prev;
	prev = g_list_find_custom (pack->breaks, 
				   broken,
				   (GCompareFunc)find_break_by_package_name);
	pack->status = PACKAGE_FILE_CONFLICT;

	if (prev) {
		PackageFileConflict *conflict = PACKAGEFILECONFLICT (prev->data);
		conflict->files = g_list_prepend (conflict->files, g_strdup (filename));
	} else {
		PackageFileConflict *conflict = packagefileconflict_new ();
		
		pack->status = PACKAGE_FILE_CONFLICT;
		broken->status = PACKAGE_FILE_CONFLICT;
	
		packagebreaks_set_package (PACKAGEBREAKS (conflict), broken);
		conflict->files = g_list_prepend (conflict->files, g_strdup (filename));
		packagedata_add_to_breaks (pack, PACKAGEBREAKS (conflict));
		gtk_object_unref (GTK_OBJECT (conflict));
	}
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
	GList *iter, *iter_file;
	PackageData *pack, *pack_other;
	char *filename;
	GList *flat_packages;

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
	trilobite_debug ("-> no-two-packages conflict check begins (%d unique packages)", g_list_length (flat_packages));
#endif
	
	for (iter = g_list_first (flat_packages); iter != NULL; iter = g_list_next (iter)) {
		gboolean reported_yet = FALSE;
		int other_conflicts = 0;

		pack = PACKAGEDATA (iter->data);

		g_message ("file uniqueness checking %s", pack->name);
		eazel_install_emit_file_uniqueness_check (service, pack);

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
					g_message ("Duplicate file : %s occurs in %s and %s",
						   filename, pack->name, pack_other->name);
					reported_yet = TRUE;
				} else {
					other_conflicts++;
				}
				add_file_conflict (pack, pack_other, filename);
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
	/* elements in flat_packages are not to be unreffed */
	g_list_free (flat_packages);
	/* the hashentries point to strings inside the packagedata objects */
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

	/* Hmmm, would eazel_install_package_compare be better ? */
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
	trilobite_debug ("-> file conflict check begins (%d unique packages)", g_list_length (flat_packages));
#endif

	for (iter = g_list_first (flat_packages); iter != NULL; iter = g_list_next (iter)) {
		pack = PACKAGEDATA (iter->data);

		if (pack->conflicts_checked) {
			continue;
		}

		g_message ("file conflict checking %s", pack->name);
		eazel_install_emit_file_conflict_check (service, pack);

		pack->conflicts_checked = TRUE;
		for (iter_file = g_list_first (pack->provides); iter_file != NULL; iter_file = g_list_next (iter_file)) {
			filename = (char *)(iter_file->data);

			/* FIXME: bugzilla.eazel.com 5720
			   this is a patch to circumvent unwanted behaviour.
			   Softcat doens't strip directories when giving NO_DIRS_IN_PROVIDES as fillflag,
			   till it does, I use this check */
			/* but wait!  this is also needed to fix bug 5799 until softcat fixes
			 * forseti bug XXXX [files and directories need to be indicated differently in the xml]
			 */
			if (g_file_test (filename, G_FILE_TEST_ISDIR)) {
				continue;
			}

			owners = eazel_package_system_query (service->private->package_system,
							     service->private->cur_root,
							     filename,
							     EAZEL_PACKAGE_SYSTEM_QUERY_OWNS,
							     OWNS_MUST_HAVE);
			/* No need to check packages that we modify */	
			packagedata_list_prune (&owners, pack->modifies, TRUE, TRUE);

			for (iter_pack = g_list_first (owners); iter_pack != NULL; iter_pack = g_list_next (iter_pack)) {
				pack_owner = (PackageData *)(iter_pack->data);

				if (strcmp (pack_owner->name, pack->name) == 0) {
					/* can't conflict with self */
					gtk_object_unref (GTK_OBJECT (pack_owner));
					continue;
				}

				g_message ("file conflict : package %s already provides %s also provided by %s",
						 pack_owner->name, filename, pack->name);
				if (package_is_upgrading_and_doesnt_need_file (pack_owner, 
									       flat_packages, filename)) {
					/* the owner of this file is a package that we're upgrading, and the
					 * new version no longer has this file, so everything's okay. */
					g_message ("...but it's okay, we're upgrading %s and it ditched that file",
						   pack_owner->name);
				} else {
					add_file_conflict (pack, pack_owner, filename);
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

/* 
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

#if EI2_DEBUG & 0x4
		trilobite_debug ("%s provides %d features", pack->name, g_list_length (pack->features));
#endif
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
#else
		g_message ("checking feature consistency of %s", pack->name);
#endif
		eazel_install_emit_feature_consistency_check (service, pack);

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
						PackageFeatureMissing *feature_missing = packagefeaturemissing_new ();
						PackageData *pack_broken = PACKAGEDATA (break_it->data);

#if EI2_DEBUG & 0x4
						trilobite_debug ("%p %s is broken by %p %s modifying %p %s", 
								 pack_broken, pack_broken->name,
								 pack, pack->name,
								 pack_modified, pack_modified->name);
#else
						g_message ("feature missing : %s breaks, if %s is installed (feature %s would be lost",
							   pack_broken->name,
							   pack->name,
							   feature);
#endif
						pack_broken->status = PACKAGE_DEPENDENCY_FAIL;
						pack->status = PACKAGE_BREAKS_DEPENDENCY;
						packagebreaks_set_package (PACKAGEBREAKS (feature_missing), pack_broken);
						feature_missing->features = g_list_prepend (feature_missing->features, g_strdup (feature));
						packagedata_add_to_breaks (pack, PACKAGEBREAKS (feature_missing));
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

	if (extra_packages && (*extra_packages==NULL)) {
		check_feature_consistency (service, *packages);
		prune_failed_packages (service, packages);
	} else {
#if EI2_DEBUG & 0x4
		trilobite_debug ("extra_packages set, no doing feature consistency check");
#endif		
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
	if (service->private->cancel_download) {
		/* we might have received a "stop" callback recently */
		return;
	}

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
	g_message ("downloading %d packages", g_list_length (packages));

	service->private->cancel_download = FALSE;
	for (iterator = flat_packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = PACKAGEDATA (iterator->data);
		if (pack->filename == NULL) {
			eazel_install_fetch_package (service, pack);
			if (service->private->cancel_download) {
				break;
			}
		}
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

static gboolean
execute (EazelInstall *service,
	 GList *packages,
	 EazelPackageSystemOperation op,
	 int flags) 
{
	TrilobiteRootHelper *root_helper;
	GList *flat_packages;
	gboolean result = FALSE;

	flat_packages = flatten_packagedata_dependency_tree (packages);

	root_helper = gtk_object_get_data (GTK_OBJECT (service), "trilobite-root-helper");
	gtk_object_set_data (GTK_OBJECT (service->private->package_system), 
			     "trilobite-root-helper", root_helper);	

	if (eazel_install_get_test (service)) {
		flags |= EAZEL_PACKAGE_SYSTEM_OPERATION_TEST;
	}

	eazel_install_init_transaction (service);
	
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

	if (service->private->failed_packages == NULL) {
		eazel_install_save_transaction_report (service);
		result = TRUE;
	} 

	eazel_install_init_transaction (service);

	g_list_free (flat_packages);
	
	return result;
}

static void 
install_packages_helper (EazelInstall *service, 
			 GList **packages,
			 GList **extra_packages)
{
#if EI2_DEBUG & 0x4
	trilobite_debug ("-> install_packages_helper");
#endif	
	do_dep_check (service, packages);
	if (service->private->cancel_download) {
		return;
	}
	do_file_conflict_check (service, packages, extra_packages);

#if EI2_DEBUG & 0x8
	trilobite_debug ("FINAL TREE BEGIN");
	dump_tree (*packages);
	if (*extra_packages) {
		trilobite_debug ("EXTRA PACKAGES BEGIN");
		dump_tree (*extra_packages);
	}
	trilobite_debug ("FINAL TREE END");
#endif
	
#if EI2_DEBUG & 0x4
	trilobite_debug ("<- install_packages_helper");
#endif	
	return;
}

static void
set_toplevel (PackageData *package,
	      EazelInstall *service)
{
	package->toplevel = TRUE;
}

static void
expand_package_suites (EazelInstall *service, GList **packages)
{
	GList *iter, *newlist, *sublist;
	EazelSoftCatError err;
	PackageData *pack;

	newlist = NULL;
	for (iter = g_list_first (*packages); iter != NULL; iter = g_list_next (iter)) {
		pack = PACKAGEDATA (iter->data);
		if (pack->suite_id != NULL) {
			/* could be multiple packages */
			sublist = NULL;
			err = eazel_softcat_query (service->private->softcat, pack, 
						   EAZEL_SOFTCAT_SENSE_EQ, MUST_HAVE, &sublist);
			if (err != EAZEL_SOFTCAT_SUCCESS) {
				g_warning ("softcat query on suite (%s) failed", pack->suite_id);
				/* leave the package alone for now */
				newlist = g_list_prepend (newlist, pack);
			} else {
				gtk_object_unref (GTK_OBJECT (pack));
				newlist = g_list_concat (sublist, newlist);
			}
		} else {
			newlist = g_list_prepend (newlist, pack);
		}
	}
	*packages = newlist;
}

/***********************************************************************************/
/* This is the revert majick */

static GList *
get_packages_with_mod_flag (GList *packages,
			    PackageModification mod) 
{
	GList *it;
	GList *res;
	
	res = NULL;
	for (it = packages; it; it = g_list_next (it)) {
		PackageData *pack;
		pack = (PackageData*)it->data;
		if (pack->modify_status == mod) {
			res = g_list_prepend (res, pack);
		}
		if (pack->soft_depends) {
			res = g_list_concat (res, 
					     get_packages_with_mod_flag (pack->soft_depends, mod));
		}
		if (pack->modifies) {
			res = g_list_concat (res, 
					     get_packages_with_mod_flag (pack->modifies, mod));
		}
	}
	return res;
}

/* Function to prune the uninstall list for elements marked as downgrade */
static void
check_uninst_vs_downgrade (GList **inst, 
			   GList **down) 
{
	GList *it;
	GList *remove;
	
	remove = NULL;
	for (it = *inst; it; it = g_list_next (it)) {
		GList *entry;
		PackageData *pack;

		pack = (PackageData*)it->data;
		entry = g_list_find_custom (*down, pack->name, (GCompareFunc)eazel_install_package_name_compare);
		if (entry != NULL) {
			remove = g_list_prepend (remove, it->data);
		}
	}

	for (it = remove; it; it = g_list_next (it)) {
		(*inst) = g_list_remove (*inst, it->data);
	}
}

static void
debug_revert (PackageData *pack, char *str)
{
	char *name = packagedata_get_readable_name (pack);
	g_message ("will %s %s", str, name);
	g_free (name);
}

/***********************************************************************************/
/* This is the uninstall dep check majick */

/*
static int
compare_break_to_package_by_name (PackageBreaks *breakage, PackageData *pack)
{
	PackageData *broken_package = packagebreaks_get_package (breakage);

	return eazel_install_package_compare (broken_package, pack);
}
*/

/* This traverses upwards in the deptree from the initial list, and adds
   all packages that will break to "breaks" */
static void
eazel_uninstall_upward_traverse (EazelInstall *service,
				 GList **packages,
				 GList **failed,
				 GList **breaks)
{
	GList *iterator;
	/*
	  Create set
	  add all packs from packages to set
	  dep check
	  for all break, add to packages and recurse
	 */

	trilobite_debug ("in eazel_uninstall_upward_traverse");

	g_assert (packages!=NULL);
	g_assert (*packages!=NULL);
	g_assert (breaks!=NULL);
	g_assert (*breaks==NULL);

	/* Open the package system */

	/* Add all packages to the set */

	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		GList *matches = NULL;
		GList *match_iterator;
		GList *tmp_breaks = NULL;
		GList *b_iterator = NULL;

		/* Get the packages required by pack */
		trilobite_debug ("checking reqs by %p %s", pack, rpmname_from_packagedata (pack));
		matches = eazel_package_system_query (service->private->package_system,
						      service->private->cur_root,
						      pack,
						      EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES,
						      PACKAGE_FILL_NO_DIRS_IN_PROVIDES | PACKAGE_FILL_NO_DEPENDENCIES);
		
		/* For all of them, mark as a break conflict */
		for (match_iterator = matches; match_iterator; match_iterator = g_list_next (match_iterator)) {
			PackageData *requiredby = (PackageData*)match_iterator->data;;
			
			requiredby->status = PACKAGE_DEPENDENCY_FAIL;
			pack->status = PACKAGE_BREAKS_DEPENDENCY;
			trilobite_debug ("%p %s requires %p %s", 
					 requiredby, requiredby->name, 
					 pack, pack->name);

			/* If the broken package is in packages, just continue */
			if (g_list_find_custom (*packages, requiredby,
						(GCompareFunc)eazel_install_package_compare)) {
#if EI2_DEBUG & 0x4
				trilobite_debug ("skip %p %s", requiredby, requiredby->name);
#endif
				continue;
			}

			/* only add to breaks if it's a new breakage */
			if (g_list_find_custom (*breaks, 
						(gpointer)requiredby, 
						(GCompareFunc)eazel_install_package_compare) == NULL) {
				PackageFeatureMissing *breakage = packagefeaturemissing_new ();
#if EI2_DEBUG & 0x4
				trilobite_debug ("Adding %p %s to breaks", requiredby, requiredby->name);
#endif
				(*breaks) = g_list_prepend ((*breaks), requiredby);
				packagebreaks_set_package (PACKAGEBREAKS (breakage), requiredby);
				packagedata_add_to_breaks (pack, PACKAGEBREAKS (breakage));
				gtk_object_unref (GTK_OBJECT (breakage));
			}

			/* If the pac has not been failed yet (and is a toplevel),
			   fail it */
			if (!g_list_find_custom (*failed, (gpointer)pack->name, 
						 (GCompareFunc)eazel_install_package_name_compare) &&
			    pack->toplevel) {
				(*failed) = g_list_prepend (*failed, pack);
			}
		}
		g_list_foreach (matches, (GFunc)gtk_object_unref, NULL);
		g_list_free (matches);

		/* Now check the packages that broke, this is where eg. uninstalling
		   glib begins to take forever */
		if (*breaks) {
			eazel_uninstall_upward_traverse (service, breaks, failed, &tmp_breaks);
		}
		
		/* Add the result from the recursion */
		for (b_iterator = tmp_breaks; b_iterator; b_iterator = g_list_next (b_iterator)) {
			(*breaks) = g_list_prepend ((*breaks), b_iterator->data);
		}
	}
	
	/* Remove the failed packages */
	for (iterator = *failed; iterator; iterator = g_list_next (iterator)) {
		(*packages) = g_list_remove (*packages, iterator->data);
	}

	trilobite_debug ("out eazel_uninstall_upward_traverse");
}

static void
eazel_uninstall_check_for_install (EazelInstall *service,
				   GList **packages,
				   GList **failed)
{
	GList *iterator;
	GList *remove  = NULL;
	GList *result = NULL;

	trilobite_debug ("in eazel_uninstall_check_for_install");
	g_assert (packages);
	trilobite_debug ("g_list_length (*packages) = %d", g_list_length (*packages)); 
	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {		
		PackageData *pack = (PackageData*)iterator->data;

		if (eazel_package_system_is_installed (service->private->package_system,
						       service->private->cur_root,
						       pack->name,
						       pack->version,
						       pack->minor,
						       EAZEL_SOFTCAT_SENSE_EQ)) {
			
			pack->toplevel = TRUE;
			result = g_list_prepend (result, pack);
		} else {
			pack->status = PACKAGE_CANNOT_OPEN;
			remove = g_list_prepend (remove, pack);
		}		
	}
	
	for (iterator = remove; iterator; iterator=g_list_next (iterator)) {
		(*packages) = g_list_remove (*packages, iterator->data);
		(*failed) = g_list_prepend (*failed, iterator->data);
	}
	g_list_free (remove);
	remove = NULL;
	
	trilobite_debug ("g_list_length (*packages) = %d", g_list_length (*packages)); 
	trilobite_debug ("g_list_length (result) = %d", g_list_length (result)); 
	
	g_list_free (*packages);
	(*packages) = result;
	
	trilobite_debug ("out eazel_uninstall_check_for_install");
}

/* Calls the upward and downward traversal */
static void
eazel_uninstall_globber (EazelInstall *service,
			 GList **packages,
			 GList **failed)
{
	GList *iterator;
	GList *tmp;

	/*
	  call upward with packages
	  call downward with packages and &tmp
	  add all from &tmp to packages
	*/

	trilobite_debug ("in eazel_uninstall_globber");

	tmp = NULL;

	eazel_uninstall_check_for_install (service, packages, failed);
	for (iterator = *failed; iterator; iterator = g_list_next (iterator)) {
		trilobite_debug ("not installed %p %s", 
				 (PackageData*)iterator->data, ((PackageData*)iterator->data)->name);
		eazel_install_emit_uninstall_failed (service, (PackageData*)iterator->data);
	}
	g_list_foreach (*failed, (GFunc)gtk_object_unref, NULL);
	g_list_free (*failed);

	/* If there are still packages and we're not forcing,
	   do upwards traversel */
	if (*packages && !eazel_install_get_force (service)) {
		eazel_uninstall_upward_traverse (service, packages, failed, &tmp);

#if EI2_DEBUG & 0x4
		if (*failed) {
			trilobite_debug ("FAILED");
			dump_tree (*failed);
		}
#endif
		for (iterator = *failed; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack = (PackageData*)iterator->data;
			trilobite_debug ("failed %p %s", pack, pack->name);
			//dump_one_package (GTK_OBJECT (pack), "");
			eazel_install_emit_uninstall_failed (service, pack);
		}
		g_list_foreach (*failed, (GFunc)gtk_object_unref, NULL);
		g_list_free (*failed);
		g_list_free (tmp);
	}

	trilobite_debug ("out eazel_uninstall_glob");
}


/***********************************************************************************/

/* These are the methods exposed to the the rest of the service object */

EazelInstallOperationStatus 
install_packages (EazelInstall *service, GList *categories)
{
	EazelInstallOperationStatus result = EAZEL_INSTALL_NOTHING;       	
	GList *packages;
	GList *extra_packages = NULL;

	eazel_softcat_reset_server_update_flag (service->private->softcat);

	packages = packagedata_list_copy (categorylist_flatten_to_packagelist (categories), TRUE);
	expand_package_suites (service, &packages);
	g_list_foreach (packages, (GFunc)set_toplevel, service);
	do {
		extra_packages = NULL;
		install_packages_helper (service, &packages, &extra_packages);
		if (extra_packages) {
			packages = g_list_concat (packages, extra_packages);
		}
	} while (extra_packages != NULL);

#if EI2_DEBUG & 0x4
	trilobite_debug ("%d packages survived", g_list_length (packages));
#endif	
	if (packages) {
		if (eazel_install_emit_preflight_check (service, packages)) {
			int flags = EAZEL_PACKAGE_SYSTEM_OPERATION_UPGRADE | EAZEL_PACKAGE_SYSTEM_OPERATION_DOWNGRADE | EAZEL_PACKAGE_SYSTEM_OPERATION_FORCE;
			gboolean go_ahead = TRUE;
			
#if EI2_DEBUG & 0x4
			trilobite_debug ("emit_preflight returned true");
#endif	
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
				/* Execute the operation */
				if (execute (service, packages, EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL, flags)) {
					result = EAZEL_INSTALL_INSTALL_OK;
				} 
			}
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
	EazelInstallStatus result = EAZEL_INSTALL_NOTHING;
	GList *packages = NULL;
	GList *failed = NULL;	

	eazel_softcat_reset_server_update_flag (service->private->softcat);

	trilobite_debug (" --> uninstall_all_packages");
	packages = packagedata_list_copy (categorylist_flatten_to_packagelist (categories), TRUE);	
	eazel_uninstall_globber (service, &packages, &failed);
	
	if (packages) {
		if (eazel_install_emit_preflight_check (service, packages)) {
			int flags = EAZEL_PACKAGE_SYSTEM_OPERATION_FORCE;
			if (execute (service, packages, EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL, flags)) {
				result = EAZEL_INSTALL_UNINSTALL_OK;
			}
		}
	}

	g_list_foreach (packages, (GFunc)gtk_object_unref, NULL);
	g_list_free (packages);

	trilobite_debug ("uninstall returns returning %s", 
			 result == EAZEL_INSTALL_UNINSTALL_OK ? "OK" : "FAILED");
	trilobite_debug (" <-- uninstall_all_packages");
	return result;
}

EazelInstallStatus
revert_transaction (EazelInstall *service, 
		    GList *packages)
{
	GList *uninst, *inst, *upgrade, *downgrade;
	CategoryData *cat;
	GList *categories;
	EazelInstallStatus result = EAZEL_INSTALL_NOTHING;

	uninst = get_packages_with_mod_flag (packages, PACKAGE_MOD_INSTALLED);
	inst = get_packages_with_mod_flag (packages, PACKAGE_MOD_UNINSTALLED);
	upgrade = get_packages_with_mod_flag (packages, PACKAGE_MOD_DOWNGRADED);
	downgrade = get_packages_with_mod_flag (packages, PACKAGE_MOD_UPGRADED);

	check_uninst_vs_downgrade (&uninst, &downgrade);

	g_list_foreach (uninst, (GFunc)debug_revert, "uninstall");
	g_list_foreach (inst, (GFunc)debug_revert, "install");
	g_list_foreach (downgrade, (GFunc)debug_revert, "downgrade");
	g_list_foreach (upgrade, (GFunc)debug_revert, "upgrade");

	cat = categorydata_new ();
	categories = g_list_prepend (NULL, cat);

	if (uninst) {
		eazel_install_set_uninstall (service, TRUE);
		eazel_install_set_downgrade (service, FALSE);
		eazel_install_set_update (service, FALSE);
		cat->packages = uninst;
		if (uninstall_packages (service, categories) == EAZEL_INSTALL_UNINSTALL_OK) {
			result = EAZEL_INSTALL_REVERSION_OK;
		}
	}
	if (inst) {
		eazel_install_set_uninstall (service, FALSE);
		eazel_install_set_downgrade (service, FALSE);
		eazel_install_set_update (service, FALSE);
		cat->packages = inst;
		if (install_packages (service, categories) == EAZEL_INSTALL_UNINSTALL_OK) {
			result = EAZEL_INSTALL_REVERSION_OK;
		}
	}
	if (downgrade) {
		eazel_install_set_uninstall (service, FALSE);
		eazel_install_set_downgrade (service, TRUE);
		eazel_install_set_update (service, FALSE);
		cat->packages = downgrade;
		if (install_packages (service, categories) == EAZEL_INSTALL_UNINSTALL_OK) {
			result = EAZEL_INSTALL_REVERSION_OK;
		}
	}
	if (upgrade) {
		eazel_install_set_uninstall (service, FALSE);
		eazel_install_set_downgrade (service, TRUE);
		eazel_install_set_update (service, TRUE);
		cat->packages = upgrade;
		if (install_packages (service, categories) == EAZEL_INSTALL_UNINSTALL_OK) {
			result = EAZEL_INSTALL_REVERSION_OK;
		}
	}


	categorydata_destroy (cat);
	g_list_free (categories);

	return result;
}


