/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-prefs-dialog.c - Implementation for preferences dialog.

   Copyright (C) 1999, 2000 Eazel, Inc.

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

#include <nautilus-widgets/nautilus-preferences-group.h>
#include <nautilus-widgets/nautilus-preferences-item.h>
#include <nautilus-widgets/nautilus-preferences-dialog.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>

/* Constants */
#define GLOBAL_PREFERENCES_DIALOG_TITLE _("Nautilus Preferences")

/* User level */
#define NAUTILUS_PREFERENCES_USER_LEVEL				"/nautilus/preferences/user_level"

enum
{
	NAUTILUS_USER_LEVEL_NOVICE,
	NAUTILUS_USER_LEVEL_INTERMEDIATE,
	NAUTILUS_USER_LEVEL_HACKER
};

/* Private stuff */
static GtkWidget *global_preferences_create_dialog   (void);
static GtkWidget *global_preferences_get_dialog      (void);
static void       global_preferences_register_for_ui (NautilusPreferences *preferences);
static void       user_level_changed_callback        (NautilusPreferences *preferences,
						      const char          *name,
						      gpointer             user_data);

/*
 * Private stuff
 */
static GtkWidget *
global_preferences_create_dialog (void)
{
	GtkWidget		*prefs_dialog;
	NautilusPreferencesBox	*preference_box;
	NautilusPreferences	*preferences;
	GtkWidget		*user_level_pane;
	GtkWidget		*window_options_pane;
	GtkWidget		*meta_view_pane;
	GtkWidget		*icon_view_pane;

	preferences = nautilus_preferences_get_global_preferences ();

	/*
	 * In the soon to come star trek future, the following widgetry
	 * might be either fetched from a glade file or generated from 
	 * an xml file.
	 */
	prefs_dialog = nautilus_preferences_dialog_new (GLOBAL_PREFERENCES_DIALOG_TITLE);

	/* Create a preference box */
	preference_box = NAUTILUS_PREFERENCES_BOX (nautilus_preferences_dialog_get_prefs_box
						   (NAUTILUS_PREFERENCES_DIALOG (prefs_dialog)));

	/*
	 * User level pane
	 */
	user_level_pane = nautilus_preferences_box_add_pane (preference_box,
							     "User Level",
							     "User Level Something");
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (user_level_pane), "User Level");
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (user_level_pane),
							 0,
							 preferences,
							 NAUTILUS_PREFERENCES_USER_LEVEL,
							 NAUTILUS_PREFERENCE_ITEM_ENUM);
	/*
	 * Window options pane
	 */
	window_options_pane = nautilus_preferences_box_add_pane (preference_box,
								 "Window Options",
								 "Window Options Something");
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (window_options_pane), "Basic window options");
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (window_options_pane),
							 0,
							 preferences,
							 NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (window_options_pane),
							 0,
							 preferences,
							 NAUTILUS_PREFERENCES_WINDOW_SEARCH_EXISTING,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	
	/*
	 * Meta view pane
	 */
	meta_view_pane = nautilus_preferences_box_add_pane (preference_box,
							    "Meta Views",
							    "Meta Views Something");
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (meta_view_pane), "Meta Views");
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (meta_view_pane),
							 0,
							 preferences,
							 NAUTILUS_PREFERENCES_META_VIEWS_SHOW_ANNOTATIONS,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (meta_view_pane),
							 0,
							 preferences,
							 NAUTILUS_PREFERENCES_META_VIEWS_SHOW_ANNOTATIONS,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (meta_view_pane),
							 0,
							 preferences,
							 NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_CONTENTS,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (meta_view_pane),
							 0,
							 preferences,
							 NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_INDEX,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (meta_view_pane),
							 0,
							 preferences,
							 NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_SEARCH,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (meta_view_pane),
							 0,
							 preferences,
							 NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HISTORY,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (meta_view_pane),
							 0,
							 preferences,
							 NAUTILUS_PREFERENCES_META_VIEWS_SHOW_WEB_SEARCH,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	/*
	 * Clicking pane
	 */
	icon_view_pane = nautilus_preferences_box_add_pane (preference_box,
							    "Click Policy",
							    "Click Policy something");
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (icon_view_pane), "Click Policy");
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (icon_view_pane),
							 0,
							 preferences,
							 NAUTILUS_PREFERENCES_CLICK_POLICY,
							 NAUTILUS_PREFERENCE_ITEM_ENUM);

	return prefs_dialog;
}

