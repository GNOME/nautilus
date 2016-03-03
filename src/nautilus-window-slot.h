/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

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

#ifndef NAUTILUS_WINDOW_SLOT_H
#define NAUTILUS_WINDOW_SLOT_H

#include "nautilus-query-editor.h"

typedef struct NautilusWindowSlot NautilusWindowSlot;
typedef struct NautilusWindowSlotClass NautilusWindowSlotClass;
typedef struct NautilusWindowSlotDetails NautilusWindowSlotDetails;

#include "nautilus-files-view.h"
#include "nautilus-view.h"
#include "nautilus-window.h"

#define NAUTILUS_TYPE_WINDOW_SLOT	 (nautilus_window_slot_get_type())
#define NAUTILUS_WINDOW_SLOT_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), NAUTILUS_TYPE_WINDOW_SLOT, NautilusWindowSlotClass))
#define NAUTILUS_WINDOW_SLOT(obj)	 (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_WINDOW_SLOT, NautilusWindowSlot))
#define NAUTILUS_IS_WINDOW_SLOT(obj)      (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_WINDOW_SLOT))
#define NAUTILUS_IS_WINDOW_SLOT_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), NAUTILUS_TYPE_WINDOW_SLOT))
#define NAUTILUS_WINDOW_SLOT_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), NAUTILUS_TYPE_WINDOW_SLOT, NautilusWindowSlotClass))

typedef enum {
	NAUTILUS_LOCATION_CHANGE_STANDARD,
	NAUTILUS_LOCATION_CHANGE_BACK,
	NAUTILUS_LOCATION_CHANGE_FORWARD,
	NAUTILUS_LOCATION_CHANGE_RELOAD
} NautilusLocationChangeType;

struct NautilusWindowSlotClass {
	GtkBoxClass parent_class;

	/* wrapped NautilusWindowInfo signals, for overloading */
	void (* active)   (NautilusWindowSlot *slot);
	void (* inactive) (NautilusWindowSlot *slot);
};

/* Each NautilusWindowSlot corresponds to a location in the window
 * for displaying a NautilusFilesView, i.e. a tab.
 */
struct NautilusWindowSlot {
	GtkBox parent;

	NautilusWindowSlotDetails *details;
};

GType   nautilus_window_slot_get_type (void);

NautilusWindowSlot * nautilus_window_slot_new              (NautilusWindow     *window);

NautilusWindow * nautilus_window_slot_get_window           (NautilusWindowSlot *slot);
void             nautilus_window_slot_set_window           (NautilusWindowSlot *slot,
							    NautilusWindow     *window);

void nautilus_window_slot_open_location_full              (NautilusWindowSlot      *slot,
                                                           GFile                   *location,
                                                           NautilusWindowOpenFlags  flags,
                                                           GList                   *new_selection);

GFile * nautilus_window_slot_get_location		   (NautilusWindowSlot *slot);

NautilusBookmark *nautilus_window_slot_get_bookmark        (NautilusWindowSlot *slot);

GList * nautilus_window_slot_get_back_history              (NautilusWindowSlot *slot);
GList * nautilus_window_slot_get_forward_history           (NautilusWindowSlot *slot);

gboolean nautilus_window_slot_get_allow_stop               (NautilusWindowSlot *slot);
void     nautilus_window_slot_set_allow_stop		   (NautilusWindowSlot *slot,
							    gboolean	        allow_stop);
void     nautilus_window_slot_stop_loading                 (NautilusWindowSlot *slot);

const gchar *nautilus_window_slot_get_title                (NautilusWindowSlot *slot);
void         nautilus_window_slot_update_title		   (NautilusWindowSlot *slot);

gboolean nautilus_window_slot_handle_event       	   (NautilusWindowSlot *slot,
							    GdkEventKey        *event);

void    nautilus_window_slot_queue_reload		   (NautilusWindowSlot *slot);

GIcon*   nautilus_window_slot_get_icon                     (NautilusWindowSlot *slot);

GtkWidget* nautilus_window_slot_get_view_widget            (NautilusWindowSlot *slot);

gboolean nautilus_window_slot_get_active                   (NautilusWindowSlot *slot);

void     nautilus_window_slot_set_active                   (NautilusWindowSlot *slot,
                                                            gboolean            active);
gboolean nautilus_window_slot_get_loading                  (NautilusWindowSlot *slot);

void     nautilus_window_slot_search                       (NautilusWindowSlot *slot,
                                                            const gchar        *text);

/* Only used by slot-dnd */
NautilusView*  nautilus_window_slot_get_current_view       (NautilusWindowSlot *slot);

#endif /* NAUTILUS_WINDOW_SLOT_H */
