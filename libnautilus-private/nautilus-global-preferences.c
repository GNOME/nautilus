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
#include <eel/eel-enumeration.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <glib/gi18n.h>
#include <libgnome/gnome-util.h>

/* Constants */
#define STRING_ARRAY_DEFAULT_TOKENS_DELIMETER ","
#define PREFERENCES_SORT_ORDER_MANUALLY 100

/* Path for gnome-vfs preferences */
static const char *EXTRA_MONITOR_PATHS[] = { "/system/gnome_vfs",
					     "/desktop/gnome/file_views",
					     NULL };

/* Forward declarations */
static void     global_preferences_install_defaults      (void);
static void     global_preferences_register_enumerations (void);
static gpointer default_font_callback                    (void);
static gpointer default_home_link_name                   (void);
static gpointer default_computer_link_name               (void);
static gpointer default_trash_link_name                  (void);
static gpointer default_network_link_name                (void);


/* An enumeration used for installing type specific preferences defaults. */
typedef enum
{
	PREFERENCE_BOOLEAN = 1,
	PREFERENCE_INTEGER,
	PREFERENCE_STRING,
	PREFERENCE_STRING_ARRAY
} PreferenceType;

/* Enumerations used to qualify some INTEGER preferences */
static EelEnumerationEntry speed_tradeoff_enum_entries[] = {
	{ "always",	    N_("_Always"),		NAUTILUS_SPEED_TRADEOFF_ALWAYS },
	{ "local_only",	    N_("_Local File Only"),	NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY },
	{ "never",	    N_("_Never"),		NAUTILUS_SPEED_TRADEOFF_NEVER }
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
	{ "largest",	    N_("400%"),		NAUTILUS_ZOOM_LEVEL_LARGEST }
};

static EelEnumerationEntry file_size_enum_entries[] = {
	{ "102400",	    N_("100 K"),	102400 },
	{ "512000",	    N_("500 K"),	512000 },
	{ "1048576",	    N_("1 MB"),		1048576 },
	{ "3145728",	    N_("3 MB"),		3145728 },
	{ "5242880",	    N_("5 MB"),		5242880 },
	{ "10485760",	    N_("10 MB"),	10485760 },
	{ "104857600",	    N_("100 MB"),	104857600 },
	{ "1073741824",     N_("1 GB"),         1073741824 }
};

static EelEnumerationEntry click_policy_enum_entries[] = {
	{ "single",
	  N_("Activate items with a _single click"),
	  NAUTILUS_CLICK_POLICY_SINGLE
	},
	{ "double",
	  N_("Activate items with a _double click"),
	  NAUTILUS_CLICK_POLICY_DOUBLE
	}
};

static EelEnumerationEntry executable_text_activation_enum_entries[] = {
	{ "launch",
	  N_("E_xecute files when they are clicked"),
	  NAUTILUS_EXECUTABLE_TEXT_LAUNCH
	},
	{ "display",
	  N_("Display _files when they are clicked"),
	  NAUTILUS_EXECUTABLE_TEXT_DISPLAY
	},
	{ "ask",
	  N_("_Ask each time"),
	  NAUTILUS_EXECUTABLE_TEXT_ASK
	}
};

static EelEnumerationEntry search_bar_type_enum_entries[] = {
	{ "search by text",
	  N_("Search for files by file name only"),
	  NAUTILUS_SIMPLE_SEARCH_BAR
	},
	{ "search by text and properties",
	  N_("Search for files by file name and file properties"),
	  NAUTILUS_COMPLEX_SEARCH_BAR
	}
};

static EelEnumerationEntry default_folder_viewer_enum_entries[] = {
	{ "icon_view",	    N_("Icon View"),	NAUTILUS_DEFAULT_FOLDER_VIEWER_ICON_VIEW },
	{ "compact_view",   N_("Compact View"),	NAUTILUS_DEFAULT_FOLDER_VIEWER_COMPACT_VIEW },
	{ "list_view",	    N_("List View"),	NAUTILUS_DEFAULT_FOLDER_VIEWER_LIST_VIEW }
};

