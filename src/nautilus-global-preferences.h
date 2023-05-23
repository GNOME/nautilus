
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
   see <http://www.gnu.org/licenses/>.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#pragma once

#include "nautilus-global-preferences.h"
#include <gio/gio.h>

G_BEGIN_DECLS

/* Display  */
#define NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES			"show-hidden"

/* Mouse */
#define NAUTILUS_PREFERENCES_MOUSE_USE_EXTRA_BUTTONS		"mouse-use-extra-buttons"
#define NAUTILUS_PREFERENCES_MOUSE_FORWARD_BUTTON		"mouse-forward-button"
#define NAUTILUS_PREFERENCES_MOUSE_BACK_BUTTON			"mouse-back-button"

/* Single/Double click preference  */
#define NAUTILUS_PREFERENCES_CLICK_POLICY			"click-policy"

/* Drag and drop preferences */
#define NAUTILUS_PREFERENCES_OPEN_FOLDER_ON_DND_HOVER   	"open-folder-on-dnd-hover"

/* Installing new packages when unknown mime type activated */
#define NAUTILUS_PREFERENCES_INSTALL_MIME_ACTIVATION		"install-mime-activation"

/* Spatial or browser mode */
#define NAUTILUS_PREFERENCES_NEW_TAB_POSITION			"tabs-open-position"

#define NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY		"always-use-location-entry"

/* Which views should be displayed for new windows */
#define NAUTILUS_WINDOW_STATE_INITIAL_SIZE			"initial-size"
#define NAUTILUS_WINDOW_STATE_MAXIMIZED				"maximized"

/* Sorting order */
#define NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST		"sort-directories-first"
#define NAUTILUS_PREFERENCES_DEFAULT_SORT_ORDER			"default-sort-order"
#define NAUTILUS_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER	"default-sort-in-reverse-order"

/* The default folder viewer - one of the two enums below */
#define NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER		"default-folder-viewer"

/* Compression */
#define NAUTILUS_PREFERENCES_DEFAULT_COMPRESSION_FORMAT         "default-compression-format"

typedef enum
{
        NAUTILUS_COMPRESSION_ZIP = 0,
        NAUTILUS_COMPRESSION_TAR_XZ,
        NAUTILUS_COMPRESSION_7ZIP,
        NAUTILUS_COMPRESSION_ENCRYPTED_ZIP
} NautilusCompressionFormat;

/* Icon View */
#define NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL		"default-zoom-level"

/* Which text attributes appear beneath icon names */
#define NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS				"captions"

/* List View */
#define NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL		"default-zoom-level"
#define NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_VISIBLE_COLUMNS		"default-visible-columns"
#define NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_COLUMN_ORDER		"default-column-order"
#define NAUTILUS_PREFERENCES_LIST_VIEW_USE_TREE                         "use-tree-view"

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

#define NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS "show-directory-item-counts"
#define NAUTILUS_PREFERENCES_SHOW_FILE_THUMBNAILS	"show-image-thumbnails"
#define NAUTILUS_PREFERENCES_FILE_THUMBNAIL_LIMIT	"thumbnail-limit"

typedef enum
{
	NAUTILUS_COMPLEX_SEARCH_BAR,
	NAUTILUS_SIMPLE_SEARCH_BAR
} NautilusSearchBarMode;

/* Lockdown */
#define NAUTILUS_PREFERENCES_LOCKDOWN_COMMAND_LINE         "disable-command-line"

/* Recent files */
#define NAUTILUS_PREFERENCES_RECENT_FILES_ENABLED          "remember-recent-files"

/* Search behaviour */
#define NAUTILUS_PREFERENCES_RECURSIVE_SEARCH "recursive-search"

/* Context menu options */
#define NAUTILUS_PREFERENCES_SHOW_DELETE_PERMANENTLY "show-delete-permanently"
#define NAUTILUS_PREFERENCES_SHOW_CREATE_LINK "show-create-link"

/* Full Text Search enabled */
#define NAUTILUS_PREFERENCES_FTS_ENABLED "fts-enabled"

/* Gtk settings migration happened */
#define NAUTILUS_PREFERENCES_MIGRATED_GTK_SETTINGS "migrated-gtk-settings"

void nautilus_global_preferences_init                      (void);

extern GSettings *nautilus_preferences;
extern GSettings *nautilus_compression_preferences;
extern GSettings *nautilus_icon_view_preferences;
extern GSettings *nautilus_list_view_preferences;
extern GSettings *nautilus_window_state;
extern GSettings *gtk_filechooser_preferences;
extern GSettings *gnome_lockdown_preferences;
extern GSettings *gnome_interface_preferences;
extern GSettings *gnome_privacy_preferences;

G_END_DECLS
