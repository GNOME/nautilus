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
 *
 */

#include <config.h>
#include <gnome.h>
#include "eazel-package-system-rpm3.h"
#include "eazel-package-system-private.h"
#include <libtrilobite/trilobite-core-utils.h>

#include <rpm/rpmlib.h>
#include <rpm/rpmmacro.h>
#include <rpm/dbindex.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

EazelPackageSystem* eazel_package_system_implementation (GList*);

/* This is the parent class pointer */
static EazelPackageSystemClass *eazel_package_system_rpm3_parent_class;

/************************************************************
*************************************************************/

static void
get_and_set_string_tag (Header hd,
			int tag, 
			char **str)
{
	char *tmp;

	g_assert (str);

	headerGetEntry (hd,
			tag, NULL,
			(void **) &tmp, NULL);
	g_free (*str);
	(*str) = g_strdup (tmp);
}

/************************************************************
 Load Package implemementation
*************************************************************/

static void 
rpm_packagedata_fill_from_rpm_header (PackageData *pack, 
				      Header hd,
				      int detail_level)
{
	unsigned long *sizep;

	get_and_set_string_tag (hd, RPMTAG_NAME, &pack->name);
	get_and_set_string_tag (hd, RPMTAG_VERSION, &pack->version);
	get_and_set_string_tag (hd, RPMTAG_RELEASE, &pack->minor);
	get_and_set_string_tag (hd, RPMTAG_ARCH, &pack->archtype);
	if (detail_level & EAZEL_INSTALL_PACKAGE_SYSTEM_QUERY_DETAIL_DESCRIPTION) {
		get_and_set_string_tag (hd, RPMTAG_DESCRIPTION, &pack->description);
	}
	if (detail_level & EAZEL_INSTALL_PACKAGE_SYSTEM_QUERY_DETAIL_SUMMARY) {
		get_and_set_string_tag (hd, RPMTAG_SUMMARY, &pack->summary);
	}

	headerGetEntry (hd,
			RPMTAG_SIZE, NULL,
			(void **) &sizep, NULL);	
	pack->bytesize = *sizep;

	/* FIXME: bugzilla.eazel.com 4862 */
	pack->packsys_struc = (gpointer)hd;


	/* FIXME: bugzilla.eazel.com 4863 */
	if (detail_level & EAZEL_INSTALL_PACKAGE_SYSTEM_QUERY_DETAIL_FILES_PROVIDED) {
		char **paths = NULL;
		char **paths_copy = NULL;
		char **names = NULL;
		int *indexes = NULL;
		int count = 0;
		int index = 0;
		int num_paths = 0;

		g_list_foreach (pack->provides, (GFunc)g_free, NULL);
		g_list_free (pack->provides);
		pack->provides = NULL;

                /* RPM v.3.0.4 and above has RPMTAG_BASENAMES */

		headerGetEntry (hd,			
				RPMTAG_DIRINDEXES, NULL,
				(void**)&indexes, NULL);
		headerGetEntry (hd,			
				RPMTAG_DIRNAMES, NULL,
				(void**)&paths, &num_paths);
		headerGetEntry (hd,			
				RPMTAG_BASENAMES, NULL,
				(void**)&names, &count);

		/* Copy all paths and shave off last /.
		   This is needed to remove the dir entries from 
		   the packagedata's provides list. */
		paths_copy = g_new0 (char*, num_paths);
		for (index=0; index<num_paths; index++) {
			paths_copy[index] = g_strdup (paths[index]);
			paths_copy[index][strlen (paths_copy[index]) - 1] = 0;
		}

		/* Now loop through all the basenames */
		/* NOTE: This algorithm has sizeof (paths) * sizeof (names)
		   complexity, aka O(n²) */
		for (index=0; index<count; index++) {
			char *fullname = NULL;
			int index2 = 0;

			if (paths) {
				fullname = g_strdup_printf ("%s/%s", paths_copy[indexes[index]], names[index]);
			} else {
				fullname = g_strdup (names[index]);
			}
			/* Check it's not a dirname, by looping through all
			   paths_copy and check that fullname does not occur there */
			for (index2 = 0; index2 < num_paths; index2++) {
				if (strcmp (paths_copy[index2], fullname)==0) {
					g_free (fullname);
					fullname = NULL;
					break;
				}
			}
			if (fullname) {
				/* trilobite_debug ("%s provides %s", pack->name, fullname);*/
				pack->provides = g_list_prepend (pack->provides, fullname);
			}
		}
		for (index=0; index<num_paths; index++) {
			g_free (paths_copy[index]);
		}
		g_free (paths_copy);
		xfree (paths);
		xfree (names);
	}
}

