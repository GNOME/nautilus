/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-global-preferences.c - Nautilus specific preference keys and
                                   functions.

   Copyright (C) 1999, 2000, 2001 Eazel, Inc.

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

#include <config.h>
#include "nautilus-global-preferences.h"

#include "nautilus-file-utilities.h"
#include "nautilus-file.h"
#include "nautilus-icon-factory.h"
#include "nautilus-sidebar-functions.h"
#include <eel/eel-enumeration.h>
#include <eel/eel-font-manager.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-scalable-font.h>
#include <eel/eel-smooth-widget.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-utils.h>

/* Constants */
#define STRING_LIST_DEFAULT_TOKENS_DELIMETER ","
#define PREFERENCES_SORT_ORDER_MANUALLY 100

/* Path for gnome-vfs preferences */
static const char SYSTEM_GNOME_VFS_PATH[] = "/system/gnome-vfs";

/* Forward declarations */
static void     global_preferences_install_defaults      (void);
static void     global_preferences_register_enumerations (void);
static gpointer default_font_callback                    (int  user_level);
static gpointer default_smooth_font_callback             (int  user_level);
static gpointer default_home_location_callback           (int  user_level);
static gpointer default_default_folder_viewer_callback	 (int  user_level);

static const char *default_smooth_font_auto_value;
static const char *icon_view_smooth_font_auto_value;

/* An enumeration used for installing type specific preferences defaults. */
typedef enum
{
	PREFERENCE_BOOLEAN = 1,
	PREFERENCE_INTEGER,
	PREFERENCE_STRING,
	PREFERENCE_STRING_LIST
} PreferenceType;

/* Enumerations used to qualify some INTEGER preferences */
static EelEnumerationEntry speed_tradeoff_enum_entries[] = {
	{ "always",	    N_("Always"),		NAUTILUS_SPEED_TRADEOFF_ALWAYS },
	{ "local_only",	    N_("Local Files Only"),	NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY },
	{ "never",	    N_("Never"),		NAUTILUS_SPEED_TRADEOFF_NEVER },
	{ NULL }
};

static EelEnumerationEntry default_zoom_level_enum_entries[] = {
	/* xgettext:no-c-format */
	{ "smallest",	    N_("25%"),		NAUTILUS_ZOOM_LEVEL_SMALLEST },
	/* xgettext:no-c-format */
	{ "smaller",	    N_("50%"),		NAUTILUS_ZOOM_LEVEL_SMALLER },
	/* xgettext:no-c-format */
	{ "small",	    N_("75%"),		NAUTILUS_ZOOM_LEVEL_SMALL },
	/* xgettext:no-c-format */
	{ "standard",	    N_("100%"),		NAUTILUS_ZOOM_LEVEL_STANDARD },
	/* xgettext:no-c-format */
	{ "large",	    N_("150%"),		NAUTILUS_ZOOM_LEVEL_LARGE },
	/* xgettext:no-c-format */
	{ "larger",	    N_("200%"),		NAUTILUS_ZOOM_LEVEL_LARGER },
	/* xgettext:no-c-format */
	{ "largest",	    N_("400%"),		NAUTILUS_ZOOM_LEVEL_LARGEST },
	{ NULL }
};

static EelEnumerationEntry file_size_enum_entries[] = {
	{ "102400",	    N_("100 K"),	102400 },
	{ "512000",	    N_("500 K"),	512000 },
	{ "1048576",	    N_("1 MB"),		1048576 },
	{ "3145728",	    N_("3 MB"),		3145728 },
	{ "5242880",	    N_("5 MB"),		5242880 },
	{ "10485760",	    N_("10 MB"),	10485760 },
	{ "104857600",	    N_("100 MB"),	104857600 },
	{ NULL }
};

static EelEnumerationEntry click_policy_enum_entries[] = {
	{ "single",
	  N_("Activate items with a single click"),
	  NAUTILUS_CLICK_POLICY_SINGLE
	},
	{ "double",
	  N_("Activate items with a double click"),
	  NAUTILUS_CLICK_POLICY_DOUBLE
	},
	{ NULL }
};

