/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2001 Eazel, Inc
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
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include <sys/utsname.h>

#include <eazel-package-system-types.h>
#include <eazel-install-corba-types.h>
#include <eazel-install-corba-callback.h>
#include <eazel-install-problem.h>

#include <libtrilobite/libtrilobite.h>
#include <trilobite-eazel-install.h>
#include "eazel-clone-lib.h"
#include <unistd.h>

CORBA_ORB orb;
CORBA_Environment ev;
int cli_result = 0;

/* Popt stuff */
int     arg_dry_run = 0,
	arg_debug = 0,
	arg_no_auth = 0,
	arg_dump = 0,
	arg_downgrade = 0;

char    *arg_server = NULL,
	*arg_cgi = NULL,
	*arg_file = NULL, 
	*arg_root = NULL,
	*arg_username = NULL;

static const struct poptOption options[] = {
	{"cgi-path", '\0', POPT_ARG_STRING, &arg_cgi, 0, N_("Specify search cgi"), NULL},
	{"debug", '\0', POPT_ARG_NONE, &arg_debug, 0 , N_("Show debug output"), NULL},
	{"file",'\0', POPT_ARG_STRING, &arg_file, 0, N_("Inventory xml to clone from (if omitted, file will be read from standard in)"), NULL},
	{"no-auth", '\0', POPT_ARG_NONE, &arg_no_auth, 0, N_("don't use eazel auth stuff"), NULL},
	{"root", '\0', POPT_ARG_STRING, &arg_root, 0, N_("Set root"), NULL},
	{"server", '\0', POPT_ARG_STRING, &arg_server, 0, N_("Specify server"), NULL},
	{"test", 't', POPT_ARG_NONE, &arg_dry_run, 0, N_("Test run"), NULL},
	{"username", '\0', POPT_ARG_STRING, &arg_username, 0, N_("Allow username"), NULL},
	{"dump", '\0', POPT_ARG_NONE, &arg_dump, 0, N_("dump inventory"), NULL},
	{NULL, '\0', 0, NULL, 0}
};

#define check_ev(s)                                                \
if (ev._major!=CORBA_NO_EXCEPTION) {                               \
	fprintf (stderr, "*** %s: Caught exception %s",            \
                 s, CORBA_exception_id (&ev));                     \
}

static void
set_parameters_from_command_line (GNOME_Trilobite_Eazel_Install service)
{
	if (arg_debug) {		
		GNOME_Trilobite_Eazel_Install__set_debug (service, TRUE, &ev);
		check_ev ("set_debug");
	}

	GNOME_Trilobite_Eazel_Install__set_protocol (service, GNOME_Trilobite_Eazel_PROTOCOL_HTTP, &ev);
	check_ev ("set_protocol");

	if (arg_downgrade) {
		GNOME_Trilobite_Eazel_Install__set_downgrade (service, TRUE, &ev);
		check_ev ("downgrade");
	}
	if (arg_server) {
		char *colon = strchr (arg_server, ':');
		if (colon) {
			char *host;
			int port;

			host = g_new0(char, (colon - arg_server) + 1);
			strncpy (host, arg_server, colon - arg_server);
			colon++;
			port = atoi (colon);
			GNOME_Trilobite_Eazel_Install__set_server (service, host, &ev);
			check_ev ("set_server");
			GNOME_Trilobite_Eazel_Install__set_server_port (service, port, &ev);
			check_ev ("set_port");
			g_free (host);
		} else {
			GNOME_Trilobite_Eazel_Install__set_server (service, arg_server, &ev);
			check_ev ("set_server");
		}
		GNOME_Trilobite_Eazel_Install__set_auth (service, FALSE, &ev);
		check_ev ("set_auth");
	} else if (arg_no_auth==0) {
		char *host, *p;
		int port;

		host = g_strdup (trilobite_get_services_address ());
		if ((p = strchr (host, ':')) != NULL) {
			*p = 0;
			port = atoi (p+1);
		} else {
			port = 80;
		}
		GNOME_Trilobite_Eazel_Install__set_auth (service, TRUE, &ev);
		check_ev ("set_auth");
		GNOME_Trilobite_Eazel_Install__set_server (service, host, &ev);
		check_ev ("set_server");
		GNOME_Trilobite_Eazel_Install__set_server_port (service, port, &ev);
		check_ev ("set_port");
		g_free (host);
	}

	if (arg_username) {
		GNOME_Trilobite_Eazel_Install__set_username (service, arg_username, &ev);
		check_ev ("set_username");
	}

	if (arg_cgi) {
		GNOME_Trilobite_Eazel_Install__set_cgi (service, arg_cgi, &ev);
		check_ev ("set_cgi");
	}

	if (arg_no_auth) {
		GNOME_Trilobite_Eazel_Install__set_auth (service, FALSE, &ev);
		check_ev ("set_auth");
	} 

	if (arg_dry_run) {
		GNOME_Trilobite_Eazel_Install__set_test_mode (service, TRUE, &ev);
	}

	if (arg_root) {
		if (arg_root[0]=='~') {
			char *tmp = g_strdup_printf ("%s/%s", g_get_home_dir (), 
						     arg_root+1);
			free (arg_root);
			arg_root = strdup (tmp);
			g_free (tmp);
		} else if (arg_root[0]!='/' || arg_root[0]=='.') {
			char *tmp = g_strdup_printf ("%s%s%s", g_get_current_dir (), 
						     arg_root[0]=='.' ? "" : "/",
						     arg_root+1);
			free (arg_root);
			arg_root = strdup (tmp);
			g_free (tmp);
		}
	}
}