static EelEnumerationEntry default_icon_view_sort_order_enum_entries[] = {
	{ "manually",	       N_("Manually"),		    PREFERENCES_SORT_ORDER_MANUALLY },
	{ "--------",             "--------" },
	{ "name",	       N_("By Name"),		    NAUTILUS_FILE_SORT_BY_DISPLAY_NAME },
	{ "size",	       N_("By Size"),		    NAUTILUS_FILE_SORT_BY_SIZE },
	{ "type",	       N_("By Type"),		    NAUTILUS_FILE_SORT_BY_TYPE },
	{ "modification_date", N_("By Modification Date"),  NAUTILUS_FILE_SORT_BY_MTIME }, 
	{ "emblems",	       N_("By Emblems"),	    NAUTILUS_FILE_SORT_BY_EMBLEMS }
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
	{ "24",		   N_("24"),	24 }
};

/* These are not translated, because the text is not used in the ui */
static EelEnumerationEntry date_format_entries[] = {
	{ "locale",	   "Locale Default",	NAUTILUS_DATE_FORMAT_LOCALE },
	{ "iso",	   "ISO Format",	NAUTILUS_DATE_FORMAT_ISO },
	{ "informal",	   "Informal",		NAUTILUS_DATE_FORMAT_INFORMAL }
};

/*
 * A callback which can be used to fetch dynamic fallback values.
 * For example, values that are dependent on the environment (such as user name) 
 * cannot be specified as constants.
 */
typedef gpointer (*PreferencesDefaultValueCallback) (void);

