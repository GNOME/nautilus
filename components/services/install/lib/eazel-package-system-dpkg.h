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
 *          Ian McKellar <ian@eazel.com>
 *
 */

#ifndef EAZEL_PACKAGE_SYSTEM_DPKG_H
#define EAZEL_PACKAGE_SYSTEM_DPKG_H

#include "eazel-package-system.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TYPE_EAZEL_PACKAGE_SYSTEM_DPKG           (eazel_package_system_dpkg_get_type ())
#define EAZEL_PACKAGE_SYSTEM_DPKG(obj)           (GTK_CHECK_CAST ((obj), TYPE_EAZEL_PACKAGE_SYSTEM_DPKG, EazelPackageSystemDpkg))
#define EAZEL_PACKAGE_SYSTEM_DPKG_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TYPE_EAZEL_PACKAGE_SYSTEM_DPKG, EazelPackageSystemDpkgClass))
#define EAZEL_IS_PACKAGE_SYSTEM_DPKG(obj)        (GTK_CHECK_TYPE ((obj), TYPE_EAZEL_PACKAGE_SYSTEM_DPKG))
#define EAZEL_IS_PACKAGE_SYSTEM_DPKG_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_EAZEL_PACKAGE_SYSTEM_DPKG))

typedef struct _EazelPackageSystemDpkg EazelPackageSystemDpkg;
typedef struct _EazelPackageSystemDpkgClass EazelPackageSystemDpkgClass;


struct _EazelPackageSystemDpkgClass
{
	EazelPackageSystemClass parent_class;
};

struct _EazelPackageSystemDpkg
{
	EazelPackageSystem parent;
};

EazelPackageSystemDpkg *eazel_package_system_dpkg_new (GList *dbpaths);
GtkType              eazel_package_system_dpkg_get_type (void);

#endif /* EAZEL_PACKAGE_SYSTEM_DPKG_H */

