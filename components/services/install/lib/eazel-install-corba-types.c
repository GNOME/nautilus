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

#include "eazel-install-corba-types.h"
#include <libtrilobite/trilobite-core-distribution.h>

static GList*
corba_string_sequence_to_glist (CORBA_sequence_CORBA_string provides) {
	GList *result = NULL;
	guint iterator;

	for (iterator = 0; iterator < provides._length; iterator++) {
		result = g_list_prepend (result, g_strdup (provides._buffer[iterator]));
	}
	return result;
}

static CORBA_sequence_CORBA_string
g_list_to_corba_string_sequence (GList *provides) {
	CORBA_sequence_CORBA_string result;
	GList *iterator;
	int i = 0;

	result._length = g_list_length (provides);
	result._buffer = CORBA_sequence_CORBA_string_allocbuf (result._length);
	for (iterator = provides; iterator; iterator = g_list_next (iterator)) {
		result._buffer[i] = CORBA_string_dup ((char*)iterator->data);
		i++;
	}

	return result;
}

GNOME_Trilobite_Eazel_PackageDataStructList
corba_packagedatastructlist_from_packagedata_list (GList *packages)
{
	GNOME_Trilobite_Eazel_PackageDataStructList packagelist;
	guint i;

	packagelist._length = g_list_length (packages);
	packagelist._buffer = CORBA_sequence_GNOME_Trilobite_Eazel_PackageDataStruct_allocbuf (packagelist._length);
	for (i = 0; i < packagelist._length; i++) {
		PackageData *pack;
		pack = (PackageData*)(g_list_nth (packages,i)->data);
		packagelist._buffer[i] = *(corba_packagedatastruct_from_packagedata (pack));
	}

	return packagelist;
}

GNOME_Trilobite_Eazel_PackageDataStruct*
corba_packagedatastruct_from_packagedata (const PackageData *pack)
{
	GNOME_Trilobite_Eazel_PackageDataStruct *corbapack;

	corbapack = GNOME_Trilobite_Eazel_PackageDataStruct__alloc ();
	corbapack->name = pack->name ? CORBA_string_dup (pack->name) : CORBA_string_dup ("");
	corbapack->eazel_id = pack->eazel_id ? CORBA_string_dup (pack->eazel_id) : CORBA_string_dup ("");
	corbapack->version = pack->version ? CORBA_string_dup (pack->version) : CORBA_string_dup ("");
	corbapack->archtype = pack->archtype ? CORBA_string_dup (pack->archtype) : CORBA_string_dup ("");
	corbapack->filename = pack->filename ? CORBA_string_dup (pack->filename) : CORBA_string_dup ("");

	corbapack->install_root = pack->install_root ? CORBA_string_dup (pack->install_root) : CORBA_string_dup ("");
	corbapack->md5 = pack->md5 ? CORBA_string_dup (pack->md5) : CORBA_string_dup ("");

	if (pack->distribution.name == DISTRO_UNKNOWN) {
		DistributionInfo dist;
		dist = trilobite_get_distribution ();
		corbapack->distribution.name = 
			CORBA_string_dup (trilobite_get_distribution_name (dist, FALSE, FALSE));
		corbapack->distribution.major = dist.version_major;
		corbapack->distribution.minor = dist.version_minor;
	} else {
		corbapack->distribution.name = 
			CORBA_string_dup (trilobite_get_distribution_name (pack->distribution, FALSE, FALSE));
		corbapack->distribution.major = pack->distribution.version_major;
		corbapack->distribution.minor = pack->distribution.version_minor;
	}
	corbapack->release = pack->minor ? CORBA_string_dup (pack->minor) : CORBA_string_dup ("");
	corbapack->summary = pack->summary ? CORBA_string_dup (pack->summary) : CORBA_string_dup ("");
	corbapack->description = pack->description ? CORBA_string_dup (pack->description) : CORBA_string_dup ("");
	corbapack->bytesize = pack->bytesize;
	corbapack->toplevel = pack->toplevel;

	switch (pack->status) {
	case PACKAGE_UNKNOWN_STATUS:
		corbapack->status = GNOME_Trilobite_Eazel_UNKNOWN_STATUS;
		break;
	case PACKAGE_SOURCE_NOT_SUPPORTED:
		corbapack->status = GNOME_Trilobite_Eazel_SOURCE_NOT_SUPPORTED;
		break;
	case PACKAGE_DEPENDENCY_FAIL:
		corbapack->status = GNOME_Trilobite_Eazel_DEPENDENCY_FAIL;
		break;
	case PACKAGE_FILE_CONFLICT:
		corbapack->status = GNOME_Trilobite_Eazel_FILE_CONFLICT;
		break;
	case PACKAGE_BREAKS_DEPENDENCY:
		corbapack->status = GNOME_Trilobite_Eazel_BREAKS_DEPENDENCY;
		break;
	case PACKAGE_INVALID:
		corbapack->status = GNOME_Trilobite_Eazel_INVALID;
		break;
	case PACKAGE_CANNOT_OPEN:
		corbapack->status = GNOME_Trilobite_Eazel_CANNOT_OPEN;
		break;
	case PACKAGE_PARTLY_RESOLVED:
		corbapack->status = GNOME_Trilobite_Eazel_PARTLY_RESOLVED;
		break;
	case PACKAGE_RESOLVED:
		corbapack->status = GNOME_Trilobite_Eazel_RESOLVED;
		break;
	case PACKAGE_ALREADY_INSTALLED:
		corbapack->status = GNOME_Trilobite_Eazel_ALREADY_INSTALLED;
		break;
	case PACKAGE_CIRCULAR_DEPENDENCY:
		corbapack->status = GNOME_Trilobite_Eazel_CIRCULAR_DEPENDENCY;
		break;
	}

	if (pack->provides) {		
		corbapack->provides = g_list_to_corba_string_sequence (pack->provides);
	} else {
		corbapack->provides._length = 0;
		corbapack->provides._buffer = 0;
	}
/*
  FIXME bugzilla.eazel.com 1542:
	if (pack->soft_depends) {
		corbapack->soft_depends = corba_packagedatastructlist_from_packagedata_list (pack->soft_depends);
	} else {
		corbapack->soft_depends._length = 0;
	}
	if (pack->hard_depends) {
		corbapack->hard_depends = corba_packagedatastructlist_from_packagedata_list (pack->hard_depends);
	} else {
		corbapack->hard_depends._length = 0;
	}
	if (pack->breaks) {
		corbapack->breaks = corba_packagedatastructlist_from_packagedata_list (pack->breaks);
	} else {
		corbapack->breaks._length = 0;
	}
*/
	return corbapack;
}

