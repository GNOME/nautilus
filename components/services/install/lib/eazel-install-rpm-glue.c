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

#include "eazel-install-public.h"
#include "eazel-install-private.h"

#include "eazel-install-query.h"

#ifndef EAZEL_INSTALL_NO_CORBA
#include <libtrilobite/libtrilobite.h>
#else
#include <libtrilobite/trilobite-root-helper.h>
#endif

#include <libtrilobite/libtrilobite-service.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmmacro.h>
#include <rpm/dbindex.h>
#include <string.h>
#include <time.h>

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

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

static int eazel_install_package_conflict_compare (PackageData *pack,
						   struct rpmDependencyConflict *conflict);

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

static GList *
eazel_install_flatten_categories (EazelInstall *service,
				  GList *categories)
{
	GList* packages = NULL;
	GList* iterator, *category_iterator;
	
	for (category_iterator = categories; category_iterator; category_iterator = g_list_next (category_iterator)) {
		CategoryData *cat = (CategoryData*)category_iterator->data;
		packages = g_list_concat (packages, cat->packages);
	}


	return packages;
}

EazelInstallStatus
install_new_packages (EazelInstall *service, GList *categories) {
	EazelInstallStatus result;
	int install_flags, interface_flags, problem_filters;
	
	install_flags = 0;
	interface_flags = 0;
	problem_filters = 0;
	
	if (eazel_install_get_test (service) == TRUE) {
		g_message (_("Dry Run Mode Activated.  Packages will not actually be installed ..."));
		install_flags |= RPMTRANS_FLAG_TEST;
	}

	if (eazel_install_get_update (service) == TRUE) {
		interface_flags |= INSTALL_UPGRADE;
	}

	if (eazel_install_get_verbose (service) == TRUE) {
		rpmSetVerbosity (RPMMESS_VERBOSE);
	}
	else {
		rpmSetVerbosity (RPMMESS_NORMAL);
	}

	if (eazel_install_get_force (service) == TRUE) {
		problem_filters |= RPMPROB_FILTER_REPLACEPKG |
			RPMPROB_FILTER_REPLACEOLDFILES |
			RPMPROB_FILTER_REPLACENEWFILES |
			RPMPROB_FILTER_OLDPACKAGE;
	}

	eazel_install_set_install_flags (service, install_flags);
	eazel_install_set_interface_flags (service, interface_flags);
	eazel_install_set_problem_filters (service, problem_filters);
	
	if (categories == NULL) {
		g_message (_("Reading the install package list %s"), eazel_install_get_package_list (service));
		categories = parse_local_xml_package_list (eazel_install_get_package_list (service));
	}

	if (categories == NULL) {
		result = EAZEL_INSTALL_NOTHING;
	} else {
		/* First, collect all packages in one list */
		GList *packages = eazel_install_flatten_categories (service, categories);

		/* Now download all the packages */
		if (eazel_install_download_packages (service, TRUE, &packages, NULL)) {
			result = EAZEL_INSTALL_DOWNLOADS;

			/* Files downloaded, now install */
			if (eazel_install_do_install_packages (service, packages)) {
				result |= EAZEL_INSTALL_INSTALL_OK;
			}
		}
	}
	
	return result;
} /* end install_new_packages */

