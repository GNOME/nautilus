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

#include <libtrilobite/helixcode-utils.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmmacro.h>
#include <rpm/dbindex.h>
#include <string.h>
#include <time.h>

#define UNKNOWN_SIZE 1024

typedef void (*rpm_install_cb)(char* name, char* group, void* user_data);

static gboolean download_all_packages (EazelInstall *service,
                                       GList* categories);

static gboolean install_all_packages (EazelInstall *service,
                                      GList* categories);


static gboolean uninstall_a_package (EazelInstall *service,
				     PackageData* package,
				     int uninstall_flags,
				     int problem_filters,
				     int interface_flags);

static void* rpm_show_progress (const Header h,
                                const rpmCallbackType callback_type,
                                const unsigned long amount,
                                const unsigned long total,
                                const void* pkgKey,
                                void* service);

static int rpm_uninstall (EazelInstall *service, 
			  char* root_dir,
                          char* file,
                          int install_flags,
                          int problem_filters,
                          int interface_flags);

static int do_rpm_install (EazelInstall *service, 
			   char* root_dir,
                           GList* packages,
                           char* location,
                           rpm_install_cb callback,
                           void* user_data);

static int do_rpm_uninstall (EazelInstall *service, 
			     char* root_dir,
                             GList* packages,
                             int install_flags,
                             int problem_filters,
                             int interface_flags);

static gboolean eazel_install_ensure_deps (EazelInstall *service, 
					   GList **filenames, 
					   GList **fails);

static int eazel_install_package_name_compare (PackageData *pack, 
					       char *name);

static int eazel_install_package_conflict_compare (PackageData *pack,
						   struct rpmDependencyConflict *conflict);

static void eazel_uninstall_globber (EazelInstall *service,
				     GList **packages,
				     GList **failed);

static gboolean eazel_install_prepare_package_system (EazelInstall *service);

static gboolean eazel_install_free_package_system (EazelInstall *service);

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
	
	rpmReadConfigFiles (eazel_install_get_rpmrc_file (service), NULL);

	if (categories == NULL) {
		g_message (_("Reading the install package list ..."));
		categories = parse_local_xml_package_list (eazel_install_get_package_list (service));
	}

	if (eazel_install_prepare_package_system (service) == FALSE) {
		return FALSE;
	}

	if (categories == NULL) {
		rv = FALSE;
	} else {
		rv = download_all_packages (service, categories);
		rv = install_all_packages (service,
					   categories);
	}
	
	eazel_install_free_package_system (service); 
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

				if (g_file_test (package->filename, G_FILE_TEST_ISFILE)) {
					fetch_package = FALSE;		
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
		CategoryData* cat = categories->data;
		GList* pkgs = cat->packages;
		GList* newfiles;
		GList* failedfiles;
		
		newfiles = NULL;
		failedfiles = NULL;
		if (pkgs) {
			eazel_install_ensure_deps (service, &pkgs, &failedfiles);
		}
		
		g_message (_("Category = %s, %d packages"), cat->name, g_list_length (pkgs));

		do_rpm_install (service,
				"/",
				pkgs,
				NULL,
				NULL,
				NULL);
		categories = categories->next;
	}

	g_list_foreach (categories, (GFunc)categorydata_destroy_foreach, NULL);
	
	return rv;
} /* end install_all_packages */


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

	if (eazel_install_prepare_package_system (service) == FALSE) {
		return FALSE;
	}

	rv = TRUE;
	while (categories) {
		CategoryData* cat = categories->data;
		GList *pkgs;
		GList *failed;

		g_message (_("Category = %s"), cat->name);

		failed = NULL;
		eazel_uninstall_globber (service, &cat->packages, &failed);

		g_message ("g_list_length (failed) = %d", g_list_length (failed));
		pkgs = cat->packages;
		while (pkgs) {
			PackageData* package = pkgs->data;

			if (uninstall_a_package (service, 
						 package,
						 uninstall_flags,
						 problem_filters,
						 interface_flags) == FALSE) {
				g_warning ("Uninstall failed for %s",package->name);
				eazel_install_emit_uninstall_failed (service, package);
				rv = FALSE;
			}
			pkgs = pkgs->next;
		}
		categories = categories->next;
	}

	g_list_foreach (categories, (GFunc)categorydata_destroy_foreach, NULL);
	
	eazel_install_free_package_system (service); 
	return rv;

} /* end install_new_packages */

