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
 * Authors: J Shane Culpepper <pepper@eazel.com>
 *
 */

#include <config.h>

#include <gnome.h>
#include <popt-gnome.h>
#include <eazel-package-system.h>
#include <eazel-inventory-utils.h>
#include <libtrilobite/trilobite-md5-tools.h>

static const struct poptOption optionsTable[] = {
	{NULL, '\0', 0, NULL, 0}
};

int main (int argc, char *argv[])
{

	poptContext		pctx;
	gboolean		return_val;
	unsigned char		digest[16];
	char			*inventory_file;


	pctx = poptGetContext("Eazel Test Inventory", argc, argv, optionsTable, 0);

	poptFreeContext (pctx);

	gtk_type_init();

	inventory_file = g_strdup_printf ("%s/.nautilus/configuration.xml", g_get_home_dir ());

	/* initialize the inventory so that the next update will return the same hash value */
	return_val = eazel_gather_inventory ();
	/* do nothing with the return_val for now */

	g_print ("Updating GConf value.\n");
	trilobite_md5_get_digest_from_file (inventory_file, digest);
	return_val = update_gconf_inventory_digest (digest);	
	if (return_val == FALSE) {
		g_print ("test FAILED\n");
		exit (1);
	}

	/* Now gather inventory again and compare to GConf value.
	 * eazel_gather_inventory will return FALSE if the md5 digests
	 * match and TRUE if they are different.  This can be used to
	 * know when inventory needs to be synced.
	 */
	return_val = eazel_gather_inventory ();
	if (return_val == FALSE) {
		g_print ("test PASSED\n");
		exit (1);
	} else {
		g_print ("test FAILED\n");
	}

	return 0;
};
