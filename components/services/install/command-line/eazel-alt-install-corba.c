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
#include <sys/utsname.h>

#include <eazel-install-types.h>
#include <eazel-install-corba-types.h>
#include <eazel-install-corba-callback.h>

#include <libtrilobite/libtrilobite.h>
#include <trilobite-eazel-install.h>

#include <unistd.h>

#define PACKAGE_FILE_NAME "package-list.xml"
#define DEFAULT_CONFIG_FILE "/var/eazel/services/eazel-services-config.xml"

#define DEFAULT_HOSTNAME "testmachine.eazel.com"
#define DEFAULT_PORT_NUMBER 80
#define DEFAULT_PROTOCOL PROTOCOL_HTTP
#define DEFAULT_TMP_DIR "/tmp/eazel-install"
#define DEFAULT_RPMRC "/usr/lib/rpm/rpmrc"
#define DEFAULT_REMOTE_PACKAGE_LIST "/package-list.xml"
#define DEFAULT_REMOTE_RPM_DIR "/RPMS"
#define DEFAULT_LOG_FILE "/tmp/eazel-install/log"

/* This ensure that if the arch is detected as i[3-9]86, the
   requested archtype will be set to i386 */
#define ASSUME_ix86_IS_i386 

#define OAF_ID "OAFIID:trilobite_eazel_install_service:8ff6e815-1992-437c-9771-d932db3b4a17"

/* Popt stuff */
int     arg_dry_run,
	arg_http,
	arg_ftp,
	arg_local,
	arg_debug,
	arg_port,
	arg_delay,
	arg_file,
	arg_force,
	arg_upgrade,
	arg_downgrade,
	arg_erase,
	arg_query;
char    *arg_server,
	*arg_config_file,
	*arg_package_list,
	*arg_tmp_dir;

CORBA_ORB orb;
CORBA_Environment ev;

