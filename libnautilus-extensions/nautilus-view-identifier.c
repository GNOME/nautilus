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

#include "nautilus-glib-extensions.h"
#include "nautilus-string.h"
#include <glib.h>
#include <stdlib.h>

NautilusViewIdentifier *
nautilus_view_identifier_new (const char *iid, const char *name)
{
        NautilusViewIdentifier *new_identifier;
        
        g_return_val_if_fail (iid != NULL, NULL);
        g_return_val_if_fail (name != NULL, NULL);
        
        new_identifier = g_new0 (NautilusViewIdentifier, 1);
        new_identifier->iid = g_strdup (iid);
        new_identifier->name = g_strdup (name);
        
        return new_identifier;
}

NautilusViewIdentifier *
nautilus_view_identifier_copy (NautilusViewIdentifier *identifier)
{
	if (identifier == NULL) {
		return NULL;
	}
	
	return nautilus_view_identifier_new (identifier->iid, identifier->name);
}


static GSList *
get_lang_list (void)
{
        GSList *retval;
        char *lang;
        char * equal_char;

        retval = NULL;

        lang = g_getenv ("LANGUAGE");

        if (!lang) {
                lang = g_getenv ("LANG");
        }


        if (lang) {
                equal_char = strchr (lang, '=');
                if (equal_char != NULL) {
                        lang = equal_char + 1;
                }

                retval = g_slist_prepend (retval, lang);
        }
        
        return retval;
}

NautilusViewIdentifier *
nautilus_view_identifier_new_from_oaf_server_info (OAF_ServerInfo *server, char *name_attribute)
{
        const char *view_as_name;       
        GSList *langs;

        langs = get_lang_list ();
        view_as_name = oaf_server_info_prop_lookup (server, name_attribute, langs);
		
        if (view_as_name == NULL) {
                view_as_name = oaf_server_info_prop_lookup (server, "name", langs);
        }

        if (view_as_name == NULL) {
                view_as_name = server->iid;
        }
       
        g_slist_free (langs);

	/* if the name is an OAFIID, clean it up for display */
	if (nautilus_str_has_prefix (view_as_name, "OAFIID:")) {
		char *display_name, *colon_ptr;
		NautilusViewIdentifier *new_identifier;
		
		display_name = g_strdup (view_as_name + 7);
		colon_ptr = strchr (display_name, ':');
		if (colon_ptr) {
			*colon_ptr = '\0';
		}
		
		new_identifier = nautilus_view_identifier_new (server->iid, display_name);
		g_free(display_name);
		return new_identifier;					
	}
		
        return nautilus_view_identifier_new (server->iid, view_as_name);
}

NautilusViewIdentifier *
nautilus_view_identifier_new_from_content_view (OAF_ServerInfo *server)
{
	return nautilus_view_identifier_new_from_oaf_server_info (server, 
								  "nautilus:view_as_name");
}

NautilusViewIdentifier *
nautilus_view_identifier_new_from_sidebar_panel (OAF_ServerInfo *server)
{
	return nautilus_view_identifier_new_from_oaf_server_info (server, 
								  "nautilus:sidebar_panel_name");
}

void
nautilus_view_identifier_free (NautilusViewIdentifier *identifier)
{
        if (identifier != NULL) {
                g_free (identifier->iid);
                g_free (identifier->name);
                g_free (identifier);
        }
}

static void
nautilus_view_identifier_free_callback (NautilusViewIdentifier *identifier, gpointer ignore)
{
	g_assert (ignore == NULL);
	nautilus_view_identifier_free (identifier);
}

void
nautilus_view_identifier_list_free (GList *identifiers)
{
	nautilus_g_list_free_deep_custom
		(identifiers,
		 (GFunc) nautilus_view_identifier_free_callback,
		 NULL);
}

int
nautilus_view_identifier_compare (NautilusViewIdentifier *a, NautilusViewIdentifier *b)
{
	return (strcmp (a->iid, b->iid) || strcmp (a->name, b->name));
} 
