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

#ifndef NAUTILUS_GLOBAL_PREFERENCES_H
#define NAUTILUS_GLOBAL_PREFERENCES_H

#include <eel/eel-preferences.h>

G_BEGIN_DECLS

/* Which theme is active */
#define NAUTILUS_PREFERENCES_THEME				"/desktop/gnome/file_views/icon_theme"

/* Desktop Background options */
#define NAUTILUS_PREFERENCES_BACKGROUND_SET                     "preferences/background_set" 
#define NAUTILUS_PREFERENCES_BACKGROUND_COLOR                   "preferences/background_color" 
#define NAUTILUS_PREFERENCES_BACKGROUND_FILENAME                "preferences/background_filename" 

/* Side Pane Background options */
#define NAUTILUS_PREFERENCES_SIDE_PANE_BACKGROUND_SET                     "preferences/side_pane_background_set" 
#define NAUTILUS_PREFERENCES_SIDE_PANE_BACKGROUND_COLOR                   "preferences/side_pane_background_color" 
#define NAUTILUS_PREFERENCES_SIDE_PANE_BACKGROUND_FILENAME                "preferences/side_pane_background_filename" 

/* How wide the sidebar is (or how wide it will be when expanded) */
#define NAUTILUS_PREFERENCES_SIDEBAR_WIDTH  			"preferences/sidebar_width"

/* Automount options */
#define NAUTILUS_PREFERENCES_MEDIA_AUTOMOUNT	                "preferences/media_automount"
#define NAUTILUS_PREFERENCES_MEDIA_AUTOMOUNT_OPEN		"preferences/media_automount_open"

/* Autorun options */
#define NAUTILUS_PREFERENCES_MEDIA_AUTORUN_NEVER                "preferences/media_autorun_never"
#define NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_ASK        "preferences/media_autorun_x_content_ask"
#define NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_IGNORE     "preferences/media_autorun_x_content_ignore"
#define NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER "preferences/media_autorun_x_content_open_folder"

/* Trash options */
#define NAUTILUS_PREFERENCES_CONFIRM_TRASH			"preferences/confirm_trash"
#define NAUTILUS_PREFERENCES_ENABLE_DELETE			"preferences/enable_delete"

/* Desktop options */
#define NAUTILUS_PREFERENCES_SHOW_DESKTOP			"preferences/show_desktop"
#define NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR                "preferences/desktop_is_home_dir"
#define NAUTILUS_PREFERENCES_DESKTOP_FONT			"preferences/desktop_font"

/* Display  */
#define NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES  		"/desktop/gnome/file_views/show_hidden_files"
#define NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES  		"/desktop/gnome/file_views/show_backup_files"
#define NAUTILUS_PREFERENCES_SHOW_ADVANCED_PERMISSIONS		"preferences/show_advanced_permissions"
#define NAUTILUS_PREFERENCES_DATE_FORMAT			"preferences/date_format"

typedef enum
{
	NAUTILUS_DATE_FORMAT_LOCALE,
	NAUTILUS_DATE_FORMAT_ISO,
	NAUTILUS_DATE_FORMAT_INFORMAL
} NautilusDateFormat;

/* Sidebar panels  */
#define NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES         "sidebar_panels/tree/show_only_directories"

/* Single/Double click preference  */
#define NAUTILUS_PREFERENCES_CLICK_POLICY			"preferences/click_policy"

/* Activating executable text files */
#define NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION		"preferences/executable_text_activation"

/* Spatial or browser mode */
#define NAUTILUS_PREFERENCES_ALWAYS_USE_BROWSER       		"preferences/always_use_browser"

/* Which views should be displayed for new windows */
#define NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR		"preferences/start_with_location_bar"
#define NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY		"preferences/always_use_location_entry"
#define NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR		"preferences/start_with_status_bar"
#define NAUTILUS_PREFERENCES_START_WITH_SIDEBAR		 	"preferences/start_with_sidebar"
#define NAUTILUS_PREFERENCES_START_WITH_TOOLBAR			"preferences/start_with_toolbar"
#define NAUTILUS_PREFERENCES_SIDE_PANE_VIEW                     "preferences/side_pane_view"
#define NAUTILUS_PREFERENCES_NAVIGATION_WINDOW_SAVED_GEOMETRY 	"preferences/navigation_window_saved_geometry"
#define NAUTILUS_PREFERENCES_NAVIGATION_WINDOW_MAXIMIZED        "preferences/navigation_window_saved_maximized"

/* Sorting order */
#define NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST		"preferences/sort_directories_first"

/* The default folder viewer - one of the two enums below */
#define NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER		"preferences/default_folder_viewer"

