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

/* Constants */
#define STRING_ARRAY_DEFAULT_TOKENS_DELIMETER ","
#define PREFERENCES_SORT_ORDER_MANUALLY 100

/* Path for gnome-vfs preferences */
static const char *EXTRA_MONITOR_PATHS[] = { "/desktop/gnome/background",
					     "/desktop/gnome/lockdown",
					     NULL };

/* An enumeration used for installing type specific preferences defaults. */
typedef enum
{
	PREFERENCE_BOOLEAN = 1,
	PREFERENCE_INTEGER,
	PREFERENCE_STRING,
	PREFERENCE_STRING_ARRAY
} PreferenceType;

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

/*
 * Public functions
 */
char *
nautilus_global_preferences_get_default_folder_viewer_preference_as_iid (void)
{
	int preference_value;
	const char *viewer_iid;

	preference_value =
		g_settings_get_enum (nautilus_preferences, NAUTILUS_PREFERENCES_DEFAULT_FOLDER_VIEWER);

	if (preference_value == NAUTILUS_DEFAULT_FOLDER_VIEWER_LIST_VIEW) {
		viewer_iid = NAUTILUS_LIST_VIEW_IID;
	} else if (preference_value == NAUTILUS_DEFAULT_FOLDER_VIEWER_COMPACT_VIEW) {
		viewer_iid = NAUTILUS_COMPACT_VIEW_IID;
	} else {
		viewer_iid = NAUTILUS_ICON_VIEW_IID;
	}

	return g_strdup (viewer_iid);
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

	nautilus_preferences = g_settings_new("org.gnome.nautilus.preferences");
	nautilus_media_preferences = g_settings_new("org.gnome.media-handling");
	nautilus_window_state = g_settings_new("org.gnome.nautilus.window-state");
	nautilus_icon_view_preferences = g_settings_new("org.gnome.nautilus.icon-view");
	nautilus_list_view_preferences = g_settings_new("org.gnome.nautilus.list-view");
	nautilus_compact_view_preferences = g_settings_new("org.gnome.nautilus.compact-view");
	nautilus_desktop_preferences = g_settings_new("org.gnome.nautilus.desktop");
	nautilus_tree_sidebar_preferences = g_settings_new("org.gnome.nautilus.sidebar-panels.tree");

	nautilus_gconf_client = gconf_client_get_default ();

	/* Add monitors for any other GConf paths we have keys in */
	for (i=0; EXTRA_MONITOR_PATHS[i] != NULL; i++) {
		gconf_client_add_dir (nautilus_gconf_client,
				      EXTRA_MONITOR_PATHS[i],
				      GCONF_CLIENT_PRELOAD_ONELEVEL,
				      NULL);
	}

	/* Preload everything in a big batch */
	eel_gconf_preload_cache ("/apps/nautilus/preferences",
				 GCONF_CLIENT_PRELOAD_ONELEVEL);
	eel_gconf_preload_cache ("/desktop/gnome/file_views",
				 GCONF_CLIENT_PRELOAD_ONELEVEL);
	eel_gconf_preload_cache ("/desktop/gnome/background",
				 GCONF_CLIENT_PRELOAD_ONELEVEL);
	eel_gconf_preload_cache ("/desktop/gnome/lockdown",
				 GCONF_CLIENT_PRELOAD_ONELEVEL);

	/* These are always needed for the desktop */
	eel_gconf_preload_cache ("/apps/nautilus/desktop",
				 GCONF_CLIENT_PRELOAD_ONELEVEL);
	eel_gconf_preload_cache ("/apps/nautilus/icon_view",
				 GCONF_CLIENT_PRELOAD_ONELEVEL);
	eel_gconf_preload_cache ("/apps/nautilus/desktop-metadata",
				 GCONF_CLIENT_PRELOAD_RECURSIVE);
}
