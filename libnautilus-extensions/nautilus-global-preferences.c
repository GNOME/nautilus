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

#include "nautilus-file-utilities.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-extensions.h"
#include "nautilus-preferences-dialog.h"
#include "nautilus-preferences-group.h"
#include "nautilus-preferences-item.h"
#include "nautilus-string.h"
#include "nautilus-user-level-manager.h"
#include "nautilus-view-identifier.h"
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <gtk/gtkbox.h>
#include <libgnome/gnome-i18n.h>
#include <liboaf/liboaf.h>

/* Constants */
#define GLOBAL_PREFERENCES_DIALOG_TITLE _("Nautilus Preferences")

/* default web search uri - this will soon be changed to point to our service */
#define DEFAULT_SEARCH_WEB_URI "http://www.google.com"

static const char PROXY_KEY[] = "/system/gnome-vfs/http-proxy";
static const char USE_PROXY_KEY[] = "/system/gnome-vfs/use-http-proxy";

/* Forward declarations */
static char *     global_preferences_get_sidebar_panel_key              (const char             *panel_iid);
static gboolean   global_preferences_is_sidebar_panel_enabled_cover     (gpointer                data,
									 gpointer                callback_data);
static GList *    global_preferences_get_sidebar_panel_view_identifiers (void);
static gboolean   global_preferences_close_dialog_callback              (GtkWidget              *dialog,
									 gpointer                user_data);
static void       global_preferences_register_boolean_with_defaults     (const char             *name,
									 const char             *description,
									 gboolean                novice_default,
									 gboolean                intermediate_default,
									 gboolean                hacker_default);