static gboolean
uninstall_a_package (EazelInstall *service,
		     PackageData* package,
                     int uninstall_flags,
                     int problem_filters,
                     int interface_flags) {
	int retval;
	gboolean rv;

	retval = 0;

	rv = TRUE;
	g_message (_("Uninstalling %s"), package->name);
	retval = rpm_uninstall (service,
				"/",
				package->name,
				uninstall_flags,
				problem_filters,
				interface_flags);
	if (retval == 0) {
		g_message ("Package uninstall successful!");
	} else {
		g_message (_("Package uninstall failed !"));
		rv = FALSE;
	}

	return rv;

} /* end uninstall_a_package */

static void*
rpm_show_progress (const Header h,
                   const rpmCallbackType callback_type,
                   const unsigned long amount,
                   const unsigned long total,
                   const void* pkgKey,
                   void* data) {

	static FD_t fd;
	int size;
	unsigned long* sizep;
	char* name;
	const char* filename;
	EazelInstall *service;

	g_assert (data != NULL);
	g_assert (IS_EAZEL_INSTALL (data));
	
	service = EAZEL_INSTALL (data);

	filename = pkgKey;

	switch (callback_type) {
	case RPMCALLBACK_INST_OPEN_FILE:
		name = "";
		size = 0;
		fd = fdOpen (filename, O_RDONLY, 0);
		headerGetEntry (h,
				RPMTAG_SIZE,
				NULL,
				(void **)&sizep,
				NULL);
		headerGetEntry (h,
				RPMTAG_NAME,
				NULL,
				(void **)&name,
				NULL);
		size = *sizep;

		return fd;

	case RPMCALLBACK_INST_CLOSE_FILE:
		fdClose (fd);
		break;
			
	case RPMCALLBACK_INST_PROGRESS: {
		/* FIXME: bugzilla.eazel.com 1585
		   Need a hash between filenames and packages, so I can lookup the
		   real package here */
		PackageData *pack;
		pack = packagedata_new ();
		pack->name = g_strdup (filename);		
		eazel_install_emit_install_progress (service, pack, amount, total);
		packagedata_destroy (pack);
	}
	break;

	default:
		break;
	}
	return NULL;
	} /* end rpm_show_progress */

