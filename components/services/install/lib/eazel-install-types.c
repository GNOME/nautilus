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

const char*
protocol_as_string (URLType protocol) 
{
	static char as_string[10];
	switch (protocol) {
	case PROTOCOL_HTTP:
		strcpy (as_string, "http");
		break;
	case PROTOCOL_FTP:
		strcpy (as_string, "ftp");
		break;
	case PROTOCOL_LOCAL:
		strcpy (as_string, "file");
		break;
	}
	return as_string;
}

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
packagedata_new ()
{
	return g_new0 (PackageData, 1);
}

PackageData*
packagedata_new_from_rpm_conflict (struct rpmDependencyConflict conflict) 
{
	PackageData *result;
	
	result = g_new0 (PackageData,1);

	result->name = g_strdup (conflict.needsName);
	result->version = (conflict.needsVersion && (strlen (conflict.needsVersion) > 1)) ? g_strdup (conflict.needsVersion) : NULL;
	return result;
}

PackageData*
packagedata_new_from_rpm_conflict_reversed (struct rpmDependencyConflict conflict) 
{
	PackageData *result;
	
	result = g_new0 (PackageData,1);

	result->name = g_strdup (conflict.byName);
	result->version = (conflict.byVersion && (strlen (conflict.byVersion) > 1)) ? g_strdup (conflict.byVersion) : NULL;
	return result;
}

PackageData*
packagedata_new_from_rpm_header (Header *hd) 
{
	PackageData *pack;

	pack = g_new0 (PackageData, 1);

	packagedata_fill_from_rpm_header (pack, hd);

	pack->status = PACKAGE_UNKNOWN_STATUS;
	pack->toplevel = FALSE;
	return pack;
};

void 
packagedata_fill_from_rpm_header (PackageData *pack, 
				  Header *hd) 
{
	headerGetEntry (*hd,
			RPMTAG_NAME, NULL,
			(void **) &pack->name, NULL);
	headerGetEntry (*hd,
			RPMTAG_VERSION, NULL,
			(void **) &pack->version, NULL);
	headerGetEntry (*hd,
			RPMTAG_RELEASE, NULL,
			(void **) &pack->minor, NULL);
	headerGetEntry (*hd,
			RPMTAG_ARCH, NULL,
			(void **) &pack->archtype, NULL);
	headerGetEntry (*hd,
			RPMTAG_SIZE, NULL,
			(void **) &pack->bytesize, NULL);
	headerGetEntry (*hd,
			RPMTAG_SUMMARY, NULL,
			(void **) &pack->summary, NULL);
	pack->packsys_struc = (gpointer)hd;
}

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
	g_list_foreach (pd->breaks, (GFunc)packagedata_destroy_foreach, NULL);
}

void 
packagedata_destroy (PackageData *pd)
{
	packagedata_destroy_foreach (pd, NULL);
}

const char*
rpmfilename_from_packagedata (const PackageData *pack)
{
	static char *filename = NULL;

	g_free (filename);
	if (pack->version && pack->minor) {
		filename = g_strdup_printf ("%s-%s-%s.%s.rpm",
					    pack->name,
					    pack->version,
					    pack->minor,
					    pack->archtype);
	} else if (pack->archtype) {
		filename = g_strdup_printf ("%s.%s.rpm",
					    pack->name,
					    pack->archtype);
	} else {
		filename = g_strconcat (pack->name,".rpm", NULL); 
	}

	return filename;
}

int 
packagedata_hash (PackageData *pd)
{
	g_assert (pd!=NULL);
	g_assert (pd->name!=NULL);
	return strlen (pd->name);
}

int 
packagedata_equal (PackageData *a, 
		   PackageData *b)
{
	g_assert (a!=NULL);
	g_assert (a->name!=NULL);
	g_assert (b!=NULL);
	g_assert (b->name!=NULL);
	
	return strcmp (a->name, b->name);
}