/* A structure that describes a single preference including defaults and visibility. */
typedef struct
{
	const char *name;
	PreferenceType type;
	const gpointer fallback_value;
	PreferencesDefaultValueCallback fallback_callback;
	GFreeFunc fallback_callback_result_free_function;
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
 *	PREFERENCE_STRING_ARRAY
 * 
 * 3. fallback_value
 *    Emergency fallback value if our gconf schemas are hosed somehow.
 * 
 * 4. fallback_callback
 *    callback to get dynamic fallback
 *
 * 5. fallback_callback_result_free_function
 *    free result of fallback_callback
 *
 * 6. enumeration_id
 *    An an enumeration id is a unique string that identifies an enumeration.
 *    If given, an enumeration id can be used to qualify a INTEGER preference.
 *    The preferences dialog widgetry will use this enumeration id to find out
 *    what choices and descriptions of choices to present to the user.
 */

/* NOTE THAT THE FALLBACKS HERE ARE NOT SUPPOSED TO BE USED -
 * YOU SHOULD EDIT THE SCHEMAS FILE TO CHANGE DEFAULTS.
 */
static const PreferenceDefault preference_defaults[] = {
	{ NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},
	{ NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},
	{ NAUTILUS_PREFERENCES_CONFIRM_TRASH,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (TRUE)
	},
	{ NAUTILUS_PREFERENCES_ENABLE_DELETE,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},
	{ NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS,
	  PREFERENCE_STRING,
	  "local_only",
	  NULL, NULL,
	  "speed_tradeoff"
	},
	/* Don't show remote directory item counts by default
	 * because computing them can be annoyingly slow, especially
	 * for FTP. If we make this fast enough for FTP in particular,
	 * we should change this default to ALWAYS.
	 */
	{ NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
	  PREFERENCE_STRING,
	  "local_only",
	  NULL, NULL,
	  "speed_tradeoff"
	},
	{ NAUTILUS_PREFERENCES_CLICK_POLICY,
	  PREFERENCE_STRING,
	  "double",
	  NULL, NULL,
	  "click_policy"
	},
	{ NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION,
	  PREFERENCE_STRING,
	  "ask",
	  NULL, NULL,
	  "executable_text_activation"
	},
	{ NAUTILUS_PREFERENCES_THEME,
	  PREFERENCE_STRING,
	  "default"
	},
	{ NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
	  PREFERENCE_STRING,
	  "local_only",
	  NULL, NULL,
	  "speed_tradeoff"
	},
	{ NAUTILUS_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT,
	  PREFERENCE_INTEGER,
	  GINT_TO_POINTER(10485760),
	  NULL, NULL,
	  "file_size"
	},
	{ NAUTILUS_PREFERENCES_PREVIEW_SOUND,
	  PREFERENCE_STRING,
	  "local_only",
	  NULL, NULL,
	  "speed_tradeoff"
	},
	{ NAUTILUS_PREFERENCES_SHOW_ADVANCED_PERMISSIONS,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},
	{ NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},
	{ NAUTILUS_PREFERENCES_SHOW_DESKTOP,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (TRUE)
	},
	{ NAUTILUS_PREFERENCES_DESKTOP_IS_HOME_DIR,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},
	{ NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
	  PREFERENCE_STRING,
	  "search_by_text",
	  NULL, NULL,
	  "search_bar_type"
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_CAPTIONS,
	  PREFERENCE_STRING_ARRAY,
	  "size,date_modified,type",
	  NULL, NULL,
	  NULL
	},
	/* FIXME bugzilla.gnome.org 41245: Saved in pixels instead of in %? */
	{ NAUTILUS_PREFERENCES_SIDEBAR_WIDTH,
	  PREFERENCE_INTEGER,
	  GINT_TO_POINTER (148)
	},
	{ NAUTILUS_PREFERENCES_ALWAYS_USE_BROWSER,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},
	{ NAUTILUS_PREFERENCES_START_WITH_TOOLBAR,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (TRUE)
	},
	{ NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (TRUE)
	},
	{ NAUTILUS_PREFERENCES_ALWAYS_USE_LOCATION_ENTRY,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},
	{ NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (TRUE)
	},
	{ NAUTILUS_PREFERENCES_START_WITH_SIDEBAR,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (TRUE)
	},
	{ NAUTILUS_PREFERENCES_NAVIGATION_WINDOW_SAVED_GEOMETRY,
	  PREFERENCE_STRING,
	  ""
	},
	{ NAUTILUS_PREFERENCES_NAVIGATION_WINDOW_MAXIMIZED,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},
	{ NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (TRUE)
	},
	{ NAUTILUS_PREFERENCES_DATE_FORMAT,
	  PREFERENCE_STRING,
	  "locale",
	  NULL, NULL,
	  "date_format"
	},
	{ NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER,
	  PREFERENCE_INTEGER,
	  GINT_TO_POINTER (NAUTILUS_DEFAULT_FOLDER_VIEWER_ICON_VIEW),
	  NULL, NULL,
	  "default_folder_viewer"
	},
	{ NAUTILUS_PREFERENCES_DESKTOP_FONT,
	  PREFERENCE_STRING,
	  NULL, default_font_callback, g_free
	},

	/* Icon View Default Preferences */
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER,
	  PREFERENCE_STRING,
	  "name",
	  NULL, NULL,
	  "default_icon_view_sort_order"
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER_OR_MANUAL_LAYOUT,
	  PREFERENCE_STRING,
	  "name",
	  NULL, NULL,
	  "default_icon_view_sort_order"
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_TIGHTER_LAYOUT,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_MANUAL_LAYOUT,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_ZOOM_LEVEL,
	  PREFERENCE_STRING,
	  "standard",
	  NULL, NULL,
	  "default_zoom_level"
	},
	{ NAUTILUS_PREFERENCES_ICON_VIEW_THUMBNAIL_SIZE,
	  PREFERENCE_INTEGER,
	  GINT_TO_POINTER (96)
	},

	/* Compact Icon View Default Preferences */
	{ NAUTILUS_PREFERENCES_COMPACT_VIEW_DEFAULT_ZOOM_LEVEL,
	  PREFERENCE_STRING,
	  "standard",
	  NULL, NULL,
	  "default_zoom_level"
	},
	
	/* List View Default Preferences */
	{ NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_ORDER,
	  PREFERENCE_STRING,
	  "name",
	  NULL, NULL,
	  NULL,
	},
	{ NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_SORT_IN_REVERSE_ORDER,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},
	{ NAUTILUS_PREFERENCES_LIST_VIEW_DEFAULT_ZOOM_LEVEL,
	  PREFERENCE_STRING,
	  "smaller",
	  NULL, NULL,
	  "default_zoom_level"
	},

	/* Desktop Preferences */
	{ NAUTILUS_PREFERENCES_DESKTOP_HOME_VISIBLE,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},
	
	{ NAUTILUS_PREFERENCES_DESKTOP_HOME_NAME,
	  PREFERENCE_STRING,
	  NULL,
	  default_home_link_name, g_free,
	},
	
	{ NAUTILUS_PREFERENCES_DESKTOP_COMPUTER_VISIBLE,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (TRUE)
	},
	
	{ NAUTILUS_PREFERENCES_DESKTOP_COMPUTER_NAME,
	  PREFERENCE_STRING,
	  NULL,
	  default_computer_link_name, g_free,
	},
	
	{ NAUTILUS_PREFERENCES_DESKTOP_TRASH_VISIBLE,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (TRUE)
	},
	
	{ NAUTILUS_PREFERENCES_DESKTOP_TRASH_NAME,
	  PREFERENCE_STRING,
	  NULL,
	  default_trash_link_name, g_free,
	},
	
	{ NAUTILUS_PREFERENCES_DESKTOP_VOLUMES_VISIBLE,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (TRUE)
	},

	{ NAUTILUS_PREFERENCES_DESKTOP_NETWORK_VISIBLE,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},

	{ NAUTILUS_PREFERENCES_DESKTOP_NETWORK_NAME,
	  PREFERENCE_STRING,
	  NULL,
	  default_network_link_name, g_free,
	},

	{ NAUTILUS_PREFERENCES_MEDIA_AUTOMOUNT,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (TRUE)
	},
	{ NAUTILUS_PREFERENCES_MEDIA_AUTOMOUNT_OPEN,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (TRUE)
	},
	{ NAUTILUS_PREFERENCES_MEDIA_AUTORUN_NEVER,
	  PREFERENCE_BOOLEAN,
	  GINT_TO_POINTER (FALSE)
	},
	{ NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_ASK,
	  PREFERENCE_STRING_ARRAY,
	  "", NULL, NULL, NULL
	},
	{ NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_IGNORE,
	  PREFERENCE_STRING_ARRAY,
	  "", NULL, NULL, NULL
	},	
	{ NAUTILUS_PREFERENCES_MEDIA_AUTORUN_X_CONTENT_OPEN_FOLDER,
	  PREFERENCE_STRING_ARRAY,
	  "", NULL, NULL, NULL
	},	
	{ NULL }
};

