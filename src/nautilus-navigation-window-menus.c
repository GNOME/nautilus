/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000, 2001 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: John Sullivan <sullivan@eazel.com>
 */

/* nautilus-window-menus.h - implementation of nautilus window menu operations,
 *                           split into separate file just for convenience.
 */
#include <config.h>

#include <locale.h> 

#include "nautilus-application.h"
#include "nautilus-bookmark-list.h"
#include "nautilus-bookmark-parsing.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-file-management-properties.h"
#include "nautilus-property-browser.h"
#include "nautilus-signaller.h"
#include "nautilus-switchable-navigation-bar.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-window-private.h"
#include <bonobo/bonobo-ui-util.h>
#include <eel/eel-debug.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-xml-extensions.h>
#include <libxml/parser.h>
#include <gtk/gtkmain.h>
#include <libgnome/gnome-help.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-about.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-undo-manager.h>
#include <libnautilus/nautilus-bonobo-ui.h>

#ifdef ENABLE_PROFILER
#include "nautilus-profiler.h"
#endif

#define STATIC_BOOKMARKS_FILE_NAME	"static_bookmarks.xml"

/* Private menu definitions; others are in <libnautilus/nautilus-bonobo-ui.h>.
 * These are not part of the published set, either because they are
 * development-only or because we expect to change them and
 * don't want other code relying on their existence.
 */

#define MENU_PATH_SHOW_HIDE_SIDEBAR			"/menu/View/Show Hide Placeholder/Show Hide Sidebar"
#define MENU_PATH_SHOW_HIDE_LOCATION_BAR		"/menu/View/Show Hide Placeholder/Show Hide Location Bar"

#define MENU_PATH_HISTORY_PLACEHOLDER			"/menu/Go/History Placeholder"
#define MENU_PATH_BUILT_IN_BOOKMARKS_PLACEHOLDER	"/menu/Bookmarks/Built-in Bookmarks Placeholder"
#define MENU_PATH_BOOKMARKS_PLACEHOLDER			"/menu/Bookmarks/Bookmarks Placeholder"

#define COMMAND_SHOW_HIDE_SIDEBAR                       "/commands/Show Hide Sidebar"
#define COMMAND_SHOW_HIDE_LOCATION_BAR                  "/commands/Show Hide Location Bar"
#define COMMAND_SHOW_HIDE_STATUS_BAR                    "/commands/Show Hide Statusbar"

#define ID_SHOW_HIDE_SIDEBAR                            "Show Hide Sidebar"
#define ID_SHOW_HIDE_LOCATION_BAR                       "Show Hide Location Bar"

#define RESPONSE_FORGET		1000

static GtkWindow *bookmarks_window = NULL;
static NautilusBookmarkList *bookmarks = NULL;

static void                  append_dynamic_bookmarks                      (NautilusNavigationWindow   *window);
static NautilusBookmarkList *get_bookmark_list                             (void);
static void                  refresh_bookmarks_menu                        (NautilusNavigationWindow   *window);
static void                  schedule_refresh_bookmarks_menu               (NautilusNavigationWindow   *window);
static void                  edit_bookmarks                                (NautilusNavigationWindow   *window);
static void                  add_bookmark_for_current_location             (NautilusNavigationWindow   *window);
static void                  schedule_refresh_go_menu                      (NautilusWindow *window);

#ifdef HAVE_MEDUSA
static void
file_menu_find_callback (BonoboUIComponent *component, 
			 gpointer user_data, 
			 const char *verb)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (user_data);

	if (!window->details->updating_bonobo_state) {
		nautilus_window_show_location_bar_temporarily
			(window, TRUE);
	}
}

static void
toolbar_toggle_find_mode_callback (BonoboUIComponent *component, 
			             gpointer user_data, 
			             const char *verb)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (user_data);

	if (!window->details->updating_bonobo_state) {
		nautilus_window_show_location_bar_temporarily
			(window, !nautilus_window_get_search_mode (window));
	}
}
#endif

static void
file_menu_close_all_windows_callback (BonoboUIComponent *component, 
			              gpointer user_data, 
			              const char *verb)
{
	nautilus_application_close_all_navigation_windows ();
}

