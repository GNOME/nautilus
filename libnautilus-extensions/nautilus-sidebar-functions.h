/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-sidebar-functions.h - Sidebar functions used throughout Nautilus.

   Copyright (C) 2001 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_SIDEBAR_FUNCTIONS_H
#define NAUTILUS_SIDEBAR_FUNCTIONS_H

#include <glib.h>

extern const char nautilus_sidebar_news_enabled_preference_name[];
extern const char nautilus_sidebar_notes_enabled_preference_name[];
extern const char nautilus_sidebar_help_enabled_preference_name[];
extern const char nautilus_sidebar_history_enabled_preference_name[];
extern const char nautilus_sidebar_tree_enabled_preference_name[];

/*
 * A callback which can be invoked for each sidebar panel available.
 */
typedef void (*NautilusSidebarPanelCallback) (const char *name,
					      const char *iid,
					      const char *preference_key,
					      gpointer callback_data);

GList *nautilus_sidebar_get_enabled_sidebar_panel_view_identifiers (void);
void   nautilus_sidebar_for_each_panel                             (NautilusSidebarPanelCallback callback,
								    gpointer                     callback_data);

#endif /* NAUTILUS_SIDEBAR_FUNCTIONS_H */

