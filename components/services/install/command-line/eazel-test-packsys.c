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
#include <eazel-package-system.h>

#include <libtrilobite/trilobite-root-helper.h>

static PackageData*
make_package (char *name, char *version, char *minor)
{
	PackageData *p;
	p = packagedata_new ();
	p->name = g_strdup (name);
	p->version = g_strdup (version);
	p->minor = g_strdup (minor);
	return p;
}

static void
test_packagelist_prune (void)
{
	PackageData *p, *q;
	GList *in = NULL, *rm = NULL;

	p = make_package ("hest", "1.0", "1");
	q = make_package ("hest", "1.1", "1");
	in = g_list_prepend (in, p);
	rm = g_list_prepend (rm, q);

	p = make_package ("fisk", "1.0", "1");
	q = make_package ("fisk", "1.0", "2");
	in = g_list_prepend (in, p);
	rm = g_list_prepend (rm, q);

	p = make_package ("gris", "1.0", "1");
	q = make_package ("gris", "1.0", "1");
	in = g_list_prepend (in, p);
	rm = g_list_prepend (rm, q);

	p = make_package ("odder", "1.0", "1");
	in = g_list_prepend (in, p);
	p = make_package ("bæver", "1.0", "1");
	in = g_list_prepend (in, p);
	p = make_package ("kanin", "1.0", "1");
	in = g_list_prepend (in, p);
	p = make_package ("osteklokke", "1.0", "1");
	in = g_list_prepend (in, p);

	packagedata_list_prune (&in, rm, TRUE, TRUE);
	if (g_list_length (in) == 4) {
		g_message ("packagedata_list_prune ok");
	} else {
		g_message ("packagedata_list_prune FAIL");
	}
}

static void
test_package_load (EazelPackageSystem *packsys,
		   const char *package_file_name) 
{
	PackageData *p;
	int flag;

	flag = 0;
	p = eazel_package_system_load_package (packsys, NULL, package_file_name, flag);
	if (p->description==NULL && p->summary==NULL && p->provides==NULL) {
		g_message ("load_package test 1 ok");
	} else {
		g_message ("load_package test 1 FAIL");
	}

	packagedata_destroy (p, TRUE);

	flag = EAZEL_PACKAGE_SYSTEM_QUERY_DETAIL_DESCRIPTION;
	p = eazel_package_system_load_package (packsys, NULL, package_file_name, flag);
	if (p->description && p->summary==NULL && p->provides==NULL) {
		g_message ("load_package test 2 ok");
	} else {
		g_message ("load_package test 2 FAIL");
	}
	packagedata_destroy (p, TRUE);

	flag = EAZEL_PACKAGE_SYSTEM_QUERY_DETAIL_SUMMARY;
	p = eazel_package_system_load_package (packsys, NULL, package_file_name, flag);
	if (p->description==NULL && p->summary && p->provides==NULL) {
		g_message ("load_package test 3 ok");
	} else {
		g_message ("load_package test 3 FAIL");
	}
	packagedata_destroy (p, TRUE);

	flag = EAZEL_PACKAGE_SYSTEM_QUERY_DETAIL_FILES_PROVIDED;
	p = eazel_package_system_load_package (packsys, NULL, package_file_name, flag);
	if (p->description==NULL && p->summary==NULL && p->provides) {
		g_message ("load_package test 4 ok");
	} else {
		g_message ("load_package test 4 FAIL");
	}
	packagedata_destroy (p, TRUE);
}

static char *
get_password_dude (TrilobiteRootHelper *root_client, 
		   const char *prompt, 
		   void *user_data)
{
	char * real_prompt;
	char * passwd;

	real_prompt = g_strdup_printf ("%s: ", prompt);
	passwd = getpass (real_prompt);
	g_free (real_prompt);

	return g_strdup (passwd);
}

