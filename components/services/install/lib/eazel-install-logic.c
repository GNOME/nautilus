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

#include "eazel-install-logic.h"
#include "eazel-install-xml-package-list.h"
#include "eazel-install-public.h"
#include "eazel-install-private.h"
#include "eazel-install-rpm-glue.h"
#include "eazel-install-query.h"
#include "eazel-install-logic2.h"

/* We use rpmvercmp to compare versions... */
#include <rpm/rpmlib.h>
#include <rpm/misc.h>

#ifndef EAZEL_INSTALL_NO_CORBA
#include <libtrilobite/libtrilobite.h>
#else
#include <libtrilobite/libtrilobite-service.h>
#include <libtrilobite/trilobite-root-helper.h>
#endif

#include <libtrilobite/trilobite-core-utils.h>
#include <string.h>
#include <time.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#ifdef EAZEL_INSTALL_SLIM
#include <sys/wait.h>
#endif

static gboolean eazel_install_do_install_packages (EazelInstall *service,
						   GList* packages);

static int eazel_install_start_transaction (EazelInstall *service, 
					    GList* packages);

static gboolean eazel_install_ensure_deps (EazelInstall *service, 
					   GList **filenames, 
					   GList **fails);

static void eazel_uninstall_globber (EazelInstall *service,
				     GList **packages,
				     GList **failed);

static gboolean eazel_install_download_packages (EazelInstall *service,
						 gboolean toplevel,
						 GList **packages,
						 GList **failed_packages);

static gboolean  eazel_install_check_for_file_conflicts (EazelInstall *service,
							 PackageData *pack,
							 GList **breaks,
							 GList **requires);

static void eazel_install_prune_packages (EazelInstall *service, 
					  PackageData *pack, 
					  ...);
/* 
   Checks for pre-existance of all the packages
 */
static void
eazel_install_pre_install_packages (EazelInstall *service,
				    GList **packages) 
{
	GList *iterator;
	GList *failed_packages = NULL;

	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		EazelInstallStatus inst_status;
		gboolean skip = FALSE;
		
		inst_status = eazel_install_check_existing_packages (service, pack);

		/* If in force mode, install it under all circumstances.
		   if not, only install if not already installed in same
		   version or up/downgrade is set */
		if (eazel_install_get_force (service) ||
		    (eazel_install_get_downgrade (service) && inst_status == EAZEL_INSTALL_STATUS_DOWNGRADES) ||
		    (eazel_install_get_update (service) && inst_status == EAZEL_INSTALL_STATUS_UPGRADES) ||
		    inst_status == EAZEL_INSTALL_STATUS_NEW_PACKAGE) {
			skip = FALSE;
		} else {
			skip = TRUE;
		}
		
		if (skip) {
			trilobite_debug ("Skipping %s...", pack->name);
#if 0
			/* Nuke the modifies list again, since we don't want to see them */
			g_list_foreach (pack->modifies, 
					(GFunc)packagedata_destroy, 
					GINT_TO_POINTER (TRUE));
			g_list_free (pack->modifies);
			pack->modifies = NULL;
#endif

			/* Add it to the list of packages to nuke at the end
			   of this function */
			failed_packages = g_list_prepend (failed_packages, pack);
		}
	}
	
	for (iterator = failed_packages; iterator; iterator=g_list_next (iterator)) {
		eazel_install_prune_packages (service, 
					      (PackageData*)iterator->data,
					      packages, NULL);
	}
	g_list_free (failed_packages);
}

EazelInstallStatus
ei_install_packages (EazelInstall *service, GList *categories) {
	EazelInstallStatus result;

	if (categories == NULL) {
		trilobite_debug (_("Reading the install package list %s"), eazel_install_get_package_list (service));
		categories = parse_local_xml_package_list (eazel_install_get_package_list (service), NULL, NULL);
	}

	result = EAZEL_INSTALL_NOTHING;
	if (categories != NULL) {
		/* First, collect all packages in one list */
		GList *packages = categorylist_flatten_to_packagelist (categories);

		/* Now download all the packages */
		if (eazel_install_download_packages (service, TRUE, &packages, NULL)) {

			/* check for packages that are already installed */
			eazel_install_pre_install_packages (service, &packages);

			/* Files downloaded, now install */
			if (eazel_install_do_install_packages (service, packages)) {
				result |= EAZEL_INSTALL_INSTALL_OK;
			}
		}
	}
	
	return result;
} /* end install_new_packages */

/*
  Download all the packages and keep doing that by recursively
  calling eazel_install_download_packages with package->soft_depends
 */
static gboolean
eazel_install_download_packages (EazelInstall *service,
				 gboolean toplevel,
				 GList **packages,
				 GList **failed_packages)
{
	GList *iterator;
	gboolean result = TRUE;
	GList *remove_list = NULL;

	g_assert (packages);
	g_assert (*packages);

	for (iterator = *packages; (iterator != NULL) && result; iterator = g_list_next (iterator)) {
		PackageData* package = (PackageData*)iterator->data;
		gboolean fetch_package;
		
		fetch_package = TRUE;

		trilobite_debug ("init for %s (%s/%s)", package->name, package->version ? package->version : "NO VERSION",
				toplevel?"TRUE":"FALSE");
		/* if filename in the package is set, but the file
		   does not exist, get it anyway */
		if (package->filename) {
			trilobite_debug ("Filename set, and file exists = %d", 
				   g_file_test (package->filename, G_FILE_TEST_ISFILE));
			if (g_file_test (package->filename, G_FILE_TEST_ISFILE)) {
				/* Don't fetch, but load rpm header and return
				   ok */
				fetch_package = FALSE;	
				result = TRUE;
				package = eazel_package_system_load_package (service->private->package_system,
									     package, 
									     package->filename,
									     PACKAGE_FILL_NO_DIRS_IN_PROVIDES);
			} else {
				/* The file didn't exist, remove the 
				   leading path and set the filename, plus
				   toggle the fetch_package to TRUE */
				g_free (package->filename);
				package->filename = g_strdup (g_basename (package->filename));
			}
		} else if (!eazel_install_get_force (service) && package->version) {
			/* If the package has a version set, check that we don't already have
			   the same or newer installed. This is almost the same check as
			   in eazel_install_pre_install_package. The reason for two checks is
			   - first check before download (if possible)
			   - after download, when we for sure have access to the version, check again
			   - we do this before do_dependency_check to avoid downloaded soft_deps.
			*/
			EazelInstallStatus inst_status;

			inst_status = eazel_install_check_existing_packages (service, package);
			if (eazel_install_get_downgrade (service) && inst_status == EAZEL_INSTALL_STATUS_DOWNGRADES) {
				trilobite_debug (_("Will download %s"), package->name);
				/* must download... */
			} else if (inst_status == EAZEL_INSTALL_STATUS_QUO ||
				   inst_status == EAZEL_INSTALL_STATUS_DOWNGRADES) {
				/* Nuke the modifies list again, since we don't want to see them */
				g_list_foreach (package->modifies, 
						(GFunc)packagedata_destroy, 
						GINT_TO_POINTER (TRUE));
				g_list_free (package->modifies);
				package->modifies = NULL;
				/* don't fecth the package */
				fetch_package = FALSE;
				/* Add it to the list of packages to nuke at the end
				   of this function */
				remove_list = g_list_prepend (remove_list, package);
				trilobite_debug (_("%s already installed"), package->name);
			}
		} 

		if (fetch_package) {
			result = eazel_install_fetch_package (service, package);

			if (!result) {
				package->status = PACKAGE_CANNOT_OPEN;
				remove_list = g_list_prepend (remove_list, package);
			} else {
#if 0
				/* If downloaded package has soft_deps,
				   fetch them by a recursive call */
				if (package->soft_depends) {
					result = eazel_install_download_packages (service,
										  FALSE,
										  &package->soft_depends,
										  NULL);
				}
#endif
			}
		}

		if (result) {
			package->toplevel = toplevel;
			if (package->source_package) {
				package->status = PACKAGE_SOURCE_NOT_SUPPORTED;
				remove_list = g_list_prepend (remove_list, package);
			}
			if (strlen ("debug")) {
				char *tmp = packagedata_dump (package, TRUE);
				fprintf (stderr, "%s", tmp);
				g_free (tmp);
			}
		}
	}

	for (iterator = remove_list; iterator; iterator = g_list_next (iterator)) {
		PackageData *package = (PackageData*)(iterator->data);
		eazel_install_prune_packages (service, package, packages, NULL);
	}

	if (failed_packages) {
		(*failed_packages) = remove_list;
	}

	return result;
}