static EelEnumerationEntry executable_text_activation_enum_entries[] = {
	{ "launch",
	  N_("Execute files when they are clicked"),
	  NAUTILUS_EXECUTABLE_TEXT_LAUNCH
	},
	{ "display",
	  N_("Display files when they are clicked"),
	  NAUTILUS_EXECUTABLE_TEXT_DISPLAY
	},
	{ "ask",
	  N_("Ask each time"),
	  NAUTILUS_EXECUTABLE_TEXT_ASK
	},
	{ NULL }
};

static EelEnumerationEntry search_bar_type_enum_entries[] = {
	{ N_("search by text"),
	  N_("Search for files by file name only"),
	  NAUTILUS_SIMPLE_SEARCH_BAR
	},
	{ N_("search by text and properties"),
	  N_("Search for files by file name and file properties"),
	  NAUTILUS_COMPLEX_SEARCH_BAR
	},
	{ NULL }
};

static EelEnumerationEntry default_folder_viewer_enum_entries[] = {
	{ "icon_view",	    N_("Icon View"),	NAUTILUS_DEFAULT_FOLDER_VIEWER_ICON_VIEW },
	{ "list_view",	    N_("List View"),	NAUTILUS_DEFAULT_FOLDER_VIEWER_LIST_VIEW },
	{ NULL }
};

static EelEnumerationEntry default_icon_view_sort_order_enum_entries[] = {
	{ "manually",	       N_("Manually"),		    PREFERENCES_SORT_ORDER_MANUALLY },
	{ "--------",             "--------" },
	{ "name",	       N_("By Name"),		    NAUTILUS_FILE_SORT_BY_DISPLAY_NAME },
	{ "size",	       N_("By Size"),		    NAUTILUS_FILE_SORT_BY_SIZE },
	{ "type",	       N_("By Type"),		    NAUTILUS_FILE_SORT_BY_TYPE },
	{ "modification_date", N_("By Modification Date"),  NAUTILUS_FILE_SORT_BY_MTIME }, 
	{ "emblems",	       N_("By Emblems"),	    NAUTILUS_FILE_SORT_BY_EMBLEMS },
	{ NULL }
};

static EelEnumerationEntry default_list_view_sort_order_enum_entries[] = {
	{ "name",	       N_("By Name"),		    NAUTILUS_FILE_SORT_BY_DISPLAY_NAME },
	{ "size",	       N_("By Size"),		    NAUTILUS_FILE_SORT_BY_SIZE },
	{ "type",	       N_("By Type"),		    NAUTILUS_FILE_SORT_BY_TYPE },
	{ "modification_date", N_("By Modification Date"),  NAUTILUS_FILE_SORT_BY_MTIME }, 
	{ "emblems",	       N_("By Emblems"),	    NAUTILUS_FILE_SORT_BY_EMBLEMS },
	{ NULL }
};

static EelEnumerationEntry standard_font_size_entries[] = {
	{ "8",		   N_("8"),	8 },
	{ "10",		   N_("10"),	10 },
	{ "12",		   N_("12"),	12 },
	{ "14",		   N_("14"),	14 },
	{ "16",		   N_("16"),	16 },
	{ "18",		   N_("18"),	18 },
	{ "20",		   N_("20"),	20 },
	{ "22",		   N_("22"),	22 },
	{ "24",		   N_("24"),	24 },
	{ NULL }
};

static EelEnumerationEntry icon_captions_enum_entries[] = {
	{ "size",	       N_("size"),		0 },
	{ "type",	       N_("type"),		1 },
	{ "date_modified",     N_("date modified"),	2 },
	{ "date_changed",      N_("date changed"),	3 }, 
	{ "date_accessed",     N_("date accessed"),	4 }, 
	{ "owner",	       N_("owner"),		5 }, 
	{ "group",	       N_("group"),		6 }, 
	{ "permissions",       N_("permissions"),	7 }, 
	{ "octal_permissions", N_("octal permissions"),	8 }, 
	{ "mime_type",	       N_("MIME type"),		9 }, 
	{ "none",	       N_("none"),		10 }, 
	{ NULL }
};

