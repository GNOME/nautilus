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

#ifndef EAZEL_PACKAGE_SYSTEM_PRIVATE_H
#define EAZEL_PACKAGE_SYSTEM_PRIVATE_H

#include "eazel-package-system.h"

#define EAZEL_PACKAGE_SYSTEM_PROGRESS_LONGS 6

#define EPS_SANE(val) g_return_if_fail (val!=NULL); \
                      g_return_if_fail (EAZEL_IS_PACKAGE_SYSTEM (val)); \
                      g_return_if_fail (val->private); 

#define EPS_SANE_VAL(val, v) g_return_val_if_fail (val!=NULL, v); \
                             g_return_val_if_fail (EAZEL_IS_PACKAGE_SYSTEM (val), v); \
                             g_return_val_if_fail (val->private, v); 

#define EPS_API(val) g_assert (val!=NULL); g_assert (EAZEL_IS_PACKAGE_SYSTEM (val)); g_assert (val->private);

#define info(system, s...) if (eazel_package_system_get_debug (EAZEL_PACKAGE_SYSTEM (system)) & EAZEL_PACKAGE_SYSTEM_DEBUG_INFO) { trilobite_debug (s); }
#define fail(system, s...) if (eazel_package_system_get_debug (EAZEL_PACKAGE_SYSTEM (system)) & EAZEL_PACKAGE_SYSTEM_DEBUG_FAIL) { trilobite_debug (s); }
#define verbose(system, s...) if (eazel_package_system_get_debug (EAZEL_PACKAGE_SYSTEM (system)) & EAZEL_PACKAGE_SYSTEM_DEBUG_VERBOSE) { trilobite_debug (s); }

typedef EazelPackageSystem*(*EazelPackageSystemConstructorFunc) (GList*);

typedef	PackageData* (*EazelPackageSytemLoadPackageFunc) (EazelPackageSystem*,
							  PackageData*, 
							  const char*, 
							  int);
typedef GList* (*EazelPackageSytemQueryFunc) (EazelPackageSystem*, 
					      const char*, 
					      gpointer key, 
					      EazelPackageSystemQueryEnum, 
					      int);
typedef void (*EazelPackageSytemInstallFunc) (EazelPackageSystem*, 
					      const char*,
					      GList*, 
					      unsigned long);
typedef void (*EazelPackageSytemUninstallFunc) (EazelPackageSystem*, 
						const char *,
						GList*, 
						unsigned long);
typedef gboolean (*EazelPackageSytemVerifyFunc) (EazelPackageSystem*, 
						 const char*,
						 GList*);
typedef int (*EazelPackageSystemCompareVersionFunc) (EazelPackageSystem*, 
						     const char *,
						     const char *);

struct _EazelPackageSystemPrivate {	
	EazelPackageSytemLoadPackageFunc load_package;
	EazelPackageSytemQueryFunc query;
	EazelPackageSytemInstallFunc install;
	EazelPackageSytemUninstallFunc uninstall;
	EazelPackageSytemVerifyFunc verify;
	EazelPackageSystemCompareVersionFunc compare_version;

	EazelPackageSystemDebug debug;
};

EazelPackageSystem  *eazel_package_system_new_real (void);

gboolean eazel_package_system_emit_start (EazelPackageSystem*, 
					  EazelPackageSystemOperation, 
					  const PackageData*);
gboolean eazel_package_system_emit_progress (EazelPackageSystem*, 
					     EazelPackageSystemOperation, 
					     const PackageData*, 
					     unsigned long[EAZEL_PACKAGE_SYSTEM_PROGRESS_LONGS]);
gboolean eazel_package_system_emit_failed (EazelPackageSystem*, 
					   EazelPackageSystemOperation, 
					   const PackageData*);
gboolean eazel_package_system_emit_end (EazelPackageSystem*, 
					EazelPackageSystemOperation, 
					const PackageData*);

void eazel_package_system_marshal_BOOL__ENUM_POINTER (GtkObject *object,
						      GtkSignalFunc func,
						      gpointer func_data,
						      GtkArg *args);
void eazel_package_system_marshal_BOOL__ENUM_POINTER_POINTER (GtkObject *object,
							      GtkSignalFunc func,
							      gpointer func_data,
							      GtkArg *args);

#endif /* EAZEL_PACKAGE_SYSTEM_PRIVATE_H */
