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

/*
 * The following preferences exist independently of the user level. 
 *
 * Note that the configuration path is fully qualified 
 */

/* Which theme is active */
#define NAUTILUS_PREFERENCES_THEME				"/apps/nautilus/preferences/theme"
/* Which text attributes appear beneath icon names */
#define NAUTILUS_PREFERENCES_ICON_CAPTIONS			"/apps/nautilus/icon_view/captions"
/* Are built-in bookmarks showing or not? */
#define NAUTILUS_PREFERENCES_HIDE_BUILT_IN_BOOKMARKS		"/apps/nautilus/preferences/hide_built_in_bookmarks"
/* How wide the sidebar is (or how wide it will be when expanded) */
#define NAUTILUS_PREFERENCES_SIDEBAR_WIDTH  			"/apps/nautilus/preferences/sidebar_width"


/*
 * The following preferences are coupled to the user level.
 *
 * Note that the configuration path does include the nautilus gconf 
 * prefix.  The nautilus_preferences_* api will fill in the missing
 * prefix according to the current user level.
 */

/* Window options */
#define NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW			"preferences/window_always_new"

/* Display  */
#define NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES  		"preferences/show_hidden_files"
#define NAUTILUS_PREFERENCES_SHOW_SPECIAL_FLAGS			"preferences/show_special_flags"

/* Home URI  */
#define NAUTILUS_PREFERENCES_HOME_URI                 		"preferences/home_uri"

/* adding/removing from property browser */
#define NAUTILUS_PREFERENCES_CAN_ADD_CONTENT			"preferences/can_add_content"

/* Single/Double click preference  */
#define NAUTILUS_PREFERENCES_CLICK_POLICY			"preferences/click_policy"

/* Smooth graphics mode (smoother but slower) */
#define NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE		"preferences/smooth_graphics_mode"

/* Which views should be displayed for new windows */
#define NAUTILUS_PREFERENCES_START_WITH_TOOL_BAR		"preferences/start_with_tool_bar"
#define NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR		"preferences/start_with_location_bar"
#define NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR		"preferences/start_with_status_bar"
#define NAUTILUS_PREFERENCES_START_WITH_SIDEBAR		 	"preferences/start_with_sidebar"

/* The sidebar panel preferences are computed from their oafids, which aren't known at
 * compile time. We publish the namespace for all of them so interested parties can
 * monitor changes to all of them collectively, without having to know any oafids. 
 */
#define NAUTILUS_PREFERENCES_SIDEBAR_PANELS_NAMESPACE		"sidebar-panels"

/* Directory view */
#define NAUTILUS_PREFERENCES_DIRECTORY_VIEW_FONT_FAMILY		"directory-view/font_family"

/* File Indexing */
#define NAUTILUS_PREFERENCES_SEARCH_METHOD			"preferences/also_do_slow_search"
#define NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE			"preferences/search_bar_type"

/* searching */
#define NAUTILUS_PREFERENCES_SEARCH_WEB_URI			"preferences/search_web_uri"			

enum
{
	NAUTILUS_CLICK_POLICY_SINGLE,
	NAUTILUS_CLICK_POLICY_DOUBLE
};

typedef enum
{
	NAUTILUS_SPEED_TRADEOFF_ALWAYS,
	NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY,
	NAUTILUS_SPEED_TRADEOFF_NEVER
} NautilusSpeedTradeoffValue;

#define NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS		"preferences/show_icon_text"
#define NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS	"preferences/show_image_thumbnails"
#define NAUTILUS_PREFERENCES_USE_PUBLIC_METADATA	"preferences/use_public_metadata"

typedef enum
{
	NAUTILUS_COMPLEX_SEARCH_BAR,
	NAUTILUS_SIMPLE_SEARCH_BAR
} NautilusSearchBarMode;


void   nautilus_global_preferences_initialize                                  (void);
void   nautilus_global_preferences_shutdown                                    (void);
void   nautilus_global_preferences_show_dialog                                 (void);
void   nautilus_global_preferences_hide_dialog                                 (void);
void   nautilus_global_preferences_set_dialog_title                            (const char *title);
void   nautilus_global_preferences_dialog_update                               (void);

/* Sidebar */
GList *nautilus_global_preferences_get_enabled_sidebar_panel_view_identifiers  (void);
GList *nautilus_global_preferences_get_disabled_sidebar_panel_view_identifiers (void);


END_GNOME_DECLS

#endif /* NAUTILUS_PREFS_GLOBAL_H */