/* These enumerations are used in the preferences dialog to 
 * populate widgets and route preferences changes between the
 * storage (GConf) and the displayed values.
 */
static EelEnumerationInfo enumerations[] = {
	{ "click_policy",		   click_policy_enum_entries },
	{ "default_folder_viewer",	   default_folder_viewer_enum_entries },
	{ "default_icon_view_sort_order",  default_icon_view_sort_order_enum_entries },
	{ "default_list_view_sort_order",  default_list_view_sort_order_enum_entries },
	{ "default_zoom_level",		   default_zoom_level_enum_entries },
	{ "executable_text_activation",	   executable_text_activation_enum_entries },
	{ "file_size",			   file_size_enum_entries },
	{ "icon_captions",		   icon_captions_enum_entries },
	{ "search_bar_type",		   search_bar_type_enum_entries },
	{ "speed_tradeoff",		   speed_tradeoff_enum_entries },
	{ "standard_font_size",		   standard_font_size_entries },
	{ NULL }
};

/*
 * A callback which can be used to fetch dynamic default values.
 * For example, values that are dependent on the environment (such as user name) 
 * cannot be specified as constants.
 */
typedef gpointer (*PreferencesDefaultValueCallback) (int user_level);

/* A structure that pairs a default value with a specific user level. */
typedef struct
{
	int user_level;
	const gpointer value;
	PreferencesDefaultValueCallback callback;
	GFreeFunc callback_result_free_function;
} PreferenceUserLevelDefault;

#define USER_LEVEL_NONE -1

/* A structure that describes a single preference including defaults and visibility. */
typedef struct
{
	const char *name;
	PreferenceType type;
	int visible_user_level;
	const PreferenceUserLevelDefault default1;
	const PreferenceUserLevelDefault default2;
	const char *enumeration_id;
} PreferenceDefault;

/* The following table defines the default values and user level visibilities of
 * Nautilus preferences.  Each of these preferences does not necessarily need to
 * have a UI item in the preferences dialog.  To add an item to the preferences
 * dialog, see the NautilusPreferencesItemDescription tables later in this file.
 * 
 * Field definitions:
 *
 * 1. name
 *
 *    The name of the preference.  Usually defined in
 *    nautilus-global-preferences.h
 *
 * 2. type
 *    The preference type.  One of:
 *
 *	PREFERENCE_BOOLEAN
 *	PREFERENCE_INTEGER
 *	PREFERENCE_STRING
 *	PREFERENCE_STRING_LIST
 * 
 * 3. visible_user_level
 *    The visible user level is the first user level at which the
 *    preference is visible.  By default all preferences have a visibility of 0.
 * 
 *    A preference with a visible_user_level greater than 0, will be "visible"
 *    only at that level or higher.  Any getters that ask for that preference at
 *    lower user levels will always receive the default value.  Also, if the
 *    preference has an entry in the preferences dialog, it will not be shown
 *    unless the current user level is greater than or equal to the preference's
 *    visible user level.
 *
 * 4. default1
 *    A pair of a user_level and a value (PreferenceUserLevelDefault).  For the
 *    left hand side user_level, the preference will have the right hand side
 *    default value.
 * 
 *    This pair does not need to be given.  It can be { USER_LEVEL_NONE }, in 
 *    which case the preference defaults to 0 at all user levels.
 * 
 * 5. default2
 *    A pair of a user_level and a value (PreferenceUserLevelDefault).  For the
 *    left hand side user_level, the preference will have the right hand side
 *    default value.
 * 
 *    This pair does not need to be given.  It can be { USER_LEVEL_NONE }, in 
 *    which case the preference defaults to 0 at all user levels.
 *
 *    Notes:
 *
 *    Define defaults only for preferences that need something other than 0 (integer)
 *    FALSE (boolean) or "" (string) as their defaults.
 *
 *    Its possible to have different defaults for different user levels  Its not 
 *    required to have defaults for EACH user level.  If there is no default
 *    installed for a high user level, the next lowest user level with a valid
 *    default is used.
 *
 * 6. enumeration_id
 *    An an enumeration id is a unique string that identifies an enumeration.
 *    If given, an enumeration id can be used to qualify a INTEGER preference.
 *    The preferences dialog widgetry will use this enumeration id to find out
 *    what choices and descriptions of choices to present to the user.
 */
