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
#include <libegg/egg-screen-help.h>
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

#define COMMAND_PATH_TOGGLE_FIND_MODE                   "/commands/Find"
#define COMMAND_PATH_TOGGLE_FIND_MODE_WITH_STATE        "/commands/Toggle Find Mode"

#define MENU_PATH_SHOW_HIDE_SIDEBAR			"/menu/View/Show Hide Placeholder/Show Hide Sidebar"
#define MENU_PATH_SHOW_HIDE_TOOLBAR			"/menu/View/Show Hide Placeholder/Show Hide Toolbar"
#define MENU_PATH_SHOW_HIDE_LOCATION_BAR		"/menu/View/Show Hide Placeholder/Show Hide Location Bar"
#define MENU_PATH_SHOW_HIDE_STATUS_BAR			"/menu/View/Show Hide Placeholder/Show Hide Statusbar"

#define MENU_PATH_HISTORY_PLACEHOLDER			"/menu/Go/History Placeholder"

#define MENU_PATH_BUILT_IN_BOOKMARKS_PLACEHOLDER	"/menu/Bookmarks/Built-in Bookmarks Placeholder"
#define MENU_PATH_BOOKMARKS_PLACEHOLDER			"/menu/Bookmarks/Bookmarks Placeholder"

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
#define BURN_CD_URI          "burn:"

#define RESPONSE_FORGET		1000

static GtkWindow *bookmarks_window = NULL;

static void                  append_bookmark_to_menu                       (NautilusWindow   *window,
									    NautilusBookmark *bookmark,
									    const char       *parent_path,
									    guint             index_in_parent,
									    gboolean          is_in_bookmarks_menu);
static void                  append_dynamic_bookmarks                      (NautilusWindow   *window);
static NautilusBookmarkList *get_bookmark_list                             (void);
static void                  refresh_go_menu                               (NautilusWindow   *window);
static void                  refresh_bookmarks_menu                        (NautilusWindow   *window);
static void                  schedule_refresh_go_menu                      (NautilusWindow   *window);
static void                  schedule_refresh_bookmarks_menu               (NautilusWindow   *window);
static void                  edit_bookmarks                                (NautilusWindow   *window);
static void                  add_bookmark_for_current_location             (NautilusWindow   *window);

/* Struct that stores all the info necessary to activate a bookmark. */
typedef struct {
        NautilusBookmark *bookmark;
        NautilusWindow *window;
        gboolean prompt_for_removal;
        guint changed_handler_id;
} BookmarkHolder;