int
do_rpm_install (EazelInstall *service,
		char* root_dir, 
		GList* packages, 
		char* location,
                rpm_install_cb cb, 
		void* user_data) {

	int mode;
	int rc;
	int i;
	int is_source;
        int stop_install;
        int pkg_count;
	int num_packages;
	int num_binary_packages;
	int num_source_packages;
	int num_failed;
	int num_conflicts;
	int total_size;
	char** pkgs;
	char* pkg_file;
	Header* binary_headers;
	rpmdb db;
	rpmTransactionSet rpmdep;
	rpmProblemSet probs;
	unsigned long* sizep;
	FD_t fd;
	struct rpmDependencyConflict* conflicts;
	GList *iterator;

	stop_install = 0;
	num_binary_packages = 0;
	num_source_packages = 0;
	num_failed = 0;
	num_packages = 0;
	total_size = 0;
	rpmdep = NULL;
	probs = NULL;


	if (eazel_install_get_install_flags (service) & RPMTRANS_FLAG_TEST) {
		mode = O_RDONLY;
	}
	else {
		mode = O_RDWR | O_CREAT;
	}

	pkg_count = g_list_length (packages);
	pkgs = g_new (char *, pkg_count + 1);
	binary_headers = g_new (Header, pkg_count + 1);
	/* First load all rpm headers */
	for (iterator = packages; iterator ; iterator = iterator->next) {
		/* RPM keeps this var (pkg_file), so don't use a g_ func to alloc it, 
		   nor should it be freed */
		/* FIXME: bugzilla.eazel.com 1602
		   I should remember these pointers and free them at the end, 
		   except that mayby RPM does, I'm just not sure. And anyways, being the service
		   "it's not that important" :-( */
		pkg_file = strdup (rpmfilename_from_packagedata ((PackageData*)iterator->data));
		g_message ("Installing %s...", pkg_file);
		fd = fdOpen (pkg_file, O_RDONLY, 0644);
		if (fdFileno (fd) < 0) {
			num_failed++;
			continue;
		}
		pkgs[num_packages++] = pkg_file;
		/* This loads the rpm header */
		rc = rpmReadPackageHeader (fd,
                                           &binary_headers[num_binary_packages],
                                           &is_source,
                                           NULL,
                                           NULL);
		fdClose (fd);

		/* Get the size tag */
		if (binary_headers[num_binary_packages]) {
			if (headerGetEntry (binary_headers[num_binary_packages],
                                            RPMTAG_SIZE, NULL,
                                            (void **) &sizep, NULL)) {
				total_size += *sizep;
			}
		}
		else {
			total_size += UNKNOWN_SIZE;
		}
		if (rc) {
			num_packages--;
			num_failed++;
		}
		else if (is_source) {
			if (binary_headers[num_binary_packages]) {
				num_source_packages++;
			}
		}
		else {
			num_binary_packages++;
		}
	}
	if (num_binary_packages) {
		/* Open the rpm db */
		if (rpmdbOpen (root_dir, &db, mode, 0644)) {
			for (i=0; i < num_binary_packages; i++) {
				headerFree(binary_headers[i]);
			}
			g_free (binary_headers);
			g_free (pkgs);
			return num_packages;
		}

		/* Create transaction set and add all
		   the headers to the set */
		rpmdep = rpmtransCreateSet (db, root_dir);
		for (i =0; i < num_binary_packages; i++) {
			rpmtransAddPackage (rpmdep,
                                            binary_headers[i],
                                            NULL,
                                            pkgs[i],
                                            eazel_install_get_interface_flags (service),
                                            NULL);
		}
		/* Ensure the order */
		if (!(eazel_install_get_interface_flags (service) & INSTALL_NOORDER)) {
			g_message ("Reordering...");
			if (rpmdepOrder(rpmdep)) {
				num_failed = num_packages;
				stop_install = 1;
			}
		}

		if (!(eazel_install_get_interface_flags (service) & INSTALL_NODEPS)) {
			/* Check the dependencies */
			rpmdepCheck (rpmdep, &conflicts, &num_conflicts);
			if (!stop_install && num_conflicts) {
				if (!eazel_install_get_force (service)) {
					PackageData *pack, *dep;
					GList *pack_entry;
					int cnt;
					
					for (cnt = 0; cnt < num_conflicts; cnt++) {
						pack_entry = g_list_find_custom (packages, 
										 (gpointer)&conflicts[cnt],
										 (GCompareFunc)eazel_install_package_conflict_compare);
						
						if (pack_entry) {
							pack = (PackageData*)(pack_entry->data);
							dep = packagedata_new_from_rpm_conflict (conflicts[cnt]);
							dep->status = PACKAGE_CANNOT_OPEN;
							pack->status = PACKAGE_DEPENDENCY_FAIL;
							g_assert (dep->name != NULL);
							pack->soft_depends = g_list_prepend (pack->soft_depends, dep);
							eazel_install_emit_install_failed (service, pack); 
						} else {
							g_assert_not_reached ();
						}
					}
					
					num_failed = num_packages;
					stop_install = 1;
				}

				rpmdepFreeConflicts (conflicts, num_conflicts);								
			}
		}
  	}
	else {
		db = NULL;
	}
	/* If all is good, do the install */
	if (num_binary_packages && !stop_install && rpmdep != NULL) {
		/* do the actual install */
		if (eazel_install_get_interface_flags (service) & INSTALL_UPGRADE) {
			 g_message (_("Upgrading..."));
		}
		else {
			g_message (_("Installing..."));
		}

		rc = rpmRunTransactions (rpmdep,
                                         rpm_show_progress,
                                         service,
                                         NULL,
                                         &probs,
                                         eazel_install_get_interface_flags (service),
                                         eazel_install_get_problem_filters (service));
		rpmProblemSetPrint (stderr, probs);
		rpmProblemSetPrint (stdout, probs);
		g_message ("rpmRunTransactions = %d", rc);
		rpmProblemSetFree(probs);
		if (rc == -1) {
			 /* some error */
			num_failed = num_packages;
		}
		else {
			num_failed += rc;
		}
	}
	
	/* Clean up */
	for (i = 0; i < num_binary_packages; i++) {
		headerFree (binary_headers[i]);
	}
	g_free (binary_headers);

	if (db) {
		rpmdbClose (db);
	}
	g_free(pkgs);
	if (rpmdep != NULL) {
		rpmtransFree (rpmdep);
	}

	g_message ("num_failed = %d", num_failed);

	return num_failed;
} /* end do_rpm_install */

