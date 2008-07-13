/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-window-slot-info.h: Interface for nautilus window slots
 
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
  
   Author: Christian Neumair <cneumair@gnome.org>
*/

#ifndef NAUTILUS_WINDOW_SLOT_INFO_H
#define NAUTILUS_WINDOW_SLOT_INFO_H

#include "nautilus-window-info.h"
#include "nautilus-view.h"


#define NAUTILUS_TYPE_WINDOW_SLOT_INFO           (nautilus_window_slot_info_get_type ())
#define NAUTILUS_WINDOW_SLOT_INFO(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_WINDOW_SLOT_INFO, NautilusWindowSlotInfo))
#define NAUTILUS_IS_WINDOW_SLOT_INFO(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_WINDOW_SLOT_INFO))
#define NAUTILUS_WINDOW_SLOT_INFO_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NAUTILUS_TYPE_WINDOW_SLOT_INFO, NautilusWindowSlotInfoIface))

typedef struct _NautilusWindowSlotInfoIface NautilusWindowSlotInfoIface;

struct _NautilusWindowSlotInfoIface
{
	GTypeInterface g_iface;

	/* signals */

	/* emitted right after this slot becomes active.
 	 * Views should connect to this signal and merge their UI
 	 * into the main window.
 	 */
	void  (* active)  (NautilusWindowSlotInfo *slot);
	/* emitted right before this slot becomes inactive.
 	 * Views should connect to this signal and unmerge their UI
 	 * from the main window.
 	 */
	void  (* inactive) (NautilusWindowSlotInfo *slot);

	/* returns the window info associated with this slot */
	NautilusWindowInfo * (* get_window) (NautilusWindowSlotInfo *slot);

	/* Returns the number of selected items in the view */	
	int  (* get_selection_count)  (NautilusWindowSlotInfo    *slot);

	/* Returns a list of uris for th selected items in the view, caller frees it */	
	GList *(* get_selection)      (NautilusWindowSlotInfo    *slot);

	char * (* get_current_location)  (NautilusWindowSlotInfo *slot);
	NautilusView * (* get_current_view) (NautilusWindowSlotInfo *slot);
	void   (* set_status)            (NautilusWindowSlotInfo *slot,
					  const char *status);
	char * (* get_title)             (NautilusWindowSlotInfo *slot);

	void   (* open_location)      (NautilusWindowSlotInfo *slot,
				       GFile *location,
				       NautilusWindowOpenMode mode,
				       NautilusWindowOpenFlags flags,
				       GList *selection);
};


GType                             nautilus_window_slot_info_get_type            (void);
NautilusWindowInfo *              nautilus_window_slot_info_get_window          (NautilusWindowSlotInfo            *slot);
void                              nautilus_window_slot_info_open_location       (NautilusWindowSlotInfo            *slot,
										 GFile                             *location,
										 NautilusWindowOpenMode             mode,
										 NautilusWindowOpenFlags            flags,
										 GList                             *selection);
void                              nautilus_window_slot_info_set_status          (NautilusWindowSlotInfo            *slot,
										 const char *status);

char *                            nautilus_window_slot_info_get_current_location (NautilusWindowSlotInfo           *slot);
NautilusView *                    nautilus_window_slot_info_get_current_view     (NautilusWindowSlotInfo           *slot);
int                               nautilus_window_slot_info_get_selection_count  (NautilusWindowSlotInfo           *slot);
GList *                           nautilus_window_slot_info_get_selection        (NautilusWindowSlotInfo           *slot);
char *                            nautilus_window_slot_info_get_title            (NautilusWindowSlotInfo           *slot);

#endif /* NAUTILUS_WINDOW_SLOT_INFO_H */
