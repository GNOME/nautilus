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
 * Authors: Robey Pointer <robey@eazel.com>
 *
 */

/*
  Test command of the day :

eazel-softcat --debug --server=services.eazel.com:80 --funk=100 \
id:3434 id:8934 id:8833 id:4544 id:3443 id:8452 id:6983 id:4599 id:9828 id:5456 id:9828 \
feat:libc.so.6 feat:libm.so.6 feat:libroev.so.69 \
nautilus xmms xfig transfig bladeenc lame grip ammonite gnumeric libghttp libtiff emacs glib

 */

#include <config.h>
#include <gnome.h>
#include <libtrilobite/libtrilobite.h>
#include <eazel-softcat.h>

char *arg_server = NULL;
char *arg_cgi_path = NULL;
char *arg_username = NULL;
char *arg_version = NULL;
int arg_debug = 0;
int arg_by_id = 0;
int arg_by_features = 0;
int arg_retries = 0;
int arg_delay = 0;
int arg_verbose = 0;
int arg_version_ge = 0;
int arg_check = 0;
int arg_funk = 0;

static const struct poptOption options[] = {
	{"server", 's', POPT_ARG_STRING, &arg_server, 0, N_("Softcat server to connect to"), "server[:port]"},
	{"cgi-path", '\0', POPT_ARG_STRING, &arg_cgi_path, 0, N_("Use alternate CGI path"), "path"},
	{"debug", '\0', POPT_ARG_NONE, &arg_debug, 0, N_("Show debug output"), NULL},
	{"user", 'u', POPT_ARG_STRING, &arg_username, 0, N_("Connect as a softcat user through ammonite"), "username"},
	{"retry", 'r', POPT_ARG_INT, &arg_retries, 0, N_("Number of times to try the request"), "times"},
	{"delay", 'd', POPT_ARG_INT, &arg_delay, 0, N_("Delay between request retries, in usec"), "delay"},
	{"by-id", 'i', POPT_ARG_NONE, &arg_by_id, 0, N_("Lookup by Eazel package id"), NULL},
	{"by-features", 'p', POPT_ARG_NONE, &arg_by_features, 0, N_("Lookup package that features a feature/file"), NULL},
	{"version", 'V', POPT_ARG_STRING, &arg_version, 0, N_("Lookup package with a specific version"), "version"},
	{"ge", '\0', POPT_ARG_NONE, &arg_version_ge, 0, N_("(with --version) Use >= comparison"), NULL},
	{"check", 'C', POPT_ARG_NONE, &arg_check, 0, N_("use check function (for debugging)"), NULL},
	{"verbose", 'v', POPT_ARG_NONE, &arg_verbose, 0, N_("Show detailed sub-package info"), NULL},
	{"funk", '\0', POPT_ARG_INT, &arg_funk, 0, N_("enable funk parser"), NULL},
	{NULL, '\0', 0, NULL, 0}
};



int
main (int argc, char **argv)
{
	poptContext popt;
	EazelSoftCat *softcat;
	const char *username;
	PackageData *package = NULL, *newpack = NULL;
	char *name;
	char *info;
	GList *package_list;
	EazelSoftCatError err;
	int sense_flags = 0;

	gnome_init_with_popt_table ("eazel-test-softcat", "1.0", argc, argv, options, 0, &popt);
	trilobite_set_log_handler (stdout, G_LOG_DOMAIN);
	trilobite_set_debug_mode (arg_debug ? TRUE : FALSE);

	package_list = NULL;
	while ((name = poptGetArg (popt)) != NULL) {
		package = packagedata_new ();
		
		if (arg_funk) {
			char *ptr = name;
			if (strncmp (ptr, "id:", 3)==0) {
				package->eazel_id = g_strdup (strchr (name, ':')+1);
			} else if (strncmp (ptr, "feat:", 5)==0) {
				package->features = g_list_prepend (package->features, g_strdup (strchr (name, ':')+1));
			} else {
				package->name = g_strdup (name);
			} 
		} else {			
			if (arg_by_id) {
				package->eazel_id = g_strdup (name);
			} else if (arg_by_features) {
				package->features = g_list_prepend (package->features, g_strdup (name));
			} else {
				package->name = g_strdup (name);
			}
			
			if (arg_version != NULL) {
				package->version = g_strdup (arg_version);
				sense_flags = (arg_version_ge ? EAZEL_SOFTCAT_SENSE_GE : EAZEL_SOFTCAT_SENSE_EQ);
			}
		}

		package_list = g_list_prepend (package_list, package);
	}
	package_list = g_list_reverse (package_list);

	if (package_list == NULL) {
		printf ("No packages requested.\n");
		exit (1);
	}

	softcat = eazel_softcat_new ();
	if (arg_server != NULL) {
		eazel_softcat_set_server (softcat, arg_server);
	}
	if (arg_cgi_path != NULL) {
		eazel_softcat_set_cgi_path (softcat, arg_cgi_path);
	}
	if (arg_username != NULL) {
		eazel_softcat_set_authn (softcat, TRUE, arg_username);
	}
	eazel_softcat_set_retry (softcat, arg_retries, arg_delay);

	if (arg_funk) {
		GList *iterator;
		GList *out=NULL, *err=NULL;
		printf ("Contacting softcat server at %s using funk technology ", eazel_softcat_get_server (softcat));
		if (eazel_softcat_get_authn (softcat, &username)) {
			printf ("(user: %s) ", username);
		}
		printf ("...\n");
		eazel_softcat_set_packages_pr_query (softcat, arg_funk);
		eazel_softcat_get_info_plural (softcat, package_list, 
					       &out, &err,
					       sense_flags, PACKAGE_FILL_EVERYTHING);


		for (iterator = err; iterator; iterator = g_list_next (iterator)) {
			PackageData *p = PACKAGEDATA (iterator->data);
			printf ("\n");
			info = packagedata_get_readable_name (p);
			printf ("Failed : %s\n", info);
			g_free (info);
		}
		for (iterator = out; iterator; iterator = g_list_next (iterator)) {
			PackageData *p = PACKAGEDATA (iterator->data);
			printf ("\n");
			info = packagedata_dump (p, arg_verbose ? TRUE : FALSE);
			printf ("%s\n", info);
			g_free (info);
		}
		return 0;
	} else while (package_list != NULL) {
		printf ("Contacting softcat server at %s ", eazel_softcat_get_server (softcat));
		if (eazel_softcat_get_authn (softcat, &username)) {
			printf ("(user: %s) ", username);
		}
		printf ("...\n");

		package = (PackageData *)(package_list->data);
		if (arg_check) {
			if (eazel_softcat_available_update (softcat, package, &newpack, PACKAGE_FILL_EVERYTHING)) {
				printf ("New package available!\n");
				info = packagedata_dump (newpack, arg_verbose ? TRUE : FALSE);
				printf ("%s\n", info);
				g_free (info);
				gtk_object_unref (GTK_OBJECT (newpack));
			} else {
				printf ("No new package available.\n");
			}
		} else {
			err = eazel_softcat_get_info (softcat, package, sense_flags, PACKAGE_FILL_EVERYTHING);
			if (err != EAZEL_SOFTCAT_SUCCESS) {
				printf ("FAILED: %s\n\n", eazel_softcat_error_string (err));
			} else {
				printf ("\n");
				info = packagedata_dump (package, arg_verbose ? TRUE : FALSE);
				printf ("%s\n", info);
				g_free (info);
			}
		}

		package_list = g_list_remove (package_list, package);
		gtk_object_unref (GTK_OBJECT (package));
	}

	return 0;
}