/* 
 * Presummably, the following would be registered
 * only if the component was present.  Once we
 * have smarter activation, that will be case.
 * 
 * For now turn on all the ones we know about.
 */

const NautilusStringList *
nautilus_global_preferences_get_meta_view_iids (void)
{
	static NautilusStringList * meta_view_names = NULL;
	
	if (!meta_view_names)
	{
		meta_view_names = nautilus_string_list_new ();

		nautilus_string_list_insert (meta_view_names, "ntl_notes_view");
		nautilus_string_list_insert (meta_view_names, "hyperbola_navigation_tree");
		nautilus_string_list_insert (meta_view_names, "hyperbola_navigation_index");
		nautilus_string_list_insert (meta_view_names, "hyperbola_navigation_search");
		nautilus_string_list_insert (meta_view_names, "ntl_history_view");
		nautilus_string_list_insert (meta_view_names, "ntl_websearch_view");
	}

	return meta_view_names;
}

static GtkWidget *
global_preferences_get_dialog (void)
{
	static GtkWidget * global_prefs_dialog = NULL;

	NautilusPreferences * global_prefs = nautilus_preferences_get_global_preferences ();

	if (!global_prefs)
	{
		g_warning ("something went terribly wrong with implicit prefs initialization");

		return NULL;
	}

	if (!global_prefs_dialog)
	{
		global_prefs_dialog = global_preferences_create_dialog ();
	}

	return global_prefs_dialog;
}

static void
global_preferences_register_for_ui (NautilusPreferences *preferences)
{
	static gboolean preference_for_ui_registered = FALSE;

	g_assert (preferences != NULL);

	if (preference_for_ui_registered)
		return;

	preference_for_ui_registered = TRUE;

	preferences = nautilus_preferences_get_global_preferences ();

	g_assert (preferences != NULL);

	/*
	 * In the soon to come star trek future, the following information
	 * will be fetched using the latest xml techniques.
	 */

	/* Window create new */
	nautilus_preferences_set_info (preferences,
				       NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
				       "Create new window for each new page",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE);
	
	/* Window seatch existing */
	nautilus_preferences_set_info (preferences,
				       NAUTILUS_PREFERENCES_WINDOW_SEARCH_EXISTING,
				       "Do not open more than one window with the same page",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE);

	/* Click activation type */
	nautilus_preferences_set_info (preferences,
				       NAUTILUS_PREFERENCES_CLICK_POLICY,
				       "Click policy",
				       NAUTILUS_PREFERENCE_ENUM,
				       (gconstpointer) NAUTILUS_CLICK_POLICY_SINGLE);

	nautilus_preferences_enum_add_entry (preferences,
					     NAUTILUS_PREFERENCES_CLICK_POLICY,
					     "single",
					     "Single Click",
					     NAUTILUS_CLICK_POLICY_SINGLE);

	nautilus_preferences_enum_add_entry (preferences,
					     NAUTILUS_PREFERENCES_CLICK_POLICY,
					     "double",
					     "Double Click",
					     NAUTILUS_CLICK_POLICY_DOUBLE);

	/* User level */
	nautilus_preferences_set_info (preferences,
				       NAUTILUS_PREFERENCES_USER_LEVEL,
				       "User Level",
				       NAUTILUS_PREFERENCE_ENUM,
				       (gconstpointer) NAUTILUS_USER_LEVEL_HACKER);
	
	
	nautilus_preferences_enum_add_entry (preferences,
					     NAUTILUS_PREFERENCES_USER_LEVEL,
					     "novice",
					     "Novice",
					     NAUTILUS_USER_LEVEL_NOVICE);

	nautilus_preferences_enum_add_entry (preferences,
					     NAUTILUS_PREFERENCES_USER_LEVEL,
					     "intermediate",
					     "Intermediate",
					     NAUTILUS_USER_LEVEL_INTERMEDIATE);

	nautilus_preferences_enum_add_entry (preferences,
					     NAUTILUS_PREFERENCES_USER_LEVEL,
					     "hacker",
					     "Hacker",
					     NAUTILUS_USER_LEVEL_HACKER);

	/* Meta views */
	nautilus_preferences_set_info (preferences,
				       NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HISTORY,
				       "History View",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) TRUE);
	
	nautilus_preferences_set_info (preferences,
				       NAUTILUS_PREFERENCES_META_VIEWS_SHOW_WEB_SEARCH,
				       "Web Search View",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) TRUE);

	nautilus_preferences_set_info (preferences,
				       NAUTILUS_PREFERENCES_META_VIEWS_SHOW_ANNOTATIONS,
				       "Annotations",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) TRUE);

	nautilus_preferences_set_info (preferences,
				       NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_CONTENTS,
				       "Help Contents",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) TRUE);

	nautilus_preferences_set_info (preferences,
				       NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_INDEX,
				       "Help Index",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE);

	nautilus_preferences_set_info (preferences,
				       NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_SEARCH,
				       "Help Search",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE);
}

