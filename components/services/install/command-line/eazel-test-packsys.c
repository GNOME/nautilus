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
#include <time.h>
#include <gnome.h>
#include <eazel-package-system.h>

#include <libtrilobite/trilobite-root-helper.h>

#define MATCHES_ONLY_ONE "glibc"
#define PROVIDED_BY_ONLY_ONE "libc.so.6"
#define OWNED_BY_ONLY_ONE "/bin/sh"
#define NEEDED_BY_MANY "glibc"

static void
test_database_mtime (EazelPackageSystem *packsys)
{
	time_t mtime = eazel_package_system_database_mtime (packsys);
	char *tmp;

	if (mtime == 0) {
		g_message ("Couldn't get package system database mtime.\n");
	} else {
		tmp = ctime (&mtime);
		g_message ("Package system database mtime: %s\n", tmp);
		g_free (tmp);
	}
}

static void
test_is_installed (EazelPackageSystem *packsys)
{
	GList *result;

	result = eazel_package_system_query (packsys,
					     NULL,
					     MATCHES_ONLY_ONE,
					     EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
					     0);
	if (g_list_length (result)==1) {
		PackageData *p;
		p = PACKAGEDATA (result->data);
		if (eazel_package_system_is_installed (packsys,
						       NULL,
						       p->name,
						       p->version,
						       p->minor,
						       EAZEL_SOFTCAT_SENSE_EQ)) {
			g_message ("is_installed 1 ok");
		} else {
			g_message ("is_installed 1 FAILED");
		}
		if (eazel_package_system_is_installed (packsys,
						       NULL,
						       p->name,
						       p->version,
						       p->minor,
						       EAZEL_SOFTCAT_SENSE_GE)) {
			g_message ("is_installed 2 ok");
		} else {
			g_message ("is_installed 2 FAILED");
		}
		if (eazel_package_system_is_installed (packsys,
						       NULL,
						       p->name,
						       p->version,
						       p->minor,
						       EAZEL_SOFTCAT_SENSE_LT)) {
			g_message ("is_installed 3 FAILED");
		} else {
			g_message ("is_installed 3 ok");
		}
		if (eazel_package_system_is_installed (packsys,
						       NULL,
						       p->name,
						       p->version,
						       p->minor,
						       EAZEL_SOFTCAT_SENSE_GT)) {
			g_message ("is_installed 4 FAILED");
		} else {
			g_message ("is_installed 4 ok");
		}
		if (eazel_package_system_is_installed (packsys,
						       NULL,
						       p->name,
						       "0",
						       NULL,
						       EAZEL_SOFTCAT_SENSE_GE)) {
			g_message ("is_installed 5 ok");
		} else {
			g_message ("is_installed 5 FAILED");
		}
		if (eazel_package_system_is_installed (packsys,
						       NULL,
						       p->name,
						       "1000",
						       NULL,
						       EAZEL_SOFTCAT_SENSE_GE)) {
			g_message ("is_installed 6 FAILED");
		} else {
			g_message ("is_installed 6 ok");
		}
	}
	g_list_foreach (result, (GFunc)gtk_object_unref, NULL);
	g_list_free (result);
}

static void
test_version_compare (EazelPackageSystem *packsys)
{
	g_message ("version compare M1 is %s",
		   eazel_package_system_compare_version (packsys, "M18", "M17") > 0 ? "ok" : "FAILED");
	g_message ("version compare M2 is %s",
		   eazel_package_system_compare_version (packsys, "M17", "M18") < 0 ? "ok" : "FAILED");
	g_message ("version compare M3 is %s",
		   eazel_package_system_compare_version (packsys, "M18", "0.7") < 0 ? "ok" : "FAILED");
	g_message ("version compare M4 is %s",
		   eazel_package_system_compare_version (packsys, "0.7", "M18") > 0 ? "ok" : "FAILED");

	g_message ("version compare 1 is %s",
		   eazel_package_system_compare_version (packsys, "1.0", "1.1") < 0 ? "ok" : "FAILED");
	g_message ("version compare 2 is %s",
		   eazel_package_system_compare_version (packsys, "1.0", "1.0") == 0 ? "ok" : "FAILED");
	g_message ("version compare 3 is %s",
		   eazel_package_system_compare_version (packsys, "1.1", "1.0") > 0 ? "ok" : "FAILED");
}

