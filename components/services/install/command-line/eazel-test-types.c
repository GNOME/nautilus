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

#include <libtrilobite/trilobite-root-helper.h>

static PackageData*
make_package (char *name, char *version, char *minor)
{
	PackageData *p;

	p = packagedata_new ();
	p->name = name ? g_strdup (name) : NULL;
	p->version = version ? g_strdup (version) : NULL;
	p->minor = minor ? g_strdup (minor) : NULL;

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
	if (g_list_length (in) == 4) {
		g_message ("packagedata_list_prune ok");
	} else {
		g_message ("packagedata_list_prune FAIL");
	}
}

#define EQ_TEST(b,a,order) do {                                                                       \
int res = eazel_install_package_matches_versioning (b, a->version, a->minor, EAZEL_SOFTCAT_SENSE_EQ); \
if (res) {                                                                                            \
	g_message ("matches_versioning (%s == %s) = %d %s",                                           \
		   packagedata_get_name (b), packagedata_get_name (a), res,                           \
		   order ? "ok" : "fail");                                                            \
} else {                                                                                              \
	g_message ("matches_versioning (%s == %s) = %d %s",                                           \
		   packagedata_get_name (b), packagedata_get_name (a), res,                           \
		   !order ? "ok" : "fail");                                                           \
} } while (0)

#define GE_TEST(b,a,order) do {                                                                       \
int res = eazel_install_package_matches_versioning (b, a->version, a->minor, EAZEL_SOFTCAT_SENSE_GE); \
if (res) {                                                                                            \
	g_message ("matches_versioning (%s >= %s) = %d %s",                                           \
		   packagedata_get_name (b), packagedata_get_name (a), res,                           \
		   order ? "ok" : "fail");                                                            \
} else {                                                                                              \
	g_message ("matches_versioning (%s >= %s) = %d %s",                                           \
		   packagedata_get_name (b), packagedata_get_name (a),                                \
		   res, !order ? "ok" : "fail");                                                      \
} } while (0)

static void
test_eazel_install_package_matches_versioning (void)
{
	PackageData *a, *b;

	a = make_package ("odder", NULL, NULL);
	b = make_package ("odder", "1.0", "1");
	EQ_TEST(b, a, 1);

	a = make_package ("odder", "1.0", NULL);
	b = make_package ("odder", "1.0", "1");
	EQ_TEST(b, a, 1);

	a = make_package ("odder", "1.0", "1");
	b = make_package ("odder", "1.0", "1");
	EQ_TEST(b, a, 1);

	a = make_package ("odder", "1.1", NULL);
	b = make_package ("odder", "1.0", "1");
	EQ_TEST(b, a, 0);

	a = make_package ("odder", "1.1", "2");
	b = make_package ("odder", "1.0", "1");
	EQ_TEST(b, a, 0);

	a = make_package ("odder", "1.0", "2");
	b = make_package ("odder", "1.0", "1");
	EQ_TEST(b, a, 0);

	a = make_package ("odder", "1.1", "1");
	b = make_package ("odder", "1.0", "1");
	EQ_TEST(b,a, 0);

	/* EQ | GT */

	a = make_package ("odder", NULL, NULL);
	b = make_package ("odder", "1.0", "1");
	GE_TEST(b,a, 1);

	a = make_package ("odder", "1.0", NULL);
	b = make_package ("odder", "1.0", "1");
	GE_TEST(b,a, 1);

	a = make_package ("odder", "1.0", "1");
	b = make_package ("odder", "1.0", "1");
	GE_TEST(b,a, 1);

	a = make_package ("odder", "1.0", NULL);
	b = make_package ("odder", "1.1", "1");
	GE_TEST(b,a, 1);

	a = make_package ("odder", "1.0", "2");
	b = make_package ("odder", "1.1", "1");
	GE_TEST(b,a, 1);

	a = make_package ("odder", "1.0", "1");
	b = make_package ("odder", "1.0", "2");
	GE_TEST(b,a, 1);

	a = make_package ("odder", "1.1", "1");
	b = make_package ("odder", "1.0", "1");
	GE_TEST(b,a, 0);

	a = make_package ("odder", "1.1", "2");
	b = make_package ("odder", "1.0", "1");
	GE_TEST(b,a, 0);
	
	a = make_package ("odder", "1.1", "1");
	b = make_package ("odder", "1.0", "2");
	GE_TEST(b,a, 0);
}

int main(int argc, char *argv[]) {
	gnome_init ("Eazel Test Packsys", "1.0", argc, argv);

	test_packagelist_prune ();
	test_eazel_install_package_matches_versioning ();

	return 0;
};
