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
#include "eazel-softcat.h"
#include <libtrilobite/trilobite-core-utils.h>
#include <libtrilobite/trilobite-core-distribution.h>

static GList*
corba_string_sequence_to_glist (const CORBA_sequence_CORBA_string *provides)
{
	GList *result = NULL;
	guint iterator;

	for (iterator = 0; iterator < provides->_length; iterator++) {
		result = g_list_prepend (result, g_strdup (provides->_buffer[iterator]));
	}
	return result;
}

static void
g_list_to_corba_string_sequence (CORBA_sequence_CORBA_string *sequence, GList *provides)
{
	GList *iterator;
	int i = 0;

	sequence->_length = g_list_length (provides);
	sequence->_buffer = CORBA_sequence_CORBA_string_allocbuf (sequence->_length);
	for (iterator = provides; iterator; iterator = g_list_next (iterator)) {
		sequence->_buffer[i] = CORBA_string_dup ((char*)iterator->data);
		i++;
	}
}

static void
corba_packagedatastruct_fill_from_packagedata (GNOME_Trilobite_Eazel_PackageDataStruct *corbapack,
					       const PackageData *pack)
{
	corbapack->name = pack->name ? CORBA_string_dup (pack->name) : CORBA_string_dup ("");
	corbapack->eazel_id = pack->eazel_id ? CORBA_string_dup (pack->eazel_id) : CORBA_string_dup ("");
	corbapack->suite_id = pack->suite_id ? CORBA_string_dup (pack->suite_id) : CORBA_string_dup ("");
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
	corbapack->filesize = pack->filesize;
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
	case PACKAGE_CANCELLED:
		corbapack->status = GNOME_Trilobite_Eazel_CANCELLED;
		break;
	case PACKAGE_CIRCULAR_DEPENDENCY:
		corbapack->status = GNOME_Trilobite_Eazel_CIRCULAR_DEPENDENCY;
		break;
	}

	/* depends will be filled in further up, if they're required --
	 * many times, this function is called to create a single corba package with no other package pointers */
	corbapack->depends._length = 0;
	corbapack->depends._buffer = NULL;
	corbapack->breaks._length = 0;
	corbapack->breaks._buffer = NULL;
	corbapack->modifies._length = 0;
	corbapack->modifies._buffer = NULL;
}

GNOME_Trilobite_Eazel_PackageDataStruct *
corba_packagedatastruct_from_packagedata (const PackageData *pack)
{
	GNOME_Trilobite_Eazel_PackageDataStruct *corbapack;

	corbapack = GNOME_Trilobite_Eazel_PackageDataStruct__alloc ();
	corba_packagedatastruct_fill_from_packagedata (corbapack, pack);

	return corbapack;
}

static void
corba_packagedatastructlist_fill_from_packagedata_list (GNOME_Trilobite_Eazel_PackageDataStructList *packagelist,
							GList *packages)
{
	guint i;

	packagelist->_length = g_list_length (packages);
	packagelist->_buffer = CORBA_sequence_GNOME_Trilobite_Eazel_PackageDataStruct_allocbuf (packagelist->_length);
	for (i = 0; i < packagelist->_length; i++) {
		PackageData *pack;
		pack = PACKAGEDATA (g_list_nth (packages, i)->data);
		corba_packagedatastruct_fill_from_packagedata (&(packagelist->_buffer[i]), pack);
	}
}

GNOME_Trilobite_Eazel_PackageDataStructList *
corba_packagedatastructlist_from_packagedata_list (GList *packages)
{
	GNOME_Trilobite_Eazel_PackageDataStructList *packagelist;

	packagelist = GNOME_Trilobite_Eazel_PackageDataStructList__alloc ();
	corba_packagedatastructlist_fill_from_packagedata_list (packagelist, packages);
	return packagelist;
}

static char *
new_fake_md5 (void)
{
	static unsigned long counter = 23;

	return g_strdup_printf ("FAKE-MD5-#%lu", counter++);
}

