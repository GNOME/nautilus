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
#include "nautilus-connect-server-dialog.h"
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
#include <libegg/egg-screen-help.h>
#include <libgnome/gnome-help.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-about.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libnautilus-extension/nautilus-menu-provider.h>
#include <libnautilus-private/nautilus-bonobo-extensions.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-module.h>
#include <libnautilus-private/nautilus-undo-manager.h>
#include <libnautilus/nautilus-bonobo-ui.h>

#ifdef ENABLE_PROFILER
#include "nautilus-profiler.h"
#endif

/* Private menu definitions; others are in <libnautilus/nautilus-bonobo-ui.h>.
 * These are not part of the published set, either because they are
 * development-only or because we expect to change them and
 * don't want other code relying on their existence.
 */

#define MENU_PATH_SHOW_HIDE_SIDEBAR			"/menu/View/Show Hide Placeholder/Show Hide Sidebar"
#define MENU_PATH_SHOW_HIDE_TOOLBAR			"/menu/View/Show Hide Placeholder/Show Hide Toolbar"
#define MENU_PATH_SHOW_HIDE_LOCATION_BAR		"/menu/View/Show Hide Placeholder/Show Hide Location Bar"
#define MENU_PATH_SHOW_HIDE_STATUS_BAR			"/menu/View/Show Hide Placeholder/Show Hide Statusbar"

#define MENU_PATH_EXTENSION_ACTIONS                     "/menu/File/Extension Actions"
#define POPUP_PATH_EXTENSION_ACTIONS                     "/popups/background/Before Zoom Items/Extension Actions"

#define COMMAND_PATH_CLOSE_WINDOW			"/commands/Close"
#define COMMAND_SHOW_HIDE_SIDEBAR                       "/commands/Show Hide Sidebar"
#define COMMAND_SHOW_HIDE_TOOLBAR                       "/commands/Show Hide Toolbar"
#define COMMAND_SHOW_HIDE_LOCATION_BAR                  "/commands/Show Hide Location Bar"
#define COMMAND_SHOW_HIDE_STATUS_BAR                    "/commands/Show Hide Statusbar"
#define COMMAND_GO_BURN_CD				"/commands/Go to Burn CD"

#define ID_SHOW_HIDE_SIDEBAR                            "Show Hide Sidebar"
#define ID_SHOW_HIDE_TOOLBAR                            "Show Hide Toolbar"
#define ID_SHOW_HIDE_LOCATION_BAR                       "Show Hide Location Bar"
#define ID_SHOW_HIDE_STATUS_BAR                         "Show Hide Statusbar"

#define START_HERE_URI          "start-here:"
#define COMPUTER_URI          "computer:"
#define BURN_CD_URI          "burn:"

/* Struct that stores all the info necessary to activate a bookmark. */
typedef struct {
        NautilusBookmark *bookmark;
        NautilusWindow *window;
        guint changed_handler_id;
	NautilusBookmarkFailedCallback failed_callback;
} BookmarkHolder;

static BookmarkHolder *
bookmark_holder_new (NautilusBookmark *bookmark, 
		     NautilusWindow *window,
		     GCallback refresh_callback,
		     NautilusBookmarkFailedCallback failed_callback)
{
	BookmarkHolder *new_bookmark_holder;

	new_bookmark_holder = g_new (BookmarkHolder, 1);
	new_bookmark_holder->window = window;
	new_bookmark_holder->bookmark = bookmark;
	new_bookmark_holder->failed_callback = failed_callback;
	/* Ref the bookmark because it might be unreffed away while 
	 * we're holding onto it (not an issue for window).
	 */
	g_object_ref (bookmark);
	new_bookmark_holder->changed_handler_id = 
		g_signal_connect_object (bookmark, "appearance_changed",
					 refresh_callback,
					 window, G_CONNECT_SWAPPED);

	return new_bookmark_holder;
}

