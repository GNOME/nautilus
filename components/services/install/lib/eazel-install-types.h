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

#ifndef EAZEL_INSTALL_SERVICES_TYPES_H
#define EAZEL_INSTALL_SERVICES_TYPES_H

#include <gnome.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <rpm/rpmlib.h>
#include <libtrilobite/trilobite-core-distribution.h>

typedef enum _URLType URLType;
typedef enum _PackageType PackageType;
typedef struct _TransferOptions TransferOptions;
typedef struct _InstallOptions InstallOptions;
typedef struct _CategoryData CategoryData;
typedef struct _PackageData PackageData;
typedef enum _PackageSystemStatus PackageSystemStatus;

/*
  Adding here requires editing in
  trilobite-eazel-install.idl
  eazel-install-corba-types.c (packagedata_from_corba_packagedatastruct) (corba_packagedatastruct_from_packagedata)
 */
enum _PackageSystemStatus {
	PACKAGE_UNKNOWN_STATUS=0,
	PACKAGE_SOURCE_NOT_SUPPORTED,
	PACKAGE_DEPENDENCY_FAIL,
	PACKAGE_BREAKS_DEPENDENCY,
	PACKAGE_INVALID,
	PACKAGE_CANNOT_OPEN,
	PACKAGE_PARTLY_RESOLVED,
	PACKAGE_ALREADY_INSTALLED,
	PACKAGE_RESOLVED
};
/* Methods to convert enum to/from char* val. The returned
   char* must not be freed */
PackageSystemStatus packagedata_status_str_to_enum (const char *st);
const char* packagedata_status_enum_to_str (PackageSystemStatus st);

enum _PackageModification {
	PACKAGE_MOD_UNTOUCHED,
	PACKAGE_MOD_UPGRADED,
	PACKAGE_MOD_DOWNGRADED,
	PACKAGE_MOD_INSTALLED,
	PACKAGE_MOD_UNINSTALLED
};
typedef enum _PackageModification PackageModification;
/* Methods to convert enum to/from char* val. The returned
   char* must not be freed */
PackageSystemStatus packagedata_modstatus_str_to_enum (const char *st);
const char* packagedata_modstatus_enum_to_str (PackageModification st);

struct _HTTPError {
	int code;
	char *reason;
};

enum _URLType {
	PROTOCOL_LOCAL,
	PROTOCOL_HTTP,
	PROTOCOL_FTP
};
const char *protocol_as_string (URLType protocol);

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
void transferoptions_destroy (TransferOptions *topts);

struct _InstallOptions {
	URLType protocol;          /* Specifies local, ftp, or http */ 
	char* pkg_list;            /* Local path to package-list.xml */
	char* transaction_dir;                     /* Local directory to store transactions */
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
void installoptions_destroy (InstallOptions *iopts);

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
	DistributionInfo distribution;
	int bytesize;
	char* summary;	
	GList* soft_depends;
	GList* hard_depends;
	GList* breaks; 

	char *filename;
	
	/* 
	   toplevel = TRUE if this a package the user requested.
	   It's used to ensure that a "install_failed" signal is
	   only emitted for toplevel packages.
	   It's set to true during the xml loading (that means
	   it should be set before given to the eazel_install_ensure_deps
	 */
	gboolean toplevel;
	/*
	  Identifies the status of the installation
	*/
	PackageSystemStatus status;
	/*
	  Pointer to keep a structure for the package system
	 */
	gpointer *packsys_struc;

	/* List of packages that this package modifies */
	GList *modifies;
	/* how was the package modified 
	   Eg. the toplevel pacakge will have INSTALLED, and some stuff in "soft/hard_depends."
	   if "modifies" has elements, these have the following meaning ;
 	     DOWNGRADED means that the package was replaced with an older version
	     UPGRADED means that the package was replaced with a never version
	 */
	PackageModification modify_status;	
};

PackageData* packagedata_new (void);
PackageData* packagedata_new_from_file (const char *file);

PackageData* packagedata_new_from_rpm_header (Header*);
PackageData* packagedata_new_from_rpm_conflict (struct rpmDependencyConflict);
PackageData* packagedata_new_from_rpm_conflict_reversed (struct rpmDependencyConflict);

void packagedata_fill_from_file (PackageData *pack, const char *filename);
void packagedata_fill_from_rpm_header (PackageData *pack, Header*);

const char *rpmfilename_from_packagedata (const PackageData *pack);
const char *rpmname_from_packagedata (const PackageData *pack);

void packagedata_destroy_foreach (PackageData *pd, gpointer unused);
void packagedata_destroy (PackageData *pd);

int packagedata_hash_equal (PackageData *a, PackageData *b);

void packagedata_add_pack_to_breaks (PackageData *pack, PackageData *b);
void packagedata_add_pack_to_soft_deps (PackageData *pack, PackageData *b);
void packagedata_add_pack_to_hard_deps (PackageData *pack, PackageData *b);

/* Evil marshal func */

void eazel_install_gtk_marshal_NONE__POINTER_INT_INT_INT_INT_INT_INT (GtkObject * object,
								      GtkSignalFunc func,
								      gpointer func_data, GtkArg * args);

#endif /* EAZEL_INSTALL_SERVICES_TYPES_H */