static void
go_menu_back_callback (BonoboUIComponent *component, 
		       gpointer user_data, 
		       const char *verb) 
{
	nautilus_navigation_window_go_back (NAUTILUS_NAVIGATION_WINDOW (user_data));
}

static void
go_menu_forward_callback (BonoboUIComponent *component, 
			  gpointer user_data, 
			  const char *verb) 
{
	nautilus_navigation_window_go_forward (NAUTILUS_NAVIGATION_WINDOW (user_data));
}

static void
forget_history_if_yes (GtkDialog *dialog, int response, gpointer callback_data)
{
	if (response == RESPONSE_FORGET) {
		nautilus_forget_history ();
	}
	gtk_object_destroy (GTK_OBJECT (dialog));
}

static void
forget_history_if_confirmed (NautilusWindow *window)
{
	GtkDialog *dialog;
	char *prompt;
	char *detail;

	/* Confirm before forgetting history because it's a rare operation that
	 * is hard to recover from. We don't want people doing it accidentally
	 * when they intended to choose another Go menu item.
	 */
	if ((rand() % 10) == 0) {
		/* This is a little joke, shows up occasionally. I only
		 * implemented this feature so I could use this joke. 
		 */
		prompt = _("Are you sure you want to forget history?");
		detail = _("If you do, you will be doomed to repeat it.");
	} else {
		prompt = _("Are you sure you want to clear the list "
			   "of locations you have visited?");
		detail = _("If you clear the list of locations,"
			   " they will be permanently deleted."); 
	}
					   
	dialog = eel_create_question_dialog (prompt,
					     detail,
					     _("Clear History"), 
					     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					     GTK_STOCK_CLEAR, RESPONSE_FORGET,
					     GTK_WINDOW (window));

	gtk_widget_show (GTK_WIDGET (dialog));
	
	g_signal_connect (dialog, "response",
			  G_CALLBACK (forget_history_if_yes), NULL);

	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_CANCEL);
}

static void
go_menu_forget_history_callback (BonoboUIComponent *component, 
			         gpointer user_data, 
			         const char *verb) 
{
	forget_history_if_confirmed (NAUTILUS_WINDOW (user_data));
}

static void
view_menu_show_hide_sidebar_state_changed_callback (BonoboUIComponent *component, 
						    const char *path,
						    Bonobo_UIComponent_EventType type,
						    const char *state,
						    gpointer user_data)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

        if (strcmp (state, "") == 0) {
                /* State goes blank when component is removed; ignore this. */
                return;
        }

	if (!strcmp (state, "1")) {
		nautilus_navigation_window_show_sidebar (window);
	} else {
		nautilus_navigation_window_hide_sidebar (window);
	}
}

static void
view_menu_show_hide_location_bar_state_changed_callback (BonoboUIComponent *component, 
							 const char *path,
							 Bonobo_UIComponent_EventType type,
							 const char *state,
							 gpointer user_data)
{
	NautilusNavigationWindow *window;

	window = NAUTILUS_NAVIGATION_WINDOW (user_data);

        if (strcmp (state, "") == 0) {
                /* State goes blank when component is removed; ignore this. */
                return;
        }

	if (!strcmp (state, "1")) {
		nautilus_navigation_window_show_location_bar (window, TRUE);
	} else {
		nautilus_navigation_window_hide_location_bar (window, TRUE);
	}
}

void
nautilus_navigation_window_update_show_hide_menu_items (NautilusNavigationWindow *window) 
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	nautilus_window_ui_freeze (NAUTILUS_WINDOW (window));

	bonobo_ui_component_freeze (NAUTILUS_WINDOW (window)->details->shell_ui, NULL);
		
	nautilus_bonobo_set_toggle_state (NAUTILUS_WINDOW (window)->details->shell_ui,
					  COMMAND_SHOW_HIDE_SIDEBAR,
					  nautilus_navigation_window_sidebar_showing (window));
	nautilus_bonobo_set_toggle_state (NAUTILUS_WINDOW (window)->details->shell_ui,
					  COMMAND_SHOW_HIDE_LOCATION_BAR,
					  nautilus_navigation_window_location_bar_showing (window));

	bonobo_ui_component_thaw (NAUTILUS_WINDOW (window)->details->shell_ui,
				  NULL);

	nautilus_window_ui_thaw (NAUTILUS_WINDOW (window));
}