static void
bookmark_holder_free (BookmarkHolder *bookmark_holder)
{
	g_signal_handler_disconnect (bookmark_holder->bookmark,
				     bookmark_holder->changed_handler_id);
	g_object_unref (bookmark_holder->bookmark);
	g_free (bookmark_holder);
}

/* Private menu definitions; others are in <libnautilus/nautilus-bonobo-ui.h>.
 * These are not part of the published set, either because they are
 * development-only or because we expect to change them and
 * don't want other code relying on their existence.
 */


static void
bookmark_holder_free_cover (gpointer callback_data, GClosure *closure)
{
	bookmark_holder_free (callback_data);
}

static void
activate_bookmark_in_menu_item (BonoboUIComponent *component, gpointer user_data, const char *path)
{
        BookmarkHolder *holder;
        char *uri;

        holder = (BookmarkHolder *)user_data;

	if (nautilus_bookmark_uri_known_not_to_exist (holder->bookmark)) {
		holder->failed_callback (holder->window, holder->bookmark);
	} else {
	        uri = nautilus_bookmark_get_uri (holder->bookmark);
	        nautilus_window_go_to (holder->window, uri);
	        g_free (uri);
        }
}

void
nautilus_menus_append_bookmark_to_menu (NautilusWindow *window, 
					BonoboUIComponent *uic,
					NautilusBookmark *bookmark, 
					const char *parent_path,
					guint index_in_parent,
					GCallback refresh_callback,
					NautilusBookmarkFailedCallback failed_callback)
{
	BookmarkHolder *bookmark_holder;		
	char *raw_name, *display_name, *truncated_name, *verb_name;
	char *ui_path;
	GdkPixbuf *pixbuf;

	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (NAUTILUS_IS_BOOKMARK (bookmark));

	nautilus_window_ui_freeze (window);

	bookmark_holder = bookmark_holder_new (bookmark, window, refresh_callback, failed_callback);

	/* We double the underscores here to escape them so Bonobo will know they are
	 * not keyboard accelerator character prefixes. If we ever find we need to
	 * escape more than just the underscores, we'll add a menu helper function
	 * instead of a string utility. (Like maybe escaping control characters.)
	 */
	raw_name = nautilus_bookmark_get_name (bookmark);
	truncated_name = eel_truncate_text_for_menu_item (raw_name);
	display_name = eel_str_double_underscores (truncated_name);
	g_free (raw_name);
	g_free (truncated_name);

	/* Create menu item with pixbuf */
	pixbuf = nautilus_bookmark_get_pixbuf (bookmark, NAUTILUS_ICON_SIZE_FOR_MENUS, FALSE);
	nautilus_bonobo_add_numbered_menu_item 
		(uic, 
		 parent_path, 
		 index_in_parent, 
		 display_name, 
		 pixbuf);
	g_object_unref (pixbuf);
	g_free (display_name);
	
	/* Add the status tip */
	ui_path = nautilus_bonobo_get_numbered_menu_item_path
		(uic, parent_path, index_in_parent);
	nautilus_bonobo_set_tip (uic, ui_path, _("Go to the location specified by this bookmark"));
	g_free (ui_path);
			
	/* Add verb to new bookmark menu item */
	verb_name = nautilus_bonobo_get_numbered_menu_item_command 
		(uic, parent_path, index_in_parent);
	bonobo_ui_component_add_verb_full (uic, verb_name, 
					   g_cclosure_new (G_CALLBACK (activate_bookmark_in_menu_item),
							   bookmark_holder, 
							   bookmark_holder_free_cover));
	g_free (verb_name);

	nautilus_window_ui_thaw (window);
}

static void
file_menu_new_window_callback (BonoboUIComponent *component, 
			       gpointer user_data, 
			       const char *verb)
{
	NautilusWindow *current_window;
	NautilusWindow *new_window;

	current_window = NAUTILUS_WINDOW (user_data);
	new_window = nautilus_application_create_navigation_window (
				current_window->application,
				gtk_window_get_screen (GTK_WINDOW (current_window)));
	nautilus_window_go_home (new_window);
}

