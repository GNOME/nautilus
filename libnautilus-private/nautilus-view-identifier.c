/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-view-identifier.c: Unique ID/Human-readable name pairs for views
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Maciej Stachowiak <mjs@eazel.com>
*/

#include <config.h>
#include "nautilus-view-identifier.h"

#include <libgnome/gnome-i18n.h>


#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>
#include <glib.h>
#include <stdlib.h>

static NautilusViewIdentifier *
nautilus_view_identifier_new (const char *iid,
			      const char *name,
			      const char *view_as_label,
			      const char *view_as_label_with_mnemonic,
			      const char *label_viewer);



NautilusViewIdentifier *
nautilus_view_identifier_new (const char *iid, 
			      const char *name,
			      const char *view_as_label,
			      const char *view_as_label_with_mnemonic,
			      const char *viewer_label)
{
        NautilusViewIdentifier *new_identifier;
        
        g_return_val_if_fail (iid != NULL, NULL);
        g_return_val_if_fail (name != NULL, NULL);
        
        new_identifier = g_new0 (NautilusViewIdentifier, 1);
        new_identifier->iid = g_strdup (iid);
        new_identifier->name = g_strdup (name);

        new_identifier->view_as_label = view_as_label ? g_strdup (view_as_label) :
		g_strdup_printf (_("View as %s"), name);

        new_identifier->view_as_label_with_mnemonic = view_as_label_with_mnemonic ? g_strdup (view_as_label_with_mnemonic) 
		: g_strdup (new_identifier->view_as_label);

        new_identifier->viewer_label = view_as_label ? g_strdup (viewer_label) :
		g_strdup_printf (_("%s Viewer"), name);
        
        return new_identifier;
}

NautilusViewIdentifier *
nautilus_view_identifier_copy (const NautilusViewIdentifier *identifier)
{
	if (identifier == NULL) {
		return NULL;
	}
	
	return nautilus_view_identifier_new (identifier->iid, 
					     identifier->name, 
					     identifier->view_as_label,
					     identifier->view_as_label_with_mnemonic,
					     identifier->viewer_label);
}

/* Returns a list of languages, containing
   the LANG or LANGUAGE environment setting (with and without region code).
   The elements in the returned list must be freed */
static GSList *
get_lang_list (void)
{
   	GSList *retval;
	const GList *l;
	const char *lang;

	retval = NULL;
	
	for (l = gnome_i18n_get_language_list (NULL);
	     l != NULL;
	     l = g_list_next (l)) {
		lang = l->data;
		/* skip C locale */
		if (l->data && strcmp (lang, "C") == 0) {
			continue;
		}
		
		if (!eel_str_is_empty (lang)) {
			retval = g_slist_prepend (retval, g_strdup (lang));
		}
	}
        return retval;
}

NautilusViewIdentifier *
nautilus_view_identifier_new_from_bonobo_server_info (Bonobo_ServerInfo *server, char *name_attribute)
{
        const char *view_as_name;       
        const char *view_as_label;       
	const char *view_as_label_with_mnemonic;
        const char *viewer_label;       
        GSList *langs;

        langs = get_lang_list ();

        view_as_name = bonobo_server_info_prop_lookup (server, name_attribute, langs);
	view_as_label = bonobo_server_info_prop_lookup (server, "nautilus:view_as_label", langs);
	view_as_label_with_mnemonic = bonobo_server_info_prop_lookup (server, "nautilus:view_as_label_with_mnemonic", langs);
	viewer_label = bonobo_server_info_prop_lookup (server, "nautilus:viewer_label", langs);

        if (view_as_name == NULL) {
                view_as_name = bonobo_server_info_prop_lookup (server, "name", langs);
        }
        if (view_as_name == NULL) {
                view_as_name = server->iid;
        }

	eel_g_slist_free_deep (langs);

	/* if the name is an OAFIID, clean it up for display */
	if (eel_str_has_prefix (view_as_name, "OAFIID:")) {
		char *display_name, *colon_ptr;
		NautilusViewIdentifier *new_identifier;
		
		display_name = g_strdup (view_as_name + 7);
		colon_ptr = strchr (display_name, ':');
		if (colon_ptr) {
			*colon_ptr = '\0';
		}
		
		new_identifier = nautilus_view_identifier_new (server->iid, display_name,
							       view_as_label,
							       view_as_label_with_mnemonic,
							       viewer_label);
		g_free(display_name);
		return new_identifier;					
	}
		
        return nautilus_view_identifier_new (server->iid, view_as_name,
					     view_as_label, 
					     view_as_label_with_mnemonic,
					     viewer_label);
}

NautilusViewIdentifier *
nautilus_view_identifier_new_from_content_view (Bonobo_ServerInfo *server)
{
	return nautilus_view_identifier_new_from_bonobo_server_info
		(server, "nautilus:view_as_name");
}

NautilusViewIdentifier *
nautilus_view_identifier_new_from_property_page (Bonobo_ServerInfo *server)
{
	return nautilus_view_identifier_new_from_bonobo_server_info
		(server, "nautilus:property_page_name");
}

NautilusViewIdentifier *
nautilus_view_identifier_new_from_sidebar_panel (Bonobo_ServerInfo *server)
{
	return nautilus_view_identifier_new_from_bonobo_server_info
		(server, "nautilus:sidebar_panel_name");
}

void
nautilus_view_identifier_free (NautilusViewIdentifier *identifier)
{
        if (identifier != NULL) {
                g_free (identifier->iid);
                g_free (identifier->name);
                g_free (identifier->view_as_label);
		g_free (identifier->view_as_label_with_mnemonic);
                g_free (identifier->viewer_label);
                g_free (identifier);
        }
}

GList *
nautilus_view_identifier_list_copy (GList *list)
{
	GList *copy, *node;

	copy = NULL;
	for (node = list; node != NULL; node = node->next) {
		copy = g_list_prepend
			(copy, nautilus_view_identifier_copy (node->data));
	}
	return g_list_reverse (copy);
}

static void
nautilus_view_identifier_free_callback (gpointer identifier, gpointer ignore)
{
	g_assert (ignore == NULL);
	nautilus_view_identifier_free (identifier);
}

void
nautilus_view_identifier_list_free (GList *list)
{
	eel_g_list_free_deep_custom
		(list, nautilus_view_identifier_free_callback, NULL);
}

int
nautilus_view_identifier_compare (const NautilusViewIdentifier *a,
				  const NautilusViewIdentifier *b)
{
	int result;

	result = strcmp (a->iid, b->iid);
	if (result != 0) {
		return result;
	}
	return strcmp (a->name, b->name);
}
