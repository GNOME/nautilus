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
 *          Ian McKellar <ian@eazel.com>
 *
 */

#include <config.h>
#include <gnome.h>
#include "eazel-package-system-dpkg.h"
#include "eazel-package-system-private.h"
#include <libtrilobite/trilobite-core-utils.h>
#include <ctype.h>

#define MAX_LINE_LEN 4096
#define ROOT "su -c "

typedef struct {
	char *name;
	char *version;
	char *minor;
	char *archtype;

	char *summary;
	char *description;

	char *status;
} DebPackage;

typedef gboolean (*PackageParseCallback) (const DebPackage *package, 
		gpointer data);

EazelPackageSystem* eazel_package_system_implementation (GList*);

/* This is the parent class pointer */
static EazelPackageSystemClass *eazel_package_system_dpkg_parent_class;

static void
debpackage_free (DebPackage *package) 
{
	g_free (package->name);
	g_free (package->version);
	g_free (package->minor);
	g_free (package->archtype);
	g_free (package->summary);
	g_free (package->description);
	g_free (package->status);
	g_free (package);
}

static void
debpackage_fill_packagedata (const DebPackage *dpkg, 
			     PackageData *pdata) 
{
	g_return_if_fail (dpkg != NULL);
	g_return_if_fail (pdata != NULL);

	if (dpkg->name) {
		pdata->name = g_strdup (dpkg->name);
	}
	if (dpkg->version) {
		pdata->version = g_strdup (dpkg->version);
	}
	if (dpkg->minor) {
		pdata->minor = g_strdup (dpkg->minor);
	}
	if (dpkg->archtype) {
		pdata->archtype = g_strdup (dpkg->archtype);
	}
	if (dpkg->summary) {
		pdata->summary = g_strdup (dpkg->summary);
	}
	if (dpkg->description) {
		pdata->description = g_strdup (dpkg->description);
	}

	/* FIXME is there an equivalent of status? */
}

static void 
strip_trailing_whitespace (char *string) 
{
	int i = strlen (string)-1;

	while ( (i>=0) && isspace(string[i])) {
		string[i] = '\0';
		i--;
	}
}


static void 
parse_packages (FILE *packages_file, 
		PackageParseCallback cb,
		gpointer user_data)
{
	char line[MAX_LINE_LEN+1];
	char *value;
	char **previous = NULL;
	DebPackage *package;

	g_return_if_fail (packages_file != NULL);

	package = g_new0 (DebPackage, 1);

	while (!feof (packages_file) && !ferror (packages_file)) {
		fgets (line, MAX_LINE_LEN, packages_file);
		strip_trailing_whitespace (line);
		//g_print ("line: %s\n", line);

		if (isspace (line[0])) {
			/* continuation */
			if (previous) {
				char *tmp1, *tmp2 = *previous;

				//g_print("continuing line...\n");
				tmp1 = g_strconcat (tmp2, "\n", line+1, NULL);
				g_free (tmp2);
				*previous = tmp1;
			}
			continue;
		}

		value = strchr (line, ':');

		if (value == NULL) {
			/* found start of new package */
			(cb)(package, user_data); /*check return? FIXME*/
			debpackage_free (package);
			package = g_new0 (DebPackage, 1);
			previous = NULL;
			continue;
		}

		*value = '\0';
		value++;
		while (isspace (*value)) {
			*value = '\0';
			value++;
		}

		previous = NULL;

		if (!strcmp (line, "Package")) {
			package->name = g_strdup (value);
			previous = &package->name;
		} else if (!strcmp (line, "Version")) {
			/* FIXME: is this the defn of minor */
			char *minor = strrchr (line, '-');

			if (minor) {
				*minor = '\0';
				minor++;
				package->minor = g_strdup (value);
			}
			package->version = g_strdup (value);
			previous = &package->version;
		} else if (!strcmp (line, "Architecture")) {
			package->archtype = g_strdup (value);
			previous = &package->archtype;
		} else if (!strcmp (line, "Description")) {
			package->summary = g_strdup (value);
			package->description = g_strdup (value);
			previous = &package->description;
		} else if (!strcmp (line, "Status")) {
			package->status = g_strdup (value);
			previous = &package->status;
		} else {
			//g_print ("Ignoring [%s][%s]\n", line, value);
		}
	}
	(cb)(package, user_data);
	debpackage_free (package);
}