static void
file_menu_close_window_callback (BonoboUIComponent *component, 
			         gpointer user_data, 
			         const char *verb)
{
	nautilus_window_close (NAUTILUS_WINDOW (user_data));
}

static void
connect_to_server_callback (BonoboUIComponent *component, 
			    gpointer user_data, 
			    const char *verb)
{
	GtkWidget *dialog;
	
	dialog = nautilus_connect_server_dialog_new (NAUTILUS_WINDOW (user_data));
	gtk_widget_show (dialog);
}

static gboolean
have_burn_uri (void)
{
	static gboolean initialized = FALSE;
	static gboolean res;
	GnomeVFSURI *uri;

	if (!initialized) {
		uri = gnome_vfs_uri_new ("burn:///");
		res = uri != NULL;
		if (uri != NULL) {
			gnome_vfs_uri_unref (uri);
		}
		initialized = TRUE;
	}
	return res;
}

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
go_menu_location_callback (BonoboUIComponent *component,
			   gpointer user_data,
			   const char *verb)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (user_data);

	nautilus_window_prompt_for_location (window);
}			   

static void
stop_button_callback (BonoboUIComponent *component, 
			       gpointer user_data, 
			       const char *verb)
{
	nautilus_window_stop_loading (NAUTILUS_WINDOW (user_data));
}

static void
edit_menu_undo_callback (BonoboUIComponent *component, 
			 gpointer user_data, 
			 const char *verb) 
{
	nautilus_undo_manager_undo
		(NAUTILUS_WINDOW (user_data)->application->undo_manager);
}

static void
go_menu_up_callback (BonoboUIComponent *component, 
		     gpointer user_data, 
		     const char *verb) 
{
	nautilus_window_go_up (NAUTILUS_WINDOW (user_data), FALSE);
}

static void
go_menu_home_callback (BonoboUIComponent *component, 
		       gpointer user_data, 
		       const char *verb) 
{
	nautilus_window_go_home (NAUTILUS_WINDOW (user_data));
}

static void
go_menu_start_here_callback (BonoboUIComponent *component, 
			     gpointer user_data, 
			     const char *verb) 
{
	nautilus_window_go_to (NAUTILUS_WINDOW (user_data),
			       START_HERE_URI);
}

static void
go_menu_go_to_computer_callback (BonoboUIComponent *component, 
				 gpointer user_data, 
				 const char *verb) 
{
	nautilus_window_go_to (NAUTILUS_WINDOW (user_data),
			       COMPUTER_URI);
}

static void
go_menu_go_to_templates_callback (BonoboUIComponent *component, 
				 gpointer user_data, 
				 const char *verb) 
{
	char *uri;

	nautilus_create_templates_directory ();
	uri = nautilus_get_templates_directory_uri ();
	nautilus_window_go_to (NAUTILUS_WINDOW (user_data),
			       uri);
	g_free (uri);
}

static void
go_menu_go_to_trash_callback (BonoboUIComponent *component, 
			      gpointer user_data, 
			      const char *verb) 
{
	nautilus_window_go_to (NAUTILUS_WINDOW (user_data),
			       EEL_TRASH_URI);
}

static void
go_menu_go_to_burn_cd_callback (BonoboUIComponent *component, 
				gpointer user_data, 
				const char *verb) 
{
	nautilus_window_go_to (NAUTILUS_WINDOW (user_data),
			       BURN_CD_URI);
}

static void
view_menu_reload_callback (BonoboUIComponent *component, 
			   gpointer user_data, 
			   const char *verb) 
{
	nautilus_window_reload (NAUTILUS_WINDOW (user_data));
}

static void
view_menu_show_hide_statusbar_state_changed_callback (BonoboUIComponent *component, 
						      const char *path,
						      Bonobo_UIComponent_EventType type,
						      const char *state,
						      gpointer user_data)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (user_data);

        if (strcmp (state, "") == 0) {
                /* State goes blank when component is removed; ignore this. */
                return;
        }

	if (!strcmp (state, "1")) {
		nautilus_window_show_status_bar (window);
	} else {
		nautilus_window_hide_status_bar (window);
	}
}