static gpointer
default_home_link_name (void)
{
	/* Note to translators: If it's hard to compose a good home
	 * icon name from the user name, you can use a string without
	 * an "%s" here, in which case the home icon name will not
	 * include the user's name, which should be fine. To avoid a
	 * warning, put "%.0s" somewhere in the string, which will
	 * match the user name string passed by the C code, but not
	 * put the user name in the final string.
	 */
	return g_strdup_printf (_("%s's Home"), g_get_user_name ());
}

static gpointer
default_computer_link_name (void)
{
	return g_strdup (_("Computer"));
}

static gpointer
default_trash_link_name (void)
{
	return g_strdup (_("Trash"));
}

static gpointer
default_network_link_name (void)
{
	return g_strdup (_("Network Servers"));
}



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

	/* Register the enumerations.
	 * These enumerations are used in the preferences dialog to 
	 * populate widgets and route preferences changes between the
	 * storage (GConf) and the displayed values.
	 */
	eel_enumeration_register ("click_policy",
				  click_policy_enum_entries,
				  G_N_ELEMENTS (click_policy_enum_entries));
	eel_enumeration_register ("default_folder_viewer",
				  default_folder_viewer_enum_entries,
				  G_N_ELEMENTS (default_folder_viewer_enum_entries));
	eel_enumeration_register ("default_icon_view_sort_order",
				  default_icon_view_sort_order_enum_entries,
				  G_N_ELEMENTS (default_icon_view_sort_order_enum_entries));
	eel_enumeration_register ("default_zoom_level",
				  default_zoom_level_enum_entries,
				  G_N_ELEMENTS (default_zoom_level_enum_entries));
	eel_enumeration_register ("executable_text_activation",
				  executable_text_activation_enum_entries,
				  G_N_ELEMENTS (executable_text_activation_enum_entries));
	eel_enumeration_register ("file_size",
				  file_size_enum_entries,
				  G_N_ELEMENTS (file_size_enum_entries));
	eel_enumeration_register ("search_bar_type",
				  search_bar_type_enum_entries,
				  G_N_ELEMENTS (search_bar_type_enum_entries));
	eel_enumeration_register ("speed_tradeoff",
				  speed_tradeoff_enum_entries,
				  G_N_ELEMENTS (speed_tradeoff_enum_entries));
	eel_enumeration_register ("standard_font_size",
				  standard_font_size_entries,
				  G_N_ELEMENTS (standard_font_size_entries));
	eel_enumeration_register ("date_format",
				  date_format_entries,
				  G_N_ELEMENTS (date_format_entries));

	/* Set the enumeration ids for preferences that need them */
	for (i = 0; preference_defaults[i].name != NULL; i++) {
		if (eel_strlen (preference_defaults[i].enumeration_id) > 0) {
			g_assert (preference_defaults[i].type == PREFERENCE_STRING
				  || preference_defaults[i].type == PREFERENCE_STRING_ARRAY
				  || preference_defaults[i].type == PREFERENCE_INTEGER);
			eel_preferences_set_enumeration_id (preference_defaults[i].name,
							    preference_defaults[i].enumeration_id);
		}
	}
}