static gboolean 
rpm_packagedata_fill_from_file (PackageData *pack, const char *filename, int detail_level)
{
	static FD_t fd;
	Header hd;

	/* Set filename field */
	if (pack->filename != filename) {
		g_free (pack->filename);
		pack->filename = g_strdup (filename);
	}

	/* FIXME: bugzilla.eazel.com 4862 */
	if (pack->packsys_struc) {
		headerFree ((Header) pack->packsys_struc);
	}

	/* Open rpm */
	fd = fdOpen (filename, O_RDONLY, 0);

	if (fd == NULL) {
		g_warning (_("Cannot open %s"), filename);
		pack->status = PACKAGE_CANNOT_OPEN;
		return FALSE;
	}

	/* Get Header block */
	rpmReadPackageHeader (fd, &hd, &pack->source_package, NULL, NULL);
	rpm_packagedata_fill_from_rpm_header (pack, hd, detail_level);	

	pack->status = PACKAGE_UNKNOWN_STATUS;

	fdClose (fd);
	return TRUE;
}

static PackageData* 
rpm_packagedata_new_from_file (const char *file, int detail_level)
{
	PackageData *pack;

	pack = packagedata_new ();
	rpm_packagedata_fill_from_file (pack, file, detail_level);
	return pack;
}

static PackageData*
eazel_package_system_rpm3_load_package (EazelPackageSystemRpm3 *system,
				   PackageData *in_package,
				   const char *filename,
				   int detail_level)
{
	PackageData *result = NULL;

	if (in_package) {
		rpm_packagedata_fill_from_file (in_package, filename, detail_level);
	} else {
		result = rpm_packagedata_new_from_file (filename, detail_level);
	}

	return result;
}

/************************************************************
 Query implemementation
*************************************************************/

struct RpmQueryPiggyBag {
	EazelPackageSystemRpm3 *system;
	gpointer key;
	EazelPackageSystemQueryEnum flag;
	int detail_level;
	GList **result;
};

static void
eazel_package_system_rpm3_query_impl (EazelPackageSystemRpm3 *system,
				      const char *root,
				      const gpointer key,
				      EazelPackageSystemQueryEnum flag,
				      int detail_level,
				      GList **result)
{
	rpmdb db;
	int rc = 1;
	char *input = NULL;
	dbiIndexSet matches;

	db = g_hash_table_lookup (system->dbs, root);
	if (db==NULL) {
		return;
	}

	switch (flag) {
	case EAZEL_PACKAGE_SYSTEM_QUERY_OWNS:		
		input = g_strdup (key);
		rc = rpmdbFindByFile (db, input, &matches);
		g_free (input);
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_PROVIDES:		
		input = g_strdup (key);
		rc = rpmdbFindByProvides (db, input, &matches);
		g_free (input);
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES:
		input = packagedata_get_readable_name ((PackageData*)key);
		rc = rpmdbFindByRequiredBy (db, input, &matches);
		g_free (input);
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES:
		input = g_strdup (key);
		rc = rpmdbFindPackage (db, input, &matches);
		g_free (input);
		break;
	default:
		g_warning ("Unknown query");
	}
	
	if (rc == 0) {
		unsigned int i;

		for (i = 0; i < dbiIndexSetCount (matches); i++) {
			unsigned int offset;
			Header hd;
			PackageData *pack;
			
			offset = dbiIndexRecordOffset (matches, i);
			hd = rpmdbGetRecord (db, offset);
			pack = packagedata_new ();
			rpm_packagedata_fill_from_rpm_header (pack, hd, detail_level);
			g_free (pack->install_root);
			pack->install_root = g_strdup (root);
			if (g_list_find_custom (*result, 
						pack, 
						(GCompareFunc)eazel_install_package_compare)!=NULL) {
				packagedata_destroy (pack, TRUE);
			} else {
				(*result) = g_list_prepend (*result, pack);
			}
		}
		dbiFreeIndexRecord (matches);
	}
}