void
nautilus_window_update_show_hide_menu_items (NautilusWindow *window) 
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	nautilus_window_ui_freeze (window);

	bonobo_ui_component_freeze (window->details->shell_ui, NULL);
		
	nautilus_bonobo_set_toggle_state (window->details->shell_ui,
					  COMMAND_SHOW_HIDE_STATUS_BAR,
					  nautilus_window_status_bar_showing (window));

	bonobo_ui_component_thaw (window->details->shell_ui, NULL);

	nautilus_window_ui_thaw (window);
}

static void
view_menu_zoom_in_callback (BonoboUIComponent *component, 
			    gpointer user_data, 
			    const char *verb) 
{
	nautilus_window_zoom_in (NAUTILUS_WINDOW (user_data));
}

static void
view_menu_zoom_out_callback (BonoboUIComponent *component, 
			     gpointer user_data, 
			     const char *verb) 
{
	nautilus_window_zoom_out (NAUTILUS_WINDOW (user_data));
}

static void
view_menu_zoom_normal_callback (BonoboUIComponent *component, 
			        gpointer user_data, 
			        const char *verb) 
{
	nautilus_window_zoom_to_level (NAUTILUS_WINDOW (user_data), 1.0);
}

static void
view_menu_view_as_callback (BonoboUIComponent *component, 
			    gpointer user_data, 
			    const char *verb) 
{
	nautilus_window_show_view_as_dialog (NAUTILUS_WINDOW (user_data));
}

static void
preferences_respond_callback (GtkDialog *dialog,
			      gint response_id)
{
	if (response_id == GTK_RESPONSE_CLOSE) {
		gtk_widget_destroy (GTK_WIDGET (dialog));
	}
}

static void
preferences_callback (BonoboUIComponent *component, 
		      gpointer user_data, 
		      const char *verb)
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);

	nautilus_file_management_properties_dialog_show (G_CALLBACK (preferences_respond_callback), window);
}

static void
backgrounds_and_emblems_callback (BonoboUIComponent *component, 
				  gpointer user_data, 
				  const char *verb)
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);

	nautilus_property_browser_show (gtk_window_get_screen (window));
}

static void
help_menu_about_nautilus_callback (BonoboUIComponent *component, 
			           gpointer user_data, 
			           const char *verb)
{
	static GtkWidget *about = NULL;
	const char *authors[] = {
		"Alexander Larsson",
		"Ali Abdin",
		"Anders Carlsson",
		"Andy Hertzfeld",
		"Arlo Rose",
		"Darin Adler",
		"David Camp",
		"Eli Goldberg",
		"Elliot Lee",
		"Eskil Heyn Olsen",
		"Ettore Perazzoli",
		"Gene Z. Ragan",
		"George Lebl",
		"Ian McKellar",
		"J Shane Culpepper",
		"James Willcox",
		"Jan Arne Petersen",
		"John Harper",
		"John Sullivan",
		"Josh Barrow",
		"Maciej Stachowiak",
		"Mark McLoughlin",
		"Mathieu Lacage",
		"Mike Engber",
		"Mike Fleming",
		"Pavel Cisler",
		"Ramiro Estrugo",
		"Raph Levien",
		"Rebecca Schulman",
		"Robey Pointer",
		"Robin * Slomkowski",
		"Seth Nickell",
		"Susan Kare",
		NULL
	};
	const char *copyright;
	const char *translator_credits;
	const char *locale;

	if (about == NULL) {
		/* We could probably just put a translation in en_US
		 * instead of doing this mess, but I got this working
		 * and I don't feel like fiddling with it any more.
		 */
		locale = setlocale (LC_MESSAGES, NULL);
		if (locale == NULL
		    || strcmp (locale, "C") == 0
		    || strcmp (locale, "POSIX") == 0
		    || strcmp (locale, "en_US") == 0) {
			/* The copyright character here is in UTF-8 */
			copyright = "Copyright \xC2\xA9 1999-2001 Eazel, Inc.";
		} else {
			/* Localize to deal with issues in the copyright
			 * symbol characters -- do not translate the company
			 * name, please.
			 */
			copyright = _("Copyright (C) 1999-2001 Eazel, Inc.");
		}

		/* Translators should localize the following string
		 * which will be displayed at the bottom of the about
		 * box to give credit to the translator(s).
		 */
		translator_credits = (strcmp (_("Translator Credits"), "Translator Credits") == 0) ?
			NULL : _("Translator Credits");
		
		about = gnome_about_new (_("Nautilus"),
					 VERSION,
					 copyright,
					 _("Nautilus is a graphical shell "
					   "for GNOME that makes it "
					   "easy to manage your files "
					   "and the rest of your system."),
					 authors,
					 NULL,
					 translator_credits,
					 NULL);
		gtk_window_set_transient_for (GTK_WINDOW (about), GTK_WINDOW (user_data));
		
		eel_add_weak_pointer (&about);
	}
	
	gtk_window_present (GTK_WINDOW (about));
}

