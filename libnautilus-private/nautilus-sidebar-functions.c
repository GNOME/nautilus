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

#include "nautilus-global-preferences.h"
#include "nautilus-view-identifier.h"
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>
#include <liboaf/liboaf.h>

#define PREFERENCES_SIDEBAR_PANEL_PREFIX "sidebar-panels"

#define NEWS_PANEL_IID		"OAFIID:nautilus_news_view:041601"
#define NOTES_PANEL_IID		"OAFIID:nautilus_notes_view:7f04c3cb-df79-4b9a-a577-38b19ccd4185"
#define HELP_PANEL_IID		"OAFIID:hyperbola_navigation_tree:57542ce0-71ff-442d-a764-462c92514234"
#define HISTORY_PANEL_IID	"OAFIID:nautilus_history_view:a7a85bdd-2ecf-4bc1-be7c-ed328a29aacb"
#define TREE_PANEL_IID		"OAFIID:nautilus_tree_view:2d826a6e-1669-4a45-94b8-23d65d22802d"

const char nautilus_sidebar_news_enabled_preference_name[] = PREFERENCES_SIDEBAR_PANEL_PREFIX "/" NEWS_PANEL_IID;
const char nautilus_sidebar_notes_enabled_preference_name[] = PREFERENCES_SIDEBAR_PANEL_PREFIX "/" NOTES_PANEL_IID;
const char nautilus_sidebar_help_enabled_preference_name[] = PREFERENCES_SIDEBAR_PANEL_PREFIX "/" HELP_PANEL_IID;
const char nautilus_sidebar_history_enabled_preference_name[] = PREFERENCES_SIDEBAR_PANEL_PREFIX "/" HISTORY_PANEL_IID;
const char nautilus_sidebar_tree_enabled_preference_name[] = PREFERENCES_SIDEBAR_PANEL_PREFIX "/" TREE_PANEL_IID;

static char *
sidebar_panel_make_preference_key (const char *panel_iid)
{
	g_return_val_if_fail (panel_iid != NULL, NULL);

	return g_strdup_printf ("%s/%s", PREFERENCES_SIDEBAR_PANEL_PREFIX, panel_iid);
}

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

static gboolean
sidebar_is_sidebar_panel_enabled (NautilusViewIdentifier *panel_identifier)
{
	gboolean enabled;
        gchar  *key;
	
	g_return_val_if_fail (panel_identifier != NULL, FALSE);
	g_return_val_if_fail (panel_identifier->iid != NULL, FALSE);
	
	key = sidebar_panel_make_preference_key (panel_identifier->iid);
	g_return_val_if_fail (key != NULL, FALSE);
        enabled = eel_preferences_get_boolean (key);
        g_free (key);

        return enabled;
}

static gboolean
sidebar_is_sidebar_panel_enabled_cover (gpointer data, gpointer callback_data)
{
	return sidebar_is_sidebar_panel_enabled (data);
}

/* Make a query to find out what sidebar panels are available. */
static GList *
sidebar_get_sidebar_panel_view_identifiers (void)
{
	CORBA_Environment ev;
	const char *query;
        OAF_ServerInfoList *oaf_result;
	guint i;
	NautilusViewIdentifier *id;
	GList *view_identifiers;

	CORBA_exception_init (&ev);

	query = "nautilus:sidebar_panel_name.defined() AND repo_ids.has ('IDL:Bonobo/Control:1.0')";

	oaf_result = oaf_query (query, NULL, &ev);
		
	view_identifiers = NULL;

        if (ev._major == CORBA_NO_EXCEPTION && oaf_result != NULL) {
		for (i = 0; i < oaf_result->_length; i++) {
			id = nautilus_view_identifier_new_from_sidebar_panel
				(&oaf_result->_buffer[i]);
			view_identifiers = g_list_prepend (view_identifiers, id);
		}
		view_identifiers = g_list_reverse (view_identifiers);
	} 

	if (oaf_result != NULL) {
		CORBA_free (oaf_result);
	}
	
	CORBA_exception_free (&ev);

	view_identifiers = g_list_sort (view_identifiers, compare_view_identifiers);

	return view_identifiers;
}

GList *
nautilus_sidebar_get_enabled_sidebar_panel_view_identifiers (void)
{
	GList *enabled_view_identifiers;
 	GList *disabled_view_identifiers;
        
	enabled_view_identifiers = sidebar_get_sidebar_panel_view_identifiers ();

	enabled_view_identifiers = eel_g_list_partition (enabled_view_identifiers,
							      sidebar_is_sidebar_panel_enabled_cover,
							      NULL,
							      &disabled_view_identifiers);
	
	nautilus_view_identifier_list_free (disabled_view_identifiers);
	
        return enabled_view_identifiers;
}

void
nautilus_sidebar_for_each_panel (NautilusSidebarPanelCallback callback,
				 gpointer callback_data)
{
	char *preference_key;
	GList *view_identifiers;
	GList *node;
	NautilusViewIdentifier *identifier;

	g_return_if_fail (callback != NULL);

	view_identifiers = sidebar_get_sidebar_panel_view_identifiers ();

	for (node = view_identifiers; node != NULL; node = node->next) {
		g_assert (node->data != NULL);
		identifier = node->data;
		
		preference_key = sidebar_panel_make_preference_key (identifier->iid);

		(* callback) (identifier->name, identifier->iid, preference_key, callback_data);

		g_free (preference_key);
	}

	nautilus_view_identifier_list_free (view_identifiers);
}
