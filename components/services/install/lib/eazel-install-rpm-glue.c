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
 */

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-services-config.xml
 * file and install a services generated package-list.xml.
 */

#include "eazel-install-rpm-glue.h"
#include "eazel-install-xml-package-list.h"

#include "helixcode-utils.h"
#include <rpm/rpmlib.h>
#include <rpm/rpmmacro.h>
#include <rpm/dbindex.h>
#include <string.h>
#include <time.h>

#define UNKNOWN_SIZE 1024

typedef void (*rpm_install_cb)(char* name, char* group, void* user_data);

static gboolean download_a_package (EazelInstall *service,
                                PackageData* package);

static gboolean download_all_packages (EazelInstall *service,
                                       GList* categories);

static gboolean install_all_packages (EazelInstall *service,
                                      int install_flags,
                                      int problem_filters,
                                      int interface_flags,
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

static int rpm_install (EazelInstall *service,
			char* root_dir,
                        char* file_name,
                        char* location,
                        int install_flags,
                        int problem_filters,
                        int interface_flags,
                        rpm_install_cb callback,
                        void* user_data);

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
                           int install_flags,
                           int problem_filters,
                           int interface_flags,
                           rpm_install_cb callback,
                           void* user_data);

static int do_rpm_uninstall (EazelInstall *service, 
			     char* root_dir,
                             GList* packages,
                             int install_flags,
                             int problem_filters,
                             int interface_flags);


gboolean
install_new_packages (EazelInstall *service) {

	GList* categories;
	gboolean rv;
	int install_flags, interface_flags, problem_filters;
	
	categories = NULL;
	install_flags = 0;
	interface_flags = 0;
	problem_filters = 0;
	
	if (eazel_install_get_test (service) == TRUE) {
		g_message (_("Dry Run Mode Activated.  Packages will not actually be installed ...\n"));
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
	
	rpmReadConfigFiles (eazel_install_get_rpmrc_file (service), NULL);

	g_message (_("Reading the install package list ...\n"));
	categories = parse_local_xml_package_list (eazel_install_get_package_list (service));

	if (categories == NULL) {
		rv = FALSE;
	} else {
		rv = download_all_packages (service, categories);
		if (rv == TRUE) {
			rv = install_all_packages (service,
						   install_flags,
						   problem_filters,
						   interface_flags,
						   categories);
		} else {
			g_warning ("Install not called, since some files were not downloaded");
		}
	}
	
	return rv;
} /* end install_new_packages */

/* Returns FALSE is file was not fetched */
static gboolean
download_a_package (EazelInstall *service, 
		    PackageData* package) 
{
	gboolean result;
	
	result = TRUE;
	if (eazel_install_get_protocol (service) == PROTOCOL_HTTP) {
		char* rpmname;
		char* targetname;
		char* url;
		gboolean rv;

                rpmname = g_strdup_printf ("%s-%s-%s.%s.rpm",
                                           package->name,
                                           package->version,
                                           package->minor,
                                           package->archtype);

		targetname = g_strdup_printf ("%s/%s",
                                              eazel_install_get_tmp_dir (service),
                                              rpmname);
		url = g_strdup_printf ("http://%s%s/%s",
                                       eazel_install_get_hostname (service),
                                       eazel_install_get_rpm_storage_path (service),
                                       rpmname);

		g_message ("Downloading %s...\n", rpmname);
		rv = http_fetch_remote_file (service, url, targetname);
		if (rv != TRUE) {
			result = FALSE;
		}

		g_free (rpmname);
		g_free (targetname);
		g_free (url);

	}
	return result;
} /* end download_a_package */

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

		cat = categories->data;
		pkgs = cat->packages;

		g_message ("Category = %s\n", cat->name);
		while (pkgs) {
			PackageData* package;

			package = pkgs->data;

			if (download_a_package (service, package) == FALSE) {
				g_warning ("*** Failed to retreive %s! ***\n", package->name);
				eazel_install_emit_download_failed (service, package->name, NULL);
				result = FALSE;
			}

			pkgs = pkgs->next;
		}
		categories = categories->next;
	}

	free_categories (categories);
	
	return result;
} /* end download_all_packages */

static gboolean
install_all_packages (EazelInstall *service,
                      int install_flags,
                      int problem_filters,
                      int interface_flags,
                      GList* categories) {

	gboolean rv;
	rv = TRUE;

	while (categories) {
		CategoryData* cat = categories->data;
		GList* pkgs = cat->packages;

		g_message ("Category = %s\n", cat->name);
		while (pkgs) {
			PackageData* pack;
			char* pkg; 
			int retval;

			pack = pkgs->data;
			retval = 0;
			
			pkg = g_strdup_printf ("%s/%s-%s-%s.%s.rpm",
                                               eazel_install_get_tmp_dir (service),
                                               pack->name,
                                               pack->version,
                                               pack->minor,
                                               pack->archtype);

			g_message ("Installing %s\n", pkg);

                        retval = rpm_install (service, 
					      "/", pkg, NULL, install_flags,
					      problem_filters, interface_flags,
                                              NULL, NULL); 

			if (retval == 0) {
				g_message (_("Package install successful !\n"));
			} else {
				g_message (_("Package install failed !\n"));				
				rv = FALSE;
			}
			pkgs = pkgs->next;
		}
		categories = categories->next;
	}

	free_categories (categories);
	
	return rv;
} /* end install_all_packages */