static void
help_menu_nautilus_manual_callback (BonoboUIComponent *component, 
			              gpointer user_data, 
			              const char *verb)
{
	NautilusWindow *window;
	GError *error;
	GtkWidget *dialog;

	error = NULL;
	window = NAUTILUS_WINDOW (user_data);

	egg_help_display_desktop_on_screen (
		NULL, "user-guide", "wgosnautilus.xml", "gosnautilus-21",
		gtk_window_get_screen (GTK_WINDOW (window)), &error);

	if (error) {
		dialog = gtk_message_dialog_new (GTK_WINDOW (window),
						 GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR,
						 GTK_BUTTONS_OK,
						 _("There was an error displaying help: \n%s"),
						 error->message);
		g_signal_connect (G_OBJECT (dialog), "response",
				  G_CALLBACK (gtk_widget_destroy),
				  NULL);

		gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
		gtk_widget_show (dialog);
		g_error_free (error);
	}
}

/**
 * nautilus_window_initialize_menus
 * 
 * Create and install the set of menus for this window.
 * @window: A recently-created NautilusWindow.
 */
void 
nautilus_window_initialize_menus_part_1 (NautilusWindow *window)
{
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("New Window", file_menu_new_window_callback),
		BONOBO_UI_VERB ("Close", file_menu_close_window_callback),
		BONOBO_UI_VERB ("Connect to Server", connect_to_server_callback),
#ifdef HAVE_MEDUSA
		BONOBO_UI_VERB ("Find", file_menu_find_callback),
		BONOBO_UI_VERB ("Toggle Find Mode", toolbar_toggle_find_mode_callback),
#endif
		BONOBO_UI_VERB ("Undo", edit_menu_undo_callback),
		BONOBO_UI_VERB ("Backgrounds and Emblems", backgrounds_and_emblems_callback),
		BONOBO_UI_VERB ("Up", go_menu_up_callback),
		BONOBO_UI_VERB ("Home", go_menu_home_callback),
		BONOBO_UI_VERB ("Start Here", go_menu_start_here_callback),
		BONOBO_UI_VERB ("Go to Computer", go_menu_go_to_computer_callback),
		BONOBO_UI_VERB ("Go to Templates", go_menu_go_to_templates_callback),
		BONOBO_UI_VERB ("Go to Trash", go_menu_go_to_trash_callback),
		BONOBO_UI_VERB ("Go to Burn CD", go_menu_go_to_burn_cd_callback),
		BONOBO_UI_VERB ("Go to Location", go_menu_location_callback),
		BONOBO_UI_VERB ("Reload", view_menu_reload_callback),
		BONOBO_UI_VERB ("Zoom In", view_menu_zoom_in_callback),
		BONOBO_UI_VERB ("Zoom Out", view_menu_zoom_out_callback),
		BONOBO_UI_VERB ("Zoom Normal", view_menu_zoom_normal_callback),
		BONOBO_UI_VERB ("View as", view_menu_view_as_callback),

#ifdef ENABLE_PROFILER
		BONOBO_UI_VERB ("Start Profiling", nautilus_profiler_bonobo_ui_start_callback),
		BONOBO_UI_VERB ("Stop Profiling", nautilus_profiler_bonobo_ui_stop_callback),
		BONOBO_UI_VERB ("Reset Profiling", nautilus_profiler_bonobo_ui_reset_callback),
		BONOBO_UI_VERB ("Report Profiling", nautilus_profiler_bonobo_ui_report_callback),
#endif

		BONOBO_UI_VERB ("About Nautilus", help_menu_about_nautilus_callback),
		BONOBO_UI_VERB ("Nautilus Manual", help_menu_nautilus_manual_callback),
		BONOBO_UI_VERB ("Preferences", preferences_callback),
		BONOBO_UI_VERB ("Stop", stop_button_callback),

		BONOBO_UI_VERB_END
	};

	nautilus_window_ui_freeze (window);

	bonobo_ui_component_freeze (window->details->shell_ui, NULL);

	nautilus_window_update_show_hide_menu_items (window);

	bonobo_ui_component_add_verb_list_with_data (window->details->shell_ui, verbs, window);

	bonobo_ui_component_add_listener
		(window->details->shell_ui,
		 ID_SHOW_HIDE_STATUS_BAR,
		 view_menu_show_hide_statusbar_state_changed_callback, 
		 window);

	/* Register to catch Bonobo UI events so we can notice View As changes */
	g_signal_connect_object (window->details->shell_ui, "ui_event", 
				 G_CALLBACK (nautilus_window_handle_ui_event_callback), window, 0);

	bonobo_ui_component_thaw (window->details->shell_ui, NULL);
	
	if (!have_burn_uri ()) {
		nautilus_bonobo_set_hidden (window->details->shell_ui,
					    COMMAND_GO_BURN_CD,
					    TRUE);
	}
	
