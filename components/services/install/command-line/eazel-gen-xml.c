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

#include <eazel-install-xml-package-list.h>

#define PACKAGE_FILE_NAME "package-list.xml"

int arg_debug;
char    *arg_local_list, 
	*arg_input_list;

CORBA_ORB orb;
CORBA_Environment ev;

static const struct poptOption options[] = {
	{"in", '\0', POPT_ARG_STRING, &arg_input_list, 0, N_("Specify package list to use (/var/eazel/service/package-list.xml)"), NULL},
	{"out", '\0', POPT_ARG_STRING, &arg_local_list, 0, N_("Use specified file to generate a package list, requires --packagelist"), NULL},
	{"debug", '\0', POPT_ARG_NONE, &arg_debug, 0 , N_("Show debug output"), NULL},
	{NULL, '\0', 0, NULL, 0}
};

int main(int argc, char *argv[]) {
	gtk_type_init ();
	gnomelib_init ("Eazel Gen Xml", "1.0");
	gnomelib_register_popt_table (options, "Eazel Gen Xml");
	gnomelib_parse_args (argc, argv, 0);

	if (arg_input_list==NULL) {
		g_error ("Please specify an input file");
	}
	if (arg_local_list==NULL) {
		arg_local_list = g_strdup (PACKAGE_FILE_NAME);
	}
	
	generate_xml_package_list (arg_input_list, arg_local_list);
       
	return 0;
};
