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

#ifndef EAZEL_PACKAGE_SYSTEM_SKELETON_H
#define EAZEL_PACKAGE_SYSTEM_SKELETON_H

#include "eazel-package-system.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define TYPE_EAZEL_PACKAGE_SYSTEM_SKELETON           (eazel_package_system_skeleton_get_type ())
#define EAZEL_PACKAGE_SYSTEM_SKELETON(obj)           (GTK_CHECK_CAST ((obj), TYPE_EAZEL_PACKAGE_SYSTEM_SKELETON, EazelPackageSystemSkeleton))
#define EAZEL_PACKAGE_SYSTEM_SKELETON_CLASS(klass)   (GTK_CHECK_CLASS_CAST ((klass), TYPE_EAZEL_PACKAGE_SYSTEM_SKELETON, EazelPackageSystemSkeletonClass))
#define IS_EAZEL_PACKAGE_SYSTEM_SKELETON(obj)        (GTK_CHECK_TYPE ((obj), TYPE_EAZEL_PACKAGE_SYSTEM_SKELETON))
#define IS_EAZEL_PACKAGE_SYSTEM_SKELETON_CLASS(klass)(GTK_CHECK_CLASS_TYPE ((klass), TYPE_EAZEL_PACKAGE_SYSTEM_SKELETON))

typedef struct _EazelPackageSystemSkeleton EazelPackageSystemSkeleton;
typedef struct _EazelPackageSystemSkeletonClass EazelPackageSystemSkeletonClass;


struct _EazelPackageSystemSkeletonClass
{
	EazelPackageSystemClass parent_class;
};

struct _EazelPackageSystemSkeleton
{
	EazelPackageSystem parent;
};

EazelPackageSystemSkeleton *eazel_package_system_skeleton_new (GList *roots);
GtkType              eazel_package_system_skeleton_get_type (void);

#endif /* EAZEL_PACKAGE_SYSTEM_SKELETON_H */