/*
  This function checks all files in pack->provides, and
  checks if another already installed package owns this file.
  returns FALSE is there are no conflicts, and TRUE if there
  are.

  If there are conflicts because of a related package,
  this package is added to *requires.
 */
static gboolean 
eazel_install_check_for_file_conflicts (EazelInstall *service,
					PackageData *pack,
					GList **breaks,
					GList **requires)
{
	GList *owners;
	GList *iterator;
	/* Set optimism to high */
	gboolean result = FALSE;
	
	g_assert (service);
	g_assert (pack);
	g_assert (requires);
	g_assert (*requires == NULL);

	trilobite_debug ("Checking file conflicts for %s", pack->name);

	for (iterator = pack->provides; iterator; glist_step (iterator)) {
		char *filename = (char*)iterator->data;		

		/* many packages will supply some dirs, eg /usr/share/locale/../LC_MESSAGES
		   dirs, so don't check those 

		   eazel-install-types
		   (packagedata_fill_from_rpm_header) now does not add
		   these. This is more safe, as checking the file
		   could still cause a conflict, if the file was not
		   on the system but two dirs had the same dir 

		if (g_file_test (filename, G_FILE_TEST_ISDIR)) {
			continue;
		} */

		owners = eazel_package_system_query (service->private->package_system,
						     service->private->cur_root,
						     filename,
						     EAZEL_PACKAGE_SYSTEM_QUERY_OWNS,
						     PACKAGE_FILL_NO_DIRS_IN_PROVIDES);
		packagedata_list_prune (&owners, pack->modifies, TRUE, TRUE);
		
		if (g_list_length (owners) > 1) {
			GList *pit;
			/* FIXME bugzilla.eazel.com 3511:
			   More than one packages owns this file,
			   this cannot happen (or should not at least)
			*/
			trilobite_debug ("***************************************************");
			trilobite_debug ("More than one package owns the file %s", filename);
			trilobite_debug ("This is filed as bug 2959");
			trilobite_debug ("Try rpm --rebuilddb");
			for (pit = owners; pit; pit = g_list_next (pit)) {
				char *tmp;
				PackageData *owner = (PackageData*)(pit->data);
				tmp = packagedata_get_readable_name (owner);
				trilobite_debug ("a owner is %s", tmp);
				g_free (tmp);
			}
			trilobite_debug ("halting...");
			g_assert_not_reached ();
		} else if (g_list_length (owners) == 1) {
			PackageData *owner = (PackageData*)owners->data;
			
			/* If the package owner is already in the breaks list for the package,
			   or in the *requires, continue  */
			if (g_list_find_custom (*breaks, owner->name, 
						(GCompareFunc)eazel_install_package_name_compare) ||
			    g_list_find_custom (*requires, owner->name, 
						(GCompareFunc)eazel_install_package_name_compare)) {
				/* trilobite_debug ("already breaking %s", owner->name); */
				packagedata_destroy (owner, TRUE);
				owner = NULL;
				continue;
			}

			if (strcmp (pack->name, owner->name)) {
				trilobite_debug ("file %s from package %s conflicts with file from package %s", 
						 filename, pack->name, owner->name);

				result = TRUE;
				if (eazel_install_check_if_related_package (service, pack, owner)) {
					trilobite_debug ("Package %s may be related to %s", 
							 owner->name, pack->name);
					g_free (owner->version);
					owner->version = g_strdup (pack->version);
					(*requires) = g_list_prepend (*requires, owner);
				} else {
					owner->status = PACKAGE_FILE_CONFLICT;
					(*breaks) = g_list_prepend (*breaks, owner);
				}
				
			} else {
				/* else it's the same package and it's okay */
				/* so FREE IT YOU SICK MONKEY! */
				packagedata_destroy (owner, TRUE);
			}
		}
		/* free the _simple_query result list */
		g_list_free (owners);

#ifdef EAZEL_INSTALL_SLIM
		/* In the slim, we need to enter the g_main_loop during file check */		
		g_main_iteration (FALSE);
#endif

	}
	return result;
}

static gboolean
eazel_install_do_install_packages (EazelInstall *service,
				  GList* packages) 
{
	gboolean rv = TRUE;
	GList* failedfiles = NULL;

	if (packages) {
		rv = FALSE;
		eazel_install_ensure_deps (service, &packages, &failedfiles);
		if (g_list_length (packages)) {
			if (eazel_install_start_transaction (service, packages) == 0) {
				rv = TRUE;
			}			
			g_list_free (packages);
		}
	}

	return rv;
} /* end install_packages */

static EazelInstallStatus
uninstall_all_packages (EazelInstall *service,
			GList *categories) 
{
	EazelInstallStatus result = EAZEL_INSTALL_UNINSTALL_OK;

	while (categories) {
		CategoryData* cat = categories->data;
		GList *failed;

		trilobite_debug (_("Category = %s"), cat->name);

		failed = NULL;
		eazel_uninstall_globber (service, &cat->packages, &failed);

		if (eazel_install_start_transaction (service, cat->packages) != 0) {
			result = EAZEL_INSTALL_NOTHING;
		}

		categories = g_list_next (categories);
	}
	return result;
}

EazelInstallStatus
ei_uninstall_packages (EazelInstall *service,
		    GList* categories) 
{
	EazelInstallStatus result = EAZEL_INSTALL_NOTHING;
	
	result |= uninstall_all_packages (service, categories);

	return result;

} /* end install_new_packages */


static GList *
ei_get_packages_with_mod_flag (GList *packages,
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
					     ei_get_packages_with_mod_flag (pack->soft_depends, mod));
		}
		if (pack->modifies) {
			res = g_list_concat (res, 
					     ei_get_packages_with_mod_flag (pack->modifies, mod));
		}
	}
	return res;
}

/* Function to prune the uninstall list for elements marked as downgrade */
static void
ei_check_uninst_vs_downgrade (GList **inst, 
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

static void hest (PackageData *pack, char *str) {
	trilobite_debug ("Must %s %s", str, pack->name);
}

EazelInstallStatus
ei_revert_transaction (EazelInstall *service, 
		    GList *packages)
{
	GList *uninst, *inst, *upgrade, *downgrade;
	CategoryData *cat;
	GList *categories;
	EazelInstallStatus result = EAZEL_INSTALL_NOTHING;

	uninst = ei_get_packages_with_mod_flag (packages, PACKAGE_MOD_INSTALLED);
	inst = ei_get_packages_with_mod_flag (packages, PACKAGE_MOD_UNINSTALLED);
	upgrade = ei_get_packages_with_mod_flag (packages, PACKAGE_MOD_DOWNGRADED);
	downgrade = ei_get_packages_with_mod_flag (packages, PACKAGE_MOD_UPGRADED);

	ei_check_uninst_vs_downgrade (&uninst, &downgrade);

	g_list_foreach (uninst, (GFunc)hest, "uninstall");
	g_list_foreach (inst, (GFunc)hest, "install");
	g_list_foreach (downgrade, (GFunc)hest, "downgrade");
	g_list_foreach (upgrade, (GFunc)hest, "upgrade");

	cat = categorydata_new ();
	categories = g_list_prepend (NULL, cat);

	if (uninst) {
		eazel_install_set_uninstall (service, TRUE);
		eazel_install_set_downgrade (service, FALSE);
		eazel_install_set_update (service, FALSE);
		cat->packages = uninst;
		result |= ei_uninstall_packages (service, categories);
	}
	if (inst) {
		eazel_install_set_uninstall (service, FALSE);
		eazel_install_set_downgrade (service, FALSE);
		eazel_install_set_update (service, FALSE);
		cat->packages = inst;
		result |= ei_install_packages (service, categories);
	}
	if (downgrade) {
		eazel_install_set_uninstall (service, FALSE);
		eazel_install_set_downgrade (service, TRUE);
		eazel_install_set_update (service, FALSE);
		cat->packages = downgrade;
		result |= ei_install_packages (service, categories);
	}
	if (upgrade) {
		eazel_install_set_uninstall (service, FALSE);
		eazel_install_set_downgrade (service, TRUE);
		eazel_install_set_update (service, TRUE);
		cat->packages = upgrade;
		result |= ei_install_packages (service, categories);
		g_list_foreach (upgrade, (GFunc)packagedata_destroy, GINT_TO_POINTER (TRUE));
	}

	return result;
}


void
eazel_install_do_transaction_add_to_transaction (EazelInstall *service,
						 PackageData *pack)
{
	service->private->transaction = g_list_prepend (service->private->transaction,
							pack);
}

static void
eazel_install_do_transaction_save_report_helper (xmlNodePtr node,
						     GList *packages)
{
	GList *iterator;

	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack;
		char *tmp;
		pack = (PackageData*)iterator->data;
		switch (pack->modify_status) {
		case PACKAGE_MOD_INSTALLED:			
			tmp = g_strdup_printf ("Installed %s", pack->name);
			xmlNewChild (node, NULL, "DESCRIPTION", tmp);
			g_free (tmp);
			break;
		case PACKAGE_MOD_UNINSTALLED:			
			tmp = g_strdup_printf ("Uninstalled %s", pack->name);
			xmlNewChild (node, NULL, "DESCRIPTION", tmp);
			g_free (tmp);
			break;
		default:
			break;
		}
		if (pack->modifies) {
			eazel_install_do_transaction_save_report_helper (node, pack->modifies);
		}
	}
}