static void 
eazel_file_conflict_check_signal (EazelInstallCallback *service, 
				  const PackageData *pack,
				  gpointer unused)
{
	char *tmp = packagedata_get_readable_name (pack);
	printf ("FILE CONFLICT CHECKING \"%s\"\n", tmp);
	fflush (stdout);
	g_free (tmp);
}

static void 
eazel_file_uniqueness_check_signal (EazelInstallCallback *service, 
				    const PackageData *pack,
				    gpointer unused)
{
	char *tmp = packagedata_get_readable_name (pack);
	fprintf (stdout, "FILE UNIQUENESS CHECKING \"%s\"\n", tmp);
	fflush (stdout);
	g_free (tmp);
}

static void 
eazel_feature_consistency_check_signal (EazelInstallCallback *service, 
					const PackageData *pack,
					gpointer unused)
{
	char *tmp = packagedata_get_readable_name (pack);
	printf ("FEATURE CONSISTENCY CHECKING \"%s\"\n", tmp);
	fflush (stdout);
	g_free (tmp);
}

static void 
eazel_download_progress_signal (EazelInstallCallback *service, 
				const PackageData *pack,
				int amount, 
				int total,
				gpointer unused) 
{
	char *tmp = packagedata_get_readable_name (pack);
	printf ("DOWNLOADING \"%s\" %d %d", tmp, amount, total);
	fflush (stdout);
	g_free (tmp);
}

static void 
eazel_install_progress_signal (EazelInstallCallback *service, 
			       const PackageData *package,
			       int package_num, int num_packages, 
			       int amount, int total,
			       int total_size_completed, int total_size, 
			       gpointer unused)
{
	char *tmp = packagedata_get_readable_name (package);
	fprintf (stdout, "INSTALLING \"%s\" %d %d %d %d %d %d",
		 tmp,
		 package_num, num_packages,
		 amount, total,
		 total_size_completed, total_size);
	fflush (stdout);
	g_free (tmp);
}

static void 
eazel_uninstall_progress_signal (EazelInstallCallback *service, 
				 const PackageData *package,
				 int package_num, int num_packages, 
				 int amount, int total,
				 int total_size_completed, int total_size, 
				 gpointer unused)
{
	char *tmp = packagedata_get_readable_name (package);
	fprintf (stdout, "UNINSTALLING \"%s\" %d %d %d %d %d %d",
		 tmp,
		 package_num, num_packages,
		 amount, total,
		 total_size_completed, total_size);
	fflush (stdout);
	g_free (tmp);
}

static void download_failed (EazelInstallCallback *service, 
		 const PackageData *package,
		 gpointer unused)
{
	char *tmp = packagedata_get_readable_name (package);
	fprintf (stdout, "DOWNLOAD FAILED \"%s\"\n", tmp);
	fflush (stdout);
	g_free (tmp);
}

/*
  This dumps the entire tree for the failed package.
 */
static void
install_failed (EazelInstallCallback *service,
		PackageData *package,
		EazelInstallProblem *problem)
{
	char *tmp = packagedata_get_readable_name (package);
	fprintf (stdout, "INSTALL FAILED \"%s\"\n", tmp);
	fflush (stdout);
	g_free (tmp);	
}