/* burrow through a package tree and stick them all into an MD5 hashtable */
static void
traverse_packagetree_md5 (const PackageData *pack, GHashTable *md5_table)
{
	PackageDependency *dep;
	PackageData *subpack;
	PackageBreaks *pbreak;
	GList *iter;

	if (pack->md5 == NULL) {
		PACKAGEDATA (pack)->md5 = new_fake_md5 ();
	}

	if (g_hash_table_lookup (md5_table, pack->md5) != NULL) {
		/* already touched this package */
		return;
	}

	g_hash_table_insert (md5_table, pack->md5, (void *)pack);
	for (iter = g_list_first (pack->depends); iter != NULL; iter = g_list_next (iter)) {
		dep = (PackageDependency *)(iter->data);
		g_assert (dep);
		traverse_packagetree_md5 (dep->package, md5_table);
	}
	for (iter = g_list_first (pack->breaks); iter != NULL; iter = g_list_next (iter)) {
		pbreak = PACKAGEBREAKS (iter->data);
		traverse_packagetree_md5 (packagebreaks_get_package (pbreak), md5_table);
	}
	for (iter = g_list_first (pack->modifies); iter != NULL; iter = g_list_next (iter)) {
		subpack = PACKAGEDATA (iter->data);
		traverse_packagetree_md5 (subpack, md5_table);
	}
}

/* given a filled-in corba package, fill in the deps/breaks/modifies fields.
 * we replace the pointers with MD5 strings, which we previously guaranteed were present (and ought to be unique).
 */
static void
corba_packagedatastruct_fill_deps (GNOME_Trilobite_Eazel_PackageDataStruct *corbapack,
				   const PackageData *pack,
				   GHashTable *md5_table)
{
	GNOME_Trilobite_Eazel_PackageDependencyStruct *corbadep;
	PackageDependency *dep;
	PackageBreaks *pbreak;
	PackageFileConflict *pbreakfile;
	PackageFeatureMissing *pbreakfeature;
	PackageData *subpack;
	GList *iter;
	char *sense_str;
	int i;

	if (pack->depends != NULL) {
		corbapack->depends._length = g_list_length (pack->depends);
		corbapack->depends._buffer = CORBA_sequence_GNOME_Trilobite_Eazel_PackageDependencyStruct_allocbuf (corbapack->depends._length);

		for (iter = g_list_first (pack->depends), i = 0;
		     iter != NULL;
		     iter = g_list_next (iter), i++) {
			dep = (PackageDependency *)(iter->data);

			/* set up a PackageDependencyStruct for it */
			g_assert (dep);
			corbadep = &(corbapack->depends._buffer[i]);
			sense_str = eazel_softcat_sense_flags_to_string (dep->sense);
			corbadep->sense = CORBA_string_dup (sense_str);
			corbadep->version = CORBA_string_dup ((dep->version != NULL) ? dep->version : "");
			corbadep->package_md5 = CORBA_string_dup (dep->package->md5);
			g_free (sense_str);
		}
		g_assert (i!=0);
	}

	if (pack->breaks != NULL) {
		corbapack->breaks._length = g_list_length (pack->breaks);
		corbapack->breaks._buffer = CORBA_sequence_GNOME_Trilobite_Eazel_PackageBreaksStruct_allocbuf (corbapack->breaks._length);
		for (iter = g_list_first (pack->breaks), i = 0;
		     iter != NULL;
		     iter = g_list_next (iter), i++) {
			pbreak = PACKAGEBREAKS (iter->data);
			g_assert (IS_VALID_PACKAGEBREAKS (pbreak));
			corbapack->breaks._buffer[i].package_md5 = CORBA_string_dup (packagebreaks_get_package (pbreak)->md5);
			/* dealing with unions in corba is a pure delight. */
			if (IS_PACKAGEFILECONFLICT (pbreak)) {
				pbreakfile = PACKAGEFILECONFLICT (pbreak);
				corbapack->breaks._buffer[i].u._d = GNOME_Trilobite_Eazel_PACKAGE_FILE_CONFLICT;
				g_list_to_corba_string_sequence (&(corbapack->breaks._buffer[i].u._u.files), pbreakfile->files);
			} else if (IS_PACKAGEFEATUREMISSING (pbreak)) {
				pbreakfeature = PACKAGEFEATUREMISSING (pbreak);
				corbapack->breaks._buffer[i].u._d = GNOME_Trilobite_Eazel_PACKAGE_FEATURE_MISSING;
				g_list_to_corba_string_sequence (&(corbapack->breaks._buffer[i].u._u.features), pbreakfeature->features);
			} else {
				g_assert_not_reached ();
			}
		}
	}

	if (pack->modifies != NULL) {
		corbapack->modifies._length = g_list_length (pack->modifies);
		corbapack->modifies._buffer = CORBA_sequence_CORBA_string_allocbuf (corbapack->modifies._length);
		for (iter = g_list_first (pack->modifies), i = 0;
		     iter != NULL;
		     iter = g_list_next (iter), i++) {
			subpack = PACKAGEDATA (iter->data);
			corbapack->modifies._buffer[i] = CORBA_string_dup (subpack->md5);
		}
	}
}

