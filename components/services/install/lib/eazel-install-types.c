/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
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
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 */

/*
  I'm declaring these _foreach, since we can then export their prototypes in the 
  api
 */

#include <config.h>
#include "eazel-install-types.h"

void
categorydata_destroy_foreach (CategoryData *cd, gpointer ununsed)
{
	g_return_if_fail (cd != NULL);
	g_free (cd->name);
	cd->name = NULL;
	g_list_foreach (cd->packages, (GFunc)packagedata_destroy_foreach, NULL);
}

void
categorydata_destroy (CategoryData *cd)
{
	categorydata_destroy_foreach (cd, NULL);
}

PackageData*
packagedata_new_from_rpm_header (Header hd) 
{
	PackageData *pd;

	pd = g_new0 (PackageData, 1);
	headerGetEntry (hd,
			RPMTAG_NAME, NULL,
			(void **) &pd->name, NULL);
	headerGetEntry (hd,
			RPMTAG_VERSION, NULL,
			(void **) &pd->version, NULL);
	headerGetEntry (hd,
			RPMTAG_RELEASE, NULL,
			(void **) &pd->minor, NULL);
	headerGetEntry (hd,
			RPMTAG_ARCH, NULL,
			(void **) &pd->archtype, NULL);
	headerGetEntry (hd,
			RPMTAG_SIZE, NULL,
			(void **) &pd->bytesize, NULL);
	headerGetEntry (hd,
			RPMTAG_SUMMARY, NULL,
			(void **) &pd->summary, NULL);
	return pd;
};

void 
packagedata_destroy_foreach (PackageData *pd, gpointer unused)
{
	g_return_if_fail (pd != NULL);
	g_free (pd->name);
	pd->name = NULL;
	g_free (pd->version);
	pd->version = NULL;
	g_free (pd->minor);
	pd->minor = NULL;
	g_free (pd->archtype);
	pd->archtype = NULL;
	g_free (pd->summary);
	pd->summary = NULL;
	pd->bytesize = 0;
	g_list_foreach (pd->soft_depends, (GFunc)packagedata_destroy_foreach, NULL);
	g_list_foreach (pd->hard_depends, (GFunc)packagedata_destroy_foreach, NULL);
}

void 
packagedata_destroy (PackageData *pd)
{
	packagedata_destroy_foreach (pd, NULL);
}