static EazelPackageSystem*
init_package_system (char *a_dbpath, char *a_root)
{
	EazelPackageSystem *packsys;
	TrilobiteRootHelper *root_helper;
	GList *dbpaths = NULL;

	dbpaths = g_list_prepend (dbpaths, a_root);
	dbpaths = g_list_prepend (dbpaths, a_dbpath);

	root_helper = trilobite_root_helper_new ();
	gtk_signal_connect (GTK_OBJECT (root_helper), "need_password", GTK_SIGNAL_FUNC (get_password_dude),
			    NULL);
	packsys = eazel_package_system_new (dbpaths);
	gtk_object_set_data (GTK_OBJECT (packsys), "trilobite-root-helper", root_helper);

	return packsys;
}

static void
test_matches_query (EazelPackageSystem *packsys) 
{
	GList *result;

	result = eazel_package_system_query (packsys,
					     NULL,
					     "rpm",
					     EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
					     0);
	if (g_list_length (result)==1) {
		g_message ("Query matches ok");
	} else {
		g_message ("Query matches fail");
	}
	g_list_free (result);
	
	result = eazel_package_system_query (packsys,
					     NULL,
					     "",
					     EAZEL_PACKAGE_SYSTEM_QUERY_SUBSTR,
					     0);
	if (g_list_length (result)>10) {
		g_message ("Query substr ok (%d hits)", g_list_length (result));
	} else {
		g_message ("Query substr fail");
	}
	
/*
	{
		GList *iterator;
		
		for (iterator = result; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack = (PackageData*)iterator->data;
			char *tmp = packagedata_get_readable_name (pack);
			g_message ("pacakge %s", tmp);
			g_free (tmp);
		}
	}
*/
	g_list_free (result);
}

static GList*
get_package_list (EazelPackageSystem *packsys,
		  const char *package_file_name)
{
	GList *packages = NULL;
	PackageData *package;

	package = eazel_package_system_load_package (packsys, NULL, package_file_name, 0);

	packages = g_list_prepend (packages, package);
	return packages;
}

static gboolean
start_signal (EazelPackageSystem *system,
	      EazelPackageSystemOperation op,
	      const PackageData *pack,
	      gboolean *signals)
{
	if (signals[0] == FALSE) {
		signals[0] = TRUE;
	} else {
		signals[3] = TRUE;
	}
	return TRUE;
}

static gboolean
end_signal (EazelPackageSystem *system,
	    EazelPackageSystemOperation op,
	    const PackageData *pack,
	    gboolean *signals)
{
	if (signals[2] == FALSE) {
		signals[2] = TRUE;
	} else {
		signals[3] = TRUE;
	}
	return TRUE;
}

static gboolean  
progress_signal (EazelPackageSystem *system,
		 EazelPackageSystemOperation op,
		 unsigned long *info,
		 const PackageData *pack,
		 gboolean *signals)
{
	signals[1] = TRUE;
	return TRUE;
}
	  
static gboolean
is_installed (EazelPackageSystem *packsys,
	      char *dbpath,
	      const char *package_file_name)
{
	GList *query;
	PackageData *package = eazel_package_system_load_package (packsys, 
								  NULL, 
								  package_file_name,
								  0);
	gboolean result = FALSE;

	query = eazel_package_system_query (packsys,
					    dbpath,
					    package->name,
					    EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
					    0);
	packagedata_destroy (package, TRUE);
	if (g_list_length (query) > 0) {
		result = TRUE;
	}
	return result;
}