static void
eazel_package_system_rpm3_query_substr (EazelPackageSystemRpm3 *system,
					const char *root,
					const char *key,
					int detail_level,
					GList **result)
{
	int offset;
	rpmdb db;
	
	db = g_hash_table_lookup (system->dbs, root);
	if (db==NULL) {
		trilobite_debug ("db == NULL");
		return;
	}

	offset = rpmdbFirstRecNum (db);

	while (offset) {
		Header hd;
		PackageData *pack;

		hd = rpmdbGetRecord (db, offset);
		pack = packagedata_new ();

		rpm_packagedata_fill_from_rpm_header (pack, hd, 0);

		if (strstr (pack->name, key)) {
			rpm_packagedata_fill_from_rpm_header (pack, hd, detail_level);
			(*result) = g_list_prepend (*result, pack);
		} else {
			packagedata_destroy (pack, TRUE);
		}
		offset = rpmdbNextRecNum (db, offset);
	}
}

static void
eazel_package_system_rpm3_query_foreach (char *root,
					 rpmdb db,
					 struct RpmQueryPiggyBag *pig)
{
	switch (pig->flag) {
	case EAZEL_PACKAGE_SYSTEM_QUERY_OWNS:		
	case EAZEL_PACKAGE_SYSTEM_QUERY_PROVIDES:		
	case EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES:
	case EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES:
		eazel_package_system_rpm3_query_impl (pig->system,
						      root,
						      pig->key,
						      pig->flag,
						      pig->detail_level,
						      pig->result);
		break;
	case EAZEL_PACKAGE_SYSTEM_QUERY_SUBSTR:
		eazel_package_system_rpm3_query_substr (pig->system,
							root,
							pig->key,
							pig->detail_level,
							pig->result);
		break;
	default:
		g_warning ("Unknown query");
	}
}

static GList*               
eazel_package_system_rpm3_query (EazelPackageSystemRpm3 *system,
				 const char *root,
				 const gpointer key,
				 EazelPackageSystemQueryEnum flag,
				 int detail_level)
{
	GList *result = NULL;
	struct RpmQueryPiggyBag pig;
	
	pig.system = system;
	pig.key = key;
	pig.flag = flag;
	pig.detail_level = detail_level;
	pig.result = &result;
	
	if (root==NULL) {
		g_hash_table_foreach (system->dbs, 
				      (GHFunc)eazel_package_system_rpm3_query_foreach,
				      &pig);
	} else {
		eazel_package_system_rpm3_query_foreach ((char*)root, NULL, &pig);
	}

	return result;
}

/************************************************************
 Install implemementation
*************************************************************/

static void                 
eazel_package_system_rpm3_install (EazelPackageSystemRpm3 *system, 
				   const char *root,
				   GList* packages,
				   long flags,
				   gpointer userdata)
{
	trilobite_debug ("eazel_package_system_rpm3_install");
}

/************************************************************
 Uninstall implemementation
*************************************************************/

static void                 
eazel_package_system_rpm3_uninstall (EazelPackageSystemRpm3 *system, 
				     const char *root,
				     GList* packages,
				     long flags,
				     gpointer userdata)
{
	trilobite_debug ("eazel_package_system_rpm3_uninstall");
}

/************************************************************
 Verify implemementation
*************************************************************/

static void                 
eazel_package_system_rpm3_verify (EazelPackageSystemRpm3 *system, 
				  const char *root,
				  GList* packages,
				  long flags,
				  gpointer userdata)
{
	trilobite_debug ("eazel_package_system_rpm3_verify");
}

/************************************************************
 
*************************************************************/

