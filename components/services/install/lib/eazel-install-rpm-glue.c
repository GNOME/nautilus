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

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-services-config.xml
 * file and install a services generated package-list.xml.
 */

#include "eazel-install-rpm-glue.h"
#include "eazel-install-xml-package-list.h"
#include "eazel-install-md5.h"
#include "eazel-install-public.h"
#include "eazel-install-private.h"

#include "eazel-install-query.h"

#ifndef EAZEL_INSTALL_NO_CORBA
#include <libtrilobite/libtrilobite.h>
#else
#include <libtrilobite/trilobite-root-helper.h>
#endif

#include <libtrilobite/trilobite-core-utils.h>
#include <libtrilobite/libtrilobite-service.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmmacro.h>
#include <rpm/dbindex.h>
#include <rpm/misc.h>
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

#define PERCENTS_PER_RPM_HASH 2

typedef void (*rpm_install_cb)(char* name, char* group, void* user_data);

static gboolean eazel_install_do_install_packages (EazelInstall *service,
						   GList* packages);

typedef void* (*RpmCallbackFunc) (const Header, const rpmCallbackType, 
				  const unsigned long, const unsigned long, 
				  const void*, void*);

static int eazel_install_start_transaction (EazelInstall *service, 
					    GList* packages);

static gboolean eazel_install_ensure_deps (EazelInstall *service, 
					   GList **filenames, 
					   GList **fails);

int eazel_install_package_name_compare (PackageData *pack, 
					char *name);

int eazel_install_package_compare (PackageData *pack, 
				   PackageData *other);

int eazel_install_requirement_dep_compare (PackageRequirement *req,
					   PackageData *pack);

int eazel_install_package_version_compare (PackageData *pack, 
					   char *version);

static void eazel_uninstall_globber (EazelInstall *service,
				     GList **packages,
				     GList **failed);

static int eazel_install_check_existing_packages (EazelInstall *service, 
						  PackageData *pack);

static void eazel_install_prune_packages (EazelInstall *service, 
					  PackageData *pack, 
					  ...);

static gboolean eazel_install_download_packages (EazelInstall *service,
						 gboolean toplevel,
						 GList **packages,
						 GList **failed_packages);

static gboolean  eazel_install_check_for_file_conflicts (EazelInstall *service,
							 PackageData *pack,
							 GList **requires);

static gboolean eazel_install_check_if_related_package (EazelInstall *service,
							PackageData *package,
							PackageData *dep);

/*
  Iterate across the categories and assemble one long
  list with all the toplevel packages in the categories
 */
static GList *
eazel_install_flatten_categories (EazelInstall *service,
				  GList *categories)
{
	GList* packages = NULL;
	GList* category_iterator;
	
	for (category_iterator = categories; category_iterator; category_iterator = g_list_next (category_iterator)) {
		CategoryData *cat = (CategoryData*)category_iterator->data;
		if (packages) {
			packages = g_list_concat (packages, cat->packages);
		} else {
			packages = g_list_copy (cat->packages);
		}
	}


	return packages;
}

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
		int inst_status;
		gboolean skip = FALSE;
		
		inst_status = eazel_install_check_existing_packages (service, pack);
		trilobite_debug ("%s: install status = %d", pack->name, inst_status);

		/* If in force mode, install it under all circumstances.
		   if not, only install if not already installed in same
		   version or up/downgrade is set */
		if (eazel_install_get_force (service) ||
		    (eazel_install_get_downgrade (service) && inst_status == -1) ||
		    (eazel_install_get_update (service) && inst_status == 1) ||
		    inst_status == 2) {
			skip = FALSE;
		} else {
			skip = TRUE;
		}
		
		if (skip) {
			trilobite_debug ("Skipping %s...", pack->name);
			/* Nuke the modifies list again, since we don't want to see them */
			g_list_foreach (pack->modifies, 
					(GFunc)packagedata_destroy, 
					GINT_TO_POINTER (TRUE));
			g_list_free (pack->modifies);
			pack->modifies = NULL;

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
install_new_packages (EazelInstall *service, GList *categories) {
	EazelInstallStatus result;
	int install_flags, interface_flags, problem_filters;
	
	install_flags = 0;
	interface_flags = 0;
	problem_filters = 0;
	
	if (eazel_install_get_test (service)) {
		trilobite_debug (_("Dry Run Mode Activated.  Packages will not actually be installed ..."));
		install_flags |= RPMTRANS_FLAG_TEST;
	}

	if (eazel_install_get_update (service)) {
		interface_flags |= INSTALL_UPGRADE;
	}

	if (eazel_install_get_verbose (service)) {
		rpmSetVerbosity (RPMMESS_VERBOSE);
	} else {
		rpmSetVerbosity (RPMMESS_NORMAL);
	}

	if (eazel_install_get_force (service)) {
		problem_filters |= RPMPROB_FILTER_REPLACEPKG |
			RPMPROB_FILTER_REPLACEOLDFILES |
			RPMPROB_FILTER_REPLACENEWFILES |
			RPMPROB_FILTER_OLDPACKAGE;
	}

	eazel_install_set_install_flags (service, install_flags);
	eazel_install_set_interface_flags (service, interface_flags);
	eazel_install_set_problem_filters (service, problem_filters);
	
	if (categories == NULL) {
		trilobite_debug (_("Reading the install package list %s"), eazel_install_get_package_list (service));
		categories = parse_local_xml_package_list (eazel_install_get_package_list (service), NULL, NULL);
	}

	result = EAZEL_INSTALL_NOTHING;
	if (categories != NULL) {
		/* First, collect all packages in one list */
		GList *packages = eazel_install_flatten_categories (service, categories);

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

		trilobite_debug ("init for %s (%s)", package->name, package->version ? package->version : "NO VERSION");
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
				packagedata_fill_from_file (package, package->filename);
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
			int inst_status = eazel_install_check_existing_packages (service, package);
			if (inst_status == -1 && eazel_install_get_downgrade (service)) {
				trilobite_debug (_("Will download %s"), package->name);
				/* must download... */
			} else if (inst_status <= 0) {
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
			/* FIXME: bugzilla.eazel.com 3413
			   Ugh, this isn't very nice. Not that there's a chance
			   that any package will have the name "id%3D", but... */
			if (strncmp (package->name, "id%3D", 5) == 0) {
				/* nautilus encodes "id=" to "id%3D" */
				result = eazel_install_fetch_package_by_id (service, package->name + 5, package);
			} else {
				result = eazel_install_fetch_package (service, package);
			}

			if (!result) {
				remove_list = g_list_prepend (remove_list, package);
			} else {
				package->toplevel = toplevel;
				/* If downloaded package has soft_deps,
				   fetch them by a recursive call */
				if (package->soft_depends) {
					result = eazel_install_download_packages (service,
										  FALSE,
										  &package->soft_depends,
										  NULL);
				}
			}
		}
	}

	for (iterator = remove_list; iterator; iterator = g_list_next (iterator)) {
		(*packages) = g_list_remove (*packages, iterator->data);
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
		   dirs, so don't check those */
		if (g_file_test (filename, G_FILE_TEST_ISDIR)) {
			continue;
		}

		owners = eazel_install_simple_query (service,
						     filename,
						     EI_SIMPLE_QUERY_OWNS,
						     1,
						     pack->modifies);
		if (g_list_length (owners) > 1) {
			/* FIXME: bugzilla.eazel.com 2959
			   More then one packages owns this file,
			   this cannot happen (or should not at least)
			*/
			g_assert_not_reached ();
		} else if (g_list_length (owners) == 1) {
			PackageData *owner = (PackageData*)owners->data;
			
			/* If the package owner is already in the breaks list for the package,
			   or in the *requires, continue  */
			if (g_list_find_custom (pack->breaks, owner->name, 
						(GCompareFunc)eazel_install_package_name_compare) ||
			    g_list_find_custom (*requires, owner->name, 
						(GCompareFunc)eazel_install_package_name_compare)) {
				trilobite_debug ("already breaking %s", owner->name);
				packagedata_destroy (owner, FALSE);
				owner = NULL;
				continue;
			}

			/* FIXME: bugzilla.eazel.com 2986
			   Ideally, this should be done by checking that owner
			   does not appear in pack->modifes, so we
			   can use the Obsoltes thingy in rpm */
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
					pack->breaks = g_list_prepend (pack->breaks, owner);
				}
				
			} /* else it's the same package and it's okay */
		} 
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
		eazel_install_free_package_system (service);
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
		eazel_install_free_package_system (service);

		if (eazel_install_start_transaction (service, cat->packages) != 0) {
			result = EAZEL_INSTALL_NOTHING;
		}

		categories = g_list_next (categories);
	}
	return result;
}

