/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-prefs-dialog.c - Implementation for preferences dialog.

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
#include "nautilus-preferences-dialog.h"
#include "nautilus-preferences-group.h"
#include "nautilus-preferences-item.h"
#include "nautilus-sidebar-functions.h"
#include <eel/eel-font-manager.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-scalable-font.h>
#include <eel/eel-smooth-widget.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <gconf/gconf-client.h>
#include <gconf/gconf.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-utils.h>

/* Constants */
static const char untranslated_global_preferences_dialog_title[] = N_("Nautilus Preferences");
#define GLOBAL_PREFERENCES_DIALOG_TITLE _(untranslated_global_preferences_dialog_title)
#define STRING_LIST_DEFAULT_TOKENS_DELIMETER ","

/* Preference names for known sidebar panels.  These are used to install the default
 * enabled state for the panel.  Unknown panels will have a default enabled state of FALSE.
 */
#define NOTES_PANEL_IID		"OAFIID:nautilus_notes_view:7f04c3cb-df79-4b9a-a577-38b19ccd4185"
#define HELP_PANEL_IID		"OAFIID:hyperbola_navigation_tree:57542ce0-71ff-442d-a764-462c92514234"
#define HISTORY_PANEL_IID	"OAFIID:nautilus_history_view:a7a85bdd-2ecf-4bc1-be7c-ed328a29aacb"
#define TREE_PANEL_IID		"OAFIID:nautilus_tree_view:2d826a6e-1669-4a45-94b8-23d65d22802d"
#define NOTES_PANEL_KEY		NAUTILUS_PREFERENCES_SIDEBAR_PANEL_PREFIX "/" NOTES_PANEL_IID
#define HELP_PANEL_KEY		NAUTILUS_PREFERENCES_SIDEBAR_PANEL_PREFIX "/" HELP_PANEL_IID
#define HISTORY_PANEL_KEY	NAUTILUS_PREFERENCES_SIDEBAR_PANEL_PREFIX "/" HISTORY_PANEL_IID
#define TREE_PANEL_KEY		NAUTILUS_PREFERENCES_SIDEBAR_PANEL_PREFIX "/" TREE_PANEL_IID

/* base path for NAUTILUS_PREFERENCES_HTTP_* */
static const char SYSTEM_GNOME_VFS_PATH[] = "/system/gnome-vfs";

/* A structure that describes a single preferences dialog ui item. */
typedef struct PreferenceDialogItem PreferenceDialogItem;

/* Forward declarations */
static gboolean   global_preferences_close_dialog_callback      (GtkWidget                  *dialog,
								 gpointer                    user_data);
static void       global_preferences_install_defaults           (void);
static void       global_preferences_register_enumerations      (void);
static GtkWidget *global_preferences_create_dialog              (void);
static void       global_preferences_create_sidebar_panels_pane (NautilusPreferencesBox     *preference_box);
static GtkWidget *global_preferences_populate_pane              (NautilusPreferencesBox     *preference_box,
								 const char                 *pane_name,
								 const PreferenceDialogItem *preference_dialog_item);
static gpointer   default_font_callback                         (int                         user_level);
static gpointer   default_smooth_font_callback                  (int                         user_level);
static gpointer   default_home_location_callback                (int                         user_level);

static GtkWidget *global_prefs_dialog = NULL;
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
	{ "smallest",	    N_("25%"),		NAUTILUS_ZOOM_LEVEL_SMALLEST },
	{ "smaller",	    N_("50%"),		NAUTILUS_ZOOM_LEVEL_SMALLER },
	{ "small",	    N_("75%"),		NAUTILUS_ZOOM_LEVEL_SMALL },
	{ "standard",	    N_("100%"),		NAUTILUS_ZOOM_LEVEL_STANDARD },
	{ "large",	    N_("150%"),		NAUTILUS_ZOOM_LEVEL_LARGE },
	{ "larger",	    N_("200%"),		NAUTILUS_ZOOM_LEVEL_LARGER },
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