static const PreferenceDefault preference_defaults[] = {
	{ NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_CONFIRM_TRASH,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_ADVANCED,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_ENABLE_DELETE,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_ADVANCED,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY) },
	  { USER_LEVEL_NONE },
	  "speed_tradeoff"
	},
	/* Don't show remote directory item counts for Beginner users because computing them
	 * can be annoyingly slow, especially for FTP. If we make this fast enough for FTP in
	 * particular, we should change this default to ALWAYS.
	 */
	{ NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY) },
	  { EEL_USER_LEVEL_INTERMEDIATE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_ALWAYS) },
	  "speed_tradeoff"
	},
	{ NAUTILUS_PREFERENCES_CLICK_POLICY,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY) },
	  { USER_LEVEL_NONE },
	  "click_policy"
	},
	{ NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_ADVANCED,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_EXECUTABLE_TEXT_ASK) },
	  { USER_LEVEL_NONE },
	  "executable_text_activation"
	},
	{ NAUTILUS_PREFERENCES_THEME,
	  PREFERENCE_STRING,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, "default" },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY) },
	  { USER_LEVEL_NONE },
	  "speed_tradeoff"
	},
	{ NAUTILUS_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_ADVANCED,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (3145728) },
	  { USER_LEVEL_NONE },
	  "file_size"
	},
	{ NAUTILUS_PREFERENCES_USE_PUBLIC_METADATA,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_ADVANCED,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY) },
	  { USER_LEVEL_NONE },
	  "speed_tradeoff"
	},
	{ NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) }, 
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_PREVIEW_SOUND,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY) },
	  { USER_LEVEL_NONE },
	  "speed_tradeoff"
	},
	{ NAUTILUS_PREFERENCES_SHOW_SPECIAL_FLAGS,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_ADVANCED,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { EEL_USER_LEVEL_ADVANCED, GINT_TO_POINTER (TRUE) }
	},
	{ NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_SHOW_DESKTOP,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_ADVANCED,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_CAN_ADD_CONTENT,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { EEL_USER_LEVEL_INTERMEDIATE, GINT_TO_POINTER (TRUE) }
	},
	{ NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SIMPLE_SEARCH_BAR) },
	  { EEL_USER_LEVEL_INTERMEDIATE, GINT_TO_POINTER (NAUTILUS_COMPLEX_SEARCH_BAR) },
	  "search_bar_type"
	},
	{ NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
	  PREFERENCE_STRING_LIST,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, "size,date_modified,type", },
	  { USER_LEVEL_NONE },
	  "icon_captions"
	},
	{ NAUTILUS_PREFERENCES_HIDE_BUILT_IN_BOOKMARKS,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_USE_EMACS_SHORTCUTS,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_ADVANCED,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	/* FIXME bugzilla.gnome.org 41245: Saved in pixels instead of in %? */
	{ NAUTILUS_PREFERENCES_SIDEBAR_WIDTH,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (148) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_SEARCH_WEB_URI,
	  PREFERENCE_STRING,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, "http://www.google.com" },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_START_WITH_TOOLBAR,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_START_WITH_SIDEBAR,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},

	/* Proxy defaults */
	{ NAUTILUS_PREFERENCES_HTTP_USE_PROXY,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_HTTP_PROXY_PORT,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (8080) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_HTTP_PROXY_USE_AUTH,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},

	/* Home URI */
	{ NAUTILUS_PREFERENCES_HOME_URI,
	  PREFERENCE_STRING,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, NULL, default_home_location_callback, g_free },
	  { EEL_USER_LEVEL_INTERMEDIATE, NULL, default_home_location_callback, g_free },
	},

	/* Default fonts */
	{ NAUTILUS_PREFERENCES_DEFAULT_FONT,
	  PREFERENCE_STRING,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, NULL, default_font_callback, g_free },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT,
	  PREFERENCE_STRING,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, NULL, default_smooth_font_callback, g_free },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_DEFAULT_FONT_SIZE,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (12) },
	  { USER_LEVEL_NONE },
	  "standard_font_size"
	},
	
	/* View Preferences */
	{ NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, NULL, default_default_folder_viewer_callback, NULL },
	  { USER_LEVEL_NONE },
	  "default_folder_viewer"
	},

	/* Icon View Default Preferences */
	{ NAUTILUS_PREFERENCES_ICON_VIEW_SMOOTH_FONT,
	  PREFERENCE_STRING,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, NULL, default_smooth_font_callback, g_free },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_FONT,
	  PREFERENCE_STRING,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, NULL, default_font_callback, g_free },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (12) },
	  { USER_LEVEL_NONE },
	  "standard_font_size"
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_FILE_SORT_BY_DISPLAY_NAME) },
	  { USER_LEVEL_NONE },
	  "default_icon_view_sort_order"
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER_OR_MANUAL_LAYOUT,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_FILE_SORT_BY_DISPLAY_NAME) },
	  { USER_LEVEL_NONE },
	  "default_icon_view_sort_order"
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_MANUAL_LAYOUT,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_ZOOM_LEVEL_STANDARD) },
	  { USER_LEVEL_NONE },
	  "default_zoom_level"
	},

	/* List View Default Preferences */
	{ NAUTILUS_PREFERENCES_LIST_VIEW_FONT,
	  PREFERENCE_STRING,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, NULL, default_font_callback, g_free },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (12) },
	  { USER_LEVEL_NONE },
	  "standard_font_size"
	},
	{ NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_FILE_SORT_BY_DISPLAY_NAME) },
	  { USER_LEVEL_NONE },
	  "default_list_view_sort_order"
	},
	{ NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_ZOOM_LEVEL_SMALLER) },
	  { USER_LEVEL_NONE },
	  "default_zoom_level"
	},

	/* Sidebar panel default */
	{ nautilus_sidebar_news_enabled_preference_name,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ nautilus_sidebar_notes_enabled_preference_name,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ nautilus_sidebar_help_enabled_preference_name,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ nautilus_sidebar_history_enabled_preference_name,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ nautilus_sidebar_tree_enabled_preference_name,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_INTERMEDIATE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { EEL_USER_LEVEL_INTERMEDIATE, GINT_TO_POINTER (TRUE) }
	},

	/* news panel preferences */
	{ NAUTILUS_PREFERENCES_NEWS_MAX_ITEMS,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (6) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_NEWS_UPDATE_INTERVAL,
	  PREFERENCE_INTEGER,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (5) },
	  { USER_LEVEL_NONE }
	},

	/* non-visible preferences */
	{ NAUTILUS_PREFERENCES_ADD_TO_SESSION,
	  PREFERENCE_BOOLEAN,
	  EEL_USER_LEVEL_NOVICE,
	  { EEL_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},

	{ NULL }
};

