/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 * Copyright (C) 2000 Helix Code, Inc
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
 *          Joe Shaw <joe@helixcode.com>
 */

/* eazel-install - services command line install/update/uninstall
 * component.  This program will parse the eazel-services-config.xml
 * file and install a services generated package-list.xml.
 */

#ifndef __EAZEL_SERVICES_TYPES_H__
#define __EAZEL_SERVICES_TYPES_H__

#include <gnome.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <rpm/rpmlib.h>

typedef enum _URLType URLType;
typedef enum _PackageType PackageType;
typedef struct _TransferOptions TransferOptions;
typedef struct _InstallOptions InstallOptions;
typedef struct _CategoryData CategoryData;
typedef struct _PackageData PackageData;
typedef enum _RPM_FAIL RPM_FAIL;

enum _RPM_FAIL {
	RPM_SRC_NOT_SUPPORTED,
	RPM_DEP_FAIL,
	RPM_NOT_AN_RPM
};

struct _HTTPError {
	int code;
	char *reason;
};

enum _URLType {
	PROTOCOL_LOCAL,
	PROTOCOL_HTTP,
	PROTOCOL_FTP
};

enum _PackageType {
	PACKAGE_TYPE_RPM,
	PACKAGE_TYPE_DPKG,
	PACKAGE_TYPE_SOLARIS
};

struct _TransferOptions {
	char* hostname;                    /* Remote hostname */
	guint port_number;                 /* Connection port */
	char* pkg_list_storage_path;       /* Remote path to package-list.xml */
	char* rpm_storage_path;            /* Remote path to RPM directory */
	char* tmp_dir;                     /* Local directory to store incoming RPMs */
	char* rpmrc_file;                  /* Location of the rpm resource file */
};

struct _InstallOptions {
	URLType protocol;          /* Specifies local, ftp, or http */ 
	char* pkg_list;            /* Local path to package-list.xml */
	gboolean mode_verbose;     /* print extra information */
	gboolean mode_silent;      /* FIXME bugzilla.eazel.com 731: print all information to a logfile */
	gboolean mode_debug;       /* Internal testing mode for debugging */
	gboolean mode_test;        /* dry run mode */
	gboolean mode_force;       /* Force the action to be performed */
	gboolean mode_depend;      /* FIXME bugzilla.eazel.com 731: print all dependancies */
	gboolean mode_update;      /* If package is already installed, update it */
	gboolean mode_uninstall;   /* Uninstall the package list */
	gboolean mode_downgrade;   /* Downgrade the packages to previous version*/
};

struct _CategoryData {
	char* name;
	GList* packages;
};
void categorydata_destroy_foreach (CategoryData *cd, gpointer ununsed);
void categorydata_destroy (CategoryData *pd);

struct _PackageData {
	char* name;
	char* version;
	char* minor;
	char* archtype;
	int bytesize;
	char* summary;
	GList* soft_depends;
	GList* hard_depends;
};
PackageData* packagedata_new_from_rpm_header (Header);
void packagedata_destroy_foreach (PackageData *pd, gpointer unused);
void packagedata_destroy (PackageData *pd);

#endif /* __EAZEL_SERVICES_TYPES_H__ */
