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

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-search-uri.h>

struct NautilusSimpleSearchBarDetails {
	GtkEntry *entry;
};

static char *nautilus_simple_search_bar_get_location     (NautilusNavigationBar        *bar);
static void  nautilus_simple_search_bar_set_location     (NautilusNavigationBar        *bar,
							  const char                   *location);
static void  nautilus_simple_search_bar_initialize_class (NautilusSimpleSearchBarClass *class);
static void  nautilus_simple_search_bar_initialize       (NautilusSimpleSearchBar      *bar);

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
	
	hbox = gtk_hbox_new (0, FALSE);
	
	entry = gtk_entry_new ();
	gtk_signal_connect_object (GTK_OBJECT (entry), "activate",
				   nautilus_navigation_bar_location_changed, GTK_OBJECT (bar));
	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
	
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
	g_return_if_fail (nautilus_search_uri_is_displayable_by_mode (location, NAUTILUS_SIMPLE_SEARCH_BAR));

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
