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
#include <liboaf/liboaf.h>
#include "nautilus-global-preferences.h"

#include <gtk/gtkbox.h>

#include <nautilus-widgets/nautilus-preferences-group.h>
#include <nautilus-widgets/nautilus-preferences-item.h>
#include <nautilus-widgets/nautilus-preferences-dialog.h>
#include <nautilus-widgets/nautilus-user-level-manager.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-view-identifier.h>

/* Constants */
#define GLOBAL_PREFERENCES_DIALOG_TITLE _("Nautilus Preferences")

/* Private stuff */
static GtkWidget *global_preferences_create_dialog                      (void);
static GtkWidget *global_preferences_get_dialog                         (void);
static void       global_preferences_register_for_ui                    (void);
static char *     global_preferences_get_sidebar_panel_key              (const char             *panel_iid);
static gboolean   global_preferences_is_sidebar_panel_enabled           (NautilusViewIdentifier *panel_identifier,
									 gpointer                ignore);
static GList *    global_preferences_get_sidebar_panel_view_identifiers (void);
static gboolean   global_preferences_close_dialog_callback              (GtkWidget               *dialog,
									 gpointer                user_data);

/*
 * Private stuff
 */
static GtkWidget *
global_preferences_create_dialog (void)
{
	GtkWidget		*prefs_dialog;
	NautilusPreferencesBox	*preference_box;
	GtkWidget		*directory_views_pane;
	GtkWidget		*sidebar_panels_pane;
	GtkWidget		*appearance_pane;

	/*
	 * In the soon to come star trek future, the following widgetry
	 * might be either fetched from a glade file or generated from 
	 * an xml file.
	 */
	prefs_dialog = nautilus_preferences_dialog_new (GLOBAL_PREFERENCES_DIALOG_TITLE);

	gtk_signal_connect (GTK_OBJECT (prefs_dialog),
			    "close",
			    GTK_SIGNAL_FUNC (global_preferences_close_dialog_callback),
			    NULL);

	/* Create a preference box */
	preference_box = NAUTILUS_PREFERENCES_BOX (nautilus_preferences_dialog_get_prefs_box
						   (NAUTILUS_PREFERENCES_DIALOG (prefs_dialog)));


	/*
	 * Directory Views pane
	 */
	directory_views_pane = nautilus_preferences_box_add_pane (preference_box,
								 "Directory Views",
								 "Directory Views Something");
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane), "Window Behavior");
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane),
							 0,
							 NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane), "Click Behavior");
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane),
							 1,
							 NAUTILUS_PREFERENCES_CLICK_POLICY,
							 NAUTILUS_PREFERENCE_ITEM_ENUM);


	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane), "Remote Views");
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane),
							 2,
							 NAUTILUS_PREFERENCES_SHOW_TEXT_IN_REMOTE_ICONS,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	/*
	 * Sidebar panels pane
	 */
	sidebar_panels_pane = nautilus_preferences_box_add_pane (preference_box,
								 "Sidebar Panels",
								 "Sidebar Panels Description");
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (sidebar_panels_pane), 
					     "Choose which panels should appear in the sidebar");
	
	{
		char *preference_key;
		GList *view_identifiers;
		GList *p;
		NautilusViewIdentifier *identifier;

		view_identifiers = global_preferences_get_sidebar_panel_view_identifiers ();

		for (p = view_identifiers; p != NULL; p = p->next) {
			identifier = (NautilusViewIdentifier *) (p->data);
			
			preference_key = global_preferences_get_sidebar_panel_key (identifier->iid);

			g_assert (preference_key != NULL);

			nautilus_preferences_pane_add_item_to_nth_group 
				(NAUTILUS_PREFERENCES_PANE (sidebar_panels_pane),
				 0,
				 preference_key,
				 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	
			g_free (preference_key);

		}
	
		nautilus_view_identifier_list_free (view_identifiers);
	}


	/*
	 * Appearance
	 */
	appearance_pane = nautilus_preferences_box_add_pane (preference_box,
							     "Appearance",
							     "Appearance Options");
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (appearance_pane), "Smoother Graphics");
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (appearance_pane),
							 0,
							 NAUTILUS_PREFERENCES_ANTI_ALIASED_CANVAS,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);


	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (appearance_pane), "Toolbar Icons");
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (appearance_pane),
							 1,
							 NAUTILUS_PREFERENCES_EAZEL_TOOLBAR_ICONS,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (appearance_pane), "Fonts");
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (appearance_pane),
							 2,
							 NAUTILUS_PREFERENCES_DIRECTORY_VIEW_FONT_FAMILY,
							 NAUTILUS_PREFERENCE_ITEM_FONT_FAMILY);

	/* all done */
	
	return prefs_dialog;
}


