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
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* nautilus-switchable-navigation-bar.c - Navigation bar for nautilus
 * that can switch between the location bar and the search bar.
 */

#include <config.h>
#include "nautilus-switchable-navigation-bar.h"

#include "nautilus-switchable-search-bar.h"
#include <gtk/gtklabel.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dock.h>
#include <libnautilus-private/nautilus-directory.h>
#include <eel/eel-gtk-macros.h>
#include <libnautilus-private/nautilus-search-uri.h>
#include <eel/eel-string.h>
#include <stdio.h>

struct NautilusSwitchableNavigationBarDetails {
	NautilusSwitchableNavigationBarMode mode;

	NautilusLocationBar *location_bar;
	NautilusSwitchableSearchBar *search_bar;
};

enum {
	MODE_CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];


static char *nautilus_switchable_navigation_bar_get_location     (NautilusNavigationBar                *bar);
static void  nautilus_switchable_navigation_bar_set_location     (NautilusNavigationBar                *bar,
								  const char                           *location);
static void  nautilus_switchable_navigation_bar_initialize_class (NautilusSwitchableNavigationBarClass *class);
static void  nautilus_switchable_navigation_bar_initialize       (NautilusSwitchableNavigationBar      *bar);
static void  nautilus_switchable_navigation_bar_destroy 	 (GtkObject 			       *object);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusSwitchableNavigationBar,
				   nautilus_switchable_navigation_bar,
				   NAUTILUS_TYPE_NAVIGATION_BAR)

static void
nautilus_switchable_navigation_bar_initialize_class (NautilusSwitchableNavigationBarClass *klass)
{
	
	GtkObjectClass *object_class;
	NautilusNavigationBarClass *navigation_bar_class;

	object_class = GTK_OBJECT_CLASS (klass);
	navigation_bar_class = NAUTILUS_NAVIGATION_BAR_CLASS (klass);

	signals[MODE_CHANGED] = gtk_signal_new
		("mode_changed",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusSwitchableNavigationBarClass,
				    mode_changed),
		 gtk_marshal_NONE__STRING,
		 GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
	
	object_class->destroy = nautilus_switchable_navigation_bar_destroy;

	navigation_bar_class->get_location = nautilus_switchable_navigation_bar_get_location;
	navigation_bar_class->set_location = nautilus_switchable_navigation_bar_set_location;
}

static void
nautilus_switchable_navigation_bar_initialize (NautilusSwitchableNavigationBar *bar)
{



	bar->details = g_new0 (NautilusSwitchableNavigationBarDetails, 1);


}