static void
bookmarks_menu_add_bookmark_callback (BonoboUIComponent *component, 
			              gpointer user_data, 
			              const char *verb)
{
        add_bookmark_for_current_location (NAUTILUS_NAVIGATION_WINDOW (user_data));
}

static void
bookmarks_menu_edit_bookmarks_callback (BonoboUIComponent *component, 
			                gpointer user_data, 
			                const char *verb)
{
        edit_bookmarks (NAUTILUS_NAVIGATION_WINDOW (user_data));
}

#ifdef WEB_NAVIGATION_ENABLED
static char *
get_static_bookmarks_file_path (void)
{
	char *update_xml_file_path, *built_in_xml_file_path;
	char *update_uri, *built_in_uri;
	char *user_directory_path;
	gboolean update_exists, built_in_exists;
	GnomeVFSFileInfo *update_info, *built_in_info;
	char *result;
	
	/* see if there is a static bookmarks file in the updates directory and get its mod-date */
	user_directory_path = nautilus_get_user_directory ();
	update_xml_file_path = g_strdup_printf ("%s/updates/%s", user_directory_path, STATIC_BOOKMARKS_FILE_NAME);
	update_exists = g_file_test (update_xml_file_path, G_FILE_TEST_EXISTS);
	g_free (user_directory_path);
	
	/* get the mod date of the built-in static bookmarks file */
	built_in_xml_file_path = g_build_filename (NAUTILUS_DATADIR, STATIC_BOOKMARKS_FILE_NAME, NULL);
	built_in_exists = g_file_test (built_in_xml_file_path, G_FILE_TEST_EXISTS);
	
	/* if we only have one file, return its path as the one to use */
	if (built_in_exists && !update_exists) {
		g_free (update_xml_file_path);
		return built_in_xml_file_path;
	}

	if (!built_in_exists && update_exists) {
		g_free (built_in_xml_file_path);
		return update_xml_file_path;
	}
	
	/* if we have neither file, return NULL */		
	if (!built_in_exists && !update_exists) {
		g_free (built_in_xml_file_path);
		g_free (update_xml_file_path);
		return NULL;
	}
	
	/* both files exist, so use the one with the most recent mod-date */
	update_uri = gnome_vfs_get_uri_from_local_path (update_xml_file_path);
	update_info = gnome_vfs_file_info_new ();
	gnome_vfs_get_file_info (update_uri, update_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	g_free (update_uri);
	
	built_in_uri = gnome_vfs_get_uri_from_local_path (built_in_xml_file_path);
	built_in_info = gnome_vfs_file_info_new ();
	gnome_vfs_get_file_info (built_in_uri, built_in_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	g_free (built_in_uri);

	/* see which is most recent */
	if (update_info->mtime <= built_in_info->mtime) {
		result = built_in_xml_file_path;
		g_free (update_xml_file_path);
	} else {
		result = update_xml_file_path;
		g_free (built_in_xml_file_path);
	}

	gnome_vfs_file_info_unref (update_info);
	gnome_vfs_file_info_unref (built_in_info);
	
	return result;
}
#endif

static void
append_separator (NautilusNavigationWindow *window, const char *path)
{
	nautilus_window_ui_freeze (NAUTILUS_WINDOW (window));

	nautilus_bonobo_add_menu_separator
		(NAUTILUS_WINDOW (window)->details->shell_ui, path);

	nautilus_window_ui_thaw (NAUTILUS_WINDOW (window));
}

static void
free_bookmark_list (void)
{
	g_object_unref (bookmarks);
}

static NautilusBookmarkList *
get_bookmark_list (void)
{
        if (bookmarks == NULL) {
                bookmarks = nautilus_bookmark_list_new ();
                eel_debug_call_at_shutdown (free_bookmark_list);
        }
	
        return bookmarks;
}


static void
remove_bookmarks_for_uri_if_yes (GtkDialog *dialog, int response, gpointer callback_data)
{
	const char *uri;

	g_assert (GTK_IS_DIALOG (dialog));
	g_assert (callback_data != NULL);

	if (response == GTK_RESPONSE_YES) {
		uri = callback_data;
		nautilus_bookmark_list_delete_items_with_uri (get_bookmark_list (), uri);
	}

	gtk_object_destroy (GTK_OBJECT (dialog));
}

static void
show_bogus_bookmark_window (NautilusWindow *window,
			    NautilusBookmark *bookmark)
{
	GtkDialog *dialog;
	char *uri;
	char *uri_for_display;
	char *prompt;
	char *detail;

	uri = nautilus_bookmark_get_uri (bookmark);
	uri_for_display = eel_format_uri_for_display (uri);
	
	prompt = _("Do you want to remove any bookmarks with the "
		   "non-existing location from your list?");
	detail = g_strdup_printf (_("The location \"%s\" does not exist."), uri_for_display);
	
	dialog = eel_show_yes_no_dialog (prompt, detail,
					 _("Bookmark for Nonexistent Location"),
					 _("Remove"), GTK_STOCK_CANCEL,
					 GTK_WINDOW (window));
	
	eel_gtk_signal_connect_free_data
		(GTK_OBJECT (dialog),
		 "response",
		 G_CALLBACK (remove_bookmarks_for_uri_if_yes),
		 g_strdup (uri));
	
	gtk_dialog_set_default_response (dialog, GTK_RESPONSE_NO);

	g_free (uri);
	g_free (uri_for_display);
	g_free (detail);
}

static void
create_menu_item_from_node (NautilusNavigationWindow *window,
			    xmlNodePtr node,
			    const char *menu_path,
			    int *index)
{
	NautilusBookmark *bookmark;
	xmlChar *xml_folder_name;
	int sub_index;
	char *sub_menu_path, *escaped_name;
	
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));
		
	if (node->type != XML_ELEMENT_NODE) {
		return;
	}

	nautilus_window_ui_freeze (NAUTILUS_WINDOW (window));

	if (strcmp (node->name, "bookmark") == 0) {
		bookmark = nautilus_bookmark_new_from_node (node);
		nautilus_menus_append_bookmark_to_menu
			(NAUTILUS_WINDOW (window), 
			 NAUTILUS_WINDOW (window)->details->shell_ui,
			 bookmark, 
			 menu_path, 
			 *index, 
			 G_CALLBACK (schedule_refresh_bookmarks_menu), 
			 show_bogus_bookmark_window);
		g_object_unref (bookmark);
	} else if (strcmp (node->name, "separator") == 0) {
		append_separator (window, menu_path);
	} else if (strcmp (node->name, "folder") == 0) {
		xml_folder_name = eel_xml_get_property_translated (node, "name");
		nautilus_bonobo_add_submenu (NAUTILUS_WINDOW (window)->details->shell_ui, menu_path, xml_folder_name, NULL);

		/* Construct path and make sure it is escaped properly */
		escaped_name = gnome_vfs_escape_string (xml_folder_name);
		sub_menu_path = g_strdup_printf ("%s/%s", menu_path, escaped_name);
		g_free (escaped_name);
				
		for (node = eel_xml_get_children (node), sub_index = 0;
		     node != NULL;
		     node = node->next) {
			create_menu_item_from_node (window, node, sub_menu_path, &sub_index);
		}
		g_free (sub_menu_path);
		xmlFree (xml_folder_name);
	} else {
		g_warning ("found unknown node '%s', ignoring", node->name);
	}

	(*index)++;

	nautilus_window_ui_thaw (NAUTILUS_WINDOW (window));
}

