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
#include <eazel-install-problem.h>

#include <libtrilobite/libtrilobite.h>
#include <trilobite-eazel-install.h>

#include <unistd.h>

#define PACKAGE_FILE_NAME "package-list.xml"

#define DEFAULT_PROTOCOL PROTOCOL_HTTP
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
	arg_query,
	arg_revert,
	arg_ssl_rename,
	arg_provides,
	arg_verbose,
	arg_id;
char    *arg_server,
	*arg_config_file,
	*arg_package_list,
	*arg_tmp_dir,
	*arg_username,
	*arg_root;

/* Yeahyeah, but this was initially a test tool,
   so stop whining... */
CORBA_ORB orb;
CORBA_Environment ev;
int cli_result = 1;
GList *cases = NULL;

static const struct poptOption options[] = {
	{"debug", '0', POPT_ARG_NONE, &arg_debug, 0 , N_("Show debug output"), NULL},
	{"delay", '\0', POPT_ARG_NONE, &arg_delay, 0 , N_("10 sec delay after starting service"), NULL},
	{"downgrade", 'd', POPT_ARG_NONE, &arg_downgrade, 0, N_("Allow downgrades"), NULL},
	{"erase", 'e', POPT_ARG_NONE, &arg_erase, 0, N_("Erase packages"), NULL},
	{"file",'\0', POPT_ARG_NONE, &arg_file, 0, N_("RPM args are filename"), NULL},
	{"force", 'F', POPT_ARG_NONE, &arg_force, 0, N_("Force install"), NULL},
	{"ftp", 'f', POPT_ARG_NONE, &arg_ftp, 0, N_("Use ftp"), NULL},
	{"local", 'l', POPT_ARG_NONE, &arg_local, 0, N_("Use local"), NULL},
	{"http", 'h', POPT_ARG_NONE, &arg_http, 0, N_("Use http"), NULL},
	{"id", 'i', POPT_ARG_NONE, &arg_id, 0, N_("RPM args are Eazel Ids"), NULL},
	{"packagefile", '\0', POPT_ARG_STRING, &arg_package_list, 0, N_("Specify package file"), NULL},
	{"port", '\0', POPT_ARG_INT, &arg_port, 0 , N_("Set port numer (80)"), NULL},
	{"provides", '\0', POPT_ARG_NONE, &arg_provides, 0, N_("RPM args are needed files"), NULL},
	{"query", 'q', POPT_ARG_NONE, &arg_query, 0, N_("Run Query"), NULL},
	{"revert", 'r', POPT_ARG_NONE, &arg_revert, 0, N_("Revert"), NULL},
	{"root", '\0', POPT_ARG_STRING, &arg_root, 0, N_("Set root"), NULL},
	{"server", '\0', POPT_ARG_STRING, &arg_server, 0, N_("Specify server"), NULL},
	{"ssl_rename", 's', POPT_ARG_NONE, &arg_ssl_rename, 0, N_("Perform ssl renaming"), NULL},
	{"test", 't', POPT_ARG_NONE, &arg_dry_run, 0, N_("Test run"), NULL},
	{"tmp", '\0', POPT_ARG_STRING, &arg_tmp_dir, 0, N_("Set tmp dir (/tmp)"), NULL},
	{"username", '\0', POPT_ARG_STRING, &arg_username, 0, N_("Allow username"), NULL},
	{"upgrade", 'u', POPT_ARG_NONE, &arg_upgrade, 0, N_("Allow upgrades"), NULL},
	{"verbose", 'v', POPT_ARG_NONE, &arg_verbose, 0, N_("Verbose output"), NULL},
	{NULL, '\0', 0, NULL, 0}
};

