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
#include "eazel-package-system-types.h"
#include "eazel-softcat.h"		/* for softcat sense flags */

#include <rpm/rpmlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <libtrilobite/trilobite-core-utils.h>

#define DEBUG_PACKAGE_ALLOCS 

#ifdef DEBUG_PACKAGE_ALLOCS
static int report_all = 0;
static int package_allocs = 0;
static int category_allocs = 0;
static gboolean at_exit_registered = FALSE;
GList *packages_allocated = NULL;

static void 
at_exit_package_data_info (void) 
{
	GList *iterator;
	if (packages_allocated == NULL) {
		trilobite_debug ("All packagedata structures deallocated");
	} else {
		trilobite_debug ("Fordømt! Some packagedata structures were not deallocated");
		for (iterator = packages_allocated; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack = PACKAGEDATA (iterator->data);
			trilobite_debug ("package %p (%s) not deallocated", pack, pack->name);
		}
	}
}
#endif /* DEBUG_PACKAGE_ALLOCS */

const char *
eazel_install_protocol_as_string (URLType protocol) 
{
	switch (protocol) {
	case PROTOCOL_HTTP:
		return "http";
		break;
	case PROTOCOL_FTP:
		return "ftp";
		break;
	case PROTOCOL_LOCAL:
		return "file";
		break;
	}
	return "???";
}

CategoryData*
categorydata_new (void)
{
	CategoryData *result;

	result = g_new0 (CategoryData, 1);
#ifdef DEBUG_PACKAGE_ALLOCS
	category_allocs ++;
	if (report_all) trilobite_debug ("category_allocs inced to %d (0x%p)", category_allocs, result);
#endif /* DEBUG_PACKAGE_ALLOCS */
	result->name = NULL;
	result->description = NULL;
	result->packages = NULL;
	result->depends = NULL;
	result->exclusive = FALSE;
	result->default_choice = FALSE;
	return result;
}

GList*
categorydata_list_copy (const GList *list)
{
	GList *result = NULL;
	const GList *ptr;

	for (ptr = list; ptr; ptr = g_list_next (ptr)) {
		result = g_list_prepend (result, categorydata_copy ((CategoryData*)(ptr->data)));
	}
	result = g_list_reverse (result); 

	return result;
}

CategoryData*
categorydata_copy (const CategoryData *cat)
{
	CategoryData *result;
	GList *ptr;

	result = categorydata_new ();

	result->name = g_strdup (cat->name);
	result->description = g_strdup (cat->description);
	result->packages = packagedata_list_copy (cat->packages, TRUE);

	for (ptr = cat->depends; ptr; ptr = g_list_next (ptr)) {
		result->depends = g_list_prepend (result->depends, 
						g_strdup ((char*)ptr->data));
	}
	result->depends = g_list_reverse (result->depends); 

	return result;
}

void
categorydata_destroy_foreach (CategoryData *cd, gpointer ununsed)
{
#ifdef DEBUG_PACKAGE_ALLOCS
	category_allocs --;
	if (report_all) trilobite_debug ("category_allocs = %d (0x%p) %s", category_allocs, cd, cd ? cd->name: "?");
#endif /* DEBUG_PACKAGE_ALLOCS */

	g_return_if_fail (cd != NULL);
	if (g_list_length (cd->packages)) {
		g_list_foreach (cd->packages, (GFunc)gtk_object_unref, NULL);
	}
	g_list_free (cd->packages);
	cd->packages = NULL;
	if (g_list_length (cd->depends)) {
		g_list_foreach (cd->depends, (GFunc)g_free, NULL);
	}
	g_list_free (cd->depends);
	cd->depends = NULL;
	g_free (cd->name);
	cd->name = NULL;
	g_free (cd->description);
	cd->description = NULL;
}

void
categorydata_destroy (CategoryData *cd)
{
	categorydata_destroy_foreach (cd, NULL);
}

void
categorydata_list_destroy (GList *list)
{
	if (g_list_length (list)) {
		g_list_foreach (list, (GFunc) categorydata_destroy_foreach, NULL);
	}
	g_list_free (list);
}

GList* 
categorylist_flatten_to_packagelist (GList *categories)
{
	GList* packages = NULL;
	GList* category_iterator;
	
	for (category_iterator = categories; category_iterator; category_iterator = g_list_next (category_iterator)) {
		CategoryData *cat = (CategoryData*)category_iterator->data;
		if (packages) {
			packages = g_list_concat (packages, g_list_copy (cat->packages));
		} else {
			packages = g_list_copy (cat->packages);
		}
	}


	return packages;
}

/*************************************************************************************************/

PackageDependency *
packagedependency_new (void)
{
	PackageDependency *dep;

	dep = g_new0 (PackageDependency, 1);
	dep->package = NULL;
	dep->version = NULL;
	return dep;
}

PackageDependency *
packagedependency_copy (const PackageDependency *dep, gboolean deep)
{
	PackageDependency *newdep;

	newdep = g_new0 (PackageDependency, 1);
	newdep->sense = dep->sense;
	newdep->version = g_strdup (dep->version);
	if (dep->package != NULL) {
		newdep->package = packagedata_copy (dep->package, deep);
	}
	return newdep;
}

void
packagedependency_destroy (PackageDependency *dep)
{
	if (dep->package) {
		gtk_object_unref (GTK_OBJECT (dep->package));
	}
	dep->package = NULL;
	g_free (dep->version);
	dep->version = NULL;
	dep->sense = 0;
	g_free (dep);
}

/**********************************************************************************
  GTK+ crap for PackageData objects 
 **********************************************************************************/