static void
global_preferences_install_one_default (const char *preference_name,
					PreferenceType preference_type,
					const PreferenceDefault *preference_default)
{
	gpointer value = NULL;
	char **string_array_value;

	g_return_if_fail (preference_name != NULL);
	g_return_if_fail (preference_type >= PREFERENCE_BOOLEAN);
	g_return_if_fail (preference_type <= PREFERENCE_STRING_ARRAY);
	g_return_if_fail (preference_default != NULL);

	/* If a callback is given, use that to fetch the default value */
	if (preference_default->fallback_callback != NULL) {
		value = (* preference_default->fallback_callback) ();
	} else {
		value = preference_default->fallback_value;
	}

	switch (preference_type) {
	case PREFERENCE_BOOLEAN:
		eel_preferences_set_emergency_fallback_boolean (preference_name,
								GPOINTER_TO_INT (value));
		break;
		
	case PREFERENCE_INTEGER:
		eel_preferences_set_emergency_fallback_integer (preference_name,
								
								GPOINTER_TO_INT (value));
		break;
		
	case PREFERENCE_STRING:
		eel_preferences_set_emergency_fallback_string (preference_name,
							       value);
		break;

	case PREFERENCE_STRING_ARRAY:
		string_array_value = g_strsplit (value,
						 STRING_ARRAY_DEFAULT_TOKENS_DELIMETER,
						 -1);
		eel_preferences_set_emergency_fallback_string_array (preference_name,
								     string_array_value);
		g_strfreev (string_array_value);
		break;
		
	default:
		g_assert_not_reached ();
	}

	/* Free the dynamic default value if needed */
	if (preference_default->fallback_callback != NULL
	    && preference_default->fallback_callback_result_free_function != NULL) {
		(* preference_default->fallback_callback_result_free_function) (value);
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
							&preference_defaults[i]);
	}
}

static gpointer
default_font_callback (void)
{
	return g_strdup ("sans 12");
}

/*
 * Public functions
 */
char *
nautilus_global_preferences_get_default_folder_viewer_preference_as_iid (void)
{
	int preference_value;
	const char *viewer_iid;

	preference_value = 
		eel_preferences_get_enum (NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER);

	if (preference_value == NAUTILUS_DEFAULT_FOLDER_VIEWER_LIST_VIEW) {
		viewer_iid = NAUTILUS_LIST_VIEW_IID;
	} else if (preference_value == NAUTILUS_DEFAULT_FOLDER_VIEWER_COMPACT_VIEW) {
		viewer_iid = NAUTILUS_COMPACT_VIEW_IID;
	} else {
		viewer_iid = NAUTILUS_ICON_VIEW_IID;
	}

	return g_strdup (viewer_iid);
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
 		eel_preferences_get_enum (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER_OR_MANUAL_LAYOUT);

	eel_preferences_set_boolean (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_USE_MANUAL_LAYOUT,
				     default_sort_order_or_manual_layout == PREFERENCES_SORT_ORDER_MANUALLY);

	if (default_sort_order_or_manual_layout != PREFERENCES_SORT_ORDER_MANUALLY) {
		default_sort_order = default_sort_order_or_manual_layout;

		g_return_if_fail (default_sort_order >= NAUTILUS_FILE_SORT_BY_DISPLAY_NAME);
		g_return_if_fail (default_sort_order <= NAUTILUS_FILE_SORT_BY_EMBLEMS);

		eel_preferences_set_enum (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER,
					  default_sort_order);
	}
}

void
nautilus_global_preferences_init (void)
{
	static gboolean initialized = FALSE;
	int i;

	if (initialized) {
		return;
	}

	initialized = TRUE;

	eel_preferences_init ("/apps/nautilus");

	/* Install defaults */
	global_preferences_install_defaults ();

	global_preferences_register_enumerations ();	

	/* Add monitors for any other GConf paths we have keys in */
	for (i=0; EXTRA_MONITOR_PATHS[i] != NULL; i++) {
		eel_preferences_monitor_directory (EXTRA_MONITOR_PATHS[i]);
	}

	/* Set up storage for values accessed in this file */
 	eel_preferences_add_callback (NAUTILUS_PREFERENCES_ICON_VIEW_DEFAULT_SORT_ORDER_OR_MANUAL_LAYOUT,
				      default_icon_view_sort_order_or_manual_layout_changed_callback, 
				      NULL);

	/* Preload everything in a big batch */
	eel_gconf_preload_cache ("/apps/nautilus/preferences",
				 GCONF_CLIENT_PRELOAD_ONELEVEL);
}
