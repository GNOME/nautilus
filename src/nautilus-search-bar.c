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
*/

#include <config.h>
#include "nautilus-search-bar.h"

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include <gtk/gtklabel.h>

#include <libnautilus-extensions/nautilus-gtk-macros.h>

static void nautilus_search_bar_set_location     (NautilusNavigationBar *bar,
						  const char            *location);


static void nautilus_search_bar_initialize_class (NautilusSearchBarClass *class);
static void nautilus_search_bar_initialize       (NautilusSearchBar      *bar);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSearchBar, nautilus_search_bar, NAUTILUS_TYPE_NAVIGATION_BAR)


static void
destroy (GtkObject *object)
{
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
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
}

static void
nautilus_search_bar_initialize (NautilusSearchBar *bar)
{
	GtkWidget *label;

	/* FIXME: set up the widgetry here. */

	label = gtk_label_new (_("The search bar goes here"));

	gtk_widget_show (label);

	gtk_container_add   (GTK_CONTAINER (bar), label);
}


GtkWidget *
nautilus_search_bar_new (void)
{
	return gtk_widget_new (nautilus_search_bar_get_type (), NULL);
}


static void
nautilus_search_bar_set_location (NautilusNavigationBar *bar,
				  const char *location)
{
	/* FIXME: should check if URI is search URI, and if so,
	   set up the controls properly. */
}