static void
eazel_install_do_transaction_save_report (EazelInstall *service) 
{
	FILE *outfile;
	xmlDocPtr doc;
	xmlNodePtr node, root;
	char *name = NULL;

	if (eazel_install_get_transaction_dir (service) == NULL) {
		g_warning ("Transaction directory not set, not storing transaction report");
	}

	/* Ensure the transaction dir is present */
	if (! g_file_test (eazel_install_get_transaction_dir (service), G_FILE_TEST_ISDIR)) {
		int retval;
		retval = mkdir (eazel_install_get_transaction_dir (service), 0755);		       
		if (retval < 0) {
			if (errno != EEXIST) {
				g_warning (_("Could not create transaction directory (%s)! ***\n"), 
					   eazel_install_get_transaction_dir (service));
				return;
			}
		}
	}

	/* Create xml */
	doc = xmlNewDoc ("1.0");
	root = node = xmlNewNode (NULL, "TRANSACTION");
	xmlDocSetRootElement (doc, node);

	/* Make a unique name */
	name = g_strdup_printf ("%s/transaction.%lu", eazel_install_get_transaction_dir (service),
				(unsigned long) time (NULL));
	while (g_file_test (name, G_FILE_TEST_ISFILE)) {
		g_free (name);
		sleep (1);
		name = g_strdup_printf ("%s/transaction.%lu", 
					eazel_install_get_transaction_dir (service), 
					(unsigned long) time (NULL));
	}

	trilobite_debug (_("Writing transaction to %s"), name);
	
	/* Open and save */
	outfile = fopen (name, "w");
	xmlAddChild (root, eazel_install_packagelist_to_xml (service->private->transaction, FALSE));
	node = xmlAddChild (node, xmlNewNode (NULL, "DESCRIPTIONS"));

	{
		char *tmp;
		tmp = g_strdup_printf ("%lu", (unsigned long) time (NULL));		
		xmlNewChild (node, NULL, "DATE", tmp);
		g_free (tmp);
	}

	eazel_install_do_transaction_save_report_helper (node, service->private->transaction);

	xmlDocDump (outfile, doc);
	
	fclose (outfile);
	g_free (name);
}

/* 
   This checks, that for a given set of packages, no two packages
   contains the same file.
   This is done by filling a hashtable with the files from
   package->provides (which have full pathname) and link to the
   owning package. Before adding a file to the hashtable, lookup the
   file first. If result is non-null, problem... 

   Did I mention that this function leaks memory like a russian submarine?  -robey
 */
static gboolean 
eazel_install_do_transaction_all_files_check (EazelInstall *service,
					      GList **packages)
{
	gboolean result = TRUE;
	GList *iterator;
	GList *conflicts = NULL;  /* PackageRequirements. ->package is the first found package
				     that providing a file, ->required is the second file that
				     has the same file */
	GHashTable *file_to_pack; /* maps from a filename to a packagedata struct */
	
	if (eazel_install_get_force (service) || 
	    eazel_install_get_ignore_file_conflicts (service) ||
		(g_list_length (*packages) == 1 )) {		
		trilobite_debug ("not performing file conflict check");
		return result;
	}

	file_to_pack = g_hash_table_new (g_str_hash, g_str_equal);

	/* Check all the packages */
	for (iterator = *packages; iterator; glist_step (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		GList *file_iterator;
		int reported_yet = FALSE;
		int other_conflicts = 0;

		/* Check all files provided */
		for (file_iterator = pack->provides; file_iterator; glist_step (file_iterator)) {
			char *fname = (char*)file_iterator->data;
			/* Lookup and check what happens... */
			PackageData *previous_pack = g_hash_table_lookup (file_to_pack,
									  fname);
			if (previous_pack) {
				/* Dang, fname is owned by previous_pack but pack also adds it */
				/* The use of reported_yet && other_conflicts is purely for
				   debug nicety. It ensures that only one fileconflicts pr 
				   package is printed, whereas the alternative is eg. 200 */
				if (! reported_yet) {
					trilobite_debug ("conflict, file %s from package %s is also in %s", 
							 fname, 
							 pack->name,
							 previous_pack->name);
					reported_yet = TRUE;
				} else {
					other_conflicts++;
				}
				if (!g_list_find_custom (conflicts, 
							 pack,
							 (GCompareFunc)eazel_install_requirement_dep_compare)) {
					PackageRequirement *req;
					
					req = packagerequirement_new (previous_pack, pack);
					conflicts = g_list_prepend (conflicts, req);
				}
			} else {
				/* File is ok */
				g_hash_table_insert (file_to_pack, 
						     fname,
						     pack);
			}
		}
		if (other_conflicts) {
			trilobite_debug ("(%d other conflicts from the same package... *sigh*)", other_conflicts);
		}
	}

	for (iterator = conflicts; iterator; glist_step (iterator)) {
		PackageRequirement *req = (PackageRequirement*)iterator->data;
		
		result = FALSE;
		/* Need to fail the package here to fully fix bug
		   FIXME bugzilla.eazel.com 3374: */
		trilobite_debug ("Conflict between %s and %s", req->package->name, req->required->name);
		req->package->status = PACKAGE_FILE_CONFLICT;
		req->required->status = PACKAGE_FILE_CONFLICT;
		packagedata_add_pack_to_breaks (req->package, req->required);
		eazel_install_prune_packages (service, req->package, packages, NULL);
	}

	return result;
}


static unsigned long
get_total_size_of_packages (const GList *packages)
{
	const GList *iterator;
	unsigned long result = 0;
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		result += pack->bytesize;
	}
	return result;
}

/* A GHRFunc to clean
   out the name_to_package hash table 
*/
static gboolean
eazel_install_clean_name_to_package_hash (char *key,
					  PackageData *pack,
					  EazelInstall *service)
{
	g_free (key);
	return TRUE;
}

gboolean eazel_install_start_signal (EazelPackageSystem *system,
				     EazelPackageSystemOperation op,
				     const PackageData *pack,
				     EazelInstall *service)
{
	service->private->infoblock[2]++;
	switch (op) {
	case EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL:
	case EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL:
		eazel_install_emit_install_progress (service, 
						     pack,
						     service->private->infoblock[2], service->private->infoblock[3],
						     0, pack->bytesize,
						     service->private->infoblock[4], service->private->infoblock[5]);				     
		break;
	default:
		break;
	}
	return TRUE;
}