#ifdef WEB_NAVIGATION_ENABLED
static void
append_static_bookmarks (NautilusWindow *window, const char *menu_path)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *file_path;
	int index;

	/* Walk through XML tree creating bookmarks, folders, and separators. */
	file_path = get_static_bookmarks_file_path ();

	if (file_path == NULL) {
		return;
	}
	
	doc = xmlParseFile (file_path);
	g_free (file_path);

	node = eel_xml_get_root_children (doc);
	index = 0;

	for (index = 0; node != NULL; node = node->next) {
		create_menu_item_from_node (window, node, menu_path, &index);
	}
	
	xmlFreeDoc(doc);
}
#endif

static GtkWindow *
get_or_create_bookmarks_window (GObject *undo_manager_source)
{
	if (bookmarks_window == NULL) {
		bookmarks_window = create_bookmarks_window (get_bookmark_list(), undo_manager_source);
	}
	return bookmarks_window;
}

/**
 * nautilus_bookmarks_exiting:
 * 
 * Last chance to save state before app exits.
 * Called when application exits; don't call from anywhere else.
 **/
void
nautilus_bookmarks_exiting (void)
{
	if (bookmarks_window != NULL) {
		nautilus_bookmarks_window_save_geometry (bookmarks_window);
	}
}

/**
 * add_bookmark_for_current_location
 * 
 * Add a bookmark for the displayed location to the bookmarks menu.
 * Does nothing if there's already a bookmark for the displayed location.
 */