EazelInstallStatus
uninstall_packages (EazelInstall *service,
		    GList* categories) 
{
	EazelInstallStatus result = EAZEL_INSTALL_NOTHING;
	int uninstall_flags, interface_flags, problem_filters;
	
	uninstall_flags = 0;
	interface_flags = 0;
	problem_filters = 0;
	
	if (eazel_install_get_test (service)) {
		trilobite_debug (_("Dry Run Mode Activated.  Packages will not actually be installed ..."));
		uninstall_flags |= RPMTRANS_FLAG_TEST;
	}

	if (eazel_install_get_verbose (service)) {
		rpmSetVerbosity (RPMMESS_VERBOSE);
	}
	else {
		rpmSetVerbosity (RPMMESS_NORMAL);
	}
	rpmSetVerbosity (RPMMESS_DEBUG);

	rpmReadConfigFiles (eazel_install_get_rpmrc_file (service), NULL);

	eazel_install_set_install_flags (service, uninstall_flags);
	eazel_install_set_interface_flags (service, interface_flags);
	eazel_install_set_problem_filters (service, problem_filters);

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
revert_transaction (EazelInstall *service, 
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
		result |= uninstall_packages (service, categories);
	}
	if (inst) {
		eazel_install_set_uninstall (service, FALSE);
		eazel_install_set_downgrade (service, FALSE);
		eazel_install_set_update (service, FALSE);
		cat->packages = inst;
		result |= install_new_packages (service, categories);
	}
	if (downgrade) {
		eazel_install_set_uninstall (service, FALSE);
		eazel_install_set_downgrade (service, TRUE);
		eazel_install_set_update (service, FALSE);
		cat->packages = downgrade;
		result |= install_new_packages (service, categories);
	}
	if (upgrade) {
		eazel_install_set_uninstall (service, FALSE);
		eazel_install_set_downgrade (service, TRUE);
		eazel_install_set_update (service, TRUE);
		cat->packages = upgrade;
		result |= install_new_packages (service, categories);
		g_list_foreach (upgrade, (GFunc)packagedata_destroy, GINT_TO_POINTER (TRUE));
	}

	return result;
}

static void
eazel_install_do_transaction_fill_hash (EazelInstall *service,
					GList *packages)
{
	GList *iterator;
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		char *tmp;		
		PackageData *pack;

		pack = (PackageData*)iterator->data;
		tmp = g_strdup_printf ("%s", pack->name);
		g_hash_table_insert (service->private->name_to_package_hash,
				     tmp,
				     iterator->data);
	}
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
eazel_install_do_transaction_add_to_transaction (EazelInstall *service,
						 PackageData *pack)
{
	service->private->transaction = g_list_prepend (service->private->transaction,
							pack);
}

static void
eazel_install_start_transaction_make_rpm_argument_list (EazelInstall *service,
							GList **args)
{
	if (eazel_install_get_test (service)) {
		(*args) = g_list_prepend (*args, g_strdup ("--test"));
	} 
	if (eazel_install_get_force (service)) {
		g_warning ("Force install mode!");
		(*args) = g_list_prepend (*args, g_strdup ("--force"));
		(*args) = g_list_prepend (*args, g_strdup ("--nodeps"));
	}
	if (eazel_install_get_downgrade (service)) {
		(*args) = g_list_prepend (*args, g_strdup ("--oldpackage"));
	}
	if (eazel_install_get_uninstall (service)) {
		(*args) = g_list_prepend (*args, g_strdup ("-e"));
	} else if (eazel_install_get_update (service) || eazel_install_get_downgrade (service)) {
		(*args) = g_list_prepend (*args, g_strdup ("-Uvh"));
	} else {
		(*args) = g_list_prepend (*args, g_strdup ("-ivh"));
	}
}

