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

struct _EazelInstallPrivate {	
	TransferOptions *topts;
	InstallOptions *iopts;
 
        /* Used in rpm-glue */
	char *root_dir;
	int install_flags; 
	int interface_flags; 
	int problem_filters; 
	char *transaction_dir;

	gboolean use_local_package_list;
	
	PackageSystem package_system;
	union {
		struct {
			rpmdb db;
			rpmTransactionSet set;
			struct rpmDependencyConflict *conflicts;
			int num_conflicts;
			int total_size, 
				current_installed_size, 
				num_packages, 
				packages_installed;
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

	FILE *logfile;
	char *logfilename;
};

#endif /* EAZEL_INSTALL_PRIVATE_H */