gboolean eazel_install_end_signal (EazelPackageSystem *system,
				   EazelPackageSystemOperation op,
				   const PackageData *pack,
				   EazelInstall *service)
{
	switch (op) {
	case EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL:
	case EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL:
		eazel_install_emit_install_progress (service, 
						     pack,
						     service->private->infoblock[2], service->private->infoblock[3],
						     pack->bytesize, pack->bytesize,
						     service->private->infoblock[4], service->private->infoblock[5]);				     
		break;
	default:
		break;
	}
	return TRUE;
}

gboolean  eazel_install_progress_signal (EazelPackageSystem *system,
					 EazelPackageSystemOperation op,
					 const PackageData *pack,
					 unsigned long *info,
					 EazelInstall *service)
{
	service->private->infoblock[4] = info[4];
	if ((info[0] != 0) && (info[0] != info[1])) {
		switch (op) {
		case EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL:
		case EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL:
			eazel_install_emit_install_progress (service, 
							     pack,
							     service->private->infoblock[2], service->private->infoblock[3],
							     info[0], pack->bytesize,
							     info[4], info[5]);
			break;
		default:
			break;
		}
	}
	return TRUE;
}

gboolean eazel_install_failed_signal (EazelPackageSystem *system,
				      EazelPackageSystemOperation op,
				      const PackageData *pack,
				      EazelInstall *service)
{
	trilobite_debug ("*** %s failed", pack->name);
	if (op==EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL) {
		eazel_install_emit_install_failed (service, pack);
	} else if (op==EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL) {
		eazel_install_emit_uninstall_failed (service, pack);
	}
	return TRUE;
}

/* This begins the package transaction.
   Return value is number of failed packages 
*/

int
eazel_install_start_transaction (EazelInstall *service,
				 GList* packages) 
{
	TrilobiteRootHelper *root_helper;
	int res;
	int flag = 0;

	if (g_list_length (packages) == 0) {
		return -1;
	}

	res = 0;

	if (service->private->downloaded_files) {
		/* I need to get the length here, because all_files_check can alter the list */
		int l  = g_list_length (packages);
		/* Unless we're force installing, check file conflicts */
		if (eazel_install_get_force (service) == FALSE) {
			if(!check_md5_on_files (service, packages)) {
				res = l;
			}
		}
	}

	if (eazel_install_get_test (service)) {
		flag |= EAZEL_PACKAGE_SYSTEM_OPERATION_TEST;
	}
	if (eazel_install_get_force (service)) {
		flag |= EAZEL_PACKAGE_SYSTEM_OPERATION_FORCE;
	}
	if (eazel_install_get_update (service)) {
		flag |= EAZEL_PACKAGE_SYSTEM_OPERATION_UPGRADE;
	}
	if (eazel_install_get_downgrade (service)) {
		flag |= EAZEL_PACKAGE_SYSTEM_OPERATION_DOWNGRADE;
	}

	root_helper = gtk_object_get_data (GTK_OBJECT (service), "trilobite-root-helper");
	gtk_object_set_data (GTK_OBJECT (service->private->package_system), 
			     "trilobite-root-helper", root_helper);	

	/* Init the hack var to emit the old style progress signals */
	service->private->infoblock [0] = 0;
	service->private->infoblock [1] = 0;
	service->private->infoblock [2] = 0;
	service->private->infoblock [3] = g_list_length (packages);
	service->private->infoblock [4] = 0;
	service->private->infoblock [5] = get_total_size_of_packages (packages);

	if (eazel_install_emit_preflight_check (service, packages)) {
		/* this makes the primary packages appear before their dependencies.  very useful for installs
		 * via the install-view, where only toplevel packages cause the package detail info to update.
		 */
		packages = g_list_reverse (packages);

		if (eazel_install_get_uninstall (service)) {
			eazel_package_system_uninstall (service->private->package_system,
							service->private->cur_root,
							packages,
							flag);
		} else {
			eazel_package_system_install (service->private->package_system,
						      service->private->cur_root,
						      packages,
						      flag);
		}

		eazel_install_do_transaction_save_report (service);
	}
	
	g_list_free (service->private->transaction);
	service->private->transaction = NULL;

	g_hash_table_foreach_remove (service->private->name_to_package_hash,
				     (GHRFunc)eazel_install_clean_name_to_package_hash,
				     service);

	return res;
} /* end start_transaction */


/* Checks if pack depends on dep, by doing a deep tree search */
static gboolean 
eazel_install_check_if_depends_on (PackageData *pack, 
				   PackageData *dep) 
{
	gboolean result = FALSE;
	GList *iterator;

	for (iterator = pack->soft_depends; !result && iterator; glist_step (iterator)) {
		PackageData *nisse = (PackageData*)iterator->data;
		if (nisse == dep) {
			result = TRUE;
		} else if (eazel_install_check_if_depends_on (nisse, dep)) {
			/* trilobite_debug ("nope, recursing"); */
			result = TRUE;
		}
	}

	return result;
}

/*
  The helper for eazel_install_prune_packages.
  If the package is in "pruned", it has already been marked
  for pruning.
  Otherwise, prune first it's softdepends, then all
  packages that depend on it.
 */
static void
eazel_install_prune_packages_helper (EazelInstall *service, 
				     GList **packages,
				     GList **pruned,
				     PackageData *pack)
{
	GList *iterator;
	char *tmp;

	g_return_if_fail (pack!=NULL);
        /* If already pruned, abort */
	if (g_list_find (*pruned, pack) || pack->name==NULL) {
		return;
	}
	tmp = packagedata_get_readable_name (pack);
	trilobite_debug (_("Removing package %s (0x%p) %s"), 
			 tmp,
			 pack,
			 pack->toplevel ? "(emit fail)" :"()");
	g_free (tmp);
	if (pack->toplevel) {
		/* We only emit signal for the toplevel packages, 
		   and only delete them. They _destroy function destroys
		   the entire dep tree */
		eazel_install_emit_install_failed (service, pack);
	}
	/* Add to list of pruned packages */
	(*pruned) = g_list_prepend (*pruned, pack);

	/* Prune all it's soft_deps */
	for (iterator = pack->soft_depends; iterator; iterator = g_list_next (iterator)) {
		PackageData *sub;
		sub = (PackageData*)iterator->data;
		eazel_install_prune_packages_helper (service, packages, pruned, sub);
	}
	
	/* For all packages in "packages", check if they depend on this */
	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *super;
		
		super = (PackageData*)iterator->data;
		/* FIXME bugzilla.eazel.com 3542:
		   This is the cause of 3542. 
		   In this specific case, gnome-print is removed from the toplevel and from
		   1st sublevel of soft_deps.
		   The problem is, that this g_list_find only looks down one level, and does'nt
		   search the entire tree, duh.
		   I need a find_custom call that does this the right way */
		if (eazel_install_check_if_depends_on (super, pack)) {			
			eazel_install_prune_packages_helper (service, packages, pruned, super);
		}
	}
}

/*
  Used to remove a package "pack" and all 
  packages in "packages" that depends
  on "pack".
  
  To do this, we need the _helper, which gathers
  the stripped files into "pruned". That way, we
  can safely traverse without altering
  the lists during the for loops (as g_list_remove
  will fuck up the for loop).

  This may end in a recursive loop if
  the ->soft_depends points to something
  upwards in the dep tree (circular dependency)

  First it calls prune_helper for all the given packages, 
  at each iteration it removes the pruned (from list "pruned")
  packages.

  Finally it deallocates all the pruned packages.
  
*/

static void
eazel_install_prune_packages (EazelInstall *service, 
			      PackageData *pack, 
			      ...)
{
	va_list ap;
	GList *pruned;
	GList *iterator;
	GList **packages;

	g_return_if_fail (pack!=NULL);

	va_start (ap, pack);
	
	pruned = NULL;
	while ( (packages = va_arg (ap, GList **)) != NULL) {
		eazel_install_prune_packages_helper (service,
						     packages,
						     &pruned,
						     pack);
		for (iterator = pruned; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack = (PackageData*)iterator->data;
			/* trilobite_debug ("%s pruned", pack->name); */
			(*packages) = g_list_remove (*packages, pack);
		};
	} 
	
	/* Note, don't destroy, all packages are destroyed when the
	   categories are destroyed 
	for (iterator = pruned; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack;
		pack = (PackageData*)iterator->data;
		packagedata_destroy (pack, TRUE); 
	};
	*/

	g_list_free (pruned);

	va_end (ap);
}