static GList*
eazel_install_start_transaction_make_argument_list (EazelInstall *service,
						    GList *packages)
{
	GList *args;
	GList *iterator;

	args = NULL;

	/* Add the packages to the arg list */
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack;

		pack = (PackageData*)iterator->data;
		/* If we're uninstalling, we need the package name, 
		   if installing/upgrading, the filename.
		   Since the rpmname is not part of the packagedata,
		   we need to g_strdup both into the args list.
		*/
		if (eazel_install_get_uninstall (service)) {
			pack->modify_status = PACKAGE_MOD_UNINSTALLED;
			args = g_list_prepend (args, g_strdup (rpmname_from_packagedata (pack)));
		} else {
			pack->modify_status = PACKAGE_MOD_INSTALLED;
			args = g_list_prepend (args, g_strdup (pack->filename));
		}
		/* NOTE: rpm does not generate hash/percent output for
		   - uninstall
		   thus, I add them to the transaction report here */
		if (pack->toplevel &&
		    eazel_install_get_uninstall (service)) {
			eazel_install_do_transaction_add_to_transaction (service, pack);
		}
	}
	
	/* Set the RPM parameters */
	switch (eazel_install_get_package_system (service)) {
	case EAZEL_INSTALL_USE_RPM:
		eazel_install_start_transaction_make_rpm_argument_list (service, &args);
		break;
	}

	return args;
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
	xmlAddChild (root, eazel_install_packagelist_to_xml (service->private->transaction));
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

static gboolean
eazel_install_monitor_rpm_propcess_pipe (GIOChannel *source,
					 GIOCondition condition,
					 EazelInstall *service)
{
	char         tmp;
	static       int package_name_length = 256;
	static       char package_name [256];
	ssize_t      bytes_read;
	static       PackageData *pack = NULL;
	static       int pct;
	
	g_io_channel_read (source, &tmp, 1, &bytes_read);
	
/* 1.39 has the code to parse --percent output */
	if (bytes_read) {
		/* fprintf (stdout, "%c", tmp); fflush (stdout);  */
		/* Percentage output, parse and emit... */
		if (tmp=='#') {
			int amount;
			if (pack == NULL) {
				return TRUE;
			}
			pct += PERCENTS_PER_RPM_HASH;
			if (pct == 100) {
				amount = pack->bytesize;
			} else {
				amount =  (pack->bytesize / 100) * pct;
			}
			if (pack && amount) {
				eazel_install_emit_install_progress (service, 
								     pack, 
								     service->private->packsys.rpm.packages_installed, 
								     service->private->packsys.rpm.num_packages,
								     amount, 
								     pack->bytesize,
								     service->private->packsys.rpm.current_installed_size + amount,
								     service->private->packsys.rpm.total_size);
			}
					/* By invalidating the pointer here, we
					   only emit with amount==total once */
			if (pct==100) {
				service->private->packsys.rpm.current_installed_size += pack->bytesize;
					/* If a toplevel pacakge completed, add to transction list */
				if (pack->toplevel) {
					eazel_install_do_transaction_add_to_transaction (service, pack);
				}
				pack = NULL;
				pct = 0;
				package_name [0] = 0;
			}
		}  else  if (tmp != ' ') {
			/* Read untill we hit a space */
			package_name[0] = 0;
			while (bytes_read && tmp != ' ') {
				if (strlen (package_name) < package_name_length) {
					/* Add char to package */
					int x;
					x = strlen (package_name);
					if (!isspace (tmp)) {
						package_name [x] = tmp;
						package_name [x+1] = 0;
					}
				}
					/* read next byte */
				g_io_channel_read (source, &tmp, 1, &bytes_read);
				if (tmp=='\n') {
					package_name[0] = '\0';
					break;
				}
			}
			
			/* Not percantage mark, that means filename, step ahead one file */
			pack = g_hash_table_lookup (service->private->name_to_package_hash, package_name);
			if (pack==NULL) {
				/* might be "warning:" */
				if (strcmp (package_name, "warning:") == 0) {
					package_name[0] = 0;
					while (tmp != '\n') {
						g_io_channel_read (source, &tmp, 1, &bytes_read);
					}
					trilobite_debug ("warning received");
				} else {
					trilobite_debug ("lookup \"%s\" failed", package_name);
				}
			} else {
				trilobite_debug ("matched \"%s\"", package_name);
				pct = 0;
				service->private->packsys.rpm.packages_installed ++;
				eazel_install_emit_install_progress (service, 
								     pack, 
								     service->private->packsys.rpm.packages_installed, 
								     service->private->packsys.rpm.num_packages,
								     0, 
								     pack->bytesize,
								     service->private->packsys.rpm.current_installed_size,
								     service->private->packsys.rpm.total_size);
			}
		}
	} 
	if (bytes_read == 0) {
		eazel_install_do_transaction_save_report (service);
		return FALSE;
	} else {
		return TRUE;
	}
}

static gboolean
eazel_install_monitor_process_pipe (GIOChannel *source,
				    GIOCondition condition,
				    EazelInstall *service)
{
	if (condition == (G_IO_ERR | G_IO_NVAL | G_IO_HUP)) {
		service->private->subcommand_running = FALSE;
	} else {
		switch (eazel_install_get_package_system (service)) {
		case EAZEL_INSTALL_USE_RPM:
			service->private->subcommand_running = eazel_install_monitor_rpm_propcess_pipe (source, 
													condition, 
													service);
			break;
		}
	}
	return service->private->subcommand_running;
}

static void
eazel_install_display_arguments (GList *args) 
{
	GList *iterator;
	fprintf (stdout, "\nARGS: ");
	for (iterator = args; iterator; iterator = g_list_next (iterator)) {
		fprintf (stdout, "%s ", (char*)iterator->data);
	}
	fprintf (stdout, "\n");
}