/* 
 * Presummably, the following would be registered
 * only if the component was present.  Once we
 * have smarter activation, that will be case.
 * 
 * For now turn on all the ones we know about.
 */

static GList *
global_preferences_get_sidebar_panel_view_identifiers (void)
{
	CORBA_Environment ev;
	const char *query;
        OAF_ServerInfoList *oaf_result;
	int i;
	NautilusViewIdentifier *id;
	GList *view_identifiers;

	CORBA_exception_init (&ev);

	query = "nautilus:sidebar_panel_name.defined() AND repo_ids.has ('IDL:Bonobo/Control:1.0')";

	oaf_result = oaf_query (query, NULL /* FIXME: alphabetize by name in the future? */, &ev);
		
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
                                                      (NautilusGPredicateFunc) global_preferences_is_sidebar_panel_enabled,
                                                      NULL,
                                                      &disabled_view_identifiers);
	
        nautilus_view_identifier_list_free (disabled_view_identifiers);

        return enabled_view_identifiers;
}

GList *
nautilus_global_preferences_get_disabled_sidebar_panel_view_identifiers (void)
{
	GList *enabled_view_identifiers;
	GList *disabled_view_identifiers;
        
	enabled_view_identifiers = global_preferences_get_sidebar_panel_view_identifiers ();
        
        enabled_view_identifiers = nautilus_g_list_partition (enabled_view_identifiers,
							      (NautilusGPredicateFunc) global_preferences_is_sidebar_panel_enabled,
							      NULL,
							      &disabled_view_identifiers);
	
        nautilus_view_identifier_list_free (enabled_view_identifiers);
	
        return disabled_view_identifiers;
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
global_preferences_register_sidebar_panels_preferences_for_ui (void)
{
	GList *view_identifiers;
	GList *p;
	NautilusViewIdentifier *identifier;
	char *preference_key;

	view_identifiers = global_preferences_get_sidebar_panel_view_identifiers ();

	for (p = view_identifiers; p != NULL; p = p->next) {
		identifier = (NautilusViewIdentifier *) (p->data);
		
		preference_key = global_preferences_get_sidebar_panel_key (identifier->iid);

		g_assert (preference_key != NULL);
 		
		nautilus_preferences_set_info (preference_key,
					       identifier->name,
					       NAUTILUS_PREFERENCE_BOOLEAN,
					       (gconstpointer) TRUE);
		
		g_free (preference_key);
	}

	nautilus_view_identifier_list_free (view_identifiers);
}

static char *
global_preferences_get_sidebar_panel_key (const char *panel_iid)
{
	g_return_val_if_fail (panel_iid != NULL, NULL);

	return g_strdup_printf ("%s/%s", NAUTILUS_PREFERENCES_SIDEBAR_PANELS_NAMESPACE, panel_iid);
}

static gboolean
global_preferences_is_sidebar_panel_enabled (NautilusViewIdentifier *panel_identifier,
					     gpointer ignore)
{
	gboolean enabled;
        gchar	 *key;

	g_return_val_if_fail (panel_identifier != NULL, FALSE);
	g_return_val_if_fail (panel_identifier->iid != NULL, FALSE);

	key = global_preferences_get_sidebar_panel_key (panel_identifier->iid);
	
	g_assert (key != NULL);

        enabled = nautilus_preferences_get_boolean (key, FALSE);

        g_free (key);

        return enabled;
}

static void
global_preferences_register_for_ui (void)
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
				       "Open each item in a new window",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE);
	
	/* Click activation type */
	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_CLICK_POLICY,
				       "Click policy",
				       NAUTILUS_PREFERENCE_ENUM,
				       (gconstpointer) NAUTILUS_CLICK_POLICY_SINGLE);

	nautilus_preferences_enum_add_entry (NAUTILUS_PREFERENCES_CLICK_POLICY,
					     "single",
					     "Activate items with a single click",
					     NAUTILUS_CLICK_POLICY_SINGLE);

	nautilus_preferences_enum_add_entry (NAUTILUS_PREFERENCES_CLICK_POLICY,
					     "double",
					     "Activate items with a double click",
					     NAUTILUS_CLICK_POLICY_DOUBLE);

	/* Remote views */
	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_SHOW_TEXT_IN_REMOTE_ICONS,
			               "Display text in icons even for remote text files",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE);
	
	/* Sidebar panels */
	global_preferences_register_sidebar_panels_preferences_for_ui ();

	/* Appearance options */
	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_ANTI_ALIASED_CANVAS,
				       "Use smoother (but slower) graphics",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE);

	/* Directory View */
	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_DIRECTORY_VIEW_FONT_FAMILY,
				       "Font familiy used to display file names",
				       NAUTILUS_PREFERENCE_STRING,
				       (gconstpointer) "helvetica");
	
	/* toolbar icons */
	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_EAZEL_TOOLBAR_ICONS,
				       "Use Eazel's toolbar icons",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE);
	
	/* Miscellaneous */
	nautilus_preferences_set_info (NAUTILUS_PREFERENCES_SHOW_REAL_FILE_NAME,
				       "Show entire file file",
				       NAUTILUS_PREFERENCE_BOOLEAN,
				       (gconstpointer) FALSE);	
}