/**
 * global_preferences_register_enumerations
 *
 * Register enumerations for INTEGER preferences that need them.
 *
 * This function needs to be called before the preferences dialog
 * panes are populated, as they use the registered information to
 * create enumeration item widgets.
 */
static void
global_preferences_register_enumerations (void)
{
	guint i;

	/* Register the enumerations */
	eel_enumeration_register (enumerations);

	/* Set the enumeration ids for preferences that need them */
	for (i = 0; preference_defaults[i].name != NULL; i++) {
		if (eel_strlen (preference_defaults[i].enumeration_id) > 0) {
			g_assert (preference_defaults[i].type == PREFERENCE_INTEGER
				  || preference_defaults[i].type == PREFERENCE_STRING_LIST);
			eel_preferences_set_enumeration_id (preference_defaults[i].name,
							    preference_defaults[i].enumeration_id);
		}
	}
}

static void
global_preferences_install_one_default (const char *preference_name,
					PreferenceType preference_type,
					const PreferenceUserLevelDefault *user_level_default)
{
	gpointer value = NULL;
	EelStringList *string_list_value;

	g_return_if_fail (preference_name != NULL);
	g_return_if_fail (preference_type >= PREFERENCE_BOOLEAN);
	g_return_if_fail (preference_type <= PREFERENCE_STRING_LIST);
	g_return_if_fail (user_level_default != NULL);

	if (user_level_default->user_level == USER_LEVEL_NONE) {
		return;
	}

	/* If a callback is given, use that to fetch the default value */
	if (user_level_default->callback != NULL) {
		value = (* user_level_default->callback) (user_level_default->user_level);
	} else {
		value = user_level_default->value;
	}

	switch (preference_type) {
	case PREFERENCE_BOOLEAN:
		eel_preferences_default_set_boolean (preference_name,
						     user_level_default->user_level,
						     GPOINTER_TO_INT (value));
		break;
		
	case PREFERENCE_INTEGER:
		eel_preferences_default_set_integer (preference_name,
						     user_level_default->user_level,
						     GPOINTER_TO_INT (value));
		break;
		
	case PREFERENCE_STRING:
		eel_preferences_default_set_string (preference_name,
						    user_level_default->user_level,
						    value);
		break;
		
	case PREFERENCE_STRING_LIST:
		string_list_value = eel_string_list_new_from_tokens (value,
								     STRING_LIST_DEFAULT_TOKENS_DELIMETER,
								     TRUE);
		eel_preferences_default_set_string_list (preference_name,
							 user_level_default->user_level,
							 string_list_value);
		eel_string_list_free (string_list_value);
		break;
		
	default:
		g_assert_not_reached ();
	}

	/* Free the dynamic default value if needed */
	if (user_level_default->callback != NULL
	    && user_level_default->callback_result_free_function != NULL) {
		(* user_level_default->callback_result_free_function) (value);
	}
}

