/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-sidebar-functions.c - Sidebar functions used throughout Nautilus.

   Copyright (C) 2001 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#include <config.h>
#include "nautilus-sidebar-functions.h"

#include "nautilus-view-identifier.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>

static int
compare_view_identifiers (gconstpointer a, gconstpointer b)
{
 	NautilusViewIdentifier *idenfifier_a;
 	NautilusViewIdentifier *idenfifier_b;
	
 	g_assert (a != NULL);
 	g_assert (b != NULL);

 	idenfifier_a = (NautilusViewIdentifier*) a;
 	idenfifier_b = (NautilusViewIdentifier*) b;
	
	return eel_strcmp (idenfifier_a->name, idenfifier_b->name);
}

/* Make a query to find out what sidebar panels are available. */
static GList *
sidebar_get_sidebar_panel_view_identifiers (void)
{
	CORBA_Environment ev;
	const char *query;
        Bonobo_ServerInfoList *bonobo_activation_result;
	guint i;
	NautilusViewIdentifier *id;
	GList *view_identifiers;

	CORBA_exception_init (&ev);

	/* get all the sidebars, and ignore the "loser" ones */
	query = "nautilus:sidebar_panel_name.defined() AND repo_ids.has ('IDL:Bonobo/Control:1.0') AND (NOT test_only == true)";

	bonobo_activation_result = bonobo_activation_query (query, NULL, &ev);
		
	view_identifiers = NULL;

        if (ev._major == CORBA_NO_EXCEPTION && bonobo_activation_result != NULL) {
		for (i = 0; i < bonobo_activation_result->_length; i++) {
			id = nautilus_view_identifier_new_from_sidebar_panel
				(&bonobo_activation_result->_buffer[i]);
			view_identifiers = g_list_prepend (view_identifiers, id);
		}
		view_identifiers = g_list_reverse (view_identifiers);
	} 

	if (bonobo_activation_result != NULL) {
		CORBA_free (bonobo_activation_result);
	}
	
	CORBA_exception_free (&ev);

	view_identifiers = g_list_sort (view_identifiers, compare_view_identifiers);

	return view_identifiers;
}

GList *
nautilus_sidebar_get_all_sidebar_panel_view_identifiers (void)
{
	return sidebar_get_sidebar_panel_view_identifiers ();
}
