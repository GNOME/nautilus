/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-switcheable-search-bar.c - multimodal search bar

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
#include "nautilus-search-bar.h"
#include "nautilus-switchable-search-bar.h"
#include "nautilus-simple-search-bar.h"
#include "nautilus-complex-search-bar.h"
#include "nautilus-search-bar-criterion.h"

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include <gtk/gtksignal.h>
#include <gtk/gtkeventbox.h>

#include <libgnomeui/gnome-uidefs.h>

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-entry.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-search-uri.h>


static void                                nautilus_switchable_search_bar_set_search_controls     
                                                                                           (NautilusSearchBar *bar,
			  								    const char *location);
static char *                              nautilus_switchable_search_bar_get_location     (NautilusSwitchableSearchBar *bar);


static void                                nautilus_switchable_search_bar_initialize_class (NautilusSwitchableSearchBarClass *class);
static void                                nautilus_switchable_search_bar_initialize       (NautilusSwitchableSearchBar      *bar);

static void                                search_activated_callback                       (GtkButton *button,
											    NautilusSwitchableSearchBar *bar);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSwitchableSearchBar, nautilus_switchable_search_bar, NAUTILUS_TYPE_SEARCH_BAR)




static void
destroy (GtkObject *object)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_switchable_search_bar_initialize_class (NautilusSwitchableSearchBarClass *klass)
{
	
	GtkObjectClass *object_class;
	NautilusSearchBarClass *search_bar_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;
	
	search_bar_class = NAUTILUS_SEARCH_BAR_CLASS (klass);
	search_bar_class->set_search_controls = nautilus_switchable_search_bar_set_search_controls;

}

static void
nautilus_switchable_search_bar_initialize (NautilusSwitchableSearchBar *bar)
{
	
	GtkWidget *label;
	GtkWidget *event_box;
	GtkWidget *hbox;
	GtkWidget *find_them, *find_them_label;
	
	hbox = gtk_hbox_new (0, FALSE);
	event_box = gtk_event_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (event_box),
					GNOME_PAD_SMALL);
	
	label = gtk_label_new (_("Search For:"));
	gtk_container_add (GTK_CONTAINER (event_box), label);
	
	gtk_box_pack_start (GTK_BOX (hbox), event_box, FALSE, TRUE, GNOME_PAD_SMALL);	
	bar->complex_search_bar = nautilus_complex_search_bar_new ();
	bar->simple_search_bar = nautilus_simple_search_bar_new ();
	
	gtk_box_pack_start  (GTK_BOX (hbox), bar->complex_search_bar, TRUE, TRUE,
			     0);
	gtk_box_pack_start  (GTK_BOX (hbox), bar->simple_search_bar, TRUE, TRUE,
			     0);

	find_them = gtk_button_new ();
	find_them_label = gtk_label_new ("Find Them!");
	gtk_container_add (GTK_CONTAINER (find_them), find_them_label);
	gtk_signal_connect (GTK_OBJECT (find_them), "pressed",
			    search_activated_callback, bar);

	gtk_box_pack_start (GTK_BOX (hbox), find_them, TRUE, TRUE, 1);

	
	gtk_container_add (GTK_CONTAINER (bar), hbox);

	bar->label = GTK_LABEL (label);
	bar->search_button = GTK_BUTTON (find_them);

	gtk_widget_show_all (hbox);
	nautilus_switchable_search_bar_set_mode (bar,nautilus_preferences_get_enum (NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
										    NAUTILUS_SIMPLE_SEARCH_BAR));
}


GtkWidget *
nautilus_switchable_search_bar_new (void)
{
	return gtk_widget_new (nautilus_switchable_search_bar_get_type (), NULL);
}


static char *                     
nautilus_switchable_search_bar_get_location (NautilusSwitchableSearchBar *bar)
{
	/* FIXME */
	return g_strdup ("file:///tmp");
}

				 

void
nautilus_switchable_search_bar_set_mode (NautilusSwitchableSearchBar *bar,
					 NautilusSearchBarMode mode)
{
	char *location;

	/* Ignore requests for impossible modes for now */
	location = nautilus_switchable_search_bar_get_location (bar);
	if (nautilus_search_uri_is_displayable_by_mode  (location, mode) == FALSE) {
		return;
	}
	
	switch (mode) {
	case NAUTILUS_SIMPLE_SEARCH_BAR:
		gtk_widget_show (bar->simple_search_bar);
		gtk_widget_hide (bar->complex_search_bar);
		bar->mode = mode;
		break;
	case NAUTILUS_COMPLEX_SEARCH_BAR:
		gtk_widget_show (bar->complex_search_bar);
		gtk_widget_hide (bar->simple_search_bar);
		bar->mode = mode;
		break;
	}
}

static void
search_activated_callback (GtkButton *button,
			   NautilusSwitchableSearchBar *bar)
{
	char *uri;
	g_assert (NAUTILUS_IS_SWITCHABLE_SEARCH_BAR (bar));

	uri = nautilus_switchable_search_bar_get_location (bar);

	nautilus_navigation_bar_location_changed (NAUTILUS_NAVIGATION_BAR (bar),
						  uri);
	g_free (uri);
	
		  

}

static void
nautilus_switchable_search_bar_set_search_controls (NautilusSearchBar *search_bar,
						    const char *location)
{
	NautilusSwitchableSearchBar *bar;
	NautilusSearchBarMode mode;

	bar = NAUTILUS_SWITCHABLE_SEARCH_BAR (search_bar);

	/* Set the mode of the search bar,
	   in case preferences have changed */
	mode = nautilus_search_uri_to_search_bar_mode (location);
	nautilus_switchable_search_bar_set_mode (bar, mode);
						 
	nautilus_search_bar_set_search_controls (NAUTILUS_SEARCH_BAR (bar->simple_search_bar),
						 location);
	nautilus_search_bar_set_search_controls (NAUTILUS_SEARCH_BAR (bar->complex_search_bar),
						 location);
}


