/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
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
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 */

#ifndef EAZEL_SERVICES_DISTRIBUTION_H
#define EAZEL_SERVICES_DISTRIBUTION_H

#include <gnome.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct _DistributionInfo DistributionInfo; 
typedef enum _DistributionName DistributionName;

enum _DistributionName {
	DISTRO_UNKNOWN = 0,
	DISTRO_REDHAT,
	DISTRO_DEBIAN,
	DISTRO_CALDERA,
	DISTRO_SUSE,
	DISTRO_LINUXPPC,
	DISTRO_TURBOLINUX,
	DISTRO_COREL,
	DISTRO_MANDRAKE
};

struct _DistributionInfo {
	DistributionName name;
	int version_major; /* -1 equals unset */
	int version_minor; /* -1 equals unset */
};

/*
  Returns a structure containing the distribution
*/
DistributionInfo trilobite_get_distribution (void);

/* Return the distribution name, optinally with version number
   Return value must be freed 
*/
char* trilobite_get_distribution_name (DistributionInfo distinfo,
				       gboolean show_version,
				       gboolean compact);

/*
  Returns the enum corresponding to the given string,
  which should be one of the strings that trilobite_get_distribution_name
  returns. The version part (if show_version = TRUE was used) is
  _not_ parsed */

DistributionName trilobite_get_distribution_enum (const char *name);

#endif /* EAZEL_SERVICES_DISTRIBUTION_H */

