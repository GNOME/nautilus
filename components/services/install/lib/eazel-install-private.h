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

/* Funky define to step a GList iterator one ahead */
#define glist_step(iterator) iterator = g_list_next (iterator)


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

	gboolean ssl_rename; /* If true, rename the hosts part in all urls to 
				localhost. This is to make stuff work
				with ssl tunneling */

	/* This holds the files that were downloaded */
	GList *downloaded_files;

	gboolean use_local_package_list;
	
	PackageSystem package_system;
	union {
		struct {
			GHashTable *dbs;
			unsigned long total_size, 
				      current_installed_size, 
				      num_packages, 
				      packages_installed;
			int install_flags,
			    interface_flags,
			    problem_filters;
		} rpm;
	} packsys;

	/* This hash maps from package name-version-release to the 
	   package. Packages are added in eazel-install-rpm-glue.c do_rpm_install
	   It's used in the progress stuff to lookup the correct
	   package */
	GHashTable *name_to_package_hash;

	/* This holds the toplevel packages requested for 
	   install/upgrade/uninstall.
	   Entries are added in eazel-install-rpm-glue.c, as
	   stuff is done */
	GList *transaction;
	
	/* The logfile used for the object */
	FILE *logfile;
	char *logfilename;
	
        /* TRUE if the rpm subcommand is running */
	volatile gboolean subcommand_running;
};

#endif /* EAZEL_INSTALL_PRIVATE_H */