static void
packagedata_finalize (GtkObject *obj) 
{
	PackageData *pack = PACKAGEDATA (obj);
	
#ifdef DEBUG_PACKAGE_ALLOCS
	package_allocs --;
	if (report_all) {
		if (pack) {
			if (pack->name) {
				trilobite_debug ("package_allocs = %d (0x%p) %s", 
						 package_allocs, pack,pack->name);
			} else if (pack->provides) {
				trilobite_debug ("package_allocs = %d (0x%p) providing %s", 
						 package_allocs, pack,
						 (char*)pack->provides->data);
			} else {
				trilobite_debug ("package_allocs = %d (0x%p) ?", package_allocs, pack);
			}
		} else {
			trilobite_debug ("package_allocs = %d (0x%p) ??", package_allocs, pack);
		}
	}
	packages_allocated = g_list_remove (packages_allocated, pack);
#endif /* DEBUG_PACKAGE_ALLOCS */
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
	g_free (pack->description);
	pack->description = NULL;
	pack->bytesize = 0;
	pack->filesize = 0;
	g_free (pack->filename);
	pack->filename = NULL;
	g_free (pack->eazel_id);
	pack->eazel_id = NULL;
	g_free (pack->suite_id);
	pack->suite_id = NULL;
	g_free (pack->remote_url);
	pack->remote_url = NULL;
	g_free (pack->install_root);
	pack->install_root = NULL;
	g_free (pack->md5);
	pack->md5 = NULL;
	g_list_foreach (pack->provides, (GFunc)g_free, NULL); 
	g_list_free (pack->provides);
	pack->provides = NULL;
	g_list_foreach (pack->features, (GFunc)g_free, NULL);
	g_list_free (pack->features);
	pack->features = NULL;

	g_list_foreach (pack->depends, (GFunc)packagedependency_destroy, GINT_TO_POINTER (FALSE));
	g_list_free (pack->depends);
	pack->depends = NULL;

	g_list_foreach (pack->soft_depends, (GFunc)gtk_object_unref, NULL);
	g_list_free (pack->soft_depends);
	pack->soft_depends = NULL;

	g_list_foreach (pack->breaks, (GFunc)gtk_object_unref, NULL);
	g_list_free (pack->breaks);
	pack->breaks = NULL;

	g_list_foreach (pack->modifies, (GFunc)gtk_object_unref, NULL);
	g_list_free (pack->modifies);
	pack->modifies = NULL;

	if (pack->packsys_struc) {
		/* FIXME: bugzilla.eazel.com 6007
		 */
#ifdef HAVE_RPM_30
		headerFree ((Header) pack->packsys_struc);
#endif /* HAVE_RPM_30 */
		pack->packsys_struc = NULL;
	}
}

static void
packagedata_class_initialize (PackageDataClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->finalize = packagedata_finalize;
}

static void
packagedata_initialize (PackageData *package) {
	g_assert (package!=NULL); 
	g_assert (IS_PACKAGEDATA (package));


#ifdef DEBUG_PACKAGE_ALLOCS
	package_allocs ++;
	if (report_all) trilobite_debug ("package_allocs inced to %d (0x%p)", package_allocs, package);
	if (!at_exit_registered) {
		atexit (&at_exit_package_data_info);
		at_exit_registered = TRUE;
	}
	packages_allocated = g_list_prepend (packages_allocated, package);
#endif /* DEBUG_PACKAGE_ALLOCS */

	package->name = NULL;
	package->version = NULL;
	package->minor = NULL;
	package->archtype = NULL;
	package->source_package = FALSE;
	package->summary = NULL;
	package->description = NULL;
	package->bytesize = 0;
	package->filesize = 0;
	package->distribution = trilobite_get_distribution ();
	package->filename = NULL;
	package->eazel_id = NULL;
	package->suite_id = NULL;
	package->remote_url = NULL;
	package->conflicts_checked = FALSE;
	package->install_root = NULL;
	package->provides = NULL;
	package->soft_depends = NULL;
	package->breaks = NULL;
	package->modifies = NULL;
	package->depends = NULL;
	package->status = PACKAGE_UNKNOWN_STATUS;
	package->modify_status = PACKAGE_MOD_UNTOUCHED;
	package->md5 = NULL;
	package->packsys_struc = NULL;
	package->features = NULL;
	package->fillflag = PACKAGE_FILL_INVALID;
}

