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

#ifndef EAZEL_PACKAGE_SYSTEM_RPM3_PRIVATE_H
#define EAZEL_PACKAGE_SYSTEM_RPM3_PRIVATE_H

#include "eazel-package-system-rpm3.h"

void eazel_package_system_rpm3_open_dbs (EazelPackageSystemRpm3 *system);
gboolean eazel_package_system_rpm3_close_dbs (EazelPackageSystemRpm3 *system);
gboolean eazel_package_system_rpm3_free_dbs (EazelPackageSystemRpm3 *system);
void eazel_package_system_rpm3_create_dbs (EazelPackageSystemRpm3 *system);
PackageData* eazel_package_system_rpm3_load_package (EazelPackageSystemRpm3 *system,
						     PackageData *in_package,
						     const char *filename,
						     unsigned long detail_level);
void eazel_package_system_rpm3_install (EazelPackageSystemRpm3 *system, 
					const char *dbpath,
					GList* packages,
					unsigned long flags);
void eazel_package_system_rpm3_uninstall (EazelPackageSystemRpm3 *system, 
					  const char *dbpath,
					  GList* packages,
					  unsigned long flags);
void eazel_package_system_rpm3_verify (EazelPackageSystemRpm3 *system, 
				       const char *dbpath,
				       GList* packages);
GList* eazel_package_system_rpm3_query (EazelPackageSystemRpm3 *system,
					const char *dbpath,
					const gpointer key,
					EazelPackageSystemQueryEnum flag,
					unsigned long detail_level);

#endif /* EAZEL_PACKAGE_SYSTEM_RPM3_PRIVATE_H */