static void tree_helper (EazelInstallCallback *service,
			 const PackageData *pd,		
			 gchar *indent,
			 gchar *indent_type,
			 int indent_level,
			 char *title);

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
	if (arg_downgrade + arg_upgrade + arg_erase +arg_revert > 1) {
			fprintf (stderr, "*** Upgrade, downgrade, revert and erase ? This is not a all-in-one tool");
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
	if (arg_server) {
		Trilobite_Eazel_Install__set_server (service, arg_server, &ev);
		check_ev ("set_server");
	}
	if (arg_username) {
		Trilobite_Eazel_Install__set_username (service, arg_username, &ev);
		check_ev ("set_username");
	}

	if (arg_ssl_rename) {
		Trilobite_Eazel_Install__set_ssl_rename (service, TRUE, &ev);
		check_ev ("set_ssl_rename");
	}

#define RANDCHAR ('A' + (rand () % 23))
	if (arg_tmp_dir == NULL) {
		int tries;
		srand (time (NULL));
		for (tries = 0; tries < 50; tries++) {
			arg_tmp_dir = g_strdup_printf ("/tmp/eazel-installer.%c%c%c%c%c%c%d",
						  RANDCHAR, RANDCHAR, RANDCHAR, RANDCHAR,
						  RANDCHAR, RANDCHAR, (rand () % 1000));
			if (g_file_test (arg_tmp_dir, G_FILE_TEST_ISDIR)==0) {
				break;
			}
			g_free (arg_tmp_dir);
		}
	}
/*
	Trilobite_Eazel_Install__set_tmp_dir (service, arg_tmp_dir, &ev);
	check_ev ("set_tmp_dir");
*/
	if (arg_port) {
		Trilobite_Eazel_Install__set_server_port (service, arg_port, &ev);
		check_ev ("set_server_port");
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
		g_message ("DB root = %s", arg_root);
	}
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
		fprintf (stdout, "%s %s: \"%20.20s\"...\n", title, pack->name, pack->description);
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

/* This is ridiculous... */
static void
tree_helper_helper(EazelInstallCallback *service,
		   gchar *indent,
		   gchar *indent_type,
		   int indent_level,
		   GList *iterator,
		   GList *next_list) 
{
	PackageData *pack = (PackageData*)iterator->data;
	char *indent2;
	char indenter;
	gchar *extra_space=NULL;
	int indent_level_cnt;

	if (indent_level>0) {
		extra_space = g_new0 (char, indent_level+1);
		for (indent_level_cnt = 0; indent_level_cnt < indent_level; indent_level_cnt++) {
			extra_space [indent_level_cnt] = ' ';
		}
	}

	if (iterator->next || next_list) {
		indenter = '|';
	} else {
		indenter = ' ';
		*indent_type = '\\';
	}
	
	indent2 = g_strdup_printf ("%s%s%c", indent, extra_space ? extra_space : "", indenter);
	tree_helper (service, pack, indent2, indent_type, indent_level, NULL);
	g_free (indent2);
	g_free (extra_space);	
}


static void
tree_helper (EazelInstallCallback *service,
	     const PackageData *pd,		
	     gchar *indent,
	     gchar *indent_type,
	     int indent_level,
	     char *title)
{
	GList *iterator;

	if (title && pd->toplevel) {
		fprintf (stdout, title);
	}

	switch (pd->status) {
	case PACKAGE_DEPENDENCY_FAIL:
		fprintf (stdout, "%s%s%s, which had dependency failure(s)\n", 
			 indent,  indent_type, 
			 rpmname_from_packagedata (pd));
		break;
	case PACKAGE_CANNOT_OPEN:
		fprintf (stdout, "%s%s%s,which was not found\n", 
			 indent,  indent_type,
			 rpmname_from_packagedata (pd));
		break;		
	case PACKAGE_SOURCE_NOT_SUPPORTED:
		fprintf (stdout, "%s%s%s, which is a source package\n", 
			 indent,  indent_type,
			 rpmname_from_packagedata (pd));
		break;
	case PACKAGE_BREAKS_DEPENDENCY:
		fprintf (stdout, "%s%s%s, which breaks deps\n", 
			 indent,  indent_type,
			 rpmname_from_packagedata (pd));
		break;
	case PACKAGE_FILE_CONFLICT:
		fprintf (stdout, "%s%s%s, which has file conflict\n", 
			 indent,  indent_type,
			 rpmname_from_packagedata (pd));
		break;
	case PACKAGE_CIRCULAR_DEPENDENCY:
		fprintf (stdout, "%s%s%s, package would cause circular dependency\n", 
			 indent,  indent_type,
			 rpmname_from_packagedata (pd));
		break;
	default:
		fprintf (stdout, "%s%s%s %s(status %s)\n", 
			 indent,  indent_type,
			 rpmname_from_packagedata (pd),
			 pd->status==PACKAGE_ALREADY_INSTALLED ? "already installed " : "",
			 packagedata_status_enum_to_str (pd->status));
		break;
	}
	for (iterator = pd->soft_depends; iterator; iterator = iterator->next) {		
		char *tmp;
		tmp = g_strdup ("-d-");
		tree_helper_helper (service, indent, tmp, indent_level, iterator, 
				    pd->breaks ? pd->breaks : pd->modifies);
		g_free (tmp);
	}
	for (iterator = pd->breaks; iterator; iterator = iterator->next) {			
		char *tmp;
		tmp = g_strdup ("-b-");
		tree_helper_helper (service, indent, tmp, indent_level, iterator, pd->modifies);
		g_free (tmp);
	}
	for (iterator = pd->modifies; iterator; iterator = iterator->next) {			
		char *tmp;
		tmp = g_strdup ("-m-");
		tree_helper_helper (service, indent, tmp, indent_level, iterator, NULL);
		g_free (tmp);
	}
}

/*
  This dumps the entire tree for the failed package.
 */
static void
install_failed (EazelInstallCallback *service,
		const PackageData *pd,
		EazelInstallProblem *problem)
{
	char *title;
	GList *stuff = NULL;	

	title = g_strdup_printf ("\nPackage %s failed to install. Here's the tree...\n", pd->name);
	tree_helper (service, pd, "", "", 4, title);
	fprintf (stdout, "\n");

	if (problem) {
		stuff = eazel_install_problem_tree_to_string (problem, pd);
		if (stuff) {
			GList *it;
			for (it = stuff; it; it = g_list_next (it)) {
				fprintf (stdout, "%s\n", (char*)(it->data));
			}
		}
		
		eazel_install_problem_tree_to_case (problem, pd, &cases);
		stuff = eazel_install_problem_cases_to_string (problem, cases);
		if (cases) {
			stuff = eazel_install_problem_cases_to_string (problem, cases);
			if (stuff) {
				GList *it;
				for (it = stuff; it; it = g_list_next (it)) {
					fprintf (stdout, "Solution : %s\n", (char*)(it->data));
			}
			}
		}
	}
	
	g_free (title);
}

static void
uninstall_failed (EazelInstallCallback *service,
		  const PackageData *pd,		
		  gpointer unused)
{
	char *title;
	title = g_strdup_printf ("\nPackage %s failed to uninstall. Here's the tree...\n", pd->name);
	tree_helper (service, pd, "", "", 4, title);
	g_free (title);
}

static gboolean
eazel_preflight_check_signal (EazelInstallCallback *service, 
			      const GList *packages,
			      int total_bytes,
			      int total_packages,
			      gpointer unused) 
{	
	const GList *iterator;

	fprintf (stdout, "About to %s a total of %d packages, %dKb\n", 
		 arg_erase ? "uninstall" : "install",
		 total_packages, total_bytes/1024);
	for (iterator = packages; iterator; iterator = iterator->next) {
		PackageData *pack = (PackageData*)iterator->data;
		tree_helper (service, pack, "", "", 4, NULL);
	}

	return TRUE;
}

static void
dep_check (EazelInstallCallback *service,
	   const PackageData *package,
	   const PackageData *needs,
	   gpointer unused) 
{
	if (needs->name && needs->version) {
		fprintf (stdout, "Doing dependency check for %s-%s - need %s-%s\n", 
			 package->name, package->version,
			 needs->name, needs->version);
	} else if (needs->name) {
		fprintf (stdout, "Doing dependency check for %s-%s - need %s\n", 
			 package->name, package->version,
			 needs->name);
	} else if (needs->provides) {
		GList *iterator;
		fprintf (stdout, "Doing dependency check for %s-%s - need", 
			 package->name, package->version);
		for (iterator = needs->provides; iterator; iterator = g_list_next (iterator)) {
			fprintf (stdout, " %s", (char*)iterator->data);
		}
		fprintf (stdout, "\n");
	} else {
		fprintf (stdout, "Doing dependency check for %s-%s - needs something, but I don't know what it was...\n", 
			 package->name, package->version);
	}
}

static void
md5_check_failed (EazelInstallCallback *service,
		  const PackageData *package,
		  const char *actual_md5,
		  gpointer unused) 
{
	fprintf (stdout, "Package %s failed md5 check!\n", package->name);
	fprintf (stdout, "server MD5 checksum is %s\n", package->md5);
	fprintf (stdout, "actual MD5 checksum is %s\n", actual_md5);
}

static PackageData*
create_package (char *name) 
{
	struct utsname buf;
	PackageData *pack;

	uname (&buf);
	pack = packagedata_new ();
	if (arg_file) {
		/* If file starts with /, use filename,
		   if it starts with ~, insert g_get_home_dir,
		   otherwise, insert g_get_current_dir */
		pack->filename = 
			name[0]=='/' ? 
			g_strdup (name) : 
			name[0]=='~' ?
			g_strdup_printf ("%s/%s", g_get_home_dir (), name+1) :
			g_strdup_printf ("%s/%s", g_get_current_dir (), name);
	} else if (arg_provides) {
		pack->provides = g_list_prepend (pack->provides, g_strdup (name));
	} else if (arg_id) {
		pack->eazel_id = g_strdup (name);
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
delete_files (EazelInstallCallback *service, EazelInstallProblem *problem)
{
	char answer[128];
	gboolean ask_delete = FALSE;

	if (cases) {
		printf ("continue? (y/n) ");
		fflush (stdout);
		
		fgets (answer, 10, stdin);
		if (answer[0] == 'y' || answer[0] == 'Y') {
			printf ("you said: YES\n");
			fflush (stdout);
			eazel_install_problem_handle_cases (problem, service, &cases, arg_root);
		} else {
			eazel_install_problem_case_list_destroy (cases);
			cases = NULL;
			printf ("you said: NO\n");
			fflush (stdout);
			ask_delete = TRUE;
		}		
	} 

	if (ask_delete) {
		printf ("should i delete the RPM files? (y/n) ");
		fflush (stdout);
		
		fgets (answer, 10, stdin);
		if (answer[0] == 'y' || answer[0] == 'Y') {
			printf ("you said: YES\n");
			fflush (stdout);
			return TRUE;
		} else {
			printf ("you said: NO\n");
			fflush (stdout);
		}
	}
	return FALSE;
}

static void
done (EazelInstallCallback *service,
      gboolean result,
      gpointer unused)
{
	fprintf (stderr, "Operation %s\n", result ? "ok" : "failed");
	cli_result = result ? 0 : 1;
	if (cases == NULL) {
		gtk_main_quit ();
	}
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
	EazelInstallProblem *problem = NULL;

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
		category = categorydata_new ();
		category->name = g_strdup ("files from commandline");
		category->packages = packages;
		categories = g_list_prepend (NULL, category);		
	} else {
		g_message ("Using remote list ");
	}

	/* Check that we're root and on a redhat system */
	if (!check_for_redhat ()) {
		fprintf (stderr, "*** This tool can only be used on RedHat.\n");
		exit (1);
	}

	orb = oaf_init (argc, argv);
	
	if (!bonobo_init (NULL, NULL, NULL)) {
		g_error ("Could not init bonobo");
	}
	bonobo_activate ();
	
	cb = eazel_install_callback_new ();
	problem = eazel_install_problem_new (); 
	gtk_object_ref (GTK_OBJECT (problem));

	if (arg_delay) {
		sleep (10);
	}

	set_parameters_from_command_line (eazel_install_callback_corba_objref (cb));
	set_root_client (eazel_install_callback_bonobo (cb));
	
	/* Set up signal connections */
	gtk_signal_connect (GTK_OBJECT (cb), "download_progress", 
			    GTK_SIGNAL_FUNC (eazel_download_progress_signal), 
			    "Download progress");
	gtk_signal_connect (GTK_OBJECT (cb), "preflight_check", 
			    GTK_SIGNAL_FUNC (eazel_preflight_check_signal), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "install_progress", 
			    GTK_SIGNAL_FUNC (eazel_install_progress_signal), 
			    "Installing");
	gtk_signal_connect (GTK_OBJECT (cb), "md5_check_failed", 
			    GTK_SIGNAL_FUNC (md5_check_failed), 
			    "");
	gtk_signal_connect (GTK_OBJECT (cb), "install_failed", 
			    GTK_SIGNAL_FUNC (install_failed), 
			    problem);
	gtk_signal_connect (GTK_OBJECT (cb), "uninstall_progress", 
			    GTK_SIGNAL_FUNC (eazel_install_progress_signal), 
			    "Uninstalling");
	gtk_signal_connect (GTK_OBJECT (cb), "uninstall_failed", 
			    GTK_SIGNAL_FUNC (uninstall_failed), 
			    problem);
	gtk_signal_connect (GTK_OBJECT (cb), "download_failed", 
			    GTK_SIGNAL_FUNC (download_failed), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "dependency_check", 
			    GTK_SIGNAL_FUNC (dep_check), 
			    NULL);
	gtk_signal_connect (GTK_OBJECT (cb), "delete_files", 
			    GTK_SIGNAL_FUNC (delete_files), 
			    problem);
	gtk_signal_connect (GTK_OBJECT (cb), "done", 
			    GTK_SIGNAL_FUNC (done), 
			    NULL);

	if (arg_erase + arg_query + arg_downgrade + arg_upgrade + arg_revert > 1) {
		g_error ("Only one operation at a time please.");
	}

	if (arg_erase) {
		eazel_install_callback_uninstall_packages (cb, categories, arg_root, &ev);
	} else if (arg_query) {
		GList *iterator;
		for (iterator = strs; iterator; iterator = iterator->next) {
			GList *matched_packages;
			GList *match_it;
			matched_packages = eazel_install_callback_simple_query (cb, 
										(char*)iterator->data, 
										NULL, 
										&ev);
			for (match_it = matched_packages; match_it; match_it = match_it->next) {
				PackageData *p;
				p = (PackageData*)match_it->data;
				if (arg_verbose) {
					char *tmp;
					GList *provide_iterator;
					tmp = trilobite_get_distribution_name (p->distribution, TRUE, FALSE);
					fprintf (stdout, "Name         : %s\n", p->name?p->name:"?"); 
					fprintf (stdout, "Version      : %s\n", p->version?p->version:"?");
					fprintf (stdout, "Minor        : %s\n", p->minor?p->minor:"?");

					fprintf (stdout, "Size         : %d\n", p->bytesize);
					fprintf (stdout, "Arch         : %s\n", p->archtype?p->archtype:"?");
					fprintf (stdout, "Distribution : %s\n", tmp?tmp:"?");
					fprintf (stdout, "Description  : %s\n", 
						 p->description?p->description:"?");
					fprintf (stdout, "Install root : %s\n", 
						 p->install_root?p->install_root:"?");
					if (p->provides) {
						fprintf (stdout, "Provides     : \n");
						for (provide_iterator = p->provides; provide_iterator; 
						     provide_iterator = g_list_next (provide_iterator)) {
							fprintf (stdout, "\t%s\n", 
								 (char*)provide_iterator->data);
						}
					}
				} else {
					fprintf (stdout, "%s %s %50.50s\n", p->name, p->version, p->description);
				}
			}
		}
		cli_result = 0;
	} else if (arg_revert) {
		GList *iterator;
		for (iterator = strs; iterator; iterator = iterator->next) {
			eazel_install_callback_revert_transaction (cb, (char*)iterator->data, arg_root, &ev);
		}
	} else {
		eazel_install_callback_install_packages (cb, categories, arg_root, &ev);
	}
	
	if (!arg_query) {
		gtk_main ();
	}

	categorydata_list_destroy (categories);

	eazel_install_callback_unref (GTK_OBJECT (cb));
	gtk_object_unref (GTK_OBJECT (problem));
	/* Corba cleanup */
	CORBA_exception_free (&ev);

	if (arg_debug) {
		g_message ("cli_result = %d", cli_result);
	}
       
	return cli_result;
};
