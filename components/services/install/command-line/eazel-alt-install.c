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

/*
  static link with
gcc -static -g -Wall -Wno-uninitialized -Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes -Wnested-externs \
-Wpointer-arith -Wno-sign-compare -Wsign-promo -o .libs/eazel-alt-install eazel-alt-install.o \
../../../../components/services/trilobite/libtrilobite/.libs/libtrilobite.so -L/gnome/lib -L/usr/lib \
../../../../components/services/install/lib/libinstall.a -Wl,--rpath -Wl,/gnome/lib \
-lgnome -lglib -ldl -lghttp -lrpm -lz -lbz2 -lpopt
 */

#include <config.h>
#include <gnome.h>
#include <liboaf/liboaf.h>
#include <bonobo.h>
#include <sys/utsname.h>

#include "eazel-install-public.h"
#include "eazel-install-xml-package-list.h"
#include <libtrilobite/helixcode-utils.h>

#define PACKAGE_FILE_NAME "package-list.xml"
#define DEFAULT_CONFIG_FILE "/var/eazel/services/eazel-services-config.xml"

#define DEFAULT_HOSTNAME "toothgnasher.eazel.com"
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


/* Popt stuff */
int     arg_dry_run,
	arg_http,
	arg_ftp,
	arg_local,
	arg_debug,
	arg_port;
char    *arg_server,
	*arg_config_file,
	*arg_local_list, 
	*arg_tmp_dir,
	*arg_input_list;

static const struct poptOption options[] = {
	{"debug", '\0', POPT_ARG_NONE, &arg_debug, 0 , N_("Show debug output"), NULL},
	{"port", '\0', POPT_ARG_NONE, &arg_port, 0 , N_("Set port numer (80)"), NULL},
	{"test", 't', POPT_ARG_NONE, &arg_dry_run, 0, N_("Test run"), NULL},
	{"tmp", '\0', POPT_ARG_STRING, &arg_tmp_dir, 0, N_("Set tmp dir (/tmp/eazel-install)"), NULL},
	{"server", '\0', POPT_ARG_STRING, &arg_server, 0, N_("Specify server"), NULL},
	{"http", 'h', POPT_ARG_NONE, &arg_http, 0, N_("Use http"), NULL},
	{"ftp", 'f', POPT_ARG_NONE, &arg_ftp, 0, N_("Use ftp"), NULL},
	{"local", 'l', POPT_ARG_NONE, &arg_local, 0, N_("Use local"), NULL},
	{"packagelist", '\0', POPT_ARG_STRING, &arg_local_list, 0, N_("Specify package list to use (/var/eazel/service/package-list.xml)"), NULL},
	{"config", '\0', POPT_ARG_STRING, &arg_config_file, 0, N_("Specify config file (/var/eazel/services/eazel-services-config.xml)"), NULL},
	{"genpkglist", '\0', POPT_ARG_STRING, &arg_input_list, 0, N_("Use specified file to generate a package list, requires --packagelist"), NULL},
	{NULL, '\0', 0, NULL, 0}
};

static void
set_parameters_from_command_line (EazelInstall *service)
{
	if (!arg_debug) {
		eazel_install_open_log (service, DEFAULT_LOG_FILE); 
	}

	/* We only want 1 protocol type */
	if (arg_http + arg_ftp + arg_local > 1) {
			fprintf (stderr, "*** You cannot specify more then one protocol type.\n");
			exit (1);
	}
	if (arg_http) {
		eazel_install_set_protocol (service, PROTOCOL_HTTP);
	} else if (arg_ftp) {
		eazel_install_set_protocol (service, PROTOCOL_FTP);
	} else if (arg_local) {
		eazel_install_set_protocol (service, PROTOCOL_LOCAL);
	} else {
		eazel_install_set_protocol (service, PROTOCOL_HTTP);
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
		eazel_install_set_test (service, TRUE);
	}
	if (arg_local_list) {
		eazel_install_set_package_list (service, arg_local_list);
	}
	if (arg_input_list) {
		if (arg_local_list == NULL) {
			fprintf (stderr, "Use of --genpkglist requires --packagelist\n");
			exit (1);
		}
		generate_xml_package_list (arg_input_list, arg_local_list);
	}

	eazel_install_set_server (service, arg_server);
	eazel_install_set_rpmrc_file (service, DEFAULT_RPMRC);
	eazel_install_set_package_list_storage_path (service, DEFAULT_REMOTE_PACKAGE_LIST);
	eazel_install_set_rpm_storage_path (service, DEFAULT_REMOTE_RPM_DIR);
	eazel_install_set_tmp_dir (service, arg_tmp_dir);
	eazel_install_set_server_port (service, arg_port);
}

