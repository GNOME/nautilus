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

/* nautilus-navigation-bar.c - Abstract navigation bar class
 */

#include <config.h>
#include "nautilus-navigation-bar.h"

#include <eel/eel-gtk-macros.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkbindings.h>
#include <string.h>

enum {
	ACTIVATE,
	CANCEL,
	LOCATION_CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

static void nautilus_navigation_bar_class_init (NautilusNavigationBarClass *class);
static void nautilus_navigation_bar_init       (NautilusNavigationBar      *bar);

EEL_CLASS_BOILERPLATE (NautilusNavigationBar, nautilus_navigation_bar, GTK_TYPE_HBOX)

EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_navigation_bar, get_location)
EEL_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_navigation_bar, set_location)

static void
nautilus_navigation_bar_class_init (NautilusNavigationBarClass *klass)
{
	GtkObjectClass *object_class;
	GtkBindingSet *binding_set;

	object_class = GTK_OBJECT_CLASS (klass);
	
	signals[ACTIVATE] = g_signal_new
		("activate",
		 G_TYPE_FROM_CLASS (object_class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusNavigationBarClass,
				    activate),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);

	signals[CANCEL] = g_signal_new
		("cancel",
		 G_TYPE_FROM_CLASS (object_class),
		 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
		 G_STRUCT_OFFSET (NautilusNavigationBarClass,
				    cancel),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);

	signals[LOCATION_CHANGED] = g_signal_new
		("location_changed",
		 G_TYPE_FROM_CLASS (object_class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusNavigationBarClass,
				    location_changed),
		 NULL, NULL,
		 g_cclosure_marshal_VOID__STRING,
		 G_TYPE_NONE, 1, G_TYPE_STRING);

	klass->activate = NULL;
	klass->cancel = NULL;

	binding_set = gtk_binding_set_by_class (klass);
	gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0, "cancel", 0);

	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_navigation_bar, get_location);
	EEL_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_navigation_bar, set_location);
}

static void
nautilus_navigation_bar_init (NautilusNavigationBar *bar)
{
}

/**
 * nautilus_navigation_bar_activate
 * 
 * Change the navigation bar to an active state.
 * 
 * @bar: A NautilusNavigationBar.
 */
void
nautilus_navigation_bar_activate (NautilusNavigationBar *bar)
{
	g_return_if_fail (NAUTILUS_IS_NAVIGATION_BAR (bar));

	g_signal_emit (bar, signals[ACTIVATE], 0);
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

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_NAVIGATION_BAR_CLASS, bar,
		 get_location, (bar));
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

	EEL_CALL_METHOD (NAUTILUS_NAVIGATION_BAR_CLASS, bar,
			      set_location, (bar, location));
}

void
nautilus_navigation_bar_location_changed (NautilusNavigationBar *bar)
{
	char *location;

	g_return_if_fail (NAUTILUS_IS_NAVIGATION_BAR (bar));

	location = nautilus_navigation_bar_get_location (bar);
	g_signal_emit (bar,
			 signals[LOCATION_CHANGED], 0,
			 location);
	g_free (location);
}