GList*
packagedata_list_from_corba_packagedatastructlist (const GNOME_Trilobite_Eazel_PackageDataStructList corbapack)
{
	GList *result;
	guint i;

	result = NULL;
	
	for (i = 0; i < corbapack._length; i++) {
		PackageData *pack;
		pack = packagedata_from_corba_packagedatastruct (corbapack._buffer[i]);
		result = g_list_prepend (result, pack);
	}

	return result;
}

PackageData*
packagedata_from_corba_packagedatastruct (const GNOME_Trilobite_Eazel_PackageDataStruct corbapack)
{
	PackageData *pack;
	
	pack = packagedata_new();
	pack->name = strlen (corbapack.name) ? g_strdup (corbapack.name) : NULL;
	pack->eazel_id = strlen (corbapack.eazel_id) ? g_strdup (corbapack.eazel_id) : NULL;
	pack->version = strlen (corbapack.version) ? g_strdup (corbapack.version) : NULL;
	pack->minor = strlen (corbapack.release) ? g_strdup (corbapack.release) : NULL;
	pack->archtype = strlen (corbapack.archtype) ? g_strdup (corbapack.archtype) : NULL;
	pack->filename = strlen (corbapack.filename) ? g_strdup (corbapack.filename) : NULL;
	pack->summary = strlen (corbapack.summary) ? g_strdup (corbapack.summary) : NULL;
	pack->description = strlen (corbapack.description) ? g_strdup (corbapack.description) : NULL;
	pack->toplevel = corbapack.toplevel;
	pack->bytesize = corbapack.bytesize;

	pack->install_root = strlen (corbapack.install_root) ? g_strdup (corbapack.install_root) : NULL;
	pack->md5 = strlen (corbapack.md5) ? g_strdup (corbapack.md5) : NULL;

	pack->distribution.name = trilobite_get_distribution_enum (corbapack.distribution.name, FALSE);
	pack->distribution.version_major = corbapack.distribution.major;
	pack->distribution.version_minor = corbapack.distribution.minor;

	if (corbapack.provides._length > 0) {
		pack->provides = corba_string_sequence_to_glist (corbapack.provides);
	} else {
		pack->provides = NULL;
	}
	
	switch (corbapack.status) {
	case GNOME_Trilobite_Eazel_UNKNOWN_STATUS:
		pack->status = PACKAGE_UNKNOWN_STATUS;
		break;
	case GNOME_Trilobite_Eazel_SOURCE_NOT_SUPPORTED:
		pack->status = PACKAGE_SOURCE_NOT_SUPPORTED;
		break;
	case GNOME_Trilobite_Eazel_DEPENDENCY_FAIL:
		pack->status = PACKAGE_DEPENDENCY_FAIL;
		break;
	case GNOME_Trilobite_Eazel_FILE_CONFLICT:
		pack->status = PACKAGE_FILE_CONFLICT;
		break;
	case GNOME_Trilobite_Eazel_BREAKS_DEPENDENCY:
		pack->status = PACKAGE_BREAKS_DEPENDENCY;
		break;
	case GNOME_Trilobite_Eazel_INVALID:
		pack->status = PACKAGE_INVALID;
		break;
	case GNOME_Trilobite_Eazel_CANNOT_OPEN:
		pack->status = PACKAGE_CANNOT_OPEN;
		break;
	case GNOME_Trilobite_Eazel_PARTLY_RESOLVED:
		pack->status = PACKAGE_PARTLY_RESOLVED;
		break;
	case GNOME_Trilobite_Eazel_ALREADY_INSTALLED:
		pack->status = PACKAGE_ALREADY_INSTALLED;
		break;
	case GNOME_Trilobite_Eazel_CIRCULAR_DEPENDENCY:
		pack->status = PACKAGE_CIRCULAR_DEPENDENCY;
		break;
	case GNOME_Trilobite_Eazel_RESOLVED:
		pack->status = PACKAGE_RESOLVED;
		break;
	}
/*
  FIXME bugzilla.eazel.com 1542:
	pack->soft_depends = packagedata_list_from_corba_packagedatastructlist (corbapack.soft_depends);
	pack->hard_depends = packagedata_list_from_corba_packagedatastructlist (corbapack.hard_depends);
	pack->breaks = packagedata_list_from_corba_packagedatastructlist (corbapack.breaks);
*/
	return pack;
}

