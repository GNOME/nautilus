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
#include "nautilus-font-manager.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-medusa-support.h"
#include "nautilus-preferences-dialog.h"
#include "nautilus-preferences-group.h"
#include "nautilus-preferences-item.h"
#include "nautilus-scalable-font.h"
#include "nautilus-string.h"
#include "nautilus-medusa-support.h"
#include "nautilus-stock-dialogs.h"
#include "nautilus-view-identifier.h"
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <gtk/gtkbox.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <liboaf/liboaf.h>
#include <libgnomevfs/gnome-vfs-utils.h>

/* Constants */
static const char untranslated_global_preferences_dialog_title[] = N_("Nautilus Preferences");
#define GLOBAL_PREFERENCES_DIALOG_TITLE _(untranslated_global_preferences_dialog_title)

static const char PROXY_HOST_KEY[] = "/system/gnome-vfs/http-proxy-host";
static const char PROXY_PORT_KEY[] = "/system/gnome-vfs/http-proxy-port";
static const char USE_PROXY_KEY[] = "/system/gnome-vfs/use-http-proxy";
static const char SYSTEM_GNOME_VFS_PATH[] = "/system/gnome-vfs";

/* A structure that describes a single preferences dialog ui item. */
typedef struct PreferenceDialogItem PreferenceDialogItem;

/* Forward declarations */
static char *     global_preferences_make_sidebar_panel_key             (const char                 *panel_iid);
static gboolean   global_preferences_is_sidebar_panel_enabled_cover     (gpointer                    data,
									 gpointer                    callback_data);
static GList *    global_preferences_get_sidebar_panel_view_identifiers (void);
static gboolean   global_preferences_close_dialog_callback              (GtkWidget                  *dialog,
									 gpointer                    user_data);
static void       global_preferences_install_sidebar_panel_defaults     (void);
static void       global_preferences_install_defaults                   (void);
static void       global_preferences_install_home_location_defaults     (void);
static void       global_preferences_install_medusa_defaults            (void);
static void       global_preferences_install_font_defaults              (void);
static int        compare_view_identifiers                              (gconstpointer               a,
									 gconstpointer               b);
static GtkWidget *global_preferences_create_dialog                      (void);
static void       global_preferences_create_search_pane                 (NautilusPreferencesBox     *preference_box);
static void       global_preferences_create_sidebar_panels_pane         (NautilusPreferencesBox     *preference_box);
static void       global_preferences_pane_update_callback               (gpointer                    callback_data);
static GtkWidget *global_preferences_populate_pane                      (NautilusPreferencesBox     *preference_box,
									 const char                 *pane_name,
									 const PreferenceDialogItem *preference_dialog_item);

static GtkWidget *global_prefs_dialog = NULL;
static const char *default_smooth_font_auto_value;
static const char *icon_view_smooth_font_auto_value;

/* An enumeration used for installing type specific preferences defaults. */
typedef enum
{
	PREFERENCE_BOOLEAN = 1,
	PREFERENCE_INTEGER,
	PREFERENCE_STRING
} PreferenceType;

/* A structure that pairs a default value with a specific user level. */
typedef struct
{
	int user_level;
	const gpointer value;
} PreferenceUserLevelDefault;

#define USER_LEVEL_NONE -1