static int
do_rpm_uninstall (EazelInstall *service, 
		  char* root_dir, 
		  GList* packages, 
		  int install_flags,
                  int problem_filters, 
		  int interface_flags) 
{
	rpmdb db;
	int count, i;
	int rc;
	int mode;
	int num_packages;
	int num_failed;
	int num_conflicts;
	int  stop_uninstall;
	char* pkg_name;
	dbiIndexSet matches;
	rpmTransactionSet rpmdep;
	struct rpmDependencyConflict* conflicts;
	rpmProblemSet probs;

	num_failed = 0;
	stop_uninstall = 0;
	rpmdep = NULL;

	if (install_flags & RPMTRANS_FLAG_TEST) {
		mode = O_RDONLY;
	}
	else {
		mode = O_RDWR | O_EXCL;
	}
	if (rpmdbOpen (root_dir, &db, mode, 0644)) {
		const char* dn;
		dn = rpmGetPath ((root_dir ? root_dir : ""), "%{_dbpath}", NULL);
		if (!dn) {
			g_warning (_("Packages database query failed !"));
		}
		return g_list_length (packages);
	}

	rpmdep = rpmtransCreateSet (db, root_dir);
	for (num_packages = 0; packages != NULL; packages = packages->next) {
		pkg_name = packages->data;
		rc = rpmdbFindByLabel (db, pkg_name, &matches);
		switch (rc) {
			case 1:
				g_message (_("Package %s is not installed"), pkg_name);
				num_failed++;
				break;
			case 2:
				g_message (_("Error finding index to %s"), pkg_name);
				num_failed++;
				break;
			default:
				count = 0;
				for (i = 0; i < dbiIndexSetCount (matches); i++) {
					unsigned int rec_offset;
					if (dbiIndexRecordOffset (matches, i)) {
						count++;
					}
					rec_offset = dbiIndexRecordOffset (matches, i);
					if (rec_offset) {
						rpmtransRemovePackage (rpmdep, rec_offset);
						num_packages++;
					}
				}
				dbiFreeIndexRecord (matches);
				break;
		}
	}

	if (!(interface_flags & UNINSTALL_NODEPS)) {

		if (rpmdepCheck (rpmdep, &conflicts, &num_conflicts)) {
			num_failed = num_packages;
			stop_uninstall = 1;
		}

		if (!stop_uninstall && conflicts) {
			int i;
			g_message (_("Dependency check failed."));
			printDepProblems (stderr, conflicts,
                                          num_conflicts);
			for (i=0; i<num_conflicts; i++) {
				switch (conflicts[i].sense) {
				case RPMDEP_SENSE_REQUIRES:
					g_message ("%s deps on %s and must also be uninstalled", conflicts[i].byName, conflicts[i].needsName);
					break;
				default:
					break;
				}
			}
			num_failed += num_packages;
			stop_uninstall = 1;
			rpmdepFreeConflicts (conflicts, num_conflicts);
		}
	}
	if (!stop_uninstall && rpmdep != NULL) {
		num_failed += rpmRunTransactions (rpmdep,
						  rpm_show_progress,
						  service,
                                                  NULL,
                                                  &probs,
                                                  install_flags,
                                                  problem_filters);
	}

	if (rpmdep != NULL) {
		rpmtransFree (rpmdep);
	}

	rpmdbClose (db);
	return num_failed;
} /* end do_rpm_uninstall */

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
		filename = g_strdup_printf ("%s",
					    rpmfilename_from_packagedata (pack));
		
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
		
		hd = g_new0 (Header, 1);
		rpm_err = rpmReadPackageHeader (fd,
						hd,
						&is_source, 
						NULL, 
						NULL);
		g_message ("rpmReadPackageHeader (%s) = %d%s", 
			   rpmfilename_from_packagedata (pack),
			   rpm_err, is_source ? " (source)":"");
		g_free (filename);
		fdClose (fd);
		
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