static void
eazel_install_add_to_extras_foreach (char *key, GList *list, GList **extrapackages)
{
	GList *iterator;
	PackageData *dep;
	for (iterator = list; iterator; iterator = g_list_next (iterator)) {
		dep = (PackageData*)iterator->data;
		(*extrapackages) =  g_list_prepend (*extrapackages, dep);		
	}
	g_list_free (list);
}


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
eazel_install_check_if_related_package (EazelInstall *service,
					PackageData *package,
					PackageData *dep)
{
	/* Pessimisn mode = HIGH */
	gboolean result = FALSE;
	GList *potiental_mates;
	GList *iterator;
	char **dep_name_elements;
	
	dep_name_elements = g_strsplit (dep->name, "-", 80);

	/* First check, if package modifies a package with the same version
	   number as dep->version */
	potiental_mates = g_list_find_custom (package->modifies, 
					      dep->version, 
					      (GCompareFunc)eazel_install_package_version_compare);
	for (iterator = potiental_mates; iterator; glist_step (iterator)) {
		PackageData *modpack = (PackageData*)iterator->data;
		
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
			g_strfreev (dep_name_elements);
			g_strfreev (mod_name_elements);			
		}
	}
	return result;
}

static gboolean
eazel_install_fetch_dependencies (EazelInstall *service, 
				  GList **packages,
				  GList **extrapackages,
				  GList **failedpackages,
				  GList *requirements)
{
	GList *iterator;
	/* Contains the packages downloaded when handling the list of requirements */
	GList *extras_in_this_batch = NULL;
	GHashTable *extras;
	gboolean fetch_result;
	
	extras = g_hash_table_new (g_str_hash, g_str_equal);
	fetch_result = FALSE;

	trilobite_debug ("%d requirements to be fetched/resolved", g_list_length (requirements));
	for (iterator = requirements; iterator; glist_step (iterator)) {
		PackageRequirement *req = (PackageRequirement*)iterator->data;
		PackageData *pack = req->package;
		PackageData *dep = req->required;

		/* Check to see if the package system happened to file a requirement
		   for a package that was also failed... */
		if (g_list_find_custom (*failedpackages,
					pack,
					(GCompareFunc)eazel_install_package_compare)) {
			char *tmp;
			tmp = packagedata_get_readable_name (pack);
			trilobite_debug ("%s already failed, will not download it's requirements", tmp);
			g_free (tmp);
			packagedata_destroy (dep, TRUE);
			continue;
		}

		/* We use the unknown status later, to see if 
		   we should set it or not */
		dep->status = PACKAGE_UNKNOWN_STATUS;
		
		/* Emit the signal here, since then we won't have to make that
		   call in for every package system (when I say every, we know
		   I mean "both"...) */
		eazel_install_emit_dependency_check_pre_ei2 (service, pack, dep);
		packagedata_add_pack_to_soft_depends (pack, dep);

		fetch_result = eazel_install_fetch_package (service, dep);

		if (fetch_result) {
			if (dep->source_package) {
				dep->status = PACKAGE_SOURCE_NOT_SUPPORTED;
				fetch_result = FALSE;
			}
		}

		if (fetch_result) {
			/* If the package we just downloaded was already in packages,
			   but we just had a dependency conflict, we're in the funky case,
			   were pacakge foo is not installed. But the dependecy stuff has caused
			   it to be downloaded in version x.y, and later in v.w. This means that
			   the first conflict (causing x.y to be downloaded) now happens again.
			*/
			if (g_list_find_custom (*packages,
						dep->name,
						(GCompareFunc)eazel_install_package_name_compare)) {
				GList *pack_entry;

				pack_entry = g_list_find_custom (*packages,
								 dep,
								 (GCompareFunc)eazel_install_package_other_version_compare);
				trilobite_debug ("Circular dependency  %s-%s-%s at 0x%p", 
						 dep->name, dep->version, dep->minor, dep);

				if (pack_entry) {
					PackageData *evil_package = packagedata_copy ((PackageData*)(pack_entry->data), FALSE);
					packagedata_add_pack_to_breaks (dep, evil_package); 
					trilobite_debug ("Circular dependency caused by %s-%s-%s at 0x%p", 
							 evil_package->name,
							 evil_package->version,
							 evil_package->minor, 
							 evil_package);
					evil_package->status = PACKAGE_CIRCULAR_DEPENDENCY;
				} else {
					trilobite_debug ("This is Bad: I cannot set the funky break list");
				}
				dep->status = PACKAGE_CIRCULAR_DEPENDENCY;
				fetch_result = FALSE;
			}
		}

		if (fetch_result) {
			EazelInstallStatus inst_status;
			/* This sets the dep->modifies and checks for a funky case.
			   This case is sort of like the one above.
			   If the call returns 0, the following must have happened ;
			   We had a dependency saying that foo required bar-x.y.
			   So we download bar and now discover that bar is already installed
			   in the exact same version (x.y). So that means, another
			   dependency solving caused us to up/downgrade bar to another
			   version, and the following dep check then caused a 
			   conflict indicating that we should *not* do this up/downgrade.
			   
			   Solution:  fail the downgraded package 
			   
			   I assume the packages have the same name...
			*/
			inst_status = eazel_install_check_existing_packages (service, dep);
			if (inst_status == EAZEL_INSTALL_STATUS_QUO) {
				GList *pack_entry;
				
				trilobite_debug ("package %s required %s", pack->name, dep->name);
				trilobite_debug ("This is because some other package was downloaded");
				trilobite_debug ("and crushed this, since %s is already installed", dep->name);
				
				/* Use name compare here, as we expect the package to have the same name */
				pack_entry = g_list_find_custom (*packages,
								 dep->name,
								 (GCompareFunc)eazel_install_package_name_compare);
				if (pack_entry) {
					/* FIXME bugzilla.eazel.com
					   I suspect that adding this to adding evil_package to pack's breaks
					   might yield a more pleasant tree */
					PackageData *evil_package = (PackageData*)pack_entry->data;
					packagedata_add_pack_to_breaks (evil_package, dep);
					evil_package->status = PACKAGE_BREAKS_DEPENDENCY;
				} else {
					trilobite_debug ("This is also Bad: I cannot set the funky break list");
				}
				dep->status = PACKAGE_CIRCULAR_DEPENDENCY;
				fetch_result = FALSE;
			} else if (eazel_install_get_downgrade (service)==FALSE && 
				   inst_status == EAZEL_INSTALL_STATUS_DOWNGRADES) {
				/* Bad, we're downgrading but not allowed to downgrade */
				fetch_result = FALSE;
			} else if (eazel_install_get_update (service)==FALSE && 
				   inst_status == EAZEL_INSTALL_STATUS_UPGRADES) {
				/* Bad, we're upgrading but not allowed to upgrade */
				fetch_result = FALSE;
			}

		}
		
		if (fetch_result) {
			/* if it succeeds, add to a list of extras for this package 
			   We cannot just put it into extrapackages, as a later dep
			   might fail, and than we have to fail the package */
			GList *extralist;
			
			/* Check if a previous requirement download solved this
			   Note, we don't check till after download, since only a download
			   will reveal the packagename in case we need to download
			   using fetch_package using the pack->provides.
			   This is a paranoia check in addition to a check done
			   in do_dependency_check, since a dep check might say
			   that we require two files that are provided by the same
			   package, and we only want to get the package once.
			   Eg. nautilus requires libgconf-gtk-1.so and libgconf-1.so,
			   both provided by gconf, so we fetch gconf twice */
			if (g_list_find_custom (extras_in_this_batch,
						dep,
						(GCompareFunc)eazel_install_package_compare)) {
				trilobite_debug ("already handled %s", dep->name);
				packagedata_remove_soft_dep (dep, pack); 
				dep = NULL;
				continue;
			}
			
			/* This maintains a list of extra packages for 
			   a package. So when a requirement D for package A fails,
			   and we've already downloaded B & C for A, 
			   we can easily find B & D and remove them */
			extralist = g_hash_table_lookup (extras, pack->name);
			extralist = g_list_append (extralist, dep);
			g_hash_table_insert (extras, pack->name, extralist);
			
			/* This list contains all the packages added in this call
			   to fetch_rpm_dependencies. It's used in the initial check,
			   to avoid that multiple requests for a file results in 
			   multiple downloads */
			extras_in_this_batch = g_list_prepend (extras_in_this_batch, dep);
			
			pack->status = PACKAGE_PARTLY_RESOLVED;
		} else {
			/*
			  If it fails
			  1) remove it from the extras hashtable for the package, 
			  thereby ensuring the fetched packages before the fail aren't added
			  2) add the package to the list of stuff to remove (deleting it
			  immediately from packages will cause 
			  eazel_install_match_package_data_from_rpm_conflict
			  to return zero. This is fine if we then just do a continue, but
			  this way, we get all the missing deps into pack->soft_depends
			  3) add to list of failed packages
			*/
			GList *extralist;
			
			pack->status = PACKAGE_DEPENDENCY_FAIL;
			if (dep->status == PACKAGE_UNKNOWN_STATUS) {
				dep->status = PACKAGE_CANNOT_OPEN;
			}

			trilobite_debug ("Fetching %s failed, status %s/%s", 
					 packagedata_get_readable_name (dep),
					 packagedata_status_enum_to_str (dep->status),
					 packagedata_modstatus_enum_to_str (dep->modify_status));
					 			
			if (!eazel_install_get_force (service)) {
				/* Remove the extra packages for this package */
				extralist = g_hash_table_lookup (extras, pack->name);			
				/* Remove all the extras from the soft_deps (is this what we want ?) */
				g_list_foreach (extralist, (GFunc)packagedata_remove_soft_dep, pack); 
				g_list_free (extralist);
				g_hash_table_remove (extras, pack->name);
				
				/* Don't add it to failedpackages more than once */
				if (g_list_find (*failedpackages, pack) == NULL) {
					(*failedpackages) = g_list_prepend (*failedpackages, pack);
				}
				(*packages) = g_list_remove (*packages, pack);
				
				/* Don't process anymore */
				break;
			}
		}
	}
	
	/* iterate over all the lists in extras and add to extrapackages */
	g_hash_table_foreach (extras, (GHFunc)eazel_install_add_to_extras_foreach, extrapackages);
	g_hash_table_destroy (extras);
	g_list_free (extras_in_this_batch);

	if (*failedpackages) {
		return FALSE;
	} else {
		return TRUE;
	}
}

