/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*- */
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

#ifndef EAZEL_PACKAGE_SYSTEM_TYPES_H
#define EAZEL_PACKAGE_SYSTEM_TYPES_H

#include <gtk/gtkobject.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libtrilobite/trilobite-core-distribution.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef enum _URLType URLType;
typedef enum _PackageType PackageType;
typedef enum _PackageFillFlags PackageFillFlags;
typedef struct _TransferOptions TransferOptions;
typedef struct _InstallOptions InstallOptions;
typedef struct _CategoryData CategoryData;
typedef enum _PackageSystemStatus PackageSystemStatus;

/*
  Adding here requires editing in
  trilobite-eazel-install.idl
  eazel-install-corba-types.c 
    - packagedata_from_corba_packagedatastruct
    - corba_packagedatastruct_from_packagedata
  and
  eazel-package-system-types.c
    - packagedata_status_enum_to_str
    - packagedata_status_str_to_enum
  and
  eazel-install-problem.c
 */
enum _PackageSystemStatus {
	PACKAGE_UNKNOWN_STATUS=0,
	PACKAGE_SOURCE_NOT_SUPPORTED,
	PACKAGE_DEPENDENCY_FAIL,
	PACKAGE_FILE_CONFLICT,
	PACKAGE_BREAKS_DEPENDENCY,
	PACKAGE_INVALID,
	PACKAGE_CANNOT_OPEN,
	PACKAGE_PARTLY_RESOLVED,
	PACKAGE_ALREADY_INSTALLED,
	PACKAGE_CIRCULAR_DEPENDENCY,
	PACKAGE_RESOLVED,
	PACKAGE_CANCELLED,
	PACKAGE_PACKSYS_FAILURE
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

/* NOTE: if you add protocols here, modify the following places :
   idl/trilobite-eazel-install.idl
   lib/eazel-install-protocols.c (eazel_install_fill_file_fetch_table)
   lib/eazel-package-system-types.c (protocol_as_string)
   lib/eazel-install-corba.c (impl_Eazel_Install__set_protocol) (impl_Eazel_Install__get_protocol)
 */
enum _URLType {
	PROTOCOL_LOCAL = 0,
	PROTOCOL_HTTP  = 1,
	PROTOCOL_FTP   = 2
};
const char *eazel_install_protocol_as_string (URLType protocol);

enum _PackageFillFlags {
	PACKAGE_FILL_EVERYTHING = 0x0,
	PACKAGE_FILL_NO_TEXT = 0x01,
	PACKAGE_FILL_NO_PROVIDES = 0x02,
	PACKAGE_FILL_NO_DEPENDENCIES = 0x04,
	PACKAGE_FILL_NO_DIRS_IN_PROVIDES = 0x8, /* only used if PACKAGE_FILL_NO_PROVIDES is not set */
	PACKAGE_FILL_NO_FEATURES = 0x10,
	PACKAGE_FILL_MINIMAL = 0x7fff,
	PACKAGE_FILL_INVALID = 0x8000
};

/* FIXME eventually this is going away completely */
struct _TransferOptions {
	char *username;                    /* The username to use in eazel-install: paths */
	char* pkg_list_storage_path;       /* Remote path to package-list.xml */
	char* tmp_dir;                     /* Local directory to store incoming RPMs */
	char* rpmrc_file;                  /* Location of the rpm resource file */
};
void transferoptions_destroy (TransferOptions *topts);

struct _InstallOptions {
	URLType protocol;          /* Specifies local, ftp, or http */ 
	char* pkg_list;            /* Local path to package-list.xml */
	char* transaction_dir;                     /* Local directory to store transactions */
	gboolean mode_verbose;     /* print extra information */
	gboolean mode_silent;      /* print all information to a logfile */
	gboolean mode_debug;       /* Internal testing mode for debugging */
	gboolean mode_test;        /* dry run mode */
	gboolean mode_force;       /* Force the action to be performed */
	gboolean mode_depend;      /* print all dependancies */
	gboolean mode_update;      /* If package is already installed, update it */
	gboolean mode_uninstall;   /* Uninstall the package list */
	gboolean mode_downgrade;   /* Downgrade the packages to previous version*/
};
void installoptions_destroy (InstallOptions *iopts);

struct _CategoryData {
	char* name;
	char* description;
	GList* packages;
	GList* depends;			/* used only for the GUI: GList<char *> -- other category names */
	gboolean exclusive;		/* no other items can be clicked if this one is */
	gboolean default_choice;	/* click this one by default */
};
CategoryData *categorydata_new (void);
CategoryData *categorydata_copy (const CategoryData *cat);
GList *categorydata_list_copy (const GList *list);
void categorydata_destroy_foreach (CategoryData *cd, gpointer ununsed);
void categorydata_destroy (CategoryData *pd);
void categorydata_list_destroy (GList *list);

/* Returns a glist of all the packages in the categories */
GList* categorylist_flatten_to_packagelist (GList *categories);

/*************************************************************************************************/

#define TYPE_PACKAGEDATA           (packagedata_get_type ())
#define PACKAGEDATA(obj)           (GTK_CHECK_CAST ((obj), TYPE_PACKAGEDATA, PackageData))
#define PACKAGEDATA_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TYPE_PACKAGEDATA, PackageDataClass))
#define IS_PACKAGEDATA(obj)        (GTK_CHECK_TYPE ((obj), TYPE_PACKAGEDATA))
#define IS_PACKAGEDATA_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_PACKAGEDATA))

typedef struct _PackageData PackageData;
typedef struct _PackageDataClass PackageDataClass;

struct _PackageDataClass {
	GtkObjectClass parent_class;
	void (*finalize) (GtkObject *obj);
};

struct _PackageData {
	GtkObject parent;

