/*
   nautilus-window-slot.h: Nautilus window slot
 
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
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
  
   Author: Christian Neumair <cneumair@gnome.org>
*/

#pragma once

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include "nautilus-types.h"

typedef enum {
	NAUTILUS_LOCATION_CHANGE_STANDARD,
	NAUTILUS_LOCATION_CHANGE_BACK,
	NAUTILUS_LOCATION_CHANGE_FORWARD,
	NAUTILUS_LOCATION_CHANGE_RELOAD
} NautilusLocationChangeType;

#define NAUTILUS_TYPE_WINDOW_SLOT (nautilus_window_slot_get_type ())
G_DECLARE_FINAL_TYPE (NautilusWindowSlot, nautilus_window_slot, NAUTILUS, WINDOW_SLOT, GtkBox)

typedef struct
{
    GList *back_list;
    GList *forward_list;
    NautilusBookmark *current_location_bookmark;
    NautilusQuery *current_search_query;
} NautilusNavigationState;

NautilusWindowSlot * nautilus_window_slot_new              (NautilusWindow     *window);

NautilusWindow * nautilus_window_slot_get_window           (NautilusWindowSlot *slot);
void             nautilus_window_slot_set_window           (NautilusWindowSlot *slot,
							    NautilusWindow     *window);

void nautilus_window_slot_open_location_full               (NautilusWindowSlot *slot,
                                                            GFile              *location,
                                                            NautilusOpenFlags   flags,
                                                            GList              *new_selection);

GFile * nautilus_window_slot_get_location		   (NautilusWindowSlot *slot);
GFile * nautilus_window_slot_get_pending_location          (NautilusWindowSlot *slot);

NautilusBookmark *nautilus_window_slot_get_bookmark        (NautilusWindowSlot *slot);

GList * nautilus_window_slot_get_back_history              (NautilusWindowSlot *slot);
GList * nautilus_window_slot_get_forward_history           (NautilusWindowSlot *slot);

gboolean nautilus_window_slot_get_allow_stop               (NautilusWindowSlot *slot);
void     nautilus_window_slot_set_allow_stop		   (NautilusWindowSlot *slot,
							    gboolean	        allow_stop);
void     nautilus_window_slot_stop_loading                 (NautilusWindowSlot *slot);

const gchar *nautilus_window_slot_get_title                (NautilusWindowSlot *slot);
void         nautilus_window_slot_update_title		   (NautilusWindowSlot *slot);

gboolean nautilus_window_slot_handle_event       	   (NautilusWindowSlot    *slot,
							    GtkEventControllerKey *controller,
							    guint                  keyval,
							    GdkModifierType        state);

void    nautilus_window_slot_queue_reload		   (NautilusWindowSlot *slot);

const gchar*   nautilus_window_slot_get_icon_name                (NautilusWindowSlot *slot);

const gchar*   nautilus_window_slot_get_tooltip                  (NautilusWindowSlot *slot);

NautilusToolbarMenuSections * nautilus_window_slot_get_toolbar_menu_sections (NautilusWindowSlot *slot);

GMenuModel* nautilus_window_slot_get_templates_menu (NautilusWindowSlot *self);

GMenuModel* nautilus_window_slot_get_extensions_background_menu (NautilusWindowSlot *self);

gboolean nautilus_window_slot_get_active                   (NautilusWindowSlot *slot);

void     nautilus_window_slot_set_active                   (NautilusWindowSlot *slot,
                                                            gboolean            active);
gboolean nautilus_window_slot_get_loading                  (NautilusWindowSlot *slot);

gboolean nautilus_window_slot_get_search_visible           (NautilusWindowSlot *slot);

GList* nautilus_window_slot_get_selection                  (NautilusWindowSlot *slot);

void     nautilus_window_slot_search                       (NautilusWindowSlot *slot,
                                                            NautilusQuery      *query);

void nautilus_window_slot_restore_navigation_state (NautilusWindowSlot      *self,
                                                    NautilusNavigationState *data);

NautilusNavigationState* nautilus_window_slot_get_navigation_state (NautilusWindowSlot *self);

NautilusQueryEditor *nautilus_window_slot_get_query_editor (NautilusWindowSlot *self);

gboolean nautilus_window_slot_is_in_search_everywhere      (NautilusWindowSlot *self);

/* Only used by slot-dnd */
NautilusView*  nautilus_window_slot_get_current_view       (NautilusWindowSlot *slot);

void nautilus_window_slot_back_or_forward                  (NautilusWindowSlot *slot,
                                                            gboolean            back,
                                                            guint               distance);

void nautilus_window_slot_go_up                            (NautilusWindowSlot *slot);
void nautilus_window_slot_go_down                          (NautilusWindowSlot *slot);

void free_navigation_state                                 (gpointer data);