/* Monitors the subcommand pipe and returns the number of packages installed */
static gint
eazel_install_monitor_subcommand_pipe (EazelInstall *service,
				       int fd, 
				       GIOFunc monitor_func)
{
	int result = 0;
	GIOChannel *channel;

	service->private->subcommand_running = TRUE;
	channel = g_io_channel_unix_new (fd);

	trilobite_debug ("beginning monitor on %d", fd);

	g_io_add_watch (channel, G_IO_IN | G_IO_ERR | G_IO_NVAL | G_IO_HUP, 
			monitor_func, 
			service);
	while (service->private->subcommand_running) {
		g_main_iteration (TRUE);
	}
	trilobite_debug ("ending monitor on %d", fd);

	switch (eazel_install_get_package_system (service)) {
	case EAZEL_INSTALL_USE_RPM:
		result =  service->private->packsys.rpm.packages_installed;
		break;
	}
	return result;
}

static gboolean
eazel_install_do_transaction_md5_check (EazelInstall *service, 
					GList *packages)
{
	gboolean result = TRUE;
	GList *iterator;

	result = eazel_install_lock_tmp_dir (service);

	if (!result) {
		g_warning (_("Failed to lock the downloaded file"));
		return FALSE;
	}

	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		
		if (pack->md5) {
			char pmd5[16];
			char md5[16];

			md5_get_digest_from_file (pack->filename, md5);
			md5_get_digest_from_md5_string (pack->md5, pmd5);

			if (memcmp (pmd5, md5, 16) != 0) {
				g_warning (_("MD5 mismatch, package %s may be compromised"), pack->name);
				trilobite_debug ("read md5 from file %s", pack->filename);
				trilobite_debug ("for package %s version %s", pack->name, pack->version);
				eazel_install_emit_md5_check_failed (service, 
								     pack, 
								     md5_get_string_from_md5_digest (md5));
				result = FALSE;
			} else {
				trilobite_debug ("md5 match on %s", pack->name);
			}
		} else {
			trilobite_debug ("No MD5 available for %s", pack->name);
		}
	}	

	return result;
}

/* This begins the package transaction.
   Return value is number of failed packages */
