/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Rebecca Schulman <rebecka@eazel.com>
 */

/* nautilus-simple-search-bar.c - One box Search bar for Nautilus
 */

#include <config.h>
#include "nautilus-simple-search-bar.h"

#include "nautilus-search-bar-criterion.h"
#include "nautilus-window-private.h"
#include <libgnome/gnome-i18n.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus/nautilus-clipboard.h>
#include <libnautilus-private/nautilus-entry.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <libnautilus-private/nautilus-search-uri.h>
#include <eel/eel-string.h>
#include <libnautilus-private/nautilus-undo-signal-handlers.h>
#include <stdio.h>

struct NautilusSimpleSearchBarDetails {
	NautilusEntry *entry;
	GtkWidget *find_button;
};

static void   real_activate				     	 (NautilusNavigationBar	       *bar);
static char * nautilus_simple_search_bar_get_location            (NautilusNavigationBar        *bar);
static void   nautilus_simple_search_bar_set_location            (NautilusNavigationBar        *bar,
								  const char                   *location);

static char * nautilus_search_uri_to_simple_search_criteria      (const char                   *location);
static char * nautilus_simple_search_criteria_to_search_uri      (const char                   *search_criteria);

static void  nautilus_simple_search_bar_initialize_class         (NautilusSimpleSearchBarClass *class);
static void  nautilus_simple_search_bar_initialize               (NautilusSimpleSearchBar      *bar);
static void  nautilus_simple_search_bar_destroy 	 	 (GtkObject 		       *object);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusSimpleSearchBar,
				   nautilus_simple_search_bar,
				   NAUTILUS_TYPE_SEARCH_BAR)

static void
nautilus_simple_search_bar_initialize_class (NautilusSimpleSearchBarClass *klass)
{
	GTK_OBJECT_CLASS (klass)->destroy = nautilus_simple_search_bar_destroy;

	NAUTILUS_NAVIGATION_BAR_CLASS (klass)->activate = real_activate;
	NAUTILUS_NAVIGATION_BAR_CLASS (klass)->get_location = nautilus_simple_search_bar_get_location;  
	NAUTILUS_NAVIGATION_BAR_CLASS (klass)->set_location = nautilus_simple_search_bar_set_location;  
}

static gboolean
search_text_is_invalid (NautilusSimpleSearchBar *bar)
{
	char *user_text;
	gboolean is_empty;

	user_text = gtk_editable_get_chars (GTK_EDITABLE (bar->details->entry), 0, -1);
	is_empty = eel_str_is_empty (user_text);
	g_free (user_text);

	return is_empty;
}

static void
update_simple_find_button_state (NautilusSimpleSearchBar *bar)
{
	gtk_widget_set_sensitive (GTK_WIDGET (bar->details->find_button), 
				  !search_text_is_invalid (bar));
}

static void
nautilus_simple_search_bar_initialize (NautilusSimpleSearchBar *bar)
{
	bar->details = g_new0 (NautilusSimpleSearchBarDetails, 1);
}

