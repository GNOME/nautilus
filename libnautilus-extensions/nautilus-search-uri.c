/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-search-uri.c -- tools for creating
   and parsing search uris 

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
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Rebecca Schulman <rebecka@eazel.com>
*/

#include <string.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include "nautilus-search-uri.h"

static NautilusSearchBarMode      other_search_mode        (NautilusSearchBarMode mode);

char*                      
nautilus_search_uri_to_simple_search_criteria (const char *uri)
{
	return "";

}

char *
nautilus_simple_search_criteria_to_search_uri (const char *search_criteria)
{
	char **words;
	char *search_uri;
	char *fragment;
	char *escaped_fragment;
	int length, i; 

	g_return_val_if_fail (search_criteria != NULL, NULL);

	words = g_new0 (char *, strlen (search_criteria));
	words = g_strsplit (search_criteria, " ", strlen (search_criteria));
	/* FIXME: this should eventually be: length = strlen ("[file%3A%2F%2F%2F]"); */
	length = strlen ("[file:///]");
	/* Count total length */
	for (i = 0; words[i] != NULL; i++) {
		length += strlen (words[i]) + strlen ("file_name contains & ");
	}
	fragment = g_new0 (char, length);
	/* FIXME: this should eventually be: sprintf (fragment, "[file%%3A%%2F%%2F%%2F]"); */
	sprintf (fragment, "[file:///]");
	if (words[0] != NULL) {
		for (i = 0; words[i+1] != NULL; i++) {
			strcat (fragment, "file_name contains ");
			strcat (fragment, words[i]);
			strcat (fragment, " & ");
		}
	strcat (fragment, "file_name contains ");
		strcat (fragment, words[i]);
	}
	escaped_fragment = gnome_vfs_escape_string (fragment);
	g_free (fragment);
	search_uri = g_strconcat ("search:", escaped_fragment, NULL);
	g_free (escaped_fragment);
#ifdef SEARCH_URI_DEBUG
	printf ("Made uri %s from simple search criteria %s\n",
		search_uri, search_criteria);
#endif
	return search_uri;
}


NautilusSearchBarMode 
nautilus_search_uri_to_search_bar_mode (const char *uri)
{
	NautilusSearchBarMode preferred_mode;

	preferred_mode = nautilus_preferences_get_enum (NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
							NAUTILUS_SIMPLE_SEARCH_BAR);
	if (nautilus_search_uri_is_displayable_by_mode (uri, preferred_mode)) {
		return preferred_mode;
	}
	else {
		return (other_search_mode (preferred_mode));
	}
}



gboolean
nautilus_search_uri_is_displayable_by_mode (const char *uri,
					    NautilusSearchBarMode mode)
{
	/* FIXME */
	return TRUE;
}


static NautilusSearchBarMode
other_search_mode (NautilusSearchBarMode mode)
{
	switch (mode) {
	case NAUTILUS_SIMPLE_SEARCH_BAR:
		return NAUTILUS_COMPLEX_SEARCH_BAR;
		break;
	case NAUTILUS_COMPLEX_SEARCH_BAR:
		return NAUTILUS_SIMPLE_SEARCH_BAR;
		break;
	default:
		g_assert_not_reached ();
	}
	return NAUTILUS_COMPLEX_SEARCH_BAR;
}
