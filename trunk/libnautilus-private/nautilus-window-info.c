/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-window-info.c: Interface for nautilus window
 
   Copyright (C) 2004 Red Hat Inc.
  
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
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#include <config.h>
#include "nautilus-window-info.h"

enum {
	LOADING_URI,
	SELECTION_CHANGED,
	TITLE_CHANGED,
	HIDDEN_FILES_MODE_CHANGED,
	LAST_SIGNAL
};

static guint nautilus_window_info_signals[LAST_SIGNAL] = { 0 };

static void
nautilus_window_info_base_init (gpointer g_class)
{
	static gboolean initialized = FALSE;

	if (! initialized) {
		nautilus_window_info_signals[LOADING_URI] =
			g_signal_new ("loading_uri",
				      NAUTILUS_TYPE_WINDOW_INFO,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (NautilusWindowInfoIface, loading_uri),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__STRING,
				      G_TYPE_NONE, 1,
				      G_TYPE_STRING);
		
		nautilus_window_info_signals[SELECTION_CHANGED] =
			g_signal_new ("selection_changed",
				      NAUTILUS_TYPE_WINDOW_INFO,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (NautilusWindowInfoIface, selection_changed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);
		
		nautilus_window_info_signals[TITLE_CHANGED] =
			g_signal_new ("title_changed",
				      NAUTILUS_TYPE_WINDOW_INFO,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (NautilusWindowInfoIface, title_changed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__STRING,
				      G_TYPE_NONE, 1,
				      G_TYPE_STRING);
		
		nautilus_window_info_signals[HIDDEN_FILES_MODE_CHANGED] =
			g_signal_new ("hidden_files_mode_changed",
				      NAUTILUS_TYPE_WINDOW_INFO,
				      G_SIGNAL_RUN_LAST,
				      G_STRUCT_OFFSET (NautilusWindowInfoIface, hidden_files_mode_changed),
				      NULL, NULL,
				      g_cclosure_marshal_VOID__VOID,
				      G_TYPE_NONE, 0);
		
		initialized = TRUE;
	}
}

GType                   
nautilus_window_info_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		const GTypeInfo info = {
			sizeof (NautilusWindowInfoIface),
			nautilus_window_info_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, 
					       "NautilusWindowInfo",
					       &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}
	
	return type;
}

void
nautilus_window_info_report_load_underway (NautilusWindowInfo      *window,
					   NautilusView            *view)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW_INFO (window));
	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	(* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->report_load_underway) (window,
									 view);
}

void
nautilus_window_info_report_load_complete (NautilusWindowInfo      *window,
					   NautilusView            *view)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW_INFO (window));
	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	(* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->report_load_complete) (window,
									  view);
}

void
nautilus_window_info_report_view_failed (NautilusWindowInfo      *window,
					 NautilusView            *view)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW_INFO (window));
	g_return_if_fail (NAUTILUS_IS_VIEW (view));

	(* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->report_view_failed) (window,
								       view);
}

void
nautilus_window_info_report_selection_changed (NautilusWindowInfo      *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW_INFO (window));

	(* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->report_selection_changed) (window);
}

void
nautilus_window_info_open_location (NautilusWindowInfo      *window,
				    GFile                   *location,
				    NautilusWindowOpenMode   mode,
				    NautilusWindowOpenFlags  flags,
				    GList                   *selection)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW_INFO (window));

	(* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->open_location) (window,
								  location,
								  mode,
								  flags,
								  selection);
}

void
nautilus_window_info_show_window (NautilusWindowInfo      *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW_INFO (window));

	(* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->show_window) (window);
}

void
nautilus_window_info_close (NautilusWindowInfo      *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW_INFO (window));

	(* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->close_window) (window);
}

void
nautilus_window_info_set_status (NautilusWindowInfo      *window,
				 const char              *status)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW_INFO (window));

	(* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->set_status) (window,
							       status);
}

NautilusWindowType
nautilus_window_info_get_window_type (NautilusWindowInfo *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW_INFO (window), NAUTILUS_WINDOW_SPATIAL);

	return (* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->get_window_type) (window);
}

char *
nautilus_window_info_get_title (NautilusWindowInfo *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW_INFO (window), NULL);
	
	return (* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->get_title) (window);
}

GList *
nautilus_window_info_get_history (NautilusWindowInfo *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW_INFO (window), NULL);
	
	return (* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->get_history) (window);
}

NautilusBookmarkList *
nautilus_window_info_get_bookmark_list (NautilusWindowInfo *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW_INFO (window), NULL);
	
	return (* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->get_bookmark_list) (window);
}


char *
nautilus_window_info_get_current_location (NautilusWindowInfo *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW_INFO (window), NULL);
	
	return (* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->get_current_location) (window);
}

int
nautilus_window_info_get_selection_count (NautilusWindowInfo *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW_INFO (window), 0);

	return (* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->get_selection_count) (window);
}

GList *
nautilus_window_info_get_selection (NautilusWindowInfo *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW_INFO (window), NULL);

	return (* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->get_selection) (window);
}

NautilusWindowShowHiddenFilesMode
nautilus_window_info_get_hidden_files_mode (NautilusWindowInfo *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW_INFO (window), NAUTILUS_WINDOW_SHOW_HIDDEN_FILES_DEFAULT);

	return (* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->get_hidden_files_mode) (window);
}

void
nautilus_window_info_set_hidden_files_mode (NautilusWindowInfo *window,
					    NautilusWindowShowHiddenFilesMode  mode)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW_INFO (window));

	(* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->set_hidden_files_mode) (window,
									    mode);
}

GtkUIManager *
nautilus_window_info_get_ui_manager (NautilusWindowInfo *window)
{
	g_return_val_if_fail (NAUTILUS_IS_WINDOW_INFO (window), NULL);
	
	return (* NAUTILUS_WINDOW_INFO_GET_IFACE (window)->get_ui_manager) (window);
}