static void
test_install (EazelPackageSystem *packsys,
	      char *dbpath,
	      const char *package_file_name)
{
	GList *packages = get_package_list (packsys, package_file_name);
	gboolean *signals;
	guint h1, h2, h3;

	signals = g_new0 (gboolean, 4);

	h1 = gtk_signal_connect (GTK_OBJECT (packsys), 
				 "start",
				 (GtkSignalFunc)start_signal,
				 signals);
	h2 = gtk_signal_connect (GTK_OBJECT (packsys), 
				 "end",
				 (GtkSignalFunc)end_signal,
				 signals);
	h3 = gtk_signal_connect (GTK_OBJECT (packsys), 
				 "progress",
				 (GtkSignalFunc)progress_signal,
				 signals);

	eazel_package_system_install (packsys,
				      dbpath,
				      packages,
				      EAZEL_PACKAGE_SYSTEM_OPERATION_UPGRADE|
				      EAZEL_PACKAGE_SYSTEM_OPERATION_DOWNGRADE|
				      EAZEL_PACKAGE_SYSTEM_OPERATION_FORCE);

	if (signals[3]) {
		g_message ("install FAIL");
	} else if (signals[0] && signals[1] && signals[2]) {
		if (is_installed (packsys, dbpath, package_file_name)) {
			g_message ("install ok");
		} else {
			g_message ("install FAIL");
		}
	} else {
		g_message ("install FAIL");
	}

	gtk_signal_disconnect (GTK_OBJECT (packsys), h1);
	gtk_signal_disconnect (GTK_OBJECT (packsys), h2);
	gtk_signal_disconnect (GTK_OBJECT (packsys), h3);
	g_free (signals);
}

static void
test_uninstall (EazelPackageSystem *packsys,
		char *dbpath,
		const char *package_file_name)
{
	GList *packages = get_package_list (packsys, package_file_name);
	gboolean *signals;
	guint h1, h2, h3;

	signals = g_new0 (gboolean, 4);

	h1 = gtk_signal_connect (GTK_OBJECT (packsys), 
				 "start",
				 (GtkSignalFunc)start_signal,
				 signals);
	h2 = gtk_signal_connect (GTK_OBJECT (packsys), 
				 "end",
				 (GtkSignalFunc)end_signal,
				 signals);
	h3 = gtk_signal_connect (GTK_OBJECT (packsys), 
				 "progress",
				 (GtkSignalFunc)progress_signal,
				 signals);

	eazel_package_system_uninstall (packsys,
					dbpath,
					packages,
					EAZEL_PACKAGE_SYSTEM_OPERATION_UPGRADE|
					EAZEL_PACKAGE_SYSTEM_OPERATION_UPGRADE|
					EAZEL_PACKAGE_SYSTEM_OPERATION_FORCE);

	if (signals[3]) {
		g_message ("install FAIL");
	} else if (signals[0] && signals[1] && signals[2]) {
		if (is_installed (packsys, dbpath, package_file_name)) {
			g_message ("install FAIL");
		} else {
			g_message ("install ok");
		}
	} else {
		g_message ("install FAIL");
	}

	gtk_signal_disconnect (GTK_OBJECT (packsys), h1);
	gtk_signal_disconnect (GTK_OBJECT (packsys), h2);
	gtk_signal_disconnect (GTK_OBJECT (packsys), h3);
	g_free (signals);
}

static void
test_verify (EazelPackageSystem *packsys,
	     char *dbpath,
	     const char *package_file_name)
{
	GList *packages = get_package_list (packsys, package_file_name);
	eazel_package_system_verify (packsys,
				     dbpath,
				     packages,
				     0);
}

int main(int argc, char *argv[]) {
	EazelPackageSystem *packsys;
	char *home_dbpath; 

	if (argc <=1 ) {
		g_error ("usage: %s <package file name>", argv[0]);
	}

	gnome_init ("Eazel Test Packsys", "1.0", argc, argv);
	home_dbpath = g_strdup_printf ("%s/.nautilus/packagedb", g_get_home_dir ());
	packsys = init_package_system (home_dbpath, g_strdup (g_get_home_dir ()));
	eazel_package_system_set_debug (packsys, EAZEL_PACKAGE_SYSTEM_DEBUG_VERBOSE); 

	test_package_load (packsys, argv[1]);
	test_packagelist_prune ();
	test_matches_query (packsys);

	test_install (packsys, home_dbpath, argv[1]);
	test_verify (packsys, home_dbpath, argv[1]);
	test_uninstall (packsys, home_dbpath, argv[1]);

	return 0;
};