GNOME_Trilobite_Eazel_CategoryStructList* 
corba_category_list_from_categorydata_list (GList *categories)
{
	GNOME_Trilobite_Eazel_CategoryStructList *corbacats;
	GList *iterator;
	int i;

	corbacats = GNOME_Trilobite_Eazel_CategoryStructList__alloc ();
	corbacats->_length = g_list_length (categories);
	corbacats->_buffer = CORBA_sequence_GNOME_Trilobite_Eazel_CategoryStruct_allocbuf (corbacats->_length);
	
	i = 0;
	for (iterator = categories; iterator; iterator = iterator->next) {
		CategoryData *cat;
		GNOME_Trilobite_Eazel_CategoryStruct corbacat;
		GNOME_Trilobite_Eazel_PackageDataStructList corbapacklist;

		cat = (CategoryData*)iterator->data;
		corbacat.name = cat->name ? CORBA_string_dup (cat->name) : CORBA_string_dup ("");
		corbapacklist = corba_packagedatastructlist_from_packagedata_list (cat->packages);
		corbacat.packages = corbapacklist;

		corbacats->_buffer[i] = corbacat;
		i++;
	}
	return corbacats;
}

GList*
categorydata_list_from_corba_categorystructlist (const GNOME_Trilobite_Eazel_CategoryStructList corbacategories)
{
	GList *categories;
	guint i,j;

	categories = NULL;

	for (i = 0; i < corbacategories._length; i++) {
		CategoryData *category;
		GList *packages;
		GNOME_Trilobite_Eazel_CategoryStruct corbacategory;
		GNOME_Trilobite_Eazel_PackageDataStructList packagelist;

		packages = NULL;
		corbacategory = corbacategories._buffer [i];
		packagelist = corbacategory.packages;

		for (j = 0; j < packagelist._length; j++) {
			PackageData *pack;
			GNOME_Trilobite_Eazel_PackageDataStruct corbapack;
			
			corbapack = packagelist._buffer [j];
			pack = packagedata_from_corba_packagedatastruct (corbapack);
			
			packages = g_list_prepend (packages, pack);
		}
		category = categorydata_new ();
		category->name = strlen (corbacategory.name)>0 ? g_strdup (corbacategory.name) : NULL;
		category->packages = packages;
		categories = g_list_prepend (categories, category);
	}

	return categories;
}