GtkType 
packagedata_get_type (void)
{
	static GtkType object_type = 0;

	/* First time it's called ? */
	if (!object_type)
	{
		static const GtkTypeInfo object_info =
		{
			"PackageData",
			sizeof (PackageData),
			sizeof (PackageDataClass),
			(GtkClassInitFunc) packagedata_class_initialize,
			(GtkObjectInitFunc) packagedata_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		object_type = gtk_type_unique (gtk_object_get_type (), &object_info);
	}

	return object_type;
}

PackageData*
packagedata_new ()
{
	PackageData *package;

	package = PACKAGEDATA (gtk_object_new (TYPE_PACKAGEDATA, NULL));
	gtk_object_ref (GTK_OBJECT (package));
	gtk_object_sink (GTK_OBJECT (package));

	return package;
}

/**********************************************************************************/

GList *
packagedata_list_copy (const GList *list, gboolean deep)
{
	const GList *ptr;
	GList *result = NULL;

	for (ptr = list; ptr; ptr = g_list_next (ptr)) {
		result = g_list_prepend (result, 
					 packagedata_copy ((PackageData*)(ptr->data), deep));
	}
	result = g_list_reverse (result);

	return result;
}

static GList *
packagedata_deplist_copy (const GList *list, gboolean deep)
{
	const GList *ptr;
	GList *result = NULL;

	for (ptr = list; ptr; ptr = g_list_next (ptr)) {
		result = g_list_prepend (result, packagedependency_copy ((PackageDependency *)(ptr->data), deep));
	}
	result = g_list_reverse (result);

	return result;
}

PackageData* 
packagedata_copy (const PackageData *pack, gboolean deep)
{
	PackageData *result;
	const GList *ptr;

	g_assert (pack);

	result = packagedata_new ();

	result->name = g_strdup (pack->name);
	result->version = g_strdup (pack->version);
	result->minor = g_strdup (pack->minor);
	result->archtype = g_strdup (pack->archtype);
	result->summary = g_strdup (pack->summary);
	result->description = g_strdup (pack->description);
	result->filename = g_strdup (pack->filename);
	result->remote_url = g_strdup (pack->remote_url);
	result->install_root = g_strdup (pack->install_root);
	result->eazel_id = g_strdup (pack->eazel_id);
	result->suite_id = g_strdup (pack->suite_id);
	result->md5 = g_strdup (pack->md5);

	result->toplevel = pack->toplevel;
	result->status = pack->status;
	result->modify_status = pack->modify_status;
	result->source_package = pack->source_package;
	result->conflicts_checked = pack->conflicts_checked;

	result->distribution = pack->distribution;
	result->bytesize = pack->bytesize;
	result->filesize = pack->filesize;
	result->md5 = g_strdup (pack->md5);

	if (deep) {
		result->soft_depends = packagedata_list_copy (pack->soft_depends, TRUE);
		result->depends = packagedata_deplist_copy (pack->depends, TRUE);
		result->modifies = packagedata_list_copy (pack->modifies, TRUE);

		/* Sloppy, just ref and copy the pointer rather then copying the
		   object */
		g_list_foreach (pack->breaks, (GFunc)gtk_object_ref, NULL);
		result->breaks = g_list_copy (pack->breaks);

		for (ptr = pack->provides; ptr; ptr = g_list_next (ptr)) {
			result->provides = g_list_prepend (result->provides, g_strdup (ptr->data));
		}
		result->provides = g_list_reverse (result->provides);
		for (ptr = pack->features; ptr; ptr = g_list_next (ptr)) {
			result->features = g_list_prepend (result->features, g_strdup (ptr->data));
		}
		result->features = g_list_reverse (result->features);
	} /* No need to null if !deep, as packagedata_new does that */

	return result;
}

#define COPY_STRING(field) \
	if (full_package->field != NULL) { \
		g_free (package->field); \
		package->field = g_strdup (full_package->field); \
	}


/* fill in a package struct with info from another one:
 * flags tells what fields to skip.
 */
void
packagedata_fill_in_missing (PackageData *package, const PackageData *full_package, int fill_flags)
{
	const GList *ptr;

	g_assert (package != NULL);
	g_assert (full_package != NULL);

	COPY_STRING (name);
	COPY_STRING (version);
	COPY_STRING (minor);
	COPY_STRING (archtype);
	package->bytesize = full_package->bytesize;
	package->filesize = full_package->filesize;
	package->distribution = full_package->distribution;
	COPY_STRING (filename);
	COPY_STRING (eazel_id);
	COPY_STRING (suite_id);
	COPY_STRING (remote_url);
	COPY_STRING (md5);

	if (! (fill_flags & PACKAGE_FILL_NO_TEXT)) {
		COPY_STRING (summary);
		COPY_STRING (description);
	}
	if (! (fill_flags & PACKAGE_FILL_NO_PROVIDES)) {
		if (package->provides != NULL) {
			g_list_foreach (package->provides, (GFunc)g_free, NULL); 
			g_list_free (package->provides);
		}
		package->provides = NULL;
		for (ptr = full_package->provides; ptr; ptr = g_list_next (ptr)) {
			package->provides = g_list_prepend (package->provides, g_strdup (ptr->data));
		}
		package->provides = g_list_reverse (package->provides);

		if (package->features != NULL) {
			g_list_foreach (package->features, (GFunc)g_free, NULL);
			g_list_free (package->features);
		}
		package->features = NULL;
		for (ptr = full_package->features; ptr; ptr = g_list_next (ptr)) {
			package->features = g_list_prepend (package->features, g_strdup (ptr->data));
		}
		package->features = g_list_reverse (package->features);
	}
	if (! (fill_flags & PACKAGE_FILL_NO_DEPENDENCIES)) {
		g_list_foreach (package->soft_depends, (GFunc)gtk_object_unref, NULL);
		g_list_foreach (package->depends, (GFunc)packagedependency_destroy, GINT_TO_POINTER (FALSE));
		package->soft_depends = NULL;
		package->depends = NULL;

		package->soft_depends = packagedata_list_copy (full_package->soft_depends, TRUE);
		package->depends = packagedata_deplist_copy (full_package->depends, TRUE);
	}
	package->fillflag = fill_flags;
}

void 
packagedata_remove_soft_dep (PackageData *remove, 
			     PackageData *from)
{
	g_assert (remove);
	g_assert (from);

	trilobite_debug ("removing %s from %s's deps", remove->name, from->name);
	from->soft_depends = g_list_remove (from->soft_depends, remove);
	gtk_object_unref (GTK_OBJECT (remove));
}

const char*
rpmfilename_from_packagedata (const PackageData *pack)
{
	static char *filename = NULL;
	
	g_free (filename);
	if (pack->filename) {
		filename = g_strdup (pack->filename);
	} else {
		if (pack->version && pack->minor && pack->archtype) {
			filename = g_strdup_printf ("%s-%s-%s.%s.rpm",
						    pack->name,
						    pack->version,
						    pack->minor,
						    pack->archtype);
		} else if (pack->version && pack->archtype) {
			filename = g_strdup_printf ("%s-%s.%s.rpm",
						    pack->name,
						    pack->version,
						    pack->archtype);
		} else if (pack->archtype) {
			filename = g_strdup_printf ("%s.%s.rpm",
						    pack->name,
						    pack->archtype);
		} else {
			filename = g_strdup (pack->name); 
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
		name = g_strdup (pack->name); 
	}

	return name;
}

char*
packagedata_get_readable_name (const PackageData *pack)
{
	char *result = NULL;
	if (pack==NULL) {
		result = NULL;
	} else if ((pack->name != NULL) && (pack->version != NULL)) {
		/* This is a hack to shorten EazelSourceSnapshot names
		   into the build date/time */
		if (strstr (pack->version, "Eazel")!=NULL && strstr (pack->minor, ".200") != NULL) {
			char *month[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
					 "Sep", "Oct", "Nov", "Dec"};
			char *temp, *temp2;
			int mo, da, ho, mi;
			/* this crap is too long to display ! */
			temp = g_strdup (pack->minor);
			temp2 = strstr (temp, ".200");
			strcpy (temp2, "ESS");
			temp2 += strlen (".200x");
			sscanf (temp2, "%2d%2d%2d%2d", &mo, &da, &ho, &mi);
			result = g_strdup_printf ("%s of %d %s, %02d:%02d", 
						  pack->name,
						  da, month[mo-1], ho, mi);
			g_free (temp);
		} else {
			result = g_strdup_printf ("%s v%s", pack->name, pack->version);
		}
	} else if (pack->name != NULL) {
		result = g_strdup_printf ("%s", pack->name);
	} else if (pack->eazel_id != NULL) {
		result = g_strdup_printf ("Eazel Package #%s", pack->eazel_id);
	} else if (pack->suite_id != NULL) {
		result = g_strdup_printf ("Eazel Suite #%s", pack->suite_id);
	} else if (pack->provides && pack->provides->data) {
		result = g_strdup_printf ("file %s", (char*)(pack->provides->data));
	} else {
		/* what the--?!  WHO ARE YOU! */
		result = g_strdup ("another package");
	}
	
	return result;
}

char*
packagedata_get_name (const PackageData *pack)
{
	char *result = NULL;
	if (pack->name && pack->version && pack->minor) {
		result = g_strdup_printf ("%s-%s-%s", pack->name, pack->version, pack->minor);
	} else if (pack->name && pack->version) {
		result = g_strdup_printf ("%s-%s", pack->name, pack->version);
	} else if (pack->name) {
		result = g_strdup (pack->name);
	} 
	return result;
}

int 
packagedata_hash_equal (PackageData *a, 
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
	case PACKAGE_FILE_CONFLICT:
		result = g_strdup ("FILE_CONFLICT");
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
	case PACKAGE_ALREADY_INSTALLED:
		result = g_strdup ("ALREADY_INSTALLED");
		break;
	case PACKAGE_CIRCULAR_DEPENDENCY:
		result = g_strdup ("CIRCULAR_DEPENDENCY");
		break;
	default:
		g_assert_not_reached ();
	}
	return result;
}

PackageSystemStatus
packagedata_status_str_to_enum (const char *st)
{
	PackageSystemStatus result;
	
	g_return_val_if_fail (st!=NULL, PACKAGE_UNKNOWN_STATUS);
	
	if (strcmp (st, "UNKNOWN_STATUS")==0) { result = PACKAGE_UNKNOWN_STATUS; } 
	else if (strcmp (st, "SOURCE_NOT_SUPPORTED")==0) { result = PACKAGE_SOURCE_NOT_SUPPORTED; } 
	else if (strcmp (st, "DEPENDENCY_FAIL")==0) { result = PACKAGE_DEPENDENCY_FAIL; } 
	else if (strcmp (st, "FILE_CONFLICT")==0) { result = PACKAGE_FILE_CONFLICT; }
	else if (strcmp (st, "BREAKS_DEPENDENCY")==0) { result = PACKAGE_BREAKS_DEPENDENCY; } 
	else if (strcmp (st, "INVALID")==0) { result = PACKAGE_INVALID; } 
	else if (strcmp (st, "CANNOT_OPEN")==0) { result = PACKAGE_CANNOT_OPEN; } 
	else if (strcmp (st, "PARTLY_RESOLVED")==0) { result = PACKAGE_PARTLY_RESOLVED; } 
	else if (strcmp (st, "RESOLVED")==0) { result = PACKAGE_RESOLVED; } 
	else if (strcmp (st, "ALREADY_INSTALLED")==0) { result = PACKAGE_ALREADY_INSTALLED; } 
	else if (strcmp (st, "CIRCULAR_DEPENDENCY")==0) { result = PACKAGE_CIRCULAR_DEPENDENCY; } 
	else { g_assert_not_reached (); result = PACKAGE_UNKNOWN_STATUS; };

	return result;
}

const char*
packagedata_modstatus_enum_to_str (PackageSystemStatus st)
{
	static char *result=NULL;
	g_free (result);

	switch (st) {
	case PACKAGE_MOD_UPGRADED:
		result = g_strdup ("UPGRADED");
		break;
	case PACKAGE_MOD_DOWNGRADED:
		result = g_strdup ("DOWNGRADED");
		break;
	case PACKAGE_MOD_INSTALLED:
		result = g_strdup ("INSTALLED");
		break;
	case PACKAGE_MOD_UNINSTALLED:
		result = g_strdup ("UNINSTALLED");
		break;
	case PACKAGE_MOD_UNTOUCHED:
	default:
		result = g_strdup ("UNTOUCHED");
		break;
	}
	return result;
}

PackageSystemStatus
packagedata_modstatus_str_to_enum (const char *st)
{
	PackageSystemStatus result;
	
	g_return_val_if_fail (st!=NULL, PACKAGE_MOD_UNTOUCHED);

	if (strcmp (st, "INSTALLED")==0) { result = PACKAGE_MOD_INSTALLED; } 
	else if (strcmp (st, "UNTOUCHED")==0) { result = PACKAGE_MOD_UNTOUCHED; } 
	else if (strcmp (st, "UNINSTALLED")==0) { result = PACKAGE_MOD_UNINSTALLED; } 
	else if (strcmp (st, "UPGRADED")==0) { result = PACKAGE_MOD_UPGRADED; } 
	else if (strcmp (st, "DOWNGRADED")==0) { result = PACKAGE_MOD_DOWNGRADED; } 
	else { 
		result = PACKAGE_MOD_UNTOUCHED;
	}

	return result;
}

static void
packagedata_add_pack_to (GList **list, GtkObject *b) {
	gtk_object_ref (b);
	(*list) = g_list_prepend (*list, b);
}

void 
packagedata_add_to_breaks (PackageData *pack, PackageBreaks *b) 
{
	g_assert (pack);
	g_assert (b);
	packagedata_add_pack_to (&pack->breaks, GTK_OBJECT (b));
}

void 
packagedata_add_pack_to_soft_depends (PackageData *pack, PackageData *b)
{
	g_assert (pack);
	g_assert (b);
	g_assert (pack != b);
	packagedata_add_pack_to (&pack->soft_depends, GTK_OBJECT (b));
}

void 
packagedata_add_pack_to_modifies (PackageData *pack, PackageData *b)
{
	g_assert (pack);
	g_assert (b);
	g_assert (pack != b);
	packagedata_add_pack_to (&pack->modifies, GTK_OBJECT (b));
}

static void
flatten_packagedata_dependency_tree_helper (GList *packagedeps, 
					    GList **result)
{
	GList *iterator;

	for (iterator = packagedeps; iterator; iterator = g_list_next (iterator)) {
		PackageDependency *dep = PACKAGEDEPENDENCY (iterator->data);
		PackageData *pack = dep->package;
		if (g_list_find (*result, pack)==NULL) {
			(*result) = g_list_prepend (*result, pack);
			flatten_packagedata_dependency_tree_helper (pack->depends, result);
		}
	}
}

GList*
flatten_packagedata_dependency_tree (GList *packages)
{
	GList *result = NULL;
	GList *iterator;

	/* I first add the toplevel, since I can then get away with only checking
	   for dupes in the helper */
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = PACKAGEDATA (iterator->data);
		result = g_list_prepend (result, pack);
	}
	for (iterator = packages; iterator; iterator = g_list_next (iterator)) {
		PackageData *pack = PACKAGEDATA (iterator->data);
		flatten_packagedata_dependency_tree_helper (pack->depends, &result);
	}

	return result;
}