static const struct poptOption options[] = {
	{"config", '\0', POPT_ARG_STRING, &arg_config_file, 0, N_("Specify config file (/var/eazel/services/eazel-services-config.xml)"), NULL},	
	{"debug", '0', POPT_ARG_NONE, &arg_debug, 0 , N_("Show debug output"), NULL},
	{"delay", '\0', POPT_ARG_NONE, &arg_delay, 0 , N_("10 sec delay after starting service"), NULL},
	{"downgrade", 'd', POPT_ARG_NONE, &arg_downgrade, 0, N_("Allow downgrades"), NULL},
	{"erase", 'e', POPT_ARG_NONE, &arg_erase, 0, N_("Erase packages"), NULL},
	{"file",'\0', POPT_ARG_NONE, &arg_file, 0, N_("RPM args are filename"), NULL},
	{"force", 'F', POPT_ARG_NONE, &arg_force, 0, N_("Force install"), NULL},
	{"ftp", 'f', POPT_ARG_NONE, &arg_ftp, 0, N_("Use ftp"), NULL},
	{"local", 'l', POPT_ARG_NONE, &arg_local, 0, N_("Use local"), NULL},
	{"http", 'h', POPT_ARG_NONE, &arg_http, 0, N_("Use http"), NULL},
	{"packagefile", '\0', POPT_ARG_STRING, &arg_package_list, 0, N_("Specify package file"), NULL},
	{"port", '\0', POPT_ARG_INT, &arg_port, 0 , N_("Set port numer (80)"), NULL},
	{"query", 'q', POPT_ARG_NONE, &arg_query, 0, N_("Run Query"), NULL},
	{"server", '\0', POPT_ARG_STRING, &arg_server, 0, N_("Specify server"), NULL},
	{"test", 't', POPT_ARG_NONE, &arg_dry_run, 0, N_("Test run"), NULL},
	{"tmp", '\0', POPT_ARG_STRING, &arg_tmp_dir, 0, N_("Set tmp dir (/tmp/eazel-install)"), NULL},
	{"upgrade", 'u', POPT_ARG_NONE, &arg_upgrade, 0, N_("Allow upgrades"), NULL},
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

	/* We only want 1 protocol type */
	if (arg_http + arg_ftp + arg_local > 1) {
			fprintf (stderr, "*** You cannot specify more then one protocol type.\n");
			exit (1);
	}
	if (arg_http) {
		Trilobite_Eazel_Install__set_protocol (service, Trilobite_Eazel_PROTOCOL_HTTP, &ev);
		check_ev ("set_protocol");
	} else if (arg_ftp) {
		Trilobite_Eazel_Install__set_protocol (service, Trilobite_Eazel_PROTOCOL_FTP, &ev);
		check_ev ("set_protocol");
	} else if (arg_local) {
		Trilobite_Eazel_Install__set_protocol (service, Trilobite_Eazel_PROTOCOL_LOCAL, &ev);
		check_ev ("set_protocol");
	} else {
		Trilobite_Eazel_Install__set_protocol (service, Trilobite_Eazel_PROTOCOL_HTTP, &ev);
		check_ev ("set_protocol");
	}
	if (arg_downgrade + arg_upgrade + arg_erase > 1) {
			fprintf (stderr, "*** Upgrade and erase ? Yeah rite....\n");
			exit (1);
	}
	if (arg_upgrade) {
		Trilobite_Eazel_Install__set_update (service, TRUE, &ev);
		check_ev ("update");
	}
	if (arg_downgrade) {
		Trilobite_Eazel_Install__set_downgrade (service, TRUE, &ev);
		check_ev ("downgrade");
	}
	if (arg_server == NULL) {
		arg_server = g_strdup (DEFAULT_HOSTNAME);
	}
	if (arg_tmp_dir == NULL) {
		arg_tmp_dir = g_strdup (DEFAULT_TMP_DIR);
	}
	if (arg_port == 0) {
		arg_port = DEFAULT_PORT_NUMBER;
	}
	if (arg_dry_run) {
		Trilobite_Eazel_Install__set_test_mode (service, TRUE, &ev);
	}
	if (arg_force) {
		Trilobite_Eazel_Install__set_force (service, TRUE, &ev);
	}
	if (arg_package_list) {
		Trilobite_Eazel_Install__set_package_list (service, arg_package_list, &ev);
		check_ev ("packagelist");
	}
/*

	Trilobite_Eazel_Install__set_rpmrc_file (service, DEFAULT_RPMRC, &ev);
	Trilobite_Eazel_Install__set_package_list_storage_path (service, DEFAULT_REMOTE_PACKAGE_LIST, &ev);
	Trilobite_Eazel_Install__set_rpm_storage_path (service, DEFAULT_REMOTE_RPM_DIR, &ev);
*/
	Trilobite_Eazel_Install__set_tmp_dir (service, arg_tmp_dir, &ev);
	check_ev ("set_tmp_dir");
	Trilobite_Eazel_Install__set_server (service, arg_server, &ev);
	check_ev ("set_server");
	Trilobite_Eazel_Install__set_server_port (service, arg_port, &ev);
	check_ev ("set_server_port");
}

static void 
eazel_preflight_check_signal (EazelInstallCallback *service, 
			      int total_bytes,
			      int total_packages,
			      gpointer unused) 
{	
	fprintf (stdout, "About to %s a total of %d packages, %dKb\n", 
		 arg_erase ? "uninstall" : "install",
		 total_packages, total_bytes/1024);
}

static void 
eazel_download_progress_signal (EazelInstallCallback *service, 
				const char *name,
				int amount, 
				int total,
				char *title) 
{
	if (amount==0) {
		fprintf (stdout, "Downloading %s...\n", name);
	} else if (amount != total ) {
		fprintf (stdout, "(%d/%d) %% %f\r", 
			 amount, total,
			 (float) (((float) amount * 100.0) / total));
		fflush (stdout);
	} else if (amount == total && total!=0) {
		fprintf (stdout, "(%d/%d) %% %f\r",
			 amount, total, 100.0);
		fprintf (stdout, "\nDone\n");
		fflush (stdout);
	}
}

