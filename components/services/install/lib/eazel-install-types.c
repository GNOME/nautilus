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

#include <libtrilobite/trilobite-core-utils.h>

/* #define DEBUG_PACKAGE_ALLOCS */

#ifdef DEBUG_PACKAGE_ALLOCS
static int package_allocs = 0;
static int category_allocs = 0;
#endif /* DEBUG_PACKAGE_ALLOCS */

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

CategoryData*
categorydata_new (void)
{
	CategoryData *result;

	result = g_new0 (CategoryData, 1);
#ifdef DEBUG_PACKAGE_ALLOCS
	category_allocs ++;
	trilobite_debug ("category_allocs inced to %d (0x%x)", category_allocs, result);
#endif /* DEBUG_PACKAGE_ALLOCS */
	return result;
}

void
categorydata_destroy_foreach (CategoryData *cd, gpointer ununsed)
{
#ifdef DEBUG_PACKAGE_ALLOCS
	category_allocs --;
	trilobite_debug ("category_allocs = %d (0x%x) %s", category_allocs, cd, cd ? cd->name: "?");
#endif /* DEBUG_PACKAGE_ALLOCS */

	g_return_if_fail (cd != NULL);
	g_free (cd->name);
	cd->name = NULL;
	g_free (cd->description);
	cd->description = NULL;
	if (g_list_length (cd->packages)) {
		g_list_foreach (cd->packages, (GFunc)packagedata_destroy, GINT_TO_POINTER (TRUE));
	}
	if (g_list_length (cd->depends)) {
		g_list_foreach (cd->depends, (GFunc)g_free, NULL);
	}
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

#ifdef DEBUG_PACKAGE_ALLOCS
	package_allocs ++;
	trilobite_debug ("package_allocs inced to %d (0x%x)", package_allocs, pack);
#endif /* DEBUG_PACKAGE_ALLOCS */

	
	pack->name = NULL;
	pack->version = NULL;
	pack->minor = NULL;
	pack->archtype = NULL;
	pack->source_package = FALSE;
	pack->description = NULL;
	pack->bytesize = 0;
	pack->distribution = trilobite_get_distribution ();
	pack->filename = NULL;
	pack->eazel_id = NULL;
	pack->remote_url = NULL;
	pack->conflicts_checked = FALSE;
	pack->install_root = NULL;
	pack->provides = NULL;
	pack->soft_depends = NULL;
	pack->hard_depends = NULL;
	pack->breaks = NULL;
	pack->modifies = NULL;
	pack->status = PACKAGE_UNKNOWN_STATUS;
	pack->modify_status = PACKAGE_MOD_UNTOUCHED;
	pack->md5 = NULL;
	pack->packsys_struc = NULL;
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

/* FIXME bugzilla.eazel.com 2351:
   check possible leaks from using headerGetEntry.
   Addition ; it looks like it depends on the tag type. From reading
   in rpm-3.0.4/lib/header.c(copyEntry), which is called by headerGetEntry,
   some types get a new memory array returned, whereas others get a pointer
   into the header...
   grr....
*/
void 
packagedata_fill_from_rpm_header (PackageData *pack, 
				  Header *hd) 
{
	unsigned long *sizep;
	char *tmp;

	headerGetEntry (*hd,
			RPMTAG_NAME, NULL,
			(void **) &tmp, NULL);
	g_free (pack->name);
	pack->name = g_strdup (tmp);

	headerGetEntry (*hd,
			RPMTAG_VERSION, NULL,
			(void **) &tmp, NULL);
	g_free (pack->version);
	pack->version = g_strdup (tmp);

	headerGetEntry (*hd,
			RPMTAG_RELEASE, NULL,
			(void **) &tmp, NULL);
	g_free (pack->minor);
	pack->minor = g_strdup (tmp);

	headerGetEntry (*hd,
			RPMTAG_ARCH, NULL,
			(void **) &tmp, NULL);
	g_free (pack->archtype);
	pack->archtype = g_strdup (tmp);

	headerGetEntry (*hd,
			RPMTAG_SIZE, NULL,
			(void **) &sizep, NULL);	
	pack->bytesize = *sizep;

	headerGetEntry (*hd,
			RPMTAG_DESCRIPTION, NULL,
			(void **) &tmp, NULL);
	g_free (pack->description);
	pack->description = g_strdup (tmp);

	pack->packsys_struc = (gpointer)hd;

	g_list_foreach (pack->provides, (GFunc)g_free, NULL);
	g_list_free (pack->provides);
	pack->provides = NULL;

	{
		char **paths = NULL;
		char **names = NULL;
		int *indexes = NULL;
		int count;
		int index;

/* RPM v.3.0.4 and above has RPMTAG_BASENAMES,
   Lets see if RPMTAG_PROVIDES works for the older ones */
#ifdef RPMTAG_BASENAMES
		headerGetEntry (*hd,			
				RPMTAG_DIRINDEXES, NULL,
				(void**)&indexes, NULL);
		headerGetEntry (*hd,			
				RPMTAG_DIRNAMES, NULL,
				(void**)&paths, NULL);
		headerGetEntry (*hd,			
				RPMTAG_BASENAMES, NULL,
				(void**)&names, &count);
#else /* RPMTAG_BASENAMES */
		/* This will most like make eazel_install_chekc_for_file_conflicts break... */
		headerGetEntry (*hd,			
				RPMTAG_FILENAMES, NULL,
				(void**)&names, &count);
#endif /* RPMTAG_BASENAMES */
		
		for (index=0; index<count; index++) {
			char *fullname;
			if (paths) {
				fullname = g_strdup_printf ("%s%s", paths[indexes[index]], names[index]);
			} else {
				fullname = g_strdup (names[index]);
			}
			/* trilobite_debug ("%s provides %s", pack->name, fullname); */
			pack->provides = g_list_prepend (pack->provides, fullname);
		}
	}
}

/* FIXME bugzilla.eazel.com 1532:
   RPM specific code */
PackageData* 
packagedata_new_from_file (const char *file)
{
	PackageData *pack;

	pack = packagedata_new ();
	packagedata_fill_from_file (pack, file);
	return pack;
}

/* FIXME bugzilla.eazel.com 1532:
   RPM specific code */
/* 
   This fills the fields from a given file.
*/
gboolean 
packagedata_fill_from_file (PackageData *pack, const char *filename)
{
	static FD_t fd;
	Header *hd;

	/* Set filename field */
	if (pack->filename != filename) {
		g_free (pack->filename);
		pack->filename = g_strdup (filename);
	}

	/* Already loaded a packsys struc ? */
	if (pack->packsys_struc) {
		/* FIXME bugzilla.eazel.com
		   This probably is a leak... */
		g_free (pack->packsys_struc);
	}

	/* Open rpm */
	fd = fdOpen (filename, O_RDONLY, 0);

	if (fd == NULL) {
		g_warning (_("Cannot open %s"), filename);
		pack->status = PACKAGE_CANNOT_OPEN;
		return FALSE;
	}

	/* Get Header block */
	hd = g_new0 (Header, 1);
	rpmReadPackageHeader (fd, hd, &pack->source_package, NULL, NULL);
	packagedata_fill_from_rpm_header (pack, hd);	

	pack->status = PACKAGE_UNKNOWN_STATUS;

	fdClose (fd);
	return TRUE;
}

void 
packagedata_destroy (PackageData *pack, gboolean deep)
{
#ifdef DEBUG_PACKAGE_ALLOCS
	package_allocs --;
	if (pack) {
		if (pack->name) {
			trilobite_debug ("package_allocs = %d (0x%x) %s", package_allocs, pack,pack->name);
		} else if (pack->provides) {
			trilobite_debug ("package_allocs = %d (0x%x) providing %s", package_allocs, pack,
					 (char*)pack->provides->data);
		} else {
			trilobite_debug ("package_allocs = %d (0x%x) ?", package_allocs, pack);
		}
	} else {
		trilobite_debug ("package_allocs = %d (0x%x) ??", package_allocs, pack);
	}
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
	g_free (pack->description);
	pack->description = NULL;
	pack->bytesize = 0;
	g_free (pack->filename);
	pack->filename = NULL;
	g_free (pack->eazel_id);
	pack->eazel_id = NULL;
	g_free (pack->remote_url);
	pack->remote_url = NULL;
	g_free (pack->install_root);
	pack->install_root = NULL;
	g_free (pack->md5);
	pack->md5 = NULL;
	g_list_foreach (pack->provides, (GFunc)g_free, NULL); 
	pack->provides = NULL;

	if (deep) {
		g_list_foreach (pack->soft_depends, (GFunc)packagedata_destroy, GINT_TO_POINTER (deep));
		g_list_foreach (pack->hard_depends, (GFunc)packagedata_destroy, GINT_TO_POINTER (deep));
		g_list_foreach (pack->breaks, (GFunc)packagedata_destroy, GINT_TO_POINTER (deep));
		g_list_foreach (pack->modifies, (GFunc)packagedata_destroy, GINT_TO_POINTER (deep));
	}
	pack->soft_depends = NULL;
	pack->hard_depends = NULL;
	pack->breaks = NULL;
	pack->modifies = NULL;

	if (pack->packsys_struc) {
		/* FIXME bugzilla.eazel.com 1532:
		   RPM specific code */
		/* even better, this just crashes 
		   headerFree (*pack->packsys_struc); */
		g_free (pack->packsys_struc);
	}

	g_free (pack);
}

void 
packagedata_remove_soft_dep (PackageData *remove, 
			     PackageData *from)
{
	g_assert (remove);
	g_assert (from);

	trilobite_debug ("removing %s from %s's deps", remove->name, from->name);
	from->soft_depends = g_list_remove (from->soft_depends, remove);
	packagedata_destroy (remove, TRUE);
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
packagedata_add_pack_to (GList **list, PackageData *pack) {
	(*list) = g_list_prepend (*list, pack);
}

void 
packagedata_add_pack_to_breaks (PackageData *pack, PackageData *b) 
{
	g_assert (pack);
	g_assert (b);
	g_assert (pack != b);
	packagedata_add_pack_to (&pack->breaks, b);
}

void 
packagedata_add_pack_to_soft_depends (PackageData *pack, PackageData *b)
{
	g_assert (pack);
	g_assert (b);
	g_assert (pack != b);
	packagedata_add_pack_to (&pack->soft_depends, b);
}

void 
packagedata_add_pack_to_hard_depends (PackageData *pack, PackageData *b)
{
	g_assert (pack);
	g_assert (b);
	g_assert (pack != b);
	packagedata_add_pack_to (&pack->hard_depends, b);
}

void 
packagedata_add_pack_to_modifies (PackageData *pack, PackageData *b)
{
	g_assert (pack);
	g_assert (b);
	g_assert (pack != b);
	packagedata_add_pack_to (&pack->modifies, b);
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

/* This does a slow and painfull comparison of all the major fields */
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
		return strcmp ((char*)pack->provides->data, (char*)req->required->provides);
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