/*
  O(mn) complexity. 
 */
void 
packagedata_list_prune (GList **input, 
			GList *remove_list, 
			gboolean destroy, 
			gboolean deep)
{
	GList *in_it=NULL, *rm_it=NULL;
	
	for (rm_it = remove_list; rm_it; rm_it = g_list_next (rm_it)) {
		PackageData *rm = (PackageData*)rm_it->data;
		PackageData *in = NULL;

		for (in_it = *input; in_it; in_it = g_list_next (in_it)) {
			in = (PackageData*)in_it->data;
			if (eazel_install_package_name_compare (in, rm->name)==0) {
				break;
			}
		}
		if (in_it && in) {
			(*input) = g_list_remove (*input, in);
			if (destroy) {
				gtk_object_unref (GTK_OBJECT (in));
			}
		}
	}
}

PackageRequirement* 
packagerequirement_new (PackageData *pack, 
			PackageData *req)
{
	PackageRequirement *result;
	result = g_new0 (PackageRequirement, 1);
	result->package = pack;
	result->required = req;
	return result;
}

/* The funky compare functions */


int
eazel_install_package_provides_basename_compare (char *a,
						 char *b)
{
	return strcmp (g_basename (a), b);
}

int
eazel_install_package_provides_compare (PackageData *pack,
					char *name)
{
	GList *ptr = NULL;
	ptr = g_list_find_custom (pack->provides, 
				  (gpointer)name, 
				  (GCompareFunc)eazel_install_package_provides_basename_compare);
	if (ptr) {
		trilobite_debug ("package %s supplies %s", pack->name, name);
		return 0;
	} 
	return -1;
}