/* A structure that describes a single preference including defaults and visibility. */
typedef struct
{
	const char *name;
	PreferenceType type;
	int visible_user_level;
	PreferenceUserLevelDefault default1;
	PreferenceUserLevelDefault default2;
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
	  { USER_LEVEL_NONE }
	},
	/* Don't show remote directory item counts for Beginner users because computing them
	 * can be annoyingly slow, especially for FTP. If we make this fast enough for FTP in
	 * particular, we should change this default to ALWAYS.
	 */
	{ NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY) },
	  { NAUTILUS_USER_LEVEL_INTERMEDIATE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_ALWAYS) }
	},
	{ NAUTILUS_PREFERENCES_CLICK_POLICY,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_ADVANCED,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_EXECUTABLE_TEXT_ASK) },
	  { USER_LEVEL_NONE }
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
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_ADVANCED,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (3145728) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_USE_PUBLIC_METADATA,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_ADVANCED,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY) },
	  { USER_LEVEL_NONE }
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
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_SHOW_SPECIAL_FLAGS,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_ADVANCED,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { NAUTILUS_USER_LEVEL_ADVANCED, GINT_TO_POINTER (TRUE) },
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
	  { NAUTILUS_USER_LEVEL_INTERMEDIATE, GINT_TO_POINTER (TRUE) },
	},
	{ NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_INTERMEDIATE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (NAUTILUS_SIMPLE_SEARCH_BAR) },
	  { NAUTILUS_USER_LEVEL_INTERMEDIATE, GINT_TO_POINTER (NAUTILUS_COMPLEX_SEARCH_BAR) },
	},
	{ NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ NAUTILUS_PREFERENCES_ICON_CAPTIONS,
	  PREFERENCE_STRING,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, "size|date_modified|type" },
	  { USER_LEVEL_NONE }
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
	{ USE_PROXY_KEY,
	  PREFERENCE_BOOLEAN,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (FALSE) },
	  { USER_LEVEL_NONE }
	},
	{ PROXY_PORT_KEY,
	  PREFERENCE_INTEGER,
	  NAUTILUS_USER_LEVEL_NOVICE,
	  { NAUTILUS_USER_LEVEL_NOVICE, GINT_TO_POINTER (8080) },
	  { USER_LEVEL_NONE }
	},

	{ NULL }
};

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
		switch (preference_defaults[i].type) {
		case PREFERENCE_BOOLEAN:
			if (preference_defaults[i].default1.user_level != USER_LEVEL_NONE) {
				nautilus_preferences_default_set_boolean (
					preference_defaults[i].name,
					preference_defaults[i].default1.user_level,
					GPOINTER_TO_INT (preference_defaults[i].default1.value));
			}
			if (preference_defaults[i].default2.user_level != USER_LEVEL_NONE) {
				nautilus_preferences_default_set_boolean (
					preference_defaults[i].name,
					preference_defaults[i].default2.user_level,
					GPOINTER_TO_INT (preference_defaults[i].default2.value));
			}
			break;

		case PREFERENCE_INTEGER:
			if (preference_defaults[i].default1.user_level != USER_LEVEL_NONE) {
				nautilus_preferences_default_set_integer (
					preference_defaults[i].name,
					preference_defaults[i].default1.user_level,
					GPOINTER_TO_INT (preference_defaults[i].default1.value));
			}
			if (preference_defaults[i].default2.user_level != USER_LEVEL_NONE) {
				nautilus_preferences_default_set_integer (
					preference_defaults[i].name,
					preference_defaults[i].default2.user_level,
					GPOINTER_TO_INT (preference_defaults[i].default2.value));
			}
			break;

		case PREFERENCE_STRING:
			if (preference_defaults[i].default1.user_level != USER_LEVEL_NONE) {
				nautilus_preferences_default_set_string (
					preference_defaults[i].name,
					preference_defaults[i].default1.user_level,
					preference_defaults[i].default1.value);
			}
			if (preference_defaults[i].default2.user_level != USER_LEVEL_NONE) {
				nautilus_preferences_default_set_string (
					preference_defaults[i].name,
					preference_defaults[i].default2.user_level,
					preference_defaults[i].default2.value);
			}
			break;
			
		default:
			g_assert_not_reached ();
		}

		nautilus_preferences_set_visible_user_level (preference_defaults[i].name,
							     preference_defaults[i].visible_user_level);
	}

	/* Add the gnome-vfs path to the list of monitored directories - for proxy settings */
	nautilus_preferences_monitor_directory (SYSTEM_GNOME_VFS_PATH);
	
	/* Sidebar panel defaults */
	global_preferences_install_sidebar_panel_defaults ();

	/* Home location */
	global_preferences_install_home_location_defaults ();

	/* Fonts */
	global_preferences_install_font_defaults ();

	/* Medusa */
	global_preferences_install_medusa_defaults ();
}

/*
 * Private stuff
 */
static int
compare_view_identifiers (gconstpointer a, gconstpointer b)
{
 	NautilusViewIdentifier *idenfifier_a;
 	NautilusViewIdentifier *idenfifier_b;
	
 	g_assert (a != NULL);
 	g_assert (b != NULL);

 	idenfifier_a = (NautilusViewIdentifier*) a;
 	idenfifier_b = (NautilusViewIdentifier*) b;
	
	return nautilus_strcmp (idenfifier_a->name, idenfifier_b->name);
}

/* An enumeration structure used for NAUTILUS_PREFERENCE_ITEM_ENUM items. */
typedef struct
{
	const char *stored_value;
	const char *display_value;
	int value;
} EnumerationEntry;

static EnumerationEntry speed_tradeoff_enumeration[] = {
	{ N_("always"),
	  N_("Always"),
	  NAUTILUS_SPEED_TRADEOFF_ALWAYS,
	},
	{ N_("local only"),
	  N_("Local Files Only"),
	  NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY,
	},
	{ N_("never"),
	  N_("Never"),
	  NAUTILUS_SPEED_TRADEOFF_NEVER,
	},
	{ NULL, NULL, 0 }
};

