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
#include "nautilus-window-private.h"
#include "nautilus-navigation-window-pane.h"
#include "nautilus-window-manage-views.h"
#include <eel/eel-gtk-macros.h>

static void nautilus_window_pane_init       (NautilusWindowPane *pane);
static void nautilus_window_pane_class_init (NautilusWindowPaneClass *class);
static void nautilus_window_pane_dispose    (GObject *object);

G_DEFINE_TYPE (NautilusWindowPane,
	       nautilus_window_pane,
	       G_TYPE_OBJECT)
#define parent_class nautilus_window_pane_parent_class


static inline NautilusWindowSlot *
get_first_inactive_slot (NautilusWindowPane *pane)
{
	GList *l;
	NautilusWindowSlot *slot;

	for (l = pane->slots; l != NULL; l = l->next) {
		slot = NAUTILUS_WINDOW_SLOT (l->data);
		if (slot != pane->active_slot) {
			return slot;
		}
	}

	return NULL;
}

void
nautilus_window_pane_show (NautilusWindowPane *pane)
{
	pane->visible = TRUE;
	EEL_CALL_METHOD (NAUTILUS_WINDOW_PANE_CLASS, pane,
			 show, (pane));
}

void
nautilus_window_pane_zoom_in (NautilusWindowPane *pane)
{
	NautilusWindowSlot *slot;

	g_assert (pane != NULL);

	nautilus_window_set_active_pane (pane->window, pane);

	slot = pane->active_slot;
	if (slot->content_view != NULL) {
		nautilus_view_bump_zoom_level (slot->content_view, 1);
	}
}

void
nautilus_window_pane_zoom_to_level (NautilusWindowPane *pane,
			       NautilusZoomLevel level)
{
	NautilusWindowSlot *slot;

	g_assert (pane != NULL);

	nautilus_window_set_active_pane (pane->window, pane);

	slot = pane->active_slot;
	if (slot->content_view != NULL) {
		nautilus_view_zoom_to_level (slot->content_view, level);
	}
}

void
nautilus_window_pane_zoom_out (NautilusWindowPane *pane)
{
	NautilusWindowSlot *slot;

	g_assert (pane != NULL);

	nautilus_window_set_active_pane (pane->window, pane);

	slot = pane->active_slot;
	if (slot->content_view != NULL) {
		nautilus_view_bump_zoom_level (slot->content_view, -1);
	}
}

void
nautilus_window_pane_zoom_to_default (NautilusWindowPane *pane)
{
	NautilusWindowSlot *slot;

	g_assert (pane != NULL);

	nautilus_window_set_active_pane (pane->window, pane);

	slot = pane->active_slot;
	if (slot->content_view != NULL) {
		nautilus_view_restore_default_zoom_level (slot->content_view);
	}
}

void
nautilus_window_pane_slot_close (NautilusWindowPane *pane, NautilusWindowSlot *slot)
{
	NautilusWindowSlot *next_slot;

	if (pane->window) {
		NautilusWindow *window;
		window = pane->window;
		if (pane->active_slot == slot) {
			g_assert (pane->active_slots != NULL);
			g_assert (pane->active_slots->data == slot);

			next_slot = NULL;
			if (pane->active_slots->next != NULL) {
				next_slot = NAUTILUS_WINDOW_SLOT (pane->active_slots->next->data);
			}

			if (next_slot == NULL) {
				next_slot = get_first_inactive_slot (NAUTILUS_WINDOW_PANE (pane));
			}

			nautilus_window_set_active_slot (window, next_slot);
		}
		nautilus_window_close_slot (slot);

		/* If that was the last slot in the active pane, close the pane or even the whole window. */
		if (window->details->active_pane->slots == NULL) {
			NautilusWindowPane *next_pane;
			next_pane = nautilus_window_get_next_pane (window);
			
			/* If next_pane is non-NULL, we have more than one pane available. In this
			 * case, close the current pane and switch to the next one. If there is
			 * no next pane, close the window. */
			if(next_pane) {
				nautilus_window_close_pane (pane);
				nautilus_window_pane_switch_to (next_pane);
				if (NAUTILUS_IS_NAVIGATION_WINDOW (window)) {
					nautilus_navigation_window_update_show_hide_menu_items (NAUTILUS_NAVIGATION_WINDOW (window));
				}
			} else {
				nautilus_window_close (window);
			}
		}
	}
}

static void
real_sync_location_widgets (NautilusWindowPane *pane)
{
	NautilusWindowSlot *slot;

	/* TODO: Would be nice with a real subclass for spatial panes */
	g_assert (NAUTILUS_IS_SPATIAL_WINDOW (pane->window));

	slot = pane->active_slot;

	/* Change the location button to match the current location. */
	nautilus_spatial_window_set_location_button (NAUTILUS_SPATIAL_WINDOW (pane->window),
						     slot->location);
}


void
nautilus_window_pane_sync_location_widgets (NautilusWindowPane *pane)
{
	EEL_CALL_METHOD (NAUTILUS_WINDOW_PANE_CLASS, pane,
			 sync_location_widgets, (pane));
}

void
nautilus_window_pane_sync_search_widgets (NautilusWindowPane *pane)
{
	g_assert (NAUTILUS_IS_WINDOW_PANE (pane));

	EEL_CALL_METHOD (NAUTILUS_WINDOW_PANE_CLASS, pane,
			 sync_search_widgets, (pane));
}

void
nautilus_window_pane_grab_focus (NautilusWindowPane *pane)
{
	if (NAUTILUS_IS_WINDOW_PANE (pane) && pane->active_slot) {
		nautilus_view_grab_focus (pane->active_slot->content_view);
	}	
}

void
nautilus_window_pane_switch_to (NautilusWindowPane *pane)
{
	nautilus_window_pane_grab_focus (pane);
}

static void
nautilus_window_pane_init (NautilusWindowPane *pane)
{
	pane->slots = NULL;
	pane->active_slots = NULL;
	pane->active_slot = NULL;
	pane->is_active = FALSE;
}

void
nautilus_window_pane_set_active (NautilusWindowPane *pane, gboolean is_active)
{
	if (is_active == pane->is_active) {
		return;
	}

	pane->is_active = is_active;

	/* notify the current slot about its activity state (so that it can e.g. modify the bg color) */
	nautilus_window_slot_is_in_active_pane (pane->active_slot, is_active);

	EEL_CALL_METHOD (NAUTILUS_WINDOW_PANE_CLASS, pane,
			 set_active, (pane, is_active));
}

static void
nautilus_window_pane_class_init (NautilusWindowPaneClass *class)
{
	G_OBJECT_CLASS (class)->dispose = nautilus_window_pane_dispose;
	NAUTILUS_WINDOW_PANE_CLASS (class)->sync_location_widgets = real_sync_location_widgets;
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

NautilusWindowSlot *
nautilus_window_pane_get_slot_for_content_box (NautilusWindowPane *pane,
					       GtkWidget *content_box)
{
	NautilusWindowSlot *slot;
	GList *l;

	for (l = pane->slots; l != NULL; l = l->next) {
		slot = NAUTILUS_WINDOW_SLOT (l->data);

		if (slot->content_box == content_box) {
			return slot;
		}
	}
	return NULL;
}