static void
dump_one_package (PackageData *pack, char *prefix)
{
	char *softprefix, *hardprefix, *modprefix, *breakprefix;
	char *packname;

	if (pack->name == NULL) {
		if (pack->provides && pack->provides->data) {
			packname = g_strdup_printf ("[provider of %s]", (char *)(pack->provides->data));
		} else {
			packname = g_strdup ("[mystery package]");
		}
	} else {
		packname = g_strdup_printf ("%s-%s-%s", pack->name, pack->version, pack->minor);
	}

	trilobite_debug ("%s%s (stat %s/%s), 0x%08X", 
			 prefix, packname,
			 packagedata_status_enum_to_str (pack->status),
			 packagedata_modstatus_enum_to_str (pack->modify_status),
			 (unsigned int)pack);
	g_free (packname);

	softprefix = g_strdup_printf ("%s (s) ", prefix);
	hardprefix = g_strdup_printf ("%s (h) ", prefix);
	breakprefix = g_strdup_printf ("%s (b) ", prefix);
	modprefix = g_strdup_printf ("%s (m) ", prefix);
	g_list_foreach (pack->soft_depends, (GFunc)dump_one_package, softprefix);
	g_list_foreach (pack->hard_depends, (GFunc)dump_one_package, hardprefix);
	g_list_foreach (pack->modifies, (GFunc)dump_one_package, modprefix);
	g_list_foreach (pack->breaks, (GFunc)dump_one_package, breakprefix);
	g_free (softprefix);
	g_free (hardprefix);
	g_free (modprefix);
	g_free (breakprefix);
}

static void
dump_packages_foreach (PackageData *pack, gpointer unused)
{
	if (pack->toplevel) {
		dump_one_package (pack, "");
	}
}

void
dump_packages (GList *packages)
{
	trilobite_debug ("#####  PACKAGE TREE  #####");
	g_list_foreach (packages, (GFunc)dump_packages_foreach, NULL);
	trilobite_debug ("-----  end  -----");
}

static void
print_package_list (char *str, GList *packages, gboolean show_deps)
{
	GList *iterator;
	PackageData *pack;
	char *tmp = NULL;
	char *dep = " depends on ";
	/*	char *breaks = " breaks ";*/

	trilobite_debug ("---------------------------");
	trilobite_debug ("%s", str);
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		pack = (PackageData*)iterator->data;
		if (show_deps) {
			GList *it2;
			tmp = g_strdup (dep);
			it2 = pack->soft_depends;
			while (it2) { 
				char *tmp2;
				tmp2 = g_strdup_printf ("%s%s ", tmp ,
							((PackageData*)it2->data)->name);
				g_free (tmp);
				tmp = tmp2;
				it2 = g_list_next (it2);
			}
/*
			tmp = g_strdup (breaks);
			it2 = pack->breaks;
			while (it2) { 
				char *tmp2;
				PackageData *p2;
				p2 = (PackageData*)it2->data;
				tmp2 = g_strdup_printf ("%s%s(%db%dd) ", tmp , rpmfilename_from_packagedata (p2), 
							g_list_length (p2->breaks),
							g_list_length (p2->soft_depends));
				g_free (tmp);
				tmp = tmp2;
				it2 = g_list_next (it2);
			}
*/
		}
		trilobite_debug ("* %s (%s) %s", 
			   pack->name, 
			   pack->toplevel ? "true" : "", 
			   (tmp && strlen (tmp) > strlen (dep)) ? tmp : "");
		g_free (tmp);
		tmp = NULL;
	}
}

/*
  Helperfunction to create PackageRequirements for fileconflicts. Should be
  used for packagesystems that don't do this (eg. RPM) */