static gboolean
load_package_callback (const DebPackage *package, 
		       gpointer data) 
{
	PackageData *pdata = data;

	g_print ("cb: %s\n", package->name);
	g_print ("cb desc: %s\n", package->description);
	debpackage_fill_packagedata (package, pdata);
	return TRUE;
}

static PackageData*
eazel_package_system_dpkg_load_package (EazelPackageSystemDpkg *system,
					PackageData *in_package,
					const char *filename,
					int detail_level)
{
	PackageData *result = in_package;
	char *dpkg_cmd;
	FILE *dpkg_file;

	g_assert (system != NULL);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_DPKG (system));
	trilobite_debug ("eazel_package_system_dpkg_load_package");

	dpkg_cmd = g_strdup_printf ("dpkg --field %s", filename);
	dpkg_file = popen (dpkg_cmd, "r");
	if (dpkg_file == NULL) {
		g_warning ("failed to run `%s'", dpkg_cmd);
	}
	g_free (dpkg_cmd);

	if (result == NULL) {
		result = packagedata_new ();
	}

	parse_packages (dpkg_file, load_package_callback, result);

	result->filename = g_strdup (filename);

	pclose (dpkg_file);

	return result;
}

struct QueryData {
	gpointer key;
	EazelPackageSystemQueryEnum flag;
	GList *packages;
};

static gboolean
query_callback (const DebPackage *package, 
		gpointer data) 
{
	struct QueryData *query_data = data;
	PackageData *pd;

	g_print ("cb: %s\n", package->name);

	if ( (package->status == NULL) ||
			strcmp (package->status, "install ok installed")) {
		/* hmm - not actually installed */
		g_print ("cb discarding\n");
		return TRUE;
	}
	g_print ("cb keeping\n");

	/* FIXME: umm - actually check that it matches */
	
	pd = packagedata_new ();
	debpackage_fill_packagedata (package, pd);
	query_data->packages = g_list_append (query_data->packages, pd);
	return TRUE;
}

static GList*               
eazel_package_system_dpkg_query (EazelPackageSystemDpkg *system,
				 const char *dbpath,
				 const gpointer key,
				 EazelPackageSystemQueryEnum flag,
				 int detail_level)
{
	struct QueryData query_data;
	FILE *status;

	g_assert (system != NULL);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_DPKG (system));
	trilobite_debug ("eazel_package_system_dpkg_query");

	query_data.key = key;
	query_data.flag = flag;
	query_data.packages = NULL;

	/* FIMXE: lock db! */

	status = fopen ("/var/lib/dpkg/status", "rt");

	if (status == NULL) return NULL;

	parse_packages (status, query_callback, &query_data);

	fclose (status);

	return query_data.packages;
}

static void                 
eazel_package_system_dpkg_install (EazelPackageSystemDpkg *epsystem, 
				   const char *dbpath,
				   GList* packages,
				   unsigned long flags)
{
	char *dpkg_cmd;
	int result;
	PackageData *pdata;

	g_assert (epsystem != NULL);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_DPKG (epsystem));
	trilobite_debug ("eazel_package_system_dpkg_install");
	/* Code Here */

	while (packages) {
		pdata = (PackageData *)packages->data;

		dpkg_cmd = g_strdup_printf (ROOT "dpkg --unpack %s", 
				pdata->filename);
		g_print ("[YAK] running: %s\n", dpkg_cmd);
		result = system (dpkg_cmd);
		g_print ("[YAK] result: %d\n", result);
		g_free (dpkg_cmd);

		/* FIXME: erm - perhaps pop up some kind of zvt???? */
		dpkg_cmd = g_strdup_printf (ROOT "dpkg --configure %s", 
				pdata->name);
		g_print ("[YAK] running: %s\n", dpkg_cmd);
		result = system (dpkg_cmd);
		g_print ("[YAK] result: %d\n", result);
		g_free (dpkg_cmd);
		g_free (dpkg_cmd);

		packages = packages->next;
	}

}