static gboolean
eazel_install_download_packages (EazelInstall *service,
				 gboolean toplevel,
				 GList **packages,
				 GList **failed_packages)
{
	GList *iterator;
	gboolean result = FALSE;
	GList *remove_list = NULL;

	g_assert (packages);
	g_assert (*packages);

	for (iterator = *packages;iterator; iterator = g_list_next (iterator)) {
		PackageData* package = (PackageData*)iterator->data;
		gboolean fetch_package;
		
		fetch_package = TRUE;

		g_message ("D: init for %s", package->name);
		/* if filename in the package is set, but the file
		   does not exist, get it anyway */
		if (package->filename) {
			g_message ("D: Filename set, and file exists = %d", 
				   g_file_test (package->filename, G_FILE_TEST_ISFILE));
			if (g_file_test (package->filename, G_FILE_TEST_ISFILE)) {
				fetch_package = FALSE;	
				result = TRUE;
				packagedata_fill_from_file (package, package->filename);
			} else {
					/* The file didn't exist, remove the 
					   leading path and set the filename, plus
					   toggle the fetch_package to TRUE */
				char *tmp;
										
				tmp = g_basename (package->filename);
				g_free (package->filename);
				package->filename = g_strdup (tmp);
				fetch_package = TRUE;
			}
		} else if (!eazel_install_get_force (service) && package->version) {
			/* If the package has a version set, check that we don't already have
			   the same or newer installed. This is almost the same check as
			   in eazel_install_pre_install_package. The reason for two checks is
			   - first check before download (if possible)
			   - after download, when we for sure have access to the version, check again
			*/
			int inst_status = eazel_install_check_existing_packages (service, package);
			if (inst_status <= 0) {
				fetch_package = FALSE;
				remove_list = g_list_prepend (remove_list, package);
				g_message (_("%s already installed"), package->name);
			}
		} 

		if (fetch_package) {
			g_message (_("Will download %s"), package->name);
			if (eazel_install_fetch_package (service, package)==FALSE) {
				remove_list = g_list_prepend (remove_list, package);
			} else {
				result = TRUE;
				package->toplevel = toplevel;
				if (package->soft_depends) {
					eazel_install_download_packages (service,
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

	return TRUE;
}

/* Checks for pre-existance of all the packages
   and returns a flattened packagelist (flattens the
   soft_ and hard_depends */
static GList*
eazel_install_pre_install_packages (EazelInstall *service,
				    GList **packages) 
{
	GList *iterator;
	GList *failed_packages = NULL;
	
	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack;
		int inst_status;
		
		pack = (PackageData*)iterator->data;
		inst_status = eazel_install_check_existing_packages (service, pack);
		g_message ("D: %s: install status = %d", pack->name, inst_status);
		/* If in force mode, install it under all circumstances.
		   if not, only install if not already installed in same
		   version or up/downgrade is set */
		if (eazel_install_get_force (service) ||
		    (eazel_install_get_downgrade (service) && inst_status == -1) ||
		    (eazel_install_get_update (service) && inst_status == 1) ||
		    inst_status == 2) {
			g_message (_("%s..."), pack->name);
		} else {
			g_message (_("Skipping %s..."), pack->name);
			pack->status = PACKAGE_ALREADY_INSTALLED;
			failed_packages = g_list_prepend (failed_packages, pack);
		}
	}
	
	for (iterator = failed_packages; iterator; iterator=iterator->next) {
		eazel_install_prune_packages (service, 
					      (PackageData*)iterator->data,
					      packages, NULL);
	}
	g_list_free (failed_packages);
}

static gboolean
eazel_install_do_install_packages (EazelInstall *service,
				  GList* packages) 
{

	gboolean rv = FALSE;
	GList* failedfiles = NULL;

	eazel_install_pre_install_packages (service, &packages);
	
	if (packages) {
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
	EazelInstallStatus result = EAZEL_INSTALL_NOTHING;

	while (categories) {
		CategoryData* cat = categories->data;
		GList *failed;

		g_message (_("Category = %s"), cat->name);

		failed = NULL;
		eazel_uninstall_globber (service, &cat->packages, &failed);
		eazel_install_free_package_system (service);
		result |= eazel_install_start_transaction (service, 
							   cat->packages);

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
	
	if (eazel_install_get_test (service) == TRUE) {
		g_message (_("Dry Run Mode Activated.  Packages will not actually be installed ..."));
		uninstall_flags |= RPMTRANS_FLAG_TEST;
	}

	if (eazel_install_get_verbose (service) == TRUE) {
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


GList *
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
void
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

void hest (PackageData *pack, char *str) {
	g_message ("D: Must %s %s", str, pack->name);
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
		g_list_foreach (upgrade, (GFunc)packagedata_destroy, GINT_TO_POINTER (TRUE));
	}	
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

static void
eazel_install_do_transaction_get_total_size (EazelInstall *service,
					     GList *packages)
{
	GList *iterator;
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack;

		pack = (PackageData*)iterator->data;
		service->private->packsys.rpm.total_size += pack->bytesize;
	}
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
	if (eazel_install_get_uninstall (service)) {
		(*args) = g_list_prepend (*args, g_strdup ("-e"));
	} else 	if (eazel_install_get_update (service)) {
		(*args) = g_list_prepend (*args, g_strdup ("-Uvh"));
	} else if (eazel_install_get_downgrade (service)) {
		(*args) = g_list_prepend (*args, g_strdup ("--oldpackage"));
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
	name = g_strdup_printf ("%s/transaction.%d", eazel_install_get_transaction_dir (service), time (NULL));
	while (g_file_test (name, G_FILE_TEST_ISFILE)) {
		g_free (name);
		sleep (1);
		name = g_strdup_printf ("%s/transaction.%d", 
					eazel_install_get_transaction_dir (service), 
					time (NULL));
	}

	g_message (_("Writing transaction to %s"), name);
	
	/* Open and save */
	outfile = fopen (name, "w");
	xmlAddChild (root, eazel_install_packagelist_to_xml (service->private->transaction));
	node = xmlAddChild (node, xmlNewNode (NULL, "DESCRIPTIONS"));

	{
		char *tmp;
		tmp = g_strdup_printf ("%d", time (NULL));		
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
	static       int package_name_length = 80;
	static       char package_name [80];
	ssize_t      bytes_read;
	static       PackageData *pack = NULL;
	static       int pct;
	
	g_io_channel_read (source, &tmp, 1, &bytes_read);
	
	if (bytes_read) {
		/* fprintf (stdout, "%c", tmp); fflush (stdout); */
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
			}
			
			/* Not percantage mark, that means filename, step ahead one file */
			pack = g_hash_table_lookup (service->private->name_to_package_hash, package_name);
			if (pack==NULL) {						
				g_warning ("D: lookup \"%s\" failed", package_name);
			} else {
				g_message ("D: matched \"%s\"", package_name);
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

/* 1.39 has the code to parse --percent output */
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

void
eazel_install_display_arguments (GList *args) 
{
	GList *iterator;
	fprintf (stdout, "\nARGS: ");
	for (iterator = args; iterator; iterator = g_list_next (iterator)) {
		fprintf (stdout, "%s ", (char*)iterator->data);
	}
	fprintf (stdout, "\n");
}

void
eazel_install_monitor_subcommand_pipe (EazelInstall *service,
				       int fd,
				       GIOFunc monitor_func)
{
	GIOChannel *channel;
	channel = g_io_channel_unix_new (fd);
	g_message ("D: beginning monitor on %d", fd);
	service->private->subcommand_running = TRUE;
	g_io_add_watch (channel, G_IO_IN | G_IO_ERR | G_IO_NVAL | G_IO_HUP, 
			monitor_func, 
			service);
	while (service->private->subcommand_running) {
		g_main_iteration (TRUE);
	}
	g_message ("D: ending monitor on %d", fd);
}


/* This begins the package transaction.
   Return value is number of failed packages */
int
eazel_install_start_transaction (EazelInstall *service,
				 GList* packages) 
{
#ifndef EAZEL_INSTALL_SLIM
	TrilobiteRootHelper *root_helper;
#endif /* EAZEL_INSTALL_SLIM */
	GList *args;
	int fd;
	int res;
	TrilobiteRootHelperStatus root_helper_stat;

	if (g_list_length (packages) == 0) {
		return 0;
	}
		
	service->private->packsys.rpm.packages_installed = 0;
	service->private->packsys.rpm.num_packages = g_list_length (packages);
	service->private->packsys.rpm.current_installed_size = 0;
	service->private->packsys.rpm.total_size = 0;

	args = NULL;
	res = 0;

	eazel_install_do_transaction_fill_hash (service, packages);
	eazel_install_do_transaction_get_total_size (service, packages);
	args = eazel_install_start_transaction_make_argument_list (service, packages);

	g_message (_("Preflight (%d bytes, %d packages)"), 
		   service->private->packsys.rpm.total_size,
		   service->private->packsys.rpm.num_packages);
	eazel_install_emit_preflight_check (service, 					     
					    service->private->packsys.rpm.total_size,
					    service->private->packsys.rpm.num_packages);

	eazel_install_display_arguments (args);
#ifdef EAZEL_INSTALL_SLIM
	{
		char **argv;
		int i;
		int flags;
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
		if (res==0 && trilobite_pexec ("/bin/rpm", argv, NULL, &fd, &useless_stderr)==0) {
			g_warning ("Could not start rpm");
			res = service->private->packsys.rpm.num_packages;
		} else {
			g_message (_("rpm running..."));
		}
	}
#else /* EAZEL_INSTALL_SLIM     */
	/* Fire off the helper */	
	root_helper = gtk_object_get_data (GTK_OBJECT (service), "trilobite-root-helper");
	root_helper_stat = trilobite_root_helper_start (root_helper);
	if (root_helper_stat != TRILOBITE_ROOT_HELPER_SUCCESS) {
		g_warning ("Error in starting trilobite_root_helper");
		res = service->private->packsys.rpm.num_packages;
	}

	/* Run RPM */
	if (res==0 && trilobite_root_helper_run (root_helper, TRILOBITE_ROOT_HELPER_RUN_RPM, args, &fd) != 
	    TRILOBITE_ROOT_HELPER_SUCCESS) {
		g_warning ("Error in running trilobite_root_helper");
		res = service->private->packsys.rpm.num_packages;
	}

#endif /* EAZEL_INSTALL_SLIM     */
	if (res==0) {		
		eazel_install_monitor_subcommand_pipe (service,
						       fd,
						       (GIOFunc)eazel_install_monitor_process_pipe);
	}

	g_list_foreach (args, (GFunc)g_free, NULL);
	g_list_free (args);

	return res;
} /* end start_transaction */

/*
  The helper for eazel_install_prune_packages.
  If the package is in "pruneds", it has already been marked
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
	g_message (_("Removing package %s %s"), pack->name, pack->toplevel ? "(emit fail)" :"()");
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
	
	for (iterator = pruned; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack;
		pack = (PackageData*)iterator->data;
		/* Note, don't destroy, all packages are destroyed when the
		   categories are destroyed */
		/* packagedata_destroy (pack); */
	};

	g_list_free (pruned);

	va_end (ap);
}

/* Given a glist of PackageDatas, loads and returs a GList,
   where the ->data points to a struct Header*.
*/
static GList*
eazel_install_load_rpm_headers (EazelInstall *service,
				GList **packages) 
{
	GList *result;
	GList *sources;
	GList *iterator;

	result = NULL;
	sources = NULL;
	
	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {
		char *filename;
		int rpm_err;
		PackageData *pack;
		FD_t fd;
		Header *hd;
		int is_source;

		pack = (PackageData*)iterator->data;

		/* If the package already has a packsys struc, keep spinning */
		if (pack->packsys_struc) {
			continue;
		}

		filename = g_strdup (rpmfilename_from_packagedata (pack));

		/* Open the RPM file */
		fd = fdOpen (filename, O_RDONLY, 0644);
		if (fd == NULL) {
			g_warning (_("Cannot open %s"), filename);
			pack->status = PACKAGE_CANNOT_OPEN;
			if (pack->toplevel) {
				eazel_install_prune_packages (service, pack, packages, NULL); 
			}
			g_free (filename);
			continue;
		}
		
		/* Get the header */
		hd = g_new0 (Header, 1);
		rpm_err = rpmReadPackageHeader (fd,
						hd,
						&is_source, 
						NULL, 
						NULL);

		g_free (filename);
		fdClose (fd);
				   
		/* If not a source, fill the pack from the header */
		if (! is_source) {
			result = g_list_prepend (result, hd);
			packagedata_fill_from_rpm_header (pack, hd);
			pack->status = PACKAGE_UNKNOWN_STATUS;
		} else {
			sources = g_list_prepend (sources, pack);
			if (*hd) {
				headerFree (*hd);
			}
		}
	}

	/* Remove all the source packages */
	for (iterator = sources; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack;
		pack = (PackageData*)iterator->data;
		pack->status = PACKAGE_SOURCE_NOT_SUPPORTED;
		eazel_install_prune_packages (service, pack, packages, NULL); 
	}
	g_list_free (sources);

	return result;
}

static GList *
eazel_install_load_headers (EazelInstall *service,
			    GList **packages)
{
	GList *result;
	switch (eazel_install_get_package_system (service)) {
	case EAZEL_INSTALL_USE_RPM:
		result = eazel_install_load_rpm_headers (service, packages);
		break;
	}
	return result;
}

static void
eazel_install_free_rpm_system_close_db_foreach (char *key, rpmdb db, gpointer unused)
{
	g_message ("D: Closing db for %s (%s)", key, db ? "yes" : "no");
	if (db) {
		rpmdbClose (db);
		db = NULL;
		g_free (key);
	}		
}

static gboolean
eazel_install_free_rpm_system (EazelInstall *service)
{
	GList *iterator;

	/* Close all the db's */
	g_message ("service->private->packsys.rpm.dbs.size = %d", g_hash_table_size (service->private->packsys.rpm.dbs));
	g_hash_table_foreach_remove (service->private->packsys.rpm.dbs, 
				     (GHRFunc)eazel_install_free_rpm_system_close_db_foreach,
				     NULL);
	g_message ("service->private->packsys.rpm.dbs.size = %d", g_hash_table_size (service->private->packsys.rpm.dbs));
	
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
		g_message ("D: db already open ?");
		return TRUE;
	}
	
	for (iterator = eazel_install_get_root_dirs (service); iterator; iterator = g_list_next (iterator)) {
		const char *root_dir;	
		rpmdb db;
		
		root_dir = (char*)iterator->data;
		
		if (rpmdbOpen (root_dir, &db, O_RDONLY, 0644)) {
			g_warning (_("RPM package database query failed !"));
		} else {			
			g_message ("D: Opened db for %s (%s)", root_dir, db ? "yes" : "no");
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
	gboolean result;

	g_message (_("Preparing package system"));

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
	gboolean result;

	switch (eazel_install_get_package_system (service)) {
	case EAZEL_INSTALL_USE_RPM:
		result = eazel_install_free_rpm_system (service);	       
		break;
	}
	return result;
}

static void
eazel_install_add_to_rpm_set (EazelInstall *service,
			      GList **packages,
			      GList **failed)
{
	GList *iterator;
	GList *tmp_failed;
	int interface_flags;

	g_assert (packages!=NULL);
	g_assert (*packages!=NULL);

	tmp_failed = NULL;

	if (eazel_install_get_update (service) == TRUE) {
		interface_flags |= INSTALL_UPGRADE;
	}

	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack;
		int err;

		pack = (PackageData*)iterator->data;

		if (!eazel_install_get_uninstall (service)) {
			g_assert (pack->packsys_struc);
			err = rpmtransAddPackage (service->private->packsys.rpm.set,
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
		} else {
			/* This was once used, but not anymore. Will remove
			   once I've ensured it's not used anymore */
			g_assert_not_reached ();
#if 0
			dbiIndexSet matches;
			int rc;
			
			/* This does not use simple_query, as the rpmtransRemovePackage uses
			   record offsets instead of headers */
			rc =  rpmdbFindByLabel (service->private->packsys.rpm.db, pack->name, &matches);

			if (rc!=0) {
				g_warning ("%s not installed, not doing rpmtransRemovePackage", pack->name);
				tmp_failed = g_list_prepend (tmp_failed, pack);
			} else {
				int i;
				for (i=0; i< dbiIndexSetCount (matches); i++) {
					unsigned int offset;
					offset = dbiIndexRecordOffset (matches, i);
					if (offset) {
						rpmtransRemovePackage (service->private->packsys.rpm.set, 
								       offset);
					} else {
						tmp_failed = g_list_prepend (tmp_failed, pack);
					}
				}
			}
#endif
		} 
	} /* end for loop */

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

/*
  Adds the headers to the package system set
 */
static void
eazel_install_add_to_set (EazelInstall *service,
			  GList **packages,
			  GList **failed)

{
	switch (eazel_install_get_package_system (service)) {
	case EAZEL_INSTALL_USE_RPM:
		eazel_install_add_to_rpm_set (service, packages, failed);
		break;
	}
}

int
eazel_install_package_name_compare (PackageData *pack,
				    char *name)
{
	return strcmp (pack->name, name);
}

static int
eazel_install_package_conflict_compare (PackageData *pack,
					struct rpmDependencyConflict *conflict)
{
	return eazel_install_package_name_compare (pack, conflict->byName);
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
		for (iterator = existing_packages; iterator; iterator = g_list_next (iterator)) {
			int res;
			PackageData *existing_package;

			existing_package = (PackageData*)iterator->data;			
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
				g_message (_("%s %s from %s to %s"), 
					   pack->name, 
					   result>0 ? _("upgrades") : _("downgrades"), 
					   existing_package->version, pack->version);
			} else {
				g_message (_("%s version %s already installed"), 
					   pack->name, 
					   existing_package->version);
			}
		}
	}

	return result;
}


/* FIXME bugzilla.eazel.com 1698:
   This function needs some refactoring. It's getting too huge */
static gboolean
eazel_install_fetch_rpm_dependencies (EazelInstall *service, 
				      GList **packages,
				      GList **extrapackages,
				      GList **failedpackages)

{
	int iterator;
	GHashTable *extras;
	GList *to_remove;
	GList *remove_iterator;
	GList *extras_in_this_batch = NULL;
	struct rpmDependencyConflict conflict;
	gboolean fetch_from_file_dependency;
	gboolean fetch_result;
	
	to_remove = NULL;
	extras = g_hash_table_new (g_str_hash, g_str_equal);
	fetch_from_file_dependency = FALSE;
	fetch_result = FALSE;

	/* FIXME bugzilla.eazel.com 1512:
	   This piece of code is rpm specific. It has some generic algorithm
	   for doing the dep stuff, but it's rpm entangled */

	for (iterator = 0; iterator < service->private->packsys.rpm.num_conflicts; iterator++) {
		GList *pack_entry;
		PackageData *pack;
		PackageData *dep;

		conflict = service->private->packsys.rpm.conflicts[iterator];

		/* Check if a previous conflict solve has fixed this conflict */
		/*
		if (g_list_find_custom (extras_in_this_batch,
					conflict.needsName,
					(GCompareFunc)eazel_install_package_name_compare)) {
			g_message ("D: already handled %s", conflict.needsName);
			continue;
		}
		*/
		if (g_list_find_custom (extras_in_this_batch,
					conflict.byName,
					(GCompareFunc)eazel_install_package_name_compare)) {
			g_message ("D: already handled %s", conflict.needsName);
			continue;
		}
		pack_entry = g_list_find_custom (*packages, 
						 (gpointer)&conflict,
						 (GCompareFunc)eazel_install_package_conflict_compare);
		if (pack_entry == NULL) {
			switch (conflict.sense) {
			case RPMDEP_SENSE_REQUIRES: {
				char *tmp;
				
				g_warning (_("%s %s breaks %s"), 
					   conflict.needsName, 
					   conflict.needsVersion, 
					   conflict.byName);
				pack_entry = g_list_find_custom (*packages, 
								 (gpointer)conflict.needsName,
								 (GCompareFunc)eazel_install_package_name_compare);
				if (pack_entry==NULL) {
					/* FIXME bugzilla.eazel.com 2583:
					   Argh, if a lib*.so breaks a package,
					   we end up here */
					/* 
					   I need to find the package P in "packages" that provides
					   conflict.needsName, then fail P marking it's status as 
					   PACKAGE_BREAKS_DEPENDENCY, then create PackageData C for
					   conflict.byName, add to P's depends and mark C's status as
					   PACKAGE_DEPENDENCY_FAIL. 
					   Then then client can rerun the operation with all the C's as
					   part of the update
					*/
					g_message ("D: await bugfix for bug 2583");
					continue;
				}
				
				/* Create a packagedata for the dependecy */
				dep = packagedata_new_from_rpm_conflict_reversed (conflict);
				pack = (PackageData*)(pack_entry->data);
				dep->archtype = g_strdup (pack->archtype);
				pack->status = PACKAGE_BREAKS_DEPENDENCY;
				dep->status = PACKAGE_DEPENDENCY_FAIL;
				g_assert (dep!=NULL);
				
				/* Here I check to see if I'm breaking the -devel package, if so,
				   request it */
				/* FIXME: bugzilla.eazel.com 2596
				   It should handle z=x-y instead of only z=x-devel when x breaks z */
				
				tmp = g_strdup_printf ("%s-devel", pack->name);
				if (strcmp (tmp, dep->name)==0) {
					g_message ("breakage is the devel package");
					g_free (dep->name);
					dep->name = g_strdup (tmp);
					g_free (dep->version);
					dep->version = g_strdup (pack->version);
					g_free (tmp);
				} else {
					g_free (tmp);
					/* not the devel package, are we in force mode ? */
					if (!eazel_install_get_force (service)) {
						/* if not, remove the package */
						pack->breaks = g_list_prepend (pack->breaks, dep);
						if (g_list_find (*failedpackages, pack) == NULL) {
							(*failedpackages) = g_list_prepend (*failedpackages, pack);
						}
						to_remove = g_list_remove (to_remove, pack);
					}
					continue;
				}
			}
			break;
			case RPMDEP_SENSE_CONFLICTS:
				/* If we end here, it's a conflict is going to break something */
				/* FIXME bugzilla.eazel.com 1514:
				   Need to handle this more intelligently */
				g_warning (_("%s conflicts %s-%s"), 
					   conflict.byName, conflict.needsName, 
					   conflict.needsVersion);
				if (g_list_find (*failedpackages, pack) == NULL) {
					(*failedpackages) = g_list_prepend (*failedpackages, pack);
				}
				to_remove = g_list_remove (to_remove, pack);
				continue;
				break;
			}
		} else {
			pack = (PackageData*)pack_entry->data;
			/* Does the conflict look like a file dependency ? */
			if (*conflict.needsName=='/' || strstr (conflict.needsName, ".so")) {
				g_message (_("Processing dep for %s : requires library %s"), 
					   pack->name, conflict.needsName);		
				dep = packagedata_new ();
				dep->name = g_strdup (conflict.needsName);
				fetch_from_file_dependency = TRUE;
			} else {
				dep = packagedata_new_from_rpm_conflict (conflict);
				dep->archtype = g_strdup (pack->archtype);
				fetch_from_file_dependency = FALSE;
				g_message (_("Processing dep for %s : requires %s"), pack->name, dep->name);
			}
		}

		eazel_install_emit_dependency_check (service, pack, dep);
		pack->soft_depends = g_list_prepend (pack->soft_depends, dep);

		if (fetch_from_file_dependency) {
			fetch_result = eazel_install_fetch_package_which_provides (service, 
										   conflict.needsName, 
										   &dep);
		} else {
			fetch_result = eazel_install_fetch_package (service, dep);
		}

		if (fetch_result) {
			/* if it succeeds, add to a list of extras for this package 
			   We cannot just put it into extrapackages, as a later dep
			   might fail, and then we have to fail the package */
			GList *extralist;

			/* FIXME bugzilla.eazel.com 2584:
			   Need to check that the downloaded package is of sufficiently high version
			*/

			/* This call sets the dep->modifies if there are already
			   packages installed of that name */
			eazel_install_check_existing_packages (service, dep);

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
				g_list_foreach (extralist, (GFunc)packagedata_remove_soft_dep, pack);
				g_list_free (extralist);
				g_hash_table_remove (extras, pack->name);
				
				/* Don't add it to failedpackages more then once */
				if (g_list_find (*failedpackages, pack) == NULL) {
					(*failedpackages) = g_list_prepend (*failedpackages, pack);
				}
				to_remove = g_list_prepend (to_remove, pack);
				
				/* Don't process anymore */
				break;
			}
		}
	}
	
	/* iterate over all the lists in extras and add to extrapackages */
	g_hash_table_foreach (extras, (GHFunc)eazel_install_add_to_extras_foreach, extrapackages);
	g_hash_table_destroy (extras);
	g_list_free (extras_in_this_batch);

	/* Removed packages marked as failed. No need to delete them, as they're in
	   (*failedpackages) */
	for (remove_iterator = to_remove; remove_iterator; remove_iterator = g_list_next (remove_iterator)) {
		(*packages) = g_list_remove (*packages, remove_iterator->data);
	}
	g_list_free (to_remove);

	rpmdepFreeConflicts (service->private->packsys.rpm.conflicts, service->private->packsys.rpm.num_conflicts);

	if (*failedpackages) {
		return FALSE;
	} else {
		return TRUE;
	}
}

static gboolean
eazel_install_fetch_dependencies (EazelInstall *service, 
				  GList **packages,
				  GList **extrapackages,
				  GList **failedpackages)
{
	gboolean result;

	switch (eazel_install_get_package_system (service)) {
	case EAZEL_INSTALL_USE_RPM:
		result = eazel_install_fetch_rpm_dependencies (service, 
							       packages,
							       extrapackages,
							       failedpackages);
		break;
	}
	
	return result;
}

static void
print_package_list (char *str, GList *packages, gboolean show_deps)
{
	GList *iterator;
	PackageData *pack;
	char *tmp = NULL;
	char *dep = " depends on ";
	char *breaks = " breaks ";

	g_message ("D: ---------------------------");
	g_message ("D: %s", str);
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		pack = (PackageData*)iterator->data;
		if (show_deps) {
			GList *it2;
			tmp = g_strdup (dep);
			it2 = pack->soft_depends;
			while (it2) { 
				char *tmp2;
				tmp2 = g_strdup_printf ("%s%s ", tmp ,
							rpmfilename_from_packagedata ((PackageData*)it2->data));
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
		g_message ("D: * %s (%s) %s", 
			   rpmfilename_from_packagedata (pack), 
			   pack->toplevel ? "true" : "", 
			   (tmp && strlen (tmp) > strlen (dep)) ? tmp : "");
		g_free (tmp);
		tmp = NULL;
	}
}

/* 
   Use package system to do the dependency check
 */
static int
eazel_install_do_dependency_check (EazelInstall *service) {
	switch (eazel_install_get_package_system (service)) {
	case EAZEL_INSTALL_USE_RPM: {
		rpmTransactionSet *set;
		int *num_conflicts;
		struct rpmDependencyConflict **conflicts;
		
		set = &service->private->packsys.rpm.set;
		num_conflicts = &service->private->packsys.rpm.num_conflicts;
		conflicts = &service->private->packsys.rpm.conflicts;
		
		/* Reorder the packages as per. deps and do the dep check */
		rpmdepOrder (*set);		
		rpmdepCheck (*set, conflicts, num_conflicts);

		return *num_conflicts;
	}
	break;
	}	
	return -1;
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

	g_return_val_if_fail (packages != NULL, TRUE);
	g_return_val_if_fail (*packages != NULL, TRUE);

	g_return_val_if_fail (g_list_length (*packages)>=1, FALSE);
	result = TRUE;
	
	/* First we load headers and prepare them.
	   The datastructures depend on the packagesystem,
	   and are places in service->private->packsys.
	*/
	/* Weak attempt at making it easy to extend to other
	   package formats (debian). 
	*/
	switch (eazel_install_get_package_system (service)) {
	case EAZEL_INSTALL_USE_RPM: {
		int num_conflicts;
		rpmdb db;
/*
		rpmTransactionSet *set;
		struct rpmDependencyConflict **conflicts;
*/
		db = (rpmdb)g_hash_table_lookup (service->private->packsys.rpm.dbs,
						  service->private->cur_root);
		if (!db) {
			return FALSE;
		}

		service->private->packsys.rpm.set = rpmtransCreateSet (db, service->private->cur_root);

		eazel_install_load_headers (service, packages);
		eazel_install_add_to_set (service, packages, failedpackages);
		num_conflicts = eazel_install_do_dependency_check (service);		
		/* rpmtransFree (service->private.packsys.rpm.set); */

		if (num_conflicts != 0) {
			GList *extrapackages;
			GList *iterator;

			extrapackages = NULL;

			/* For all the packages, set state to partly_resolved. */
			for (iterator=*packages; iterator; iterator = g_list_next (iterator)) {
				PackageData *pack;
				pack = (PackageData*)iterator->data;
				pack->status = PACKAGE_PARTLY_RESOLVED;
			}

			g_message (_("%d dependencies failed!"), num_conflicts);
			
			/* Fetch the needed stuff. 
			   "extrapackages" gets the new packages added,
			   packages in "failedpackages" are packages moved from 
			   "packages" that could not be resolved. */
			eazel_install_fetch_dependencies (service, 
							  packages,
							  &extrapackages,
							  failedpackages);

			/* Some debug printing */
			print_package_list ("Packages to install (a)", *packages, FALSE);
			print_package_list ("Packages that were fetched", extrapackages, FALSE);
			print_package_list ("Packages that failed", *failedpackages, TRUE);		       		       

			/* If there was failedpackages, prune them from the tree 
			   and the "extrapackages".
			   We need to strip from "extrapackages" as well, since :
			   while installing A & B, C was added for A, D was
			   added for B but B also needs E. Therefore
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
					(*packages) = g_list_prepend (*packages, iterator->data);
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
		} else {
			GList *iterator;

			/* Deps are fine, set all packages to resolved */
			for (iterator=*packages; iterator; iterator = g_list_next (iterator)) {
				PackageData *pack;
				pack = (PackageData*)iterator->data;
				pack->status = PACKAGE_RESOLVED;
			}
			g_message (_("Dependencies are ok"));
		}
	}
	break;
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
	int num_conflicts;
	GList *iterator;
	/*
	  Create set
	  add all packs from packages to set
	  dep check
	  for all break, add to packages and recurse
	 */

	g_message ("D: in eazel_uninstall_upward_traverse");

	g_assert (packages!=NULL);
	g_assert (*packages!=NULL);
	g_assert (breaks!=NULL);
	g_assert (*breaks==NULL);

	/* Open the package system */

	/* Add all packages to the set */
	/* eazel_install_add_to_set (service, packages, failed); */

	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = (PackageData*)iterator->data;
		GList *matches;
		GList *match_iterator;
		GList *tmp_breaks = NULL;
		GList *break_iterator = NULL;
		
		g_message ("D: checking reqs by %s", rpmname_from_packagedata (pack));
		matches = eazel_install_simple_query (service, pack->name, EI_SIMPLE_QUERY_REQUIRES,
						      1, *packages);
		
		for (match_iterator = matches; match_iterator; match_iterator = g_list_next (match_iterator)) {
			PackageData *requiredby = (PackageData*)match_iterator->data;;
			
			requiredby->status = PACKAGE_DEPENDENCY_FAIL;
			pack->status = PACKAGE_BREAKS_DEPENDENCY;
			g_message ("D: %s requires %s", requiredby->name, pack->name, *breaks);

			/* If we're already marked it as breaking, go on */
			if (g_list_find_custom (*breaks, (gpointer)requiredby->name, 
						(GCompareFunc)eazel_install_package_name_compare)) {
				g_message ("D: skip %s", requiredby->name);
				packagedata_destroy (requiredby, TRUE);
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

	g_message ("D: out eazel_uninstall_upward_traverse");
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
	g_message ("D: in eazel_uninstall_downward_traverse");
	
	/* First iterate across the packages in "packages" */
	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {
		GList *matches;
		PackageData *pack;
		GList *match_iterator;

		pack = (PackageData*)iterator->data;

		matches = eazel_install_simple_query (service, pack->name, EI_SIMPLE_QUERY_MATCHES, 0, NULL);
		g_message ("D: %s had %d hits", pack->name, g_list_length (matches));

		/* Now iterate over all packages that match pack->name */
		for (match_iterator = matches; match_iterator; match_iterator = g_list_next (match_iterator)) {
			PackageData *matched_pack;
			const char **require_name;
			int require_name_count;
			Header hd;
			unsigned int offset;
			int type;
			int j;

			matched_pack = (PackageData*)match_iterator->data;
			hd = *((Header*)matched_pack->packsys_struc);

			if (!headerGetEntry(hd, RPMTAG_REQUIRENAME, &type, (void **) &require_name,
					    &require_name_count)) {
				require_name_count = 0;
			}
			
			g_message ("D: requirename count = %d", require_name_count);
			
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
							g_message ("D: skipped %s", isrequired->name);
							packagedata_destroy (isrequired, TRUE);
							continue;
						}		
						g_message ("D: ** %s requires %s", pack->name, isrequired->name);

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
								g_message ("D: skipped %s, others depend on it", 
									   isrequired->name);
								print_package_list ("BY", third_matches, FALSE);
								g_list_foreach (third_matches, 
										(GFunc)packagedata_destroy, 
										GINT_TO_POINTER (TRUE));
								packagedata_destroy (isrequired, TRUE);
							} else {
								g_message ("D: Also nuking %s", isrequired->name);
								tmp_requires = g_list_prepend (tmp_requires, 
											       isrequired);
							}
						}
					}
					g_list_free (second_matches);
				} else {
					g_message ("D: must lookup %s", require_name[j]);
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
	}

	if (tmp_requires) {
		eazel_uninstall_downward_traverse (service, &tmp_requires, failed, requires);
	}

	/* Now move the entries in tmp_requires into *requires */
	for (iterator = tmp_requires; iterator; iterator = g_list_next (iterator)) {
		(*requires) = g_list_prepend (*requires, iterator->data);
	}
	g_list_free (tmp_requires);

	g_message ("D: out eazel_uninstall_downward_traverse");
}

static void
eazel_uninstall_check_for_install (EazelInstall *service,
				   GList **packages,
				   GList **failed)
{
	GList *iterator;
	GList *remove  = NULL;
	GList *result = NULL;

	g_message ("D: in eazel_uninstall_check_for_install");
	g_assert (packages);
	g_message ("g_list_length (*packages) = %d", g_list_length (*packages)); 
	for (iterator = *packages; iterator; iterator = g_list_next (iterator)) {		
		PackageData *pack = (PackageData*)iterator->data;
		GList *matches;

		matches = eazel_install_simple_query (service, pack->name, EI_SIMPLE_QUERY_MATCHES, 0, NULL);
		/* If it's installed, continue */
		if (matches) {
			if (g_list_length (matches)==1) {
				PackageData *matched = (PackageData*)matches->data;
				g_message ("hest");
				/* This is mucho important. If not marked 
				   as toplevel, upwards traverse will not fail the package
				   is it has dependents */
				matched->toplevel = TRUE;

				g_message ("bæver %s", matched->name); 				
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

	g_message ("fisk");
	for (iterator = remove; iterator; iterator=iterator->next) {
		(*packages) = g_list_remove (*packages, iterator->data);
		(*failed) = g_list_prepend (*failed, iterator->data);
	}
	g_message ("torsk");
	g_message ("g_list_length (*packages) = %d", g_list_length (*packages)); 
	g_message ("g_list_length (result) = %d", g_list_length (result)); 
	g_list_foreach (*packages, (GFunc)packagedata_destroy, FALSE);
	g_list_free (remove);
	g_message ("odder");
	g_list_free (*packages);
	g_message ("sild");
	(*packages) = g_list_copy (result);
	g_message ("hund");
	g_list_free (result);

	g_message ("D: out eazel_uninstall_check_for_install");
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

	g_message ("D: in eazel_uninstall_globber");

	tmp = NULL;

	eazel_uninstall_check_for_install (service, packages, failed);
	for (iterator = *failed; iterator; iterator = g_list_next (iterator)) {
		g_message ("D: not installed %s", ((PackageData*)iterator->data)->name);
		eazel_install_emit_uninstall_failed (service, (PackageData*)iterator->data);
	}
		
	if (*packages) {
		eazel_uninstall_upward_traverse (service, packages, failed, &tmp);
		print_package_list ("FAILED", *failed, TRUE);
		for (iterator = *failed; iterator; iterator = g_list_next (iterator)) {
			g_message ("D: failed %s", ((PackageData*)iterator->data)->name);
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

	g_message ("D: out eazel_uninstall_glob");
}