static void
eazel_install_do_file_conflict_check (EazelInstall *service,
				      GList **packages,
				      GList **failedpackages,
				      GList **requirements)
{
	GList *iterator;
	GList *tmp_failed = NULL;

	if (eazel_install_get_ignore_file_conflicts (service) ||
	    eazel_install_get_force (service)) {
		trilobite_debug ("not performing file conflict check");
	}

	/* Now do file conflicts on all packages */
	for (iterator = *packages; iterator; glist_step (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		GList *required = NULL;

		/* If we haven't tested conflicts yet */
		if (pack->conflicts_checked == FALSE) {
			GList *breaks = NULL;
			pack->conflicts_checked = TRUE;			
			if (eazel_install_check_for_file_conflicts (service, pack, &breaks, &required)) {
				if (required) {
					/* Create PackageRequirements for all the requirements */
					GList *reqiterator;
					for (reqiterator = required;reqiterator;glist_step (reqiterator)) {
						PackageData *required_pack = (PackageData*)reqiterator->data;
						if (g_list_find_custom (*packages, 
									required_pack->name,
									(GCompareFunc)eazel_install_package_name_compare)) {
							trilobite_debug ("but we're updating it (requirement)");
							/* packagedata_destroy (broken_package, FALSE); */
						} else {
							PackageRequirement *req;
							req = packagerequirement_new (pack, required_pack);
							(*requirements) = g_list_prepend (*requirements, req);
						}
					}
				}
				if (breaks) {
					GList *break_iterator;
					gboolean fail_it = FALSE;
					for (break_iterator = breaks; break_iterator; glist_step (break_iterator)) {
						PackageData *broken_package = (PackageData*)break_iterator->data;
						trilobite_debug ("breaking %s", broken_package->name);
						if (g_list_find_custom (*packages, 
									broken_package->name,
									(GCompareFunc)eazel_install_package_name_compare)) {
							trilobite_debug ("but we're updating it");
							/* packagedata_destroy (broken_package, FALSE); */
						} else {
							fail_it = TRUE;
							packagedata_add_pack_to_breaks (pack, broken_package);
						}
					}
					if (fail_it) {
						tmp_failed = g_list_prepend (tmp_failed, pack);
					}
				}
			} else {
				/* No file conflicts */
			}
		}
	}
	
	/* Now clean up */
	for (iterator = tmp_failed; iterator; glist_step (iterator)) {
		PackageData *cpack = (PackageData*)(iterator->data);
		(*failedpackages) = g_list_prepend (*failedpackages, cpack);
		(*packages) = g_list_remove (*packages, cpack);
	}
}

/* 
   Use package system to do the dependency check
 */
static void
eazel_install_do_dependency_check (EazelInstall *service,
				   GList **packages,
				   GList **failedpackages,
				   GList **requirements)
{
	eazel_install_do_rpm_dependency_check (service, 
					       packages,
					       failedpackages,
					       requirements);
	/* RPM's depCheck doens't do fileconflicts, so we do
		   them ourselves */
	eazel_install_do_file_conflict_check (service, 
					      packages,
					      failedpackages,
					      requirements);
}

/*
  Given a glist of PackageData's, ensure_deps_are_fetched checks deps
  for them all, if deps fail, fetch the depency and add to packages.
  Returns FALSE if outfiles was set, TRUE is all dependencies were satisfied.
  If a dep could not be found, that name is added to fails)
 */
static gboolean
eazel_install_ensure_deps (EazelInstall *service, 
			   GList **packages, 
			   GList **failedpackages)
{
	GList *extrapackages = NULL; /* This list contains packages that were added to "packages" */
	gboolean result;             
	GList *requirements = NULL;  /* This list contains the PackageRequirements for the dependecy failures */

	g_return_val_if_fail (packages != NULL, TRUE);
	g_return_val_if_fail (*packages != NULL, TRUE);

	g_return_val_if_fail (g_list_length (*packages)>=1, FALSE);
	result = TRUE;
	
	trilobite_debug ("Into eazel_install_ensure_deps");

	/* First we load headers and prepare them.
	   The datastructures depend on the packagesystem,
	   and are places in service->private->packsys.
	*/
		
	eazel_install_do_dependency_check (service, 
					   packages, 
					   failedpackages,
					   &requirements);		
	
	if (requirements != NULL) {
		GList *iterator;
		
		extrapackages = NULL;
		
		/* For all the packages, set state to partly_resolved. */
		for (iterator=*packages; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack;
			pack = (PackageData*)iterator->data;
			pack->status = PACKAGE_PARTLY_RESOLVED;
		}
		
		trilobite_debug ("%d dependency failure(s)", g_list_length (requirements));
		
		/* Fetch the needed stuff. 
		   "extrapackages" gets the new packages added,
		   packages in "failedpackages" are packages moved from 
		   "packages" that could not be resolved. */
		eazel_install_fetch_dependencies (service, 
						  packages,
						  &extrapackages,
						  failedpackages,
						  requirements);

		/* Delete the PackageRequirements.
		   Note, that we just need to free the structure, both elements
		   are kept, req->package in extrapackages or failedpackages and
		   req->required will be in req->package's breaks/soft_depends.
		*/
		g_list_foreach (requirements, (GFunc)g_free, NULL);
		g_list_free (requirements);
		
		/* Some debug printing */
		dump_packages (*packages);
		print_package_list ("Packages that were fetched", extrapackages, FALSE);
		print_package_list ("Packages that failed", *failedpackages, TRUE);
	} else {
		GList *iterator;
		
		/* Deps are fine, set all packages to resolved */
		for (iterator=*packages; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack;
			pack = (PackageData*)iterator->data;
			pack->status = PACKAGE_RESOLVED;
		}
		trilobite_debug (_("Dependencies appear ok"));

		if (!eazel_install_do_transaction_all_files_check (service, packages)) {
			trilobite_debug (_("But there are file conflicts"));
			/* Now recurse into eazel_install_ensure_deps with
			   the new "packages" list */
			eazel_install_ensure_deps (service, packages, failedpackages);
		} else {
			trilobite_debug ("Dependencies still appear ok");
		}

	}

	/* If there was failedpackages, prune them from the tree 
	   and the "extrapackages".
	   We need to strip from "extrapackages" as well, since :
	   while installing A & B, C was added for A, D was
	   added for B but B also needs E (but not found). Therefore
	   we strip D from "extrapackages" and B is stripped
	   from "packages". Keeping D around would
	   install a non-needed package
	*/
	if (*failedpackages) {
		GList *iterator;

		for (iterator = *failedpackages; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack;
			pack = (PackageData*)iterator->data;
			trilobite_debug ("calling prune on %s", pack->name);
			eazel_install_prune_packages (service, pack, packages, 
							      &extrapackages, NULL);
		}			
	} 
	/* If there were conflicts, we'll have called fetch_dependencies and might
	   have downloaded extra packages. So we have to recurse and process these */
	if (extrapackages) {
		GList *iterator;
		
		/* Add to "packages" */
		for (iterator = extrapackages; iterator; iterator = g_list_next (iterator)) {
			(*packages) = g_list_append (*packages, iterator->data);
		}
		g_list_free (extrapackages);
		
		/* Now recurse into eazel_install_ensure_deps with
		   the new "packages" list */
		eazel_install_ensure_deps (service, packages, failedpackages);
		
		/* Now remove the packages that failed from "packages" 
		   and copy them into "failedpackages".  */
		for (iterator = *failedpackages; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack;
			pack = (PackageData*)iterator->data;
			(*packages) = g_list_remove (*packages, pack);					
		}
	}
	dump_packages (*packages);
		      
	return result;
}

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
		GList *break_iterator = NULL;
		
		trilobite_debug ("checking reqs by %s", rpmname_from_packagedata (pack));
		matches = eazel_package_system_query (service->private->package_system,
						      service->private->cur_root,
						      pack,
						      EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES,
						      PACKAGE_FILL_NO_DIRS_IN_PROVIDES);
		packagedata_list_prune (&matches, *packages, TRUE, TRUE);
		
		for (match_iterator = matches; match_iterator; match_iterator = g_list_next (match_iterator)) {
			PackageData *requiredby = (PackageData*)match_iterator->data;;
			
			requiredby->status = PACKAGE_DEPENDENCY_FAIL;
			pack->status = PACKAGE_BREAKS_DEPENDENCY;
			trilobite_debug ("logic.c: %s requires %s", requiredby->name, pack->name);

			/* If we're already marked it as breaking, go on 
			if (g_list_find_custom (*breaks, (gpointer)requiredby->name, 
						(GCompareFunc)eazel_install_package_name_compare)) {
				trilobite_debug ("skip %s", requiredby->name);
				packagedata_destroy (requiredby, TRUE);
				requiredby = NULL;
				continue;
			}
			*/

			/* Guess not, mark it as breaking (and that pack is the offender */
			packagedata_add_pack_to_breaks (pack, requiredby);
			(*breaks) = g_list_prepend ((*breaks), requiredby);

			/* If the package has not been failed yet (and is a toplevel),
			   fail it */
			if (!g_list_find_custom (*failed, (gpointer)pack->name, 
						 (GCompareFunc)eazel_install_package_name_compare) &&
			    pack->toplevel) {
				(*failed) = g_list_prepend (*failed, pack);
			}
		}
		/* fre the list structure from _simple_query */
		g_list_free (matches);
		
		if (*breaks) {
			eazel_uninstall_upward_traverse (service, breaks, failed, &tmp_breaks);
		}
		
		for (break_iterator = tmp_breaks; break_iterator; break_iterator = g_list_next (break_iterator)) {
			(*breaks) = g_list_prepend ((*breaks), break_iterator->data);
		}
	}
	
	for (iterator = *failed; iterator; iterator = g_list_next (iterator)) {
		(*packages) = g_list_remove (*packages, iterator->data);
	}

	trilobite_debug ("out eazel_uninstall_upward_traverse");
}

