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

typedef void (*rpm_install_cb)(char* name, char* group, void* user_data);

static gboolean download_all_packages (EazelInstall *service,
                                       GList* categories);

static gboolean install_all_packages (EazelInstall *service,
                                      GList* categories);

typedef void* (*RpmCallbackFunc) (const Header, const rpmCallbackType, 
				  const unsigned long, const unsigned long, 
				  const void*, void*);

static int do_rpm_transaction (EazelInstall *service, 
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

gboolean eazel_install_prepare_package_system (EazelInstall *service);

gboolean eazel_install_free_package_system (EazelInstall *service);

static int eazel_install_check_existing_packages (EazelInstall *service, 
						  PackageData *pack);

gboolean
install_new_packages (EazelInstall *service, GList *categories) {

	gboolean rv;
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
		g_message (_("Reading the install package list ..."));
		categories = parse_local_xml_package_list (eazel_install_get_package_list (service));
	}

	if (categories == NULL) {
		rv = FALSE;
	} else {
		rv = download_all_packages (service, categories);
		rv = install_all_packages (service,
					   categories);
	}
	
	return rv;
} /* end install_new_packages */

/*
  Returns FALSE if some packages failed;
 */
static gboolean
download_all_packages (EazelInstall *service,
                       GList* categories) 
{
	gboolean result;

	result = TRUE;
	while (categories) {
		CategoryData* cat;
		GList* pkgs;
		GList *remove, *iterator;
	
		remove = NULL;
		cat = categories->data;
		pkgs = cat->packages;

		g_message (_("Category = %s"), cat->name);
		while (pkgs) {
			PackageData* package;
			gboolean fetch_package;

			package = pkgs->data;
			fetch_package = TRUE;

			/* if filename in the package is set, but the file
			   does not exist, get it anyway */
			if (package->filename) {
				g_message ("Filename set, and file exists = %d", 
					   g_file_test (package->filename, G_FILE_TEST_ISFILE));
				if (g_file_test (package->filename, G_FILE_TEST_ISFILE)) {
					fetch_package = FALSE;	
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

			} else {
				g_message ("Must download %s", package->name);
			}

			if (fetch_package &&
			    eazel_install_fetch_package (service, package) == FALSE) {
				g_warning (_("Failed to retreive %s!"), package->name);
				eazel_install_emit_download_failed (service, package->name);
				remove = g_list_prepend (remove, package); 
			}
			
			pkgs = pkgs->next;
		}
		
		for (iterator = remove; iterator; iterator = iterator->next) {
			cat->packages = g_list_remove (cat->packages, iterator->data);
		}

		categories = categories->next;
	}

	g_list_foreach (categories, (GFunc)categorydata_destroy_foreach, NULL);
	
	return result;
} /* end download_all_packages */

static gboolean
install_all_packages (EazelInstall *service,
                      GList* categories) {

	gboolean rv;
	rv = TRUE;

	while (categories) {
		CategoryData* cat;
		GList* packages;
		GList* failedfiles;
		GList* iterator;
		
		cat = categories->data;
		failedfiles = NULL;
		packages = NULL;

		/* Check for existing installed packages */
		for (iterator = cat->packages; iterator; iterator = iterator->next) {
			PackageData *pack;
			int inst_status;

			pack = (PackageData*)iterator->data;
			inst_status = eazel_install_check_existing_packages (service, pack);		      
			g_message ("inst status = %d", inst_status);
			/* If in force mode, install it under all circumstances.
			   if not, only install if not already installed in same
			   version or up/downgrade is set */
			if (eazel_install_get_force (service) ||
			    (eazel_install_get_downgrade (service) && inst_status == -1) ||
			    (eazel_install_get_update (service) && inst_status == 1) ||
			    inst_status == 2) {
				packages = g_list_prepend (packages, pack);
			} else {
				g_message ("Skipping %s...", pack->name);
				pack->status = PACKAGE_ALREADY_INSTALLED;				
				eazel_install_emit_install_failed (service, pack);
			}
		}

		if (packages) {
			if (eazel_install_prepare_package_system (service) == FALSE) {
				return FALSE;
			}
			eazel_install_ensure_deps (service, &packages, &failedfiles);
			eazel_install_free_package_system (service);

			g_message (_("Category = %s, %d packages"), cat->name, g_list_length (packages));
			do_rpm_transaction (service,
					    packages);
			
			g_list_free (packages);
		}

		categories = categories->next;
	}

	g_list_foreach (categories, (GFunc)categorydata_destroy_foreach, NULL);
	
	return rv;
} /* end install_all_packages */

static gboolean
uninstall_all_packages (EazelInstall *service,
			GList *categories) 
{
	gboolean rv;
	rv = TRUE;
	while (categories) {
		CategoryData* cat = categories->data;
		GList *failed;

		g_message (_("Category = %s"), cat->name);

		failed = NULL;
		eazel_uninstall_globber (service, &cat->packages, &failed);

		do_rpm_transaction (service, 
				    cat->packages);

		categories = categories->next;
	}
	return rv;
}

gboolean 
uninstall_packages (EazelInstall *service,
		    GList* categories) 
{
	gboolean rv;
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

	rv = TRUE;

	rv = uninstall_all_packages (service, categories);
/*
	g_list_foreach (categories, (GFunc)categorydata_destroy_foreach, NULL);
*/
	return rv;

} /* end install_new_packages */


GList *
ei_get_packages_with_mod_flag (GList *packages,
			       PackageModification mod) 
{
	GList *it;
	GList *res;
	
	res = NULL;
	for (it = packages; it; it = it->next) {
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
	for (it = *inst; it; it = it->next) {
		GList *entry;
		PackageData *pack;

		pack = (PackageData*)it->data;
		entry = g_list_find_custom (*down, pack->name, (GCompareFunc)eazel_install_package_name_compare);
		if (entry != NULL) {
			remove = g_list_prepend (remove, it->data);
		}
	}

	for (it = remove; it; it = it->next) {
		(*inst) = g_list_remove (*inst, it->data);
	}
}

void hest (PackageData *pack, char *str) {
	g_message ("Must %s %s", str, pack->name);
}

gboolean 
revert_transaction (EazelInstall *service, 
		    GList *packages)
{
	GList *uninst, *inst, *upgrade, *downgrade;
	CategoryData *cat;
	GList *categories;

	uninst = ei_get_packages_with_mod_flag (packages, PACKAGE_MOD_INSTALLED);
	inst = ei_get_packages_with_mod_flag (packages, PACKAGE_MOD_UNINSTALLED);
	upgrade = ei_get_packages_with_mod_flag (packages, PACKAGE_MOD_DOWNGRADED);
	downgrade = ei_get_packages_with_mod_flag (packages, PACKAGE_MOD_UPGRADED);

	ei_check_uninst_vs_downgrade (&uninst, &downgrade);

	g_list_foreach (uninst, (GFunc)hest, "uninstall");
	g_list_foreach (inst, (GFunc)hest, "install");
	g_list_foreach (downgrade, (GFunc)hest, "downgrade");
	g_list_foreach (upgrade, (GFunc)hest, "upgrade");

	cat = g_new0 (CategoryData, 1);
	categories = g_list_prepend (NULL, cat);
	if (uninst) {
		eazel_install_set_uninstall (service, TRUE);
		eazel_install_set_downgrade (service, FALSE);
		eazel_install_set_update (service, FALSE);
		cat->packages = uninst;
		uninstall_packages (service, categories);
	}
	if (inst) {
		eazel_install_set_uninstall (service, FALSE);
		eazel_install_set_downgrade (service, FALSE);
		eazel_install_set_update (service, FALSE);
		cat->packages = inst;
		install_new_packages (service, categories);
	}
	if (downgrade) {
		eazel_install_set_uninstall (service, FALSE);
		eazel_install_set_downgrade (service, TRUE);
		eazel_install_set_update (service, FALSE);
		cat->packages = downgrade;
		install_new_packages (service, categories);
	}
	if (upgrade) {
		eazel_install_set_uninstall (service, FALSE);
		eazel_install_set_downgrade (service, TRUE);
		eazel_install_set_update (service, TRUE);
		cat->packages = upgrade;
		install_new_packages (service, categories);
	}
}

static void
eazel_install_do_rpm_transaction_fill_hash (EazelInstall *service,
					GList *packages)
{
	GList *iterator;
	for (iterator = packages; iterator; iterator = iterator->next) {
		char *tmp;		
		PackageData *pack;

		pack = (PackageData*)iterator->data;
		tmp = g_strdup_printf ("%s-%s-%s", pack->name, pack->version, pack->minor);
		g_hash_table_insert (service->private->name_to_package_hash,
				     tmp,
				     iterator->data);
	}
}

static void
eazel_install_do_rpm_transaction_get_total_size (EazelInstall *service,
					     GList *packages)
{
	GList *iterator;
	for (iterator = packages; iterator; iterator = iterator->next) {
		PackageData *pack;

		pack = (PackageData*)iterator->data;
		service->private->packsys.rpm.total_size += pack->bytesize;
	}
}

static GList*
eazel_install_do_rpm_transaction_make_argument_list (EazelInstall *service,
						     GList *packages)
{
	GList *args;
	GList *iterator;

	args = NULL;

	/* Add the packages to the arg list */
	for (iterator = packages; iterator; iterator = iterator->next) {
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
	}
	
	/* Set the RPM parameters */
	if (eazel_install_get_test (service)) {
		g_message ("Dry run mode!");
		args = g_list_prepend (args, g_strdup ("--test"));
	} 
	if (eazel_install_get_force (service)) {
		g_warning ("Force install mode!");
		args = g_list_prepend (args, g_strdup ("--force"));
		args = g_list_prepend (args, g_strdup ("--nodeps"));
	} 
	if (eazel_install_get_uninstall (service)) {
		args = g_list_prepend (args, g_strdup ("-ev"));
	} else 	if (eazel_install_get_update (service)) {
		args = g_list_prepend (args, g_strdup ("--percent"));
		args = g_list_prepend (args, g_strdup ("-Uv"));
	} else if (eazel_install_get_downgrade (service)) {
		args = g_list_prepend (args, g_strdup ("--percent"));
		args = g_list_prepend (args, g_strdup ("--oldpackage"));
		args = g_list_prepend (args, g_strdup ("-Uv"));
	} else {
		args = g_list_prepend (args, g_strdup ("--percent"));
		args = g_list_prepend (args, g_strdup ("-iv"));
	}

	return args;
}

static void
eazel_install_do_rpm_transaction_process_pipe (EazelInstall *service,
					       int fd) 
{
	char *tmp;
	FILE *pipe;
	PackageData *pack;
	
	/* Get the helpers stdout */
	pipe = fdopen (fd, "r");
	fflush (pipe);
	tmp = g_new0 (char, 1024);
	pack = NULL;
	
	/* while something there... */
	while (!feof (pipe)) {
		fflush (pipe);
		fgets (tmp, 1023, pipe);
		if (feof (pipe)) {
			break;
		}

		if (tmp) {
			g_message ("READ \"%s\"", tmp);
			/* Percentage output, parse and emit... */
			if (tmp[0]=='%' && tmp[1]=='%') {
				char *ptr;
				int pct;
				int amount;


				if (pack == NULL) {
					continue;
				}
				ptr = tmp + 3;
				pct = strtol (ptr, NULL, 10);
				if (pct == 100) {
					amount = pack->bytesize;
				} else {
					amount = (pack->bytesize * pct) / 100;
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
						service->private->transaction = g_list_prepend (service->private->transaction,
												pack);
					}
					pack = NULL;
				}
			}  else if (strlen (tmp)) {
					/* Not percantage mark, that means filename, step ahead one file */
				tmp [ strlen (tmp) - 1] = 0;
				pack = g_hash_table_lookup (service->private->name_to_package_hash, tmp);
				if (pack==NULL) {						
					g_warning ("lookup \"%s\" failed", tmp);
				} else {
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
	}

	fclose (pipe);
}

#ifndef EAZEL_INSTALL_SLIM
static void
eazel_install_do_rpm_transaction_save_report_helper (xmlNodePtr node,
						     GList *packages)
{
	GList *iterator;

	for (iterator = packages; iterator; iterator = iterator->next) {
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
			eazel_install_do_rpm_transaction_save_report_helper (node, pack->modifies);
		}
	}
}

static void
eazel_install_do_rpm_transaction_save_report (EazelInstall *service) 
{
	FILE *outfile;
	xmlDocPtr doc;
	xmlNodePtr node, root;
	char *name;
	
	/* Ensure the transaction dir is present */
	if (! g_file_test (eazel_install_get_transaction_dir (service), G_FILE_TEST_ISDIR)) {
		int retval;
		retval = mkdir (eazel_install_get_transaction_dir (service), 0755);		       
		if (retval < 0) {
			if (errno != EEXIST) {
				g_warning (_("*** Could not create transaction directory (%s)! ***\n"), 
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
		name = g_strdup_printf ("%s/transaction.%d", eazel_install_get_transaction_dir (service), time (NULL));
	}

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

	eazel_install_do_rpm_transaction_save_report_helper (node, service->private->transaction);

	xmlDocDump (outfile, doc);
	
	fclose (outfile);
	g_free (name);}
#endif /* EAZEL_INSTALL_SLIM */

int
do_rpm_transaction (EazelInstall *service,
		    GList* packages) 
{
#ifndef EAZEL_INSTALL_SLIM
	TrilobiteRootHelper *root_helper;
#endif /* EAZEL_INSTALL_SLIM */
	GList *args;
	int fd;
	int res;

	if (g_list_length (packages) == 0) {
		return 0;
	}
		
	service->private->packsys.rpm.packages_installed = 0;
	service->private->packsys.rpm.num_packages = g_list_length (packages);
	service->private->packsys.rpm.current_installed_size = 0;
	service->private->packsys.rpm.total_size = 0;

	args = NULL;
	res = 0;

	eazel_install_do_rpm_transaction_fill_hash (service, packages);
	eazel_install_do_rpm_transaction_get_total_size (service, packages);
	args = eazel_install_do_rpm_transaction_make_argument_list (service, packages);

	eazel_install_emit_preflight_check (service, 					     
					    service->private->packsys.rpm.total_size,
					    service->private->packsys.rpm.num_packages);

	{
		GList *iterator;
		fprintf (stdout, "\nARGS: ");
		for (iterator = args; iterator; iterator = iterator->next) {
			fprintf (stdout, "%s ", (char*)iterator->data);
		}
		fprintf (stdout, "\n");
	}

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
		for (iterator = args; iterator; iterator = iterator->next) {
			argv[i] = g_strdup (iterator->data);
			i++;
		}
		argv[i] = NULL;

		if (access ("/bin/rpm", R_OK|X_OK)!=0) {
			g_warning ("/bin/rpm missing or not executable for uid");
			res = service->private->packsys.rpm.num_packages;
		} 
		/* start /bin/rpm... */
		if (res==0 && trilobite_pexec ("/bin/rpm", argv, NULL, &fd, &useless_stderr)!=0) {
			g_warning ("Could not start rpm");
			res = service->private->packsys.rpm.num_packages;
		} else {
			g_message ("rpm running...");
		}
	}
#else /* EAZEL_INSTALL_SLIM     */
	/* Fire off the helper */	
	root_helper = gtk_object_get_data (GTK_OBJECT (service), "trilobite-root-helper");
	if (trilobite_root_helper_start (root_helper) != TRILOBITE_ROOT_HELPER_SUCCESS) {
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
		eazel_install_do_rpm_transaction_process_pipe (service, fd);
#ifndef EAZEL_INSTALL_SLIM
		eazel_install_do_rpm_transaction_save_report (service);
#endif /* EAZEL_INSTALL_SLIM     */
	}

	g_list_foreach (args, (GFunc)g_free, NULL);
	g_list_free (args);

	return res;
} /* end do_rpm_transaction */

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
	g_message (_("Removing package %s"), pack->name);
	if (pack->toplevel) {
		/* We only emit signal for the toplevel packages, 
		   and only delete them. They _destroy function destroys
		   the entire dep tree */
		eazel_install_emit_install_failed (service, pack);
	}
	(*pruned) = g_list_prepend (*pruned, pack);
	for (iterator = pack->soft_depends; iterator; iterator = iterator->next) {
		PackageData *sub;
		sub = (PackageData*)iterator->data;
		eazel_install_prune_packages_helper (service, packages, pruned, sub);
	}
	for (iterator = *packages; iterator; iterator = iterator->next) {
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
			      GList **packages_arg, ...)
{
	va_list ap;
	GList *pruned;
	GList *iterator;
	GList **packages;

	g_return_if_fail (pack!=NULL);

	va_start (ap, packages_arg);
	
	packages = packages_arg;
	pruned = NULL;
	do {
		eazel_install_prune_packages_helper (service,
						     packages,
						     &pruned,
						     pack);
		for (iterator = pruned; iterator; iterator = iterator->next) {
			PackageData *pack;
			pack = (PackageData*)iterator->data;
			(*packages) = g_list_remove (*packages, pack);
	};
	} while ( (packages = va_arg (ap, GList **)) != NULL);

	for (iterator = pruned; iterator; iterator = iterator->next) {
		PackageData *pack;
		pack = (PackageData*)iterator->data;
		packagedata_destroy (pack);
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
	
	for (iterator = *packages; iterator; iterator = iterator->next) {
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
	for (iterator = sources; iterator; iterator = iterator->next) {
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

static gboolean
eazel_install_free_rpm_system (EazelInstall *service)
{
	rpmdb *db;
	rpmTransactionSet *set;

	db = &(service->private->packsys.rpm.db);
	set = &(service->private->packsys.rpm.set);

	if (*db) {
		rpmdbClose (*db);
		(*db) = NULL;
	}
/*
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
	const char *root_dir;
	rpmdb *db;
	rpmTransactionSet *set;

	db = &(service->private->packsys.rpm.db);
	set = &(service->private->packsys.rpm.set);

	rpmReadConfigFiles (eazel_install_get_rpmrc_file (service), NULL);

	root_dir = eazel_install_get_root_dir (service);

	if (rpmdbOpen (root_dir, db, O_RDONLY, 0644)) {
		const char* dn;
		dn = rpmGetPath (root_dir, "%{_dbpath}", NULL);
		if (!dn) {
			g_warning (_("RPM package database query failed !"));
		}
		return FALSE;
	}

	return TRUE;
}

gboolean
eazel_install_prepare_package_system (EazelInstall *service)
{
	gboolean result;

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

	for (iterator = *packages; iterator; iterator = iterator->next) {
		PackageData *pack;
		int err;

		pack = (PackageData*)iterator->data;

		if (!eazel_install_get_uninstall (service)) {
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
		} 
	} /* end for loop */

	/* Remove all failed from packages, and add them to failed */
	if (tmp_failed) {
		for (iterator = tmp_failed; iterator; iterator = iterator->next) {
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
	for (iterator = list; iterator; iterator = iterator->next) {
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
	existing_packages = eazel_install_simple_query (service, pack->name, EI_SIMPLE_QUERY_MATCHES, 0, NULL);
	if (existing_packages) {
		/* Get the existing package, set it's modify flag and add it */
		GList *existing_iterator;
		for (existing_iterator = existing_packages; existing_iterator; existing_iterator = existing_iterator->next) {
			int res;
			PackageData *existing_package;

			existing_package = (PackageData*)existing_iterator->data;			
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
		
			g_message ("%s %sgrades from %s to %s", pack->name, result != 0 ? (result>0 ? "up" : "down") : "", existing_package->version, pack->version);
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
	GList *to_remove, *remove_iterator;
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

		pack_entry = g_list_find_custom (*packages, 
						 (gpointer)&conflict,
						 (GCompareFunc)eazel_install_package_conflict_compare);

		if (pack_entry == NULL) {
			switch (conflict.sense) {
			case RPMDEP_SENSE_REQUIRES: {
				char *tmp;
				
				g_warning (_("%s %s breaks %s"), conflict.needsName, conflict.needsVersion, conflict.byName);
				pack_entry = g_list_find_custom (*packages, 
								 (gpointer)conflict.needsName,
								 (GCompareFunc)eazel_install_package_name_compare);
				if (pack_entry==NULL) {
					/* FIXME: bugzilla.eazel.com
					   Argh, if a lib*.so breaks a package,
					   we end up here */
					continue;
				}
				dep = packagedata_new_from_rpm_conflict_reversed (conflict);
				pack = (PackageData*)(pack_entry->data);
				dep->archtype = g_strdup (pack->archtype);
				pack->status = PACKAGE_BREAKS_DEPENDENCY;
				dep->status = PACKAGE_DEPENDENCY_FAIL;
				g_assert (dep!=NULL);
				
				/* Here I check to see if I'm breaking the -devel package, if so,
				   request it */

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
				g_warning (_("%s conflicts %s-%s"), conflict.byName, conflict.needsName, conflict.needsVersion);
				if (g_list_find (*failedpackages, pack) == NULL) {
					(*failedpackages) = g_list_prepend (*failedpackages, pack);
				}
				to_remove = g_list_remove (to_remove, pack);
				continue;
				break;
			}
		} else {
			/* Does the conflict look like a file dependency ? */
			pack = (PackageData*)pack_entry->data;
			if (*conflict.needsName=='/' || strstr (conflict.needsName, ".so")) {
				g_message (_("Processing dep for %s : requires %s"), pack->name, conflict.needsName);		
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
			fetch_result = eazel_install_fetch_package_which_provides (service, conflict.needsName, &dep);
		} else {
			fetch_result = eazel_install_fetch_package (service, dep);
		}

		if (fetch_result) {
			/* if it succeeds, add to a list of extras for this package 
			   We cannot just put it into extrapackages, as a later dep
			   might fail, and then we have to fail the package */
			GList *extralist;

			/* FIXME bugzilla.eazel.com:
			   Need to check that the downloaded package is of sufficiently high version
			*/

			/* This call sets the dep->modifies if there are already
			   packages installed of that name */
			eazel_install_check_existing_packages (service, dep);

			extralist = g_hash_table_lookup (extras, pack->name);
			extralist = g_list_append (extralist, dep);
			g_hash_table_insert (extras, pack->name, extralist);

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
			
			eazel_install_emit_download_failed (service, dep->name);				
			
			pack->status = PACKAGE_DEPENDENCY_FAIL;
			dep->status = PACKAGE_CANNOT_OPEN;
			
			if (!eazel_install_get_force (service)) {
				extralist = g_hash_table_lookup (extras, pack->name);
				g_list_foreach (extralist, (GFunc)packagedata_destroy_foreach, NULL);
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

	/* Removed packages marked as failed. No need to delete them, as they're in
	   (*failedpackages) */
	for (remove_iterator = to_remove; remove_iterator; remove_iterator = remove_iterator->next) {
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
	char *tmp;
	char *dep = " depends on ";
	char *breaks = " breaks ";

	g_message ("---------------------------");
	g_message (str);
	for (iterator = packages; iterator; iterator = iterator->next) {
		pack = (PackageData*)iterator->data;
		if (show_deps) {
			GList *it2;
			tmp = g_strdup (dep);
			it2 = pack->soft_depends;
			while (it2) { 
				char *tmp2;
				tmp2 = g_strdup_printf ("%s%s ", tmp ,rpmfilename_from_packagedata ((PackageData*)it2->data));
				g_free (tmp);
				tmp = tmp2;
				it2 = it2->next;
			}
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
				it2 = it2->next;
			}
		}
		g_message ("* %s (%s) %s", 
			   rpmfilename_from_packagedata (pack), 
			   pack->toplevel ? "true" : "", 
			   (strlen (tmp) > strlen (dep)) ? tmp : "");
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
  for them all, if deps fail, fetch the depency and add to outfiles.
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
/*
		rpmTransactionSet *set;
		struct rpmDependencyConflict **conflicts;
*/
		service->private->packsys.rpm.set = rpmtransCreateSet (service->private->packsys.rpm.db, "/");
		eazel_install_load_headers (service, packages);
		eazel_install_add_to_set (service, packages, failedpackages);
		num_conflicts = eazel_install_do_dependency_check (service);		
		/* rpmtransFree (service->private.packsys.rpm.set); */

		if (num_conflicts != 0) {
			GList *extrapackages;
			GList *iterator;

			extrapackages = NULL;

			/* For all the packages, set state to partly_resolved. */
			for (iterator=*packages; iterator; iterator = iterator->next) {
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
				
				for (iterator = *failedpackages; iterator; iterator = iterator->next) {
					PackageData *pack;
					pack = (PackageData*)iterator->data;
					eazel_install_prune_packages (service, pack, packages, &extrapackages, NULL);
				}			
			} 
			if (extrapackages) {
				GList *iterator;

				/* Add to "packages" */
				for (iterator = extrapackages; iterator; iterator = iterator->next) {
					(*packages) = g_list_prepend (*packages, iterator->data);
				}
				
				/* Now recurse into eazel_install_ensure_deps with
				   the new "packages" list */
				eazel_install_ensure_deps (service, packages, failedpackages);

				/* Now remove the packages that failed from "packages" 
				   and copy them into "failedpackages".  */
				for (iterator = *failedpackages; iterator; iterator = iterator->next) {
					PackageData *pack;
					pack = (PackageData*)iterator->data;
					(*packages) = g_list_remove (*packages, pack);					
				}
			}
		} else {
			GList *iterator;

			/* Deps are fine, set all packages to resolved */
			for (iterator=*packages; iterator; iterator = iterator->next) {
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

	g_message ("in eazel_uninstall_upward_traverse");

	g_assert (packages!=NULL);
	g_assert (*packages!=NULL);
	g_assert (breaks!=NULL);
	g_assert (*breaks==NULL);

	/* Open the package system */

	service->private->packsys.rpm.set = rpmtransCreateSet (service->private->packsys.rpm.db, "/");
	/* Add all packages to the set */
	/* eazel_install_add_to_set (service, packages, failed); */

	for (iterator = *packages; iterator; iterator = iterator->next) {
		PackageData *pack;
		dbiIndexSet matches;
		int rc;
		rpmdb db;

		db = service->private->packsys.rpm.db;
		
		pack = (PackageData*)iterator->data;
		g_message ("checking reqs by %s (0x%x)", rpmname_from_packagedata (pack), pack);
		/* FIXME bugzilla.eazel.com 1697:
		   use eazel_install_simple_query instead */
		rc = rpmdbFindByRequiredBy (db, pack->name, &matches);
		if (!rc) {
			/* Hits */
			int i;
			GList *tmp_breaks;
			GList *break_iterator;

			tmp_breaks = NULL;
			for (i = 0; i < dbiIndexSetCount(matches); i++) {				
				unsigned int offset;
				Header *hd;
				PackageData *requiredby;

				offset = dbiIndexRecordOffset (matches, i);
				hd = g_new0 (Header,1);
				(*hd) = rpmdbGetRecord (db, offset);
				requiredby = packagedata_new_from_rpm_header (hd);
				requiredby->status = PACKAGE_DEPENDENCY_FAIL;
				pack->status = PACKAGE_BREAKS_DEPENDENCY;
				g_message ("%s (0x%x) is required by %s (0x%x)", pack->name, pack, requiredby->name, requiredby);
				if (g_list_find_custom (*breaks, (gpointer)requiredby->name, 
							 (GCompareFunc)eazel_install_package_name_compare) ||
				    g_list_find_custom (*packages, (gpointer)requiredby->name, 
							(GCompareFunc)eazel_install_package_name_compare)) {
					g_message ("skip %s", requiredby->name);
					packagedata_destroy (requiredby);
					continue;
				}
				pack->breaks = g_list_prepend (pack->breaks, requiredby);
				/* If the package has not been failed yet (and is a toplevel),
				   fail it */
				if (!g_list_find_custom (*failed, (gpointer)pack->name, 
							 (GCompareFunc)eazel_install_package_name_compare) &&
				    pack->toplevel) {
					(*failed) = g_list_prepend (*failed, pack);
				}
				(*breaks) = g_list_prepend ((*breaks), requiredby);
			}

			dbiFreeIndexRecord (matches);
			
			/* rpmtransFree (service->private->packsys.rpm.set); */
			if (*breaks) {
				eazel_uninstall_upward_traverse (service, breaks, failed, &tmp_breaks);
			}
			
			for (break_iterator = tmp_breaks; break_iterator; break_iterator = break_iterator->next) {
				(*breaks) = g_list_prepend ((*breaks), break_iterator->data);
			}
		} else {
                        /* No hits/error */
			/* rpmtransFree (service->private->packsys.rpm.set); */
		}
	}
	
	for (iterator = *failed; iterator; iterator = iterator->next) {
		(*packages) = g_list_remove (*packages, iterator->data);
	}

	g_message ("out eazel_uninstall_upward_traverse");
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
	rpmdb db;

	/* 
	   create set
	   find all requirements from "packages"
	   add all packs + requirements from "packages" to set
	   dep check
	   for all breaks, remove from requirements
	   recurse calling eazel_uninstall_downward_traverse (requirements, &tmp)
	   add all from tmp to requirements
	*/
	g_message ("in eazel_uninstall_downward_traverse");
	db = service->private->packsys.rpm.db;
	
	/* First iterate across the packages in "packages" */
	for (iterator = *packages; iterator; iterator = iterator->next) {
		dbiIndexSet matches;
		PackageData *pack;
		int i;
		int rc;

		pack = (PackageData*)iterator->data;
		/* FIXME bugzilla.eazel.com 1697:
		   use eazel_install_simple_query instead */
		rc = rpmdbFindPackage (db,
				       pack->name,
				       &matches);

		/* Now iterate over all packages that match pack->name */
		for (i = 0; i < dbiIndexSetCount (matches); i++) {
			const char **require_name;
			int require_name_count;
			Header hd;
			unsigned int offset;
			int type;
			int j;

			offset = dbiIndexRecordOffset (matches, i);
			hd = rpmdbGetRecord (db, offset);

			if (!headerGetEntry(hd, RPMTAG_REQUIRENAME, &type, (void **) &require_name,
					    &require_name_count)) {
				require_name_count = 0;
			}
			
			/* No iterate over all packages required by the current package */
			for (j = 0; j < require_name_count; j++) {
				if ((*require_name[j] != '/') &&
				    !strstr (require_name[j], ".so")) {
					dbiIndexSet secondmatches;
					int k;
					Header *hd2;
					PackageData *isrequired;
					
					/* FIXME bugzilla.eazel.com 1697:
					   use eazel_install_simple_query instead */
					rc = rpmdbFindPackage (db, require_name[j], &secondmatches);
					/* Iterate over all packages that match the required package */
					for (k = 0; k < dbiIndexSetCount (secondmatches); k++) {
						unsigned int offset2;
						offset2 = dbiIndexRecordOffset (secondmatches, k);
						
						hd2 = g_new0 (Header, 1);
						(*hd2) = rpmdbGetRecord (db, offset2);
						isrequired = packagedata_new_from_rpm_header (hd2);
						if (g_list_find_custom (*requires, isrequired->name,
									(GCompareFunc)eazel_install_package_name_compare) ||
						    g_list_find_custom (*packages, isrequired->name,
									(GCompareFunc)eazel_install_package_name_compare)) {
							g_message ("skipped %s", isrequired->name);
							packagedata_destroy (isrequired);
							continue;
						}		
						g_message ("** %s requires %s", pack->name, isrequired->name);

						{
							dbiIndexSet thirdmatches;
							int l;
							/* FIXME bugzilla.eazel.com 1697:
							   use eazel_install_simple_query instead */
							rc = rpmdbFindByRequiredBy (db, isrequired->name, &thirdmatches);
						}
						/* FIXME bugzilla.eazel.com 1539:
						   check noone outside of "packages" & "requires" require
						   this before adding */
						(*requires) = g_list_prepend (*requires, isrequired);
					}
					dbiFreeIndexRecord (secondmatches);
				} else {
					/* FIXME bugzilla.eazel.com 1542:
					   Need the ability to lookup a pacakge that provides a file
					   and process that */
					g_message ("must lookup %s", require_name[j]);
					/* FIXME bugzilla.eazel.com 1539:
					   lookup package "p" that provides requires[j],
					   if packages that that require "p" are not in "packages"
					   don't add it, otherwise add to requires */
				}
			}

			headerFree (hd);
		}
		dbiFreeIndexRecord (matches);
	}

	g_message ("out eazel_uninstall_downward_traverse");
}

/* Calls the upward and downward traversal */
static void
eazel_uninstall_globber (EazelInstall *service,
			 GList **packages,
			 GList **failed)
{
	GList *iterator;
	GList *tmp;
	gboolean close_base;
	/*
	  call upward with packages
	  call downward with packages and &tmp
	  add all from &tmp to packages
	*/

	g_message ("in eazel_uninstall_globber");

	if (eazel_install_prepare_package_system (service) == FALSE) {
		close_base = FALSE;
		for (iterator = *packages; iterator; iterator = iterator->next) {
			(*failed) = g_list_prepend (*failed, iterator->data);
		}
	} else {
		close_base = TRUE;
		tmp = NULL;
		eazel_uninstall_upward_traverse (service, packages, failed, &tmp);
		for (iterator = tmp; iterator; iterator = iterator->next) {
			g_message ("breaks %s", ((PackageData*)iterator->data)->name);
		}
	}
	print_package_list ("FAILED", *failed, TRUE);
	for (iterator = *failed; iterator; iterator = iterator->next) {
		g_message ("failed %s", ((PackageData*)iterator->data)->name);
		eazel_install_emit_uninstall_failed (service, (PackageData*)iterator->data);
	}
	g_list_free (tmp);
/*
  I've disabled downwards traverse untill it's done.
	tmp = NULL;
	eazel_uninstall_downward_traverse (service, packages, failed, &tmp);
	for (iterator = tmp; iterator; iterator = iterator->next) {
		g_message ("also doing %s", ((PackageData*)iterator->data)->name);
		(*packages) = g_list_prepend (*packages, iterator->data);
	}
	g_list_free (tmp);
*/
	if (close_base) {
		eazel_install_free_package_system (service);
	}

	g_message ("out eazel_uninstall_globber");
}
