/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-simple-search-bar.c - One box Search bar for Nautilus

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

#include <config.h>
#include "nautilus-simple-search-bar.h"

#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>

struct NautilusSimpleSearchBarDetails {
	GtkEntry *entry;
};

static char * nautilus_simple_search_bar_get_location            (NautilusNavigationBar        *bar);
static void   nautilus_simple_search_bar_set_location            (NautilusNavigationBar        *bar,
								  const char                   *location);

static char * nautilus_search_uri_to_simple_search_criteria      (const char                   *location);
static char * nautilus_simple_search_criteria_to_search_uri      (const char                   *search_criteria);

static void  nautilus_simple_search_bar_initialize_class         (NautilusSimpleSearchBarClass *class);
static void  nautilus_simple_search_bar_initialize               (NautilusSimpleSearchBar      *bar);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSimpleSearchBar,
				   nautilus_simple_search_bar,
				   NAUTILUS_TYPE_SEARCH_BAR)

static void
nautilus_simple_search_bar_initialize_class (NautilusSimpleSearchBarClass *klass)
{
	NAUTILUS_NAVIGATION_BAR_CLASS (klass)->get_location = nautilus_simple_search_bar_get_location;  
	NAUTILUS_NAVIGATION_BAR_CLASS (klass)->set_location = nautilus_simple_search_bar_set_location;  
}

static void
nautilus_simple_search_bar_initialize (NautilusSimpleSearchBar *bar)
{
	GtkWidget *entry;
	GtkWidget *hbox;
	GtkWidget *find_them, *find_them_label;
	
	hbox = gtk_hbox_new (0, FALSE);
	
	entry = gtk_entry_new ();
	gtk_signal_connect_object (GTK_OBJECT (entry), "activate",
				   nautilus_navigation_bar_location_changed, GTK_OBJECT (bar));
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

	find_them = gtk_button_new ();
	find_them_label = gtk_label_new ("Find Them!");
	gtk_container_add (GTK_CONTAINER (find_them), find_them_label);
	gtk_signal_connect_object (GTK_OBJECT (find_them), "pressed",
				   nautilus_navigation_bar_location_changed,
				   GTK_OBJECT (bar));
	gtk_box_pack_start (GTK_BOX (hbox), find_them, FALSE, TRUE, 1);

	
	gtk_container_add (GTK_CONTAINER (bar), hbox);


	
	gtk_widget_show_all (hbox);
	
	bar->entry = GTK_ENTRY (entry);
}

GtkWidget *
nautilus_simple_search_bar_new (void)
{
	return gtk_widget_new (NAUTILUS_TYPE_SIMPLE_SEARCH_BAR, NULL);
}

static void
nautilus_simple_search_bar_set_location (NautilusNavigationBar *navigation_bar,
					 const char *location)
{
	NautilusSimpleSearchBar *bar;
	char *criteria;

	/* We shouldn't have gotten here if the uri can't be displayed
	 * using a simple search bar
	 */
	bar = NAUTILUS_SIMPLE_SEARCH_BAR (navigation_bar);

	/* Set the words in the box to be the words originally done in the search */ 
	criteria = nautilus_search_uri_to_simple_search_criteria (location);
	gtk_entry_set_text (bar->entry, criteria);
	g_free (criteria);
}

static char *
nautilus_simple_search_bar_get_location (NautilusNavigationBar *navigation_bar)
{
	NautilusSimpleSearchBar *bar;
	char *search_entry_text;

	bar = NAUTILUS_SIMPLE_SEARCH_BAR (navigation_bar);
	search_entry_text = gtk_entry_get_text (bar->entry);
	return nautilus_simple_search_criteria_to_search_uri (search_entry_text);
}



char *
nautilus_search_uri_to_simple_search_criteria (const char *uri)
{
	/* FIXME: Not yet implemented. */
	return g_strdup ("");
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

	words = g_strsplit (search_criteria, " ", strlen (search_criteria));
	/* FIXME: this should eventually be: length = strlen ("[file%3A%2F%2F%2F]"); */
	length = strlen ("[file:///]");
	/* Count total length */
	for (i = 0; words[i] != NULL; i++) {
		length += strlen (words[i]) + strlen ("file_name contains & ");
	}
	fragment = g_new0 (char, length + 1);
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
	g_strfreev (words);
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