static void
user_level_changed_callback (NautilusPreferences	*preferences,
			     const char			*name,
			     gpointer			user_data)
{
	gint		user_level;
	gboolean	show_hidden_files = FALSE;
	GString		*home_uri_string;

	const char	*user_top_directory;
	
	g_assert (NAUTILUS_IS_PREFERENCES (preferences));
	g_assert (name != NULL);
	g_assert (strcmp (name, NAUTILUS_PREFERENCES_USER_LEVEL) == 0);

	home_uri_string = g_string_new ("file://");

	user_level = nautilus_preferences_get_enum (preferences,
						    NAUTILUS_PREFERENCES_USER_LEVEL,
						    NAUTILUS_USER_LEVEL_HACKER);

	/* Set some preferences according to the user level */
	switch (user_level) {
	case NAUTILUS_USER_LEVEL_NOVICE:
		user_top_directory = nautilus_user_top_directory ();
		
		g_string_append (home_uri_string, user_top_directory);

		show_hidden_files = FALSE;
		break;

	case NAUTILUS_USER_LEVEL_INTERMEDIATE: 
		g_string_append (home_uri_string, g_get_home_dir ());
		show_hidden_files = FALSE;
		break;
		
	case NAUTILUS_USER_LEVEL_HACKER:
		g_string_append (home_uri_string, g_get_home_dir ());
		show_hidden_files = TRUE;
		break;
	}

	nautilus_preferences_set_boolean (preferences,
					  NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
					  show_hidden_files);

	nautilus_preferences_set (preferences,
				  NAUTILUS_PREFERENCES_HOME_URI,
				  home_uri_string->str);

	g_string_free (home_uri_string, TRUE);
}

/*
 * Public functions
 */
void
nautilus_global_preferences_show_dialog (void)
{
	GtkWidget * global_prefs_dialog = global_preferences_get_dialog ();

	gtk_widget_show (global_prefs_dialog);
}

void
nautilus_global_preferences_shutdown (void)
{
	GtkWidget * global_prefs_dialog;
	GtkObject * global_prefs;

	/* FIXME: Unfortunately the following triggers a bogus gtk_object_unref */
	return;

	global_prefs_dialog = global_preferences_get_dialog ();

	global_prefs = GTK_OBJECT (nautilus_preferences_get_global_preferences ());

	gtk_widget_unref (global_prefs_dialog);

	gtk_object_unref (global_prefs);
}

void
nautilus_global_preferences_startup (void)
{
	NautilusPreferences *preferences;
	
	preferences = nautilus_preferences_get_global_preferences ();

	g_assert (preferences != NULL);

	global_preferences_register_for_ui (preferences);

	/* Keep track of user level changes */
	nautilus_preferences_add_enum_callback (preferences,
						NAUTILUS_PREFERENCES_USER_LEVEL,
						user_level_changed_callback,
						NULL);
}

