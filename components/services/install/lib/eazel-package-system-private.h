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
                      g_return_if_fail (IS_EAZEL_PACKAGE_SYSTEM (val));

#define EPS_SANE_VAL(val, v) g_return_val_if_fail (val!=NULL, v); \
                             g_return_val_if_fail (IS_EAZEL_PACKAGE_SYSTEM (val), v);

#define EPS_API(val) g_assert (val!=NULL); g_assert (IS_EAZEL_PACKAGE_SYSTEM (val));

typedef EazelPackageSystem*(*EazelPackageSystemConstructor) (GList*);

typedef	PackageData* (*EazelPackageSytem_load_package) (EazelPackageSystem*,
							PackageData*, const char*, int);
typedef GList* (*EazelPackageSytem_query) (EazelPackageSystem*, const char*, 
					   gpointer key, EazelPackageSystemQueryEnum, int);
typedef void (*EazelPackageSytem_install) (EazelPackageSystem*, const char*,
					   GList*, long, gpointer);
typedef void (*EazelPackageSytem_uninstall) (EazelPackageSystem*, const char *,
					     GList*, long, gpointer);
typedef void (*EazelPackageSytem_verify) (EazelPackageSystem*, const char*,
					  GList*, long, gpointer);

struct _EazelPackageSystemPrivate {	
	EazelPackageSytem_load_package load_package;
	EazelPackageSytem_query query;
	EazelPackageSytem_install install;
	EazelPackageSytem_uninstall uninstall;
	EazelPackageSytem_verify verify;
};

EazelPackageSystem  *eazel_package_system_new_real (void);

gboolean eazel_package_system_emit_start (EazelPackageSystem*, 
					  EazelPackageSystemOperation, 
					  PackageData*);
gboolean eazel_package_system_emit_progress (EazelPackageSystem*, 
					     EazelPackageSystemOperation, 
					     unsigned long[EAZEL_PACKAGE_SYSTEM_PROGRESS_LONGS],
					     PackageData*);
gboolean eazel_package_system_emit_failed (EazelPackageSystem*, 
					   EazelPackageSystemOperation, 
					   PackageData*);
gboolean eazel_package_system_emit_end (EazelPackageSystem*, 
					EazelPackageSystemOperation, 
					PackageData*);

#endif /* EAZEL_PACKAGE_SYSTEM_PRIVATE_H */