/*
static void
eazel_install_free_rpm_headers (GList *packages) 
{
	GList *iterator;
	
	for (iterator = packages; iterator; iterator = iterator->next) {
		PackageData *pack;
		pack = (PackageData*)(iterator->data);
		headerFree (*(Header*)(pack->packsys_struc));
	}
}

static void
eazel_install_free_headers (EazelInstall *service,
			    GList *packages)
{
	switch (eazel_install_get_package_system (service)) {
	case EAZEL_INSTALL_USE_RPM:
		eazel_install_free_rpm_headers (packages);
		break;
	}      
}
*/

static gboolean
eazel_install_free_rpm_system (EazelInstall *service)
{
	rpmdb *db;
	rpmTransactionSet *set;

	db = &(service->private->packsys.rpm.db);
	set = &(service->private->packsys.rpm.set);

	if (*db) {
		rpmdbClose (*db);
	}
/*
	if (*set != NULL) {
		rpmtransFree (*set);
	}
*/
	return TRUE;
}

static gboolean
eazel_install_prepare_rpm_system(EazelInstall *service)
{
	int mode;
	const char *root_dir;
	rpmdb *db;
	rpmTransactionSet *set;

	db = &(service->private->packsys.rpm.db);
	set = &(service->private->packsys.rpm.set);

	root_dir = eazel_install_get_root_dir (service) ? eazel_install_get_root_dir (service) : "";

	if (eazel_install_get_install_flags (service) & RPMTRANS_FLAG_TEST) {
		mode = O_RDONLY;
	}
	else {
		mode = O_RDWR | O_EXCL;
	}

	if (rpmdbOpen (root_dir, db, mode, 0644)) {
		const char* dn;
		dn = rpmGetPath (root_dir, "%{_dbpath}", NULL);
		if (!dn) {
			g_warning (_("RPM package database query failed !"));
		}
		return FALSE;
	}

	return TRUE;
}

static gboolean
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

static gboolean
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