static void
uninstall_failed (EazelInstallCallback *service,
		  PackageData *package,		
		  EazelInstallProblem *problem)
{
	char *tmp = packagedata_get_readable_name (package);
	fprintf (stdout, "UNINSTALL FAILED \"%s\"\n", tmp);
	fflush (stdout);
	g_free (tmp);
}

static gboolean
eazel_preflight_check_signal (EazelInstallCallback *service, 
			      EazelInstallCallbackOperation op,
			      const GList *packages,
			      int total_bytes,
			      int total_packages,
			      gpointer unused) 
{	
	switch (op) {
	case EazelInstallCallbackOperation_INSTALL:
		fprintf (stdout, "PREPARING INSTALL %d %d\n", 
			 total_packages, total_bytes);
		break;
	case EazelInstallCallbackOperation_UNINSTALL:
		fprintf (stdout, "PREPARING UNINSTALL %d %d\n", 
			 total_packages, total_bytes);
		break;
	case EazelInstallCallbackOperation_REVERT:
		fprintf (stdout, "PREPARING REVERT %d %d\n", 
			 total_packages, total_bytes);
		break;
	}

	return TRUE;
}

static gboolean
eazel_save_transaction_signal (EazelInstallCallback *service, 
			       EazelInstallCallbackOperation op,
			       const GList *packages,
			       gpointer unused) 
{	
	return FALSE;
}

static void
dep_check (EazelInstallCallback *service,
	   const PackageData *package,
	   const PackageData *needs_package,
	   gpointer unused) 
{
	char *pack, *needs;
	pack = packagedata_get_readable_name (package);
	needs = packagedata_get_readable_name (needs_package);
	fprintf (stdout, "DEPENDENCY : \"%s\" \"%s\"\n", pack, needs);
	fflush (stdout);
	g_free (pack);
	g_free (needs);
}

static void
md5_check_failed (EazelInstallCallback *service,
		  const PackageData *package,
		  const char *actual_md5,
		  gpointer unused) 
{
	char *tmp = packagedata_get_readable_name (package);
	fprintf (stdout, "MD5 FAILURE \"%s\"", tmp);
	fflush (stdout);
	g_free (tmp);
}


static void
delete_files (EazelInstallCallback *service)
{
	eazel_install_callback_delete_files (service, &ev);			
}

static void
done (EazelInstallCallback *service,
      gboolean result,
      EazelInstallProblem *problem)
{
	if (result) {
		fprintf (stdout, "DONE: OK");
	} else {
		fprintf (stdout, "DONE: FAILED");
	}
	fflush (stdout);
}

static char *
get_password_dude (TrilobiteRootClient *root_client, const char *prompt, void *user_data)
{
	char * passwd;

	passwd = getpass ("ROOT PASSWORD: ");
	return g_strdup (passwd);
}

static TrilobiteRootClient *
set_root_client (BonoboObjectClient *service)
{
	TrilobiteRootClient *root_client;

	if (bonobo_object_client_has_interface (service, "IDL:Trilobite/PasswordQuery:1.0", &ev)) {
		root_client = trilobite_root_client_new ();
		gtk_signal_connect (GTK_OBJECT (root_client), "need_password", GTK_SIGNAL_FUNC (get_password_dude),
				    NULL);

		if (! trilobite_root_client_attach (root_client, service)) {
			fprintf (stderr, "*** unable to attach root client to Trilobite/PasswordQuery!");
		}

		return root_client;
	} else {
		fprintf (stderr, "*** Object does not support IDL:Trilobite/PasswordQuery:1.0");
		return NULL;
	}
}

static void
do_clone (EazelInstallCallback *cb,
	  EazelInstallProblem *problem) 
{
	GList *inventory;
	GList *install = NULL;
	GList *upgrade = NULL;
	GList *downgrade = NULL;

	fprintf (stderr, "D: do clone\n");

	inventory = eazel_install_clone_load_inventory (arg_file);
	fprintf (stderr, "D: do clone %d packages\n", g_list_length (inventory));
	if (inventory == NULL) {
		fprintf (stderr, "D: no inventory loaded");
		cli_result = 1;
	} else {
		CategoryData *cat = categorydata_new ();
		GList *categories = NULL;
		GList *iterator;

		cat->name = g_strdup ("clone");

		eazel_install_clone_compare_inventory (inventory,
						       &install,
						       &upgrade,
						       &downgrade);

		fprintf (stderr, "D: %d installs, %d upgrade, %d downgrades\n",
			 g_list_length (install),
			 g_list_length (upgrade),
			 g_list_length (downgrade));

		for (iterator = install; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack = PACKAGEDATA (iterator->data);
			fprintf (stderr, "D: installing %s\n", packagedata_get_readable_name (pack));
			cat->packages = g_list_prepend (cat->packages, pack);
		}
		for (iterator = upgrade; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack = PACKAGEDATA (iterator->data);
			fprintf (stderr, "D: upgrading %s\n", packagedata_get_readable_name (pack));
			cat->packages = g_list_prepend (cat->packages, pack);
		}
		for (iterator = downgrade; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack = PACKAGEDATA (iterator->data);
			fprintf (stderr, "D: downgrading %s\n", packagedata_get_readable_name (pack));
			cat->packages = g_list_prepend (cat->packages, pack);
		}
		categories = g_list_prepend (categories, cat);

		eazel_install_callback_install_packages (cb, categories, NULL, &ev);
	}
}

