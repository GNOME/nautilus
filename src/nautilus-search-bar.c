/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-search-bar.c - Search bar for Nautilus

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
           Rebecca Schulman <rebecka@eazel.com>
*/

#include <config.h>
#include "nautilus-search-bar.h"
#include "nautilus-simple-search-bar.h"
#include "nautilus-complex-search-bar.h"

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include <gtk/gtksignal.h>
#include <gtk/gtkeventbox.h>

#include <libgnomeui/gnome-uidefs.h>

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-entry.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-directory.h>

static void                       nautilus_search_bar_initialize_class (NautilusSearchBarClass *class);
static void                       nautilus_search_bar_initialize       (NautilusSearchBar      *bar);

static void                       nautilus_search_bar_set_location     (NautilusNavigationBar *navigation_bar,
									const char *location);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSearchBar, nautilus_search_bar, NAUTILUS_TYPE_NAVIGATION_BAR)



NAUTILUS_IMPLEMENT_MUST_OVERRIDE_SIGNAL (nautilus_search_bar, set_search_controls)

static void
destroy (GtkObject *object)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


GtkWidget *
nautilus_search_bar_new (void)
{
	return gtk_widget_new (NAUTILUS_TYPE_SEARCH_BAR, NULL);
}


static void
nautilus_search_bar_initialize_class (NautilusSearchBarClass *klass)
{
	
	GtkObjectClass *object_class;
	NautilusNavigationBarClass *navigation_bar_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	object_class->destroy = destroy;
	
	navigation_bar_class = NAUTILUS_NAVIGATION_BAR_CLASS (klass);

	navigation_bar_class->set_location = nautilus_search_bar_set_location;

	NAUTILUS_ASSIGN_MUST_OVERRIDE_SIGNAL (klass, nautilus_search_bar, set_search_controls);
}

static void
nautilus_search_bar_initialize (NautilusSearchBar *bar)
{

}



static void
nautilus_search_bar_set_location (NautilusNavigationBar *navigation_bar,
				  const char *location)
{
	NautilusSearchBar *search_bar;

	search_bar = NAUTILUS_SEARCH_BAR (navigation_bar);

	(* NAUTILUS_SEARCH_BAR_CLASS (GTK_OBJECT (search_bar)->klass)->set_search_controls)
		(search_bar, location);

}

/* This is here for children (nautilus-switchable-search-bar, for one)
   to use */
void
nautilus_search_bar_set_search_controls (NautilusSearchBar *search_bar,
					 const char *location)
{
	(* NAUTILUS_SEARCH_BAR_CLASS (GTK_OBJECT (search_bar)->klass)->set_search_controls)
		(search_bar, location);

}