static EelEnumerationEntry default_sort_order_enum_entries[] = {
	{ "name",	       N_("By Name"),		    NAUTILUS_FILE_SORT_BY_NAME },
	{ "size",	       N_("By Size"),		    NAUTILUS_FILE_SORT_BY_SIZE },
	{ "type",	       N_("By Type"),		    NAUTILUS_FILE_SORT_BY_TYPE },
	{ "modification date", N_("By Modification Date"),  NAUTILUS_FILE_SORT_BY_MTIME }, 
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
	{ "click_policy",		click_policy_enum_entries },
	{ "default_folder_viewer",	default_folder_viewer_enum_entries },
	{ "default_sort_order",		default_sort_order_enum_entries },
	{ "default_zoom_level",		default_zoom_level_enum_entries },
	{ "executable_text_activation",	executable_text_activation_enum_entries },
	{ "file_size",			file_size_enum_entries },
	{ "search_bar_type",		search_bar_type_enum_entries },
	{ "speed_tradeoff",		speed_tradeoff_enum_entries },
	{ "standard_font_size",		standard_font_size_entries },
	{ "icon_captions",		icon_captions_enum_entries },
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
 * dialog, see the PreferenceDialogItem tables later in this file.
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
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_CONFIRM_TRASH,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_ADVANCED,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_ENABLE_DELETE,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_ADVANCED,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY) },
	  { USER_LEVEL_NONE },
	  "speed_tradeoff"
	},
	/* Don't show remote directory item counts for Beginner users because computing them
	 * can be annoyingly slow, especially for FTP. If we make this fast enough for FTP in
	 * particular, we should change this default to ALWAYS.
	 */
	{ NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY) },
	  { NAUTILUS_USER_LEVEL_INTERMEDIATE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_ALWAYS) },
	  "speed_tradeoff"
	},
	{ NAUTILUS_PREFERENCES_CLICK_POLICY,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY) },
	  { USER_LEVEL_NONE },
	  "click_policy"
	},
	{ NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_ADVANCED,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_EXECUTABLE_TEXT_ASK) },
	  { USER_LEVEL_NONE },
	  "executable_text_activation"
	},
	{ NAUTILUS_PREFERENCES_THEME,
	  PREFERENCE_STRING,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, "default" },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY) },
	  { USER_LEVEL_NONE },
	  "speed_tradeoff"
	},
	{ NAUTILUS_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_ADVANCED,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (3145728) },
	  { USER_LEVEL_NONE },
	  "file_size"
	},
	{ NAUTILUS_PREFERENCES_USE_PUBLIC_METADATA,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_ADVANCED,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY) },
	  { USER_LEVEL_NONE },
	  "speed_tradeoff"
	},
	{ NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) }, 
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_PREVIEW_SOUND,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY) },
	  { USER_LEVEL_NONE },
	  "speed_tradeoff"
	},
	{ NAUTILUS_PREFERENCES_SHOW_SPECIAL_FLAGS,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_ADVANCED,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { NAUTILUS_USER_LEVEL_ADVANCED, GINT_TO_POINTER (TRUE) }
	},
	{ NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_SHOW_DESKTOP,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_CAN_ADD_CONTENT,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { NAUTILUS_USER_LEVEL_INTERMEDIATE, GINT_TO_POINTER (TRUE) }
	},
	{ NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SIMPLE_SEARCH_BAR) },
	  { NAUTILUS_USER_LEVEL_INTERMEDIATE, GINT_TO_POINTER (NAUTILUS_COMPLEX_SEARCH_BAR) },
	  "search_bar_type"
	},
	{ NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
	  PREFERENCE_STRING_LIST,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, "size,date_modified,type", },
	  { USER_LEVEL_NONE },
	  "icon_captions"
	},
	{ NAUTILUS_PREFERENCES_HIDE_BUILT_IN_BOOKMARKS,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_USE_EMACS_SHORTCUTS,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_ADVANCED,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	/* FIXME bugzilla.eazel.com 1245: Saved in pixels instead of in %? */
	{ NAUTILUS_PREFERENCES_SIDEBAR_WIDTH,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (148) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_SEARCH_WEB_URI,
	  PREFERENCE_STRING,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, "http://www.eazel.com/websearch" },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_START_WITH_TOOLBAR,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_START_WITH_SIDEBAR,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},

	/* Proxy defaults */
	{ NAUTILUS_PREFERENCES_HTTP_USE_PROXY,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_HTTP_PROXY_PORT,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (8080) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_HTTP_PROXY_USE_AUTH,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},

	/* Home URI */
	{ NAUTILUS_PREFERENCES_HOME_URI,
	  PREFERENCE_STRING,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, NULL, default_home_location_callback, g_free },
	  { NAUTILUS_USER_LEVEL_INTERMEDIATE, NULL, default_home_location_callback, g_free },
	},

	/* Default fonts */
	{ NAUTILUS_PREFERENCES_DEFAULT_FONT,
	  PREFERENCE_STRING,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, NULL, default_font_callback, g_free },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT,
	  PREFERENCE_STRING,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, NULL, default_smooth_font_callback, g_free },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_DEFAULT_FONT_SIZE,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (12) },
	  { USER_LEVEL_NONE },
	  "standard_font_size"
	},
	
	/* View Preferences */
	{ NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_DEFAULT_FOLDER_VIEWER_ICON_VIEW) },
	  { USER_LEVEL_NONE },
	  "default_folder_viewer"
	},

	/* Icon View Default Preferences */
	{ NAUTILUS_PREFERENCES_ICON_VIEW_SMOOTH_FONT,
	  PREFERENCE_STRING,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, NULL, default_smooth_font_callback, g_free },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_FONT,
	  PREFERENCE_STRING,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, NULL, default_font_callback, g_free },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (12) },
	  { USER_LEVEL_NONE },
	  "standard_font_size"
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_FILE_SORT_BY_NAME) },
	  { USER_LEVEL_NONE },
	  "default_sort_order"
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_ZOOM_LEVEL_STANDARD) },
	  { USER_LEVEL_NONE },
	  "default_zoom_level"
	},

	/* List View Default Preferences */
	{ NAUTILUS_PREFERENCES_LIST_VIEW_FONT,
	  PREFERENCE_STRING,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, NULL, default_font_callback, g_free },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (12) },
	  { USER_LEVEL_NONE },
	  "standard_font_size"
	},
	{ NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_FILE_SORT_BY_NAME) },
	  { USER_LEVEL_NONE },
	  "default_sort_order"
	},
	{ NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_ZOOM_LEVEL_SMALLER) },
	  { USER_LEVEL_NONE },
	  "default_zoom_level"
	},

	/* Sidebar panel default */
	{ NOTES_PANEL_KEY,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ HELP_PANEL_KEY,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ HISTORY_PANEL_KEY,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (TRUE) },
	  { USER_LEVEL_NONE }
	},
	{ TREE_PANEL_KEY,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { NAUTILUS_USER_LEVEL_INTERMEDIATE, GINT_TO_POINTER (TRUE) }
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
			nautilus_preferences_set_enumeration_id (preference_defaults[i].name,
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
		nautilus_preferences_default_set_boolean (preference_name,
							  user_level_default->user_level,
							  GPOINTER_TO_INT (value));
		break;
		
	case PREFERENCE_INTEGER:
		nautilus_preferences_default_set_integer (preference_name,
							  user_level_default->user_level,
							  GPOINTER_TO_INT (value));
		break;
		
	case PREFERENCE_STRING:
		nautilus_preferences_default_set_string (preference_name,
							 user_level_default->user_level,
							 value);
		break;
		
	case PREFERENCE_STRING_LIST:
		string_list_value = eel_string_list_new_from_tokens (value,
								     STRING_LIST_DEFAULT_TOKENS_DELIMETER,
								     TRUE);
		nautilus_preferences_default_set_string_list (preference_name,
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
		
		nautilus_preferences_set_visible_user_level (preference_defaults[i].name,
							     preference_defaults[i].visible_user_level);
	}

	/* Add the gnome-vfs path to the list of monitored directories - for proxy settings */
	nautilus_preferences_monitor_directory (SYSTEM_GNOME_VFS_PATH);
}

/* A structure that describes a single preferences dialog ui item. */
struct PreferenceDialogItem
{
	const char *group_name;
	const char *preference_name;
	const char *preference_description;
	NautilusPreferencesItemType item_type;
	const char *control_preference_name;
	NautilusPreferencesItemControlAction control_action;
	int column;
	const char *enumeration_list_unique_exceptions;
};

/* The following tables define preference items for the preferences dialog.
 * Each item corresponds to one preference.
 * 
 * Field definitions:
 *
 * 1. group_name
 *
 *    The group under which the preference is placed.  Each unique group will
 *    be framed and titled with the group_name.
 *
 *    Many items can have the same group_name.  Groups will be created as needed
 *    while populating the items.
 *
 *    This field needs to be non NULL.
 *
 * 2. preference_name
 *
 *    The name of the preference
 *
 *    This field needs to be non NULL.
 *
 * 3. preference_description
 *
 *    A user visible description of the preference.  Not all items use the
 *    description.  In particular, enumeration items use the descriptions from
 *    an enumeration structure.  See field XX below.
 *
 *    This field needs to be non NULL for items other than:
 * 
 *      NAUTILUS_PREFERENCE_ITEM_ENUMERATION_VERTICAL_RADIO or 
 *      NAUTILUS_PREFERENCE_ITEM_ENUMERATION_HORIZONTAL_RADIO
 * 
 * 4. item_type
 *
 *    The type of the item.  Needs to be one of the valid values of
 *    NautilusPreferencesItemType.  See nautilus-preference-item.h.
 *
 *    This field needs to be one of the valid item types.
 * 
 * 5. control_preference_name
 *
 *    A second preference that "controls" this preference.  Can only
 *    be a boolean preference.
 *
 *    This field can be NULL, in which case field 6 is ignored.
 *
 * 6. control_action
 *
 *    The action to take when the control preference in field 5 changes.
 *    There are only 2 possible actions:
 *
 *      NAUTILUS_PREFERENCE_ITEM_SHOW - If the control preference is TRUE
 *                                      the show this item.
 *
 *      NAUTILUS_PREFERENCE_ITEM_HIDE - If the control preference is FALSE
 *                                      the hide this item.
 *
 * 7. column
 *
 *    A preference pane is composed of groups.  Each group is bounded by
 *    a frame.  Each of these groups can have 0 or 1 columns of preference
 *    item widgets.  This field controls which column the preference item 
 *    widgets appear in.
 *
 * 8. enumeration_list_unique_exceptions
 *    If the item type is one of:
 *
 *      NAUTILUS_PREFERENCE_ITEM_ENUMERATION_LIST_HORIZONTAL
 *      NAUTILUS_PREFERENCE_ITEM_ENUMERATION_LIST_VERTICAL
 *
 *    The this field can be a string of exceptions to the rule that enumeration
 *    list items must always not allow duplicate choices.  For example, if there
 *    are 3 string pickers in the item, then each one cannot select and item
 *    which is already selected in one of the other two.  The preferences item
 *    widget enforces this rule by making such items insensitive.  
 *
 *    The enumeration_list_unique_exceptions allows a way to bypass this rule
 *    for certain choices.
 */
static PreferenceDialogItem appearance_items[] = {
	{ N_("Smoother Graphics"),
	  NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
	  N_("Use smoother (but slower) graphics"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Fonts"),
	  NAUTILUS_PREFERENCES_DEFAULT_FONT,
	  N_("Font for elsewhere in Nautilus:"),
	  NAUTILUS_PREFERENCE_ITEM_FONT,
	  NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
	  NAUTILUS_PREFERENCE_ITEM_HIDE
	},
	{ N_("Fonts"),
	  NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT,
	  N_("Font for elsewhere in Nautilus:"),
	  NAUTILUS_PREFERENCE_ITEM_SMOOTH_FONT,
	  NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
	  NAUTILUS_PREFERENCE_ITEM_SHOW
	},
	{ NULL }
};

static PreferenceDialogItem windows_and_desktop_items[] = {
	{ N_("Desktop"),
	  NAUTILUS_PREFERENCES_SHOW_DESKTOP,
	  N_("Use Nautilus to draw the desktop"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Opening New Windows"),
	  NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
	  N_("Open each file or folder in a separate window"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Opening New Windows"),
	  NAUTILUS_PREFERENCES_START_WITH_TOOLBAR,
	  N_("Display toolbar in new windows"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Opening New Windows"),
	  NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR,
	  N_("Display location bar in new windows"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Opening New Windows"),
	  NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR,
	  N_("Display status bar in new windows"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Opening New Windows"),
	  NAUTILUS_PREFERENCES_START_WITH_SIDEBAR,
	  N_("Display sidebar in new windows"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Trash Behavior"),
	  NAUTILUS_PREFERENCES_CONFIRM_TRASH,
	  N_("Ask before emptying the Trash or deleting files"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Trash Behavior"),
	  NAUTILUS_PREFERENCES_ENABLE_DELETE,
	  N_("Include a Delete command that bypasses Trash"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	/* FIXME: This group clearly doesn't belong in Windows &
	 * Desktop, but there's no obviously-better place for it and
	 * it probably doesn't deserve a pane of its own.
	 */
	{ N_("Keyboard Shortcuts"),
	  NAUTILUS_PREFERENCES_USE_EMACS_SHORTCUTS,
	  N_("Use Emacs-style keyboard shortcuts in text fields"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ NULL }
};

static PreferenceDialogItem directory_views_items[] = {
	{ N_("Click Behavior"),
	  NAUTILUS_PREFERENCES_CLICK_POLICY,
	  N_("Click Behavior"),
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_VERTICAL_RADIO
	},
	{ N_("Executable Text Files"),
	  NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION,
	  N_("Executable Text Files"),
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_VERTICAL_RADIO
	},
	{ N_("Show/Hide Options"),
	  NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
	  N_("Show hidden files (file names start with \".\")"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Show/Hide Options"),
	  NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
	  N_("Show backup files (file names end with \"~\")"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Show/Hide Options"),
	  NAUTILUS_PREFERENCES_SHOW_SPECIAL_FLAGS,
	  N_("Show special flags in Properties window"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Sorting Order"),
	  NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST,
	  N_("Always list folders before files"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ NULL }
};

static PreferenceDialogItem icon_captions_items[] = {
	{ N_("Icon Captions"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
	  N_("Choose the order for information to appear beneath icon names.\n"
	     "More information appears as you zoom in closer"),
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_LIST_VERTICAL,
	  NULL,
	  0,
	  0,
	  "none"
	},
	{ NULL }
};

static PreferenceDialogItem view_preferences_items[] = {
	{ N_("Default View"),
	  NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER,
	  N_("View new folders using:"),
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_MENU
	},

	/* Icon View Defaults */
	{ N_("Icon View Defaults"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER,
	  N_("Lay Out Items:"),
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_MENU
	},
	{ N_("Icon View Defaults"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
	  N_("Sort in reversed order"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("Icon View Defaults"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_FONT,
	  N_("Font:"),
	  NAUTILUS_PREFERENCE_ITEM_FONT,
	  NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
	  NAUTILUS_PREFERENCE_ITEM_HIDE
	},
	{ N_("Icon View Defaults"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_SMOOTH_FONT,
	  N_("Font:"),
	  NAUTILUS_PREFERENCE_ITEM_SMOOTH_FONT,
	  NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
	  NAUTILUS_PREFERENCE_ITEM_SHOW
	},
	{ N_("Icon View Defaults"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
	  N_("Default zoom level:"),
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_MENU,
	  NULL, 0, 1
	},
	{ N_("Icon View Defaults"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT,
	  N_("Use tighter layout"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL, 0, 1
	},
	{ N_("Icon View Defaults"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE,
	  N_("Font size at default zoom level:"),
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_MENU,
	  NULL, 0, 1
	},

	/* List View Defaults */
	{ N_("List View Defaults"),
	  NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER,
	  N_("Lay Out Items:"),
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_MENU
	},
	{ N_("List View Defaults"),
	  NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
	  N_("Sort in reversed order"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("List View Defaults"),
	  NAUTILUS_PREFERENCES_LIST_VIEW_FONT,
	  N_("Font:"),
	  NAUTILUS_PREFERENCE_ITEM_FONT
	},
	{ N_("List View Defaults"),
	  NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
	  N_("Default zoom level:"),
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_MENU,
	  NULL, 0, 1
	},
	{ N_("List View Defaults"),
	  "dummy-string",
	  NULL,
	  NAUTILUS_PREFERENCE_ITEM_PADDING,
	  NULL, 0, 1
	},
	{ N_("List View Defaults"),
	  NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL_FONT_SIZE,
	  N_("Font size at default zoom level:"),
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_MENU,
	  NULL, 0, 1
	},
	{ NULL }
};

static PreferenceDialogItem search_items[] = {
	{ N_("Search Complexity Options"),
	  NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
	  N_("search type to do by default"),
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_VERTICAL_RADIO
	},
	{ N_("Search Engines"),
	  NAUTILUS_PREFERENCES_SEARCH_WEB_URI,
	  N_("Search Engine Location"),
	  NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING
	},
	{ NULL }
};

static PreferenceDialogItem navigation_items[] = {
	{ N_("Home"),
	  NAUTILUS_PREFERENCES_HOME_URI,
	  N_("Location:"),
	  NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING
	},
	{ N_("HTTP Proxy Settings"),
	  NAUTILUS_PREFERENCES_HTTP_USE_PROXY,
	  N_("Use HTTP Proxy"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ N_("HTTP Proxy Settings"),
	  NAUTILUS_PREFERENCES_HTTP_PROXY_HOST,
	  N_("Location:"),
	  NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING
	},
	{ N_("HTTP Proxy Settings"),
	  NAUTILUS_PREFERENCES_HTTP_PROXY_PORT,
	  N_("Port:"),
	  NAUTILUS_PREFERENCE_ITEM_EDITABLE_INTEGER
	},
	{ N_("HTTP Proxy Settings"),
	  NAUTILUS_PREFERENCES_HTTP_PROXY_USE_AUTH,
	  N_("Proxy requires a username and password:"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NAUTILUS_PREFERENCES_HTTP_USE_PROXY,
	  NAUTILUS_PREFERENCE_ITEM_SHOW
	},
	{ N_("HTTP Proxy Settings"),
	  NAUTILUS_PREFERENCES_HTTP_PROXY_AUTH_USERNAME,
	  N_("Username:"),
	  NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING,
	  NAUTILUS_PREFERENCES_HTTP_USE_PROXY,
	  NAUTILUS_PREFERENCE_ITEM_SHOW
	},
	{ N_("HTTP Proxy Settings"),
	  NAUTILUS_PREFERENCES_HTTP_USE_AUTH_PASSWORD,
	  N_("Password:"),
	  NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING,
	  NAUTILUS_PREFERENCES_HTTP_USE_PROXY,
	  NAUTILUS_PREFERENCE_ITEM_SHOW
	},
	{ N_("Built-in Bookmarks"),
	  NAUTILUS_PREFERENCES_HIDE_BUILT_IN_BOOKMARKS,
	  N_("Don't include the built-in bookmarks in the Bookmarks menu"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ NULL }
};

static PreferenceDialogItem tradeoffs_items[] = {
	{ N_("Show Text in Icons"),
	  NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS,
	  NULL,
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_HORIZONTAL_RADIO
	},
	{ N_("Show Count of Items in Folders"),
	  NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
	  NULL,
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_HORIZONTAL_RADIO
	},
	{ N_("Show Thumbnails for Image Files"),
	  NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
	  NULL,
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_HORIZONTAL_RADIO
	},
	{ N_("Show Thumbnails for Image Files"),
	  NAUTILUS_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT,
	  N_("Don't make thumbnails for files larger than:"),
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_MENU
	},
	{ N_("Preview Sound Files"),
	  NAUTILUS_PREFERENCES_PREVIEW_SOUND,
	  NULL,
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_HORIZONTAL_RADIO
	},

	/* FIXME bugzilla.eazel.com 2560: This title phrase needs improvement. */
	{ N_("Make Folder Appearance Details Public"),
	  NAUTILUS_PREFERENCES_USE_PUBLIC_METADATA,
	  NULL,
	  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_HORIZONTAL_RADIO
	},
	{ NULL }
};

static GtkWidget *
global_preferences_create_dialog (void)
{
	GtkWidget *prefs_dialog;
	NautilusPreferencesBox *preference_box;

	/*
	 * In the soon to come star trek future, the following widgetry
	 * might be either fetched from a glade file or generated from 
	 * an xml file.
	 */
	prefs_dialog = nautilus_preferences_dialog_new (GLOBAL_PREFERENCES_DIALOG_TITLE);

	gtk_window_set_wmclass (GTK_WINDOW (prefs_dialog), "global_preferences", "Nautilus");

	gtk_signal_connect (GTK_OBJECT (prefs_dialog),
			    "close",
			    GTK_SIGNAL_FUNC (global_preferences_close_dialog_callback),
			    NULL);

	/* Create a preference box */
	preference_box = NAUTILUS_PREFERENCES_BOX (nautilus_preferences_dialog_get_prefs_box
						   (NAUTILUS_PREFERENCES_DIALOG (prefs_dialog)));

	/* View Preferences */
	global_preferences_populate_pane (preference_box,
					  _("View Preferences"),
					  view_preferences_items);
	
	/* Appearance */
	global_preferences_populate_pane (preference_box,
					  _("Appearance"),
					  appearance_items);
	
	/* Windows & Desktop */
	global_preferences_populate_pane (preference_box,
					  _("Windows & Desktop"),
					  windows_and_desktop_items);

	/* Directory Views */
	global_preferences_populate_pane (preference_box,
					  _("Icon & List Views"),
					  directory_views_items);

	/* Icon Captions */
	global_preferences_populate_pane (preference_box,
					  _("Icon Captions"),
					  icon_captions_items);

	/* Sidebar Panels */
	global_preferences_create_sidebar_panels_pane (preference_box);

	/* Search */
	global_preferences_populate_pane (preference_box,
					  _("Search"),
					  search_items);

	/* Navigation */
	global_preferences_populate_pane (preference_box,
					  _("Navigation"),
					  navigation_items);

	/* Tradeoffs */
	global_preferences_populate_pane (preference_box,
					  _("Speed Tradeoffs"),
					  tradeoffs_items);

	/* Update the dialog so that the right items show up based on the current user level */
	nautilus_preferences_dialog_update (NAUTILUS_PREFERENCES_DIALOG (prefs_dialog));

	return prefs_dialog;
}

static PreferenceDialogItem sidebar_items[] = {
	{ N_("Tree"),
	  NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES,
	  N_("Show only folders (no files) in the tree"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN
	},
	{ NULL }
};

static void
global_preferences_populate_sidebar_panels_callback (const char *name,
						     const char *iid,
						     const char *preference_key,
						     gpointer callback_data) 
{
	NautilusPreferencesPane *sidebar_pane;
	char *description;

	g_return_if_fail (name != NULL);
	g_return_if_fail (iid != NULL);
	g_return_if_fail (preference_key != NULL);
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_PANE (callback_data));

	sidebar_pane = NAUTILUS_PREFERENCES_PANE (callback_data);
	
	description = g_strdup_printf (_("Display %s tab in sidebar"), name);
	
	nautilus_preferences_set_description (preference_key, description);

	nautilus_preferences_pane_add_item_to_nth_group (sidebar_pane,
							 0,
							 preference_key,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
							 0);
	
	g_free (description);
}

static void
global_preferences_create_sidebar_panels_pane (NautilusPreferencesBox *preference_box)
{
	GtkWidget *sidebar_pane;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_BOX (preference_box));

 	/* Sidebar Panels - dynamic part */
 	sidebar_pane = nautilus_preferences_box_add_pane (preference_box, _("Sidebar Panels"));
	
 	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (sidebar_pane), _("Tabs"));

	nautilus_sidebar_for_each_panel (global_preferences_populate_sidebar_panels_callback, sidebar_pane);

	/* Sidebar Panels - non dynamic parts */
	global_preferences_populate_pane (preference_box,
					  _("Sidebar Panels"),
					  sidebar_items);
}

static void
destroy_global_prefs_dialog (void)
{
	/* Free the dialog first, cause it has refs to preferences */
	if (global_prefs_dialog != NULL) {
		/* Since it's a top-level window, it's OK to destroy rather than unref'ing. */
		gtk_widget_destroy (global_prefs_dialog);
	}
}

static GtkWidget *
global_preferences_get_dialog (void)
{
	static gboolean set_up_exit = FALSE;

	nautilus_global_preferences_initialize ();

	if (global_prefs_dialog == NULL) {
		global_prefs_dialog = global_preferences_create_dialog ();
	}

	if (!set_up_exit) {
		g_atexit (destroy_global_prefs_dialog);
		set_up_exit = TRUE;
	}

	return global_prefs_dialog;
}

static gboolean
global_preferences_close_dialog_callback (GtkWidget   *dialog,
					  gpointer    user_data)
{
	nautilus_global_preferences_hide_dialog ();

	return TRUE;
}

static GtkWidget *
global_preferences_populate_pane (NautilusPreferencesBox *preference_box,
				  const char *pane_name,
				  const PreferenceDialogItem *preference_dialog_item)
{
	GtkWidget *pane;
	GtkWidget *item;
	EelStringList *group_names;
	guint i;
	int group_index;
	guint start_group_index;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_BOX (preference_box), NULL);
	g_return_val_if_fail (pane_name != NULL, NULL);
	g_return_val_if_fail (preference_dialog_item != NULL, NULL);

	/* Create the pane if needed */
	pane = nautilus_preferences_box_find_pane (preference_box, pane_name);
	if (pane == NULL) {
		pane = nautilus_preferences_box_add_pane (preference_box, pane_name);
	}

	group_names = eel_string_list_new (TRUE);

	start_group_index = nautilus_preferences_pane_get_num_groups (NAUTILUS_PREFERENCES_PANE (pane));

	for (i = 0; preference_dialog_item[i].group_name != NULL; i++) {
		if (!eel_string_list_contains (group_names, preference_dialog_item[i].group_name)) {
			eel_string_list_insert (group_names, preference_dialog_item[i].group_name);
			nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (pane),
							     _(preference_dialog_item[i].group_name));
		}
	}

	for (i = 0; preference_dialog_item[i].group_name != NULL; i++) {
		group_index = start_group_index + 
			eel_string_list_get_index_for_string (group_names, preference_dialog_item[i].group_name);

		if (preference_dialog_item[i].preference_description != NULL) {
			nautilus_preferences_set_description (preference_dialog_item[i].preference_name,
							      _(preference_dialog_item[i].preference_description));
		}

		item = nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (pane),
									group_index,
									preference_dialog_item[i].preference_name,
									preference_dialog_item[i].item_type,
									preference_dialog_item[i].column);

		/* Install a control preference if needed */
		if (preference_dialog_item[i].control_preference_name != NULL) {
			nautilus_preferences_item_set_control_preference (NAUTILUS_PREFERENCES_ITEM (item),
									  preference_dialog_item[i].control_preference_name);
			nautilus_preferences_item_set_control_action (NAUTILUS_PREFERENCES_ITEM (item),
								      preference_dialog_item[i].control_action);

			nautilus_preferences_pane_add_control_preference (NAUTILUS_PREFERENCES_PANE (pane),
									  preference_dialog_item[i].control_preference_name);

		}

		/* Install exceptions to enum lists uniqueness rule */
		if (preference_dialog_item[i].enumeration_list_unique_exceptions != NULL) {
			g_assert (preference_dialog_item[i].item_type == 
				  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_LIST_VERTICAL
				  || preference_dialog_item[i].item_type == 
				  NAUTILUS_PREFERENCE_ITEM_ENUMERATION_LIST_HORIZONTAL);
			nautilus_preferences_item_enumeration_list_set_unique_exceptions (
				NAUTILUS_PREFERENCES_ITEM (item),
				preference_dialog_item[i].enumeration_list_unique_exceptions,
				STRING_LIST_DEFAULT_TOKENS_DELIMETER);
		}
	}

	eel_string_list_free (group_names);

	return pane;
}

static gpointer
default_font_callback (int user_level)
{
	g_return_val_if_fail (nautilus_preferences_user_level_is_valid (user_level), NULL);

	if (eel_dumb_down_for_multi_byte_locale_hack ()) {
		return g_strdup ("fixed");
	}

	return g_strdup ("helvetica");
}

static gpointer
default_smooth_font_callback (int user_level)
{
	g_return_val_if_fail (nautilus_preferences_user_level_is_valid (user_level), NULL);
	return eel_font_manager_get_default_font ();
}

static gpointer
default_home_location_callback (int user_level)
{
	char *default_home_location;
	char *user_main_directory;		

	g_return_val_if_fail (nautilus_preferences_user_level_is_valid (user_level), NULL);

	if (user_level == NAUTILUS_USER_LEVEL_NOVICE) {
		user_main_directory = nautilus_get_user_main_directory ();
		default_home_location = gnome_vfs_get_uri_from_local_path (user_main_directory);
		g_free (user_main_directory);
		return default_home_location;
	}

	if (user_level == NAUTILUS_USER_LEVEL_INTERMEDIATE) {
		return gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
	}
	
	return NULL;
}

/*
 * Public functions
 */
void
nautilus_global_preferences_show_dialog (void)
{
	GtkWidget *dialog = global_preferences_get_dialog ();

	eel_gtk_window_present (GTK_WINDOW (dialog));
}

void
nautilus_global_preferences_hide_dialog (void)
{
	GtkWidget *dialog = global_preferences_get_dialog ();

	gtk_widget_hide (dialog);
}

void
nautilus_global_preferences_set_dialog_title (const char *title)
{
	GtkWidget *dialog;
	g_return_if_fail (title != NULL);
	
	dialog = global_preferences_get_dialog ();

	gtk_window_set_title (GTK_WINDOW (dialog), title);
}

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

	is_smooth = nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE);
	
	eel_smooth_widget_global_set_is_smooth (is_smooth);
}

void
nautilus_global_preferences_initialize (void)
{
	static gboolean initialized = FALSE;

	if (initialized) {
		return;
	}

	initialized = TRUE;

	nautilus_preferences_initialize ("/apps/nautilus");

	/* Install defaults */
	global_preferences_install_defaults ();

	global_preferences_register_enumerations ();

	/* Set up storage for values accessed in this file */
	nautilus_preferences_add_auto_string (NAUTILUS_PREFERENCES_ICON_VIEW_SMOOTH_FONT,
					      &icon_view_smooth_font_auto_value);
	nautilus_preferences_add_auto_string (NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT,
					      &default_smooth_font_auto_value);

	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE, 
					   smooth_graphics_mode_changed_callback, 
					   NULL);

	/* Keep track of smooth graphics mode changes in order to notify the smooth
	 * widget machinery.
	 */
	smooth_graphics_mode_changed_callback (NULL);
}
