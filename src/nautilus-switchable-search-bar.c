/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Rebecca Schulman <rebecka@eazel.com> 
 */

/* nautilus-switchable-search-bar.c - multimodal search bar
 */

#include <config.h>
#include "nautilus-switchable-search-bar.h"

#include "nautilus-complex-search-bar.h"
#include "nautilus-simple-search-bar.h"
#include <gtk/gtkeventbox.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dock.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-private/nautilus-directory.h>
#include <libnautilus-private/nautilus-global-preferences.h>
#include <eel/eel-gtk-macros.h>

static void		     real_activate				     (NautilusNavigationBar	       *bar);
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

EEL_DEFINE_CLASS_BOILERPLATE (NautilusSwitchableSearchBar,
				   nautilus_switchable_search_bar,
				   NAUTILUS_TYPE_SEARCH_BAR)

static void
nautilus_switchable_search_bar_initialize_class (NautilusSwitchableSearchBarClass *klass)
{
	NAUTILUS_NAVIGATION_BAR_CLASS (klass)->activate = real_activate;
	NAUTILUS_NAVIGATION_BAR_CLASS (klass)->get_location = nautilus_switchable_search_bar_get_location;
	NAUTILUS_NAVIGATION_BAR_CLASS (klass)->set_location = nautilus_switchable_search_bar_set_location;

	GTK_OBJECT_CLASS (klass)->destroy = nautilus_switchable_search_bar_destroy;
}

static void
search_bar_preference_changed_callback (gpointer user_data)
{
	char *location;

	g_assert (NAUTILUS_IS_SWITCHABLE_SEARCH_BAR (user_data));

	/* Switch immediately as long as the current search_uri doesn't veto the switch.
	 * FIXME bugzilla.gnome.org 42515: 
	 * Perhaps switch immediately anyway and blow away partially-formed
	 * search criteria?
	 */
	location = nautilus_switchable_search_bar_get_location 
		(NAUTILUS_NAVIGATION_BAR (user_data));
	nautilus_switchable_search_bar_set_mode 
		(NAUTILUS_SWITCHABLE_SEARCH_BAR (user_data), 
		 nautilus_search_uri_to_search_bar_mode (location));
	g_free (location);
}

static void
nautilus_switchable_search_bar_initialize (NautilusSwitchableSearchBar *bar)
{

}	


static void
nautilus_switchable_search_bar_destroy (GtkObject *object)
{
	NautilusSwitchableSearchBar *bar;

	bar = NAUTILUS_SWITCHABLE_SEARCH_BAR (object);

	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
					      search_bar_preference_changed_callback,
					      bar);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

GtkWidget *
nautilus_switchable_search_bar_new (NautilusWindow *window)
{
	GtkWidget *label;
	GtkWidget *event_box;
	GtkWidget *hbox;
	GtkWidget *switchable_search_bar;
	NautilusSwitchableSearchBar *bar;

	switchable_search_bar = gtk_widget_new (nautilus_switchable_search_bar_get_type (), NULL);
	bar = NAUTILUS_SWITCHABLE_SEARCH_BAR (switchable_search_bar);
	
	hbox = gtk_hbox_new (0, FALSE);
	event_box = gtk_event_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (event_box),
					GNOME_PAD_SMALL);
	
	label = gtk_label_new (_("Find:"));
	gtk_container_add (GTK_CONTAINER (event_box), label);
	
	gtk_box_pack_start (GTK_BOX (hbox), event_box, FALSE, TRUE, GNOME_PAD_SMALL);	
	bar->complex_search_bar = nautilus_complex_search_bar_new (window);
	bar->simple_search_bar = nautilus_simple_search_bar_new (window);

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
		 eel_preferences_get_integer (NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE));
	
	/* React to future preference changes. */
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
				      search_bar_preference_changed_callback,
				      bar);
	
	return switchable_search_bar;

}

static void
real_activate (NautilusNavigationBar *navigation_bar)
{
	NautilusSwitchableSearchBar *bar;
	NautilusNavigationBar *bar_to_activate;

	bar = NAUTILUS_SWITCHABLE_SEARCH_BAR (navigation_bar);

	switch (bar->mode) {
	default:
		g_assert_not_reached();
	case NAUTILUS_SIMPLE_SEARCH_BAR:
		bar_to_activate = NAUTILUS_NAVIGATION_BAR (bar->simple_search_bar);
		break;
	case NAUTILUS_COMPLEX_SEARCH_BAR:
		bar_to_activate = NAUTILUS_NAVIGATION_BAR (bar->complex_search_bar);
		break;
	}

	g_assert (bar_to_activate != NULL);
	nautilus_navigation_bar_activate (bar_to_activate);
}

void
nautilus_switchable_search_bar_set_mode (NautilusSwitchableSearchBar *bar,
					 NautilusSearchBarMode mode)
{
	char *location;
	GtkWidget *dock;

	g_return_if_fail (NAUTILUS_IS_SWITCHABLE_SEARCH_BAR (bar));
	g_return_if_fail (mode == NAUTILUS_SIMPLE_SEARCH_BAR
			  || mode == NAUTILUS_COMPLEX_SEARCH_BAR);

	/* Ignore requests for impossible modes for now */
	location = nautilus_navigation_bar_get_location (NAUTILUS_NAVIGATION_BAR (bar));
	if (!nautilus_search_uri_is_displayable_by_mode (location, mode)) {
		g_free (location);
		return;
	}
	g_free (location);

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

	/* FIXME bugzilla.gnome.org 43171:
	 * We don't know why this line is needed here, but if it's removed
	 * then the bar won't shrink when we switch to the simple search bar
	 * (though it does grow when switching to the complex one).
	 */
	dock = gtk_widget_get_ancestor (GTK_WIDGET (bar), GNOME_TYPE_DOCK);
	if (dock != NULL) {
		gtk_widget_queue_resize (dock);
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
	/* FIXME bugzilla.gnome.org 42514:  This doesn't work yet. */
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

	preferred_mode = eel_preferences_get_integer (NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE);
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
	/* FIXME bugzilla.gnome.org 42514 */
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


