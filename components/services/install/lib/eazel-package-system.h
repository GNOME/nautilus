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

#include "eazel-package-system-types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TYPE_EAZEL_PACKAGE_SYSTEM           (eazel_package_system_get_type ())
#define EAZEL_PACKAGE_SYSTEM(obj)           (GTK_CHECK_CAST ((obj), TYPE_EAZEL_PACKAGE_SYSTEM, EazelPackageSystem))
#define EAZEL_PACKAGE_SYSTEM_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TYPE_EAZEL_PACKAGE_SYSTEM, EazelPackageSystemClass))
#define EAZEL_IS_PACKAGE_SYSTEM(obj)        (GTK_CHECK_TYPE ((obj), TYPE_EAZEL_PACKAGE_SYSTEM))
#define EAZEL_IS_PACKAGE_SYSTEM_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_EAZEL_PACKAGE_SYSTEM))

typedef struct _EazelPackageSystem EazelPackageSystem;
typedef struct _EazelPackageSystemClass EazelPackageSystemClass;

/* This enum identifies the package system
   used for the object instance */
typedef enum {
	EAZEL_PACKAGE_SYSTEM_UNSUPPORTED,
	EAZEL_PACKAGE_SYSTEM_RPM_3,
	EAZEL_PACKAGE_SYSTEM_RPM_4,
	EAZEL_PACKAGE_SYSTEM_DEB,
} EazelPackageSystemId;

typedef enum {
	EAZEL_PACKAGE_SYSTEM_QUERY_OWNS,
	EAZEL_PACKAGE_SYSTEM_QUERY_PROVIDES,
	EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES,
	EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES_FEATURE,
	EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
	EAZEL_PACKAGE_SYSTEM_QUERY_SUBSTR
} EazelPackageSystemQueryEnum;

enum {
	EAZEL_PACKAGE_SYSTEM_OPERATION_TEST = 0x1,
	EAZEL_PACKAGE_SYSTEM_OPERATION_FORCE = 0x2,
	EAZEL_PACKAGE_SYSTEM_OPERATION_UPGRADE = 0x10, 
	EAZEL_PACKAGE_SYSTEM_OPERATION_DOWNGRADE = 0x20
};

typedef enum {
	EAZEL_PACKAGE_SYSTEM_OPERATION_INSTALL,
	EAZEL_PACKAGE_SYSTEM_OPERATION_UNINSTALL,
	EAZEL_PACKAGE_SYSTEM_OPERATION_VERIFY,
} EazelPackageSystemOperation;

struct _EazelPackageSystemClass
{
	GtkObjectClass parent_class;
	gboolean (*start)(EazelPackageSystem*, 
			  EazelPackageSystemOperation, 
			  const PackageData*,
			  unsigned long*);
	gboolean (*progress)(EazelPackageSystem*, 
			     EazelPackageSystemOperation, 
			     const PackageData*, 
			     unsigned long*);
	gboolean (*failed)(EazelPackageSystem*, 
			   EazelPackageSystemOperation, 
			   const PackageData*);
	gboolean (*end)(EazelPackageSystem*, 
			EazelPackageSystemOperation, 
			const PackageData*);
};

typedef enum {
	EAZEL_PACKAGE_SYSTEM_DEBUG_SILENT = 0x0,
	EAZEL_PACKAGE_SYSTEM_DEBUG_INFO = 0x1,
	EAZEL_PACKAGE_SYSTEM_DEBUG_FAIL = 0x2,
	EAZEL_PACKAGE_SYSTEM_DEBUG_VERBOSE = 0xffff
} EazelPackageSystemDebug;

/* I hate myself for this... please, give me exceptions! */
typedef enum  {
	EazelPackageSystemError_DB_ACCESS
} EazelPackageSystemErrorEnum;

typedef struct _EazelPackageSystemError EazelPackageSystemError;
struct  _EazelPackageSystemError {	
	EazelPackageSystemErrorEnum e;
	union {
		struct {
			const char *path;
			pid_t pid;
		} db_access;
	} u;
};

typedef struct _EazelPackageSystemPrivate EazelPackageSystemPrivate;
struct _EazelPackageSystem
{
	GtkObject parent;
	EazelPackageSystemPrivate *private;
	EazelPackageSystemError *err;
};

EazelPackageSystemId eazel_package_system_suggest_id (void);
EazelPackageSystem  *eazel_package_system_new (GList *dbpaths);
EazelPackageSystem  *eazel_package_system_new_with_id (EazelPackageSystemId, GList *dbpaths);
GtkType              eazel_package_system_get_type (void);

EazelPackageSystemDebug eazel_package_system_get_debug (EazelPackageSystem *system);
void                    eazel_package_system_set_debug (EazelPackageSystem *system, EazelPackageSystemDebug d);

gboolean             eazel_package_system_is_installed (EazelPackageSystem *package_system,
							const char *dbpath,
							const char *name,
							const char *version,
							const char *minor,
							EazelSoftCatSense version_sense);

PackageData         *eazel_package_system_load_package (EazelPackageSystem *package_system,
							PackageData *in_package,
							const char *filename,
							int detail_level);
GList*               eazel_package_system_query (EazelPackageSystem *package_system,
						 const char *dbpath,
						 const gpointer key,
						 EazelPackageSystemQueryEnum flag,
						 int detail_level);
void                 eazel_package_system_install (EazelPackageSystem *package_system, 
						   const char *dbpath,
						   GList* packages,
						   unsigned long flags);
void                 eazel_package_system_uninstall (EazelPackageSystem *package_system, 
						     const char *dbpath,
						     GList* packages,
						     unsigned long flags);
gboolean                 eazel_package_system_verify (EazelPackageSystem *package_system, 
						      const char *dbpath,
						      GList* packages);
int                  eazel_package_system_compare_version (EazelPackageSystem *package_system, 
							   const char *a,
							   const char *b);
time_t               eazel_package_system_database_mtime (EazelPackageSystem *package_system);

#endif /* EAZEL_PACKAGE_SYSTEM_PUBLIC_H */