int
eazel_install_package_name_compare (PackageData *pack,
				    char *name)
{
	return strcmp (pack->name, name);
}

/* This does a slow and painful comparison of all the major fields */
int 
eazel_install_package_compare (PackageData *pack, 
			       PackageData *other)
{
	int result = 0;
	/* For the field sets, if they both exists, compare them,
	   if one has it and the other doesn't, not equal */
	if (pack->name && other->name) {
		int tmp_result = strcmp (pack->name, other->name);
		if (tmp_result) {
			result = tmp_result;
		}
	} else if (pack->name || other->name) {
		result = 1;
	}
	if (pack->version && other->version) {
		int tmp_result = strcmp (pack->version, other->version);
		if (tmp_result) {
			result = tmp_result;
		}
	} else if (pack->version || other->version) {
		result = 1;
	}
	if (pack->minor && other->minor) {
		int tmp_result = strcmp (pack->minor, other->minor);
		if (tmp_result) {
			result = tmp_result;
		}
	} else if (pack->minor || other->minor) {
		result = 1;
	}
	
	return result;
}

/* Compare function used while creating the PackageRequirements in 
   eazel_install_do_dependency_check.
   It checks for equality on the package names, if one doens't have a name,
   it checks for the same 1st element in ->provides, if one doens't have 
   a provides list, they're not the same */
int 
eazel_install_requirement_dep_compare (PackageRequirement *req,
				       PackageData *pack)
{
	if (pack->name && req->required->name) {
		return strcmp (req->required->name, pack->name);
	} else if (pack->provides && req->required->provides) {
		return strcmp ((char*)pack->provides->data, (char*)req->required->provides->data);
	} else {
		return -1;
	}
}

/* Given a package name, checks to see if theres a requirement already for this */
int 
eazel_install_requirement_dep_name_compare (PackageRequirement *req, 
					    const char *name)
{
	g_assert (req->required);
	g_assert (name);
	if (req->required->name) {
		return strcmp (req->required->name, name);
	} else if (req->required->provides) {
		return strcmp ((char*)(req->required->provides->data), name);
	} else {
		return -1;
	}
}


int 
eazel_install_package_version_compare (PackageData *pack, 
				       char *version)
{
	return strcmp (pack->version, version);
}

int 
eazel_install_package_other_version_compare (PackageData *pack, 
					     PackageData *other)
{
	if (pack->name && other->name) {
		if (strcmp (pack->name, other->name)==0) {
			if (pack->version && other->version) {
				if (strcmp (pack->version, other->version)) {
					return 0;
				} else {
					return 1;
				}
			} else {
				return -11;
			}
		} else {
			return 1;
		}
	} 
	return -1;
}

