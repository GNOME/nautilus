/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-navigation-bar.c - Abstract navigation bar class

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

   Author: Maciej Stachowiak <mjs@eazel.com>
*/

#include <config.h>

#include <string.h>
#include <gnome.h>

#include "nautilus-navigation-bar.h"

#include <gtk/gtksignal.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>

enum {
	LOCATION_CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

static void nautilus_navigation_bar_initialize_class (NautilusNavigationBarClass *class);
static void nautilus_navigation_bar_initialize       (NautilusNavigationBar      *bar);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusNavigationBar, nautilus_navigation_bar, NAUTILUS_TYPE_GENEROUS_BIN)

NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_navigation_bar, get_location)
NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_navigation_bar, set_location)

static void
nautilus_navigation_bar_initialize_class (NautilusNavigationBarClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	signals[LOCATION_CHANGED]
		= gtk_signal_new ("location_changed",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusNavigationBarClass,
						     location_changed),
				  gtk_marshal_NONE__STRING,
				  GTK_TYPE_NONE, 1, GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_navigation_bar, get_location);
	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_navigation_bar, set_location);
}

static void
nautilus_navigation_bar_initialize (NautilusNavigationBar *bar)
{
}

/**
 * nautilus_navigation_bar_get_location
 * 
 * Return the location displayed in the navigation bar.
 * 
 * @bar: A NautilusNavigationBar.
 * @location: The uri that should be displayed.
 */
char *
nautilus_navigation_bar_get_location (NautilusNavigationBar *bar)
{
	g_return_val_if_fail (NAUTILUS_IS_NAVIGATION_BAR (bar), NULL);

	return (* NAUTILUS_NAVIGATION_BAR_CLASS (GTK_OBJECT (bar)->klass)->get_location) (bar);
}

/**
 * nautilus_navigation_bar_set_location
 * 
 * Change the location displayed in the navigation bar.
 * 
 * @bar: A NautilusNavigationBar.
 * @location: The uri that should be displayed.
 */
void
nautilus_navigation_bar_set_location (NautilusNavigationBar *bar,
				      const char *location)
{
	g_return_if_fail (NAUTILUS_IS_NAVIGATION_BAR (bar));

	(* NAUTILUS_NAVIGATION_BAR_CLASS (GTK_OBJECT (bar)->klass)->set_location) (bar, location);
}

/**
 * nautilus_navigation_get_mapped_location
 * 
 * apply heuristics to map the uri to something reasonable
 * 
 * @bar: A NautilusNavigationBar.
 * @location: The resulting uri
 */
static char *
nautilus_navigation_bar_get_mapped_location (NautilusNavigationBar *bar)
{
	char *location, *new_location;
	
	g_return_val_if_fail (NAUTILUS_IS_NAVIGATION_BAR (bar), NULL);
	location = nautilus_navigation_bar_get_location (bar);
	/* see if there's a protocol already specified; if so, there's no need to map */
	if (strstr(location, "://"))
		return location;
		
	/* there's no protocol specified, so see if there's an actual file */
	if (g_file_exists (location))
		return location;
		
	/* see if there's anything that looks like a domain name */
	if (strstr (location, ".com") || strstr (location, ".org") || strstr (location, ".net")) {
		new_location =  g_strconcat ("http://", location, NULL);
		g_free (location);
		return new_location;
	}

	return location;
}

void
nautilus_navigation_bar_location_changed (NautilusNavigationBar *bar)
{
	char *location;

	g_return_if_fail (NAUTILUS_IS_NAVIGATION_BAR (bar));

	location = nautilus_navigation_bar_get_mapped_location (bar);
	gtk_signal_emit (GTK_OBJECT (bar),
			 signals[LOCATION_CHANGED],
			 location);
	g_free (location);
}