gboolean 
uninstall_packages (EazelInstall *service) {
	GList* categories;
	gboolean rv;
	int uninstall_flags, interface_flags, problem_filters;
	
	categories = NULL;
	uninstall_flags = 0;
	interface_flags = 0;
	problem_filters = 0;
	
	if (eazel_install_get_test (service) == TRUE) {
		uninstall_flags |= RPMTRANS_FLAG_TEST;
	}

	if (eazel_install_get_verbose (service) == TRUE) {
		rpmSetVerbosity (RPMMESS_VERBOSE);
	}
	else {
		rpmSetVerbosity (RPMMESS_NORMAL);
	}

	rpmReadConfigFiles (eazel_install_get_rpmrc_file (service), NULL);

	g_message (_("Reading the uninstall package list ...\n"));
	categories = parse_local_xml_package_list (eazel_install_get_package_list (service));

	while (categories) {
		CategoryData* cat = categories->data;
		GList* pkgs = cat->packages;

		g_message ("Category = %s\n", cat->name);
		while (pkgs) {
			PackageData* package = pkgs->data;

			if (uninstall_a_package (service, 
						 package,
						 uninstall_flags,
						 problem_filters,
						 interface_flags) == FALSE) {
				g_warning ("Uninstall failed for %s",package->name);
				eazel_install_emit_uninstall_failed (service, package);
			} else {
				rv = FALSE;
			}
			pkgs = pkgs->next;
		}
		categories = categories->next;
	}

	free_categories (categories);
	
	return rv;

} /* end install_new_packages */

static gboolean
uninstall_a_package (EazelInstall *service,
		     PackageData* package,
                     int uninstall_flags,
                     int problem_filters,
                     int interface_flags) {
	char* pkg; 
	int retval;
	gboolean rv;

	retval = 0;

	pkg = g_strdup_printf ("%s-%s-%s",
                               package->name,
                               package->version,
                               package->minor);

	rv = TRUE;
	if (g_strcasecmp (package->archtype, "src") != 0) {

		g_message ("Uninstalling %s\n", pkg);
		retval = rpm_uninstall (service,
					"/",
                                        pkg,
                                        uninstall_flags,
                                        problem_filters,
                                        interface_flags);
		g_free (pkg);
		if (retval == 0) {
			g_message ("Package uninstall successful!\n");
		} else {
			g_message (_("Package uninstall failed !\n"));
			rv = FALSE;
		}
	} else {
  		g_message ("%s seems to be a source package.  Skipping ...\n", pkg);
		g_free (pkg);
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

		case RPMCALLBACK_INST_PROGRESS:			
			eazel_install_emit_install_progress (service, filename, amount, total);
			break;

		default:
			break;
	}
	return NULL;
} /* end rpm_show_progress */

static GList*
build_packagedata_list_from_deps (struct rpmDependencyConflict* conflicts, 
				  int num_conflicts) 
{
	GList *result;
	int i;
	
	g_return_val_if_fail (num_conflicts >= 1, NULL);
	g_return_val_if_fail (conflicts != NULL, NULL);

	result = NULL;

	for (i = 0; i < num_conflicts; i++) {
		struct rpmDependencyConflict c;
		PackageData *pd;

		c = conflicts [i];

		pd = g_new0 (PackageData, 1);
		pd->name = g_strdup (c.needsName);
		pd->version = g_strdup (c.needsVersion);
		pd->minor = NULL;
		pd->archtype = NULL;
		pd->bytesize = 0;
		pd->summary = NULL;
		pd->soft_depends = NULL;
		pd->hard_depends = NULL;

		result = g_list_prepend (result, pd);
	}
	return result;
}