static BookmarkHolder *
bookmark_holder_new (NautilusBookmark *bookmark, 
		     NautilusWindow *window,
		     gboolean is_bookmarks_menu)
{
	BookmarkHolder *new_bookmark_holder;

	new_bookmark_holder = g_new (BookmarkHolder, 1);
	new_bookmark_holder->window = window;
	new_bookmark_holder->bookmark = bookmark;
	/* Ref the bookmark because it might be unreffed away while 
	 * we're holding onto it (not an issue for window).
	 */
	g_object_ref (bookmark);
	new_bookmark_holder->prompt_for_removal = is_bookmarks_menu;

	new_bookmark_holder->changed_handler_id = 
		g_signal_connect_object (bookmark, "appearance_changed",
					 is_bookmarks_menu
					 ? G_CALLBACK (schedule_refresh_bookmarks_menu)
					 : G_CALLBACK (schedule_refresh_go_menu),
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
file_menu_new_window_callback (BonoboUIComponent *component, 
			       gpointer user_data, 
			       const char *verb)
{
	NautilusWindow *current_window;
	NautilusWindow *new_window;

	current_window = NAUTILUS_WINDOW (user_data);
	new_window = nautilus_application_create_window (
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
file_menu_close_all_windows_callback (BonoboUIComponent *component, 
			              gpointer user_data, 
			              const char *verb)
{
	nautilus_application_close_all_windows ();
}

static void
file_menu_burn_cd_callback (BonoboUIComponent *component, 
			    gpointer user_data, 
			    const char *verb)
{
	GError *error;
	char *argv[] = { "nautilus-cd-burner", NULL};
	char *text;

	error = NULL;
	if (!g_spawn_async (NULL,
			    argv, NULL,
			    G_SPAWN_SEARCH_PATH,
			    NULL, NULL,
			    NULL,
			    &error)) {
		text = g_strdup_printf (_("Unable to launch the cd burner application:\n%s"), error->message);
		eel_show_error_dialog (text,
				       _("Can't launch cd burner"),
				       GTK_WINDOW (user_data));
		g_free (text);
		g_error_free (error);
	}

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

static void
nautilus_window_show_location_bar_temporarily (NautilusWindow *window, 
					       gboolean in_search_mode)
{
	if (!nautilus_window_location_bar_showing (window)) {
		nautilus_window_show_location_bar (window, FALSE);
		window->details->temporary_navigation_bar = TRUE;
	}
	nautilus_window_set_search_mode 
		(window, in_search_mode);
	nautilus_switchable_navigation_bar_activate 
		(NAUTILUS_SWITCHABLE_NAVIGATION_BAR (window->navigation_bar));
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

	if (!window->details->updating_bonobo_state) {
		nautilus_window_show_location_bar_temporarily
			(window, FALSE);
	}
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
go_menu_back_callback (BonoboUIComponent *component, 
		       gpointer user_data, 
		       const char *verb) 
{
	nautilus_window_go_back (NAUTILUS_WINDOW (user_data));
}

static void
go_menu_forward_callback (BonoboUIComponent *component, 
			  gpointer user_data, 
			  const char *verb) 
{
	nautilus_window_go_forward (NAUTILUS_WINDOW (user_data));
}

static void
go_menu_up_callback (BonoboUIComponent *component, 
		     gpointer user_data, 
		     const char *verb) 
{
	nautilus_window_go_up (NAUTILUS_WINDOW (user_data));
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

	/* Confirm before forgetting history because it's a rare operation that
	 * is hard to recover from. We don't want people doing it accidentally
	 * when they intended to choose another Go menu item.
	 */
	if ((rand() % 10) == 0) {
		/* This is a little joke, shows up occasionally. I only
		 * implemented this feature so I could use this joke. 
		 */
		prompt = g_strdup (_("Are you sure you want to forget history? "
				     "If you do, you will be doomed to repeat it."));
	} else {
		prompt = g_strdup (_("Are you sure you want to clear the list "
				     "of locations you have visited?"));
	}
					   
	dialog = eel_create_question_dialog (prompt,
					     _("Clear History"), 
					     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
					     GTK_STOCK_CLEAR, RESPONSE_FORGET,
					     GTK_WINDOW (window));

	gtk_widget_show (GTK_WIDGET (dialog));

	g_free (prompt);					 
	
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
view_menu_reload_callback (BonoboUIComponent *component, 
			   gpointer user_data, 
			   const char *verb) 
{
	nautilus_window_reload (NAUTILUS_WINDOW (user_data));
}

static void
view_menu_show_hide_sidebar_state_changed_callback (BonoboUIComponent *component, 
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
		nautilus_window_show_sidebar (window);
	} else {
		nautilus_window_hide_sidebar (window);
	}
}

static void
view_menu_show_hide_toolbar_state_changed_callback (BonoboUIComponent *component, 
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
		nautilus_window_show_toolbar (window);
	} else {
		nautilus_window_hide_toolbar (window);
	}
}

static void
view_menu_show_hide_location_bar_state_changed_callback (BonoboUIComponent *component, 
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
		nautilus_window_show_location_bar (window, TRUE);
	} else {
		nautilus_window_hide_location_bar (window, TRUE);
	}
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
					  COMMAND_SHOW_HIDE_SIDEBAR,
					  nautilus_window_sidebar_showing (window));
	nautilus_bonobo_set_toggle_state (window->details->shell_ui,
					  COMMAND_SHOW_HIDE_TOOLBAR,
					  nautilus_window_toolbar_showing (window));
	nautilus_bonobo_set_toggle_state (window->details->shell_ui,
					  COMMAND_SHOW_HIDE_LOCATION_BAR,
					  nautilus_window_location_bar_showing (window));
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
bookmarks_menu_add_bookmark_callback (BonoboUIComponent *component, 
			              gpointer user_data, 
			              const char *verb)
{
        add_bookmark_for_current_location (NAUTILUS_WINDOW (user_data));
}

static void
bookmarks_menu_edit_bookmarks_callback (BonoboUIComponent *component, 
			                gpointer user_data, 
			                const char *verb)
{
        edit_bookmarks (NAUTILUS_WINDOW (user_data));
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
						 GTK_BUTTONS_CLOSE,
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
show_bogus_bookmark_window (BookmarkHolder *holder)
{
	GtkDialog *dialog;
	char *uri;
	char *uri_for_display;
	char *prompt;

	uri = nautilus_bookmark_get_uri (holder->bookmark);
	uri_for_display = eel_format_uri_for_display (uri);

	if (holder->prompt_for_removal) {
		prompt = g_strdup_printf (_("The location \"%s\" does not exist. Do you "
					    "want to remove any bookmarks with this "
					    "location from your list?"), uri_for_display);
		dialog = eel_show_yes_no_dialog (prompt,
						 _("Bookmark for Nonexistent Location"),
						 _("Remove"), GTK_STOCK_CANCEL,
						 GTK_WINDOW (holder->window));

		eel_gtk_signal_connect_free_data
			(GTK_OBJECT (dialog),
			 "response",
			 G_CALLBACK (remove_bookmarks_for_uri_if_yes),
			 g_strdup (uri));

		gtk_dialog_set_default_response (dialog, GTK_RESPONSE_NO);
	} else {
		prompt = g_strdup_printf (_("The location \"%s\" no longer exists."), uri_for_display);
		dialog = eel_show_info_dialog (prompt, _("Go to Nonexistent Location"), GTK_WINDOW (holder->window));
	}

	g_free (uri);
	g_free (uri_for_display);
	g_free (prompt);
}

static void
activate_bookmark_in_menu_item (BonoboUIComponent *component, gpointer user_data, const char *path)
{
        BookmarkHolder *holder;
        char *uri;

        holder = (BookmarkHolder *)user_data;

	if (nautilus_bookmark_uri_known_not_to_exist (holder->bookmark)) {
		show_bogus_bookmark_window (holder);
	} else {
	        uri = nautilus_bookmark_get_uri (holder->bookmark);
	        nautilus_window_go_to (holder->window, uri);
	        g_free (uri);
        }
}

static void
append_bookmark_to_menu (NautilusWindow *window, 
                         NautilusBookmark *bookmark, 
                         const char *parent_path,
			 guint index_in_parent,
                         gboolean is_bookmarks_menu)
{
	BookmarkHolder *bookmark_holder;		
	char *raw_name, *display_name, *truncated_name, *verb_name;
	char *ui_path;
	GdkPixbuf *pixbuf;

	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (NAUTILUS_IS_BOOKMARK (bookmark));

	nautilus_window_ui_freeze (window);

	bookmark_holder = bookmark_holder_new (bookmark, window, is_bookmarks_menu);

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
		(window->details->shell_ui, 
		 parent_path, 
		 index_in_parent, 
		 display_name, 
		 pixbuf);
	g_object_unref (pixbuf);
	g_free (display_name);
	
	/* Add the status tip */
	ui_path = nautilus_bonobo_get_numbered_menu_item_path
		(window->details->shell_ui, parent_path, index_in_parent);
	nautilus_bonobo_set_tip (window->details->shell_ui, ui_path, _("Go to the location specified by this bookmark"));
	g_free (ui_path);
			
	/* Add verb to new bookmark menu item */
	verb_name = nautilus_bonobo_get_numbered_menu_item_command 
		(window->details->shell_ui, parent_path, index_in_parent);
	bonobo_ui_component_add_verb_full (window->details->shell_ui, verb_name, 
					   g_cclosure_new (G_CALLBACK (activate_bookmark_in_menu_item),
							   bookmark_holder, 
							   bookmark_holder_free_cover));
	g_free (verb_name);

	nautilus_window_ui_thaw (window);
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
append_separator (NautilusWindow *window, const char *path)
{
	nautilus_window_ui_freeze (window);

	nautilus_bonobo_add_menu_separator (window->details->shell_ui, path);

	nautilus_window_ui_thaw (window);
}

static void
create_menu_item_from_node (NautilusWindow *window,
			    xmlNodePtr node,
			    const char *menu_path,
			    int *index)
{
	NautilusBookmark *bookmark;
	xmlChar *xml_folder_name;
	int sub_index;
	char *sub_menu_path, *escaped_name;
	
	g_assert (NAUTILUS_IS_WINDOW (window));
		
	if (node->type != XML_ELEMENT_NODE) {
		return;
	}

	nautilus_window_ui_freeze (window);

	if (strcmp (node->name, "bookmark") == 0) {
		bookmark = nautilus_bookmark_new_from_node (node);
		append_bookmark_to_menu (window, bookmark, menu_path, *index, TRUE);
		g_object_unref (bookmark);
	} else if (strcmp (node->name, "separator") == 0) {
		append_separator (window, menu_path);
	} else if (strcmp (node->name, "folder") == 0) {
		xml_folder_name = eel_xml_get_property_translated (node, "name");
		nautilus_bonobo_add_submenu (window->details->shell_ui, menu_path, xml_folder_name, NULL);

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

	nautilus_window_ui_thaw (window);
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

static NautilusBookmarkList *bookmarks = NULL;

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
add_bookmark_for_current_location (NautilusWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_bookmark_list_append (get_bookmark_list (), window->current_location_bookmark);
}

static void
edit_bookmarks (NautilusWindow *window)
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
	refresh_bookmarks_menu (NAUTILUS_WINDOW (user_data));
}

static void
refresh_bookmarks_menu (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	/* Unregister any pending call to this function. */
	nautilus_window_remove_bookmarks_menu_callback (window);

	g_object_ref (G_OBJECT (window));
	bonobo_ui_component_freeze (window->details->shell_ui, NULL);

	nautilus_window_remove_bookmarks_menu_items (window);

#ifdef WEB_NAVIGATION_ENABLED
	if (!eel_preferences_get_boolean (NAUTILUS_PREFERENCES_HIDE_BUILT_IN_BOOKMARKS)) {
		append_static_bookmarks (window, MENU_PATH_BUILT_IN_BOOKMARKS_PLACEHOLDER);
	}
#endif

	append_dynamic_bookmarks (window);

	bonobo_ui_component_thaw (window->details->shell_ui, NULL);
	g_object_unref (G_OBJECT (window));
}

/**
 * nautilus_window_initialize_bookmarks_menu
 * 
 * Fill in bookmarks menu with stored bookmarks, and wire up signals
 * so we'll be notified when bookmark list changes.
 */
static void 
nautilus_window_initialize_bookmarks_menu (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

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

/**
 * nautilus_window_initialize_go_menu
 * 
 * Wire up signals so we'll be notified when history list changes.
 */
static void 
nautilus_window_initialize_go_menu (NautilusWindow *window)
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
nautilus_window_initialize_menus_part_1 (NautilusWindow *window)
{
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("New Window", file_menu_new_window_callback),
		BONOBO_UI_VERB ("Close", file_menu_close_window_callback),
		BONOBO_UI_VERB ("Burn CD", file_menu_burn_cd_callback),
		BONOBO_UI_VERB ("Close All Windows", file_menu_close_all_windows_callback),
#ifdef HAVE_MEDUSA
		BONOBO_UI_VERB ("Find", file_menu_find_callback),
		BONOBO_UI_VERB ("Toggle Find Mode", toolbar_toggle_find_mode_callback),
#endif
		BONOBO_UI_VERB ("Undo", edit_menu_undo_callback),
		BONOBO_UI_VERB ("Backgrounds and Emblems", backgrounds_and_emblems_callback),
		BONOBO_UI_VERB ("Back", go_menu_back_callback),
		BONOBO_UI_VERB ("Forward", go_menu_forward_callback),
		BONOBO_UI_VERB ("Up", go_menu_up_callback),
		BONOBO_UI_VERB ("Home", go_menu_home_callback),
		BONOBO_UI_VERB ("Start Here", go_menu_start_here_callback),
		BONOBO_UI_VERB ("Go to Trash", go_menu_go_to_trash_callback),
		BONOBO_UI_VERB ("Go to Burn CD", go_menu_go_to_burn_cd_callback),
		BONOBO_UI_VERB ("Go to Location", go_menu_location_callback),
		BONOBO_UI_VERB ("Clear History", go_menu_forget_history_callback),
		BONOBO_UI_VERB ("Reload", view_menu_reload_callback),
		BONOBO_UI_VERB ("Zoom In", view_menu_zoom_in_callback),
		BONOBO_UI_VERB ("Zoom Out", view_menu_zoom_out_callback),
		BONOBO_UI_VERB ("Zoom Normal", view_menu_zoom_normal_callback),
		BONOBO_UI_VERB ("View as", view_menu_view_as_callback),
		BONOBO_UI_VERB ("Add Bookmark", bookmarks_menu_add_bookmark_callback),
		BONOBO_UI_VERB ("Edit Bookmarks", bookmarks_menu_edit_bookmarks_callback),

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
		 ID_SHOW_HIDE_SIDEBAR,
		 view_menu_show_hide_sidebar_state_changed_callback, 
		 window);
	bonobo_ui_component_add_listener
		(window->details->shell_ui,
		 ID_SHOW_HIDE_TOOLBAR,
		 view_menu_show_hide_toolbar_state_changed_callback, 
		 window); 
	bonobo_ui_component_add_listener
		(window->details->shell_ui,
		 ID_SHOW_HIDE_LOCATION_BAR,
		 view_menu_show_hide_location_bar_state_changed_callback, 
		 window);
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

#ifndef HAVE_MEDUSA
	nautilus_bonobo_set_hidden (window->details->shell_ui,
				    COMMAND_PATH_TOGGLE_FIND_MODE,
				    TRUE);
	nautilus_bonobo_set_hidden (window->details->shell_ui,
				    COMMAND_PATH_TOGGLE_FIND_MODE_WITH_STATE,
				    TRUE);
	/* Also set these items insensitive so that keyboard shortcuts do not trigger
	   warnings */
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       COMMAND_PATH_TOGGLE_FIND_MODE,
				       FALSE);
	nautilus_bonobo_set_sensitive (window->details->shell_ui,
				       COMMAND_PATH_TOGGLE_FIND_MODE_WITH_STATE,
				       FALSE);

#endif
	nautilus_window_ui_thaw (window);
}

void 
nautilus_window_initialize_menus_part_2 (NautilusWindow *window)
{
	nautilus_window_ui_freeze (window);

        nautilus_window_initialize_bookmarks_menu (window);
        nautilus_window_initialize_go_menu (window);

	nautilus_window_ui_thaw (window);
}

void
nautilus_window_remove_bookmarks_menu_callback (NautilusWindow *window)
{
        if (window->details->refresh_bookmarks_menu_idle_id != 0) {
                g_source_remove (window->details->refresh_bookmarks_menu_idle_id);
		window->details->refresh_bookmarks_menu_idle_id = 0;
        }
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
nautilus_window_remove_bookmarks_menu_items (NautilusWindow *window)
{
	nautilus_window_ui_freeze (window);

	nautilus_bonobo_remove_menu_items_and_commands
		(window->details->shell_ui, 
		 MENU_PATH_BUILT_IN_BOOKMARKS_PLACEHOLDER);					   
	nautilus_bonobo_remove_menu_items_and_commands 
		(window->details->shell_ui, 
		 MENU_PATH_BOOKMARKS_PLACEHOLDER);					   

	nautilus_window_ui_thaw (window);
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

static void
append_dynamic_bookmarks (NautilusWindow *window)
{
        NautilusBookmarkList *bookmarks;
	guint bookmark_count;
	guint index;
		
	g_assert (NAUTILUS_IS_WINDOW (window));

	bookmarks = get_bookmark_list ();

	/* append new set of bookmarks */
	bookmark_count = nautilus_bookmark_list_length (bookmarks);
	for (index = 0; index < bookmark_count; ++index) {
		append_bookmark_to_menu (window,
		                         nautilus_bookmark_list_item_at (bookmarks, index),
		                         MENU_PATH_BOOKMARKS_PLACEHOLDER,
		                         index,
		                         TRUE);
	}
}

static gboolean
refresh_bookmarks_menu_idle_callback (gpointer data)
{
	g_assert (NAUTILUS_IS_WINDOW (data));

	refresh_bookmarks_menu (NAUTILUS_WINDOW (data));

        /* Don't call this again (unless rescheduled) */
        return FALSE;
}

static void
schedule_refresh_bookmarks_menu (NautilusWindow *window)
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	if (window->details->refresh_bookmarks_menu_idle_id == 0) {
                window->details->refresh_bookmarks_menu_idle_id
                        = g_idle_add (refresh_bookmarks_menu_idle_callback,
				      window);
	}	
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
		append_bookmark_to_menu (window,
					 NAUTILUS_BOOKMARK (node->data),
					 MENU_PATH_HISTORY_PLACEHOLDER,
					 index,
					 FALSE);
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