static void
nautilus_switchable_navigation_bar_destroy (GtkObject *object)
{
	g_free (NAUTILUS_SWITCHABLE_NAVIGATION_BAR (object)->details);
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

GtkWidget *
nautilus_switchable_navigation_bar_new (NautilusWindow *window)
{
	GtkWidget *bar;
	GtkWidget *hbox;
	NautilusSwitchableNavigationBar *switchable_navigation_bar;
	
	bar = gtk_widget_new (NAUTILUS_TYPE_SWITCHABLE_NAVIGATION_BAR, NULL);

	hbox = gtk_hbox_new (FALSE, 0);

	switchable_navigation_bar = NAUTILUS_SWITCHABLE_NAVIGATION_BAR (bar);
	switchable_navigation_bar->details->location_bar = NAUTILUS_LOCATION_BAR (nautilus_location_bar_new (window));
	switchable_navigation_bar->details->search_bar = NAUTILUS_SWITCHABLE_SEARCH_BAR (nautilus_switchable_search_bar_new (window));

	gtk_signal_connect_object (GTK_OBJECT (switchable_navigation_bar->details->location_bar),
				   "location_changed",
				   nautilus_navigation_bar_location_changed,
				   GTK_OBJECT (bar));
	gtk_signal_connect_object (GTK_OBJECT (switchable_navigation_bar->details->search_bar),
				   "location_changed",
				   nautilus_navigation_bar_location_changed,
				   GTK_OBJECT (bar));
	
	gtk_box_pack_start  (GTK_BOX (hbox), GTK_WIDGET (switchable_navigation_bar->details->location_bar), TRUE, TRUE,
			     0);
	gtk_box_pack_start  (GTK_BOX (hbox), GTK_WIDGET (switchable_navigation_bar->details->search_bar), TRUE, TRUE,
			     0);

	gtk_widget_show (GTK_WIDGET (switchable_navigation_bar->details->location_bar));
	gtk_widget_show (GTK_WIDGET (hbox));
	gtk_container_add (GTK_CONTAINER (bar), hbox);

	return bar;
}

NautilusSwitchableNavigationBarMode
nautilus_switchable_navigation_bar_get_mode (NautilusSwitchableNavigationBar     *bar)
{
	return bar->details->mode;
}

void
nautilus_switchable_navigation_bar_activate (NautilusSwitchableNavigationBar *bar)
{
	NautilusNavigationBar *bar_to_activate;

	switch (bar->details->mode) {
	case NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_LOCATION:
		bar_to_activate = NAUTILUS_NAVIGATION_BAR (bar->details->location_bar);
		break;
	case NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_SEARCH:
		bar_to_activate = NAUTILUS_NAVIGATION_BAR (bar->details->search_bar);
		break;
	default:
		g_return_if_fail (FALSE);
	}

	nautilus_navigation_bar_activate (bar_to_activate);
}


void
nautilus_switchable_navigation_bar_set_mode (NautilusSwitchableNavigationBar     *bar,
					     NautilusSwitchableNavigationBarMode  mode)
{
	GtkWidget *widget_to_hide, *widget_to_show;
	GtkWidget *dock;

	if (bar->details->mode == mode) {
		return;
	}

	bar->details->mode = mode;

	switch (mode) {
	case NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_LOCATION:
		widget_to_show = GTK_WIDGET (bar->details->location_bar);
		widget_to_hide = GTK_WIDGET (bar->details->search_bar);
		break;
	case NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_SEARCH:
		widget_to_show = GTK_WIDGET (bar->details->search_bar);
		widget_to_hide = GTK_WIDGET (bar->details->location_bar);
		break;
	default:
		g_return_if_fail (mode && 0);
	}

	gtk_widget_show (widget_to_show);
	gtk_widget_hide (widget_to_hide);

	nautilus_switchable_navigation_bar_activate (bar);

	/* FIXME bugzilla.gnome.org 43171:
	 * We don't know why this line is needed here, but if it's removed
	 * then the bar won't shrink when we switch from the complex search
	 * bar to the location bar (though it does grow when switching in
	 * the other direction)
	 */
	dock = gtk_widget_get_ancestor (GTK_WIDGET (bar), GNOME_TYPE_DOCK);
	if (dock != NULL) {
		gtk_widget_queue_resize (dock);
	}

	gtk_signal_emit (GTK_OBJECT (bar), signals[MODE_CHANGED], mode);
}

static char *
nautilus_switchable_navigation_bar_get_location (NautilusNavigationBar *navigation_bar)
{
	NautilusSwitchableNavigationBar *bar;

	bar = NAUTILUS_SWITCHABLE_NAVIGATION_BAR (navigation_bar);

	switch (bar->details->mode) {
	case NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_LOCATION:
		return nautilus_navigation_bar_get_location (NAUTILUS_NAVIGATION_BAR (bar->details->location_bar));
	case NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_SEARCH:
		return nautilus_navigation_bar_get_location (NAUTILUS_NAVIGATION_BAR (bar->details->search_bar));
	default:
		g_assert_not_reached ();
		return NULL;
	}
}

static void
nautilus_switchable_navigation_bar_set_location (NautilusNavigationBar *navigation_bar,
						 const char *location)
{
	NautilusSwitchableNavigationBar *bar;

	bar = NAUTILUS_SWITCHABLE_NAVIGATION_BAR (navigation_bar);

	/* Set location for both bars so if we switch things will
	 * still look OK.
	 */
	nautilus_navigation_bar_set_location (NAUTILUS_NAVIGATION_BAR (bar->details->location_bar),
					      location);
	nautilus_navigation_bar_set_location (NAUTILUS_NAVIGATION_BAR (bar->details->search_bar),
					      location);
	
	/* Toggle the search button on and off appropriately */
	if (nautilus_is_search_uri (location)) {
		nautilus_switchable_navigation_bar_set_mode
			(bar, NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_SEARCH);
	} else {
		nautilus_switchable_navigation_bar_set_mode
			(bar, NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_LOCATION);
	}
}