static void
test_package_load (EazelPackageSystem *packsys,
		   const char *package_file_name) 
{
	PackageData *p;
	int flag;
	unsigned int provides_count = 0;

	flag = PACKAGE_FILL_EVERYTHING;
	p = eazel_package_system_load_package (packsys, NULL, package_file_name, flag);
	if (p->description && p->summary && p->provides && p->depends) {
		g_message ("load_package test 1 ok");
		provides_count = g_list_length (p->provides);
	} else {
		g_message ("load_package test 1 FAIL");
	}

	gtk_object_unref (GTK_OBJECT (p));

	flag = PACKAGE_FILL_NO_PROVIDES;
	p = eazel_package_system_load_package (packsys, NULL, package_file_name, flag);
	if (p->description && p->summary && p->provides==NULL) {
		g_message ("load_package test 2 ok");
	} else {
		g_message ("load_package test 2 FAIL");
	}
	gtk_object_unref (GTK_OBJECT (p));


	flag = PACKAGE_FILL_NO_DIRS_IN_PROVIDES;
	p = eazel_package_system_load_package (packsys, NULL, package_file_name, flag);
	if (p->description && p->summary && p->provides) {
		if (provides_count > g_list_length (p->provides)) {
			g_message ("load_package test 3 ok");
		} else {
			g_message ("load_package test 3 FAIL (%d in provides, should have less then %d)", 
				g_list_length (p->provides), provides_count);
		}
	} else {
		g_message ("load_package test 3 FAIL");
	}
	gtk_object_unref (GTK_OBJECT (p));
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
test_query (EazelPackageSystem *packsys) 
{
	GList *result;

	result = eazel_package_system_query (packsys,
					     NULL,
					     MATCHES_ONLY_ONE,
					     EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
					     0);
	if (g_list_length (result)==1) {
		g_message ("Query matches ok (1 hit on %s)", MATCHES_ONLY_ONE);
	} else {
		g_message ("Query matches fail (got %d, not 1 for %s)", 
			   g_list_length (result), MATCHES_ONLY_ONE);
	}
	g_list_foreach (result, (GFunc)gtk_object_unref, NULL);
	g_list_free (result);

	result = eazel_package_system_query (packsys,
					     NULL,
					     PROVIDED_BY_ONLY_ONE,
					     EAZEL_PACKAGE_SYSTEM_QUERY_PROVIDES,
					     0);
	if (g_list_length (result)==1) {
		g_message ("Query provides ok (1 hit for %s)", PROVIDED_BY_ONLY_ONE);
	} else {
		g_message ("Query provides fail (got %d, not 1 for %s)", 
			   g_list_length (result), PROVIDED_BY_ONLY_ONE);
	}
	g_list_foreach (result, (GFunc)gtk_object_unref, NULL);
	g_list_free (result);

	result = eazel_package_system_query (packsys,
					     NULL,
					     OWNED_BY_ONLY_ONE,
					     EAZEL_PACKAGE_SYSTEM_QUERY_OWNS,
					     0);
	if (g_list_length (result)==1) {
		g_message ("Query owned ok (1 hit for %s)", OWNED_BY_ONLY_ONE);
	} else {
		g_message ("Query owned fail (got %d, not 1 for %s)", 
			   g_list_length (result), OWNED_BY_ONLY_ONE);
	}
	g_list_foreach (result, (GFunc)gtk_object_unref, NULL);
	g_list_free (result);
	
	result = eazel_package_system_query (packsys,
					     NULL,
					     "",
					     EAZEL_PACKAGE_SYSTEM_QUERY_SUBSTR,
					     0);
	if (g_list_length (result)>10) {
		g_message ("Query substr ok (%d hits for \"\")", g_list_length (result));
	} else {
		g_message ("Query substr fail (%d hits, too few (<10) for \"\")", 
			   g_list_length (result));
	}
	g_list_foreach (result, (GFunc)gtk_object_unref, NULL);
	g_list_free (result);

	{		
		GList *glibc_result;

		glibc_result = eazel_package_system_query (packsys,
							   NULL,
							   NEEDED_BY_MANY,
							   EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
							   0);

		if (g_list_length (glibc_result) > 0) {
			PackageData *pack = (PackageData*)glibc_result->data;
			
			result = eazel_package_system_query (packsys,
							     NULL,
							     pack,
							     EAZEL_PACKAGE_SYSTEM_QUERY_REQUIRES,
							     0);
			if (g_list_length (result)>50) {
				g_message ("Query requries ok (%d hits for %s)", 
					   g_list_length (result), NEEDED_BY_MANY);
			} else {
				g_message ("Query requires fail (%d hits, too few (<50) for %s)",  
					   g_list_length (result), NEEDED_BY_MANY);
			}
			g_list_foreach (result, (GFunc)gtk_object_unref, NULL);
			g_list_free (result);
		} else {
			g_message ("Can't test query requires, no hits for %s", NEEDED_BY_MANY);
		}
		g_list_foreach (glibc_result, (GFunc)gtk_object_unref, NULL);
		g_list_free (glibc_result);
		
	}
}

static void
test_query_owns_mem (EazelPackageSystem *packsys) 
{
	GList *result;
	int i;

	g_message ("doing 1000 owns queries");
	for (i=1;i<=1000; i++) {
		result = eazel_package_system_query (packsys,
						     NULL,
						     OWNED_BY_ONLY_ONE,
						     EAZEL_PACKAGE_SYSTEM_QUERY_OWNS,
						     0);
		if (i%50==0) {
			g_message ("%d queries done...", i);
		}
		g_list_foreach (result, (GFunc)gtk_object_unref, NULL);
		g_list_free (result);
	}
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
		 const PackageData *pack,
		 unsigned long *info,
		 gboolean *signals)
{
	if (op==EAZEL_PACKAGE_SYSTEM_OPERATION_VERIFY) {
		/*
		g_message ("checking file \"%s\" (%ld/%ld %ld/%ld %ld/%ld)",
			   (char*)((g_list_nth (pack->provides, info[0]-1))->data),
			   info [0], info [1],
			   info [2], info [3],
			   info [4], info [5]);
		*/
	}
	signals[1] = TRUE;
	return TRUE;
}

static gboolean
failed_signal (EazelPackageSystem *system,
	       EazelPackageSystemOperation op, 
	       const PackageData *package,
	       gpointer unused)
{
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
	gtk_object_unref (GTK_OBJECT (package));
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
		g_message ("install FAIL (inconsistent)");
	} else if (signals[0] && signals[1] && signals[2]) {
		if (is_installed (packsys, dbpath, package_file_name)) {
			g_message ("install ok");
		} else {
			g_message ("install FAIL (package not installed)");
		}
	} else {
		g_message ("install FAIL");
	}
	g_list_foreach (packages, (GFunc)gtk_object_unref, NULL);

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
		g_message ("uninstall FAIL (inconsistent)");
	} else if (signals[0] && signals[2]) {
		if (is_installed (packsys, dbpath, package_file_name)) {
			g_message ("uninstall FAIL (package still installed)");
		} else {
			g_message ("uninstall ok");
		}
	} else {
		g_message ("uninstall FAIL");
	}
	g_list_foreach (packages, (GFunc)gtk_object_unref, NULL);

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
	gboolean *signals;
	guint h1, h2, h3, h4;

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
	h4 = gtk_signal_connect (GTK_OBJECT (packsys), 
				 "failed",
				 (GtkSignalFunc)failed_signal,
				 signals);

	eazel_package_system_verify (packsys,
				     dbpath,
				     packages);

	if (signals[3]) {
		g_message ("verified more then 1 file");
	} else if (signals[0] && signals[1] && signals[2]) {
		g_message ("verify ok");
	} else {
		g_message ("verify didn't emit enough signals");
	}
	g_list_foreach (packages, (GFunc)gtk_object_unref, NULL);

	gtk_signal_disconnect (GTK_OBJECT (packsys), h1);
	gtk_signal_disconnect (GTK_OBJECT (packsys), h2);
	gtk_signal_disconnect (GTK_OBJECT (packsys), h3);
	gtk_signal_disconnect (GTK_OBJECT (packsys), h4);
	g_free (signals);
}

