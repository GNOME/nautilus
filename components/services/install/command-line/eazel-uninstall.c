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
#include <liboaf/liboaf.h>
#include <bonobo.h>

#include <eazel-install-types.h>
#include <eazel-install-corba-types.h>
#include <eazel-install-corba-callback.h>
#include <trilobite-eazel-install.h>

#include <libtrilobite/libtrilobite.h>

#define PACKAGE_FILE_NAME "package-list.xml"
#define DEFAULT_CONFIG_FILE "/var/eazel/services/eazel-services-config.xml"

#define DEFAULT_HOSTNAME "testmachine.eazel.com"
#define DEFAULT_PORT_NUMBER 80
#define DEFAULT_PROTOCOL PROTOCOL_HTTP
#define DEFAULT_TMP_DIR "/tmp/eazel-install"
#define DEFAULT_RPMRC "/usr/lib/rpm/rpmrc"
#define DEFAULT_REMOTE_PACKAGE_LIST "/package-list.xml"
#define DEFAULT_REMOTE_RPM_DIR "/RPMS"
#define DEFAULT_LOG_FILE "/tmp/eazel-install/ulog"

#define OAF_ID "OAFIID:trilobite_eazel_install_service:8ff6e815-1992-437c-9771-d932db3b4a17"

/* Popt stuff */
int     arg_dry_run,
	arg_http,
	arg_ftp,
	arg_local,
	arg_debug,
	arg_port,
	arg_delay;
char    *arg_server,
	*arg_config_file,
	*arg_local_list, 
	*arg_tmp_dir,
	*arg_input_list;

CORBA_ORB orb;
CORBA_Environment ev;

static const struct poptOption options[] = {
	{"debug", '\0', POPT_ARG_NONE, &arg_debug, 0 , N_("Show debug output"), NULL},
	{"test", 't', POPT_ARG_NONE, &arg_dry_run, 0, N_("Test run"), NULL},
	{"delay", '\0', POPT_ARG_NONE, &arg_delay, 0 , N_("10 sec delay after starting service"), NULL},
	{"config", '\0', POPT_ARG_STRING, &arg_config_file, 0, N_("Specify config file (/var/eazel/services/eazel-services-config.xml)"), NULL},
	{NULL, '\0', 0, NULL, 0}
};

#define check_ev(s) if (ev._major!=CORBA_NO_EXCEPTION) { g_warning ("%s: Caught exception %s", s, CORBA_exception_id (&ev)); }

static void
set_parameters_from_command_line (Trilobite_Eazel_Install service)
{
	if (!arg_debug) {
		Trilobite_Eazel_Install__set_log_file (service, DEFAULT_LOG_FILE, &ev);
		check_ev ("set_log_file");
	}
	if (arg_tmp_dir == NULL) {
		arg_tmp_dir = g_strdup (DEFAULT_TMP_DIR);
	}
	if (arg_dry_run) {
		Trilobite_Eazel_Install__set_test_mode (service, TRUE, &ev);
		check_ev ("set_test_mode");
	}

	Trilobite_Eazel_Install__set_tmp_dir (service, arg_tmp_dir, &ev);
	check_ev ("set_tmp_dir");
}

static void 
progress_signal (EazelInstallCallback *service, 
				const PackageData *pack,
				int amount, 
				int total,
				char *title) 
{
	fprintf (stdout, "%s - %s %% %f\r", title, pack->name,
		 (total ? ((float)
			   ((((float) amount) / total) * 100))
		  : 100.0));
	fflush (stdout);
	if (amount == total && total!=0) {
		fprintf (stdout, "\n");
	}
}

/*
  This dumps the entire tree for the failed package.
 */
static void
uninstall_failed (EazelInstallCallback *service,
		  const PackageData *pd,
		  gchar *indent)
{
	GList *iterator;

	if (pd->toplevel) {
		fprintf (stdout, "\n***The package %s failed. Here's the dep tree\n", pd->name);
	}
	switch (pd->status) {
	case PACKAGE_DEPENDENCY_FAIL:
		fprintf (stdout, "%s- %s FAILED\n", indent, rpmfilename_from_packagedata (pd));
		break;
	case PACKAGE_CANNOT_OPEN:
		fprintf (stdout, "%s- %s NOT FOUND\n", indent, rpmfilename_from_packagedata (pd));
		break;		
	case PACKAGE_SOURCE_NOT_SUPPORTED:
		fprintf (stdout, "%s- %s is a source package\n", indent, rpmfilename_from_packagedata (pd));
		break;
	case PACKAGE_BREAKS_DEPENDENCY:
		fprintf (stdout, "%s- %s breaks (%dd  %db)\n", indent, rpmfilename_from_packagedata (pd),
			 g_list_length (pd->soft_depends), g_list_length (pd->breaks));
		break;
	default:
		fprintf (stdout, "%s- %s (%d deps and %d breaks)\n", indent, rpmfilename_from_packagedata (pd),
			 g_list_length (pd->soft_depends), g_list_length (pd->breaks));
		break;
	}
	for (iterator = pd->soft_depends; iterator; iterator = iterator->next) {			
		PackageData *pack;
		char *indent2;
		indent2 = g_strconcat (indent, iterator->next ? " |" : "  " , NULL);
		pack = (PackageData*)iterator->data;
		uninstall_failed (service, pack, indent2);
		g_free (indent2);
	}
	for (iterator = pd->breaks; iterator; iterator = iterator->next) {			
		PackageData *pack;
		char *indent2;
		indent2 = g_strconcat (indent, iterator->next ? " |" : "  " , NULL);
		pack = (PackageData*)iterator->data;
		uninstall_failed (service, pack, indent2);
		g_free (indent2);
	}
}

