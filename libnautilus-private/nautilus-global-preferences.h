/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-global-preferences.h - Nautilus specific preference keys and
                                   functions.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

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

#include <eel/eel-preferences.h>

BEGIN_GNOME_DECLS

/* Which theme is active */
#define NAUTILUS_PREFERENCES_THEME				"preferences/theme"

/* How wide the sidebar is (or how wide it will be when expanded) */
#define NAUTILUS_PREFERENCES_SIDEBAR_WIDTH  			"preferences/sidebar_width"

/* Text fields */
#define NAUTILUS_PREFERENCES_USE_EMACS_SHORTCUTS                "preferences/use_emacs_shortcuts"

/* Window options */
#define NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW			"preferences/window_always_new"

/* Trash options */
#define NAUTILUS_PREFERENCES_CONFIRM_TRASH			"preferences/confirm_trash"
#define NAUTILUS_PREFERENCES_ENABLE_DELETE			"preferences/enable_delete"

/* Desktop options */
#define NAUTILUS_PREFERENCES_SHOW_DESKTOP			"preferences/show_desktop"
#define NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR                "preferences/desktop_is_home_dir"

/* Display  */
#define NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES  		"preferences/show_hidden_files"
#define NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES  		"preferences/show_backup_files"
#define NAUTILUS_PREFERENCES_SHOW_SPECIAL_FLAGS			"preferences/show_special_flags"

/* Sidebar panels  */
#define NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES         "sidebar-panels/tree/show_only_directories"

/* Navigation  */
#define NAUTILUS_PREFERENCES_HOME_URI                 		"preferences/home_uri"
#define NAUTILUS_PREFERENCES_HIDE_BUILT_IN_BOOKMARKS		"preferences/hide_built_in_bookmarks"

/* Proxy */
#define NAUTILUS_PREFERENCES_HTTP_USE_PROXY                	"/system/gnome-vfs/use-http-proxy"
#define NAUTILUS_PREFERENCES_HTTP_PROXY_HOST                 	"/system/gnome-vfs/http-proxy-host"
#define NAUTILUS_PREFERENCES_HTTP_PROXY_PORT                	"/system/gnome-vfs/http-proxy-port"

/* HTTP Proxy Authentication */
#define NAUTILUS_PREFERENCES_HTTP_PROXY_USE_AUTH                "/system/gnome-vfs/use-http-proxy-authorization"
#define NAUTILUS_PREFERENCES_HTTP_PROXY_AUTH_USERNAME      	"/system/gnome-vfs/http-proxy-authorization-user"
#define NAUTILUS_PREFERENCES_HTTP_USE_AUTH_PASSWORD     	"/system/gnome-vfs/http-proxy-authorization-password"

/* adding/removing from property browser */
#define NAUTILUS_PREFERENCES_CAN_ADD_CONTENT			"preferences/can_add_content"

/* Content fonts */
#define NAUTILUS_PREFERENCES_DEFAULT_FONT			"preferences/default_font"
#define NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT		"preferences/default_smooth_font"
#define NAUTILUS_PREFERENCES_DEFAULT_FONT_SIZE			"preferences/default_font_size"

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

/* Sorting order */
#define NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST		"preferences/sort_directories_first"

/* The default folder viewer - one of the two enums below */
#define NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER		"preferences/default_folder_viewer"

enum
{
	NAUTILUS_DEFAULT_FOLDER_VIEWER_ICON_VIEW,
	NAUTILUS_DEFAULT_FOLDER_VIEWER_LIST_VIEW,
	NAUTILUS_DEFAULT_FOLDER_VIEWER_OTHER
};

/* These IIDs are used by the preferences code and in nautilus-application.c */
#define NAUTILUS_ICON_VIEW_IID	     "OAFIID:nautilus_file_manager_icon_view:42681b21-d5ca-4837-87d2-394d88ecc058"
#define NAUTILUS_LIST_VIEW_IID	     "OAFIID:nautilus_file_manager_list_view:521e489d-0662-4ad7-ac3a-832deabe111c"


/* Icon View */
#define NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER	"icon-view/default_sort_in_reverse_order"
#define NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER		"icon-view/default_sort_order"
#define NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT	"icon-view/default_use_tighter_layout"
#define NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL		"icon-view/default_zoom_level"
#define NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE	"icon-view/default_zoom_level_font_size"
#define NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_MANUAL_LAYOUT	"icon-view/default_use_manual_layout"
#define NAUTILUS_PREFERENCES_ICON_VIEW_FONT				"icon-view/font"
#define NAUTILUS_PREFERENCES_ICON_VIEW_SMOOTH_FONT			"icon-view/smooth_font"

/* The icon view uses 2 variables to store the sort order and
 * whether to use manual layout.  However, the UI for these
 * preferences presensts them as single option menu.  So we
 * use the following preference as a proxy for the other two.
 * In nautilus-global-preferences.c we install callbacks for
 * the proxy preference and update the other 2 when it changes 
 */
#define NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER_OR_MANUAL_LAYOUT "icon-view/default_sort_order_or_manual_layout"

/* Which text attributes appear beneath icon names */
#define NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS				"icon-view/captions"

/* List View */
#define NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER	"list-view/default_sort_in_reverse_order"
#define NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER		"list-view/default_sort_order"
#define NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL		"list-view/default_zoom_level"
#define NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE	"list-view/default_zoom_level_font_size"
#define NAUTILUS_PREFERENCES_LIST_VIEW_FONT				"list-view/font"

/* News panel */
#define NAUTILUS_PREFERENCES_NEWS_MAX_ITEMS				"news/max_items"
#define NAUTILUS_PREFERENCES_NEWS_UPDATE_INTERVAL			"news/update_interval"

/* File Indexing */
#define NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE				"preferences/search_bar_type"

/* searching */
#define NAUTILUS_PREFERENCES_SEARCH_WEB_URI				"preferences/search_web_uri"			
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
#define NAUTILUS_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT	"preferences/thumbnail_limit"
#define NAUTILUS_PREFERENCES_USE_PUBLIC_METADATA	"preferences/use_public_metadata"
#define NAUTILUS_PREFERENCES_PREVIEW_SOUND		"preferences/preview_sound"

typedef enum
{
	NAUTILUS_COMPLEX_SEARCH_BAR,
	NAUTILUS_SIMPLE_SEARCH_BAR
} NautilusSearchBarMode;

/* Gnome session management */
#define NAUTILUS_PREFERENCES_ADD_TO_SESSION		"preferences/add_to_session"


void                    nautilus_global_preferences_initialize                   (void);

/* Sidebar */
struct EelScalableFont *nautilus_global_preferences_get_icon_view_smooth_font    (void);
struct EelScalableFont *nautilus_global_preferences_get_default_smooth_font      (void);
struct EelScalableFont *nautilus_global_preferences_get_default_smooth_bold_font (void);

void                    nautilus_global_preferences_set_default_folder_viewer    (const char *iid);


END_GNOME_DECLS

#endif /* NAUTILUS_PREFS_GLOBAL_H */