int 
eazel_install_package_matches_versioning (PackageData *a, 
					  const char *version,
					  const char *minor,
					  EazelSoftCatSense sense)
{
	int version_result = 0, minor_result = 0;

	g_assert (!((version==NULL) && minor));

	if (version) {
		if (sense & EAZEL_SOFTCAT_SENSE_EQ) {
			if (strcmp (a->version, version)==0) {
				version_result = 1;
			}
		}
		if ((version_result==0) && (sense & EAZEL_SOFTCAT_SENSE_GT)) {
			if (sense & EAZEL_SOFTCAT_SENSE_EQ) {
				if (strcmp (a->version, version)>=0) {
					version_result = 1;
				}
			} else {
				if (strcmp (a->version, version)>0) {
					version_result = 1;
				}
			}			
		}
		if ((version_result==0) && (sense & EAZEL_SOFTCAT_SENSE_LT)) {
			if (sense & EAZEL_SOFTCAT_SENSE_EQ) {
				if (strcmp (a->version, version)<=0) {
					version_result = 1;
				}
			} else {
				if (strcmp (a->version, version)<0) {
					version_result = 1;
				}
			}			
		}
	} else {
		version_result = 1;
	}

	if (minor) {
		if (sense & EAZEL_SOFTCAT_SENSE_EQ) {
			if (strcmp (a->minor, minor)==0) {
				minor_result = 1;
			}
		}
		if ((minor_result==0) && (sense & EAZEL_SOFTCAT_SENSE_GT)) {
			if (version_result) {
				minor_result = 1;
			}			
		}
		if ((minor_result==0) && (sense & EAZEL_SOFTCAT_SENSE_LT)) {
			if (version_result) {
				minor_result = 1;
			}			
		}
	} else {
		minor_result = 1;
	}

/*
	if (sense & EAZEL_SOFTCAT_SENSE_EQ) {
		if (version && minor) {
			if (strcmp (a->minor, minor)==0 &&
			    strcmp (a->version, version)==0) {
				result = 1;
			}
		} else if (version && !minor) {
			if (strcmp (a->version, version)==0) {
				result = 1;
			}
		} else if (!version && minor) {
			if (strcmp (a->minor, minor)==0) {
				result = 1;
			}
		} else if (!version && !minor) {
			result = 1;
		}
	}
	if ((result==0) && (sense & EAZEL_SOFTCAT_SENSE_GT)) {
		if (version && minor) {
			if (strcmp (a->minor, minor)>0 &&
			    strcmp (a->version, version)>0) {
				result = 1;
			}
		} else if (version && !minor) {
			if (strcmp (a->version, version)>0) {
				result = 1;
			}
		} else if (!version && minor) {
			if (strcmp (a->minor, minor)>0) {
				result = 1;
			}
		} else if (!version && !minor) {
			result = 1;
		}
	}
	if ((result==0) && (sense & EAZEL_SOFTCAT_SENSE_LT)) {
		if (version && minor) {
			if (strcmp (a->minor, minor)<0 &&
			    strcmp (a->version, version)<0) {
				result = 1;
			}
		} else if (version && !minor) {
			if (strcmp (a->version, version)<0) {
				result = 1;
			}
		} else if (!version && minor) {
			if (strcmp (a->minor, minor)<0) {
				result = 1;
			}
		} else if (!version && !minor) {
			result = 1;
		}
	}
	return result;
*/ 	
	return version_result && minor_result;
}

/* The evil marshal func */

typedef void (*GtkSignal_NONE__POINTER_INT_INT_INT_INT_INT_INT) (GtkObject * object,
                         gpointer arg1,
                         gint arg2,
                         gint arg3,			
                         gint arg4,
                         gint arg5,			
                         gint arg6,
                         gint arg7,			
                         gpointer user_data);
void
eazel_install_gtk_marshal_NONE__POINTER_INT_INT_INT_INT_INT_INT (GtkObject * object,
								 GtkSignalFunc func,
								 gpointer func_data, GtkArg * args)
{
  GtkSignal_NONE__POINTER_INT_INT_INT_INT_INT_INT rfunc;
  rfunc = (GtkSignal_NONE__POINTER_INT_INT_INT_INT_INT_INT) func;
  (*rfunc) (object,
	    GTK_VALUE_POINTER (args[0]),
	    GTK_VALUE_INT (args[1]), GTK_VALUE_INT (args[2]),
	    GTK_VALUE_INT (args[3]), GTK_VALUE_INT (args[4]),
	    GTK_VALUE_INT (args[5]), GTK_VALUE_INT (args[6]),func_data);
}


static void
gstr_indent (GString *out, int indent)
{
	for ( ; indent >= 8; indent -= 8) {
		g_string_sprintfa (out, "\t");
	}
	for ( ; indent > 0; indent--) {
		g_string_sprintfa (out, " ");
	}
}

static char *packagedata_dump_int (const PackageData *package, gboolean deep, int indent);

static void
dump_package_list (GString *out, const GList *list, gboolean deep, int indent)
{
	const GList *iter;
	char *name;
	char *tmp;

	if (deep) {
		g_string_sprintfa (out, "\n");
	}
	for (iter = g_list_first ((GList *)list); iter != NULL; iter = g_list_next (iter)) {
		if (deep) {
			tmp = packagedata_dump_int ((PackageData *)(iter->data), deep, indent+4);
			g_string_sprintfa (out, "%s", tmp);
			g_free (tmp);
		} else {
			name = packagedata_get_readable_name ((PackageData *)(iter->data));
			if (iter == list) {
				g_string_sprintfa (out, "%s", name);
			} else {
				g_string_sprintfa (out, ", %s", name);
			}
			g_free (name);
		}
	}
}