static GtkWidget *global_prefs_dialog = NULL;

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
	GtkWidget		*file_indexing_pane;
	GtkWidget		*tradeoffs_pane;
	GtkWidget		*navigation_pane;

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
								 _("Folder Views"),
								 _("Folder Views Settings"));
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane), _("Window Behavior"));
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane),
							 0,
							 NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane), _("Click Behavior"));
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane),
							 1,
							 NAUTILUS_PREFERENCES_CLICK_POLICY,
							 NAUTILUS_PREFERENCE_ITEM_ENUM);

	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane), _("Trash Behavior"));
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane),
							 2,
							 NAUTILUS_PREFERENCES_CONFIRM_TRASH,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane), _("Display"));
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane),
							 3,
							 NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane),
							 3,
							 NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (directory_views_pane),
							 3,
							 NAUTILUS_PREFERENCES_SHOW_SPECIAL_FLAGS,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	/*
	 * Sidebar panels pane
	 */
	sidebar_panels_pane = nautilus_preferences_box_add_pane (preference_box,
								 _("Sidebar Panels"),
								 _("Sidebar Panels Description"));
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (sidebar_panels_pane), 
					     _("Choose which panels should appear in the sidebar"));
	
	{
		char *preference_key;
		GList *view_identifiers;
		GList *p;
		NautilusViewIdentifier *identifier;

		view_identifiers = global_preferences_get_sidebar_panel_view_identifiers ();

		view_identifiers = g_list_sort (view_identifiers, compare_view_identifiers);

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
							     _("Appearance"),
							     _("Appearance Settings"));
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (appearance_pane), _("Smoother Graphics"));
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (appearance_pane),
							 0,
							 NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (appearance_pane), _("Fonts"));
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (appearance_pane),
							 1,
							 NAUTILUS_PREFERENCES_DIRECTORY_VIEW_FONT_FAMILY,
							 NAUTILUS_PREFERENCE_ITEM_FONT_FAMILY);

	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (appearance_pane), _("Views"));
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (appearance_pane),
							 2,
							 NAUTILUS_PREFERENCES_START_WITH_TOOL_BAR,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (appearance_pane),
							 2,
							 NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (appearance_pane),
							 2,
							 NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (appearance_pane),
							 2,
							 NAUTILUS_PREFERENCES_START_WITH_SIDEBAR,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	

	
	/*
	 * Tradeoffs
	 */
	tradeoffs_pane = nautilus_preferences_box_add_pane (preference_box,
							    _("Speed Tradeoffs"),
							    _("Speed Tradeoffs Settings"));

	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (tradeoffs_pane), _("Show Text in Icons"));
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (tradeoffs_pane),
							 0,
							 NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS,
							 NAUTILUS_PREFERENCE_ITEM_SHORT_ENUM);

	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (tradeoffs_pane), _("Show Thumbnails for Image Files"));
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (tradeoffs_pane),
							 1,
							 NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
							 NAUTILUS_PREFERENCE_ITEM_SHORT_ENUM);

	/* FIXME bugzilla.eazel.com 2560: This title phrase needs improvement. */
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (tradeoffs_pane), _("Make Folder Appearance Details Public"));
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (tradeoffs_pane),
							 2,
							 NAUTILUS_PREFERENCES_USE_PUBLIC_METADATA,
							 NAUTILUS_PREFERENCE_ITEM_SHORT_ENUM);

	/*
	 * Search Settings 
	 */

	file_indexing_pane = nautilus_preferences_box_add_pane (preference_box,
							     _("Search"),
							     _("Search Settings"));
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (file_indexing_pane),
					     _("Search Complexity Options"));
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (file_indexing_pane),
							 0,
							 NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
							 NAUTILUS_PREFERENCE_ITEM_ENUM);
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (file_indexing_pane),
					     _("Search Tradeoffs"));
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (file_indexing_pane),
							 1,
							 NAUTILUS_PREFERENCES_SEARCH_METHOD,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);
	
	
	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (file_indexing_pane),
					     _("Search Locations"));
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (file_indexing_pane),
							 2,
							 NAUTILUS_PREFERENCES_SEARCH_WEB_URI,
							 NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING);
					
	/*
	 * Navigation
	 */
	navigation_pane = nautilus_preferences_box_add_pane (preference_box,
							    _("Navigation"),
							    _("Navigation Settings"));

	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (navigation_pane), _("Home Location"));
	
	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (navigation_pane),
							 0,
							 NAUTILUS_PREFERENCES_HOME_URI,
							 NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING);

	nautilus_preferences_pane_add_group (NAUTILUS_PREFERENCES_PANE (navigation_pane), _("Proxy Settings"));

	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (navigation_pane),
							 1,
							 NAUTILUS_PREFERENCES_HTTP_USE_PROXY,
							 NAUTILUS_PREFERENCE_ITEM_BOOLEAN);

	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (navigation_pane),
							 1,
							 NAUTILUS_PREFERENCES_HTTP_PROXY,
							 NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING);

	nautilus_preferences_pane_add_item_to_nth_group (NAUTILUS_PREFERENCES_PANE (navigation_pane),
							 1,
							 NAUTILUS_PREFERENCES_HTTP_PROXY_PORT,
							 NAUTILUS_PREFERENCE_ITEM_EDITABLE_STRING);

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
        
        enabled_view_identifiers = nautilus_g_list_partition
		(enabled_view_identifiers,
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

/* FIXME bugzilla.eazel.com 1275: 
 * The actual defaults need to be user level specific.
 */
static const char *novice_default_sidebar_panel_iids[] =
{
	"OAFIID:nautilus_notes_view:7f04c3cb-df79-4b9a-a577-38b19ccd4185",
	"OAFIID:hyperbola_navigation_tree:57542ce0-71ff-442d-a764-462c92514234",
	"OAFIID:nautilus_history_view:a7a85bdd-2ecf-4bc1-be7c-ed328a29aacb",
	NULL
};

static const char *intermediate_default_sidebar_panel_iids[] =
{
	"OAFIID:nautilus_history_view:a7a85bdd-2ecf-4bc1-be7c-ed328a29aacb",
	"OAFIID:nautilus_notes_view:7f04c3cb-df79-4b9a-a577-38b19ccd4185",
	"OAFIID:nautilus_tree_view:2d826a6e-1669-4a45-94b8-23d65d22802d",
	"OAFIID:hyperbola_navigation_tree:57542ce0-71ff-442d-a764-462c92514234",
	NULL

};

static const char *hacker_default_sidebar_panel_iids[] = 
{
	"OAFIID:nautilus_notes_view:7f04c3cb-df79-4b9a-a577-38b19ccd4185",
	"OAFIID:nautilus_history_view:a7a85bdd-2ecf-4bc1-be7c-ed328a29aacb",
	"OAFIID:nautilus_tree_view:2d826a6e-1669-4a45-94b8-23d65d22802d",
	"OAFIID:hyperbola_navigation_tree:57542ce0-71ff-442d-a764-462c92514234",
	NULL
};

static const char **
get_default_sidebar_iids_for_user_level (guint user_level)
{

	switch (user_level) {
	case NAUTILUS_USER_LEVEL_NOVICE:
		return novice_default_sidebar_panel_iids;
		break;
	case NAUTILUS_USER_LEVEL_INTERMEDIATE:
		return intermediate_default_sidebar_panel_iids;
		break;
	case NAUTILUS_USER_LEVEL_HACKER:
		return hacker_default_sidebar_panel_iids;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	g_assert_not_reached ();
	return NULL;
}

static gboolean
sidebar_panel_iid_is_in_default_list (const char *iid,
				      guint user_level)
{
	guint i;
	const char **default_sidebar_panel_iids;

	g_return_val_if_fail (iid != NULL, FALSE);
	default_sidebar_panel_iids = get_default_sidebar_iids_for_user_level (user_level);
	
	for (i = 0; default_sidebar_panel_iids[i] != NULL; i++) {
		if (strcmp (iid, default_sidebar_panel_iids[i]) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static void
global_preferences_register_sidebar_panels (void)
{
	GList *view_identifiers;
	GList *p;

	view_identifiers = global_preferences_get_sidebar_panel_view_identifiers ();

	for (p = view_identifiers; p != NULL; p = p->next) {
		NautilusViewIdentifier	 *identifier;
		char			 *preference_key;
		gboolean		 novice_default_value;
		gboolean                 intermediate_default_value;
		gboolean                 hacker_default_value;

		identifier = (NautilusViewIdentifier *) (p->data);
		g_assert (identifier != NULL);
		
		preference_key = global_preferences_get_sidebar_panel_key (identifier->iid);
		g_assert (preference_key != NULL);
		
		novice_default_value = sidebar_panel_iid_is_in_default_list (identifier->iid,
									    NAUTILUS_USER_LEVEL_NOVICE);
		intermediate_default_value = sidebar_panel_iid_is_in_default_list (identifier->iid,
										   NAUTILUS_USER_LEVEL_INTERMEDIATE);
		hacker_default_value = sidebar_panel_iid_is_in_default_list (identifier->iid,
									     NAUTILUS_USER_LEVEL_HACKER);
		global_preferences_register_boolean_with_defaults (preference_key,
								   identifier->name,
								   novice_default_value,
								   intermediate_default_value,
								   hacker_default_value);

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
global_preferences_is_sidebar_panel_enabled (NautilusViewIdentifier *panel_identifier)
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

static gboolean
global_preferences_is_sidebar_panel_enabled_cover (gpointer data, gpointer callback_data)
{
	return global_preferences_is_sidebar_panel_enabled (data);
}

static void
global_preferences_register_with_defaults (const char			*name,
					   const char			*description,
					   NautilusPreferenceType	type,
					   gconstpointer		novice_default,
					   gconstpointer		intermediate_default,
					   gconstpointer		hacker_default)
{
	gconstpointer defaults[3];

	defaults[0] = novice_default;
	defaults[1] = intermediate_default;
	defaults[2] = hacker_default;

	nautilus_preference_set_info_by_name (name,
					      description,
					      type,
					      defaults,
					      3);
}

static void
global_preferences_register_boolean_with_defaults (const char	*name,
						   const char	*description,
						   gboolean	novice_default,
						   gboolean	intermediate_default,
						   gboolean	hacker_default)
{
	global_preferences_register_with_defaults (name,
						   description,
						   NAUTILUS_PREFERENCE_BOOLEAN,
						   (gconstpointer) GINT_TO_POINTER (novice_default),
						   (gconstpointer) GINT_TO_POINTER (intermediate_default),
						   (gconstpointer) GINT_TO_POINTER (hacker_default));
}

static void
global_preferences_register_string_with_defaults (const char	*name,
						  const char	*description,
						  const char    *novice_default,
						  const char    *intermediate_default,
						  const char    *hacker_default)
{
	global_preferences_register_with_defaults (name,
						   description,
						   NAUTILUS_PREFERENCE_STRING,
						   (gconstpointer) novice_default,
						   (gconstpointer) intermediate_default,
						   (gconstpointer) hacker_default);
}

static void
global_preferences_register_enum_with_defaults (const char	*name,
						const char	*description,
						int		novice_default,
						int		intermediate_default,
						int		hacker_default)
{
	global_preferences_register_with_defaults (name,
						   description,
						   NAUTILUS_PREFERENCE_ENUM,
						   (gconstpointer) GINT_TO_POINTER (novice_default),
						   (gconstpointer) GINT_TO_POINTER (intermediate_default),
						   (gconstpointer) GINT_TO_POINTER (hacker_default));
}

static void
global_preferences_register_speed_tradeoff_with_defaults (const char		     *name,
							  const char		     *description,
							  NautilusSpeedTradeoffValue  novice_default,
							  NautilusSpeedTradeoffValue  intermediate_default,
							  NautilusSpeedTradeoffValue  hacker_default)
{							  
	global_preferences_register_with_defaults (name,
						   description,
						   NAUTILUS_PREFERENCE_ENUM,
						   (gconstpointer) GINT_TO_POINTER (novice_default),
						   (gconstpointer) GINT_TO_POINTER (intermediate_default),
						   (gconstpointer) GINT_TO_POINTER (hacker_default));
	
	nautilus_preference_enum_add_entry_by_name (name,
						    _("always"),
						    _("Always"),
						    NAUTILUS_SPEED_TRADEOFF_ALWAYS);
	
	nautilus_preference_enum_add_entry_by_name (name,
						    _("local only"),
						    _("Local Files Only"),
						    NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY);
	
	nautilus_preference_enum_add_entry_by_name (name,
						    _("never"),
						    _("Never"),
						    NAUTILUS_SPEED_TRADEOFF_NEVER);
	
}							  


/* These three callbacks should go away in the future when we can handle system wide prefs */
static void
proxy_changed (gpointer user_data)
{
	char *proxy, *port, *new_proxy;
	gboolean use_proxy;

	/* Don't write to the system preference if use_proxy is FALSE */
	use_proxy = nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_HTTP_USE_PROXY, FALSE);
	if (!use_proxy) {
		return;
	}

	proxy = nautilus_preferences_get (NAUTILUS_PREFERENCES_HTTP_PROXY, "");
	port = nautilus_preferences_get (NAUTILUS_PREFERENCES_HTTP_PROXY_PORT, "");

	new_proxy = g_strdup_printf ("%s:%s", proxy, port);
	
	nautilus_preferences_set (PROXY_KEY, new_proxy);

	g_free (proxy);
	g_free (port);
	g_free (new_proxy);
}

static void
use_proxy_changed (gpointer user_data)
{
	gboolean use_proxy;
	GConfClient *gconf_client;

	use_proxy = nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_HTTP_USE_PROXY, FALSE);
	gconf_client = gconf_client_get_default ();

	if (gconf_client != NULL) {
		gconf_client_set_bool (gconf_client, USE_PROXY_KEY, use_proxy, NULL);
	}

	if (use_proxy) {
		proxy_changed (NULL);
	} else {
		/* Unset the system key */		
		if (gconf_client != NULL) {			
			gconf_client_unset (gconf_client, PROXY_KEY, NULL);
		}
	}
}

static void
register_proxy_preferences (void)
{
	gboolean use_proxy;
	char *proxy_string, *port, *proxy;
	GConfClient *gconf_client;
	
	use_proxy = FALSE;
	proxy = NULL;
	port = NULL;
	proxy_string = NULL;
	
	gconf_client = gconf_client_get_default ();
	if (gconf_client != NULL) {
		/* Get system level proxy values from gconf */
		use_proxy = gconf_client_get_bool (gconf_client, USE_PROXY_KEY, NULL);
		proxy_string = gconf_client_get_string (gconf_client, PROXY_KEY, NULL);
		if (proxy_string != NULL) {			
			port = strchr (proxy_string, ':');
			if (port != NULL) {
				proxy = g_strdup (proxy_string);
				proxy [port - proxy_string] = '\0';
				port++;								
			}
			
		}						
		gtk_object_unref (GTK_OBJECT (gconf_client));
	}
	
	if (proxy == NULL) {
		proxy = g_strdup ("");
	}
	if (port == NULL) {
		port = "8080";
	}

	nautilus_preferences_set_boolean (NAUTILUS_PREFERENCES_HTTP_USE_PROXY, use_proxy);
	nautilus_preferences_set (NAUTILUS_PREFERENCES_HTTP_PROXY, proxy);
	nautilus_preferences_set (NAUTILUS_PREFERENCES_HTTP_PROXY_PORT, port);
	
	global_preferences_register_boolean_with_defaults (NAUTILUS_PREFERENCES_HTTP_USE_PROXY,
							   _("Use HTTP Proxy"),
							   use_proxy,
							   use_proxy,
							   use_proxy);
	
	global_preferences_register_string_with_defaults (NAUTILUS_PREFERENCES_HTTP_PROXY,
							  _("HTTP Proxy"),
							  proxy,
							  proxy,
							  proxy);
	
	global_preferences_register_string_with_defaults (NAUTILUS_PREFERENCES_HTTP_PROXY_PORT,
							  _("HTTP Proxy Port"),
							  port,
							  port,
							  port);
	
	g_free (proxy);
	g_free (proxy_string);

	/* Add a callbacks to update the system setting with the new local setting. This should go away when the
	 * prefs mechanism can handle system wide prefs.
	 */
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_HTTP_USE_PROXY, use_proxy_changed, NULL);
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_HTTP_PROXY, proxy_changed, NULL);
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_HTTP_PROXY_PORT, proxy_changed, NULL);
}


/**
 * global_preferences_register
 *
 * Register preferences that meet one of the following criteria:
 *
 * 1) Need a 'nice' default
 * 2) Need a description (because the appear in the preferences dialog)
 */
static void
global_preferences_register (void)
{
	static gboolean preferences_registered = FALSE;

	if (preferences_registered) {
		return;
	}

	preferences_registered = TRUE;

	/*
	 * In the soon to come star trek future, the following information
	 * will be fetched using the latest xml techniques.
	 */


	/* 
	 * Non user level specific preferences:
	 */

	/* FIXME bugzilla.eazel.com 2654: 
	 * Cannot set default values for non user level preferecens.  Yes, this
	 * is bad.  The right way to fix this problem (and many other default
	 * value problems) is to use gconf schemas.  See the bug for info.
	 */

#if 0
	global_preferences_register_string_with_defaults (NAUTILUS_PREFERENCES_THEME,
							  _("current theme"),
							  "default",
							  "default",
							  "default");
#endif

	/* 
	 * User user level specific preferences:
	 */
	
	/* Window create new */
	global_preferences_register_boolean_with_defaults (NAUTILUS_PREFERENCES_WINDOW_ALWAYS_NEW,
							   _("Open each item in a new window"),
							   FALSE,
							   FALSE,
							   FALSE);
	
	/* Trash confirm */
	global_preferences_register_boolean_with_defaults (NAUTILUS_PREFERENCES_CONFIRM_TRASH,
							   _("Ask before deleting items from the trash"),
							   TRUE,
							   TRUE,
							   TRUE);
	
	/* Click activation type */
	global_preferences_register_enum_with_defaults (NAUTILUS_PREFERENCES_CLICK_POLICY,
							_("Click policy"),
							NAUTILUS_CLICK_POLICY_DOUBLE,
							NAUTILUS_CLICK_POLICY_DOUBLE,
							NAUTILUS_CLICK_POLICY_DOUBLE);
	
	nautilus_preference_enum_add_entry_by_name (NAUTILUS_PREFERENCES_CLICK_POLICY,
						    _("single"),
						    _("Activate items with a single click"),
						    NAUTILUS_CLICK_POLICY_SINGLE);
	
	nautilus_preference_enum_add_entry_by_name (NAUTILUS_PREFERENCES_CLICK_POLICY,
						    _("double"),
						    _("Activate items with a double click"),
						    NAUTILUS_CLICK_POLICY_DOUBLE);
	
	/* Speed tradeoffs */
	global_preferences_register_speed_tradeoff_with_defaults (NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS,
							   	  _("Display text in icons"),
							   	  NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY,
							   	  NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY,
							   	  NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY);
	
	global_preferences_register_speed_tradeoff_with_defaults (NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
							   	  _("Show thumbnails for image files"),
							   	  NAUTILUS_SPEED_TRADEOFF_ALWAYS,
							   	  NAUTILUS_SPEED_TRADEOFF_ALWAYS,
							   	  NAUTILUS_SPEED_TRADEOFF_ALWAYS);
	
	global_preferences_register_speed_tradeoff_with_defaults (NAUTILUS_PREFERENCES_USE_PUBLIC_METADATA,
							   	  _("Read and write metadata in each folder"),
							   	  NAUTILUS_SPEED_TRADEOFF_ALWAYS,
							   	  NAUTILUS_SPEED_TRADEOFF_ALWAYS,
							   	  NAUTILUS_SPEED_TRADEOFF_ALWAYS);
	
	/* Sidebar panels */
	global_preferences_register_sidebar_panels ();

	/* Appearance options */
	global_preferences_register_boolean_with_defaults (NAUTILUS_PREFERENCES_SMOOTH_GRAPHICS_MODE,
							   _("Use smoother (but slower) graphics"),
							   TRUE,
							   TRUE,
							   TRUE);

	global_preferences_register_string_with_defaults (NAUTILUS_PREFERENCES_DIRECTORY_VIEW_FONT_FAMILY,
							  _("Font family used to display file names"),
							  "helvetica",
							  "helvetica",
							  "helvetica");


	global_preferences_register_boolean_with_defaults (NAUTILUS_PREFERENCES_START_WITH_TOOL_BAR,
							   _("Display tool bar in new windows"),
							   TRUE,
							   TRUE,
							   TRUE);

	global_preferences_register_boolean_with_defaults (NAUTILUS_PREFERENCES_START_WITH_LOCATION_BAR,
							   _("Display location bar in new windows"),
							   TRUE,
							   TRUE,
							   TRUE);

	global_preferences_register_boolean_with_defaults (NAUTILUS_PREFERENCES_START_WITH_STATUS_BAR,
							   _("Display status bar in new windows"),
							   TRUE,
							   TRUE,
							   TRUE);

	global_preferences_register_boolean_with_defaults (NAUTILUS_PREFERENCES_START_WITH_SIDEBAR,
							   _("Display sidebar in new windows"),
							   TRUE,
							   TRUE,
							   TRUE);

								  
	/* search tradeoffs */
	global_preferences_register_boolean_with_defaults (NAUTILUS_PREFERENCES_SEARCH_METHOD,
							   _("Always do slow, complete search"),
							   FALSE,
							   FALSE,
							   FALSE);

	/* search bar type */
	global_preferences_register_enum_with_defaults (NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
							_("search type to do by default"),
							NAUTILUS_SIMPLE_SEARCH_BAR,
							NAUTILUS_COMPLEX_SEARCH_BAR,
							NAUTILUS_COMPLEX_SEARCH_BAR);

	nautilus_preference_enum_add_entry_by_name (NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
						     _("search by text"),
						     _("Search for files by text only"),
						     NAUTILUS_SIMPLE_SEARCH_BAR);

	nautilus_preference_enum_add_entry_by_name (NAUTILUS_PREFERENCES_SEARCH_BAR_TYPE,
						     _("search by text and properties"),
						     _("Search for files by text and by their properties"),
						     NAUTILUS_COMPLEX_SEARCH_BAR);

	/* web search uri  */
	global_preferences_register_string_with_defaults (NAUTILUS_PREFERENCES_SEARCH_WEB_URI,
							 _("Search Web Location"),
							 DEFAULT_SEARCH_WEB_URI,
							 DEFAULT_SEARCH_WEB_URI,
							 DEFAULT_SEARCH_WEB_URI);

	
	global_preferences_register_boolean_with_defaults (NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
							   _("Show hidden files (starting with \".\")"),
							   FALSE,
							   FALSE,
							   TRUE);

	global_preferences_register_boolean_with_defaults (NAUTILUS_PREFERENCES_SHOW_BACKUP_FILES,
							   _("Show backup files (ending with \"~\")"),
							   FALSE,
							   FALSE,
							   TRUE);

	global_preferences_register_boolean_with_defaults (NAUTILUS_PREFERENCES_SHOW_SPECIAL_FLAGS,
							   _("Show special flags in Properties window"),
							   FALSE,
							   FALSE,
							   TRUE);

	global_preferences_register_boolean_with_defaults (NAUTILUS_PREFERENCES_CAN_ADD_CONTENT,
							   _("Can add Content"),
							   FALSE,
							   TRUE,
							   TRUE);
	

	{
		char	*user_main_directory;
		char	*novice_home_location;
		char	*intermediate_home_location;
		char	*hacker_home_location;

		user_main_directory = nautilus_get_user_main_directory ();
		
		novice_home_location = g_strdup_printf ("file://%s", user_main_directory);
		intermediate_home_location = g_strdup_printf ("file://%s", g_get_home_dir());
		hacker_home_location = g_strdup_printf ("file://%s", g_get_home_dir());
		
		global_preferences_register_string_with_defaults (NAUTILUS_PREFERENCES_HOME_URI,
								  "Home Location",
								  novice_home_location,
								  intermediate_home_location,
								  hacker_home_location);

		g_free (user_main_directory);
		g_free (novice_home_location);
		g_free (intermediate_home_location);
		g_free (hacker_home_location);
	}

	register_proxy_preferences ();
}

static gboolean
global_preferences_close_dialog_callback (GtkWidget   *dialog,
					  gpointer    user_data)
{
	nautilus_global_preferences_hide_dialog ();

	return TRUE;
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

void
nautilus_global_preferences_dialog_update (void)
{
	gboolean was_showing = FALSE;

	/* Free the dialog first, cause it has refs to preferences */
	if (global_prefs_dialog != NULL) {
		was_showing = GTK_WIDGET_VISIBLE (global_prefs_dialog);

		gtk_widget_destroy (global_prefs_dialog);
		global_prefs_dialog = NULL;
	}
	
	global_preferences_get_dialog ();

	if (was_showing) {
		nautilus_global_preferences_show_dialog ();
	}
}

void
nautilus_global_preferences_initialize (void)
{
	static gboolean initialized = FALSE;
	
	if (initialized) {
		return;
	}

	global_preferences_register ();

	initialized = TRUE;
}
