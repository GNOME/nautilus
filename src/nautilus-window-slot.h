/*
 * SPDX-FileCopyrightText: 2008 Free Software Foundation, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Christian Neumair <cneumair@gnome.org>
 */

#pragma once

#include <gdk/gdk.h>
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <adwaita.h>

#include "nautilus-types.h"

#define NAUTILUS_TYPE_WINDOW_SLOT (nautilus_window_slot_get_type ())
G_DECLARE_FINAL_TYPE (NautilusWindowSlot, nautilus_window_slot, NAUTILUS, WINDOW_SLOT, AdwBin)

typedef struct
{
    GList *back_list;
    GList *forward_list;
    NautilusBookmark *current_location_bookmark;
} NautilusNavigationState;

NautilusWindowSlot * nautilus_window_slot_new              (NautilusMode        mode);

void nautilus_window_slot_open_location_full               (NautilusWindowSlot *slot,
                                                            GFile              *location,
                                                            NautilusFileList   *new_selection);

GtkFilter *nautilus_window_slot_get_filter                 (NautilusWindowSlot *slot);
void nautilus_window_slot_set_filter                       (NautilusWindowSlot *slot,
                                                            GtkFilter          *filter);
NautilusMode nautilus_window_slot_get_mode                 (NautilusWindowSlot *slot);
GFile * nautilus_window_slot_get_location		   (NautilusWindowSlot *slot);

GList * nautilus_window_slot_get_back_history              (NautilusWindowSlot *slot);
GList * nautilus_window_slot_get_forward_history           (NautilusWindowSlot *slot);

gboolean nautilus_window_slot_get_allow_stop               (NautilusWindowSlot *slot);
void     nautilus_window_slot_stop_loading                 (NautilusWindowSlot *slot);

const gchar *nautilus_window_slot_get_title                (NautilusWindowSlot *slot);

gboolean nautilus_window_slot_handle_activate_files        (NautilusWindowSlot *slot,
                                                            GList              *files);
gboolean nautilus_window_slot_handle_event       	   (NautilusWindowSlot    *slot,
							    GtkEventControllerKey *controller,
							    guint                  keyval,
							    GdkModifierType        state);

const gchar*   nautilus_window_slot_get_icon_name                (NautilusWindowSlot *slot);

const gchar*   nautilus_window_slot_get_tooltip                  (NautilusWindowSlot *slot);
const gchar*   nautilus_window_slot_get_tooltip_with_description (NautilusWindowSlot  *slot,
                                                                  const gchar        **description);

GMenuModel* nautilus_window_slot_get_templates_menu (NautilusWindowSlot *self);

GMenuModel* nautilus_window_slot_get_extensions_background_menu (NautilusWindowSlot *self);

gboolean nautilus_window_slot_get_active                   (NautilusWindowSlot *slot);

void     nautilus_window_slot_set_active                   (NautilusWindowSlot *slot,
                                                            gboolean            active);
gboolean nautilus_window_slot_get_loading                  (NautilusWindowSlot *slot);

gboolean nautilus_window_slot_get_search_visible           (NautilusWindowSlot *slot);

gboolean nautilus_window_slot_get_search_global            (NautilusWindowSlot *self);

GList* nautilus_window_slot_get_selection                  (NautilusWindowSlot *slot);

NautilusSelectionSource nautilus_window_slot_get_selection_source (NautilusWindowSlot *self);

void     nautilus_window_slot_search                       (NautilusWindowSlot *slot,
                                                            NautilusQuery      *query);

void nautilus_window_slot_restore_navigation_state (NautilusWindowSlot      *self,
                                                    NautilusNavigationState *data);

NautilusNavigationState* nautilus_window_slot_get_navigation_state (NautilusWindowSlot *self);

NautilusQueryEditor *nautilus_window_slot_get_query_editor (NautilusWindowSlot *self);

NautilusFilesView*  nautilus_window_slot_get_current_view  (NautilusWindowSlot *slot);

void
nautilus_window_slot_navigate (NautilusWindowSlot *self,
                               int                 distance);

void free_navigation_state                                 (gpointer data);