static void
nautilus_simple_search_bar_destroy (GtkObject *object)
{
	NautilusSimpleSearchBar *bar;

	bar = NAUTILUS_SIMPLE_SEARCH_BAR (object);
	
	nautilus_undo_editable_set_undo_key (GTK_EDITABLE (bar->details->entry), FALSE);
	nautilus_undo_tear_down_nautilus_entry_for_undo (bar->details->entry);

	g_free (bar->details);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

GtkWidget *
nautilus_simple_search_bar_new (NautilusWindow *window)
{
	GtkWidget *simple_search_bar;
	NautilusSimpleSearchBar *bar;
	
	GtkWidget *hbox;

	simple_search_bar =  gtk_widget_new (NAUTILUS_TYPE_SIMPLE_SEARCH_BAR, NULL);
	bar = NAUTILUS_SIMPLE_SEARCH_BAR (simple_search_bar);
	hbox = gtk_hbox_new (0, FALSE);
	
	/* Create button first so we can use it for auto_click */
	bar->details->find_button = gtk_button_new_with_label (_("Find Them!"));

	bar->details->entry = NAUTILUS_ENTRY (nautilus_entry_new ());
	nautilus_undo_set_up_nautilus_entry_for_undo (bar->details->entry);
	nautilus_undo_editable_set_undo_key (GTK_EDITABLE (bar->details->entry), TRUE);
	nautilus_clipboard_set_up_editable
		(GTK_EDITABLE (bar->details->entry),
		 nautilus_window_get_ui_container (window),
		 TRUE);
	
	gtk_signal_connect_object (GTK_OBJECT (bar->details->entry), "activate",
				   eel_gtk_button_auto_click, 
				   GTK_OBJECT (bar->details->find_button));
	gtk_signal_connect_object (GTK_OBJECT (bar->details->entry), "changed",
				   update_simple_find_button_state, GTK_OBJECT (bar));

	gtk_box_pack_start (GTK_BOX (hbox), GTK_WIDGET (bar->details->entry), TRUE, TRUE, 0);

	gtk_signal_connect_object (GTK_OBJECT (bar->details->find_button), "clicked",
				   nautilus_navigation_bar_location_changed,
				   GTK_OBJECT (bar));
	gtk_box_pack_start (GTK_BOX (hbox), bar->details->find_button, FALSE, TRUE, 1);
	update_simple_find_button_state (bar);
	
	gtk_container_add (GTK_CONTAINER (bar), hbox);

	gtk_widget_show_all (hbox);	

	return simple_search_bar;
}

static void
real_activate (NautilusNavigationBar *navigation_bar)
{
	NautilusSimpleSearchBar *bar;

	bar = NAUTILUS_SIMPLE_SEARCH_BAR (navigation_bar);

	/* Put the keyboard focus in the text field when switching to search mode */
	gtk_widget_grab_focus (GTK_WIDGET (bar->details->entry));
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
	nautilus_entry_set_text (bar->details->entry, criteria);
	g_free (criteria);
}

static char *
nautilus_simple_search_bar_get_location (NautilusNavigationBar *navigation_bar)
{
	NautilusSimpleSearchBar *bar;
	char *search_entry_text;

	bar = NAUTILUS_SIMPLE_SEARCH_BAR (navigation_bar);
	search_entry_text = gtk_entry_get_text (GTK_ENTRY (bar->details->entry));
	return nautilus_simple_search_criteria_to_search_uri (search_entry_text);
}



char *
nautilus_search_uri_to_simple_search_criteria (const char *uri)
{
	/* FIXME bugzilla.gnome.org 42511: Not yet implemented. */
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

	/* FIXME bugzilla.gnome.org 42512: 
	 * The logic here should be exactly the same as the logic for
	 * a complex search-by-file-name. Currently the complex search doesn't
	 * do the multi-word handling that this function does. They should use
	 * the same code.
	 */
	g_return_val_if_fail (search_criteria != NULL, NULL);

	words = g_strsplit (search_criteria, " ", strlen (search_criteria));
	/* FIXME bugzilla.gnome.org 42513: this should eventually be: length = strlen ("[file%3A%2F%2F%2F]"); */
	length = strlen ("[file:///]");
	/* Count total length */
	for (i = 0; words[i] != NULL; i++) {
		length += strlen (NAUTILUS_SEARCH_URI_TEXT_NAME) + strlen (" contains ") + strlen (words[i]) + strlen (" & ");
	}
	fragment = g_new0 (char, length + 1);
	/* FIXME bugzilla.gnome.org 42513: this should eventually be: sprintf (fragment, "[file%%3A%%2F%%2F%%2F]"); */
	sprintf (fragment, "[file:///]");
	if (words[0] != NULL) {
		for (i = 0; words[i+1] != NULL; i++) {
			strcat (fragment, NAUTILUS_SEARCH_URI_TEXT_NAME);
			strcat (fragment, " contains ");
			strcat (fragment, words[i]);
			strcat (fragment, " & ");
		}
		strcat (fragment, NAUTILUS_SEARCH_URI_TEXT_NAME);
		strcat (fragment, " contains ");
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