int
eazel_install_start_transaction (EazelInstall *service,
				 GList* packages) 
{
#ifdef EAZEL_INSTALL_SLIM
	int child_pid, child_status;
#else
	TrilobiteRootHelper *root_helper;
	TrilobiteRootHelperStatus root_helper_stat;
#endif
	GList *args;
	int fd;
	int res;
	int child_exitcode;

	if (g_list_length (packages) == 0) {
		return 0;
	}
		
	service->private->packsys.rpm.packages_installed = 0;
	service->private->packsys.rpm.num_packages = g_list_length (packages);
	service->private->packsys.rpm.current_installed_size = 0;
	service->private->packsys.rpm.total_size = 0;

	args = NULL;
	res = 0;

	if (service->private->downloaded_files) {
		if (!eazel_install_do_transaction_md5_check (service, packages)) {
			res = g_list_length (packages);
		}
	}

	if (res == 0) {
		eazel_install_do_transaction_fill_hash (service, packages);
		service->private->packsys.rpm.total_size = 
			eazel_install_get_total_size_of_packages (service,
								  packages);
		args = eazel_install_start_transaction_make_argument_list (service, packages);
		
		trilobite_debug (_("Preflight (%ld bytes, %ld packages)"), 
			   service->private->packsys.rpm.total_size,
			   service->private->packsys.rpm.num_packages);
		if (!eazel_install_emit_preflight_check (service, packages)) {
			trilobite_debug ("Operation aborted at user request");
			res = g_list_length (packages);
		} 
		
		eazel_install_display_arguments (args);
	}
#ifdef EAZEL_INSTALL_SLIM
	if (res == 0) {
		char **argv;
		int i;
		int useless_stderr;
		GList *iterator;
		 
		/* Create argv list */
		argv = g_new0 (char*, g_list_length (args) + 2);
		argv[0] = g_strdup ("rpm");
		i = 1;
		for (iterator = args; iterator; iterator = g_list_next (iterator)) {
			argv[i] = g_strdup (iterator->data);
			i++;
		}
		argv[i] = NULL;

		if (access ("/bin/rpm", R_OK|X_OK)!=0) {
			g_warning ("/bin/rpm missing or not executable for uid");
			res = service->private->packsys.rpm.num_packages;
		} 
		/* start /bin/rpm... */
		if (res==0 &&
		    (child_pid = trilobite_pexec ("/bin/rpm", argv, NULL, &fd, &useless_stderr))==0) {
			g_warning ("Could not start rpm");
			res = service->private->packsys.rpm.num_packages;
		} else {
			trilobite_debug (_("rpm running..."));
		}

		for (i = 0; argv[i]; i++) {
			g_free (argv[i]);
		}
		g_free (argv);
	}
#else /* EAZEL_INSTALL_SLIM     */
	if (res == 0) {
		/* Fire off the helper */	
		root_helper = gtk_object_get_data (GTK_OBJECT (service), "trilobite-root-helper");
		root_helper_stat = trilobite_root_helper_start (root_helper);
		if (root_helper_stat != TRILOBITE_ROOT_HELPER_SUCCESS) {
			g_warning ("Error in starting trilobite_root_helper");
			res = service->private->packsys.rpm.num_packages;
		}
		
		/* Run RPM */
		if (res==0 && trilobite_root_helper_run (root_helper, 
							 TRILOBITE_ROOT_HELPER_RUN_RPM, args, &fd) != 
		    TRILOBITE_ROOT_HELPER_SUCCESS) {
			g_warning ("Error in running trilobite_root_helper");
			res = service->private->packsys.rpm.num_packages;
			trilobite_root_helper_destroy (GTK_OBJECT (root_helper));
		}
	}

#endif /* EAZEL_INSTALL_SLIM     */
	if (res==0) {		
		int installed_packages;
		installed_packages = eazel_install_monitor_subcommand_pipe (service,
									    fd,
								 (GIOFunc)eazel_install_monitor_process_pipe);
		res = g_list_length (packages) - installed_packages;
#ifdef EAZEL_INSTALL_SLIM
		waitpid (child_pid, &child_status, 0);
		if (WIFEXITED (child_status)) {
			child_exitcode = WEXITSTATUS (child_status);
		} else {
			child_exitcode = -1;
		}
#else	/* EAZEL_INSTALL_SLIM */
		/* this REALLY SUCKS -- but the exit code from userhelper is WORTHLESS! */
		child_exitcode = 0;
		/*
		  child_exitcode = trilobite_root_helper_get_exit_code (root_helper);
		*/
		trilobite_root_helper_destroy (GTK_OBJECT (root_helper));
#endif	/* EAZEL_INSTALL_SLIM */
		trilobite_debug ("child exit code = %d", child_exitcode);
		/* but first, do a sanity check:
		 * if rpm returned 0 exit code, and we're uninstalling, it probably did them all.
		 * (it doesn't tend to give any progress info on that stuff.)
		 * if we think we succeeded, but rpm returned a non-zero exit code, we probably
		 * actually failed.
		 */
		if (eazel_install_get_uninstall (service) && (child_exitcode == 0)) {
			res = 0;
		} else if (eazel_install_get_test (service) && (child_exitcode == 0)) {
			/* in test mode, 0 = good */
			res = 0;
		} else if ((child_exitcode != 0) && (res == 0)) {
			res = g_list_length (packages);
		}
		trilobite_debug ("transaction status = %d", res);
	}

	g_list_foreach (args, (GFunc)g_free, NULL);
	g_list_free (args);

	return res;
} /* end start_transaction */

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
	g_return_if_fail (pack!=NULL);
	if (g_list_find (*pruned, pack) || pack->name==NULL) {
		return;
	}
	trilobite_debug (_("Removing package %s %s"), pack->name, pack->toplevel ? "(emit fail)" :"()");
	if (pack->toplevel) {
		/* We only emit signal for the toplevel packages, 
		   and only delete them. They _destroy function destroys
		   the entire dep tree */
		eazel_install_emit_install_failed (service, pack);
	}
	(*pruned) = g_list_prepend (*pruned, pack);
	for (iterator = pack->soft_depends; iterator; iterator = g_list_next (iterator)) {
		PackageData *sub;
		sub = (PackageData*)iterator->data;
		eazel_install_prune_packages_helper (service, packages, pruned, sub);
	}
	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *super;
		
		super = (PackageData*)iterator->data;
		if (g_list_find (super->soft_depends, pack)) {			
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
			PackageData *pack;
			pack = (PackageData*)iterator->data;
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
eazel_install_free_rpm_system_close_db_foreach (char *key, rpmdb db, gpointer unused)
{
	if (db) {
		trilobite_debug (_("Closing db for %s (open)"), key);
		rpmdbClose (db);
		db = NULL;
		g_free (key);
	} else {
		trilobite_debug (_("Closing db for %s (not open)"), key);
	}

		
}

static gboolean
eazel_install_free_rpm_system (EazelInstall *service)
{
	/* Close all the db's */
	trilobite_debug ("service->private->packsys.rpm.dbs.size = %d", g_hash_table_size (service->private->packsys.rpm.dbs));
	g_hash_table_foreach_remove (service->private->packsys.rpm.dbs, 
				     (GHRFunc)eazel_install_free_rpm_system_close_db_foreach,
				     NULL);
	trilobite_debug ("service->private->packsys.rpm.dbs.size = %d", g_hash_table_size (service->private->packsys.rpm.dbs));
	
/*
  This crashes, so it's commented out. 
  These are the vars used for this.
  
	rpmTransactionSet *set;
	set = &(service->private->packsys.rpm.set);

	if (*set != NULL) {
		rpmtransFree (*set);
		(*set) = NULL;
	}
*/
	return TRUE;
}

static gboolean
eazel_install_prepare_rpm_system(EazelInstall *service)
{
	GList *iterator;

	rpmReadConfigFiles (eazel_install_get_rpmrc_file (service), NULL);

	g_assert (g_list_length (eazel_install_get_root_dirs (service)));

	addMacro(NULL, "_dbpath", NULL, "/", 0);

	if (g_hash_table_size (service->private->packsys.rpm.dbs) > 0) {
		trilobite_debug ("db already open ?");
		return TRUE;
	}
	
	for (iterator = eazel_install_get_root_dirs (service); iterator; iterator = g_list_next (iterator)) {
		const char *root_dir;	
		rpmdb db;
		
		root_dir = (char*)iterator->data;
		
		if (rpmdbOpen (root_dir, &db, O_RDONLY, 0644)) {
			g_warning (_("RPM package database query failed !"));
		} else {			
			if (db) {
				trilobite_debug (_("Opened packages database in %s"), root_dir);
			} else {
				trilobite_debug (_("Opening packages database in %s failed"), root_dir);
			}
			g_hash_table_insert (service->private->packsys.rpm.dbs,
					     g_strdup (root_dir),
					     db);
		}
	}


	return TRUE;
}

gboolean
eazel_install_prepare_package_system (EazelInstall *service)
{
	gboolean result = FALSE;

	trilobite_debug (_("Preparing package system"));

	switch (eazel_install_get_package_system (service)) {
	case EAZEL_INSTALL_USE_RPM:
		result = eazel_install_prepare_rpm_system (service);	       
		break;
	}
	return result;
}

gboolean
eazel_install_free_package_system (EazelInstall *service)
{
	gboolean result = FALSE;

	switch (eazel_install_get_package_system (service)) {
	case EAZEL_INSTALL_USE_RPM:
		result = eazel_install_free_rpm_system (service);	       
		break;
	}
	return result;
}

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
						  *((Header*)pack->packsys_struc),
						  NULL, 
						  NULL,
						  interface_flags, 
						  NULL);
			if (err!=0) {
				g_warning ("rpmtransAddPackage (..., %s, ...) = %d", pack->name, err);
				/* We cannot remove the thing immediately from packages, as
				   we're iterating it, so add to a tmp list and nuke later */
				tmp_failed = g_list_prepend (tmp_failed, pack);
			}
			/* just flailing around here (robey) */
			if (pack->soft_depends) {
				eazel_install_add_to_rpm_set (service, set, &pack->soft_depends, failed);
			}
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

