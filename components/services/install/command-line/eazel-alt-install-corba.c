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

#include <eazel-package-system-types.h>
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
	arg_id,
	arg_ei2,
	arg_no_pct,
	arg_no_auth;
char    *arg_server,
	*arg_cgi,
	*arg_config_file,
	*arg_package_list,
	*arg_username,
	*arg_root,
	*arg_batch;

/* Yeahyeah, but this was initially a test tool,
   so stop whining... */
CORBA_ORB orb;
CORBA_Environment ev;
int cli_result = 0;
GList *cases = NULL;
GList *categories;
gboolean downloaded_files = FALSE;

static const struct poptOption options[] = {
	{"batch", '\0', POPT_ARG_STRING, &arg_batch, 0, N_("Set the default answer to continue, also default delete to Yes"), NULL},
	{"cgi-path", '\0', POPT_ARG_STRING, &arg_cgi, 0, N_("Specify search cgi"), NULL},
	{"debug", '\0', POPT_ARG_NONE, &arg_debug, 0 , N_("Show debug output"), NULL},
	{"delay", '\0', POPT_ARG_NONE, &arg_delay, 0 , N_("10 sec delay after starting service"), NULL},
	{"downgrade", 'd', POPT_ARG_NONE, &arg_downgrade, 0, N_("Allow downgrades"), NULL},
	{"erase", 'e', POPT_ARG_NONE, &arg_erase, 0, N_("Erase packages"), NULL},
	{"ei2", '\0', POPT_ARG_NONE, &arg_ei2, 0, N_("enable ei2"), NULL},
	{"file",'\0', POPT_ARG_NONE, &arg_file, 0, N_("RPM args are filename"), NULL},
	{"force", 'F', POPT_ARG_NONE, &arg_force, 0, N_("Force install"), NULL},
	{"ftp", 'f', POPT_ARG_NONE, &arg_ftp, 0, N_("Use ftp"), NULL},
	{"local", 'l', POPT_ARG_NONE, &arg_local, 0, N_("Use local"), NULL},
	{"http", 'h', POPT_ARG_NONE, &arg_http, 0, N_("Use http"), NULL},
	{"id", 'i', POPT_ARG_NONE, &arg_id, 0, N_("RPM args are Eazel Ids"), NULL},
	{"no-percent", '\0', POPT_ARG_NONE, &arg_no_pct, 0, N_("Don't print fancy percent output"), NULL},
	{"no-auth", '\0', POPT_ARG_NONE, &arg_no_auth, 0, N_("don't use eazel auth stuff"), NULL},
	{"packagefile", '\0', POPT_ARG_STRING, &arg_package_list, 0, N_("Specify package file"), NULL},
	{"provides", '\0', POPT_ARG_NONE, &arg_provides, 0, N_("RPM args are needed files"), NULL},
	{"query", 'q', POPT_ARG_NONE, &arg_query, 0, N_("Run Query"), NULL},
	{"revert", 'r', POPT_ARG_NONE, &arg_revert, 0, N_("Revert"), NULL},
	{"root", '\0', POPT_ARG_STRING, &arg_root, 0, N_("Set root"), NULL},
	{"server", '\0', POPT_ARG_STRING, &arg_server, 0, N_("Specify server"), NULL},
	{"ssl-rename", 's', POPT_ARG_NONE, &arg_ssl_rename, 0, N_("Perform ssl renaming"), NULL},
	{"test", 't', POPT_ARG_NONE, &arg_dry_run, 0, N_("Test run"), NULL},
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
set_parameters_from_command_line (GNOME_Trilobite_Eazel_Install service)
{
	if (!arg_debug) {		
		GNOME_Trilobite_Eazel_Install__set_log_file (service, DEFAULT_LOG_FILE, &ev);
		check_ev ("set_log_file");
	} else {
		GNOME_Trilobite_Eazel_Install__set_debug (service, TRUE, &ev);
		check_ev ("set_debug");
	}

	/* We only want 1 protocol type */
	if (arg_http + arg_ftp + arg_local > 1) {
			fprintf (stderr, "*** You cannot specify more then one protocol type.\n");
			exit (1);
	}
	if (arg_http) {
		GNOME_Trilobite_Eazel_Install__set_protocol (service, GNOME_Trilobite_Eazel_PROTOCOL_HTTP, &ev);
		check_ev ("set_protocol");
	} else if (arg_ftp) {
		GNOME_Trilobite_Eazel_Install__set_protocol (service, GNOME_Trilobite_Eazel_PROTOCOL_FTP, &ev);
		check_ev ("set_protocol");
	} else if (arg_local) {
		GNOME_Trilobite_Eazel_Install__set_protocol (service, GNOME_Trilobite_Eazel_PROTOCOL_LOCAL, &ev);
		check_ev ("set_protocol");
	} else {
		GNOME_Trilobite_Eazel_Install__set_protocol (service, GNOME_Trilobite_Eazel_PROTOCOL_HTTP, &ev);
		check_ev ("set_protocol");
	}
	if (arg_downgrade + arg_upgrade + arg_erase +arg_revert > 1) {
			fprintf (stderr, "*** Upgrade, downgrade, revert and erase ? This is not a all-in-one tool");
			exit (1);
	}
	if (arg_upgrade) {
		GNOME_Trilobite_Eazel_Install__set_update (service, TRUE, &ev);
		check_ev ("update");
	}
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

	if (arg_ssl_rename) {
		GNOME_Trilobite_Eazel_Install__set_ssl_rename (service, TRUE, &ev);
		check_ev ("set_ssl_rename");
	}
	if (arg_ei2) {
		GNOME_Trilobite_Eazel_Install__set_ei2 (service, TRUE, &ev);
		check_ev ("set_ei2");
	}
	if (arg_no_auth) {
		GNOME_Trilobite_Eazel_Install__set_auth (service, FALSE, &ev);
		check_ev ("set_auth");
	} 
	if (arg_dry_run) {
		GNOME_Trilobite_Eazel_Install__set_test_mode (service, TRUE, &ev);
	}
	if (arg_force) {
		GNOME_Trilobite_Eazel_Install__set_force (service, TRUE, &ev);
	}
	if (arg_package_list) {
		GNOME_Trilobite_Eazel_Install__set_package_list (service, arg_package_list, &ev);
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
				const PackageData *pack,
				int amount, 
				int total,
				char *title) 
{
	static time_t t;
	static int pct;
	static int old_pct;

	time_t end;
	time_t diff;
	static float ks=0;

	g_assert (pack->name != NULL);

	downloaded_files = TRUE;

	if (amount==0) {
		fprintf (stdout, "Downloading %s...", pack->name);
		t = time (NULL);
		old_pct = pct = 0;
	} else if (amount != total ) {
		if (arg_no_pct==0) {
			pct = amount / (total / 100);
			if (pct > 5) {
				if (old_pct != pct && pct%5==0) {
					end = time (NULL);
					diff = end - t;
					ks = ((float)amount/1024)/diff;
					old_pct = pct;
				}
				fprintf (stdout, "\rDownloading %s... (%d/%d) = %d%% %.1f Kb/s     \r", 
					 pack->name,
					 amount, total, pct,
					 ks);
			} else {
				fprintf (stdout, "\rDownloading %s... (%d/%d) = %d%%", 
					 pack->name,
					 amount, total, pct);
			}
		}
	} else if (amount == total && total!=0) {
		if (arg_no_pct==0) {
			fprintf (stdout, "\rDownloading %s... (%d/%d) %.1f Kb/s Done      \n",
				 pack->name,
				 amount, total, 
				 ks);
		} else {
			fprintf (stdout, "Downloading %s... %3.1f KB/s Done\n", 
				 pack->name, ks);
		}
	}
	fflush (stdout);
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
		fprintf (stdout, "%s %s", title, pack->name);
	} else if (amount != total ) {
		if (arg_no_pct==0) {
			fprintf (stdout, "\r%s %s (%d/%d), (%d/%d)b - (%d/%d) = %d%%", 
				 title, pack->name,
				 package_num, num_packages,
				 total_size_completed, total_size,
				 amount, total,
				 amount / (total / 100));
		}
	}
	if (amount == total && total!=0) {
		if (arg_no_pct==0) {
			fprintf (stdout, "\r%s %s (%d/%d), (%d/%d)b - (%d/%d) = %d%% Done\n",
				 title, pack->name,
				 package_num, num_packages,
				 total_size_completed, total_size,
				 amount, total, 100);
		} else {
			fprintf (stdout, "Done\n");
		}

	}
	fflush (stdout);
}

static void
download_failed (EazelInstallCallback *service, 
		 const PackageData *pack,
		 gpointer unused)
{
	g_assert (pack->name != NULL);
	fprintf (stdout, "Download of %s FAILED\n", pack->name);
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
	PackageData *pack;
	char *indent2;
	char indenter;
	gchar *extra_space=NULL;
	int indent_level_cnt;

	if (IS_PACKAGEDATA (iterator->data)) {
		pack = PACKAGEDATA (iterator->data);
	} else {
		PackageDependency *dep = PACKAGEDEPENDENCY (iterator->data);
		pack = dep->package;
	}

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
	char *readable_name;
	GList *iterator;

	if (title && pd->toplevel) {
		fprintf (stdout, title);
	}

	readable_name = packagedata_get_readable_name (pd);
	fprintf (stdout, "%s%s%s (%s/%s)\n", 
		 indent,  indent_type,
		 readable_name,
		 packagedata_status_enum_to_str (pd->status),
		 packagedata_modstatus_enum_to_str (pd->modify_status));
	g_free (readable_name);

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

static void
something_failed (EazelInstallCallback *service,
		  const PackageData *pd,
		  EazelInstallProblem *problem,
		  gboolean uninstall)
{
	char *title;
	GList *stuff = NULL;	

	gtk_object_ref (GTK_OBJECT (pd));

	if (uninstall) {
		title = g_strdup_printf ("\nPackage %s failed to uninstall.\n", pd->name);
	} else {
		title = g_strdup_printf ("\nPackage %s failed to install.\n", pd->name);
	}

	if (arg_debug) {
		tree_helper (service, pd, "", "", 4, title);
		fprintf (stdout, "\n");
	} else {
		fprintf (stdout, "%s", title);
	}

	g_free (title);

	if (problem) {
		stuff = eazel_install_problem_tree_to_string (problem, pd, uninstall);
		if (stuff) {
			GList *it;
			for (it = stuff; it; it = g_list_next (it)) {
				fprintf (stdout, "Problem : %s\n", (char*)(it->data));
			}
		}
		
		eazel_install_problem_tree_to_case (problem, pd, uninstall, &cases);
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
	gtk_object_unref (GTK_OBJECT (pd));
}

/*
  This dumps the entire tree for the failed package.
 */
static void
install_failed (EazelInstallCallback *service,
		const PackageData *pd,
		EazelInstallProblem *problem)
{
	something_failed (service, pd, problem, FALSE);
}

static void
uninstall_failed (EazelInstallCallback *service,
		  const PackageData *pd,		
		  EazelInstallProblem *problem)
{
	something_failed (service, pd, problem, TRUE);
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
	   const PackageData *needs_package,
	   gpointer unused) 
{
	char *pack, *needs;
	pack = packagedata_get_readable_name (package);
	needs = packagedata_get_readable_name (needs_package);
	printf ("Dependency : %s needs %s\n", pack, needs);
	g_free (pack);
	g_free (needs);
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
	gboolean ask_delete = TRUE;
	gboolean result = TRUE;
	
	if (cases) {
		printf ("continue? (y/n) ");
		fflush (stdout);
		if (arg_batch) {			
			fprintf (stdout, "%s\n", arg_batch);
			strcpy (answer, arg_batch);
		} else {
			fgets (answer, 10, stdin);
		}
		if (answer[0] == 'y' || answer[0] == 'Y') {
			fflush (stdout);
			eazel_install_problem_handle_cases (problem, 
							    service, 
							    &cases, 
							    &categories, 
							    NULL,
							    arg_root);
			ask_delete = FALSE;
			result = FALSE;
		} else {
			eazel_install_problem_case_list_destroy (cases);
			cases = NULL;
		}		
	} 

	if (/* downloaded_files && */ !arg_query && !arg_erase && !arg_file && ask_delete) {
		printf ("should i delete the RPM files? (y/n) ");
		fflush (stdout);
		if (arg_batch) {			
			fprintf (stdout, "yes\n");
			strcpy (answer, "yes");
		} else {
			fgets (answer, 10, stdin);
		}
		
		if (answer[0] == 'y' || answer[0] == 'Y') {
			fflush (stdout);
			eazel_install_callback_delete_files (service, &ev);			
		} 
	}
	return result;
}

static void
done (EazelInstallCallback *service,
      gboolean result,
      EazelInstallProblem *problem)
{
	fprintf (stderr, "Operation %s\n", result ? "ok" : "failed");
	cli_result = result ? 0 : 1;

	if (delete_files (service, problem)) {
		trilobite_main_quit ();
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
	char *str;
	GList *strs;
	EazelInstallCallback *cb;		
	EazelInstallProblem *problem = NULL;

	CORBA_exception_init (&ev);

	strs = NULL;

	/* Seems that bonobo_main doens't like
	   not having gnome_init called, dies in a
	   X call, yech */
#if 0
	gnome_init_with_popt_table ("Eazel Install", "1.0", argc, argv, options, 0, &ctxt);
	orb = oaf_init (argc, argv);
	if (!bonobo_init (NULL, NULL, NULL)) {
		g_error ("Could not init bonobo");
	}
#else
	trilobite_init ("Eazel Install", "1.0", NULL, options, argc, argv);
	ctxt = trilobite_get_popt_context ();
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
	gtk_signal_connect (GTK_OBJECT (cb), "done", 
			    GTK_SIGNAL_FUNC (done), 
			    problem);

	if (arg_erase + arg_query + arg_downgrade + arg_upgrade + arg_revert > 1) {
		g_error ("Only one operation at a time please.");
	}

	if (arg_erase) {
		if (categories == NULL) {
			fprintf (stderr, "%s: --help for usage\n", argv[0]);
			cli_result = 1;
		} else {
			eazel_install_callback_uninstall_packages (cb, categories, arg_root, &ev);
		}
	} else if (arg_query) {
		GList *iterator;
		if (strs == NULL) {
			fprintf (stderr, "%s: --help for usage\n", argv[0]);
			cli_result = 1;
		} else for (iterator = strs; iterator; iterator = iterator->next) {
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
			cli_result = 0;
		}
	} else if (arg_revert) {
		GList *iterator;
		if (strs == NULL) {
			fprintf (stderr, "%s: --help for usage\n", argv[0]);
			cli_result = 1;
		} else for (iterator = strs; iterator; iterator = iterator->next) {
			eazel_install_callback_revert_transaction (cb, (char*)iterator->data, arg_root, &ev);
		}
	} else {
		if (categories == NULL) {
			fprintf (stderr, "%s: --help for usage\n", argv[0]);
			cli_result = 1;
		} else {
			eazel_install_callback_install_packages (cb, categories, arg_root, &ev);
		}
	}
	
	if (!cli_result && !arg_query) {
		trilobite_main ();
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
