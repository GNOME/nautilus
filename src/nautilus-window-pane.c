/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-window-pane.c: Nautilus window pane

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

#include "nautilus-window-pane.h"

static void nautilus_window_pane_init       (NautilusWindowPane *pane);
static void nautilus_window_pane_class_init (NautilusWindowPaneClass *class);
static void nautilus_window_pane_dispose    (GObject *object);

G_DEFINE_TYPE (NautilusWindowPane,
	       nautilus_window_pane,
	       G_TYPE_OBJECT)
#define parent_class nautilus_window_pane_parent_class


static void
nautilus_window_pane_init (NautilusWindowPane *pane)
{
	pane->slots = NULL;
	pane->active_slots = NULL;
	pane->active_slot = NULL;
}

static void
nautilus_window_pane_class_init (NautilusWindowPaneClass *class)
{
	G_OBJECT_CLASS (class)->dispose = nautilus_window_pane_dispose;
}

static void
nautilus_window_pane_dispose (GObject *object)
{
	NautilusWindowPane *pane = NAUTILUS_WINDOW_PANE (object);

	g_assert (pane->slots == NULL);

	pane->window = NULL;
	G_OBJECT_CLASS (parent_class)->dispose (object);
}

NautilusWindowPane *
nautilus_window_pane_new (NautilusWindow *window)
{
	NautilusWindowPane *pane;

	pane = g_object_new (NAUTILUS_TYPE_WINDOW_PANE, NULL);
	pane->window = window;
	return pane;
}