static void
dep_check (EazelInstallCallback *service,
	   const PackageData *package,
	   const PackageData *needs,
	   gpointer unused) 
{
	fprintf (stdout, "Doing dependency check for %s - need %s\n", package->name, needs->name);
}

static PackageData*
create_package (char *name) 
{
	PackageData *pack;

	pack = packagedata_new ();
	pack->name = g_strdup (name);
	pack->toplevel = TRUE;
	
	return pack;
}

static void
done (EazelInstallCallback *service,
      gpointer unused)
{
	fprintf (stderr, "\nDone\n");
	gtk_main_quit ();
}

static char *
get_password_dude (TrilobiteRootClient *root_client, const char *prompt, void *user_data)
{
	char * real_prompt;
	char * passwd;

	real_prompt = g_strdup_printf ("%s: ", prompt);
	passwd = getpass (real_prompt);
	g_free (real_prompt);

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
			g_warning ("unable to attach root client to Trilobite/PasswordQuery!");
		}

		return root_client;
	} else {
		g_warning ("Object does not support IDL:Trilobite/PasswordQuery:1.0");
		return NULL;
	}
}

int main(int argc, char *argv[]) {
	poptContext ctxt;
	GList *packages;
	GList *categories;
	char *str;
	EazelInstallCallback *cb;		

	CORBA_exception_init (&ev);

	/* Seems that bonobo_main doens't like
	   not having gnome_init called, dies in a
	   X call, yech */
#if 1
	gnome_init_with_popt_table ("Eazel Uninstall", "1.0", argc, argv, options, 0, &ctxt);
#else
	gtk_type_init ();
	gnomelib_init ("Eazel Uninstall", "1.0");
	gnomelib_register_popt_table (options, "Eazel Uninstall");
	ctxt = gnomelib_parse_args (argc, argv, 0);
#endif
	
	packages = NULL;
	categories = NULL;
	/* If there are more args, get them and parse them as packages */
	while ((str = poptGetArg (ctxt)) != NULL) {
		packages = g_list_prepend (packages, create_package (str));
	}
	if (packages) {
		CategoryData *category;
		category = g_new0 (CategoryData, 1);
		category->packages = packages;
		categories = g_list_prepend (NULL, category);		
	} else {
		g_message ("Using remote list ");
	}

	/* Chech that we're root and on a redhat system */
	if (check_for_root_user() == FALSE) {
		fprintf (stderr, "*** This tool requires root access.\n");
	}
	if (check_for_redhat () == FALSE) {
		fprintf (stderr, "*** This tool can only be used on RedHat.\n");
	}

	orb = oaf_init (argc, argv);
	
	if (bonobo_init (NULL, NULL, NULL) == FALSE) {
		g_error ("Could not init bonobo");
	}
	bonobo_activate ();

	cb = eazel_install_callback_new ();

	if (arg_delay) {
		sleep (10);
	}

	set_parameters_from_command_line (eazel_install_callback_corba_objref (cb));
	set_root_client (eazel_install_callback_bonobo (cb));

	gtk_signal_connect (GTK_OBJECT (cb), "uninstall_progress", progress_signal, "Unnstall progress");
	gtk_signal_connect (GTK_OBJECT (cb), "uninstall_failed", uninstall_failed, "");
	gtk_signal_connect (GTK_OBJECT (cb), "dependency_check", dep_check, NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "done", done, NULL);
	
	eazel_install_callback_uninstall_packages (cb, categories, &ev);
	
	fprintf (stdout, "\nEntering main loop...\n");
	bonobo_main ();

	/* Corba cleanup */
	eazel_install_callback_destroy (GTK_OBJECT (cb));
	CORBA_exception_free (&ev);
       
	return 0;
};