	char* name;
	char* version;
	char* minor;
	char* archtype;
	DistributionInfo distribution;
	guint32 bytesize;
	guint32 filesize;

	char* summary;
	char* description;	
	GList* depends;		/* GList<PackageDependency *> */
	GList* breaks; 	        /* GList<PackageBreaks*> */

	char *filename;
	char *remote_url;		/* url where we can get this rpm */
	char *md5;
	char *install_root;
	
	/* various odd ways to look up packages in softcat */
	char *eazel_id;
	char *suite_id;

	gboolean source_package;
	gboolean conflicts_checked; /* set to TRUE when the files provided by the package
				       have been checked against already installed packages */

	int fillflag;
	
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
	
	/* These are the files that the package provides
	   NOTE: should not be corbafied in eazel-install-corba-types.c */
	GList *provides;
	/* This is true if provides contains directories */
	gboolean provides_has_dirs; 

	/* List of packages that this package modifies */
	GList *modifies;
	/* how was the package modified 
	   Eg. the toplevel pacakge will have INSTALLED, and some stuff in "depends".
	   if "modifies" has elements, these have the following meaning ;
 	     DOWNGRADED means that the package was replaced with an older version
	     UPGRADED means that the package was replaced with a never version
	 */
	PackageModification modify_status;

	/* if the package info was downloaded from softcat, this is a list of "features"
	 * the package resolves for its parent package.  for example, if this package is only
	 * needed to fulfill a "/bin/sh" requirement for its parent package, the "features"
	 * list will contain "/bin/sh".
	 */
	GList *features;

	/* This identifies a package (by name) that should be deleted if installing this package */
        GList *obsoletes;