static void
dump_package_deplist (GString *out, const GList *list, gboolean deep, int indent)
{
	const GList *iter;
	PackageDependency *dep;
	char *name;
	char *tmp;
	char *sense;

	if (deep) {
		g_string_sprintfa (out, "\n");
	}
	for (iter = g_list_first ((GList *)list); iter != NULL; iter = g_list_next (iter)) {
		dep = (PackageDependency *)(iter->data);
		if (!deep && (iter != list)) {
			g_string_sprintfa (out, "; ");
		}
		if (deep) {
			tmp = packagedata_dump_int (dep->package, deep, indent+4);
			g_string_sprintfa (out, "%s", tmp);
			g_free (tmp);
			if (dep->version != NULL) {
				gstr_indent (out, indent+8);
				sense = eazel_softcat_sense_flags_to_string (dep->sense);
				g_string_sprintfa (out, "Solves %s %s %s\n", dep->package->name, sense,
						   dep->version);
				g_free (sense);
			}
		} else {
			if (dep->version != NULL) {
				sense = eazel_softcat_sense_flags_to_string (dep->sense);
				g_string_sprintfa (out, "[dep %s %s] ", sense, dep->version);
				g_free (sense);
			}
			name = packagedata_get_readable_name (dep->package);
			g_string_sprintfa (out, "%s", name);
			g_free (name);
		}
	}
}

/* useful debugging tool: dump a packagedata struct into a string */

static void
add_string_list (GString *out, GList *list, int indent, char *title) 
{
	GList *fit;
	for (fit = list; fit; fit = g_list_next (fit)) {
		gstr_indent (out, indent);
		g_string_sprintfa (out, "\t%s : %s\n", title, (char*)fit->data);
	}
}

static void
dump_package_break_list (GString *out, GList *breaks, gboolean deep, int indent)
{
	GList *iterator;

	for (iterator = breaks; iterator; iterator = g_list_next (iterator)) {
		PackageBreaks *breakage = PACKAGEBREAKS (iterator->data);
		char *readable_name = packagedata_get_readable_name (packagebreaks_get_package (breakage));

		gstr_indent (out, indent);

		g_string_sprintfa (out, "Breaks : %s\n", readable_name);
		if (IS_PACKAGEFEATUREMISSING (breakage)) {
			add_string_list (out, 
					 PACKAGEFILECONFLICT (breakage)->files,
					 indent,
					 "FeautureMissing");
		} else if (IS_PACKAGEFILECONFLICT (breakage)) {
			add_string_list (out, 
					 PACKAGEFEATUREMISSING (breakage)->features,
					 indent,
					 "FeautureMissing");
		} 
		
	}
}

static char *
packagedata_dump_int (const PackageData *package, gboolean deep, int indent)
{
	GString *out = g_string_new ("");
	GList *iter;
	char *dist_name;
	char *outstr;

	dist_name = trilobite_get_distribution_name (package->distribution, TRUE, TRUE);
	gstr_indent (out, indent);
	g_string_sprintfa (out, "Package %s v%s%s%s (arch %s) for %s\n",
			   (package->name != NULL) ? package->name : "(no name)",
			   (package->version != NULL) ? package->version : "",
			   (package->minor != NULL) ? "-" : "",
			   (package->minor != NULL) ? package->minor : "",
			   (package->archtype != NULL) ? package->archtype : "none",
			   dist_name);
	g_free (dist_name);

	indent += 4;
	gstr_indent (out, indent);
	g_string_sprintfa (out, "%s/%s",
			   packagedata_status_enum_to_str (package->status),
			   packagedata_modstatus_enum_to_str (package->modify_status));
	if (package->eazel_id != NULL) {
		g_string_sprintfa (out, ", EID %s", package->eazel_id);
	}
	if (package->bytesize > 0) {
		g_string_sprintfa (out, ", %d bytes installed", package->bytesize);
	}
	if (package->filesize > 0) {
		g_string_sprintfa (out, ", %d bytes file", package->filesize);
	}
	if (package->toplevel) {
		g_string_sprintfa (out, ", TOPLEVEL");
	}
	if (package->source_package) {
		g_string_sprintfa (out, ", source package");
	}
	if (package->conflicts_checked) {
		g_string_sprintfa (out, ", checked");
	}
	g_string_sprintfa (out, "\n");

	if (package->filename != NULL) {
		gstr_indent (out, indent);
		g_string_sprintfa (out, "Filename: %s\n", package->filename);
	}
	if (package->remote_url != NULL) {
		gstr_indent (out, indent);
		g_string_sprintfa (out, "URL: %s\n", package->remote_url);
	}
	if (package->md5 != NULL) {
		gstr_indent (out, indent);
		g_string_sprintfa (out, "MD5: %s\n", package->md5);
	}
	if (package->install_root != NULL) {
		gstr_indent (out, indent);
		g_string_sprintfa (out, "Install root: %s\n", package->install_root);
	}

	if (package->summary != NULL) {
		gstr_indent (out, indent);
		g_string_sprintfa (out, "Summary: %s\n", package->summary);
	}
	if (package->description != NULL) {
		gstr_indent (out, indent);
		g_string_sprintfa (out, "Description:\n%s\n", package->description);
	}

	if (package->features != NULL) {
		gstr_indent (out, indent);
		g_string_sprintfa (out, "Features: ");
		for (iter = g_list_first (package->features); iter != NULL; iter = g_list_next (iter)) {
			if (iter == package->features) {
				g_string_sprintfa (out, "%s", (char *)(iter->data));
			} else {
				g_string_sprintfa (out, "; %s", (char *)(iter->data));
			}
		}
		g_string_sprintfa (out, "\n");
	}

	if (deep && package->provides != NULL) {
		gstr_indent (out, indent);
		g_string_sprintfa (out, "Provides:\n");
		for (iter = g_list_first (package->provides); iter != NULL; iter = g_list_next (iter)) {
			g_string_sprintfa (out, "\t\t%s\n", (char *)(iter->data));
		}
		g_string_sprintfa (out, "\n");
	}

	if (package->soft_depends != NULL) {
		gstr_indent (out, indent);
		g_string_sprintfa (out, "Soft (old) depends: ");
		dump_package_list (out, package->soft_depends, deep, indent);
		g_string_sprintfa (out, "\n");
	}
	if (package->depends != NULL) {
		gstr_indent (out, indent);
		g_string_sprintfa (out, "Depends: ");
		dump_package_deplist (out, package->depends, deep, indent);
		g_string_sprintfa (out, "\n");
	}
	if (package->modifies != NULL) {
		gstr_indent (out, indent);
		g_string_sprintfa (out, "Modifies: ");
		dump_package_list (out, package->modifies, deep, indent);
		g_string_sprintfa (out, "\n");
	}
	if (package->breaks != NULL) {
		gstr_indent (out, indent);
		g_string_sprintfa (out, "Breaks: ");
		dump_package_break_list (out, package->breaks, deep, indent);
		g_string_sprintfa (out, "\n");
	}

	indent -= 4;

	outstr = out->str;
	g_string_free (out, FALSE);
	return outstr;
}