static gboolean
global_preferences_close_dialog_callback (GtkWidget   *dialog,
					  gpointer    user_data)
{
	nautilus_global_preferences_hide_dialog ();

	return TRUE;
}

#define USER_LEVEL_NOVICE	0
#define USER_LEVEL_INTERMEDIATE 1
#define USER_LEVEL_HACKER	2

static void
user_level_changed_callback (GtkObject	*user_level_manager,
			     gpointer	user_data)
{
	int			new_user_level;
	char			*home_uri_string;

	gboolean		show_hidden_files = FALSE;
	gboolean		use_real_home = TRUE;
	gboolean		show_real_file_name = FALSE;
	gboolean		can_add_content = TRUE;
	
	const char		*user_main_directory;

	new_user_level = nautilus_user_level_manager_get_user_level ();

	/* Set some preferences according to the user level */
	switch (new_user_level) {
	case USER_LEVEL_NOVICE:
		show_hidden_files = FALSE;
		use_real_home = FALSE;
		show_real_file_name = FALSE;
		can_add_content = FALSE;
		break;

	case USER_LEVEL_INTERMEDIATE: 
		show_hidden_files = FALSE;
		use_real_home = TRUE;
		show_real_file_name = FALSE;
		can_add_content = TRUE;
		break;
		
	case USER_LEVEL_HACKER:
	default:
		show_hidden_files = TRUE;
		use_real_home = TRUE;
		show_real_file_name = TRUE;
		can_add_content = TRUE;
		break;
	}

	nautilus_preferences_set_boolean (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
					  show_hidden_files);
	
	nautilus_preferences_set_boolean (NAUTILUS_PREFERENCES_SHOW_REAL_FILE_NAME,
					  show_real_file_name);

	nautilus_preferences_set_boolean (NAUTILUS_PREFERENCES_CAN_ADD_CONTENT,
					  can_add_content);
	
	/* FIXME bugzilla.eazel.com 715: This call needs to be spanked to conform.  Should return a strduped string */
	user_main_directory = nautilus_get_user_main_directory ();
	
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
nautilus_global_preferences_hide_dialog (void)
{
	GtkWidget * global_prefs_dialog = global_preferences_get_dialog ();

	gtk_widget_hide (global_prefs_dialog);
}

void
nautilus_global_preferences_set_dialog_title (const char *title)
{
	GtkWidget *global_prefs_dialog;
	g_return_if_fail (title != NULL);
	
	global_prefs_dialog = global_preferences_get_dialog ();

	gtk_window_set_title (GTK_WINDOW (global_prefs_dialog), title);
}

void
nautilus_global_preferences_shutdown (void)
{
	/* Free the dialog first, cause it has refs to preferences */
	GtkWidget * global_prefs_dialog = global_preferences_get_dialog ();
	gtk_widget_destroy (global_prefs_dialog);

	gtk_signal_disconnect_by_func (GTK_OBJECT (nautilus_user_level_manager_get ()),
				       user_level_changed_callback,
				       NULL);

	/* Now free the preferences tables and stuff */
	nautilus_preferences_shutdown ();
}

void
nautilus_global_preferences_startup (int argc, char **argv)
{
	gboolean result;

	/* Initialize preferences */
	result = nautilus_preferences_initialize (argc, argv);

	/* FIXME bugzilla.eazel.com 672: 
	 * Need error reporting if this fails instead of a core dump.
	 */
	g_assert (result);
	
	global_preferences_register_for_ui ();
	
	/* Register to find out about user level changes */
	gtk_signal_connect (GTK_OBJECT (nautilus_user_level_manager_get ()),
			    "user_level_changed",
			    user_level_changed_callback,
			    NULL);
	
	/* Invoke the callback once to make sure stuff is properly setup */
	user_level_changed_callback (NULL, NULL);
}