/*******************************************************************************************/
int arg_debug;

static const struct poptOption options[] = {
	{"debug", '\0', POPT_ARG_INT, &arg_debug, 0 , N_("Show debug output"), NULL},
	{NULL, '\0', 0, NULL, 0}
};


int main(int argc, char *argv[]) {
	EazelPackageSystem *packsys;
	char *home_dbpath; 
	char *filename;
	poptContext ctxt;

	gnome_init_with_popt_table ("Eazel Test Packsys", "1.0", argc, argv, options, 0, &ctxt);
	home_dbpath = g_strdup_printf ("/tmp/packagedb");
	packsys = init_package_system (home_dbpath, g_strdup (g_get_home_dir ()));

	filename= poptGetArg (ctxt);
	if (filename==NULL) {
		g_error ("usage : %s [options (-h for help)] filename", argv[1]);
	}

	eazel_package_system_set_debug (packsys, arg_debug);

	test_database_mtime (packsys);
	test_is_installed (packsys);
	test_version_compare (packsys);
	test_package_load (packsys, filename);
	test_query (packsys);
	test_query_owns_mem (packsys);

	test_install (packsys, home_dbpath, filename);
	test_verify (packsys, home_dbpath, filename);
	test_uninstall (packsys, home_dbpath, filename);

	return 0;
};
