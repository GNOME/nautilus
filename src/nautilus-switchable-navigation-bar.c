/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-switchable-navigation-bar.c - Navigation bar for nautilus
   that can switch between the location bar and the search bar.

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

   Author: Maciej Stachowiak <mjs@eazel.com> */

#include <config.h>
#include "nautilus-switchable-navigation-bar.h"

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include <gtk/gtklabel.h>
#include <gtk/gtksignal.h>

#include <libnautilus-extensions/nautilus-gtk-macros.h>

#include <stdio.h>


enum {
	MODE_CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];


static void child_location_changed_callback                      (NautilusNavigationBar           *navigation_bar,
								 const char                      *location,
								 NautilusSwitchableNavigationBar *bar);

static void nautilus_switchable_navigation_bar_set_location     (NautilusNavigationBar *bar,
								 const char            *location);


static void nautilus_switchable_navigation_bar_initialize_class (NautilusSwitchableNavigationBarClass *class);
static void nautilus_switchable_navigation_bar_initialize       (NautilusSwitchableNavigationBar      *bar);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSwitchableNavigationBar, nautilus_switchable_navigation_bar, NAUTILUS_TYPE_NAVIGATION_BAR)


static void
destroy (GtkObject *object)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_switchable_navigation_bar_initialize_class (NautilusSwitchableNavigationBarClass *klass)
{
	
	GtkObjectClass *object_class;
	NautilusNavigationBarClass *navigation_bar_class;

	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;

	signals[MODE_CHANGED]
		= gtk_signal_new ("mode_changed",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusSwitchableNavigationBarClass,
						     mode_changed),
				  gtk_marshal_NONE__STRING,
				  GTK_TYPE_NONE, 1, GTK_TYPE_INT);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
	
	navigation_bar_class = NAUTILUS_NAVIGATION_BAR_CLASS (klass);


	navigation_bar_class->set_location = nautilus_switchable_navigation_bar_set_location;
}

static void
nautilus_switchable_navigation_bar_initialize (NautilusSwitchableNavigationBar *bar)
{
	GtkWidget *hbox;

	hbox = gtk_hbox_new (0, FALSE);
	bar->location_bar = nautilus_location_bar_new ();
	bar->search_bar = nautilus_search_bar_new ();

	gtk_signal_connect (GTK_OBJECT (bar->location_bar),
			    "location_changed",
			    (GtkSignalFunc) child_location_changed_callback,
			    bar);

	gtk_signal_connect (GTK_OBJECT (bar->search_bar),
			    "location_changed",
			    (GtkSignalFunc) child_location_changed_callback,
			    bar);
	
	gtk_box_pack_start  (GTK_BOX (hbox), bar->location_bar, TRUE, TRUE,
			     0);
	gtk_box_pack_start  (GTK_BOX (hbox), bar->search_bar, TRUE, TRUE,
			     0);

	nautilus_switchable_navigation_bar_set_mode (bar, NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_LOCATION);

	gtk_widget_show (GTK_WIDGET (hbox));

	gtk_container_add   (GTK_CONTAINER (bar), hbox);
}


GtkWidget *
nautilus_switchable_navigation_bar_new (void)
{
	return gtk_widget_new (NAUTILUS_TYPE_SWITCHABLE_NAVIGATION_BAR, NULL);
}


void
nautilus_switchable_navigation_bar_set_mode (NautilusSwitchableNavigationBar     *bar,
					     NautilusSwitchableNavigationBarMode  mode)
{
	switch (mode) {
	case NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_LOCATION:
		gtk_widget_show (bar->location_bar);
		gtk_widget_hide (bar->search_bar);
		bar->mode = mode;
		break;
	case NAUTILUS_SWITCHABLE_NAVIGATION_BAR_MODE_SEARCH:
		gtk_widget_show (bar->search_bar);
		gtk_widget_hide (bar->location_bar);
		bar->mode = mode;
		break;
	default:
		g_return_if_fail (mode && 0);
	}
}

static void
child_location_changed_callback (NautilusNavigationBar           *navigation_bar,
				 const char                      *location,
				 NautilusSwitchableNavigationBar *bar)
{
	nautilus_navigation_bar_location_changed (NAUTILUS_NAVIGATION_BAR (bar),
						  location);
}


static void
nautilus_switchable_navigation_bar_set_location (NautilusNavigationBar *navigation_bar,
						 const char *location)
{
	NautilusSwitchableNavigationBar *bar;

	bar = NAUTILUS_SWITCHABLE_NAVIGATION_BAR (navigation_bar);

	puts ("XXX: foo");
	
	/* Set location for both bars so if we switch things will stil
           look OK. */

	nautilus_navigation_bar_set_location (NAUTILUS_NAVIGATION_BAR (bar->location_bar),
					      location);
	nautilus_navigation_bar_set_location (NAUTILUS_NAVIGATION_BAR (bar->search_bar),
					      location);
}