static void
corba_packagedatastructlist_foreach (const char *key, const PackageData *package, GList **list)
{
	*list = g_list_prepend (*list, (void *)package);
}

/* flatten a package tree (really a directed graph) into a list of packages,
 * changing all the depends/breaks/modifies pointers into lists of MD5's
 */
GNOME_Trilobite_Eazel_PackageDataStructList *
corba_packagedatastructlist_from_packagedata_tree (GList *packlist)
{
	GNOME_Trilobite_Eazel_PackageDataStructList *corbalist;
	GHashTable *md5_table;		/* GHashTable<char* md5, PackageData *> */
	GList *list, *iter;
	PackageData *package;
	int i;

	md5_table = g_hash_table_new (g_str_hash, g_str_equal);
	for (iter = g_list_first (packlist); iter != NULL; iter = g_list_next (iter)) {
		package = PACKAGEDATA (iter->data);
		traverse_packagetree_md5 (package, md5_table);
	}

	/* convert hashtable of MD5=>PackageData into GList<PackageData *> */
	list = NULL;
	g_hash_table_foreach (md5_table, (GHFunc)corba_packagedatastructlist_foreach, &list);

	/* build up actual corba list from the flattened tree */
	corbalist = GNOME_Trilobite_Eazel_PackageDataStructList__alloc ();
	corbalist->_length = g_list_length (list);
	corbalist->_buffer = CORBA_sequence_GNOME_Trilobite_Eazel_PackageDataStruct_allocbuf (corbalist->_length);
	for (iter = g_list_first (list), i = 0; iter != NULL; iter = g_list_next (iter), i++) {
		package = PACKAGEDATA (iter->data);
		corba_packagedatastruct_fill_from_packagedata (&(corbalist->_buffer[i]), package);
		corba_packagedatastruct_fill_deps (&(corbalist->_buffer[i]), package, md5_table);
	}

	g_hash_table_destroy (md5_table);
	g_list_free (list);

	return corbalist;
}

PackageData*
packagedata_from_corba_packagedatastruct (const GNOME_Trilobite_Eazel_PackageDataStruct *corbapack)
{
	PackageData *pack;
	
	pack = packagedata_new();
	pack->name = strlen (corbapack->name) ? g_strdup (corbapack->name) : NULL;
	pack->eazel_id = strlen (corbapack->eazel_id) ? g_strdup (corbapack->eazel_id) : NULL;
	pack->suite_id = strlen (corbapack->suite_id) ? g_strdup (corbapack->suite_id) : NULL;
	pack->version = strlen (corbapack->version) ? g_strdup (corbapack->version) : NULL;
	pack->minor = strlen (corbapack->release) ? g_strdup (corbapack->release) : NULL;
	pack->archtype = strlen (corbapack->archtype) ? g_strdup (corbapack->archtype) : NULL;
	pack->filename = strlen (corbapack->filename) ? g_strdup (corbapack->filename) : NULL;
	pack->summary = strlen (corbapack->summary) ? g_strdup (corbapack->summary) : NULL;
	pack->description = strlen (corbapack->description) ? g_strdup (corbapack->description) : NULL;
	pack->toplevel = corbapack->toplevel;
	pack->bytesize = corbapack->bytesize;
	pack->filesize = corbapack->filesize;

	pack->install_root = strlen (corbapack->install_root) ? g_strdup (corbapack->install_root) : NULL;
	pack->md5 = strlen (corbapack->md5) ? g_strdup (corbapack->md5) : NULL;

	pack->distribution.name = trilobite_get_distribution_enum (corbapack->distribution.name, FALSE);
	pack->distribution.version_major = corbapack->distribution.major;
	pack->distribution.version_minor = corbapack->distribution.minor;

	switch (corbapack->status) {
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
	case GNOME_Trilobite_Eazel_CANCELLED:
		pack->status = PACKAGE_CANCELLED;
		break;
	case GNOME_Trilobite_Eazel_CIRCULAR_DEPENDENCY:
		pack->status = PACKAGE_CIRCULAR_DEPENDENCY;
		break;
	case GNOME_Trilobite_Eazel_RESOLVED:
		pack->status = PACKAGE_RESOLVED;
		break;
	}

	return pack;
}

