/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-navigation-window-pane.c: Nautilus navigation window pane
 
   Copyright (C) 2008 Free Software Foundation, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Holger Berndt <berndth@gmx.de>
*/

#include "nautilus-navigation-window-pane.h"

static void nautilus_navigation_window_pane_init       (NautilusNavigationWindowPane *pane);
static void nautilus_navigation_window_pane_class_init (NautilusNavigationWindowPaneClass *class);
static void nautilus_navigation_window_pane_dispose    (GObject *object);

G_DEFINE_TYPE (NautilusNavigationWindowPane,
               nautilus_navigation_window_pane,
               NAUTILUS_TYPE_WINDOW_PANE)
#define parent_class nautilus_navigation_window_pane_parent_class


static void
nautilus_navigation_window_pane_init (NautilusNavigationWindowPane *pane)
{
}

static void
nautilus_navigation_window_pane_class_init (NautilusNavigationWindowPaneClass *class)
{
	G_OBJECT_CLASS (class)->dispose = nautilus_navigation_window_pane_dispose;
}

static void
nautilus_navigation_window_pane_dispose (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

NautilusNavigationWindowPane *
nautilus_navigation_window_pane_new (NautilusWindow *window)
{
	NautilusNavigationWindowPane *pane;

	pane = g_object_new (NAUTILUS_TYPE_NAVIGATION_WINDOW_PANE, NULL);
	NAUTILUS_WINDOW_PANE(pane)->window = window;

	return pane;
}