#ifndef ENABLE_PROFILER
	nautilus_bonobo_set_hidden (window->details->shell_ui, NAUTILUS_MENU_PATH_PROFILER, TRUE);
#endif

	nautilus_window_ui_thaw (window);
}

static GList *
get_extension_menus (NautilusWindow *window)
{
	GList *providers;
	GList *items;
	GList *l;
	
	providers = nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_MENU_PROVIDER);
	items = NULL;

	for (l = providers; l != NULL; l = l->next) {
		NautilusMenuProvider *provider;
		GList *file_items;
		
		provider = NAUTILUS_MENU_PROVIDER (l->data);
		file_items = nautilus_menu_provider_get_background_items (provider,
									  GTK_WIDGET (window),
									  window->details->viewed_file);
		items = g_list_concat (items, file_items);
	}

	nautilus_module_extension_list_free (providers);

	return items;
}

void
nautilus_window_load_extension_menus (NautilusWindow *window)
{
	GList *items;
	GList *l;
	
	nautilus_bonobo_remove_menu_items_and_commands
		(window->details->shell_ui, POPUP_PATH_EXTENSION_ACTIONS);
	nautilus_bonobo_remove_menu_items_and_commands
		(window->details->shell_ui, MENU_PATH_EXTENSION_ACTIONS);

	items = get_extension_menus (window);

	for (l = items; l != NULL; l = l->next) {
		NautilusMenuItem *item;
		
		item = NAUTILUS_MENU_ITEM (l->data);

		nautilus_bonobo_add_extension_item_command
			(window->details->shell_ui, item);
		
		nautilus_bonobo_add_extension_item
			(window->details->shell_ui, 
			 MENU_PATH_EXTENSION_ACTIONS,
			 item);

		nautilus_bonobo_add_extension_item
			(window->details->shell_ui, 
			 POPUP_PATH_EXTENSION_ACTIONS,
			 item);

		g_object_unref (item);
	}

	g_list_free (items);
}