static void                 
eazel_package_system_dpkg_uninstall (EazelPackageSystemDpkg *system, 
				     const char *dbpath,
				     GList* packages,
				     unsigned long flags)
{
	g_assert (system != NULL);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_DPKG (system));
	trilobite_debug ("eazel_package_system_dpkg_uninstall");
	/* Code Here */
}

static void                 
eazel_package_system_dpkg_verify (EazelPackageSystemDpkg *system, 
				  GList* packages)
{
	g_assert (system != NULL);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_DPKG (system));
	trilobite_debug ("eazel_package_system_dpkg_verify");
	/* Code Here */
}

static int
eazel_package_system_dpkg_compare_version (EazelPackageSystem *system,
					   const char *a,
					   const char *b)
{
	g_assert (system != NULL);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_DPKG (system));
	trilobite_debug ("eazel_package_system_dpkg_compare_version");
	/* Code Here */
	return 0;
}


/*****************************************
  GTK+ object stuff
*****************************************/

static void
eazel_package_system_dpkg_finalize (GtkObject *object)
{
	EazelPackageSystemDpkg *system;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_PACKAGE_SYSTEM_DPKG (object));

	system = EAZEL_PACKAGE_SYSTEM_DPKG (object);

	if (GTK_OBJECT_CLASS (eazel_package_system_dpkg_parent_class)->finalize) {
		GTK_OBJECT_CLASS (eazel_package_system_dpkg_parent_class)->finalize (object);
	}
}

static void
eazel_package_system_dpkg_class_initialize (EazelPackageSystemDpkgClass *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->finalize = eazel_package_system_dpkg_finalize;
	
	eazel_package_system_dpkg_parent_class = gtk_type_class (eazel_package_system_get_type ());
}

static void
eazel_package_system_dpkg_initialize (EazelPackageSystemDpkg *system) {
	g_assert (system != NULL);
	g_assert (EAZEL_IS_PACKAGE_SYSTEM_DPKG (system));
}

GtkType
eazel_package_system_dpkg_get_type() {
	static GtkType system_type = 0;

	/* First time it's called ? */
	if (!system_type)
	{
		static const GtkTypeInfo system_info =
		{
			"EazelPackageSystemDpkg",
			sizeof (EazelPackageSystemDpkg),
			sizeof (EazelPackageSystemDpkgClass),
			(GtkClassInitFunc) eazel_package_system_dpkg_class_initialize,
			(GtkObjectInitFunc) eazel_package_system_dpkg_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		system_type = gtk_type_unique (eazel_package_system_get_type (), &system_info);
	}

	return system_type;
}

EazelPackageSystemDpkg *
eazel_package_system_dpkg_new (GList *dbpaths) 
{
	EazelPackageSystemDpkg *system;

	system = EAZEL_PACKAGE_SYSTEM_DPKG (gtk_object_new (TYPE_EAZEL_PACKAGE_SYSTEM_DPKG, NULL));

	gtk_object_ref (GTK_OBJECT (system));
	gtk_object_sink (GTK_OBJECT (system));

	return system;
}

EazelPackageSystem*
eazel_package_system_implementation (GList *dbpaths)
{
	EazelPackageSystem *result;

	trilobite_debug ("eazel_package_system_implementation (dpkg)");

	result = EAZEL_PACKAGE_SYSTEM (eazel_package_system_dpkg_new (dbpaths));
	
	result->private->load_package
		= (EazelPackageSytemLoadPackageFunc)eazel_package_system_dpkg_load_package;

	result->private->query
		= (EazelPackageSytemQueryFunc)eazel_package_system_dpkg_query;

	result->private->install
		= (EazelPackageSytemInstallFunc)eazel_package_system_dpkg_install;

	result->private->uninstall
		= (EazelPackageSytemUninstallFunc)eazel_package_system_dpkg_uninstall;

	result->private->verify
		= (EazelPackageSytemVerifyFunc)eazel_package_system_dpkg_verify;

	result->private->compare_version
		= (EazelPackageSystemCompareVersionFunc)eazel_package_system_dpkg_compare_version;

	return result;
}
