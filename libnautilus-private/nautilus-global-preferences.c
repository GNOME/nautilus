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
	/* Start at something other than zero - which is reserved as the unspecified default value. */
	NAUTILUS_USER_LEVEL_NOVICE = 100,
	NAUTILUS_USER_LEVEL_INTERMEDIATE,
	NAUTILUS_USER_LEVEL_HACKER
};

/* Private stuff */
static GtkWidget *global_preferences_create_dialog   (void);
static GtkWidget *global_preferences_get_dialog      (void);
static void       global_preferences_register_for_ui (void);
static void       user_level_changed_callback        (gpointer             user_data);

/*
 * Private stuff
 */
static GtkWidget *
global_preferences_create_dialog (void)
{
	GtkWidget		*prefs_dialog;
	NautilusPreferencesBox	*preference_box;
	GtkWidget		*user_level_pane;
	GtkWidget		*window_options_pane;
	GtkWidget		*meta_view_pane;
	GtkWidget		*icon_view_pane;

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
							 NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (window_options_pane),
							 0,
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
							 NAUTILUS_PREFERENCES_META_VIEWS_SHOW_ANNOTATIONS,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (meta_view_pane),
							 0,
							 NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_CONTENTS,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (meta_view_pane),
							 0,
							 NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_INDEX,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (meta_view_pane),
							 0,
							 NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_SEARCH,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (meta_view_pane),
							 0,
							 NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HISTORY,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (meta_view_pane),
							 0,
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

		nautilus_string_list_insert (meta_view_names, 
					     "OAFIID:ntl_notes_view:7f04c3cb-df79-4b9a-a577-38b19ccd4185");
		nautilus_string_list_insert (meta_view_names, 
					     "OAFIID:hyperbola_navigation_tree:57542ce0-71ff-442d-a764-462c92514234");
		nautilus_string_list_insert (meta_view_names, 
					     "OAFIID:hyperbola_navigation_index:0bafadc7-09f1-4f10-8c8e-dad53124fc49");
		nautilus_string_list_insert (meta_view_names, 
					     "OAFIID:hyperbola_navigation_search:89b2f3b8-4f09-49c8-9a7b-ccb14d034813");
		nautilus_string_list_insert (meta_view_names, 
					     "OAFIID:ntl_history_view:a7a85bdd-2ecf-4bc1-be7c-ed328a29aacb");
		nautilus_string_list_insert (meta_view_names, 
					     "OAFIID:ntl_websearch_view:8216e1e4-6b01-4a28-82d9-5df30ed7d044");
	}

	return meta_view_names;
}

static GtkWidget *
global_preferences_get_dialog (void)
{
	static GtkWidget * global_prefs_dialog = NULL;

	g_assert (nautilus_preferences_is_initialized ());

	if (!global_prefs_dialog)
	{
		global_prefs_dialog = global_preferences_create_dialog ();
	}

	return global_prefs_dialog;
}

