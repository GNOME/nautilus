/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

#ifndef EAZEL_INSTALL_PRIVATE_H
#define EAZEL_INSTALL_PRIVATE_H

#include "eazel-install-public.h"
#include "eazel-package-system.h"
#include "eazel-softcat.h"

/* Funky define to step a GList iterator one ahead */
#define glist_step(iterator) iterator = g_list_next (iterator)

#define DEFAULT_RPM_DB_ROOT "/var/lib/rpm"

struct _EazelInstallPrivate {	
	TransferOptions *topts;
	InstallOptions *iopts;
 
        /* Used in rpm-glue */
	GList *root_dirs; /* Holds the root dirs for package databases. Eazel package database
			     glue must handle this as seen fit */
	char *cur_root; /* For a given operation, this holds the current root dir */
	int install_flags; 
	int interface_flags; 
	int problem_filters; 
	char *transaction_dir;

	gboolean ei2; /* This for the transition period. ei2==true means use the
			 new ei2 stuff */

	gboolean ssl_rename; /* If true, rename the hosts part in all urls to 
				localhost. This is to make stuff work
				with ssl tunneling */

	gboolean ignore_file_conflicts; /* Instructs the installer to ignore file conflicts */

	gboolean revert; /* If true, the current operation is a reversion */

	/* This holds the files that were downloaded */
	GList *downloaded_files;

	gboolean use_local_package_list;
  	
	EazelPackageSystem *package_system;

	/* hacky way to implement the old style signals with the new package system object */
	unsigned long infoblock[6];

	/* This hash maps from package name-version-release to the 
	   package. Packages are added in eazel-install-rpm-glue.c do_rpm_install
	   It's used in the progress stuff to lookup the correct
	   package. 
	   It's cleaned up in the end of eazel_install_start_transaction,
	   but not erased. */
	GHashTable *name_to_package_hash;

	/* During an operation, this logs the toplevel packages
	   that were installed */
	GList *transaction;

	/* During an operation, this logs all packages that failed */
	GList *failed_packages;
	
	/* The logfile used for the object */
	FILE *logfile;
	char *logfilename;
	gboolean log_to_stderr;

        /* TRUE if the rpm subcommand is running */
	volatile gboolean subcommand_running;

	/* used to cancel a download before it finished */
	gboolean cancel_download;

	/* hacky way to notice if the disk is full */
	gboolean disk_full;

	/* look in these local directories for packages before downloading */
	GList *local_repositories;

	/* context to use for softcat queries */
	EazelSoftCat *softcat;

	/* These are hashtables used in eazel-install-logic2.c */
	GHashTable *dedupe_hash; /* This hash matches from eazel_id to PackageData objects, 
				    and is used to quickly identify dupes after getting the softcat
				    information */
	GHashTable *dep_ok_hash; /* This hash matches from eazel_id to enum (found in eazel-install-logic2.c), 
				    and is used during check for dependencies which are required. If the
				    string occurs in the hash, the package has already been checked
				    for, and the returned enum denotes the dependency state.
				    The hash also matches from a eazel_id-sense-version concatenation
				    to a PackageData for the case where the dependency has both a version
				    and sense */
};

#endif /* EAZEL_INSTALL_PRIVATE_H */
