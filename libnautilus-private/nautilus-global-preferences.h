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

#include <libnautilus-extensions/nautilus-preferences.h>

BEGIN_GNOME_DECLS

/* Which theme is active */
#define NAUTILUS_PREFERENCES_THEME				"preferences/theme"
/* Which text attributes appear beneath icon names */
#define NAUTILUS_PREFERENCES_ICON_CAPTIONS			"icon_view/captions"
/* How wide the sidebar is (or how wide it will be when expanded) */
#define NAUTILUS_PREFERENCES_SIDEBAR_WIDTH  			"preferences/sidebar_width"

/* Keep track of the sound playing process */
#define NAUTILUS_PREFERENCES_CURRENT_SOUND_STATE		"preferences/sound_state"
/* Does the system have audio output capability */
#define NAUTILUS_PREFERENCES_HAS_AUDIO_OUT			"preferences/audio_out"

/* Text fields */
#define NAUTILUS_PREFERENCES_USE_EMACS_SHORTCUTS                "preferences/use_emacs_shortcuts"

/* Window options */
#define NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW			"preferences/window_always_new"

/* Trash options */
#define NAUTILUS_PREFERENCES_CONFIRM_TRASH			"preferences/confirm_trash"

/* Desktop options */
#define NAUTILUS_PREFERENCES_SHOW_DESKTOP			"preferences/show_desktop"

/* Display  */
#define NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES  		"preferences/show_hidden_files"
#define NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES  		"preferences/show_backup_files"
#define NAUTILUS_PREFERENCES_SHOW_SPECIAL_FLAGS			"preferences/show_special_flags"
#define NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES         "sidebar-panels/tree/show_only_directories"

/* Navigation  */
#define NAUTILUS_PREFERENCES_HOME_URI                 		"preferences/home_uri"
#define NAUTILUS_PREFERENCES_HIDE_BUILT_IN_BOOKMARKS		"preferences/hide_built_in_bookmarks"

/* Proxy */
#define NAUTILUS_PREFERENCES_HTTP_PROXY                 	"preferences/http-proxy"
#define NAUTILUS_PREFERENCES_HTTP_PROXY_PORT                	"preferences/http-proxy-port"
#define NAUTILUS_PREFERENCES_HTTP_USE_PROXY                	"preferences/http-use-proxy"

/* adding/removing from property browser */
#define NAUTILUS_PREFERENCES_CAN_ADD_CONTENT			"preferences/can_add_content"

/* Single/Double click preference  */
#define NAUTILUS_PREFERENCES_CLICK_POLICY			"preferences/click_policy"

/* Activating executable text files */
#define NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION		"preferences/executable_text_activation"

/* Smooth graphics mode (smoother but slower) */
#define NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE		"preferences/smooth_graphics_mode"

/* Which views should be displayed for new windows */
#define NAUTILUS_PREFERENCES_START_WITH_TOOLBAR			"preferences/start_with_toolbar"
#define NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR		"preferences/start_with_location_bar"
#define NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR		"preferences/start_with_status_bar"
#define NAUTILUS_PREFERENCES_START_WITH_SIDEBAR		 	"preferences/start_with_sidebar"

/* enabling annotations */
#define NAUTILUS_PREFERENCES_LOOKUP_ANNOTATIONS			"preferences/lookup_annotations"
#define NAUTILUS_PREFERENCES_DISPLAY_ANNOTATIONS		"preferences/display_annotations"

/* The sidebar panel preferences are computed from their oafids, which aren't known at
 * compile time. We publish the namespace so that interested parties can monitor changes
 * to all of them collectively, without having to know the exact oaf iids.
 */
#define NAUTILUS_PREFERENCES_SIDEBAR_PANELS_NAMESPACE		"sidebar-panels"

/* Sorting order */
#define NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST		"preferences/sort_directories_first"

/* Directory view */
#define NAUTILUS_PREFERENCES_DIRECTORY_VIEW_FONT_FAMILY		"directory-view/font_family"
#define NAUTILUS_PREFERENCES_DIRECTORY_VIEW_SMOOTH_FONT		"directory-view/smooth_font"

/* File Indexing */
#define NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE			"preferences/search_bar_type"
#define NAUTILUS_PREFERENCES_MEDUSA_BLOCKED                     "preferences/medusa_blocked"
#define NAUTILUS_PREFERENCES_USE_FAST_SEARCH                    "preferences/use_fast_search"

/* searching */
#define NAUTILUS_PREFERENCES_SEARCH_WEB_URI			"preferences/search_web_uri"			

enum
{
	NAUTILUS_CLICK_POLICY_SINGLE,
	NAUTILUS_CLICK_POLICY_DOUBLE
};

enum
{
	NAUTILUS_EXECUTABLE_TEXT_LAUNCH,
	NAUTILUS_EXECUTABLE_TEXT_DISPLAY,
	NAUTILUS_EXECUTABLE_TEXT_ASK
};

typedef enum
{
	NAUTILUS_SPEED_TRADEOFF_ALWAYS,
	NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY,
	NAUTILUS_SPEED_TRADEOFF_NEVER
} NautilusSpeedTradeoffValue;

#define NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS		"preferences/show_icon_text"
#define NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS "preferences/show_directory_item_counts"
#define NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS	"preferences/show_image_thumbnails"
#define NAUTILUS_PREFERENCES_USE_PUBLIC_METADATA	"preferences/use_public_metadata"
#define NAUTILUS_PREFERENCES_PREVIEW_SOUND		"preferences/preview_sound"

typedef enum
{
	NAUTILUS_COMPLEX_SEARCH_BAR,
	NAUTILUS_SIMPLE_SEARCH_BAR
} NautilusSearchBarMode;

/* Gnome session management */
#define NAUTILUS_PREFERENCES_ADD_TO_SESSION		"preferences/add_to_session"

void                         nautilus_global_preferences_initialize                                 (void);
void                         nautilus_global_preferences_show_dialog                                (void);
void                         nautilus_global_preferences_hide_dialog                                (void);
void                         nautilus_global_preferences_set_dialog_title                           (const char *title);

/* Sidebar */
GList *                      nautilus_global_preferences_get_enabled_sidebar_panel_view_identifiers (void);
struct NautilusScalableFont *nautilus_global_preferences_get_smooth_font                            (void);
struct NautilusScalableFont *nautilus_global_preferences_get_smooth_bold_font                       (void);

END_GNOME_DECLS

#endif /* NAUTILUS_PREFS_GLOBAL_H */
