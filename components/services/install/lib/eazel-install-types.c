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

#include <rpm/rpmlib.h>
#include <rpm/rpmmacro.h>
#include <rpm/dbindex.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

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
	PackageData *pack;
	pack = g_new0 (PackageData, 1);

	pack->name = NULL;
	pack->version = NULL;
	pack->minor = NULL;
	pack->archtype = NULL;
	pack->summary = NULL;
	pack->bytesize = 0;
	pack->distribution = trilobite_get_distribution ();
	pack->filename = NULL;
	pack->soft_depends = NULL;
	pack->hard_depends = NULL;
	pack->breaks = NULL;
	return pack;
}

PackageData*
packagedata_new_from_rpm_conflict (struct rpmDependencyConflict conflict) 
{
	PackageData *result;
	
	result = packagedata_new ();

	result->name = g_strdup (conflict.needsName);
	result->version = (conflict.needsVersion && (strlen (conflict.needsVersion) > 1)) ? g_strdup (conflict.needsVersion) : NULL;
	return result;
}

PackageData*
packagedata_new_from_rpm_conflict_reversed (struct rpmDependencyConflict conflict) 
{
	PackageData *result;
	
	result = packagedata_new ();

	result->name = g_strdup (conflict.byName);
	result->version = (conflict.byVersion && (strlen (conflict.byVersion) > 1)) ? g_strdup (conflict.byVersion) : NULL;
	return result;
}

PackageData*
packagedata_new_from_rpm_header (Header *hd) 
{
	PackageData *pack;

	pack = packagedata_new ();

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
packagedata_destroy_foreach (PackageData *pack, gpointer unused)
{
	g_return_if_fail (pack != NULL);
	g_free (pack->name);
	pack->name = NULL;
	g_free (pack->version);
	pack->version = NULL;
	g_free (pack->minor);
	pack->minor = NULL;
	g_free (pack->archtype);
	pack->archtype = NULL;
	g_free (pack->summary);
	pack->summary = NULL;
	pack->bytesize = 0;
	g_free (pack->filename);
	pack->filename = NULL;

	g_list_foreach (pack->soft_depends, (GFunc)packagedata_destroy_foreach, NULL);
	g_list_foreach (pack->hard_depends, (GFunc)packagedata_destroy_foreach, NULL);
	g_list_foreach (pack->breaks, (GFunc)packagedata_destroy_foreach, NULL);
	pack->soft_depends = NULL;
	pack->hard_depends = NULL;
	pack->breaks = NULL;

	if (pack->packsys_struc) {
		/* FIXME: bugzilla.eazel.com 1532
		   RPM specific code */
		/* even better, this just crashes 
		   headerFree (*pack->packsys_struc); */
		g_free (pack->packsys_struc);
	}
}

void 
packagedata_destroy (PackageData *pack)
{
	packagedata_destroy_foreach (pack, NULL);
}

const char*
rpmfilename_from_packagedata (const PackageData *pack)
{
	static char *filename = NULL;
	
	g_free (filename);
	if (pack->filename) {
		filename = g_strdup (pack->filename);
	} else {
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
			filename = g_strconcat (pack->name, NULL); 
		}
	}

	return filename;
}

const char*
rpmname_from_packagedata (const PackageData *pack)
{
	static char *name = NULL;
	
	g_free (name);
	
	if (pack->version && pack->minor) {
		name = g_strdup_printf ("%s-%s-%s",
					pack->name,
					pack->version,
					pack->minor);
	} else if (pack->version) {
		name = g_strdup_printf ("%s-%s",
					pack->name,
					pack->version);
	} else {
		name = g_strconcat (pack->name, NULL); 
	}

	return name;
}

int 
packagedata_hash (PackageData *pack)
{
	g_assert (pack!=NULL);
	g_assert (pack->name!=NULL);
	return strlen (pack->name);
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


const char*
packagedata_status_enum_to_str (PackageSystemStatus st)
{
	static char *result=NULL;
	g_free (result);

	switch (st) {
	case PACKAGE_UNKNOWN_STATUS:
		result = g_strdup ("UNKNOWN_STATUS");
		break;
	case PACKAGE_SOURCE_NOT_SUPPORTED:
		result = g_strdup ("SOURCE_NOT_SUPPORTED");
		break;
	case PACKAGE_DEPENDENCY_FAIL:
		result = g_strdup ("DEPENDENCY_FAIL");
		break;
	case PACKAGE_BREAKS_DEPENDENCY:
		result = g_strdup ("BREAKS_DEPENDENCY");
		break;
	case PACKAGE_INVALID:
		result = g_strdup ("INVALID");
		break;
	case PACKAGE_CANNOT_OPEN:
		result = g_strdup ("CANNOT_OPEN");
		break;
	case PACKAGE_PARTLY_RESOLVED:
		result = g_strdup ("PARTLY_RESOLVED");
		break;
	case PACKAGE_RESOLVED:
		result = g_strdup ("RESOLVED");
		break;
	}
	return result;
}

PackageSystemStatus
packagedata_status_str_to_enum (const char *st)
{
	PackageSystemStatus result;
	
	if (strcmp (st, "UNKNOWN_STATUS")==0) { result = PACKAGE_UNKNOWN_STATUS; } 
	else if (strcmp (st, "SOURCE_NOT_SUPPORTED")==0) { result = PACKAGE_SOURCE_NOT_SUPPORTED; } 
	else if (strcmp (st, "DEPENDENCY_FAIL")==0) { result = PACKAGE_DEPENDENCY_FAIL; } 
	else if (strcmp (st, "BREAKS_DEPENDENCY")==0) { result = PACKAGE_BREAKS_DEPENDENCY; } 
	else if (strcmp (st, "INVALID")==0) { result = PACKAGE_INVALID; } 
	else if (strcmp (st, "CANNOT_OPEN")==0) { result = PACKAGE_CANNOT_OPEN; } 
	else if (strcmp (st, "PARTLY_RESOLVED")==0) { result = PACKAGE_PARTLY_RESOLVED; } 
	else if (strcmp (st, "RESOLVED")==0) { result = PACKAGE_RESOLVED; } 

	return result;
}