static void
global_preferences_register_for_ui ()
{
	static gboolean preference_for_ui_registered = FALSE;

	g_assert (nautilus_preferences_is_initialized ());

	if (preference_for_ui_registered)
		return;

	preference_for_ui_registered = TRUE;


	/*
	 * In the soon to come star trek future, the following information
	 * will be fetched using the latest xml techniques.
	 */

	/* Window create new */
	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
				       "Create new window for each new page",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE);
	
	/* Window seatch existing */
	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_WINDOW_SEARCH_EXISTING,
				       "Do not open more than one window with the same page",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE);

	/* Click activation type */
	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_CLICK_POLICY,
				       "Click policy",
				       NAUTILUS_PREFERENCE_ENUM,
				       (gconstpointer) NAUTILUS_CLICK_POLICY_SINGLE);

	nautilus_preferences_enum_add_entry (NAUTILUS_PREFERENCES_CLICK_POLICY,
					     "single",
					     "Single Click",
					     NAUTILUS_CLICK_POLICY_SINGLE);

	nautilus_preferences_enum_add_entry (NAUTILUS_PREFERENCES_CLICK_POLICY,
					     "double",
					     "Double Click",
					     NAUTILUS_CLICK_POLICY_DOUBLE);

	/* User level */
	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_USER_LEVEL,
				       "User Level",
				       NAUTILUS_PREFERENCE_ENUM,
				       (gconstpointer) NAUTILUS_USER_LEVEL_HACKER);
	
	
	nautilus_preferences_enum_add_entry (NAUTILUS_PREFERENCES_USER_LEVEL,
					     "novice",
					     "Novice",
					     NAUTILUS_USER_LEVEL_NOVICE);

	nautilus_preferences_enum_add_entry (NAUTILUS_PREFERENCES_USER_LEVEL,
					     "intermediate",
					     "Intermediate",
					     NAUTILUS_USER_LEVEL_INTERMEDIATE);

	nautilus_preferences_enum_add_entry (NAUTILUS_PREFERENCES_USER_LEVEL,
					     "hacker",
					     "Hacker",
					     NAUTILUS_USER_LEVEL_HACKER);

	/* Meta views */
	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HISTORY,
				       "History View",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) TRUE);
	
	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_META_VIEWS_SHOW_WEB_SEARCH,
				       "Web Search View",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE);

	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_META_VIEWS_SHOW_ANNOTATIONS,
				       "Annotations",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) TRUE);

	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_CONTENTS,
				       "Help Contents",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) TRUE);

	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_INDEX,
				       "Help Index",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE);

	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_META_VIEWS_SHOW_HELP_SEARCH,
				       "Help Search",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE);
	/* miscellaneous */
	
	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_SHOW_REAL_FILE_NAME,
				       "Show entire file file",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE);	
}

static void
user_level_changed_callback (gpointer user_data)
{
	gint			user_level;
	char			*home_uri_string;

	gboolean		show_hidden_files = FALSE;
	gboolean		use_real_home = TRUE;
	gboolean		show_real_file_name = FALSE;
	
	const char		*user_main_directory;

	user_level = nautilus_preferences_get_enum (NAUTILUS_PREFERENCES_USER_LEVEL,
						    NAUTILUS_USER_LEVEL_HACKER);

	/* Set some preferences according to the user level */
	switch (user_level) {
	case NAUTILUS_USER_LEVEL_NOVICE:
		show_hidden_files = FALSE;
		use_real_home = FALSE;
		show_real_file_name = FALSE;
		break;

	case NAUTILUS_USER_LEVEL_INTERMEDIATE: 
		show_hidden_files = FALSE;
		use_real_home = TRUE;
		show_real_file_name = FALSE;
		break;
		
	case NAUTILUS_USER_LEVEL_HACKER:
	default:
		show_hidden_files = TRUE;
		use_real_home = TRUE;
		show_real_file_name = TRUE;
		break;
	}

	nautilus_preferences_set_boolean (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
					  show_hidden_files);
	
	nautilus_preferences_set_boolean (NAUTILUS_PREFERENCES_SHOW_REAL_FILE_NAME,
					  show_real_file_name);

	/* FIXME: This call needs to be spanked to conform.  Should return a strduped string */
	user_main_directory = nautilus_user_main_directory ();
	
	if (use_real_home)
		home_uri_string = g_strdup_printf ("file://%s", g_get_home_dir());
	else
		home_uri_string = g_strdup_printf ("file://%s", user_main_directory);

	g_assert (home_uri_string != NULL);
	
	nautilus_preferences_set (NAUTILUS_PREFERENCES_HOME_URI,
				  home_uri_string);

	g_free (home_uri_string);
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
	/* Free the dialog first, cause it has refs to preferences */
	GtkWidget * global_prefs_dialog = global_preferences_get_dialog ();
	gtk_widget_destroy (global_prefs_dialog);

	/* Now free the preferences tables and stuff */
	nautilus_preferences_shutdown ();
}

void
nautilus_global_preferences_startup (void)
{
	global_preferences_register_for_ui ();

	/* Keep track of user level changes */
	nautilus_preferences_add_enum_callback (NAUTILUS_PREFERENCES_USER_LEVEL,
						user_level_changed_callback,
						NULL);

	/* Invoke the callback once to make sure stuff is properly setup */
	user_level_changed_callback (NULL);
}