enum
{
	NAUTILUS_DEFAULT_FOLDER_VIEWER_ICON_VIEW,
	NAUTILUS_DEFAULT_FOLDER_VIEWER_COMPACT_VIEW,
	NAUTILUS_DEFAULT_FOLDER_VIEWER_LIST_VIEW,
	NAUTILUS_DEFAULT_FOLDER_VIEWER_OTHER
};

/* These IIDs are used by the preferences code and in nautilus-application.c */
#define NAUTILUS_ICON_VIEW_IID		"OAFIID:Nautilus_File_Manager_Icon_View"
#define NAUTILUS_COMPACT_VIEW_IID	"OAFIID:Nautilus_File_Manager_Compact_View"
#define NAUTILUS_LIST_VIEW_IID		"OAFIID:Nautilus_File_Manager_List_View"


/* Icon View */
#define NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER	"icon_view/default_sort_in_reverse_order"
#define NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER		"icon_view/default_sort_order"
#define NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT	"icon_view/default_use_tighter_layout"
#define NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL		"icon_view/default_zoom_level"
#define NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_MANUAL_LAYOUT	"icon_view/default_use_manual_layout"

#define NAUTILUS_PREFERENCES_ICON_VIEW_LABELS_BESIDE_ICONS      	"icon_view/labels_beside_icons"


/* The icon view uses 2 variables to store the sort order and
 * whether to use manual layout.  However, the UI for these
 * preferences presensts them as single option menu.  So we
 * use the following preference as a proxy for the other two.
 * In nautilus-global-preferences.c we install callbacks for
 * the proxy preference and update the other 2 when it changes 
 */
#define NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER_OR_MANUAL_LAYOUT "icon_view/default_sort_order_or_manual_layout"

/* Which text attributes appear beneath icon names */
#define NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS				"icon_view/captions"

/* The default size for thumbnail icons */
#define NAUTILUS_PREFERENCES_ICON_VIEW_THUMBNAIL_SIZE			"icon_view/thumbnail_size"

/* Compact View */
#define NAUTILUS_PREFERENCES_COMPACT_VIEW_DEFAULT_ZOOM_LEVEL		"compact_view/default_zoom_level"
#define NAUTILUS_PREFERENCES_COMPACT_VIEW_ALL_COLUMNS_SAME_WIDTH	"compact_view/all_columns_same_width"

/* List View */
#define NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER	"list_view/default_sort_in_reverse_order"
#define NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER		"list_view/default_sort_order"
#define NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL		"list_view/default_zoom_level"
#define NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS	        "list_view/default_visible_columns"
#define NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER	        "list_view/default_column_order"

/* News panel */
#define NAUTILUS_PREFERENCES_NEWS_MAX_ITEMS				"news/max_items"
#define NAUTILUS_PREFERENCES_NEWS_UPDATE_INTERVAL			"news/update_interval"

/* File Indexing */
#define NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE				"preferences/search_bar_type"

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
#define NAUTILUS_PREFERENCES_PREVIEW_SOUND		"preferences/preview_sound"

typedef enum
{
	NAUTILUS_COMPLEX_SEARCH_BAR,
	NAUTILUS_SIMPLE_SEARCH_BAR
} NautilusSearchBarMode;

#define NAUTILUS_PREFERENCES_DESKTOP_HOME_VISIBLE          "desktop/home_icon_visible"
#define NAUTILUS_PREFERENCES_DESKTOP_HOME_NAME             "desktop/home_icon_name"
#define NAUTILUS_PREFERENCES_DESKTOP_COMPUTER_VISIBLE      "desktop/computer_icon_visible"
#define NAUTILUS_PREFERENCES_DESKTOP_COMPUTER_NAME         "desktop/computer_icon_name"
#define NAUTILUS_PREFERENCES_DESKTOP_TRASH_VISIBLE         "desktop/trash_icon_visible"
#define NAUTILUS_PREFERENCES_DESKTOP_TRASH_NAME            "desktop/trash_icon_name"
#define NAUTILUS_PREFERENCES_DESKTOP_VOLUMES_VISIBLE	   "desktop/volumes_visible"
#define NAUTILUS_PREFERENCES_DESKTOP_NETWORK_VISIBLE       "desktop/network_icon_visible"
#define NAUTILUS_PREFERENCES_DESKTOP_NETWORK_NAME          "desktop/network_icon_name"

void nautilus_global_preferences_init                      (void);
char *nautilus_global_preferences_get_default_folder_viewer_preference_as_iid (void);
G_END_DECLS

#endif /* NAUTILUS_GLOBAL_PREFERENCES_H */
