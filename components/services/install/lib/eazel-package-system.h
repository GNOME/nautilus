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

#ifndef EAZEL_PACKAGE_SYSTEM_PUBLIC_H
#define EAZEL_PACKAGE_SYSTEM_PUBLIC_H

#include "eazel-install-types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TYPE_EAZEL_PACKAGE_SYSTEM           (eazel_package_system_get_type ())
#define EAZEL_PACKAGE_SYSTEM(obj)           (GTK_CHECK_CAST ((obj), TYPE_EAZEL_PACKAGE_SYSTEM, EazelPackageSystem))
#define EAZEL_PACKAGE_SYSTEM_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TYPE_EAZEL_PACKAGE_SYSTEM, EazelPackageSystemClass))
#define IS_EAZEL_PACKAGE_SYSTEM(obj)        (GTK_CHECK_TYPE ((obj), TYPE_EAZEL_PACKAGE_SYSTEM))
#define IS_EAZEL_PACKAGE_SYSTEM_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_EAZEL_PACKAGE_SYSTEM))

typedef struct _EazelPackageSystem EazelPackageSystem;
typedef struct _EazelPackageSystemClass EazelPackageSystemClass;

/* This enum identifies the package system
   used for the object instance */
typedef enum {
	EAZEL_PACKAGE_SYSTEM_RPM_3,
	EAZEL_PACKAGE_SYSTEM_RPM_4,
	EAZEL_PACKAGE_SYSTEM_DEB
} EazelPackageSystemId;

/* This is query enums, and specifies what the "key" in
   eazel_package_system_query means */
typedef enum {
	/* "key" is a const char* filename, eg "/usr/lib/libglib.so.1" 
	    returned packages are packages that are 
           listed as owning the specified file */
	EAZEL_PACKAGE_SYSTEM_QUERY_OWNS,

        /* "key" is a const char* feature, eg. "libglib-1.2.so.0"
	   returned pacakges are packages that are listed as providing
	   this feature */
	EAZEL_PACKAGE_SYSTEM_QUERY_PROVIDES,

	/* "key" is a const PackageData* packageobject, the following
	    fields are needed :
           p->name must be set
           p->version must be set
           p->release must be set
	   returned packages are packages listed as requiring the
           matched package. This means first a query is done for
           all packages matching the given name-version-release,
           and then a search for packages requiring this */
	EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES,

	/* "key" is a const char* package-name-regexp eg. "glib", "gnome-.*" ".*-devel"
	   returned packages are packages that has names that matches the given regexp.
	    [3] */
	EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
} EazelPackageSystemQueryEnum;

enum {
	/* Do operation in "test" mode. If package system does not have this, 
	   don't do the operation */
	EAZEL_INSTALL_PACKAGE_SYSTEM_OPERATION_TEST = 0x1,

	/* Do operation is force mode if available, if not, you're screwed */
	EAZEL_INSTALL_PACKAGE_SYSTEM_OPERATION_FORCE = 0x2,

	/* Allow upgrading packages (only used for install) */   
	EAZEL_INSTALL_PACKAGE_SYSTEM_OPERATION_UPGRADE = 0x10, 

	/* Allow downgrading packages (only used for install) */   
	EAZEL_INSTALL_PACKAGE_SYSTEM_OPERATION_DOWNGRADE = 0x20
};

enum {
	EAZEL_INSTALL_PACKAGE_SYSTEM_QUERY_DETAIL_SHORT_SUMMARY = 0x1,
	EAZEL_INSTALL_PACKAGE_SYSTEM_QUERY_DETAIL_LONG_SUMMARY = 0x2,
	EAZEL_INSTALL_PACKAGE_SYSTEM_QUERY_DETAIL_FILES_PROVIDED = 0x4,
	EAZEL_INSTALL_PACKAGE_SYSTEM_QUERY_DETAIL_PROVIDES = 0x8
};

/* This enum is used in the signals, to let
   the signal handler know what the current operation
   was (rather then the client having to remember it) */
typedef enum {
	EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL,
	EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL,
	EAZEL_PACKAGE_SYSTEM_OPERATION_VERIFY,
} EazelPackageSystemOperation;

struct _EazelPackageSystemClass
{
	GtkObjectClass parent_class;
	void (*start)(EazelPackageSystem*, 
		      EazelPackageSystemOperation, 
		      PackageData*);
	void (*progress)(EazelPackageSystem*, 
			 EazelPackageSystemOperation, 
			 PackageData*, 
			 unsigned long[6]);
	void (*failed)(EazelPackageSystem*, 
		       EazelPackageSystemOperation, 
		       PackageData*);
	void (*end)(EazelPackageSystem*, 
		    EazelPackageSystemOperation, 
		    PackageData*);
};

typedef struct _EazelPackageSystemPrivate EazelPackageSystemPrivate;

struct _EazelPackageSystem
{
	GtkObject parent;
	EazelPackageSystemPrivate *private;
};

EazelPackageSystemId eazel_package_system_suggest_id (void);
EazelPackageSystem  *eazel_package_system_new (void);
EazelPackageSystem  *eazel_package_system_new_with_id (EazelPackageSystemId);
GtkType              eazel_package_system_get_type (void);
void                 eazel_package_system_unref        (GtkObject *object);
PackageData         *eazel_package_system_load_package (EazelPackageSystem *package_system,
							const char *filename,
							int detail_level);
GList*               eazel_package_system_query (EazelPackageSystem *package_system,
						 const gpointer key,
						 EazelPackageSystemQueryEnum flag,
						 int detail_level);
void                 eazel_package_system_install (EazelPackageSystem *package_system, 
						   GList* packages,
						   long flags,
						   gpointer userdata);
void                 eazel_package_system_uninstall (EazelPackageSystem *package_system, 
						     GList* packages,
						     long flags,
						     gpointer userdata);
void                 eazel_package_system_verify (EazelPackageSystem *package_system, 
						  GList* packages,
						  long flags,
						  gpointer userdata);

#endif /* EAZEL_PACKAGE_SYSTEM_PUBLIC_H */