static void
add_bookmark_for_current_location (NautilusNavigationWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	nautilus_bookmark_list_append (get_bookmark_list (), 
				       NAUTILUS_WINDOW (window)->current_location_bookmark);
}

static void
edit_bookmarks (NautilusNavigationWindow *window)
{
	GtkWindow *dialog;

	dialog = get_or_create_bookmarks_window (G_OBJECT (window));

	gtk_window_set_screen (
		dialog, gtk_window_get_screen (GTK_WINDOW (window)));
        gtk_window_present (dialog);
}

void
nautilus_window_bookmarks_preference_changed_callback (gpointer user_data)
{
	refresh_bookmarks_menu (NAUTILUS_NAVIGATION_WINDOW (user_data));
}

static void
refresh_bookmarks_menu (NautilusNavigationWindow *window)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	/* Unregister any pending call to this function. */
	nautilus_navigation_window_remove_bookmarks_menu_callback (window);

	g_object_ref (G_OBJECT (window));
	bonobo_ui_component_freeze 
		(NAUTILUS_WINDOW (window)->details->shell_ui, NULL);

	nautilus_navigation_window_remove_bookmarks_menu_items (window);

#ifdef WEB_NAVIGATION_ENABLED
	if (!eel_preferences_get_boolean (NAUTILUS_PREFERENCES_HIDE_BUILT_IN_BOOKMARKS)) {
		append_static_bookmarks (window, MENU_PATH_BUILT_IN_BOOKMARKS_PLACEHOLDER);
	}
#endif

	append_dynamic_bookmarks (window);

	bonobo_ui_component_thaw (NAUTILUS_WINDOW (window)->details->shell_ui, NULL);
	g_object_unref (G_OBJECT (window));
}

/**
 * nautilus_navigation_window_initialize_bookmarks_menu
 * 
 * Fill in bookmarks menu with stored bookmarks, and wire up signals
 * so we'll be notified when bookmark list changes.
 */
static void 
nautilus_navigation_window_initialize_bookmarks_menu (NautilusNavigationWindow *window)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	/* Construct the initial set of bookmarks. */
	refresh_bookmarks_menu (window);

	/* Recreate static & dynamic part of menu if preference about
	 * showing static bookmarks changes.
	 */
	eel_preferences_add_callback_while_alive (NAUTILUS_PREFERENCES_HIDE_BUILT_IN_BOOKMARKS,
						  nautilus_window_bookmarks_preference_changed_callback,
						  window,
						  G_OBJECT (window));
		
	/* Recreate dynamic part of menu if bookmark list changes */
	g_signal_connect_object (get_bookmark_list (), "contents_changed",
				 G_CALLBACK (schedule_refresh_bookmarks_menu),
				 window, G_CONNECT_SWAPPED);

	/* Recreate static & dynamic parts of menu if icon theme changes */
	g_signal_connect_object (nautilus_icon_factory_get (), "icons_changed",
				 G_CALLBACK (schedule_refresh_bookmarks_menu),
				 window, G_CONNECT_SWAPPED);
}