/* This traverses downwards on all requirements in "packages", 
   checks that their uninstall do _not_ break anything, and
   adds thm to requires */

static void
eazel_uninstall_downward_traverse (EazelInstall *service,
				   GList **packages,
				   GList **failed,
				   GList **requires)
{
	GList *iterator;
	GList *tmp_requires = NULL;

	/* 
	   create set
	   find all requirements from "packages"
	   add all packs + requirements from "packages" to set
	   dep check
	   for all breaks, remove from requirements
	   recurse calling eazel_uninstall_downward_traverse (requirements, &tmp)
	   add all from tmp to requirements
	*/
	trilobite_debug ("in eazel_uninstall_downward_traverse");
	
	/* First iterate across the packages in "packages" */
	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {
		GList *matches = NULL;
		PackageData *pack;
		GList *match_iterator;

		pack = (PackageData*)iterator->data;

		matches = eazel_package_system_query (service->private->package_system,
						      service->private->cur_root,
						      pack->name,
						      EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
						      PACKAGE_FILL_NO_DIRS_IN_PROVIDES);
		trilobite_debug ("%s had %d hits", pack->name, g_list_length (matches));

		/* Now iterate over all packages that match pack->name */
		for (match_iterator = matches; match_iterator; match_iterator = g_list_next (match_iterator)) {
			PackageData *matched_pack;
			const char **require_name;
			int require_name_count;
			Header hd;
			int type;
			int j;

			matched_pack = (PackageData*)match_iterator->data;
			hd = ((Header) matched_pack->packsys_struc);

			if (!headerGetEntry(hd, RPMTAG_REQUIRENAME, &type, (void **) &require_name,
					    &require_name_count)) {
				require_name_count = 0;
			}
			
			trilobite_debug ("requirename count = %d", require_name_count);
			
			/* No iterate over all packages required by the current package */
			for (j = 0; j < require_name_count; j++) {
				if ((*require_name[j] != '/') &&
				    !strstr (require_name[j], ".so")) {
					PackageData *tmp_pack = packagedata_new ();
					GList *second_matches;
					GList *second_match_iterator;

					tmp_pack->name = g_strdup (require_name[j]);
					second_matches = 
						eazel_package_system_query (service->private->package_system,
									    service->private->cur_root,
									    tmp_pack,
									    EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES,
									    PACKAGE_FILL_NO_DIRS_IN_PROVIDES);
					packagedata_list_prune (&second_matches, *packages, TRUE, TRUE);
					packagedata_list_prune (&second_matches, *requires, TRUE, TRUE);
					packagedata_destroy (tmp_pack, TRUE);

					/* Iterate over all packages that match the required package */
					for (second_match_iterator = second_matches;
					     second_match_iterator; 
					     second_match_iterator = g_list_next (second_match_iterator)) {
						PackageData *isrequired;

						isrequired = (PackageData*)second_match_iterator->data;
						if (g_list_find_custom (*requires, isrequired->name,
									(GCompareFunc)eazel_install_package_name_compare) ||
						    g_list_find_custom (*packages, isrequired->name,
									(GCompareFunc)eazel_install_package_name_compare)) {
							trilobite_debug ("skipped %s", isrequired->name);
							packagedata_destroy (isrequired, TRUE);\
							isrequired = NULL;
							continue;
						}		
						trilobite_debug ("** %s requires %s", pack->name, isrequired->name);

						{
							GList *third_matches;

							/* Search for packages
							   requiring the requirement, excluding
							   all pacakges from requires and packages */
							third_matches = 
								eazel_package_system_query (service->private->package_system,
											    service->private->cur_root,
											    pack->name,
											    EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES,
											    PACKAGE_FILL_NO_DIRS_IN_PROVIDES);
							packagedata_list_prune (&third_matches, 
										*packages, TRUE, TRUE);
							packagedata_list_prune (&third_matches, 
										*requires, TRUE, TRUE);
							packagedata_list_prune (&third_matches, 
										tmp_requires, TRUE, TRUE);
							
							if (third_matches) {
								trilobite_debug ("skipped %s, others depend on it", 
									   isrequired->name);
								print_package_list ("BY", third_matches, FALSE);
								g_list_foreach (third_matches, 
										(GFunc)packagedata_destroy, 
										GINT_TO_POINTER (TRUE));
								g_list_free (third_matches);
								third_matches = NULL;
								packagedata_destroy (isrequired, TRUE);
								isrequired = NULL;
							} else {
								trilobite_debug ("Also nuking %s", isrequired->name);
								tmp_requires = g_list_prepend (tmp_requires, 
											       isrequired);
							}
						}
					}
					g_list_free (second_matches);
				} else {
					trilobite_debug ("must lookup %s", require_name[j]);
					/* FIXME bugzilla.eazel.com 1541:
					   lookup package "p" that provides requires[j],
					   if packages that that require "p" are not in "packages"
					   don't add it, otherwise add to requires */
				}
			}

			headerFree (hd);
		}
		g_list_foreach (matches, (GFunc)packagedata_destroy, GINT_TO_POINTER (TRUE));
		g_list_free (matches);
		matches = NULL;
	}

	if (tmp_requires) {
		eazel_uninstall_downward_traverse (service, &tmp_requires, failed, requires);
	}

	/* Now move the entries in tmp_requires into *requires */
	for (iterator = tmp_requires; iterator; iterator = g_list_next (iterator)) {
		(*requires) = g_list_prepend (*requires, iterator->data);
	}
	g_list_free (tmp_requires);

	trilobite_debug ("out eazel_uninstall_downward_traverse");
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
		GList *matches;

		matches = eazel_package_system_query (service->private->package_system,
						      service->private->cur_root,
						      pack->name,
						      EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
						      PACKAGE_FILL_NO_TEXT | PACKAGE_FILL_NO_DEPENDENCIES | PACKAGE_FILL_NO_DIRS_IN_PROVIDES);
		/* If it's installed, continue */
		if (matches) {
			GList *match_it;
			gboolean any = FALSE;
			for (match_it = matches; match_it; match_it = g_list_next (match_it)) {
				PackageData *matched = (PackageData*)match_it->data;
				if (eazel_install_package_matches_versioning (matched, 
									      pack->version, 
									      pack->minor,
									      EAZEL_SOFTCAT_SENSE_EQ)) {
					matched->toplevel = TRUE;
					/* mark that at least one matched */
					any = TRUE;
					result = g_list_prepend (result, matched);
				} else {
					packagedata_destroy (matched, TRUE);
				}
				
 			} 
			if (!any) {
				pack->status = PACKAGE_CANNOT_OPEN;
				remove = g_list_prepend (remove, pack);
			}
			g_list_free (matches);
			continue;
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

	g_list_foreach (*packages, (GFunc)packagedata_destroy, FALSE);
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
		trilobite_debug ("not installed %s", ((PackageData*)iterator->data)->name);
		eazel_install_emit_uninstall_failed (service, (PackageData*)iterator->data);
	}
		
	/* If there are still packages and we're not forcing,
	   do upwards traversel */
	if (*packages && !eazel_install_get_force (service)) {
		eazel_uninstall_upward_traverse (service, packages, failed, &tmp);
		print_package_list ("FAILED", *failed, TRUE);
		for (iterator = *failed; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack = (PackageData*)iterator->data;
			trilobite_debug ("failed %s", pack->name);
			dump_one_package (pack, "");
			eazel_install_emit_uninstall_failed (service, pack);
		}
		g_list_free (tmp);
	}

/*
  I've disabled downwards traverse untill it's done.

	tmp = NULL;
	eazel_uninstall_downward_traverse (service, packages, failed, &tmp);
	for (iterator = tmp; iterator; iterator = g_list_next (iterator)) {
		g_message ("also doing %s", ((PackageData*)iterator->data)->name);
		(*packages) = g_list_prepend (*packages, iterator->data);
	}
	g_list_free (tmp);
*/

	trilobite_debug ("out eazel_uninstall_glob");
}