/**
 * global_preferences_install_defaults
 *
 * Install defaults and visibilities.
 *
 * Most of the defaults come from the preference_defaults table above.
 *
 * Many preferences require their defaults to be computed, and so there
 * are special functions to install those.
 */
static void
global_preferences_install_defaults (void)
{
	guint i;

	for (i = 0; preference_defaults[i].name != NULL; i++) {
		global_preferences_install_one_default (preference_defaults[i].name,
							preference_defaults[i].type,
							&preference_defaults[i].default1);
		
		global_preferences_install_one_default (preference_defaults[i].name,
							preference_defaults[i].type,
							&preference_defaults[i].default2);
		
		eel_preferences_set_visible_user_level (preference_defaults[i].name,
							preference_defaults[i].visible_user_level);
	}
}

static gpointer
default_font_callback (int user_level)
{
	g_return_val_if_fail (eel_preferences_user_level_is_valid (user_level), NULL);

	if (eel_dumb_down_for_multi_byte_locale_hack ()) {
		return g_strdup ("fixed");
	}

	return g_strdup ("helvetica");
}

static gpointer
default_smooth_font_callback (int user_level)
{
	g_return_val_if_fail (eel_preferences_user_level_is_valid (user_level), NULL);
	return eel_font_manager_get_default_font ();
}

static int
get_default_folder_viewer_preference_from_iid (const char *iid)
{
	g_return_val_if_fail (iid != NULL, NAUTILUS_DEFAULT_FOLDER_VIEWER_ICON_VIEW);

	if (strcmp (iid, NAUTILUS_LIST_VIEW_IID) == 0) {
		return NAUTILUS_DEFAULT_FOLDER_VIEWER_LIST_VIEW;
	} else if (strcmp (iid, NAUTILUS_ICON_VIEW_IID) == 0) {
		return NAUTILUS_DEFAULT_FOLDER_VIEWER_ICON_VIEW;
	}

	return NAUTILUS_DEFAULT_FOLDER_VIEWER_OTHER;
}