int
do_rpm_install (EazelInstall *service,
		char* root_dir, 
		GList* packages, 
		char* location,
                int install_flags, 
		int problem_filters, 
		int interface_flags,
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

	stop_install = 0;
	num_binary_packages = 0;
	num_source_packages = 0;
	num_failed = 0;
	total_size = 0;
	rpmdep = NULL;
	probs = NULL;


	if (install_flags & RPMTRANS_FLAG_TEST) {
		mode = O_RDONLY;
	}
	else {
		mode = O_RDWR | O_CREAT;
	}

	pkg_count = g_list_length (packages);
	pkgs = g_new (char *, pkg_count + 1);
	binary_headers = g_new (Header, pkg_count + 1);
	/* First load all rpm headers */
	for (num_packages = 0; packages != NULL; packages = packages->next) {
		pkg_file = packages->data;
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

		/* Fail source packages */
		if (is_source) {
			PackageData *pd;
			g_warning ("Source Package installs not supported!\n"
                                     "Package %s skipped.", pkg_file);	
			
			/*
			  FIXME bugzilla.eazel.com 1093:
			  this call fails.
			  pd = packagedata_new_from_rpm_header (binary_headers[num_binary_packages]);
			*/
			pd = g_new0 (PackageData, 1);
			headerGetEntry (binary_headers[num_binary_packages],
					RPMTAG_NAME, NULL,
					(void **) &pd->name, NULL);
			headerGetEntry (binary_headers[num_binary_packages],
					RPMTAG_VERSION, NULL,
					(void **) &pd->version, NULL);
			headerGetEntry (binary_headers[num_binary_packages],
					RPMTAG_RELEASE, NULL,
					(void **) &pd->minor, NULL);
			headerGetEntry (binary_headers[num_binary_packages],
					RPMTAG_ARCH, NULL,
					(void **) &pd->archtype, NULL);
			headerGetEntry (binary_headers[num_binary_packages],
					RPMTAG_SIZE, NULL,
					(void **) &pd->bytesize, NULL);
			headerGetEntry (binary_headers[num_binary_packages],
					RPMTAG_SUMMARY, NULL,
					(void **) &pd->summary, NULL);
			
			eazel_install_emit_install_failed (service, pd, RPM_SRC_NOT_SUPPORTED, NULL);
			packagedata_destroy (pd);
		}
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
                                            interface_flags,
                                            NULL);
		}
		/* Ensure the order */
		if (!(interface_flags & INSTALL_NOORDER)) {
			if (rpmdepOrder(rpmdep)) {
				num_failed = num_packages;
				stop_install = 1;
			}
		}

		if (!(interface_flags & INSTALL_NODEPS)) {
			/* Check the dependencies */
			if (rpmdepCheck (rpmdep, &conflicts, &num_conflicts)) {
				num_failed = num_packages;
				stop_install = 1;
			}
			/* Deps failed ? */
			if (!stop_install && conflicts) {
				PackageData *pd;
				GList *deps;

				deps = build_packagedata_list_from_deps (conflicts, num_conflicts);
				pd = g_new0 (PackageData, 1);
				pd->name = g_strdup (conflicts[0].byName);

				g_message ("Dep failed for %s",pd->name);
				eazel_install_emit_install_failed (service, pd, RPM_DEP_FAIL, deps); 

				num_failed = num_packages;
				stop_install = 1;

				packagedata_destroy (pd);
				g_list_foreach (deps, (GFunc)packagedata_destroy_foreach, NULL);
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
		if (interface_flags & INSTALL_UPGRADE) {
			 g_message (_("Upgrading...\n"));
		}
		else {
			g_message (_("Installing...\n"));
		}

		rc = rpmRunTransactions (rpmdep,
                                         rpm_show_progress,
                                         service,
                                         NULL,
                                         &probs,
                                         install_flags,
                                         problem_filters);

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
			g_warning (_("Packages database query failed !\n"));
		}
		return g_list_length (packages);
	}

	rpmdep = rpmtransCreateSet (db, root_dir);
	for (num_packages = 0; packages != NULL; packages = packages->next) {
		pkg_name = packages->data;
		rc = rpmdbFindByLabel (db, pkg_name, &matches);
		switch (rc) {
			case 1:
				g_message (_("Package %s is not installed\n"), pkg_name);
				num_failed++;
				break;
			case 2:
				g_message (_("Error finding index to %s\n"), pkg_name);
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

			g_message (_("Dependancy check failed.\n"));
			printDepProblems (stderr, conflicts,
                                          num_conflicts);
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

#if 0
/*
  Given a list of filenames, ensure_deps_are_fetched checks deps
  for them all, if deps fail, fetch the depency and add to outfiles.
  Returns FALSE if outfiles was set, TRUE is all dependencies were satisfied.
 */
static gboolean
ensure_deps_are_fetched (GList *filenames, GList **_outfiles)
{
	GList *appendfiles;
	GList *removefiles;
	GList *outfiles;
	gboolean result;
	rpmTransactionSet rpmdep;
	struct rpmDependencyConflict* conflicts;
	int num_conflicts;
	rpmdb db;
	GList *file_iterator;

	g_assert (outfiles == NULL);
	g_return_val_if_fail (filenames != NULL, TRUE);

	outfiles = NULL;
	result = TRUE;
	append = NULL;

	rpmdep = rpmtransCreateSet (db, root_dir);
	for (file_iterator = filenames; file_iterator; file_iterator = file_iterator->next) {
		g_message ("ensuring deps for %s", file_iterator->data);
		
	}

	(*_outfiles) = outfiles;
	(*_filenames) = filenames;

	return result;
}
#endif 

static int
rpm_install (EazelInstall *service, 
	     char* root_dir,
             char* file,
             char* location,
             int install_flags,
             int problem_filters,
             int interface_flags,
             rpm_install_cb cb,
             void* user_data) {

	GList* list;
	int result;

	list = g_list_append (NULL, file);
	result = do_rpm_install (service,
				 root_dir,
                                 list,
                                 location,
                                 install_flags,
                                 problem_filters,
                                 interface_flags,
                                 cb,
                                 user_data);
	g_list_free (list);
	return result;
} /* end rpm_install */

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