void
nautilus_window_remove_go_menu_callback (NautilusWindow *window)
{
        if (window->details->refresh_go_menu_idle_id != 0) {
                g_source_remove (window->details->refresh_go_menu_idle_id);
		window->details->refresh_go_menu_idle_id = 0;
        }
}

void
nautilus_window_remove_go_menu_items (NautilusWindow *window)
{
	nautilus_window_ui_freeze (window);

	nautilus_bonobo_remove_menu_items_and_commands 
		(window->details->shell_ui, 
		 MENU_PATH_HISTORY_PLACEHOLDER);

	nautilus_window_ui_thaw (window);
}

/**
 * refresh_go_menu:
 * 
 * Refresh list of bookmarks at end of Go menu to match centralized history list.
 * @window: The NautilusWindow whose Go menu will be refreshed.
 **/
static void
refresh_go_menu (NautilusWindow *window)
{
	GList *node;
	int index;
	
	g_assert (NAUTILUS_IS_WINDOW (window));

	/* Unregister any pending call to this function. */
	nautilus_window_remove_go_menu_callback (window);

	bonobo_ui_component_freeze (window->details->shell_ui, NULL);

	/* Remove old set of history items. */
	nautilus_window_remove_go_menu_items (window);

	/* Add in a new set of history items. */
	for (node = nautilus_get_history_list (), index = 0;
	     node != NULL && index < 10;
	     node = node->next, index++) {
		nautilus_menus_append_bookmark_to_menu 
			(window,
			 window->details->shell_ui,
			 NAUTILUS_BOOKMARK (node->data),
			 MENU_PATH_HISTORY_PLACEHOLDER,
			 index,
			 G_CALLBACK (schedule_refresh_go_menu),
			 show_bogus_bookmark_window);
	}

	bonobo_ui_component_thaw (window->details->shell_ui, NULL);
}

static gboolean
refresh_go_menu_idle_callback (gpointer data)
{
	g_assert (NAUTILUS_IS_WINDOW (data));

	refresh_go_menu (NAUTILUS_WINDOW (data));

        /* Don't call this again (unless rescheduled) */
        return FALSE;
}

static void
schedule_refresh_go_menu (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	if (window->details->refresh_go_menu_idle_id == 0) {
                window->details->refresh_go_menu_idle_id
                        = g_idle_add (refresh_go_menu_idle_callback,
				      window);
	}	
}

/**
 * nautilus_navigation_window_initialize_go_menu
 * 
 * Wire up signals so we'll be notified when history list changes.
 */
static void 
nautilus_navigation_window_initialize_go_menu (NautilusNavigationWindow *window)
{
	/* Recreate bookmarks part of menu if history list changes
	 * or if icon theme changes.
	 */
	g_signal_connect_object (nautilus_signaller_get_current (), "history_list_changed",
				 G_CALLBACK (schedule_refresh_go_menu), window, G_CONNECT_SWAPPED);
	g_signal_connect_object (nautilus_icon_factory_get (), "icons_changed",
				 G_CALLBACK (schedule_refresh_go_menu), window, G_CONNECT_SWAPPED);
}

/**
 * nautilus_window_initialize_menus
 * 
 * Create and install the set of menus for this window.
 * @window: A recently-created NautilusWindow.
 */