static gpointer
default_default_folder_viewer_callback (int user_level)
{
	OAF_ServerInfo *oaf_info;
	int result;

	oaf_info = gnome_vfs_mime_get_default_component ("x-directory/normal");
	if (oaf_info == NULL) {
		result = NAUTILUS_DEFAULT_FOLDER_VIEWER_ICON_VIEW;
	} else {
		result = get_default_folder_viewer_preference_from_iid (oaf_info->iid);
		if (result == NAUTILUS_DEFAULT_FOLDER_VIEWER_OTHER) {
			result = NAUTILUS_DEFAULT_FOLDER_VIEWER_ICON_VIEW;
		}
		CORBA_free (oaf_info);
	}

	return GINT_TO_POINTER (result);
}

static gpointer
default_home_location_callback (int user_level)
{
	g_return_val_if_fail (eel_preferences_user_level_is_valid (user_level), NULL);
	return gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
}

/*
 * Public functions
 */
static EelScalableFont *
global_preferences_get_smooth_font (const char *smooth_font_file_name)
{
	EelScalableFont *smooth_font;

	smooth_font = (smooth_font_file_name != NULL && g_file_exists (smooth_font_file_name)) ?
		eel_scalable_font_new (smooth_font_file_name) :
		eel_scalable_font_get_default_font ();
	
	g_assert (EEL_IS_SCALABLE_FONT (smooth_font));
	return smooth_font;
}

static EelScalableFont *
global_preferences_get_smooth_bold_font (const char *file_name)
{
	EelScalableFont *plain_font;
	EelScalableFont *bold_font;

	plain_font = global_preferences_get_smooth_font (file_name);
	g_assert (EEL_IS_SCALABLE_FONT (plain_font));

	bold_font = eel_scalable_font_make_bold (plain_font);

	if (bold_font == NULL) {
		bold_font = plain_font;
	} else {
		gtk_object_unref (GTK_OBJECT (plain_font));
	}

	g_assert (EEL_IS_SCALABLE_FONT (bold_font));
	return bold_font;
}

/**
 * nautilus_global_preferences_get_icon_view_smooth_font
 *
 * Return value: The user's smooth font for icon file names.  Need to 
 *               unref the returned GtkObject when done with it.
 */
EelScalableFont *
nautilus_global_preferences_get_icon_view_smooth_font (void)
{
	return global_preferences_get_smooth_font (icon_view_smooth_font_auto_value);
}

/**
 * nautilus_global_preferences_get_default_smooth_font
 *
 * Return value: The user's smooth font for default text.
 *               Need to unref the returned GtkObject when done with it.
 */
EelScalableFont *
nautilus_global_preferences_get_default_smooth_font (void)
{
	return global_preferences_get_smooth_font (default_smooth_font_auto_value);
}

/**
 * nautilus_global_preferences_get_default_smooth_bold_font
 *
 * Return value: A bold flavor on the user's default text font.  If
 *               no bold font is found, then the plain preffered font is
 *               used. Need to unref the returned GtkObject when done
 *               with it.
 */
EelScalableFont *
nautilus_global_preferences_get_default_smooth_bold_font (void)
{
	return global_preferences_get_smooth_bold_font (default_smooth_font_auto_value);
}

/* Let the smooth widget machinery know about smoothness changes */
static void
smooth_graphics_mode_changed_callback (gpointer callback_data)
{
	gboolean is_smooth;

	is_smooth = eel_preferences_get_boolean (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE);
	
	eel_smooth_widget_global_set_is_smooth (is_smooth);
}

static void
set_default_folder_viewer_in_gnome_vfs (const char *iid)
{
	gnome_vfs_mime_set_default_action_type ("x-directory/normal", GNOME_VFS_MIME_ACTION_TYPE_COMPONENT);
	gnome_vfs_mime_set_default_component ("x-directory/normal", iid);
}

