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
 * Authors: Ian McKellar <yakk@yakk.net.au>
 *
 */

#include <config.h>
#include <glib.h>
#include <popt-gnome.h>
#include <string.h>
#include "vault-operations.h"

#define _(X) X // eek! FIXME

gchar *vault_location;
gboolean debug = FALSE;
gchar *operation = NULL;

static const struct poptOption options[] = {
        {"debug", 'd', POPT_ARG_NONE, &debug, 0, _("Enable debugging"), NULL},
        {"uri", 'u', POPT_ARG_STRING, &vault_location, 0, _("Vault location"), NULL},
        {NULL, '\0', 0, NULL, 0} /* end the list */
};

static void valid_ops() {
	gint vopnum = 0;
	struct VaultOperation *vop;

	g_print(_("Valid operations:"));
	for(;;) {
		vop = &vault_operations[vopnum++];
		if(vop->name == NULL) {
			g_print(".\n");
			return;
		}
		g_print(" %s", vop->name);
	}
}

int main (int argc, char *argv[]) {
	poptContext pctx = poptGetContext("eazel-vault", argc, argv,
			options, 0);
	gint opt;
	gint vopnum = 0;
	struct VaultOperation *vop;

	vault_location = g_strdup("http://localhost/webdav/"); /* load from gconf */

        while ( (opt = poptGetNextOpt (pctx)) >= 0) {
		switch (opt) {
		case 'd':
			debug = TRUE;
			break;
		case 'u':
			g_free(vault_location);
			vault_location = g_strdup(poptGetArg(pctx));
		}
	}

	operation = g_strdup(poptGetArg(pctx));

#if 0
	g_print("vault_location = `%s'\n", vault_location);
	g_print("debug = `%d'\n", debug);
	g_print("operation = `%s'\n", operation);
#endif

	if(operation == NULL) {
		g_print(_("Error: No operation supplied\n"));
		valid_ops();
		exit(1);
	}

	for(;;) {
		vop = &vault_operations[vopnum++];
		if(vop->name == NULL) {
			g_print(_("Error: Invalid operation supplied (%s)\n"), operation);
			valid_ops();
			exit(1);
		}
		if(!strcasecmp(vop->name, operation)) {
			/* we've found a matching operation */
			GList *args = NULL;
			gint argcount = 0;
			gchar *arg;
			while((arg = poptGetArg(pctx))) {
				argcount++;
				args = g_list_append(args, arg);
			}
			if(argcount <= vop->maxargs && argcount >= vop->minargs) {
				GnomeVFSResult result;
				gchar *error_context = "eazel-vault";

				poptFreeContext (pctx);
				gnome_vfs_init ();
				result = (vop->function)(args, vault_location, debug, &error_context);
				if(result != GNOME_VFS_OK) {
					g_print("%s: %s\n", error_context, gnome_vfs_result_to_string(result));
				}
				return result;
			} else {
				g_print(_("Error: Invalid syntax\nSyntax: %s\n"), vop->syntax);
				exit(1);
			}

		}
	
	}	

	return 0;
};