static void 
eazel_install_progress_signal (EazelInstall *service, 
			       const PackageData *pack,
			       int a, int b,
			       int amount, 
			       int total,
			       int c, int d,
			       char *title) 
{
	fprintf (stdout, "%s - %s %% %f\r", title, pack->name, (total ? ((float)
									 ((((float) amount) / total) * 100))
								: 100.0));
	fflush (stdout);
	if (amount == total && total!=0) {
		fprintf (stdout, "\n");
	}
}

static void 
eazel_download_progress_signal (EazelInstall *service, 
				const PackageData *pack,
				int amount, 
				int total,
				char *title) 
{
	fprintf (stdout, "%s - %s %% %f\r", title, pack->name, (total ? ((float)
									 ((((float) amount) / total) * 100))
								: 100.0));
	fflush (stdout);
	if (amount == total && total!=0) {
		fprintf (stdout, "\n");
	}
}

static void
download_failed (EazelInstall *service, 
		 const char *name,
		 const gpointer info, 
		 gpointer unused)
{
	fprintf (stdout, "Download of %s FAILED\n", name);
}

/*
  This dumps the entire tree for the failed package.
 */
static void
install_failed (EazelInstall *service,
		const PackageData *pd,
		gchar *indent)
{
	GList *iterator;

	if (pd->toplevel) {
		fprintf (stdout, "\n***The package %s failed. Here's the dep tree\n", pd->name);
	}
	switch (pd->status) {
	case PACKAGE_DEPENDENCY_FAIL:
		fprintf (stdout, "%s-%s FAILED\n", indent, rpmfilename_from_packagedata (pd));
		break;
	case PACKAGE_CANNOT_OPEN:
		fprintf (stdout, "%s-%s NOT FOUND\n", indent, rpmfilename_from_packagedata (pd));
		break;		
	case PACKAGE_SOURCE_NOT_SUPPORTED:
		fprintf (stdout, "%s-%s is a source package\n", indent, rpmfilename_from_packagedata (pd));
		break;
	case PACKAGE_BREAKS_DEPENDENCY:
		fprintf (stdout, "%s-%s breaks\n", indent, rpmfilename_from_packagedata (pd));
		break;
	default:
		fprintf (stdout, "%s-%s\n", indent, rpmfilename_from_packagedata (pd));
		break;
	}
	for (iterator = pd->soft_depends; iterator; iterator = iterator->next) {			
		PackageData *pack;
		char *indent2;
		indent2 = g_strconcat (indent, iterator->next ? " |" : "  " , NULL);
		pack = (PackageData*)iterator->data;
		install_failed (service, pack, indent2);
		g_free (indent2);
	}
	for (iterator = pd->breaks; iterator; iterator = iterator->next) {			
		PackageData *pack;
		char *indent2;
		indent2 = g_strconcat (indent, iterator->next ? " |" : "  " , NULL);
		pack = (PackageData*)iterator->data;
		install_failed (service, pack, indent2);
		g_free (indent2);
	}
}

static void
dep_check (EazelInstall *service,
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
	pack->name = g_strdup (name);
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

int main(int argc, char *argv[]) {
	EazelInstall *service;
	poptContext ctxt;
	GList *packages;
	GList *categories;
	char *str;

	gtk_type_init ();
	gnomelib_init ("Eazel Install", "1.0");
	gnomelib_register_popt_table (options, "Eazel Install");
	ctxt = gnomelib_parse_args (argc, argv, 0);

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


	/* Get the config file, and create the object */
	if (arg_config_file == NULL) {
		arg_config_file = g_strdup (DEFAULT_CONFIG_FILE);
	}
	service = eazel_install_new_with_config (arg_config_file);
	g_assert (service != NULL);
	set_parameters_from_command_line (service);

	/* Bind the callbacks */
	gtk_signal_connect (GTK_OBJECT (service), "download_progress", eazel_download_progress_signal, "Download progress");
	gtk_signal_connect (GTK_OBJECT (service), "install_progress", eazel_install_progress_signal, "Install progress");
	gtk_signal_connect (GTK_OBJECT (service), "install_failed", install_failed, "");
	gtk_signal_connect (GTK_OBJECT (service), "download_failed", download_failed, NULL);
	gtk_signal_connect (GTK_OBJECT (service), "dependency_check", dep_check, NULL);

	eazel_install_install_packages (service, categories);
							
	eazel_install_destroy (GTK_OBJECT (service));

	return 0;
};
