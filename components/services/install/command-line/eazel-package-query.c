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

#include <eazel-install-public.h>
#include <eazel-install-query.h>

#include <libtrilobite/helixcode-utils.h>

/* Popt stuff */
char *arg_query;
int arg_owns, arg_provides, arg_requires;

static const struct poptOption options[] = {
	{"query", 'q', POPT_ARG_STRING, &arg_query, 0, N_("Lookup package in the db"), NULL},
	{"owns", 'o', POPT_ARG_NONE, &arg_owns, 0, N_("Who owns specified files"), NULL}, 
	{"provides", 'p', POPT_ARG_NONE, &arg_provides, 0, N_("Who provides specified files"), NULL}, 
	{"requires", 'r', POPT_ARG_NONE, &arg_requires, 0, N_("Who requires specified packages"), NULL}, 
	{NULL, '\0', 0, NULL, 0}
};

int main(int argc, char *argv[]) {
	EazelInstall *service;
	poptContext ctxt;
	GList *packages;
	GList *categories;
	char *str;
	
#if 1
	gnome_init_with_popt_table ("Eazel Package Query", "1.0", argc, argv, options, 0, &ctxt);
	oaf_init (argc, argv);
#else
	gtk_type_init ();
	gnomelib_init ("Eazel Install", "1.0");
	gnomelib_register_popt_table (options, "Eazel Install");
	oaf_init (argc, argv);
	ctxt = gnomelib_parse_args (argc, argv, 0);
#endif

	if (bonobo_init (NULL, NULL, NULL) == FALSE) {
		g_error ("Could not init bonobo");
	}
	bonobo_activate ();

	packages = NULL;
	categories = NULL;
	service = eazel_install_new ();
	g_assert (service != NULL);

	/* If there are more args, get them and parse them as packages */
	while ((str = poptGetArg (ctxt)) != NULL) {
		GList *result;
		GList *iterator;
		EazelInstallSimpleQueryEnum flag;
		
		if (arg_owns) {
			flag = EI_SIMPLE_QUERY_OWNS;			
		} else if (arg_provides) {
			flag = EI_SIMPLE_QUERY_PROVIDES;
		} else if (arg_requires) {
			flag = EI_SIMPLE_QUERY_REQUIRES;
		} else {
			flag = EI_SIMPLE_QUERY_MATCHES;
		}
		
		result = eazel_install_simple_query (service, str, flag, 0, NULL);

		for (iterator = result; iterator; iterator = iterator->next) {
			PackageData *pack;
			pack = (PackageData*)iterator->data;
			
			fprintf (stdout, "Package : %s, %s (\"%40.40s\")\n", pack->name, pack->version, pack->summary);
			packagedata_destroy (pack);
		}
		g_list_free (result);
	}
							
	eazel_install_destroy (GTK_OBJECT (service));

	return 0;
};