/* A structure that describes a single preferences dialog ui item. */
struct PreferenceDialogItem
{
	const char *group_name;
	const char *preference_name;
	const char *preference_description;
	NautilusPreferencesItemType item_type;
	const char *control_preference_name;
	NautilusPreferencesItemControlAction control_action;
	const EnumerationEntry *enumeration_values;
	const char *constrained_integer_values;
	const char *constrained_integer_labels;
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
 *    This field needs to be non NULL for items other than 
 *    NAUTILUS_PREFERENCE_ITEM_ENUM or 
 *    NAUTILUS_PREFERENCE_ITEM_SHORT_ENUM
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
 *       NAUTILUS_PREFERENCE_ITEM_SHOW - If the control preference is TRUE
 *                                       the show this item.
 *
 *       NAUTILUS_PREFERENCE_ITEM_HIDE - If the control preference is FALSE
 *                                       the hide this item.
 *
 * 7. enumeration_values
 *
 *    If the item_type in field 4 was NAUTILUS_PREFERENCE_ITEM_ENUM or
 *    NAUTILUS_PREFERENCE_ITEM_SHORT_ENUM, then this field should be 
 *    a pointer to a NULL terminated array of EnumerationEntry.
 *
 * 8. constrained_integer_values
 *
 *    If the item_type in field 4 was NAUTILUS_PREFERENCE_ITEM_CONSTRAINED_INTEGER,
 *    then this field should be a string of comma delimeted integers to be
 *    tokenized and used as the list of possible integers.
 *
 * 9. constrained_integer_labels
 *
 *    If the item_type in field 4 was NAUTILUS_PREFERENCE_ITEM_CONSTRAINED_INTEGER,
 *    then this field should be a string of comma delimeted labels corresponding to
 *    the integers in field 8.
 *
 */
static PreferenceDialogItem appearance_items[] = {
	{ N_("Smoother Graphics"),
	  NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
	  N_("Use smoother (but slower) graphics"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ N_("Fonts"),
	  NAUTILUS_PREFERENCES_DEFAULT_FONT,
	  N_("Use this font in Sidebar and some dialog boxes:"),
	  NAUTILUS_PREFERENCE_ITEM_FONT,
	  NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
	  NAUTILUS_PREFERENCE_ITEM_HIDE
	},
	{ N_("Fonts"),
	  NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT,
	  N_("Use this font in Sidebar and some dialog boxes:"),
	  NAUTILUS_PREFERENCE_ITEM_SMOOTH_FONT,
	  NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
	  NAUTILUS_PREFERENCE_ITEM_SHOW
	},
	{ NULL, NULL, NULL, 0, NULL, 0 }
};

static PreferenceDialogItem windows_and_desktop_items[] = {
	{ N_("Desktop"),
	  NAUTILUS_PREFERENCES_SHOW_DESKTOP,
	  N_("Use Nautilus to draw the desktop"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ N_("Opening New Windows"),
	  NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
	  N_("Open each file or folder in a separate window"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ N_("Opening New Windows"),
	  NAUTILUS_PREFERENCES_START_WITH_TOOLBAR,
	  N_("Display toolbar in new windows"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ N_("Opening New Windows"),
	  NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR,
	  N_("Display location bar in new windows"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ N_("Opening New Windows"),
	  NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR,
	  N_("Display status bar in new windows"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ N_("Opening New Windows"),
	  NAUTILUS_PREFERENCES_START_WITH_SIDEBAR,
	  N_("Display sidebar in new windows"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ N_("Trash Behavior"),
	  NAUTILUS_PREFERENCES_CONFIRM_TRASH,
	  N_("Ask before emptying the Trash or deleting files"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ N_("Trash Behavior"),
	  NAUTILUS_PREFERENCES_ENABLE_DELETE,
	  N_("Include a Delete command that bypasses Trash"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	/* FIXME: This group clearly doesn't belong in Windows &
	 * Desktop, but there's no obviously-better place for it and
	 * it probably doesn't deserve a pane of its own.
	 */
	{ N_("Keyboard Shortcuts"),
	  NAUTILUS_PREFERENCES_USE_EMACS_SHORTCUTS,
	  N_("Use Emacs-style keyboard shortcuts in text fields"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ NULL, NULL, NULL, 0, NULL, 0 }
};

static EnumerationEntry click_policy_enumeration[] = {
	{ N_("single"),
	  N_("Activate items with a single click"),
	  NAUTILUS_CLICK_POLICY_SINGLE,
	},
	{ N_("double"),
	  N_("Activate items with a double click"),
	  NAUTILUS_CLICK_POLICY_DOUBLE,
	},
	{ NULL, NULL, 0 }
};

static EnumerationEntry executable_text_activation_enumeration[] = {
	{ N_("launch"),
	  N_("Execute files when they are clicked"),
	  NAUTILUS_EXECUTABLE_TEXT_LAUNCH,
	},
	{ N_("display"),
	  N_("Display files when they are clicked"),
	  NAUTILUS_EXECUTABLE_TEXT_DISPLAY,
	},
	{ N_("ask"),
	  N_("Ask each time"),
	  NAUTILUS_EXECUTABLE_TEXT_ASK,
	},
	{ NULL, NULL, 0 }
};

static PreferenceDialogItem directory_views_items[] = {
	{ N_("Fonts"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_FONT,
	  N_("Use this font in Icon view:"),
	  NAUTILUS_PREFERENCE_ITEM_FONT,
	  NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
	  NAUTILUS_PREFERENCE_ITEM_HIDE
	},
	{ N_("Fonts"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_SMOOTH_FONT,
	  N_("Use this font in Icon view:"),
	  NAUTILUS_PREFERENCE_ITEM_SMOOTH_FONT,
	  NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
	  NAUTILUS_PREFERENCE_ITEM_SHOW
	},
	{ N_("Fonts"),
	  NAUTILUS_PREFERENCES_ICON_VIEW_STANDARD_FONT_SIZE,
	  N_("Use this font size in Icon view at 100% zoom"),
	  NAUTILUS_PREFERENCE_ITEM_CONSTRAINED_INTEGER,
	  NULL,
	  0,
	  NULL,
	  "8,10,12,14,16,18,20,22,24",
	  "8,10,12,14,16,18,20,22,24",
	},
	{ N_("Fonts"),
	  NAUTILUS_PREFERENCES_LIST_VIEW_FONT,
	  N_("Use this font in List view:"),
	  NAUTILUS_PREFERENCE_ITEM_FONT,
	  NULL,
	  0
	},
	{ N_("Fonts"),
	  NAUTILUS_PREFERENCES_LIST_VIEW_STANDARD_FONT_SIZE,
	  N_("Use this font size in List view at 100% zoom"),
	  NAUTILUS_PREFERENCE_ITEM_CONSTRAINED_INTEGER,
	  NULL,
	  0,
	  NULL,
	  "8,10,12,14,16,18,20,22,24",
	  "8,10,12,14,16,18,20,22,24",
	},
	{ N_("Click Behavior"),
	  NAUTILUS_PREFERENCES_CLICK_POLICY,
	  N_("Click Behavior"),
	  NAUTILUS_PREFERENCE_ITEM_ENUM,
	  NULL,
	  0,
	  click_policy_enumeration
	},
	{ N_("Executable Text Files"),
	  NAUTILUS_PREFERENCES_EXECUTABLE_TEXT_ACTIVATION,
	  N_("Executable Text Files"),
	  NAUTILUS_PREFERENCE_ITEM_ENUM,
	  NULL,
	  0,
	  executable_text_activation_enumeration
	},
	{ N_("Show/Hide Options"),
	  NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
	  N_("Show hidden files (file names start with \".\")"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ N_("Show/Hide Options"),
	  NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
	  N_("Show backup files (file names end with \"~\")"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ N_("Show/Hide Options"),
	  NAUTILUS_PREFERENCES_SHOW_SPECIAL_FLAGS,
	  N_("Show special flags in Properties window"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ N_("Sorting Order"),
	  NAUTILUS_PREFERENCES_SORT_DIRECTORIES_FIRST,
	  N_("Always list folders before files"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ NULL, NULL, NULL, 0, NULL, 0 }
};

static PreferenceDialogItem navigation_items[] = {
	{ N_("Home"),
	  NAUTILUS_PREFERENCES_HOME_URI,
	  N_("Location:"),
	  NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING,
	  NULL,
	  0
	},
	{ N_("HTTP Proxy Settings"),
	  USE_PROXY_KEY,
	  N_("Use HTTP Proxy"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ N_("HTTP Proxy Settings"),
	  PROXY_HOST_KEY,
	  N_("Location:"),
	  NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING,
	  NULL,
	  0
	},
	{ N_("HTTP Proxy Settings"),
	  PROXY_PORT_KEY,
	  N_("Port:"),
	  NAUTILUS_PREFERENCE_ITEM_EDITABLE_INTEGER,
	  NULL,
	  0
	},
	{ N_("Built-in Bookmarks"),
	  NAUTILUS_PREFERENCES_HIDE_BUILT_IN_BOOKMARKS,
	  N_("Don't include the built-in bookmarks in the Bookmarks menu"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ NULL, NULL, NULL, 0, NULL, 0 }
};

static PreferenceDialogItem tradeoffs_items[] = {
	{ N_("Show Text in Icons"),
	  NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS,
	  N_(""),
	  NAUTILUS_PREFERENCE_ITEM_SHORT_ENUM,
	  NULL,
	  0,
	  speed_tradeoff_enumeration
	},
	{ N_("Show Count of Items in Folders"),
	  NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
	  N_(""),
	  NAUTILUS_PREFERENCE_ITEM_SHORT_ENUM,
	  NULL,
	  0,
	  speed_tradeoff_enumeration
	},
	{ N_("Show Thumbnails for Image Files"),
	  NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
	  N_(""),
	  NAUTILUS_PREFERENCE_ITEM_SHORT_ENUM,
	  NULL,
	  0,
	  speed_tradeoff_enumeration
	},
	{ N_("Show Thumbnails for Image Files"),
	  NAUTILUS_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT,
	  N_("Don't make thumbnails for files larger than:"),
	  NAUTILUS_PREFERENCE_ITEM_CONSTRAINED_INTEGER,
	  NULL,
	  0,
	  NULL,
	  "102400,512000,1048576,3145728,5242880,10485760,104857600",
	  "100 K,500 K,1 MB,3 MB,5 MB,10 MB,100 MB",
	},
	{ N_("Preview Sound Files"),
	  NAUTILUS_PREFERENCES_PREVIEW_SOUND,
	  N_(""),
	  NAUTILUS_PREFERENCE_ITEM_SHORT_ENUM,
	  NULL,
	  0,
	  speed_tradeoff_enumeration
	},

	/* FIXME bugzilla.eazel.com 2560: This title phrase needs improvement. */
	{ N_("Make Folder Appearance Details Public"),
	  NAUTILUS_PREFERENCES_USE_PUBLIC_METADATA,
	  N_(""),
	  NAUTILUS_PREFERENCE_ITEM_SHORT_ENUM,
	  NULL,
	  0,
	  speed_tradeoff_enumeration
	},
	{ NULL, NULL, NULL, 0, NULL, 0 }
};

static GtkWidget *
global_preferences_create_dialog (void)
{
	GtkWidget *prefs_dialog;
	NautilusPreferencesBox *preference_box;
	GtkWidget *directory_views_pane;
	GtkWidget *appearance_pane;

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


	/* Appearance */
	appearance_pane = global_preferences_populate_pane (preference_box,
							    _("Appearance"),
							    appearance_items);
	
 	nautilus_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
						       global_preferences_pane_update_callback,
						       appearance_pane,
						       GTK_OBJECT (prefs_dialog));
	

	/* Windows & Desktop */
	global_preferences_populate_pane (preference_box,
					  _("Windows & Desktop"),
					  windows_and_desktop_items);

	/* Folder Views */
	directory_views_pane = global_preferences_populate_pane (preference_box,
								 _("Icon & List Views"),
								 directory_views_items);
	
	nautilus_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
						       global_preferences_pane_update_callback,
						       directory_views_pane,
						       GTK_OBJECT (prefs_dialog));
	
	/* Sidebar Panels */
	global_preferences_create_sidebar_panels_pane (preference_box);

	/* Search */
	global_preferences_create_search_pane (preference_box);

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

/* Update the sensitivity of the search pane when the medusa blocked state changes */
static void
global_preferences_medusa_blocked_changed_callback (gpointer callback_data)
{
	gboolean medusa_blocked;
	
	g_return_if_fail (GTK_IS_WIDGET (callback_data));

	medusa_blocked = nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_MEDUSA_BLOCKED);

	gtk_widget_set_sensitive (GTK_WIDGET (callback_data), !medusa_blocked);
}

static EnumerationEntry search_bar_type_enumeration[] = {
	{ N_("search by text"),
	  N_("Search for files by file name only"),
	  NAUTILUS_SIMPLE_SEARCH_BAR,
	},
	{ N_("search by text and properties"),
	  N_("Search for files by file name and file properties"),
	  NAUTILUS_COMPLEX_SEARCH_BAR,
	},
	{ NULL, NULL, 0 }
};

static PreferenceDialogItem search_items[] = {
	{ N_("Search Complexity Options"),
	  NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
	  N_("search type to do by default"),
	  NAUTILUS_PREFERENCE_ITEM_ENUM,
	  NULL,
	  0,
	  search_bar_type_enumeration
	},
	{ N_("Fast Search"),
	  NAUTILUS_PREFERENCES_USE_FAST_SEARCH,
	  N_("Enable fast search (indexes your hard drive)"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ N_("Search Engines"),
	  NAUTILUS_PREFERENCES_SEARCH_WEB_URI,
	  N_("Search Engine Location"),
	  NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING,
	  NULL,
	  0
	},
	{ NULL, NULL, NULL, 0, NULL, 0 }
};

static void
global_preferences_create_search_pane (NautilusPreferencesBox *preference_box)
{
	GtkWidget *fast_search_group;
	GtkWidget *search_pane;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_BOX (preference_box));

	search_pane = global_preferences_populate_pane (preference_box,
							_("Search"),
							search_items);

	/* Setup callbacks so that we can update the sensitivity of
	 * the search pane when the medusa blocked state changes
	 */
	fast_search_group = nautilus_preferences_pane_find_group (NAUTILUS_PREFERENCES_PANE (search_pane),
								  _("Fast Search"));

	g_assert (NAUTILUS_IS_PREFERENCES_GROUP (fast_search_group));

	nautilus_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_MEDUSA_BLOCKED,
						       global_preferences_medusa_blocked_changed_callback,
						       fast_search_group,
						       GTK_OBJECT (fast_search_group));
	global_preferences_medusa_blocked_changed_callback (fast_search_group);
}

static PreferenceDialogItem sidebar_items[] = {
	{ N_("Tree"),
	  NAUTILUS_PREFERENCES_TREE_SHOW_ONLY_DIRECTORIES,
	  N_("Show only folders (no files) in the tree"),
	  NAUTILUS_PREFERENCE_ITEM_BOOLEAN,
	  NULL,
	  0
	},
	{ NULL, NULL, NULL, 0, NULL, 0 }
};

static void
global_preferences_create_sidebar_panels_pane (NautilusPreferencesBox *preference_box)
{
	char *preference_key;
	GList *view_identifiers;
	GList *p;
	NautilusViewIdentifier *identifier;
	char *description;
	
	GtkWidget *sidebar_pane;

	g_return_if_fail (NAUTILUS_IS_PREFERENCES_BOX (preference_box));

	/* Sidebar Panels - dynamic part */
	sidebar_pane = nautilus_preferences_box_add_pane (preference_box, _("Sidebar Panels"));

	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (sidebar_pane), _("Tabs"));

	view_identifiers = global_preferences_get_sidebar_panel_view_identifiers ();
	
	view_identifiers = g_list_sort (view_identifiers, compare_view_identifiers);
	
	for (p = view_identifiers; p != NULL; p = p->next) {
		identifier = (NautilusViewIdentifier *) (p->data);
		
		preference_key = global_preferences_make_sidebar_panel_key (identifier->iid);
		
		g_assert (preference_key != NULL);
		
		description = g_strdup_printf (_("Display %s tab in sidebar"), identifier->name);
		nautilus_preferences_set_description (preference_key, description);
		g_free (description);
		
		nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (sidebar_pane),
								 0,
								 preference_key,
								 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
		
		g_free (preference_key);
		
	}
	nautilus_view_identifier_list_free (view_identifiers);


	/* Sidebar Panels - non dynamic parts */
	global_preferences_populate_pane (preference_box,
					  _("Sidebar Panels"),
					  sidebar_items);
}

/* Update a pane as a result of a preference change.
 * For example, we have 2 font picker items, but we only show
 * one depending on the value of the SMOOTH_GRAPHICS preference.
 */
static void
global_preferences_pane_update_callback (gpointer callback_data)
{
	g_return_if_fail (NAUTILUS_IS_PREFERENCES_PANE (callback_data));

	nautilus_preferences_pane_update (NAUTILUS_PREFERENCES_PANE (callback_data));
}

/* Make a query to find out what sidebar panels are available. */
static GList *
global_preferences_get_sidebar_panel_view_identifiers (void)
{
	CORBA_Environment ev;
	const char *query;
        OAF_ServerInfoList *oaf_result;
	guint i;
	NautilusViewIdentifier *id;
	GList *view_identifiers;

	CORBA_exception_init (&ev);

	query = "nautilus:sidebar_panel_name.defined() AND repo_ids.has ('IDL:Bonobo/Control:1.0')";

	oaf_result = oaf_query (query, NULL, &ev);
		
	view_identifiers = NULL;

        if (ev._major == CORBA_NO_EXCEPTION && oaf_result != NULL) {
		for (i = 0; i < oaf_result->_length; i++) {
			id = nautilus_view_identifier_new_from_sidebar_panel
				(&oaf_result->_buffer[i]);
			view_identifiers = g_list_prepend (view_identifiers, id);
		}
		view_identifiers = g_list_reverse (view_identifiers);
	} 

	if (oaf_result != NULL) {
		CORBA_free (oaf_result);
	}
	
	CORBA_exception_free (&ev);

	return view_identifiers;
}

GList *
nautilus_global_preferences_get_enabled_sidebar_panel_view_identifiers (void)
{
	GList *enabled_view_identifiers;
 	GList *disabled_view_identifiers;
        
	enabled_view_identifiers = global_preferences_get_sidebar_panel_view_identifiers ();

	enabled_view_identifiers = nautilus_g_list_partition (enabled_view_identifiers,
							      global_preferences_is_sidebar_panel_enabled_cover,
							      NULL,
							      &disabled_view_identifiers);
	
	nautilus_view_identifier_list_free (disabled_view_identifiers);
	
        return enabled_view_identifiers;
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

static struct 
{
	const char *name;
	gboolean novice_default;
	gboolean intermediate_default;
	gboolean advanced_default;
	int visible_user_level;
} known_sidebar_panels[] =
{
	{ "OAFIID:nautilus_notes_view:7f04c3cb-df79-4b9a-a577-38b19ccd4185",       TRUE,  TRUE,  TRUE,  NAUTILUS_USER_LEVEL_INTERMEDIATE},
	{ "OAFIID:hyperbola_navigation_tree:57542ce0-71ff-442d-a764-462c92514234", TRUE,  TRUE,  TRUE,  NAUTILUS_USER_LEVEL_INTERMEDIATE },
	{ "OAFIID:nautilus_history_view:a7a85bdd-2ecf-4bc1-be7c-ed328a29aacb",     TRUE,  TRUE,  TRUE,  NAUTILUS_USER_LEVEL_INTERMEDIATE },
	{ "OAFIID:nautilus_tree_view:2d826a6e-1669-4a45-94b8-23d65d22802d",        FALSE, TRUE,  TRUE,  NAUTILUS_USER_LEVEL_INTERMEDIATE },
};

static void
global_preferences_install_sidebar_panel_defaults (void)
{
	guint i;
	
	/* Install the user level on/off defaults for known sidebar panels */
	for (i = 0; i < NAUTILUS_N_ELEMENTS (known_sidebar_panels); i++) {
		char *key = global_preferences_make_sidebar_panel_key (known_sidebar_panels[i].name);
		
		nautilus_preferences_default_set_boolean (key,
							  NAUTILUS_USER_LEVEL_NOVICE,
							  known_sidebar_panels[i].novice_default);
		nautilus_preferences_default_set_boolean (key,
							  NAUTILUS_USER_LEVEL_INTERMEDIATE,
							  known_sidebar_panels[i].intermediate_default);
		nautilus_preferences_default_set_boolean (key,
							  NAUTILUS_USER_LEVEL_ADVANCED,
							  known_sidebar_panels[i].advanced_default);

		nautilus_preferences_set_visible_user_level (key,
							     known_sidebar_panels[i].visible_user_level);

		g_free (key);
	}
}

static char *
global_preferences_make_sidebar_panel_key (const char *panel_iid)
{
	g_return_val_if_fail (panel_iid != NULL, NULL);

	return g_strdup_printf ("%s/%s", NAUTILUS_PREFERENCES_SIDEBAR_PANELS_NAMESPACE, panel_iid);
}

static gboolean
global_preferences_is_sidebar_panel_enabled (NautilusViewIdentifier *panel_identifier)
{
	gboolean enabled;
        gchar  *key;
	
	g_return_val_if_fail (panel_identifier != NULL, FALSE);
	g_return_val_if_fail (panel_identifier->iid != NULL, FALSE);
	
	key = global_preferences_make_sidebar_panel_key (panel_identifier->iid);
	g_return_val_if_fail (key != NULL, FALSE);
        enabled = nautilus_preferences_get_boolean (key);
        g_free (key);

        return enabled;
}

static gboolean
global_preferences_is_sidebar_panel_enabled_cover (gpointer data, gpointer callback_data)
{
	return global_preferences_is_sidebar_panel_enabled (data);
}

static void
global_preferences_install_home_location_defaults (void)
{
	char *default_novice_home_uri;
	char *default_intermediate_home_uri;
	char *user_main_directory;		
	
	user_main_directory = nautilus_get_user_main_directory ();
	
	default_novice_home_uri = gnome_vfs_get_uri_from_local_path (user_main_directory);
	default_intermediate_home_uri = gnome_vfs_get_uri_from_local_path (g_get_home_dir ());
	
	nautilus_preferences_default_set_string (NAUTILUS_PREFERENCES_HOME_URI,
						 NAUTILUS_USER_LEVEL_NOVICE,
						 default_novice_home_uri);
	
	nautilus_preferences_default_set_string (NAUTILUS_PREFERENCES_HOME_URI,
						 NAUTILUS_USER_LEVEL_INTERMEDIATE,
						 default_intermediate_home_uri);
	
	nautilus_preferences_set_visible_user_level (NAUTILUS_PREFERENCES_HOME_URI,
						     NAUTILUS_USER_LEVEL_INTERMEDIATE);
	g_free (user_main_directory);
	g_free (default_novice_home_uri);
	g_free (default_intermediate_home_uri);
}

static void
global_preferences_install_font_defaults (void)
{
	char *default_smooth_font;
	const char *default_font;

	default_font = nautilus_dumb_down_for_multi_byte_locale_hack () ? "fixed" : "helvetica";
	default_smooth_font = nautilus_font_manager_get_default_font ();

	/* Icon view fonts */
	nautilus_preferences_default_set_string (NAUTILUS_PREFERENCES_ICON_VIEW_FONT,
						 NAUTILUS_USER_LEVEL_NOVICE,
						 default_font);
	nautilus_preferences_default_set_string (NAUTILUS_PREFERENCES_ICON_VIEW_SMOOTH_FONT,
						 NAUTILUS_USER_LEVEL_NOVICE,
						 default_smooth_font);
	nautilus_preferences_default_set_integer (NAUTILUS_PREFERENCES_ICON_VIEW_STANDARD_FONT_SIZE,
 						  NAUTILUS_USER_LEVEL_NOVICE,
						  12);

	/* List view fonts */
	nautilus_preferences_default_set_string (NAUTILUS_PREFERENCES_LIST_VIEW_FONT,
						 NAUTILUS_USER_LEVEL_NOVICE,
						 default_font);
	nautilus_preferences_default_set_integer (NAUTILUS_PREFERENCES_LIST_VIEW_STANDARD_FONT_SIZE,
 						  NAUTILUS_USER_LEVEL_NOVICE,
						  12);

	/* Default fonts */
	nautilus_preferences_default_set_string (NAUTILUS_PREFERENCES_DEFAULT_FONT,
						 NAUTILUS_USER_LEVEL_NOVICE,
						 default_font);

	nautilus_preferences_default_set_string (NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT,
						 NAUTILUS_USER_LEVEL_NOVICE,
						 default_smooth_font);

	nautilus_preferences_default_set_integer (NAUTILUS_PREFERENCES_DEFAULT_FONT_SIZE,
						  NAUTILUS_USER_LEVEL_NOVICE,
						  12);

	g_free (default_smooth_font);
}

static void
global_preferences_use_fast_search_changed_callback (gpointer callback_data)
{
	gboolean use_fast_search;
	gboolean services_are_blocked;
	NautilusCronStatus cron_status;

	if (global_prefs_dialog) {
		use_fast_search = nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_USE_FAST_SEARCH);
		
		nautilus_medusa_enable_services (use_fast_search);
		services_are_blocked = nautilus_medusa_blocked ();
		if (use_fast_search && !services_are_blocked) {
			cron_status = nautilus_medusa_check_cron_is_enabled ();
			switch (cron_status) {
			case NAUTILUS_CRON_STATUS_OFF:
				nautilus_show_info_dialog_with_details (_("Indexing is turned on, enabling the "
									  "fast search feature. However, indexing "
									  "currently can't be performed because "
									  "the program crond, which does "
									  "nightly tasks on your computer, "
									  "is turned off. To make sure fast "
									  "searches can be done, turn crond on."),
									_("Files May Not Be Indexed"),
									_("If you are running Linux, you can log "
									  "in as root and type these commands "
									  "to start cron:\n\n"
									  "/sbin/chkconfig --level 345 crond on\n"
									  "/etc/rc.d/init.d/cron start\n"),
									NULL);
				
				break;
			case NAUTILUS_CRON_STATUS_UNKNOWN:
				nautilus_show_info_dialog_with_details (_("Indexing is turned on, enabling the "
									  "fast search feature. However, indexing "
									  "may not be performed because the "
									  "program crond, which does nightly "
									  "tasks on your computer, may be turned "
									  "off. To make sure fast searches can be "
									  "done, check to make sure that crond "
									  "is turned on.\n\n"),
									_("Files May Not Be Indexed"),
									_("If you are running Linux, you can log "
									  "in as root and type these commands "
									  "to start cron:\n\n"
									  "/sbin/chkconfig --level 345 crond on\n"
									  "/etc/rc.d/init.d/cron start\n"),
									NULL);
				break;
			default:
				break;
			}
		}
	}
}
	
static void 
global_preferences_medusa_state_changed_callback (gpointer callback_data)
{
	nautilus_preferences_set_boolean (NAUTILUS_PREFERENCES_USE_FAST_SEARCH,
					  nautilus_medusa_services_have_been_enabled_by_user ());
	
	nautilus_preferences_set_boolean (NAUTILUS_PREFERENCES_MEDUSA_BLOCKED,
					  nautilus_medusa_blocked ());

}

static void
global_preferences_install_medusa_defaults (void)
{
	gboolean medusa_blocked;
	gboolean use_fast_search;

	medusa_blocked = nautilus_medusa_blocked ();
	use_fast_search = nautilus_medusa_services_have_been_enabled_by_user ();

	/* This one controls the sensitivity */
	nautilus_preferences_default_set_boolean (NAUTILUS_PREFERENCES_MEDUSA_BLOCKED,
						  NAUTILUS_USER_LEVEL_NOVICE,
						  medusa_blocked);

	/* This is the one that appears in the preferences dialog */
	nautilus_preferences_default_set_boolean (NAUTILUS_PREFERENCES_USE_FAST_SEARCH,
						  NAUTILUS_USER_LEVEL_NOVICE,
						  use_fast_search);

	nautilus_preferences_set_visible_user_level (NAUTILUS_PREFERENCES_USE_FAST_SEARCH,
						     NAUTILUS_USER_LEVEL_INTERMEDIATE);

	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_USE_FAST_SEARCH,
					   global_preferences_use_fast_search_changed_callback,
					   NULL);

	nautilus_medusa_add_system_state_changed_callback (global_preferences_medusa_state_changed_callback,
							   NULL);
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
	NautilusStringList *group_names;
	guint i;
	int group_index;
	guint start_group_index;
	const EnumerationEntry *enumeration_values;

	g_return_val_if_fail (NAUTILUS_IS_PREFERENCES_BOX (preference_box), NULL);
	g_return_val_if_fail (pane_name != NULL, NULL);
	g_return_val_if_fail (preference_dialog_item != NULL, NULL);

	/* Create the pane if needed */
	pane = nautilus_preferences_box_find_pane (preference_box, pane_name);
	if (pane == NULL) {
		pane = nautilus_preferences_box_add_pane (preference_box, pane_name);
	}

	group_names = nautilus_string_list_new (TRUE);

	start_group_index = nautilus_preferences_pane_get_num_groups (NAUTILUS_PREFERENCES_PANE (pane));

	for (i = 0; preference_dialog_item[i].group_name != NULL; i++) {
		if (!nautilus_string_list_contains (group_names, preference_dialog_item[i].group_name)) {
			nautilus_string_list_insert (group_names, preference_dialog_item[i].group_name);
			nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (pane),
							     preference_dialog_item[i].group_name);
		}
	}

	for (i = 0; preference_dialog_item[i].group_name != NULL; i++) {
		group_index = start_group_index + 
			nautilus_string_list_get_index_for_string (group_names, preference_dialog_item[i].group_name);

		nautilus_preferences_set_description (preference_dialog_item[i].preference_name,
						      preference_dialog_item[i].preference_description);

		enumeration_values = preference_dialog_item[i].enumeration_values;
		while (enumeration_values != NULL && enumeration_values->stored_value != NULL) {
			nautilus_preferences_enumeration_insert (preference_dialog_item[i].preference_name,
								 enumeration_values->stored_value,
								 enumeration_values->display_value,
								 enumeration_values->value);
			enumeration_values++;
		}

		item = nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (pane),
									group_index,
									preference_dialog_item[i].preference_name,
									preference_dialog_item[i].item_type);

		if (preference_dialog_item[i].item_type == NAUTILUS_PREFERENCE_ITEM_CONSTRAINED_INTEGER) {
			g_assert (nautilus_strlen (preference_dialog_item[i].constrained_integer_values) > 0);
			g_assert (nautilus_strlen (preference_dialog_item[i].constrained_integer_labels) > 0);
			nautilus_preferences_item_set_constrained_integer_values (
				NAUTILUS_PREFERENCES_ITEM (item),
				preference_dialog_item[i].constrained_integer_values,
				preference_dialog_item[i].constrained_integer_labels);
		}

		if (preference_dialog_item[i].control_preference_name != NULL) {
			nautilus_preferences_item_set_control_preference (NAUTILUS_PREFERENCES_ITEM (item),
									  preference_dialog_item[i].control_preference_name);
			nautilus_preferences_item_set_control_action (NAUTILUS_PREFERENCES_ITEM (item),
								      preference_dialog_item[i].control_action);
		}
	}

	nautilus_string_list_free (group_names);

	return pane;
}

/*
 * Public functions
 */
void
nautilus_global_preferences_show_dialog (void)
{
	GtkWidget *dialog = global_preferences_get_dialog ();

	nautilus_gtk_window_present (GTK_WINDOW (dialog));
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

static NautilusScalableFont *
global_preferences_get_smooth_font (const char *smooth_font_file_name)
{
	NautilusScalableFont *smooth_font;

	smooth_font = (smooth_font_file_name != NULL && g_file_exists (smooth_font_file_name)) ?
		nautilus_scalable_font_new (smooth_font_file_name) :
		nautilus_scalable_font_get_default_font ();
	
	g_assert (NAUTILUS_IS_SCALABLE_FONT (smooth_font));
	return smooth_font;
}

static NautilusScalableFont *
global_preferences_get_smooth_bold_font (const char *file_name)
{
	NautilusScalableFont *plain_font;
	NautilusScalableFont *bold_font;

	plain_font = global_preferences_get_smooth_font (file_name);
	g_assert (NAUTILUS_IS_SCALABLE_FONT (plain_font));

	bold_font = nautilus_scalable_font_make_bold (plain_font);

	if (bold_font == NULL) {
		bold_font = plain_font;
	} else {
		gtk_object_unref (GTK_OBJECT (plain_font));
	}

	g_assert (NAUTILUS_IS_SCALABLE_FONT (bold_font));
	return bold_font;
}

/**
 * nautilus_global_preferences_get_icon_view_smooth_font
 *
 * Return value: The user's smooth font for icon file names.  Need to 
 *               unref the returned GtkObject when done with it.
 */
NautilusScalableFont *
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
NautilusScalableFont *
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
NautilusScalableFont *
nautilus_global_preferences_get_default_smooth_bold_font (void)
{
	return global_preferences_get_smooth_bold_font (default_smooth_font_auto_value);
}

void
nautilus_global_preferences_initialize (void)
{
	static gboolean initialized = FALSE;

	if (initialized) {
		return;
	}

	initialized = TRUE;

	/* Install defaults */
	global_preferences_install_defaults ();

	/* Set up storage for values accessed in this file */
	nautilus_preferences_add_auto_string (NAUTILUS_PREFERENCES_ICON_VIEW_SMOOTH_FONT,
					      &icon_view_smooth_font_auto_value);
	nautilus_preferences_add_auto_string (NAUTILUS_PREFERENCES_DEFAULT_SMOOTH_FONT,
					      &default_smooth_font_auto_value);
}