static void
eazel_package_system_rpm3_open_roots (EazelPackageSystemRpm3 *system,
				      GList *roots)
{
	GList *iterator;
	
	g_assert (system);
	g_assert (IS_EAZEL_PACKAGE_SYSTEM_RPM3 (system));
	g_assert (system->dbs);

	/* First, ensure the db's exist */
	for (iterator = roots; iterator; iterator = g_list_next (iterator)) {
		char *root = (char*)iterator->data;
		addMacro (NULL, "_dbpath", NULL, "/", 0);
		if (strcmp (root, "/")) {
			trilobite_debug ("Creating %s", root);
			mkdir (root, 0700);
			rpmdbInit (root, 0644);
		}
	}

	trilobite_debug ("Read rpmrc file");
	rpmReadConfigFiles ("/usr/lib/rpm/rpmrc", NULL);
	
	for (iterator = roots; iterator; iterator = g_list_next (iterator)) {
		const char *root_dir;	
		rpmdb db;
		
		root_dir = (char*)iterator->data;
		
		addMacro(NULL, "_dbpath", NULL, "/", 0);
		if (rpmdbOpen (root_dir, &db, O_RDONLY, 0644)) {
			trilobite_debug ("Opening packages database in %s failed (a)", root_dir);
		} else {			
			if (db) {
				trilobite_debug (_("Opened packages database in %s"), root_dir);
				g_hash_table_insert (system->dbs,
						     g_strdup (root_dir),
						     db);
			} else {
				trilobite_debug (_("Opening packages database in %s failed"), root_dir);
			}
		}
	}


}

/*****************************************
  GTK+ object stuff
*****************************************/

static void
eazel_package_system_rpm3_finalize (GtkObject *object)
{
	EazelPackageSystemRpm3 *system;

	g_return_if_fail (object != NULL);
	g_return_if_fail (EAZEL_PACKAGE_SYSTEM_RPM3 (object));

	system = EAZEL_PACKAGE_SYSTEM_RPM3 (object);

	if (GTK_OBJECT_CLASS (eazel_package_system_rpm3_parent_class)->finalize) {
		GTK_OBJECT_CLASS (eazel_package_system_rpm3_parent_class)->finalize (object);
	}
}

static void
eazel_package_system_rpm3_class_initialize (EazelPackageSystemRpm3Class *klass) 
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass*)klass;
	object_class->finalize = eazel_package_system_rpm3_finalize;
	
	eazel_package_system_rpm3_parent_class = gtk_type_class (eazel_package_system_get_type ());
}

static void
eazel_package_system_rpm3_initialize (EazelPackageSystemRpm3 *system) {
	g_assert (system != NULL);
	g_assert (IS_EAZEL_PACKAGE_SYSTEM_RPM3 (system));
	
	system->dbs = g_hash_table_new (g_str_hash, g_str_equal);
}

GtkType
eazel_package_system_rpm3_get_type() {
	static GtkType system_type = 0;

	/* First time it's called ? */
	if (!system_type)
	{
		static const GtkTypeInfo system_info =
		{
			"EazelPackageSystemRpm3",
			sizeof (EazelPackageSystemRpm3),
			sizeof (EazelPackageSystemRpm3Class),
			(GtkClassInitFunc) eazel_package_system_rpm3_class_initialize,
			(GtkObjectInitFunc) eazel_package_system_rpm3_initialize,
			/* reserved_1 */ NULL,
			/* reserved_2 */ NULL,
			(GtkClassInitFunc) NULL,
		};

		system_type = gtk_type_unique (eazel_package_system_get_type (), &system_info);
	}

	return system_type;
}

EazelPackageSystemRpm3 *
eazel_package_system_rpm3_new (GList *roots) 
{
	EazelPackageSystemRpm3 *system;

	system = EAZEL_PACKAGE_SYSTEM_RPM3 (gtk_object_new (TYPE_EAZEL_PACKAGE_SYSTEM_RPM3, NULL));

	gtk_object_ref (GTK_OBJECT (system));
	gtk_object_sink (GTK_OBJECT (system));

	eazel_package_system_rpm3_open_roots (system, roots);

	return system;
}

EazelPackageSystem*
eazel_package_system_implementation (GList *roots)
{
	EazelPackageSystem *result;
	GList *troot;

	trilobite_debug ("eazel_package_system_implementation (rpm3)");

	troot = g_list_prepend (g_list_copy (roots), "/var/lib/rpm");
	result = EAZEL_PACKAGE_SYSTEM (eazel_package_system_rpm3_new (troot));
	g_list_free (troot);
	
	result->private->load_package = (EazelPackageSytem_load_package)eazel_package_system_rpm3_load_package;
	result->private->query = (EazelPackageSytem_query)eazel_package_system_rpm3_query;
	result->private->install = (EazelPackageSytem_install)eazel_package_system_rpm3_install;
	result->private->uninstall = (EazelPackageSytem_uninstall)eazel_package_system_rpm3_uninstall;
	result->private->verify = (EazelPackageSytem_verify)eazel_package_system_rpm3_verify;

	return result;
}
