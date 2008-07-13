/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-window-slot-info.c: Interface for nautilus window slots
 
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
#include "nautilus-window-slot-info.h"

enum {
	ACTIVE,
	INACTIVE,
	LAST_SIGNAL
};

static guint nautilus_window_slot_info_signals[LAST_SIGNAL] = { 0 };

static void
nautilus_window_slot_info_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (!initialized) {
		nautilus_window_slot_info_signals[ACTIVE] =
			g_signal_new ("active",
				      NAUTILUS_TYPE_WINDOW_SLOT_INFO,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (NautilusWindowSlotInfoIface, active),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);

		nautilus_window_slot_info_signals[INACTIVE] =
			g_signal_new ("inactive",
				      NAUTILUS_TYPE_WINDOW_SLOT_INFO,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (NautilusWindowSlotInfoIface, inactive),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);
		
		initialized = TRUE;
	}
}

GType                   
nautilus_window_slot_info_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		const GTypeInfo info = {
			sizeof (NautilusWindowSlotInfoIface),
			nautilus_window_slot_info_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, 
					       "NautilusWindowSlotInfo",
					       &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}
	
	return type;
}

void
nautilus_window_slot_info_set_status (NautilusWindowSlotInfo *slot,
				      const char             *status)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT_INFO (slot));

	(* NAUTILUS_WINDOW_SLOT_INFO_GET_IFACE (slot)->set_status) (slot,
								    status);
}


void
nautilus_window_slot_info_open_location (NautilusWindowSlotInfo  *slot,
					 GFile                   *location,
					 NautilusWindowOpenMode   mode,
					 NautilusWindowOpenFlags  flags,
					 GList                   *selection)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT_INFO (slot));

	(* NAUTILUS_WINDOW_SLOT_INFO_GET_IFACE (slot)->open_location) (slot,
								       location,
								       mode,
								       flags,
								       selection);
}

char *
nautilus_window_slot_info_get_title (NautilusWindowSlotInfo *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT_INFO (slot));
	
	return (* NAUTILUS_WINDOW_SLOT_INFO_GET_IFACE (slot)->get_title) (slot);
}

char *
nautilus_window_slot_info_get_current_location (NautilusWindowSlotInfo *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT_INFO (slot));
	
	return (* NAUTILUS_WINDOW_SLOT_INFO_GET_IFACE (slot)->get_current_location) (slot);
}

NautilusView *
nautilus_window_slot_info_get_current_view (NautilusWindowSlotInfo *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT_INFO (slot));
	
	return (* NAUTILUS_WINDOW_SLOT_INFO_GET_IFACE (slot)->get_current_view) (slot);
}

NautilusWindowInfo *
nautilus_window_slot_info_get_window (NautilusWindowSlotInfo *slot)
{
	g_assert (NAUTILUS_IS_WINDOW_SLOT_INFO (slot));

	return (* NAUTILUS_WINDOW_SLOT_INFO_GET_IFACE (slot)->get_window) (slot);
}