	/* if set, the package has an epoch, currently only set for stuff read from the local db system */
	guint32 epoch;
};

PackageData* packagedata_new (void);
GtkType packagedata_get_type (void);

PackageData* packagedata_new_from_file (const char *file);
PackageData* packagedata_copy (const PackageData *pack, gboolean deep);
GList *packagedata_list_copy (const GList *list, gboolean deep);

void packagedata_fill_in_missing (PackageData *package, const PackageData *full_package, int fill_flags);

void packagedata_remove_soft_dep (PackageData *remove, PackageData *from);

const char *rpmfilename_from_packagedata (const PackageData *pack);
const char *rpmname_from_packagedata (const PackageData *pack);

/* This is now the authorative way to get a nice human-readble name
   from a given package */
char *packagedata_get_readable_name (const PackageData *pack);

/* This is now the authorative way to get a nice "real" name
   from a given package, real meanign name[-version-[release]] string */
char *packagedata_get_name (const PackageData *pack);

int packagedata_hash_equal (PackageData *a, PackageData *b);

GList *flatten_packagedata_dependency_tree (GList *packages);

/* all elements in "remove_list" that matches elements in "input" list
   (comparison is done via eazel_install_package_name_compare).
   If destroy is TRUE, packagedata_destroy is called on the removed
   elements. If deep is TRUE, packagedata_destroy is called with deep destruction
*/
void packagedata_list_prune (GList **input, GList *remove_list, gboolean destroy, gboolean deep);

/*************************************************************************************************/

typedef enum {
	EAZEL_SOFTCAT_SENSE_EQ = 0x1,
	EAZEL_SOFTCAT_SENSE_GT = 0x2,
	EAZEL_SOFTCAT_SENSE_LT = 0x4,
	EAZEL_SOFTCAT_SENSE_GE = (EAZEL_SOFTCAT_SENSE_GT | EAZEL_SOFTCAT_SENSE_EQ),
	EAZEL_SOFTCAT_SENSE_ANY = (EAZEL_SOFTCAT_SENSE_GT | EAZEL_SOFTCAT_SENSE_EQ | EAZEL_SOFTCAT_SENSE_LT)
} EazelSoftCatSense;

/* dependency list */
typedef struct {
	PackageData *package;
	/* if this dependency fills a requirement, like "gconf >= 0.6",
	 * the requirement is listed here: */
	EazelSoftCatSense sense;
	char *version;
} PackageDependency;

PackageDependency *packagedependency_new (void);
PackageDependency *packagedependency_copy (const PackageDependency *dep, gboolean deep);
void packagedependency_destroy (PackageDependency *dep);

/* WAAH! */
#define PACKAGEDEPENDENCY(obj) ((PackageDependency*)(obj))
#define IS_PACKAGEDEPENDENCY(obj) (1)

/*************************************************************************************************/

#define TYPE_PACKAGEBREAKS           (packagebreaks_get_type ())
#define PACKAGEBREAKS(obj)           (GTK_CHECK_CAST ((obj), TYPE_PACKAGEBREAKS, PackageBreaks))
#define PACKAGEBREAKS_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TYPE_PACKAGEBREAKS, PackageBreaksClass))
#define IS_PACKAGEBREAKS(obj)        (GTK_CHECK_TYPE ((obj), TYPE_PACKAGEBREAKS))
#define IS_PACKAGEBREAKS_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_PACKAGEBREAKS))

typedef struct _PackageBreaks PackageBreaks;
typedef struct _PackageBreaksClass PackageBreaksClass;

struct _PackageBreaksClass {
	GtkObjectClass parent_class;
	void (*finalize) (GtkObject *obj);
};

struct _PackageBreaks {
	GtkObject parent;
	PackageData *__package;
};

PackageBreaks* packagebreaks_new (void);
GtkType packagebreaks_get_type (void);
void packagebreaks_set_package (PackageBreaks *breaks, PackageData *pack);
PackageData *packagebreaks_get_package (PackageBreaks *breaks);

#define TYPE_PACKAGEFILECONFLICT           (packagefileconflict_get_type ())
#define PACKAGEFILECONFLICT(obj)           (GTK_CHECK_CAST ((obj), TYPE_PACKAGEFILECONFLICT, PackageFileConflict))
#define PACKAGEFILECONFLICT_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TYPE_PACKAGEFILECONFLICT, PackageFileConflictClass))
#define IS_PACKAGEFILECONFLICT(obj)        (GTK_CHECK_TYPE ((obj), TYPE_PACKAGEFILECONFLICT))
#define IS_PACKAGEFILECONFLICT_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_PACKAGEFILECONFLICT))

typedef struct _PackageFileConflict PackageFileConflict;
typedef struct _PackageFileConflictClass PackageFileConflictClass;

struct _PackageFileConflictClass {
	PackageBreaksClass parent_class;
	void (*finalize) (GtkObject *obj);
};

struct _PackageFileConflict {
	PackageBreaks parent;
	GList *files;
};

PackageFileConflict* packagefileconflict_new (void);
GtkType packagefileconflict_get_type (void);

#define TYPE_PACKAGEFEATUREMISSING           (packagefeaturemissing_get_type ())
#define PACKAGEFEATUREMISSING(obj)           (GTK_CHECK_CAST ((obj), TYPE_PACKAGEFEATUREMISSING, PackageFeatureMissing))
#define PACKAGEFEATUREMISSING_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TYPE_PACKAGEFEATUREMISSING, PackageFeatureMissingClass))
#define IS_PACKAGEFEATUREMISSING(obj)        (GTK_CHECK_TYPE ((obj), TYPE_PACKAGEFEATUREMISSING))
#define IS_PACKAGEFEATUREMISSING_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_PACKAGEFEATUREMISSING))

typedef struct _PackageFeatureMissing PackageFeatureMissing;
typedef struct _PackageFeatureMissingClass PackageFeatureMissingClass;

struct _PackageFeatureMissingClass {
	PackageBreaksClass parent_class;
	void (*finalize) (GtkObject *obj);
};

struct _PackageFeatureMissing {
	PackageBreaks parent;
	GList *features;
};

#define IS_VALID_PACKAGEBREAKS(obj)		(IS_PACKAGEFEATUREMISSING (obj) || IS_PACKAGEFILECONFLICT (obj))

PackageFeatureMissing* packagefeaturemissing_new (void);
GtkType packagefeaturemissing_get_type (void);


/*************************************************************************************************/

void packagedata_add_to_breaks (PackageData *pack, PackageBreaks *b);
void packagedata_add_pack_to_breaks (PackageData *pack, PackageData *b);
void packagedata_add_pack_to_depends (PackageData *pack, PackageDependency *b);
void packagedata_add_pack_to_modifies (PackageData *pack, PackageData *b);

/*************************************************************************************************/
/* FIXME: deprecating eazel-install-logic.c will also deprecate this structure */

typedef struct {
	PackageData *package;
	PackageData *required;
} PackageRequirement;

PackageRequirement* packagerequirement_new (PackageData *pack, PackageData *req);

/* glib style ompares */

int eazel_install_package_provides_basename_compare (char *a, char *b);
int eazel_install_package_provides_compare (PackageData *pack, char *name);
int eazel_install_package_name_compare (PackageData *pack, char *name);
int eazel_install_package_compare (PackageData *pack, PackageData *other);
int eazel_install_requirement_dep_name_compare (PackageRequirement *req, const char *name);
int eazel_install_requirement_dep_compare (PackageRequirement *req, PackageData *pack);
int eazel_install_package_version_compare (PackageData *pack, char *version);
int eazel_install_package_other_version_compare (PackageData *pack, PackageData *other);

/* Other compare functions */

/* Specific compare where b is more complete then a, do not use in
   glib functions.  both version and minor can be null, however, if
   version is null, minor must also be null. */
int eazel_install_package_matches_versioning (PackageData *a, const char *version, 
					      const char *minor, EazelSoftCatSense);

/* Evil marshal func */

void eazel_install_gtk_marshal_NONE__POINTER_INT_INT_INT_INT_INT_INT (GtkObject * object,
								      GtkSignalFunc func,
								      gpointer func_data, GtkArg * args);

void eazel_install_gtk_marshal_BOOL__ENUM_POINTER_INT_INT (GtkObject * object,
							   GtkSignalFunc func,
							   gpointer func_data, GtkArg * args);

void eazel_install_gtk_marshal_BOOL__ENUM_POINTER (GtkObject * object,
                                                   GtkSignalFunc func,
                                                   gpointer func_data, GtkArg * args);

char *packagedata_dump_tree (const GList *packlst, int indent_level);
char *packagedata_dump (const PackageData *package, gboolean deep);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EAZEL_PACKAGE_SYSTEM_TYPES_H */