int main(int argc, char *argv[]) {
	poptContext ctxt;
	EazelInstallCallback *cb;		
	EazelInstallProblem *problem = NULL;

	CORBA_exception_init (&ev);

	/* Seems that bonobo_main doens't like
	   not having gnome_init called, dies in a
	   X call, yech */

#ifdef ENABLE_NLS /* sadly we need this ifdef because otherwise the following get empty statement warnings */
	bindtextdomain (PACKAGE, GNOMELOCALEDIR);
	textdomain (PACKAGE);
#endif

#if 0
	gnomelib_register_popt_table (oaf_popt_options, oaf_get_popt_table_name ());
	orb = oaf_init (argc, argv);
	gnome_init_with_popt_table ("Eazel Install", "1.0", argc, argv, options, 0, &ctxt);
	if (!bonobo_init (NULL, NULL, NULL)) {
		g_error ("Could not init bonobo");
	}
#else
	trilobite_init ("Eazel Install", "1.0", NULL, options, argc, argv);
	ctxt = trilobite_get_popt_context ();
#endif

	bonobo_activate ();

	cb = eazel_install_callback_new ();
	problem = eazel_install_problem_new (); 
	gtk_object_ref (GTK_OBJECT (problem));

	set_parameters_from_command_line (eazel_install_callback_corba_objref (cb));
	set_root_client (eazel_install_callback_bonobo (cb));
	
	/* Set up signal connections */
	gtk_signal_connect (GTK_OBJECT (cb), "file_conflict_check", 
			    GTK_SIGNAL_FUNC (eazel_file_conflict_check_signal), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "file_uniqueness_check", 
			    GTK_SIGNAL_FUNC (eazel_file_uniqueness_check_signal), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "feature_consistency_check", 
			    GTK_SIGNAL_FUNC (eazel_feature_consistency_check_signal), 
			    NULL);

	gtk_signal_connect (GTK_OBJECT (cb), "download_progress", 
			    GTK_SIGNAL_FUNC (eazel_download_progress_signal), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "preflight_check", 
			    GTK_SIGNAL_FUNC (eazel_preflight_check_signal), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "save_transaction", 
			    GTK_SIGNAL_FUNC (eazel_save_transaction_signal), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "install_progress", 
			    GTK_SIGNAL_FUNC (eazel_install_progress_signal), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "md5_check_failed", 
			    GTK_SIGNAL_FUNC (md5_check_failed), 
			    "");
	gtk_signal_connect (GTK_OBJECT (cb), "install_failed", 
			    GTK_SIGNAL_FUNC (install_failed), 
			    problem);
	gtk_signal_connect (GTK_OBJECT (cb), "uninstall_progress", 
			    GTK_SIGNAL_FUNC (eazel_uninstall_progress_signal), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "uninstall_failed", 
			    GTK_SIGNAL_FUNC (uninstall_failed), 
			    problem);
	gtk_signal_connect (GTK_OBJECT (cb), "download_failed", 
			    GTK_SIGNAL_FUNC (download_failed), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "dependency_check", 
			    GTK_SIGNAL_FUNC (dep_check), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "done", 
			    GTK_SIGNAL_FUNC (done), 
			    problem);

	if (arg_dump) {
		char *mem;
		long size;
		eazel_install_clone_create_inventory (&mem, &size);
		fprintf (stdout, mem);
	} else {
		do_clone (cb, problem);
		if (!cli_result) {
			trilobite_main ();
		}
		delete_files (cb);
	}

	eazel_install_callback_unref (GTK_OBJECT (cb));
	gtk_object_unref (GTK_OBJECT (problem));

	/* Corba cleanup */
	CORBA_exception_free (&ev);

	return cli_result;
};