void 
nautilus_navigation_window_initialize_menus_part_1 (NautilusNavigationWindow *navigation_window)
{
	NautilusWindow *window;
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Close All Windows", file_menu_close_all_windows_callback),
#ifdef HAVE_MEDUSA
		BONOBO_UI_VERB ("Find", file_menu_find_callback),
		BONOBO_UI_VERB ("Toggle Find Mode", toolbar_toggle_find_mode_callback),
#endif
		BONOBO_UI_VERB ("Back", go_menu_back_callback),
		BONOBO_UI_VERB ("Forward", go_menu_forward_callback),
		BONOBO_UI_VERB ("Clear History", go_menu_forget_history_callback),
		BONOBO_UI_VERB ("Add Bookmark", bookmarks_menu_add_bookmark_callback),
		BONOBO_UI_VERB ("Edit Bookmarks", bookmarks_menu_edit_bookmarks_callback),

		BONOBO_UI_VERB_END
	};

	window = NAUTILUS_WINDOW (navigation_window);

	nautilus_window_ui_freeze (window);

	bonobo_ui_component_freeze (window->details->shell_ui, NULL);

	nautilus_navigation_window_update_show_hide_menu_items (navigation_window);

	bonobo_ui_component_add_verb_list_with_data (window->details->shell_ui,
						     verbs, window);

	bonobo_ui_component_add_listener
		(window->details->shell_ui,
		 ID_SHOW_HIDE_SIDEBAR,
		 view_menu_show_hide_sidebar_state_changed_callback, 
		 window);
	bonobo_ui_component_add_listener
		(window->details->shell_ui,
		 ID_SHOW_HIDE_LOCATION_BAR,
		 view_menu_show_hide_location_bar_state_changed_callback, 
		 window);

	bonobo_ui_component_thaw (window->details->shell_ui, NULL);

	nautilus_window_ui_thaw (window);
}

void 
nautilus_navigation_window_initialize_menus_part_2 (NautilusNavigationWindow *window)
{
	nautilus_window_ui_freeze (NAUTILUS_WINDOW (window));

        nautilus_navigation_window_initialize_go_menu (window);
        nautilus_navigation_window_initialize_bookmarks_menu (window);

	nautilus_window_ui_thaw (NAUTILUS_WINDOW (window));
}

void
nautilus_navigation_window_remove_bookmarks_menu_callback (NautilusNavigationWindow *window)
{
        if (window->details->refresh_bookmarks_menu_idle_id != 0) {
                g_source_remove (window->details->refresh_bookmarks_menu_idle_id);
		window->details->refresh_bookmarks_menu_idle_id = 0;
        }
}

void
nautilus_navigation_window_remove_bookmarks_menu_items (NautilusNavigationWindow *window)
{
	nautilus_window_ui_freeze (NAUTILUS_WINDOW (window));

	nautilus_bonobo_remove_menu_items_and_commands
		(NAUTILUS_WINDOW (window)->details->shell_ui, 
		 MENU_PATH_BUILT_IN_BOOKMARKS_PLACEHOLDER);
	nautilus_bonobo_remove_menu_items_and_commands 
		(NAUTILUS_WINDOW (window)->details->shell_ui, 
		 MENU_PATH_BOOKMARKS_PLACEHOLDER);

	nautilus_window_ui_thaw (NAUTILUS_WINDOW (window));
}

static void
append_dynamic_bookmarks (NautilusNavigationWindow *window)
{
        NautilusBookmarkList *bookmarks;
	guint bookmark_count;
	guint index;
		
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	bookmarks = get_bookmark_list ();

	/* append new set of bookmarks */
	bookmark_count = nautilus_bookmark_list_length (bookmarks);
	for (index = 0; index < bookmark_count; ++index) {
		nautilus_menus_append_bookmark_to_menu
			(NAUTILUS_WINDOW (window), 
			 NAUTILUS_WINDOW (window)->details->shell_ui,
			 nautilus_bookmark_list_item_at (bookmarks, index),
			 MENU_PATH_BOOKMARKS_PLACEHOLDER,
			 index,
			 G_CALLBACK (schedule_refresh_bookmarks_menu), 
			 show_bogus_bookmark_window);
	}
}

static gboolean
refresh_bookmarks_menu_idle_callback (gpointer data)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (data));

	refresh_bookmarks_menu (NAUTILUS_NAVIGATION_WINDOW (data));

        /* Don't call this again (unless rescheduled) */
        return FALSE;
}

static void
schedule_refresh_bookmarks_menu (NautilusNavigationWindow *window)
{
	g_assert (NAUTILUS_IS_NAVIGATION_WINDOW (window));

	if (window->details->refresh_bookmarks_menu_idle_id == 0) {
                window->details->refresh_bookmarks_menu_idle_id
                        = g_idle_add (refresh_bookmarks_menu_idle_callback,
				      window);
	}	
}