GList*
packagedata_list_from_corba_packagedatastructlist (const GNOME_Trilobite_Eazel_PackageDataStructList *corbapack)
{
	PackageData *pack;
	GList *result;
	guint i;

	result = NULL;
	
	for (i = 0; i < corbapack->_length; i++) {
		pack = packagedata_from_corba_packagedatastruct (&(corbapack->_buffer[i]));
		result = g_list_prepend (result, pack);
	}
	result = g_list_reverse (result);

	return result;
}

static gboolean
empty_hash_table (char *key,
		  PackageData *pack,
		  gpointer unused)
{
	gtk_object_unref (GTK_OBJECT (pack));
	return TRUE;
}

/* inflate a corba package list into a full-blown package tree (really a
 * directed graph), by converting the soft MD5 pointers into physical ones.
 */
GList *
packagedata_tree_from_corba_packagedatastructlist (const GNOME_Trilobite_Eazel_PackageDataStructList *corbalist)
{
	GList *packlist, *outlist, *iter;
	PackageData *pack, *subpack;
	PackageDependency *dep;
	PackageFileConflict *pbreakfile;
	PackageFeatureMissing *pbreakfeature;
	GNOME_Trilobite_Eazel_PackageDataStruct *corbapack;
	GNOME_Trilobite_Eazel_PackageDependencyStruct *corbadep;
	GHashTable *md5_table;
	guint i, j;

	packlist = packagedata_list_from_corba_packagedatastructlist (corbalist);
	md5_table = g_hash_table_new (g_str_hash, g_str_equal);

	/* mark them all toplevel, first, and populate the MD5 hashtable */
	for (iter = g_list_first (packlist); iter != NULL; iter = g_list_next (iter)) {
		pack = PACKAGEDATA (iter->data);
		pack->toplevel = TRUE;
		if (pack->md5 == NULL) {
			pack->md5 = new_fake_md5 ();
		}
		g_hash_table_insert (md5_table, pack->md5, pack);
	}

	/* now, resolve MD5 "soft" references between packages */
	for (i = 0; i < corbalist->_length; i++) {
		pack = PACKAGEDATA (g_hash_table_lookup (md5_table, corbalist->_buffer[i].md5));
		g_assert (pack != NULL);
		corbapack = &(corbalist->_buffer[i]);

		for (j = 0; j < corbapack->depends._length; j++) {
			corbadep = &(corbapack->depends._buffer[j]);
			dep = g_new0 (PackageDependency, 1);
			dep->sense = eazel_softcat_string_to_sense_flags (corbadep->sense);
			dep->version = g_strdup (corbadep->version);
			dep->package = PACKAGEDATA (g_hash_table_lookup (md5_table, corbadep->package_md5));
			gtk_object_ref (GTK_OBJECT (dep->package));
			if (dep->package == NULL) {
				g_warning ("corba unpack: can't follow md5 soft pointer '%s'", corbadep->package_md5);
				g_free (dep);
			} else {
				dep->package->toplevel = FALSE;
				pack->depends = g_list_prepend (pack->depends, dep);
			}
		}

		pack->depends = g_list_reverse (pack->depends);

		for (j = 0; j < corbapack->breaks._length; j++) {
			subpack = g_hash_table_lookup (md5_table, corbapack->breaks._buffer[j].package_md5);
			if (subpack == NULL) {
				g_warning ("corba unpack: can't follow md5 soft pointer '%s'", corbapack->breaks._buffer[j].package_md5);
			} else {
				subpack->toplevel = FALSE;
				switch (corbapack->breaks._buffer[j].u._d) {
				case GNOME_Trilobite_Eazel_PACKAGE_FILE_CONFLICT:
					pbreakfile = packagefileconflict_new ();
					packagebreaks_set_package (PACKAGEBREAKS (pbreakfile), subpack);
					pbreakfile->files = corba_string_sequence_to_glist (&(corbapack->breaks._buffer[j].u._u.files));
					packagedata_add_to_breaks (pack, PACKAGEBREAKS (pbreakfile));
					gtk_object_unref (GTK_OBJECT (pbreakfile));
					break;
				case GNOME_Trilobite_Eazel_PACKAGE_FEATURE_MISSING:
					pbreakfeature = packagefeaturemissing_new ();
					packagebreaks_set_package (PACKAGEBREAKS (pbreakfeature), subpack);
					pbreakfeature->features = corba_string_sequence_to_glist (&(corbapack->breaks._buffer[j].u._u.features));
					packagedata_add_to_breaks (pack, PACKAGEBREAKS (pbreakfeature));
					gtk_object_unref (GTK_OBJECT (pbreakfeature));
					break;
				default:
					g_assert_not_reached ();
				}
			}
		}
		pack->breaks = g_list_reverse (pack->breaks);

		for (j = 0; j < corbapack->modifies._length; j++) {
			subpack = g_hash_table_lookup (md5_table, corbapack->modifies._buffer[j]);
			if (subpack == NULL) {
				g_warning ("corba unpack: can't follow md5 soft pointer '%s'", corbapack->modifies._buffer[j]);
			} else {
				subpack->toplevel = FALSE;
				pack->modifies = g_list_prepend (pack->modifies, subpack);
				gtk_object_ref (GTK_OBJECT (subpack));
			}
		}
		pack->modifies = g_list_reverse (pack->modifies);
	}

	/* now make a list of JUST the toplevel packages */
	outlist = NULL;
	for (iter = g_list_first (packlist); iter != NULL; iter = g_list_next (iter)) {
		pack = PACKAGEDATA (iter->data);
		if (pack->toplevel) {
			gtk_object_ref (GTK_OBJECT (pack));
			outlist = g_list_prepend (outlist, pack);
		}
	}
	g_list_free (packlist);

	g_hash_table_foreach_remove (md5_table, (GHRFunc)empty_hash_table, NULL);
	g_hash_table_destroy (md5_table);

	return outlist;
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

		cat = (CategoryData *)iterator->data;
		corbacats->_buffer[i].name = CORBA_string_dup ((cat->name != NULL) ? cat->name : "");
		corba_packagedatastructlist_fill_from_packagedata_list (&(corbacats->_buffer[i].packages), cat->packages);
		i++;
	}
	return corbacats;
}

GList*
categorydata_list_from_corba_categorystructlist (const GNOME_Trilobite_Eazel_CategoryStructList *corbacategories)
{
	GList *categories;
	guint i;

	categories = NULL;

	for (i = 0; i < corbacategories->_length; i++) {
		CategoryData *category;
		GList *packages;
		GNOME_Trilobite_Eazel_CategoryStruct *corbacategory;
		GNOME_Trilobite_Eazel_PackageDataStructList *packagelist;

		packages = NULL;
		corbacategory = &(corbacategories->_buffer[i]);
		packagelist = &(corbacategory->packages);

		category = categorydata_new ();
		category->name = (strlen (corbacategory->name) > 0) ? g_strdup (corbacategory->name) : NULL;
		category->packages = packagedata_list_from_corba_packagedatastructlist (packagelist);
		categories = g_list_prepend (categories, category);
	}

	return categories;
}