char *
packagedata_dump (const PackageData *package, gboolean deep)
{
	return packagedata_dump_int (package, deep, 0);
}


/**********************************************************************************
  GTK+ crap for PackageBreaks objects 
 **********************************************************************************/

static void
packagebreaks_finalize (GtkObject *obj)
{
	PackageBreaks *breaks = PACKAGEBREAKS (obj);
	gtk_object_unref (GTK_OBJECT (breaks->__package));
	trilobite_debug ("I'm DYING! %p", obj);
}

static void
packagebreaks_class_initialize (PackageBreaksClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass*)klass;
	object_class->finalize = packagebreaks_finalize;
}

static void
packagebreaks_initialize (PackageBreaks *breaks)
{
	g_assert (breaks);
	g_assert (IS_PACKAGEBREAKS (breaks));
	breaks->__package = NULL;
}

GtkType
packagebreaks_get_type (void)
{
	static GtkType object_type = 0;

	/* First time it's called ? */
	if (!object_type)
	{
		static const GtkTypeInfo object_info =
		{
			"PackageBreaks",
			sizeof (PackageBreaks),
			sizeof (PackageBreaksClass),
			(GtkClassInitFunc) packagebreaks_class_initialize,
			(GtkObjectInitFunc) packagebreaks_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		object_type = gtk_type_unique (gtk_object_get_type (), &object_info);
	}

	return object_type;
}

PackageBreaks*
packagebreaks_new (void)
{
	PackageBreaks *result;

	result = PACKAGEBREAKS (gtk_object_new (TYPE_PACKAGEBREAKS, NULL));
	gtk_object_ref (GTK_OBJECT (result));
	gtk_object_sink (GTK_OBJECT (result));
	
	return result;
}
void 
packagebreaks_set_package (PackageBreaks *breaks, 
			   PackageData *pack)
{
	if (breaks->__package) {
		gtk_object_unref (GTK_OBJECT (pack));
	}
	gtk_object_ref (GTK_OBJECT (pack));
	breaks->__package = pack;
}

PackageData *
packagebreaks_get_package (PackageBreaks *breaks)
{
	return breaks->__package;
}

/**********************************************************************************/

/**********************************************************************************
  GTK+ crap for PackageFileConflict objects 
 **********************************************************************************/

static void
packagefileconflict_finalize (GtkObject *obj)
{
	PackageFileConflict *conflict = PACKAGEFILECONFLICT (obj);
	g_list_foreach (conflict->files, (GFunc)g_free, NULL);
}

static void
packagefileconflict_class_initialize (PackageFileConflictClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass*)klass;
	object_class->finalize = packagefileconflict_finalize;
}

static void
packagefileconflict_initialize (PackageFileConflict *fileconflict)
{
	g_assert (fileconflict);
	g_assert (IS_PACKAGEFILECONFLICT (fileconflict));
}

GtkType
packagefileconflict_get_type (void)
{
	static GtkType object_type = 0;

	/* First time it's called ? */
	if (!object_type)
	{
		static const GtkTypeInfo object_info =
		{
			"PackageFileConflict",
			sizeof (PackageFileConflict),
			sizeof (PackageFileConflictClass),
			(GtkClassInitFunc) packagefileconflict_class_initialize,
			(GtkObjectInitFunc) packagefileconflict_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		object_type = gtk_type_unique (packagebreaks_get_type (), &object_info);
	}

	return object_type;
}

PackageFileConflict*
packagefileconflict_new (void)
{
	PackageFileConflict *result;

	result = PACKAGEFILECONFLICT (gtk_object_new (TYPE_PACKAGEFILECONFLICT, NULL));
	gtk_object_ref (GTK_OBJECT (result));
	gtk_object_sink (GTK_OBJECT (result));
	
	return result;
}

/**********************************************************************************/

/**********************************************************************************
  GTK+ crap for PackageFeatureMissing objects 
 **********************************************************************************/

static void
packagefeaturemissing_finalize (GtkObject *obj)
{
	PackageFeatureMissing *conflict = PACKAGEFEATUREMISSING (obj);
	g_list_foreach (conflict->features, (GFunc)g_free, NULL);
}

static void
packagefeaturemissing_class_initialize (PackageFeatureMissingClass *klass)
{
	GtkObjectClass *object_class;
	
	object_class = (GtkObjectClass*)klass;
	object_class->finalize = packagefeaturemissing_finalize;
}

static void
packagefeaturemissing_initialize (PackageFeatureMissing *breaks)
{
	g_assert (breaks);
	g_assert (IS_PACKAGEFEATUREMISSING (breaks));
}

GtkType
packagefeaturemissing_get_type (void)
{
	static GtkType object_type = 0;

	/* First time it's called ? */
	if (!object_type)
	{
		static const GtkTypeInfo object_info =
		{
			"PackageFeatureMissing",
			sizeof (PackageFeatureMissing),
			sizeof (PackageFeatureMissingClass),
			(GtkClassInitFunc) packagefeaturemissing_class_initialize,
			(GtkObjectInitFunc) packagefeaturemissing_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		object_type = gtk_type_unique (packagebreaks_get_type (), &object_info);
	}

	return object_type;
}

PackageFeatureMissing*
packagefeaturemissing_new (void)
{
	PackageFeatureMissing *result;

	result = PACKAGEFEATUREMISSING (gtk_object_new (TYPE_PACKAGEFEATUREMISSING, NULL));
	gtk_object_ref (GTK_OBJECT (result));
	gtk_object_sink (GTK_OBJECT (result));
	
	return result;
}

/**********************************************************************************/
