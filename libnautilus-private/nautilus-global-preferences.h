/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-global-prefs.h - Nautilus main preferences api.

   Copyright (C) 1999, 2000 Eazel, Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with this program; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_PREFS_GLOBAL_H
#define NAUTILUS_PREFS_GLOBAL_H

#include <gnome.h>
#include <nautilus-widgets/nautilus-preferences.h>

BEGIN_GNOME_DECLS

/* Window options */
#define NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW			"/nautilus/preferences/window_always_new"

/* Show hidden files  */
#define NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES  		"/nautilus/preferences/show_hidden_files"

/* sidebar width */
#define NAUTILUS_PREFERENCES_SIDEBAR_WIDTH  			"/nautilus/preferences/sidebar_width"

/* Home URI  */
#define NAUTILUS_PREFERENCES_HOME_URI                 		"/nautilus/preferences/home_uri"

/* adding/removing from property browser */
#define NAUTILUS_PREFERENCES_CAN_ADD_CONTENT			"/nautilus/preferences/can_add_content"

/* Preferences not (currently?) displayed in dialog */
#define NAUTILUS_PREFERENCES_ICON_VIEW_TEXT_ATTRIBUTE_NAMES	"/nautilus/icon_view/text_attribute_names"
#define NAUTILUS_PREFERENCES_SHOW_REAL_FILE_NAME		"/nautilus/preferences/show_real_file_name"

/* Single/Double click preference  */
#define NAUTILUS_PREFERENCES_CLICK_POLICY			"/nautilus/preferences/click_policy"

/* use anti-aliased canvas */
#define NAUTILUS_PREFERENCES_ANTI_ALIASED_CANVAS		"/nautilus/preferences/anti_aliased_canvas"

/* Sidebar panels */
#define NAUTILUS_PREFERENCES_SIDEBAR_PANELS_NAMESPACE		"/nautilus/sidebar-panels"

/* themes */
#define NAUTILUS_PREFERENCES_EAZEL_TOOLBAR_ICONS		"/nautilus/preferences/eazel_toolbar_icons"
#define NAUTILUS_PREFERENCES_ICON_THEME				"/nautilus/preferences/icon_theme"

enum
{
	NAUTILUS_CLICK_POLICY_SINGLE,
	NAUTILUS_CLICK_POLICY_DOUBLE
};

#define NAUTILUS_PREFERENCES_SHOW_TEXT_IN_REMOTE_ICONS "/nautilus/preferences/remote_icon_text"

void                      nautilus_global_preferences_startup                            (void);
void                      nautilus_global_preferences_shutdown                           (void);
void                      nautilus_global_preferences_show_dialog                        (void);

/* Sidebar */
GList *                   nautilus_global_preferences_get_enabled_sidebar_panel_view_identifiers (void);
GList *                   nautilus_global_preferences_get_disabled_sidebar_panel_view_identifiers (void);

BEGIN_GNOME_DECLS

#endif /* NAUTILUS_PREFS_GLOBAL_H */