int
eazel_install_package_name_compare (PackageData *pack,
				    char *name)
{
	return strcmp (pack->name, name);
}

/* This does a slow and painfull comparison of all the fields */
int eazel_install_package_compare (PackageData *pack, 
				   PackageData *other)
{
	int result = 0;
	/* For the field sets, if they both exists, compare them,
	   if one has it and the other doesn't, not equal */
	if (pack->name && other->name) {
		int tmp_result = strcmp (pack->name, other->name);
		if (tmp_result) {
			result = tmp_result;
		}
	} else if (pack->name || other->name) {
		result = 1;
	}
	if (pack->version && other->version) {
		int tmp_result = strcmp (pack->version, other->version);
		if (tmp_result) {
			result = tmp_result;
		}
	} else if (pack->version || other->version) {
		result = 1;
	}
	if (pack->minor && other->minor) {
		int tmp_result = strcmp (pack->minor, other->minor);
		if (tmp_result) {
			result = tmp_result;
		}
	} else if (pack->minor || other->minor) {
		result = 1;
	}
	
	return result;
}

/* Compare function used while creating the PackageRequirements in 
   eazel_install_do_dependency_check.
   It checks for equality on the package names, if one doens't have a name,
   it checks for the same 1st element in ->provides, if one doens't have 
   a provides list, they're not the same */
int 
eazel_install_requirement_dep_compare (PackageRequirement *req,
				       PackageData *pack)
{
	if (pack->name && req->required->name ) {
		return strcmp (req->required->name, pack->name);
	} else if (pack->provides && req->required->provides) {
		return strcmp ((char*)pack->provides->data, (char*)req->required->provides);
	} else {
		return -1;
	}
}


int eazel_install_package_version_compare (PackageData *pack, 
					   char *version)
{
	return strcmp (pack->version, version);
}

static int
eazel_install_package_provides_basename_compare (char *a,
						 char *b)
{
	return strcmp (g_basename (a), b);
}

static int
eazel_install_package_provides_compare (PackageData *pack,
					char *name)
{
	GList *ptr = NULL;
	ptr = g_list_find_custom (pack->provides, 
				  (gpointer)name, 
				  (GCompareFunc)eazel_install_package_provides_basename_compare);
	if (ptr) {
		trilobite_debug ("package %s supplies %s", pack->name, name);
		return 0;
	} 
	return -1;
}