/* Convert between gnome-vfs and gconf ways of storing this information. */
static void
default_folder_viewer_changed_callback (gpointer callback_data)
{
	int preference_value;
	const char *viewer_iid;

	g_assert (callback_data == NULL);

	preference_value = 
		eel_preferences_get_integer (NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER);

	if (preference_value == NAUTILUS_DEFAULT_FOLDER_VIEWER_LIST_VIEW) {
		viewer_iid = NAUTILUS_LIST_VIEW_IID;
	} else {
		g_return_if_fail (preference_value == NAUTILUS_DEFAULT_FOLDER_VIEWER_ICON_VIEW);
		viewer_iid = NAUTILUS_ICON_VIEW_IID;
	}

	set_default_folder_viewer_in_gnome_vfs (viewer_iid);
}

void
nautilus_global_preferences_set_default_folder_viewer (const char *iid)
{
	int viewer_preference;

	set_default_folder_viewer_in_gnome_vfs (iid);

	viewer_preference = get_default_folder_viewer_preference_from_iid (iid);

	/* If viewer is set to one that the Preferences dialog doesn't know about,
	 * just change the underlying gnome-vfs setting but leave the Preferences dialog alone.
	 */
	if (viewer_preference == NAUTILUS_DEFAULT_FOLDER_VIEWER_OTHER) {
		return;		
	}
	
	eel_preferences_set_integer (NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER,
				     viewer_preference);
}

/* The icon view uses 2 variables to store the sort order and
 * whether to use manual layout.  However, the UI for these
 * preferences presensts them as single option menu.  So we
 * use the following preference as a proxy for the other two.
 * In nautilus-global-preferences.c we install callbacks for
 * the proxy preference and update the other 2 when it changes 
 */
static void
default_icon_view_sort_order_or_manual_layout_changed_callback (gpointer callback_data)
{
 	int default_sort_order_or_manual_layout;
 	int default_sort_order;

 	default_sort_order_or_manual_layout = 
 		eel_preferences_get_integer (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER_OR_MANUAL_LAYOUT);

	eel_preferences_set_boolean (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_MANUAL_LAYOUT,
				     default_sort_order_or_manual_layout == PREFERENCES_SORT_ORDER_MANUALLY);

	if (default_sort_order_or_manual_layout != PREFERENCES_SORT_ORDER_MANUALLY) {
		default_sort_order = default_sort_order_or_manual_layout;

		g_return_if_fail (default_sort_order >= NAUTILUS_FILE_SORT_BY_DISPLAY_NAME);
		g_return_if_fail (default_sort_order <= NAUTILUS_FILE_SORT_BY_EMBLEMS);

		eel_preferences_set_integer (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER,
					     default_sort_order);
	}
}

void
nautilus_global_preferences_initialize (void)
{
	static gboolean initialized = FALSE;

	if (initialized) {
		return;
	}

	initialized = TRUE;

	eel_preferences_initialize ("/apps/nautilus");

	/* Install defaults */
	global_preferences_install_defaults ();

	global_preferences_register_enumerations ();

	/* Add the gnome-vfs path to the list of monitored directories - for proxy settings */
	eel_preferences_monitor_directory (SYSTEM_GNOME_VFS_PATH);

	/* Set up storage for values accessed in this file */
	eel_preferences_add_auto_string (NAUTILUS_PREFERENCES_ICON_VIEW_SMOOTH_FONT,
					 &icon_view_smooth_font_auto_value);
	eel_preferences_add_auto_string (NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT,
					 &default_smooth_font_auto_value);

	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE, 
				      smooth_graphics_mode_changed_callback, 
				      NULL);
	
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER, 
				      default_folder_viewer_changed_callback, 
				      NULL);

 	eel_preferences_add_callback (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER_OR_MANUAL_LAYOUT,
				      default_icon_view_sort_order_or_manual_layout_changed_callback, 
				      NULL);

	/* Keep track of smooth graphics mode changes in order to notify the smooth
	 * widget machinery.
	 */
	smooth_graphics_mode_changed_callback (NULL);
}