static int
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
	
	to_remove = NULL;
	extras = g_hash_table_new (g_str_hash, g_str_equal);

	/* FIXME: bugzilla.eazel.com 1512 
	   This piece of code is rpm specific. It has some generic algorithm
	   for doing the dep stuff, but it's rpm entangled */

	for (iterator = 0; iterator < service->private->packsys.rpm.num_conflicts; iterator++) {
		GList *pack_entry;
		PackageData *pack;
		PackageData *dep;

		conflict = service->private->packsys.rpm.conflicts[iterator];

		/* FIXME bugzilla.eazel.com 1316
		   Need to find a solid way to capture file dependencies, and once they do not
		   appear at this point anymore, remove them. And in the end, check the list is
		   empty. 

		   Perhaps the smartest thing is to check at the final install. We can't resolve
		   these deps anyway, as we have on idea as to where the file is from.
		   
		   Or do a http search for a package providing the lib
		*/
		if (*conflict.needsName=='/' || strstr (conflict.needsName, ".so")) {
			continue;
		}

		pack_entry = g_list_find_custom (*packages, 
						 (gpointer)&conflict,
						 (GCompareFunc)eazel_install_package_conflict_compare);
		if (pack_entry == NULL) {
			switch (conflict.sense) {
			case RPMDEP_SENSE_REQUIRES:
				/* FIXME: bugzilla.eazel.com 1584
				   This sigsegvs...
				*/
				g_warning (_("%s %s breaks %s"), conflict.needsName, conflict.needsVersion, conflict.byName);
				pack_entry = g_list_find_custom (*packages, 
								 (gpointer)conflict.needsName,
								 (GCompareFunc)eazel_install_package_name_compare);
				g_message ("step 1");
				dep = packagedata_new_from_rpm_conflict_reversed (conflict);
				g_message ("step 2");
				dep->archtype = g_strdup (pack->archtype);
				pack = (PackageData*)(pack_entry->data);
				g_message ("step 3");
				pack->status = PACKAGE_BREAKS_DEPENDENCY;
				g_message ("step assertion");
				g_assert (dep!=NULL);
				/* FIXME: bugzilla.eazel.com 1363
				   Here, it'd be cool to compare dep->name to pack->name to see if
				   dep is pack's devel package. And if so, replace dep->version with
				   pack->version and not fail pack, but continue */

				{
					char *tmp;
					tmp = g_strdup_printf ("%s-devel", pack->name);
					if (strcmp (tmp, dep->name)==0) {
						g_message ("breakage is the devel package");
					}
					g_free (tmp);
				}

				if (!eazel_install_get_force (service)) {
					pack->breaks = g_list_prepend (pack->breaks, dep);
					if (g_list_find (*failedpackages, pack) == NULL) {
						(*failedpackages) = g_list_prepend (*failedpackages, pack);
					}
					to_remove = g_list_remove (to_remove, pack);
				}
				continue;
				break;
			case RPMDEP_SENSE_CONFLICTS:
				/* If we end here, it's a conflict is going to break something */
				/* FIXME: bugzilla.eazel.com 1514
				   Need to handle this more intelligently */
				g_warning (_("%s conflicts %s-%s"), conflict.byName, conflict.needsName, conflict.needsVersion);
				continue;
				break;
			}
		} else {
			pack = (PackageData*)(pack_entry->data);
			dep = packagedata_new_from_rpm_conflict (conflict);
			dep->archtype = g_strdup (pack->archtype);
		}
		g_assert (dep->name != NULL);
		eazel_install_emit_dependency_check (service, pack, dep);
		dep->archtype = g_strdup (pack->archtype);	       						
		pack->soft_depends = g_list_prepend (pack->soft_depends, dep);

		g_message (_("Processing dep for %s : requires %s"), pack->name, dep->name);

		if (eazel_install_fetch_package (service, dep)) {
			/* if it succeeds, add to a list of extras for this package 
			   We cannot just put it into extrapackages, as a later dep
			   might fail, and then we have to fail the package */
			GList *extralist;

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

/*
static int
rpm_install (EazelInstall *service, 
	     char* root_dir,
             char* file,
             char* location,
             rpm_install_cb cb,
             void* user_data) {

	GList* list;
	int result;

	list = g_list_append (NULL, file);

	result = do_rpm_install (service,
				 root_dir,
                                 list,
                                 location,
                                 cb,
                                 user_data);
	g_list_free (list);
	return result;
	} */ /* end rpm_install */


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
	eazel_install_add_to_set (service, packages, failed);

	for (iterator = *packages; iterator; iterator = iterator->next) {
		PackageData *pack;
		dbiIndexSet matches;
		int rc;
		rpmdb db;

		db = service->private->packsys.rpm.db;
		
		pack = (PackageData*)iterator->data;
		g_message ("checking reqs by %s (0x%x)", rpmname_from_packagedata (pack), pack);
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
							rc = rpmdbFindByRequiredBy (db, isrequired->name, &thirdmatches);
						}
						/* FIXME bugzilla.eazel.com 1539
						   check noone outside of "packages" & "requires" require
						   this before adding */
						(*requires) = g_list_prepend (*requires, isrequired);
					}
					dbiFreeIndexRecord (secondmatches);
				} else {
					/* FIXME bugzilla.eazel.com 1542
					   Need the ability to lookup a pacakge that provides a file
					   and process that */
					g_message ("must lookup %s", require_name[j]);
					/* FIXME bugzilla.eazel.com 1539
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
	/*
	  call upward with packages
	  call downward with packages and &tmp
	  add all from &tmp to packages
	*/

	g_message ("in eazel_uninstall_globber");

	tmp = NULL;
	eazel_uninstall_upward_traverse (service, packages, failed, &tmp);
	for (iterator = tmp; iterator; iterator = iterator->next) {
		g_message ("breaks %s", ((PackageData*)iterator->data)->name);
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
	g_message ("out eazel_uninstall_globber");
}

static int
rpm_uninstall (EazelInstall *service, 
	       char* root_dir,
               char* file,
               int install_flags,
               int problem_filters,
               int interface_flags) {
  GList* list;
  int result;

  list = g_list_append (NULL, file);

  result = do_rpm_uninstall (service,
			     root_dir,
                             list,
                             install_flags,
                             problem_filters,
                             interface_flags);

  g_list_free (list);
  return result;
} /* end rpm_uninstall */