static void 
eazel_install_progress_signal (EazelInstallCallback *service, 
			       const PackageData *pack,
			       int package_num, int num_packages, 
			       int amount, int total,
			       int total_size_completed, int total_size, 
			       char *title)
{
	if (amount==0) {
		fprintf (stdout, "%s %s: \"%20.20s\"...\n", title, pack->name, pack->summary);
	} else if (amount != total ) {
		fprintf (stdout, "(%d/%d), (%d/%d)b - (%d/%d) %% %f\r", 
			 package_num, num_packages,
			 total_size_completed, total_size,
			 amount, total,
			 (float) (((float) amount * 100.0) / total));
		fflush (stdout);
	}
	if (amount == total && total!=0) {
		fprintf (stdout, "(%d/%d), (%d/%d)b - (%d/%d) %% %f\r",
			 package_num, num_packages,
			 total_size_completed, total_size,
			 amount, total, 100.0);
		fprintf (stdout, "\nDone\n");
		fflush (stdout);
	}
}

static void
download_failed (EazelInstallCallback *service, 
		 const char *name,
		 gpointer unused)
{
	fprintf (stdout, "Download of %s FAILED\n", name);
}

/*
  This dumps the entire tree for the failed package.
 */
static void
install_failed (EazelInstallCallback *service,
		const PackageData *pd,
		gchar *indent)
{
	GList *iterator;

	if (pd->toplevel) {
		fprintf (stdout, "\n***The package %s failed. Here's the dep tree\n", pd->name);
	}
	switch (pd->status) {
	case PACKAGE_DEPENDENCY_FAIL:
		fprintf (stdout, "%s%s, which FAILED\n", indent, rpmfilename_from_packagedata (pd));
		break;
	case PACKAGE_CANNOT_OPEN:
		fprintf (stdout, "%s%s,which was NOT FOUND\n", indent, rpmfilename_from_packagedata (pd));
		break;		
	case PACKAGE_SOURCE_NOT_SUPPORTED:
		fprintf (stdout, "%s%s, which is a source package\n", indent, rpmfilename_from_packagedata (pd));
		break;
	case PACKAGE_BREAKS_DEPENDENCY:
		fprintf (stdout, "%s%s, which breaks deps\n", indent, rpmfilename_from_packagedata (pd));
		break;
	default:
		fprintf (stdout, "%s%s\n", indent, rpmfilename_from_packagedata (pd));
		break;
	}
	for (iterator = pd->soft_depends; iterator; iterator = iterator->next) {			
		PackageData *pack;
		char *indent2;
		indent2 = g_strconcat (indent, (iterator->next || pd->breaks) ? " |-d- " : " +-d- " , NULL);
		pack = (PackageData*)iterator->data;
		install_failed (service, pack, indent2);
		g_free (indent2);
	}
	for (iterator = pd->breaks; iterator; iterator = iterator->next) {			
		PackageData *pack;
		char *indent2;
		indent2 = g_strconcat (indent, iterator->next ? " |-b- " : " +-b- " , NULL);
		pack = (PackageData*)iterator->data;
		install_failed (service, pack, indent2);
		g_free (indent2);
	}
	for (iterator = pd->modifies; iterator; iterator = iterator->next) {			
		PackageData *pack;
		char *indent2;
		indent2 = g_strconcat (indent, iterator->next ? " |-m- " : " +-m- " , NULL);
		pack = (PackageData*)iterator->data;
		install_failed (service, pack, indent2);
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
	struct utsname buf;
	PackageData *pack;

	uname (&buf);
	pack = packagedata_new ();
	if (arg_file) {
		pack->filename = g_strdup (name);
	} else {
		pack->name = g_strdup (name);
	}
	pack->archtype = g_strdup (buf.machine);
#ifdef ASSUME_ix86_IS_i386
	if (strlen (pack->archtype)==4 && pack->archtype[0]=='i' &&
	    pack->archtype[1]>='3' && pack->archtype[1]<='9' &&
	    pack->archtype[2]=='8' && pack->archtype[3]=='6') {
		g_free (pack->archtype);
		pack->archtype = g_strdup ("i386");
	}
#endif
	pack->distribution = trilobite_get_distribution ();
	pack->toplevel = TRUE;
	
	return pack;
}

static gboolean
delete_files (EazelInstallCallback *service, gpointer unused)
{
	char answer[10];

	printf ("should i delete the RPM files? (y/n) ");
	fflush (stdout);

	fgets (answer, 10, stdin);
	if (answer[0] == 'y' || answer[0] == 'Y') {
		printf ("you said: YES\n");
		fflush (stdout);
		return TRUE;
	}
	printf ("you said: NO\n");
	fflush (stdout);
	return FALSE;
}

static void
done (EazelInstallCallback *service,
      gpointer unused)
{
	fprintf (stderr, "Installation Done\n");
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
	GList *strs;
	EazelInstallCallback *cb;		

	CORBA_exception_init (&ev);

	strs = NULL;

	/* Seems that bonobo_main doens't like
	   not having gnome_init called, dies in a
	   X call, yech */
#if 1
	gnome_init_with_popt_table ("Eazel Install", "1.0", argc, argv, options, 0, &ctxt);
#else
	gtk_type_init ();
	gnomelib_init ("Eazel Install", "1.0");
	gnomelib_register_popt_table (options, "Eazel Install");
	ctxt = gnomelib_parse_args (argc, argv, 0);
#endif
	
	packages = NULL;
	categories = NULL;
	/* If there are more args, get them and parse them as packages */
	while ((str = poptGetArg (ctxt)) != NULL) {
		packages = g_list_prepend (packages, create_package (str));
		strs = g_list_prepend (strs, g_strdup (str));
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
	if (check_for_redhat () == FALSE) {
		fprintf (stderr, "*** This tool can only be used on RedHat.\n");
		exit (1);
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
	
	gtk_signal_connect (GTK_OBJECT (cb), "download_progress", eazel_download_progress_signal, "Download progress");
	gtk_signal_connect (GTK_OBJECT (cb), "preflight_check", eazel_preflight_check_signal, NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "install_progress", eazel_install_progress_signal, "Installing");
	gtk_signal_connect (GTK_OBJECT (cb), "install_failed", install_failed, "");
	gtk_signal_connect (GTK_OBJECT (cb), "uninstall_progress", eazel_install_progress_signal, "Uninstalling");
	gtk_signal_connect (GTK_OBJECT (cb), "uninstall_failed", install_failed, "");
	gtk_signal_connect (GTK_OBJECT (cb), "download_failed", download_failed, NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "dependency_check", dep_check, NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "delete_files", (void *)delete_files, NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "done", done, NULL);

	if (arg_erase + arg_query > 1) {
		g_error ("Only one operation at a time please.");
	}

	if (arg_erase) {
		eazel_install_callback_uninstall_packages (cb, categories, &ev);
	} else if (arg_query) {
		GList *iterator;
		for (iterator = strs; iterator; iterator = iterator->next) {
			GList *matched_packages;
			GList *match_it;
			matched_packages = eazel_install_callback_simple_query (cb, (char*)iterator->data, &ev);
			for (match_it = matched_packages; match_it; match_it = match_it->next) {
				PackageData *p;
				p = (PackageData*)match_it->data;
				fprintf (stdout, "%s %s %50.50s", p->name, p->version, p->summary);
			}
		}
	} else {
		eazel_install_callback_install_packages (cb, categories, &ev);
	}
	
	if (!arg_query) {
		fprintf (stdout, "\nEntering main loop...\n");
		bonobo_main ();
	}

	eazel_install_callback_destroy (GTK_OBJECT (cb));

	/* Corba cleanup */
	CORBA_exception_free (&ev);
       
	return 0;
};