static int
eazel_install_package_modifies_provides_compare (PackageData *pack,
						 char *name)
{
	GList *ptr = NULL;
	ptr = g_list_find_custom (pack->modifies, 
				  (gpointer)name, 
				  (GCompareFunc)eazel_install_package_provides_compare);
	if (ptr) {
		trilobite_debug ("package %s caused harm to %s", pack->name, name);
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
eazel_install_add_to_extras_foreach (char *key, GList *list, GList **extrapackages)
{
	GList *iterator;
	PackageData *dep;
	for (iterator = list; iterator; iterator = g_list_next (iterator)) {
		dep = (PackageData*)iterator->data;
		(*extrapackages) =  g_list_prepend (*extrapackages, dep);		
	}
}

/* Returns 0 if the package is causes same state (ie package already installed
   with same version.
   -1 if it'll downgrade your system, (ie. you have a newer version installed)
   1 if it'll upgrade your system (ie. if you have an older version installed)
   2 if you don't have the package already 

   In some weird cases, you should eg get -1 and 1 returned (or such). But this will always
   return the "lowest possible" stage */
static int
eazel_install_check_existing_packages (EazelInstall *service, 
				       PackageData *pack)
{
	GList *existing_packages;
	int result;

	result = 2;
	/* query for existing package of same name */
	existing_packages = eazel_install_simple_query (service, 
							pack->name, 
							EI_SIMPLE_QUERY_MATCHES, 
							0, NULL);
	if (existing_packages) {
		/* Get the existing package, set it's modify flag and add it */
		GList *iterator;
		if (g_list_length (existing_packages)>1) {
			trilobite_debug ("there are %d existing packages called %s",
					 g_list_length (existing_packages),
					 pack->name);
			trilobite_debug ("This is a bad bad case, see bug 3511");
			/* FIXME bugzilla.eazel.com: 3511 */
			g_assert_not_reached ();
		}
		for (iterator = existing_packages; iterator; iterator = g_list_next (iterator)) {
			PackageData *existing_package = (PackageData*)iterator->data;
			int res;

			if (g_list_find_custom (pack->modifies, 
						existing_package->name,
						(GCompareFunc)eazel_install_package_name_compare)) {
				trilobite_debug ("%s already marked as modified", existing_package->name);
				packagedata_destroy (existing_package, TRUE);
				existing_package = NULL;
				continue;
			}
			pack->modifies = g_list_prepend (pack->modifies, existing_package);
			existing_package->status = PACKAGE_RESOLVED;
			/* The order of arguments to rpmvercmp is important... */
			res = rpmvercmp (pack->version, existing_package->version);
			
			if (res == 0) {
				existing_package->modify_status = PACKAGE_MOD_UNTOUCHED;
			} else if (res > 0) {
				existing_package->modify_status = PACKAGE_MOD_UPGRADED;
			} else {
				existing_package->modify_status = PACKAGE_MOD_DOWNGRADED;
			}

			if (res == 0 && result > 0) {
				result = 0;
			} else if (res > 0 && result > 1) {
				result = 1;
			} else {
				result = -1;
			}
		
			if (result!=0) {
				if (result>0) {
					trilobite_debug (_("%s upgrades from version %s to %s"),
							 pack->name, 
							 existing_package->version, 
							 pack->version);
				} else {
					trilobite_debug (_("%s downgrades from version %s to %s"),
							 pack->name, 
							 existing_package->version, 
							 pack->version);
				}
			} else {
				pack->status = PACKAGE_ALREADY_INSTALLED;
				trilobite_debug (_("%s version %s already installed"), 
						 pack->name, 
						 existing_package->version);
			}
		}
	}

	return result;
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
static gboolean
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

	trilobite_debug ("%d requirements", g_list_length (requirements));
	for (iterator = requirements; iterator; glist_step (iterator)) {
		PackageRequirement *req = (PackageRequirement*)iterator->data;
		PackageData *pack = req->package;
		PackageData *dep = req->required;

		/* Emit the signal here, since then we won't have to make that
		   call in for every package system (when I say every, we know
		   I mean "both"...) */
		eazel_install_emit_dependency_check (service, pack, dep);
		pack->soft_depends = g_list_prepend (pack->soft_depends, dep);

		fetch_result = eazel_install_fetch_package (service, dep);

		if (fetch_result) {
			/* if it succeeds, add to a list of extras for this package 
			   We cannot just put it into extrapackages, as a later dep
			   might fail, and then we have to fail the package */
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
						dep->name,
						(GCompareFunc)eazel_install_package_name_compare)) {
				trilobite_debug ("already handled %s", dep->name);
				trilobite_debug ("This verifies the existence of 5 lines of code...");
				packagedata_remove_soft_dep (dep, pack); 
				dep = NULL;
				continue;
			}

			/* Sets the pack->modifies list */
			eazel_install_check_existing_packages (service, pack);

			/* FIXME bugzilla.eazel.com 2584:
			   Need to check that the downloaded package is of sufficiently high version
			   At this point the packagedata struct is overwritten with the 
			   headerinfo, so we need to keep it. Optionally we should also 
			   check provides. But we should be able to assume the server does not
			   give us the wrong package, so this bug is probably not that important.
			*/
			
			/* This call sets the dep->modifies if there are already
			   packages installed of that name */
			eazel_install_check_existing_packages (service, dep);
			
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
			dep->status = PACKAGE_CANNOT_OPEN;
			
			if (!eazel_install_get_force (service)) {
				/* Remove the extra packages for this package */
				extralist = g_hash_table_lookup (extras, pack->name);			
				/* Remove all the extras from the soft_deps (is this what we want ?) */
				g_list_foreach (extralist, (GFunc)packagedata_remove_soft_dep, pack); 
				g_list_free (extralist);
				g_hash_table_remove (extras, pack->name);
				
				/* Don't add it to failedpackages more then once */
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
	char *softprefix, *hardprefix;

	trilobite_debug ("%s%s (%s) %08X", prefix, pack->name, pack->version ? pack->version : "NO VERSION",
			 (unsigned int)pack);
	softprefix = g_strdup_printf ("%s (s) ", prefix);
	hardprefix = g_strdup_printf ("%s (h) ", prefix);
	g_list_foreach (pack->soft_depends, (GFunc)dump_one_package, softprefix);
	g_list_foreach (pack->hard_depends, (GFunc)dump_one_package, hardprefix);
	g_free (softprefix);
	g_free (hardprefix);
}

void
dump_packages (GList *packages)
{
	trilobite_debug ("#####  PACKAGE TREE  #####");
	g_list_foreach (packages, (GFunc)dump_one_package, "");
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

/* This is the function to do the RPM system dependency check */
static void
eazel_install_do_rpm_dependency_check (EazelInstall *service,
				       GList **packages,
				       GList **failedpackages,
				       GList **requirements)
{
	int iterator;
	rpmTransactionSet set;
	int num_conflicts;
	struct rpmDependencyConflict *conflicts;
	struct rpmDependencyConflict conflict;
	rpmdb db;

	trilobite_debug ("eazel_install_do_rpm_dependency_check");

	db = (rpmdb)g_hash_table_lookup (service->private->packsys.rpm.dbs,
					 service->private->cur_root);
	if (!db) {
		return;
	}

	set =  rpmtransCreateSet (db, service->private->cur_root);
	
	eazel_install_add_to_rpm_set (service, set, packages, failedpackages); 

	/* Reorder the packages as per. deps and do the dep check */
	rpmdepOrder (set);		
	rpmdepCheck (set, &conflicts, &num_conflicts);

	/* FIXME bugzilla.eazel.com 1512:
	   This piece of code is rpm specific. It has some generic algorithm
	   for doing the dep stuff, but it's rpm entangled */

	for (iterator = 0; iterator < num_conflicts; iterator++) {
		GList *pack_entry = NULL;
		PackageData *pack = NULL;
		PackageData *dep = NULL;

		conflict = conflicts[iterator];

		/* Locate the package that caused the conflict */
		pack_entry = g_list_find_custom (*packages, 
						 conflict.byName,
						 (GCompareFunc)eazel_install_package_name_compare);

		/* If we did not find it, we're in a special case conflict */
		if (pack_entry == NULL) {
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
				/* If pack_entry is null, we're in the worse case, where
				   install A causes file f to disappear, and package conflict.byName
				   needs f (conflict.needsName). So conflict does not identify which
				   package caused the conflict */
				if (pack_entry==NULL) {
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
						trilobite_debug ("This was certainly unexpected!");
						g_assert_not_reached ();
					}
				}
				
				/* Create a packagedata for the dependecy */
				dep = packagedata_new_from_rpm_conflict_reversed (conflict);
				pack = (PackageData*)(pack_entry->data);
				dep->archtype = g_strdup (pack->archtype);
				pack->status = PACKAGE_BREAKS_DEPENDENCY;
				dep->status = PACKAGE_DEPENDENCY_FAIL;
				g_assert (dep!=NULL);
				
				/* Here I check to see if I'm breaking the -devel package, if so,
				   request it. It does a pretty generic check to see
				   if dep is on the form x-z and pack is x[-y] */

				if (eazel_install_check_if_related_package (service, pack, dep)) {
					trilobite_debug ("check_if_related_package returned TRUE");
					g_free (dep->version);
					dep->version = g_strdup (pack->version);
				} else {
					/* not the devel package, are we in force mode ? */
					if (!eazel_install_get_force (service)) {
						/* if not, remove the package */
						pack->breaks = g_list_prepend (pack->breaks, dep);
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
		} else {
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
		}
		
		
		if (pack && dep) {
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
		} else {
			/* We shouldn't end here */
			g_assert_not_reached ();
		}
	}

	rpmdepFreeConflicts (conflicts, num_conflicts);
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

	/* Now do file conflicts on all packages */
	for (iterator = *packages; iterator; glist_step (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		GList *required = NULL;
		
		/* If we haven't tested conflicts yet */
		if (pack->conflicts_checked == FALSE && 
		    eazel_install_check_for_file_conflicts (service, pack, &required)) {
			pack->conflicts_checked = TRUE;
			if (required) {
				/* Create PackageRequirements for all the requirements */
				GList *reqiterator;
				for (reqiterator = required;reqiterator;glist_step (reqiterator)) {
					PackageData *required_pack = (PackageData*)reqiterator->data;
					PackageRequirement *req;
					req = packagerequirement_new (pack, required_pack);
					(*requirements) = g_list_prepend (*requirements, req);
				}
			} else {
				tmp_failed = g_list_prepend (tmp_failed, pack);
			}
		} else {
			/* No file conflicts */
			trilobite_debug ("package %s had no file conflicts", pack->name);
		}
	}
	
	/* Now clean up */
	for (iterator = tmp_failed; iterator; glist_step (iterator)) {
		(*failedpackages) = g_list_prepend (*failedpackages, iterator->data);
		(*packages) = g_list_remove (*packages, iterator->data);
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
	switch (eazel_install_get_package_system (service)) {
	case EAZEL_INSTALL_USE_RPM: {
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
	break;
	}	
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
	gboolean result;
	GList *requirements = NULL;

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
		GList *extrapackages;
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
		
		/* Some debug printing */
		print_package_list ("Packages to install (a)", *packages, FALSE);
		print_package_list ("Packages that were fetched", extrapackages, FALSE);
		print_package_list ("Packages that failed", *failedpackages, TRUE);		       		       
		
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
				eazel_install_prune_packages (service, pack, packages, 
							      &extrapackages, NULL);
			}			
		} 
		if (extrapackages) {
			GList *iterator;
			
			/* Add to "packages" */
			for (iterator = extrapackages; iterator; iterator = g_list_next (iterator)) {
				(*packages) = g_list_append (*packages, iterator->data);
			}
			
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
	} else {
		GList *iterator;
		
		/* Deps are fine, set all packages to resolved */
		for (iterator=*packages; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack;
			pack = (PackageData*)iterator->data;
			pack->status = PACKAGE_RESOLVED;
		}
		trilobite_debug (_("Dependencies are ok"));
	}
	      
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
		GList *matches;
		GList *match_iterator;
		GList *tmp_breaks = NULL;
		GList *break_iterator = NULL;
		
		trilobite_debug ("checking reqs by %s", rpmname_from_packagedata (pack));
		matches = eazel_install_simple_query (service, pack->name, EI_SIMPLE_QUERY_REQUIRES,
						      1, *packages);
		
		for (match_iterator = matches; match_iterator; match_iterator = g_list_next (match_iterator)) {
			PackageData *requiredby = (PackageData*)match_iterator->data;;
			
			requiredby->status = PACKAGE_DEPENDENCY_FAIL;
			pack->status = PACKAGE_BREAKS_DEPENDENCY;
			trilobite_debug ("%s requires %s", requiredby->name, pack->name);

			/* If we're already marked it as breaking, go on */
			if (g_list_find_custom (*breaks, (gpointer)requiredby->name, 
						(GCompareFunc)eazel_install_package_name_compare)) {
				trilobite_debug ("skip %s", requiredby->name);
				packagedata_destroy (requiredby, TRUE);
				requiredby = NULL;
				continue;
			}

			/* Guess not, mark it as breaking (and that pack is the offender */
			pack->breaks = g_list_prepend (pack->breaks, requiredby);
			(*breaks) = g_list_prepend ((*breaks), requiredby);

			/* If the package has not been failed yet (and is a toplevel),
			   fail it */
			if (!g_list_find_custom (*failed, (gpointer)pack->name, 
						 (GCompareFunc)eazel_install_package_name_compare) &&
			    pack->toplevel) {
				(*failed) = g_list_prepend (*failed, pack);
			}
		}
		
		
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
		GList *matches;
		PackageData *pack;
		GList *match_iterator;

		pack = (PackageData*)iterator->data;

		matches = eazel_install_simple_query (service, pack->name, EI_SIMPLE_QUERY_MATCHES, 0, NULL);
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
			hd = *((Header*)matched_pack->packsys_struc);

			if (!headerGetEntry(hd, RPMTAG_REQUIRENAME, &type, (void **) &require_name,
					    &require_name_count)) {
				require_name_count = 0;
			}
			
			trilobite_debug ("requirename count = %d", require_name_count);
			
			/* No iterate over all packages required by the current package */
			for (j = 0; j < require_name_count; j++) {
				if ((*require_name[j] != '/') &&
				    !strstr (require_name[j], ".so")) {
					GList *second_matches;
					GList *second_match_iterator;

					second_matches = eazel_install_simple_query (service, 
										     require_name[j],
										     EI_SIMPLE_QUERY_MATCHES,
										     2, *packages, *requires);
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
								eazel_install_simple_query (service, 
											    isrequired->name,
											    EI_SIMPLE_QUERY_REQUIRES, 
											    3, 
											    *packages, 
											    *requires,
											    tmp_requires);
							
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

		matches = eazel_install_simple_query (service, pack->name, EI_SIMPLE_QUERY_MATCHES, 0, NULL);
		/* If it's installed, continue */
		if (matches) {
			if (g_list_length (matches)==1) {
				PackageData *matched = (PackageData*)matches->data;
				/* This is mucho important. If not marked 
				   as toplevel, upwards traverse will not fail the package
				   is it has dependents */
				matched->toplevel = TRUE;

				result = g_list_prepend (result, matched);
 			} else {
				g_assert_not_reached ();
			}
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
		
	if (*packages) {
		eazel_uninstall_upward_traverse (service, packages, failed, &tmp);
		print_package_list ("FAILED", *failed, TRUE);
		for (iterator = *failed; iterator; iterator = g_list_next (iterator)) {
			trilobite_debug ("failed %s", ((PackageData*)iterator->data)->name);
			eazel_install_emit_uninstall_failed (service, (PackageData*)iterator->data);
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
