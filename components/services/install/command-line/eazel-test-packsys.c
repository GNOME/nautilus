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
#include <eazel-package-system.h>

static PackageData*
make_package (char *name, char *version, char *minor)
{
	PackageData *p;
	p = packagedata_new ();
	p->name = g_strdup (name);
	p->version = g_strdup (version);
	p->minor = g_strdup (minor);
	return p;
}

static void
test_packagelist_prune (void)
{
	PackageData *p, *q;
	GList *in = NULL, *rm = NULL;

	p = make_package ("hest", "1.0", "1");
	q = make_package ("hest", "1.1", "1");
	in = g_list_prepend (in, p);
	rm = g_list_prepend (rm, q);

	p = make_package ("fisk", "1.0", "1");
	q = make_package ("fisk", "1.0", "2");
	in = g_list_prepend (in, p);
	rm = g_list_prepend (rm, q);

	p = make_package ("gris", "1.0", "1");
	q = make_package ("gris", "1.0", "1");
	in = g_list_prepend (in, p);
	rm = g_list_prepend (rm, q);

	p = make_package ("odder", "1.0", "1");
	in = g_list_prepend (in, p);
	p = make_package ("bæver", "1.0", "1");
	in = g_list_prepend (in, p);
	p = make_package ("kanin", "1.0", "1");
	in = g_list_prepend (in, p);
	p = make_package ("osteklokke", "1.0", "1");
	in = g_list_prepend (in, p);

	packagedata_list_prune (&in, rm, TRUE, TRUE);
	if (g_list_length (in) == 2) {
		g_message ("packagedata_list_prune ok");
	} else {
		g_message ("packagedata_list_prune FAIL");
	}
}

static void
test_package_load (EazelPackageSystem *packsys,
		   const char *file) 
{
	PackageData *p;
	int flag;

	flag = 0;
	p = eazel_package_system_load_package (packsys, NULL, file, flag);
	if (p->description==NULL && p->summary==NULL && p->provides==NULL) {
		g_message ("load_package test 1 ok");
	} else {
		g_message ("load_package test 1 FAIL");
	}

	packagedata_destroy (p, TRUE);

	flag = EAZEL_INSTALL_PACKAGE_SYSTEM_QUERY_DETAIL_DESCRIPTION;
	p = eazel_package_system_load_package (packsys, NULL, file, flag);
	if (p->description && p->summary==NULL && p->provides==NULL) {
		g_message ("load_package test 2 ok");
	} else {
		g_message ("load_package test 2 FAIL");
	}
	packagedata_destroy (p, TRUE);

	flag = EAZEL_INSTALL_PACKAGE_SYSTEM_QUERY_DETAIL_SUMMARY;
	p = eazel_package_system_load_package (packsys, NULL, file, flag);
	if (p->description==NULL && p->summary && p->provides==NULL) {
		g_message ("load_package test 3 ok");
	} else {
		g_message ("load_package test 3 FAIL");
	}
	packagedata_destroy (p, TRUE);

	flag = EAZEL_INSTALL_PACKAGE_SYSTEM_QUERY_DETAIL_FILES_PROVIDED;
	p = eazel_package_system_load_package (packsys, NULL, file, flag);
	if (p->description==NULL && p->summary==NULL && p->provides) {
		g_message ("load_package test 4 ok");
	} else {
		g_message ("load_package test 4 FAIL");
	}
	packagedata_destroy (p, TRUE);
}

static EazelPackageSystem*
init_package_system (void)
{
	EazelPackageSystem *packsys;
	GList *roots = NULL;
	char *tmp;

	tmp = g_strdup_printf ("%s/.nautilus/package-db", g_get_home_dir ());

	roots = g_list_prepend (roots, tmp);

	packsys = eazel_package_system_new (roots);

	return packsys;
}

static void
test_matches_query (EazelPackageSystem *packsys) 
{
	GList *result;

	result = eazel_package_system_query (packsys,
					     NULL,
					     "rpm",
					     EAZEL_PACKAGE_SYSTEM_QUERY_MATCHES,
					     0);
	if (g_list_length (result)==1) {
		g_message ("Query matches ok");
	} else {
		g_message ("Query matches fail");
	}
	g_list_free (result);
	
	result = eazel_package_system_query (packsys,
					     NULL,
					     "gnome",
					     EAZEL_PACKAGE_SYSTEM_QUERY_SUBSTR,
					     0);
	if (g_list_length (result)>10) {
		g_message ("Query substr ok");
	} else {
		g_message ("Query substr fail");
	}
	
/*
	{
		GList *iterator;
		
		for (iterator = result; iterator; iterator = g_list_next (iterator)) {
			PackageData *pack = (PackageData*)iterator->data;
			char *tmp = packagedata_get_readable_name (pack);
			g_message ("pacakge %s", tmp);
			g_free (tmp);
		}
	}
*/
	g_list_free (result);
}

int main(int argc, char *argv[]) {
	EazelPackageSystem *packsys;

	if (argc <=1 ) {
		g_error ("usage: %s <rpmfile>", argv[0]);
	}

	gnome_init ("Eazel Test Packsys", "1.0", argc, argv);
	packsys = init_package_system ();

	test_package_load (packsys, argv[1]);
	test_packagelist_prune ();
	test_matches_query (packsys);

	return 0;
};
