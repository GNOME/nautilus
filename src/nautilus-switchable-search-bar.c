/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-switchable-search-bar.c - multimodal search bar

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
#include "nautilus-switchable-search-bar.h"

#include "nautilus-complex-search-bar.h"
#include "nautilus-simple-search-bar.h"
#include <gtk/gtkeventbox.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-extensions/nautilus-directory.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>

static void                  nautilus_switchable_search_bar_set_location     (NautilusNavigationBar            *bar,
									      const char                       *location);
static char *                nautilus_switchable_search_bar_get_location     (NautilusNavigationBar            *bar);
static void                  nautilus_switchable_search_bar_initialize_class (NautilusSwitchableSearchBarClass *class);
static void                  nautilus_switchable_search_bar_initialize       (NautilusSwitchableSearchBar      *bar);
static void  		     nautilus_switchable_search_bar_destroy          (GtkObject                	       *object);

static NautilusSearchBarMode other_search_mode                               (NautilusSearchBarMode            mode);
static NautilusSearchBarMode nautilus_search_uri_to_search_bar_mode          (const char *uri);
static gboolean              nautilus_search_uri_is_displayable_by_mode      (const char *uri,
									      NautilusSearchBarMode mode);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSwitchableSearchBar,
				   nautilus_switchable_search_bar,
				   NAUTILUS_TYPE_SEARCH_BAR)

static void
nautilus_switchable_search_bar_initialize_class (NautilusSwitchableSearchBarClass *klass)
{
	NAUTILUS_NAVIGATION_BAR_CLASS (klass)->get_location = nautilus_switchable_search_bar_get_location;
	NAUTILUS_NAVIGATION_BAR_CLASS (klass)->set_location = nautilus_switchable_search_bar_set_location;

	GTK_OBJECT_CLASS (klass)->destroy = nautilus_switchable_search_bar_destroy;
}

static void
search_bar_preference_changed_callback (gpointer user_data)
{
	g_assert (NAUTILUS_IS_SWITCHABLE_SEARCH_BAR (user_data));

	/* Switch immediately as long as the current search_uri doesn't veto the switch.
	 * FIXME: Perhaps switch immediately anyway and blow away partially-formed
	 * search criteria?
	 */
	nautilus_switchable_search_bar_set_mode 
		(NAUTILUS_SWITCHABLE_SEARCH_BAR (user_data), 
		 nautilus_search_uri_to_search_bar_mode 
			(nautilus_switchable_search_bar_get_location 
				(NAUTILUS_NAVIGATION_BAR (user_data))));
}

static void
nautilus_switchable_search_bar_initialize (NautilusSwitchableSearchBar *bar)
{
	
	GtkWidget *label;
	GtkWidget *event_box;
	GtkWidget *vbox;
	GtkWidget *hbox;
	
	hbox = gtk_hbox_new (0, FALSE);
	event_box = gtk_event_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (event_box),
					GNOME_PAD_SMALL);
	
	vbox = gtk_vbox_new (0, FALSE);
	label = gtk_label_new (_("Search For:"));
	gtk_container_add (GTK_CONTAINER (event_box), label);
	
	gtk_box_pack_start (GTK_BOX (hbox), event_box, FALSE, TRUE, GNOME_PAD_SMALL);	
	bar->complex_search_bar = nautilus_complex_search_bar_new ();
	bar->simple_search_bar = nautilus_simple_search_bar_new ();

	gtk_signal_connect_object (GTK_OBJECT (bar->complex_search_bar),
				   "location_changed",
				   nautilus_navigation_bar_location_changed,
				   GTK_OBJECT (bar));
	gtk_signal_connect_object (GTK_OBJECT (bar->simple_search_bar),
				   "location_changed",
				   nautilus_navigation_bar_location_changed,
				   GTK_OBJECT (bar));

	
	gtk_box_pack_start  (GTK_BOX (hbox), bar->complex_search_bar, TRUE, TRUE,
			     0);
	gtk_box_pack_start  (GTK_BOX (hbox), bar->simple_search_bar, TRUE, TRUE,
			     0);
	
	gtk_container_add (GTK_CONTAINER (bar), hbox);

	gtk_widget_show_all (hbox);
	nautilus_switchable_search_bar_set_mode
		(bar, 
		 nautilus_preferences_get_enum (NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
						NAUTILUS_SIMPLE_SEARCH_BAR));

	/* React to future preference changes. */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
					   search_bar_preference_changed_callback,
					   bar);
}

static void
nautilus_switchable_search_bar_destroy (GtkObject *object)
{
	NautilusSwitchableSearchBar *bar;

	bar = NAUTILUS_SWITCHABLE_SEARCH_BAR (object);

	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
					      search_bar_preference_changed_callback,
					      bar);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

GtkWidget *
nautilus_switchable_search_bar_new (void)
{
	return gtk_widget_new (nautilus_switchable_search_bar_get_type (), NULL);
}

void
nautilus_switchable_search_bar_set_mode (NautilusSwitchableSearchBar *bar,
					 NautilusSearchBarMode mode)
{
	char *location;

	g_return_if_fail (NAUTILUS_IS_SWITCHABLE_SEARCH_BAR (bar));
	g_return_if_fail (mode == NAUTILUS_SIMPLE_SEARCH_BAR
			  || mode == NAUTILUS_COMPLEX_SEARCH_BAR);

	/* Ignore requests for impossible modes for now */
	location = nautilus_navigation_bar_get_location (NAUTILUS_NAVIGATION_BAR (bar));
	if (nautilus_search_uri_is_displayable_by_mode (location, mode) == FALSE) {
		g_free (location);
		return;
	}
	g_free (location);

	/* FIXME bugzilla.eazel.com 1860: 
	 * Switching to the complex search bar, which is taller, leaves
	 * the navigation bar forever at the taller height, even after switching
	 * back to the simple search bar or the location bar.
	 */
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
	default:
		g_assert_not_reached();
		break;
	}
}

static char *
nautilus_switchable_search_bar_get_location (NautilusNavigationBar *navigation_bar)
{
	NautilusSwitchableSearchBar *bar;

	bar = NAUTILUS_SWITCHABLE_SEARCH_BAR (navigation_bar);

	switch (bar->mode) {
	case NAUTILUS_SIMPLE_SEARCH_BAR:
		return nautilus_navigation_bar_get_location (NAUTILUS_NAVIGATION_BAR (bar->simple_search_bar));
	case NAUTILUS_COMPLEX_SEARCH_BAR:
		return nautilus_navigation_bar_get_location (NAUTILUS_NAVIGATION_BAR (bar->complex_search_bar));
	default:
		g_assert_not_reached();
		return NULL;
	}
}

static void
nautilus_switchable_search_bar_set_location (NautilusNavigationBar *navigation_bar,
					     const char *location)
{
	NautilusSwitchableSearchBar *bar;
	NautilusSearchBarMode mode;

	bar = NAUTILUS_SWITCHABLE_SEARCH_BAR (navigation_bar);

	/* Set the mode of the search bar,
	   in case preferences have changed 
	*/
	/* FIXME:  This doesn't work yet. */
	mode = nautilus_search_uri_to_search_bar_mode (location);
	nautilus_switchable_search_bar_set_mode (bar, mode);
						 
	nautilus_navigation_bar_set_location (NAUTILUS_NAVIGATION_BAR (bar->simple_search_bar),
					      location);
	nautilus_navigation_bar_set_location (NAUTILUS_NAVIGATION_BAR (bar->complex_search_bar),
					      location);
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


