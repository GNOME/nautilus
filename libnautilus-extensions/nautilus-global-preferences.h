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
#include <libnautilus-extensions/nautilus-preferences.h>

BEGIN_GNOME_DECLS

/* Window options */
#define NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW			"preferences/window_always_new"

/* Show hidden files  */
#define NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES  		"preferences/show_hidden_files"

/* sidebar width */
#define NAUTILUS_PREFERENCES_SIDEBAR_WIDTH  			"preferences/sidebar_width"

/* Home URI  */
#define NAUTILUS_PREFERENCES_HOME_URI                 		"preferences/home_uri"

/* adding/removing from property browser */
#define NAUTILUS_PREFERENCES_CAN_ADD_CONTENT			"preferences/can_add_content"

/* Preferences not (currently?) displayed in dialog */
#define NAUTILUS_PREFERENCES_ICON_VIEW_TEXT_ATTRIBUTE_NAMES	"icon_view/text_attribute_names"
#define NAUTILUS_PREFERENCES_SHOW_REAL_FILE_NAME		"preferences/show_real_file_name"

/* Single/Double click preference  */
#define NAUTILUS_PREFERENCES_CLICK_POLICY			"preferences/click_policy"

/* use anti-aliased canvas */
#define NAUTILUS_PREFERENCES_ANTI_ALIASED_CANVAS		"preferences/anti_aliased_canvas"

/* Sidebar panels */
#define NAUTILUS_PREFERENCES_SIDEBAR_PANELS_NAMESPACE		"sidebar-panels"

/* Directory view */
#define NAUTILUS_PREFERENCES_DIRECTORY_VIEW_FONT_FAMILY		"directory-view/font_family"

/* themes */
#define NAUTILUS_PREFERENCES_TOOLBAR_ICON_THEME			"preferences/toolbar_icon_theme"
#define NAUTILUS_PREFERENCES_ICON_THEME				"preferences/icon_theme"

/* File Indexing */
#define NAUTILUS_PREFERENCES_SEARCH_SPEED_SETTINGS              "preferences/do_slow_search"

enum
{
	NAUTILUS_CLICK_POLICY_SINGLE,
	NAUTILUS_CLICK_POLICY_DOUBLE
};

#define NAUTILUS_PREFERENCES_SHOW_TEXT_IN_REMOTE_ICONS		"preferences/remote_icon_text"

void   nautilus_global_preferences_shutdown                                    (void);
void   nautilus_global_preferences_show_dialog                                 (void);
void   nautilus_global_preferences_hide_dialog                                 (void);
void   nautilus_global_preferences_set_dialog_title                            (const char *title);
void   nautilus_global_preferences_dialog_update                               (void);

/* Sidebar */
GList *nautilus_global_preferences_get_enabled_sidebar_panel_view_identifiers  (void);
GList *nautilus_global_preferences_get_disabled_sidebar_panel_view_identifiers (void);


BEGIN_GNOME_DECLS

#endif /* NAUTILUS_PREFS_GLOBAL_H */


