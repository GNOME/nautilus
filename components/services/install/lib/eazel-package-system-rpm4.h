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

#ifndef EAZEL_PACKAGE_SYSTEM_RPM4_H
#define EAZEL_PACKAGE_SYSTEM_RPM4_H

#include "eazel-package-system-rpm3.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TYPE_EAZEL_PACKAGE_SYSTEM_RPM4           (eazel_package_system_rpm4_get_type ())
#define EAZEL_PACKAGE_SYSTEM_RPM4(obj)           (GTK_CHECK_CAST ((obj), TYPE_EAZEL_PACKAGE_SYSTEM_RPM4, EazelPackageSystemRpm4))
#define EAZEL_PACKAGE_SYSTEM_RPM4_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TYPE_EAZEL_PACKAGE_SYSTEM_RPM4, EazelPackageSystemRpm4Class))
#define EAZEL_IS_PACKAGE_SYSTEM_RPM4(obj)        (GTK_CHECK_TYPE ((obj), TYPE_EAZEL_PACKAGE_SYSTEM_RPM4))
#define EAZEL_IS_PACKAGE_SYSTEM_RPM4_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_EAZEL_PACKAGE_SYSTEM_RPM4))

typedef struct _EazelPackageSystemRpm4 EazelPackageSystemRpm4;
typedef struct _EazelPackageSystemRpm4Class EazelPackageSystemRpm4Class;

struct _EazelPackageSystemRpm4Class
{
	EazelPackageSystemRpm3Class parent_class;
	
};

struct _EazelPackageSystemRpm4
{
	EazelPackageSystemRpm3 parent;
};
EazelPackageSystemRpm4 *eazel_package_system_rpm4_new (GList *dbpaths);
GtkType              eazel_package_system_rpm4_get_type (void);

#endif /* EAZEL_PACKAGE_SYSTEM_RPM4_H */

