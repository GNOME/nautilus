/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
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

#include "nautilus-about.h"
#include "nautilus-application.h"
#include "nautilus-bookmark-list.h"
#include "nautilus-bookmark-parsing.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-property-browser.h"
#include "nautilus-signaller.h"
#include "nautilus-theme-selector.h"
#include "nautilus-window-manage-views.h"
#include "nautilus-window-private.h"
#include <gtk/gtkmain.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-debug.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-undo-manager.h>
#include <libnautilus-extensions/nautilus-user-level-manager.h>
#include <libnautilus-extensions/nautilus-xml-extensions.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <parser.h>
#include <xmlmemory.h>

/* FIXME: bugzilla.eazel.com 3497 set to 1 when the bug is fixed in bonobo */
#define BONOBO_WORKAROUND 0

#define STATIC_BOOKMARKS_FILE_NAME	"static_bookmarks.xml"

/* Private menu paths that components don't know about */
#define NAUTILUS_MENU_PATH_NEW_WINDOW_ITEM		"/menu/File/New Window"
#define NAUTILUS_MENU_PATH_CLOSE_ITEM			"/menu/File/Close"
#define NAUTILUS_MENU_PATH_CLOSE_ALL_WINDOWS_ITEM	"/menu/File/Close All Windows"
#define NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_FIND	"/menu/File/Separator before Find"
#define NAUTILUS_MENU_PATH_TOGGLE_FIND_MODE		"/menu/File/Toggle Find Mode"
#define NAUTILUS_MENU_PATH_GO_TO_WEB_SEARCH		"/menu/File/Go to Web Search"

#define NAUTILUS_MENU_PATH_UNDO_ITEM			"/menu/Edit/Undo"
#define NAUTILUS_MENU_PATH_SEPARATOR_AFTER_UNDO		"/menu/Edit/Separator after Undo"
#define NAUTILUS_MENU_PATH_SEPARATOR_AFTER_CLEAR	"/menu/Edit/Separator after Clear"
#define NAUTILUS_MENU_PATH_SEPARATOR_AFTER_SELECT_ALL	"/menu/Edit/Separator after Select All"

#define NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_SHOW_HIDE	"/menu/View/Separator before Show Hide"
#define NAUTILUS_MENU_PATH_SHOW_HIDE_SIDEBAR		"/menu/View/Show Hide Placeholder/Show Hide Sidebar"
#define NAUTILUS_MENU_PATH_SHOW_HIDE_TOOL_BAR		"/menu/View/Show Hide Placeholder/Show Hide Tool Bar"
#define NAUTILUS_MENU_PATH_SHOW_HIDE_LOCATION_BAR	"/menu/View/Show Hide Placeholder/Show Hide Location Bar"
#define NAUTILUS_MENU_PATH_SHOW_HIDE_STATUS_BAR		"/menu/View/Show Hide Placeholder/Show Hide Status Bar"

#define NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_ZOOM	"/menu/View/Separator before Zoom"

#define NAUTILUS_MENU_PATH_HOME_ITEM			"/menu/Go/Home"
#define NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_FORGET	"/menu/Go/Separator before Forget"
#define NAUTILUS_MENU_PATH_FORGET_HISTORY_ITEM		"/menu/Go/Forget History"

#define NAUTILUS_MENU_PATH_HISTORY_ITEMS_PLACEHOLDER	"/menu/Go/History Placeholder"
#define NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_HISTORY	"/menu/Go/Separator before History"

#define NAUTILUS_MENU_PATH_ADD_BOOKMARK_ITEM		"/menu/Bookmarks/Add Bookmark"
#define NAUTILUS_MENU_PATH_EDIT_BOOKMARKS_ITEM		"/menu/Bookmarks/Edit Bookmarks"
#define NAUTILUS_MENU_PATH_BOOKMARK_ITEMS_PLACEHOLDER	"/menu/Bookmarks/Bookmarks Placeholder"
#define NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_BOOKMARKS   "/menu/Bookmarks/Separator before Bookmarks"

#define NAUTILUS_MENU_PATH_ABOUT_ITEM			"/menu/Help/About Nautilus"
#define NAUTILUS_MENU_PATH_NAUTILUS_FEEDBACK		"/menu/Help/Nautilus Feedback"


#define SWITCH_TO_BEGINNER_VERB				"Switch to Beginner Level"
#define SWITCH_TO_INTERMEDIATE_VERB			"Switch to Intermediate Level"
#define SWITCH_TO_ADVANCED_VERB				"Switch to Advanced Level"

#define BEGINNER_ICON_NAME                              "nautilus/novice.png"
#define INTERMEDIATE_ICON_NAME                          "nautilus/intermediate.png"
#define ADVANCED_ICON_NAME                              "nautilus/expert.png"


static GtkWindow *bookmarks_window = NULL;

#ifdef UIH
static void                  activate_bookmark_in_menu_item                 (BonoboUIHandler        *uih,
									     gpointer                user_data,
									     const char             *path);
#endif
static void                  append_bookmark_to_menu                        (NautilusWindow         *window,
									     NautilusBookmark *bookmark,
									     const char             *menu_item_path,
									     gboolean		     is_in_bookmarks_menu);
static void		     append_dynamic_bookmarks 			    (NautilusWindow *window);
static void                  remove_bookmarks_after	                    (NautilusWindow         *window,
									     const char             *menu_path,
									     const char             *last_static_item_path);
static NautilusBookmarkList *get_bookmark_list                              (void);
static void                  refresh_go_menu                   		    (NautilusWindow         *window);
static void                  refresh_bookmarks_menu            	    	    (NautilusWindow         *window);
static void		     schedule_refresh_go_menu 		    	    (NautilusWindow 	    *window);
static void		     schedule_refresh_bookmarks_menu 	    	    (NautilusWindow 	    *window);
static void                  edit_bookmarks                                 (NautilusWindow         *window);




/* User level things */
static guint                 convert_verb_to_user_level               	    (const char             *verb);
static const char *          convert_user_level_to_path                     (guint                   user_level);
static char *                get_customize_user_level_settings_menu_string  (void);
static void                  update_user_level_menu_items                   (NautilusWindow         *window);
static char *                get_customize_user_level_string                (void);
static void		     switch_to_user_level 			    (NautilusWindow 	    *window, 
									     int 		     new_user_level);
static void		     update_preferences_dialog_title		    (void);

static const char * normal_menu_titles[] = {
	NAUTILUS_MENU_PATH_FILE_MENU,
	NAUTILUS_MENU_PATH_EDIT_MENU,
	NAUTILUS_MENU_PATH_VIEW_MENU,
	NAUTILUS_MENU_PATH_GO_MENU,
	NAUTILUS_MENU_PATH_BOOKMARKS_MENU,
	NAUTILUS_MENU_PATH_HELP_MENU
};

/* Struct that stores all the info necessary to activate a bookmark. */
typedef struct {
        NautilusBookmark *bookmark;
        NautilusWindow *window;
        gboolean prompt_for_removal;
} BookmarkHolder;

static BookmarkHolder *
bookmark_holder_new (NautilusBookmark *bookmark, 
		     NautilusWindow *window,
		     gboolean prompt_for_removal)
{
	BookmarkHolder *new_bookmark_holder;

	new_bookmark_holder = g_new (BookmarkHolder, 1);
	new_bookmark_holder->window = window;
	new_bookmark_holder->bookmark = bookmark;
	/* Ref the bookmark because it might be unreffed away while 
	 * we're holding onto it (not an issue for window).
	 */
	gtk_object_ref (GTK_OBJECT (bookmark));
	new_bookmark_holder->prompt_for_removal = prompt_for_removal;

	return new_bookmark_holder;
}

static void
bookmark_holder_free (BookmarkHolder *bookmark_holder)
{
	gtk_object_unref (GTK_OBJECT (bookmark_holder->bookmark));
	g_free (bookmark_holder);
}

/* Private menu definitions; others are in <libnautilus/nautilus-bonobo-ui.h>.
 * These are not part of the published set, either because they are
 * development-only or because we expect to change them and
 * don't want other code relying on their existence.
 */

#define NAUTILUS_MENU_PATH_CUSTOMIZE_ITEM			"/menu/Edit/Customization"
#define NAUTILUS_MENU_PATH_CHANGE_APPEARANCE_ITEM		"/menu/Edit/Change_Appearance"

#define NAUTILUS_MENU_PATH_USER_LEVEL				"/menu/Preferences"
#define NAUTILUS_MENU_PATH_NOVICE_ITEM				"/menu/Preferences/User Levels Placeholder/Switch to Beginner Level"
#define NAUTILUS_MENU_PATH_INTERMEDIATE_ITEM			"/menu/Preferences/User Levels Placeholder/Switch to Intermediate Level"
#define NAUTILUS_MENU_PATH_EXPERT_ITEM				"/menu/Preferences/User Levels Placeholder/Switch to Advanced Level"
#define NAUTILUS_MENU_PATH_AFTER_USER_LEVEL_SEPARATOR		"/menu/Preferences/After User Level Separator"
#define NAUTILUS_MENU_PATH_USER_LEVEL_CUSTOMIZE			"/menu/Preferences/User Level Customization"

#define NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_CANNED_BOOKMARKS	"/menu/Bookmarks/Before Canned Separator"

static void
file_menu_new_window_callback (BonoboUIComponent *component, 
			       gpointer user_data, 
			       const char *verb)
{
	NautilusWindow *current_window;
	NautilusWindow *new_window;

	current_window = NAUTILUS_WINDOW (user_data);
	new_window = nautilus_application_create_window (current_window->application);
	nautilus_window_goto_uri (new_window, current_window->location);
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
file_menu_toggle_find_mode_callback (BonoboUIComponent *component, 
			             gpointer user_data, 
			             const char *verb)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (user_data);

	nautilus_window_set_search_mode 
		(window, !nautilus_window_get_search_mode (window));
}

static void
file_menu_web_search_callback (BonoboUIComponent *component, 
			       gpointer user_data, 
			       const char *verb)
{
	nautilus_window_go_web_search (NAUTILUS_WINDOW (user_data));
}

static void
stop_button_callback (BonoboUIComponent *component, 
			       gpointer user_data, 
			       const char *verb)
{
	nautilus_window_stop_loading (NAUTILUS_WINDOW (user_data));
}

static void
services_button_callback (BonoboUIComponent *component, 
			       gpointer user_data, 
			       const char *verb)
{
	nautilus_window_goto_uri (NAUTILUS_WINDOW (user_data), "eazel:");
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
edit_menu_cut_callback (BonoboUIComponent *component, 
			gpointer user_data, 
			const char *verb) 
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);
	if (GTK_IS_EDITABLE (window->focus_widget)) {
		gtk_editable_cut_clipboard (GTK_EDITABLE (window->focus_widget));
	}
}

static void
edit_menu_copy_callback (BonoboUIComponent *component, 
			 gpointer user_data, 
			 const char *verb) 
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);
	if (GTK_IS_EDITABLE (window->focus_widget)) {
		gtk_editable_copy_clipboard (GTK_EDITABLE (window->focus_widget));
	}
}


static void
edit_menu_paste_callback (BonoboUIComponent *component, 
			  gpointer user_data, 
			  const char *verb) 
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);
	if (GTK_IS_EDITABLE (window->focus_widget)) {
		gtk_editable_paste_clipboard (GTK_EDITABLE (window->focus_widget));
	}

}

static void
edit_menu_clear_callback (BonoboUIComponent *component, 
			  gpointer user_data, 
			  const char *verb) 
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);
	if (GTK_IS_EDITABLE (window->focus_widget)) {
		/* A negative index deletes until the end of the string */
		gtk_editable_delete_text (GTK_EDITABLE (window->focus_widget),0, -1);
	}

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
forget_history_if_confirmed (NautilusWindow *window)
{
	GnomeDialog *dialog;
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
		prompt = g_strdup (_("Are you sure you want Nautilus to forget "
				     "which locations you have visited?"));
	}
					   
	dialog = nautilus_yes_no_dialog (prompt,
					 _("Forget History?"),
					 _("Forget"),
					 GNOME_STOCK_BUTTON_CANCEL,
					 GTK_WINDOW (window));
	g_free (prompt);					 

	gtk_signal_connect
		(GTK_OBJECT (nautilus_gnome_dialog_get_button_by_index
			     (dialog, GNOME_OK)),
		 "clicked",
		 nautilus_forget_history,
		 NULL);

	gnome_dialog_set_default (dialog, GNOME_CANCEL);
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
view_menu_show_hide_sidebar_callback (BonoboUIComponent *component, 
			              gpointer user_data, 
			              const char *verb) 
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (user_data);
	if (nautilus_window_sidebar_showing (window)) {
		nautilus_window_hide_sidebar (window);
	} else {
		nautilus_window_show_sidebar (window);
	}
}

static void
view_menu_show_hide_tool_bar_callback (BonoboUIComponent *component, 
			               gpointer user_data, 
			               const char *verb) 
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (user_data);
	if (nautilus_window_tool_bar_showing (window)) {
		nautilus_window_hide_tool_bar (window);
	} else {
		nautilus_window_show_tool_bar (window);
	}
}

static void
view_menu_show_hide_location_bar_callback (BonoboUIComponent *component, 
			                   gpointer user_data, 
			                   const char *verb) 
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (user_data);
	if (nautilus_window_location_bar_showing (window)) {
		nautilus_window_hide_location_bar (window);
	} else {
		nautilus_window_show_location_bar (window);
	}
}

static void
view_menu_show_hide_status_bar_callback (BonoboUIComponent *component, 
			                 gpointer user_data, 
			                 const char *verb) 
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (user_data);
	if (nautilus_window_status_bar_showing (window)) {
		nautilus_window_hide_status_bar (window);
	} else {
		nautilus_window_show_status_bar (window);
	}
}

void
nautilus_window_update_show_hide_menu_items (NautilusWindow *window) 
{
	g_assert (NAUTILUS_IS_WINDOW (window));

	bonobo_ui_component_freeze (window->details->shell_ui, NULL);
	nautilus_bonobo_set_label (window->details->shell_ui,
				   NAUTILUS_MENU_PATH_SHOW_HIDE_STATUS_BAR,
				   nautilus_window_status_bar_showing (window)
				   ? _("Hide Status Bar")
				   : _("Show Status Bar"));
		
	nautilus_bonobo_set_label (window->details->shell_ui,
				   NAUTILUS_MENU_PATH_SHOW_HIDE_SIDEBAR,
				   nautilus_window_sidebar_showing (window)
				   ? _("Hide Sidebar")
				   : _("Show Sidebar"));

	nautilus_bonobo_set_label (window->details->shell_ui,
				   NAUTILUS_MENU_PATH_SHOW_HIDE_TOOL_BAR,
				   nautilus_window_tool_bar_showing (window)
				   ? _("Hide Tool Bar")
				   : _("Show Tool Bar"));
		
	nautilus_bonobo_set_label (window->details->shell_ui,
				   NAUTILUS_MENU_PATH_SHOW_HIDE_LOCATION_BAR,
				   nautilus_window_location_bar_showing (window)
				   ? _("Hide Location Bar")
				   : _("Show Location Bar"));
	bonobo_ui_component_thaw (window->details->shell_ui, NULL);
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
	nautilus_window_zoom_to_fit (NAUTILUS_WINDOW (user_data));
}

static void
bookmarks_menu_add_bookmark_callback (BonoboUIComponent *component, 
			              gpointer user_data, 
			              const char *verb)
{
        nautilus_window_add_bookmark_for_current_location (NAUTILUS_WINDOW (user_data));
}

static void
bookmarks_menu_edit_bookmarks_callback (BonoboUIComponent *component, 
			                gpointer user_data, 
			                const char *verb)
{
        edit_bookmarks (NAUTILUS_WINDOW (user_data));
}

static void
switch_and_show_intermediate_settings_callback (GtkWidget *button, gpointer user_data)
{
	g_assert (NAUTILUS_IS_WINDOW (user_data));

	switch_to_user_level (NAUTILUS_WINDOW (user_data), NAUTILUS_USER_LEVEL_INTERMEDIATE);
	nautilus_global_preferences_show_dialog ();
}

static void
user_level_customize_callback (BonoboUIComponent *component, 
			       gpointer user_data, 
			       const char *verb)
{
	GnomeDialog *dialog;
	NautilusWindow *window;
	char *novice_level_name;
	char *intermediate_level_name;
	char *expert_level_name;
	char *prompt;
	char *dialog_title;

	window = NAUTILUS_WINDOW (user_data);

	if (nautilus_user_level_manager_get_user_level () == NAUTILUS_USER_LEVEL_NOVICE) {
		novice_level_name = 
			nautilus_user_level_manager_get_user_level_name_for_display 
				(NAUTILUS_USER_LEVEL_NOVICE);
		intermediate_level_name = 
			nautilus_user_level_manager_get_user_level_name_for_display 
				(NAUTILUS_USER_LEVEL_INTERMEDIATE);
		expert_level_name = 
			nautilus_user_level_manager_get_user_level_name_for_display 
				(NAUTILUS_USER_LEVEL_HACKER);
		/* Localizers: This is the message in the dialog that appears if the user chooses "Edit Beginner Settings".
		 * The first %s is the name of the lowest user level ("Beginner"). The 2nd and 4th %s are the
		 * names of the middle user level ("Intermediate"). The 3rd %s is the name of the highest user
		 * level ("Advanced").
		 */
		prompt = g_strdup_printf (_("None of the settings for the %s level can be edited. "
					    "If you want to edit the settings you must choose the %s or %s "
					    "level. Do you want to switch to the %s level now "
					    "and edit its settings?"),
					    novice_level_name,
					    intermediate_level_name,
					    expert_level_name,
					    intermediate_level_name);

		/* Localizers: This is the title of the dialog that appears if the user chooses "Edit Beginner Settings".
		 * The %s is the name of the middle user level ("Intermediate").
		 */
		dialog_title = g_strdup_printf (_("Switch to %s Level?"), intermediate_level_name);
		dialog = nautilus_yes_no_dialog (prompt,
						 dialog_title,
						 _("Switch"),
						 GNOME_STOCK_BUTTON_CANCEL,
						 GTK_WINDOW (window));
		gnome_dialog_button_connect 
			(dialog, GNOME_OK, switch_and_show_intermediate_settings_callback, window);
		gnome_dialog_set_default (dialog, GNOME_CANCEL);

		g_free (prompt);
		g_free (dialog_title);
		g_free (novice_level_name);
		g_free (intermediate_level_name);
		g_free (expert_level_name);
	} else {
		nautilus_global_preferences_show_dialog ();
	}
}

static void
customize_callback (BonoboUIComponent *component, 
		    gpointer user_data, 
		    const char *verb)
{
	nautilus_property_browser_show ();
}

static void
change_appearance_callback (BonoboUIComponent *component, 
			    gpointer user_data, 
			    const char *verb)
{
	nautilus_theme_selector_show ();
}

static void
help_menu_about_nautilus_callback (BonoboUIComponent *component, 
			           gpointer user_data, 
			           const char *verb)
{
	static GtkWidget *about = NULL;

	if (about == NULL) {

		const char *authors[] = {
			"Ali Abdin",
			"Darin Adler",
			"Josh Barrow",
			"Pavel Císler",
			"J Shane Culpepper",
			"Mike Engber",
			"Ramiro Estrugo",
			"Mike Fleming",
			"Andy Hertzfeld",
			"Susan Kare",
			"Mathieu Lacage",
			"George Lebl",
			"Elliot Lee",
			"Raph Levien",
			"Ian McKellar",
			"Seth Nickell",
			"Eskil Heyn Olsen",
			"Ettore Perazzoli",
			"Robey Pointer",
			"Gene Z. Ragan",
			"Arlo Rose",
			"Rebecca Schulman",
			"Robin * Slomkowski",
			"Maciej Stachowiak",
			"John Sullivan",
			"Bud Tribble",
			NULL
		};

		about = nautilus_about_new(_("Nautilus"),
					VERSION,
					"(C) 1999-2000 Eazel, Inc.",
					authors,
					_("Nautilus is a graphical shell \nfor GNOME that makes it \neasy to manage your files \nand the rest of your system."),
					NAUTILUS_TIMESTAMP);
	} else {
		nautilus_about_update_authors (NAUTILUS_ABOUT (about));
	}
	
	nautilus_gtk_window_present (GTK_WINDOW (about));
}

static void
help_menu_nautilus_feedback_callback (BonoboUIComponent *component, 
			              gpointer user_data, 
			              const char *verb)
{
	nautilus_window_goto_uri (NAUTILUS_WINDOW (user_data), "http://www.eazel.com/feedback.html");
}

/* utility routine to return an image corresponding to the passed-in user level */

static char *
get_user_level_icon_name (int user_level, gboolean is_selected)
{
	const char *image_name;
	char *full_image_name;
	
	switch (user_level) {
	case NAUTILUS_USER_LEVEL_NOVICE:
		image_name = "novice";
		break;
	case NAUTILUS_USER_LEVEL_HACKER:
		image_name = "expert";
		break;
	case NAUTILUS_USER_LEVEL_INTERMEDIATE:
	default:
		image_name = "intermediate";
		break;
	}
	
	if (is_selected) {
		full_image_name = g_strdup_printf ("nautilus/%s-selected.png", image_name);
	} else {
		full_image_name = g_strdup_printf ("nautilus/%s.png", image_name);	
	}
	
	return full_image_name;
}

/* handle user level changes */
static void
switch_to_user_level (NautilusWindow *window, int new_user_level)
{
	char *old_user_level_icon_name;
	int old_user_level;
	char *new_user_level_icon_name;
	char *new_user_level_icon_name_selected;

	if (window->details->shell_ui == NULL) {
		return;
	}

	old_user_level = nautilus_user_level_manager_get_user_level ();
	if (new_user_level == old_user_level) {
		return;
	}

	nautilus_user_level_manager_set_user_level (new_user_level);
	
	bonobo_ui_component_freeze (window->details->shell_ui, NULL);
	/* change the item icons to reflect the new user level */
	old_user_level_icon_name = get_user_level_icon_name (old_user_level, FALSE);
	nautilus_bonobo_set_icon (window->details->shell_ui,
				  convert_user_level_to_path (old_user_level),
				  old_user_level_icon_name);
	g_free (old_user_level_icon_name);
	
	new_user_level_icon_name_selected = get_user_level_icon_name (new_user_level, TRUE);
	nautilus_bonobo_set_icon (window->details->shell_ui,
				  convert_user_level_to_path (new_user_level),
				  new_user_level_icon_name_selected);
	g_free (new_user_level_icon_name_selected);
	

	/* set up the menu title image to reflect the new user level */
	new_user_level_icon_name = get_user_level_icon_name (new_user_level, FALSE);
#if BONOBO_WORKAROUND
	/* the line below is disabled because of a bug in bonobo. */
	nautilus_bonobo_set_icon (window->details->shell_ui,
				  NAUTILUS_MENU_PATH_USER_LEVEL,
				  new_user_level_icon_name);
#endif
	g_free (new_user_level_icon_name);
	bonobo_ui_component_thaw (window->details->shell_ui, NULL);
} 
 
static void
user_level_menu_item_callback (BonoboUIComponent *component, 
			       gpointer user_data, 
			       const char *verb)
{
	NautilusWindow *window;

	window = NAUTILUS_WINDOW (user_data);
		
	switch_to_user_level (window, convert_verb_to_user_level (verb));
}

static void
remove_bookmarks_for_uri (GtkWidget *button, gpointer callback_data)
{
	const char *uri;

	g_assert (GTK_IS_WIDGET (button));
	g_assert (callback_data != NULL);

	uri = (const char *)callback_data;

	nautilus_bookmark_list_delete_items_with_uri (get_bookmark_list (), uri);
}

static void
show_bogus_bookmark_window (BookmarkHolder *holder)
{
	GnomeDialog *dialog;
	char *uri;
	char *uri_for_display;
	char *prompt;

	uri = nautilus_bookmark_get_uri (holder->bookmark);
	uri_for_display = nautilus_format_uri_for_display (uri);

	if (holder->prompt_for_removal) {
		prompt = g_strdup_printf (_("The location \"%s\" does not exist. Do you "
					    "want to remove any bookmarks with this "
					    "location from your list?"), uri_for_display);
		dialog = nautilus_yes_no_dialog (prompt,
						 _("Bookmark for Nonexistent Location"),
						 _("Remove"),
						 GNOME_STOCK_BUTTON_CANCEL,
						 GTK_WINDOW (holder->window));

		nautilus_gtk_signal_connect_free_data
			(GTK_OBJECT (nautilus_gnome_dialog_get_button_by_index
				     (dialog, GNOME_OK)),
			 "clicked",
			 remove_bookmarks_for_uri,
			 g_strdup (uri));

		gnome_dialog_set_default (dialog, GNOME_CANCEL);
	} else {
		prompt = g_strdup_printf (_("The location \"%s\" no longer exists."), uri_for_display);
		dialog = nautilus_info_dialog (prompt, _("Go to Nonexistent Location"), GTK_WINDOW (holder->window));
	}

	g_free (uri);
	g_free (uri_for_display);
	g_free (prompt);
}

#ifdef UIH

static void
activate_bookmark_in_menu_item (BonoboUIHandler *uih, gpointer user_data, const char *path)
{
        BookmarkHolder *holder;
        char *uri;

        holder = (BookmarkHolder *)user_data;

	if (nautilus_bookmark_uri_known_not_to_exist (holder->bookmark)) {
		show_bogus_bookmark_window (holder);
	} else {
	        uri = nautilus_bookmark_get_uri (holder->bookmark);
	        nautilus_window_goto_uri (holder->window, uri);
	        g_free (uri);
        }
}

#endif

static void
append_bookmark_to_menu (NautilusWindow *window, 
                         NautilusBookmark *bookmark, 
                         const char *menu_item_path,
                         gboolean is_bookmarks_menu)
{
	BookmarkHolder *bookmark_holder;	
	GdkPixbuf *pixbuf;
#ifdef UIH
	BonoboUIHandlerPixmapType pixmap_type;
#endif
	char *raw_name, *display_name, *truncated_name;

	g_assert (NAUTILUS_IS_WINDOW (window));
	g_assert (NAUTILUS_IS_BOOKMARK (bookmark));

	pixbuf = nautilus_bookmark_get_pixbuf (bookmark, NAUTILUS_ICON_SIZE_FOR_MENUS);

	/* Set up pixmap type based on result of function.  If we fail, set pixmap type to none */
#ifdef UIH
	if (pixbuf != NULL) {
		pixmap_type = BONOBO_UI_HANDLER_PIXMAP_PIXBUF_DATA;
	} else {
		pixmap_type = BONOBO_UI_HANDLER_PIXMAP_NONE;
	}
#endif

	bookmark_holder = bookmark_holder_new (bookmark, window, is_bookmarks_menu);

	/* We double the underscores here to escape them so Bonobo will know they are
	 * not keyboard accelerator character prefixes. If we ever find we need to
	 * escape more than just the underscores, we'll add a menu helper function
	 * instead of a string utility. (Like maybe escaping control characters.)
	 */
	raw_name = nautilus_bookmark_get_name (bookmark);
	truncated_name = nautilus_truncate_text_for_menu_item (raw_name);
	display_name = nautilus_str_double_underscores (truncated_name);
	g_free (raw_name);
	g_free (truncated_name);
#ifdef UIH
 	bonobo_ui_handler_menu_new_item (window->ui_handler,
					 menu_item_path,
					 display_name,
					 _("Go to the specified location"),
					 -1,
					 pixmap_type,
					 pixbuf,
					 0,
					 0,
					 NULL,
					 NULL);
#endif
	g_free (display_name);

	/* We must use "set_callback" since we have a destroy-notify function. */
#ifdef UIH
	bonobo_ui_handler_menu_set_callback
		(window->ui_handler, menu_item_path,
		 activate_bookmark_in_menu_item,
		 bookmark_holder, (GDestroyNotify) bookmark_holder_free);
#endif

	/* Let's get notified whenever a bookmark changes. */
	gtk_signal_connect_object (GTK_OBJECT (bookmark), "changed",
				   is_bookmarks_menu
				   ? schedule_refresh_bookmarks_menu
				   : schedule_refresh_go_menu,
				   GTK_OBJECT (window));
}

static char *
get_static_bookmarks_file_path (void)
{
	char *xml_file_path, *user_directory_path;
	
	/* first, try to fetch it from the service update directory. Use the one from there
	 * if there is one, otherwise, get the built-in one from shared data
	 */
	
	user_directory_path = nautilus_get_user_directory ();
	xml_file_path = g_strdup_printf ("%s/updates/%s", user_directory_path, STATIC_BOOKMARKS_FILE_NAME);
	g_free (user_directory_path);
	if (g_file_exists (xml_file_path)) {
		return xml_file_path;
	}
	
	g_free (xml_file_path);
	
	xml_file_path = nautilus_make_path (NAUTILUS_DATADIR, STATIC_BOOKMARKS_FILE_NAME);
	if (g_file_exists (xml_file_path)) {
		return xml_file_path;
	}
	g_free (xml_file_path);

	return NULL;
}

static void
append_separator (NautilusWindow *window, const char *path)
{
#ifdef UIH
	/* Need to implement this for the built-in bookmarks */
#endif
}

static char *
create_menu_item_from_node (NautilusWindow *window,
			     xmlNodePtr node, 
			     const char *menu_path,
			     int index)
{
	NautilusBookmark *bookmark;
	xmlChar *xml_folder_name;
	int sub_index;
	char *index_as_string;
	char *item_path;
	char *sub_item_path;

	g_assert (NAUTILUS_IS_WINDOW (window));
	
	index_as_string = g_strdup_printf ("item_%d", index);
#ifdef UIH
	item_path = bonobo_ui_handler_build_path (menu_path, index_as_string, NULL);
#else
	item_path = NULL;
#endif
	g_free (index_as_string);

	if (strcmp (node->name, "bookmark") == 0) {
		bookmark = nautilus_bookmark_new_from_node (node);
		append_bookmark_to_menu (window, bookmark, item_path, TRUE);
		gtk_object_unref (GTK_OBJECT (bookmark));
	} else if (strcmp (node->name, "separator") == 0) {
		append_separator (window, item_path);
	} else if (strcmp (node->name, "folder") == 0) {
		xml_folder_name = xmlGetProp (node, "name");
#ifdef UIH
	 	bonobo_ui_handler_menu_new_subtree (window->ui_handler,
						    item_path,
						    xml_folder_name,
						    NULL,
						    -1,
						    BONOBO_UI_HANDLER_PIXMAP_NONE,
						    NULL,
						    0,
						    0);
#endif
		for (node = nautilus_xml_get_children (node), sub_index = 0;
		     node != NULL;
		     node = node->next, ++sub_index) {
			sub_item_path = create_menu_item_from_node (window, node, item_path, sub_index);
			g_free (sub_item_path);
		}
		xmlFree (xml_folder_name);
	} else {
		g_message ("found unknown node '%s', ignoring", node->name);
	}

	return item_path;
}

static void
append_static_bookmarks (NautilusWindow *window, const char *menu_path)
{
	xmlDocPtr doc;
	xmlNodePtr node;
	char *file_path;
	char *item_path;
	int index;

	/* Walk through XML tree creating bookmarks, folders, and separators. */
	file_path = get_static_bookmarks_file_path ();

	if (file_path == NULL) {
		return;
	}
	
	doc = xmlParseFile (file_path);
	g_free (file_path);

	node = nautilus_xml_get_root_children (doc);
	index = 0;
	
	if (node != NULL) {
		append_separator (window, NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_CANNED_BOOKMARKS);
	}

	for (index = 0; node != NULL; node = node->next, ++index) {
		item_path = create_menu_item_from_node 
			(window, node, menu_path, index);
		g_free (item_path);
	}
	
	xmlFreeDoc(doc);
}

/**
 * remove_bookmarks_after
 * 
 * Remove bookmark menu items from the end of this window's menu. Each removed
 * item should have callback data that's either NULL or is a BookmarkHolder *.
 * @window: The NautilusWindow whose menu should be cleaned.
 * @menu_path: The BonoboUIHandler-style path for the menu to be cleaned (e.g. "/Go").
 * @last_retained_item_path: The BonoboUIHandler-style path for the last menu item to
 * leave in place (e.g. "/Go/Home"). All menu items after this one will be removed.
 */
static void
remove_bookmarks_after (NautilusWindow *window, 
                        const char *menu_path, 
                        const char *last_retained_item_path)
{
#ifdef UIH
	GList *children, *p;
	gboolean found_items_to_remove;
	gpointer callback_data;
	BookmarkHolder *bookmark_holder;

	g_assert (NAUTILUS_IS_WINDOW (window));

	children = bonobo_ui_handler_menu_get_child_paths (window->ui_handler, menu_path);
	found_items_to_remove = FALSE;

	for (p = children; p != NULL; p = p->next) {
                if (found_items_to_remove) {
                	bonobo_ui_handler_menu_get_callback (window->ui_handler, p->data, 
                					     NULL, &callback_data, NULL);
                	bookmark_holder = (BookmarkHolder *)callback_data;
			/* bookmark_holder is NULL for separator in Bookmarks menu */
                	if (bookmark_holder != NULL) {
	                	gtk_signal_disconnect_by_data (GTK_OBJECT (bookmark_holder->bookmark), window);
                	}
                        bonobo_ui_handler_menu_remove (window->ui_handler, p->data);
		} else if (strcmp ((const char *) p->data, last_retained_item_path) == 0) {
			found_items_to_remove = TRUE;
		}
	}

	g_assert (found_items_to_remove);
        nautilus_g_list_free_deep (children);
#endif
}

static NautilusBookmarkList *bookmarks = NULL;

static void
free_bookmark_list (void)
{
	gtk_object_unref (GTK_OBJECT (bookmarks));
}

static NautilusBookmarkList *
get_bookmark_list (void)
{
        if (bookmarks == NULL) {
                bookmarks = nautilus_bookmark_list_new ();
		g_atexit (free_bookmark_list);
        }

        return bookmarks;
}

static GtkWindow *
get_or_create_bookmarks_window (GtkObject *undo_manager_source)
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
 * nautilus_window_add_bookmark_for_current_location
 * 
 * Add a bookmark for the displayed location to the bookmarks menu.
 * Does nothing if there's already a bookmark for the displayed location.
 */
void
nautilus_window_add_bookmark_for_current_location (NautilusWindow *window)
{
	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	nautilus_bookmark_list_append (get_bookmark_list (), window->current_location_bookmark);
}

static void
edit_bookmarks (NautilusWindow *window)
{
        nautilus_gtk_window_present
		(get_or_create_bookmarks_window (GTK_OBJECT (window)));
}

static void
refresh_bookmarks_menu (NautilusWindow *window)
{
	/* Unregister any pending call to this function. */
	nautilus_window_remove_bookmarks_menu_callback (window);

	nautilus_window_remove_bookmarks_menu_items (window);				

	if (!nautilus_preferences_get_boolean (NAUTILUS_PREFERENCES_HIDE_BUILT_IN_BOOKMARKS, 
					       FALSE)) {
		append_static_bookmarks (window, NAUTILUS_MENU_PATH_BOOKMARKS_MENU);
	}

	append_dynamic_bookmarks (window);
}

static char *
get_menu_title (const char *path)
{
	if (strcmp (path, NAUTILUS_MENU_PATH_FILE_MENU) == 0) {
		return g_strdup (_("_File"));
	}

	if (strcmp (path, NAUTILUS_MENU_PATH_EDIT_MENU) == 0) {
		return g_strdup (_("_Edit"));
	}

	if (strcmp (path, NAUTILUS_MENU_PATH_VIEW_MENU) == 0) {
		return g_strdup (_("_View"));
	}

	if (strcmp (path, NAUTILUS_MENU_PATH_GO_MENU) == 0) {
		return g_strdup (_("_Go"));
	}

	if (strcmp (path, NAUTILUS_MENU_PATH_BOOKMARKS_MENU) == 0) {
		return g_strdup (_("_Bookmarks"));
	}

	if (strcmp (path, NAUTILUS_MENU_PATH_HELP_MENU) == 0) {
		return g_strdup (_("_Help"));
	}

	g_assert_not_reached ();

	return NULL;
}


static void
remove_underline_accelerator_from_menu_title (NautilusWindow *window, 
					      const char *menu_path)
{
#ifdef UIH
	char *old_label;
	char *new_label;

	old_label = bonobo_ui_handler_menu_get_label (window->ui_handler, menu_path);
	new_label = nautilus_str_strip_chr (old_label, '_');
	bonobo_ui_handler_menu_set_label (window->ui_handler, menu_path, new_label);

	g_free (old_label);
	g_free (new_label);
#endif
}					      

/**
 * nautilus_window_disable_keyboard_navigation_for_menus
 * 
 * Prevents alt-key shortcuts from pulling down menus.
 */
void
nautilus_window_disable_keyboard_navigation_for_menus (NautilusWindow *window)
{
	int index;

	for (index = 0; index < NAUTILUS_N_ELEMENTS (normal_menu_titles); ++index) {
		remove_underline_accelerator_from_menu_title (window, normal_menu_titles[index]);
	}
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
	nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_HIDE_BUILT_IN_BOOKMARKS,
					   (NautilusPreferencesCallback)refresh_bookmarks_menu,
					   window);
		
	/* Recreate dynamic part of menu if bookmark list changes */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (get_bookmark_list ()),
			                       "contents_changed",
			                       schedule_refresh_bookmarks_menu,
			   	               GTK_OBJECT (window));

	/* Recreate static & dynamic parts of menu if icon theme changes */
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       schedule_refresh_bookmarks_menu,
					       GTK_OBJECT (window));
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
	gtk_signal_connect_object_while_alive (GTK_OBJECT (nautilus_signaller_get_current ()),
			                       "history_list_changed",
			                       schedule_refresh_go_menu,
			   	               GTK_OBJECT (window));
	 
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       schedule_refresh_go_menu,
					       GTK_OBJECT (window));

}

static void
new_top_level_menu (NautilusWindow *window, 
		    const char *menu_path)
{
	char *title;

	title = get_menu_title (menu_path);

	/* Note that we don't bother with hints for menu titles.
	 * We can revisit this anytime if someone thinks they're useful.
	 */
#ifdef UIH
	bonobo_ui_handler_menu_new_subtree (window->ui_handler,
					    menu_path,
					    title,
					    NULL,
					    -1,
					    BONOBO_UI_HANDLER_PIXMAP_NONE,
					    NULL,
					    0,
					    0);
#endif

	g_free (title);
}				    

/* handler to receive the user_level_changed signal, so we can update the menu and dialog
   when the user level changes */
static void
user_level_changed_callback (GtkObject	*user_level_manager,
			     gpointer	user_data)
{
	g_return_if_fail (user_data != NULL);
	g_return_if_fail (NAUTILUS_IS_WINDOW (user_data));

	update_user_level_menu_items (NAUTILUS_WINDOW (user_data));

	/* Hide the customize dialog for novice user level */
	if (nautilus_user_level_manager_get_user_level () == NAUTILUS_USER_LEVEL_NOVICE) {
		nautilus_global_preferences_hide_dialog ();
	}
	/* Otherwise update its title to reflect the user level */
	else {
		update_preferences_dialog_title ();
	}
}

static void
nautilus_window_create_top_level_menus (NautilusWindow *window)
{
	int index;

	for (index = 0; index < NAUTILUS_N_ELEMENTS (normal_menu_titles); ++index) {
		new_top_level_menu (window, normal_menu_titles[index]);
	}
}

static void
add_user_level_menu_item (NautilusWindow *window, 
			  const char *menu_path, 
			  guint user_level)
{

        guint current_user_level;
        char *icon_name;

	if (window->details->shell_ui == NULL) {
		return;
	}

	current_user_level = nautilus_user_level_manager_get_user_level ();

	icon_name = get_user_level_icon_name (user_level, current_user_level == user_level);


	nautilus_bonobo_set_icon (window->details->shell_ui,
				  menu_path,
				  icon_name);
	

	g_free (icon_name);
}

/**
 * nautilus_window_initialize_menus
 * 
 * Create and install the set of menus for this window.
 * @window: A recently-created NautilusWindow.
 */
void 
nautilus_window_initialize_menus (NautilusWindow *window)
{
#ifdef UIH
        GdkPixbuf *pixbuf;
	BonoboUIHandler *ui_handler;
#endif
	char *new_user_level_icon_name;
	guint new_user_level;

	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("New Window", file_menu_new_window_callback),
		BONOBO_UI_VERB ("Close", file_menu_close_window_callback),
		BONOBO_UI_VERB ("Close All", file_menu_close_all_windows_callback),
		BONOBO_UI_VERB ("Toggle Find Mode", file_menu_toggle_find_mode_callback),
		BONOBO_UI_VERB ("Go to Web Search", file_menu_web_search_callback),
		BONOBO_UI_VERB ("Undo", edit_menu_undo_callback),
		BONOBO_UI_VERB ("Cut", edit_menu_cut_callback),
		BONOBO_UI_VERB ("Copy", edit_menu_copy_callback),
		BONOBO_UI_VERB ("Paste", edit_menu_paste_callback),
		BONOBO_UI_VERB ("Clear", edit_menu_clear_callback),
		BONOBO_UI_VERB ("Customize", customize_callback),
		BONOBO_UI_VERB ("Change Appearance", change_appearance_callback),
		BONOBO_UI_VERB ("Back", go_menu_back_callback),
		BONOBO_UI_VERB ("Forward", go_menu_forward_callback),
		BONOBO_UI_VERB ("Up", go_menu_up_callback),
		BONOBO_UI_VERB ("Home", go_menu_home_callback),
		BONOBO_UI_VERB ("Forget History", go_menu_forget_history_callback),
		BONOBO_UI_VERB ("Reload", view_menu_reload_callback),
		BONOBO_UI_VERB ("Show Hide Sidebar", view_menu_show_hide_sidebar_callback),
		BONOBO_UI_VERB ("Show Hide Tool Bar", view_menu_show_hide_tool_bar_callback),
		BONOBO_UI_VERB ("Show Hide Location Bar", view_menu_show_hide_location_bar_callback),
		BONOBO_UI_VERB ("Show Hide Status Bar", view_menu_show_hide_status_bar_callback),
		BONOBO_UI_VERB ("Zoom In", view_menu_zoom_in_callback),
		BONOBO_UI_VERB ("Zoom Out", view_menu_zoom_out_callback),
		BONOBO_UI_VERB ("Zoom Normal", view_menu_zoom_normal_callback),
		BONOBO_UI_VERB ("Add Bookmark", bookmarks_menu_add_bookmark_callback),
		BONOBO_UI_VERB ("Edit Bookmarks", bookmarks_menu_edit_bookmarks_callback),
		BONOBO_UI_VERB ("About Nautilus", help_menu_about_nautilus_callback),
		BONOBO_UI_VERB ("Nautilus Feedback", help_menu_nautilus_feedback_callback),


		BONOBO_UI_VERB ("Switch to Beginner Level", user_level_menu_item_callback),
		BONOBO_UI_VERB ("Switch to Intermediate Level", user_level_menu_item_callback),
		BONOBO_UI_VERB ("Switch to Advanced Level", user_level_menu_item_callback),
		BONOBO_UI_VERB ("User Level Customization", user_level_customize_callback),

		BONOBO_UI_VERB ("Stop", stop_button_callback),
		BONOBO_UI_VERB ("Services", services_button_callback),

		BONOBO_UI_VERB_END
	};

	bonobo_ui_component_add_verb_list_with_data (window->details->shell_ui, verbs, window);

	nautilus_window_create_top_level_menus (window);

        nautilus_window_update_find_menu_item (window);
        nautilus_window_update_show_hide_menu_items (window);

	bonobo_ui_component_freeze (window->details->shell_ui, NULL);
	add_user_level_menu_item (window, NAUTILUS_MENU_PATH_NOVICE_ITEM, 
				  NAUTILUS_USER_LEVEL_NOVICE);
	add_user_level_menu_item (window, NAUTILUS_MENU_PATH_INTERMEDIATE_ITEM, 
				  NAUTILUS_USER_LEVEL_INTERMEDIATE);
	add_user_level_menu_item (window, NAUTILUS_MENU_PATH_EXPERT_ITEM, 
				  NAUTILUS_USER_LEVEL_HACKER);
	new_user_level = nautilus_user_level_manager_get_user_level ();
	new_user_level_icon_name = get_user_level_icon_name (new_user_level, FALSE);
#if BONOBO_WORKAROUND
	/* the line below is disabled because of a bug in bonobo */
	nautilus_bonobo_set_icon (window->details->shell_ui,
				  NAUTILUS_MENU_PATH_USER_LEVEL,
				  new_user_level_icon_name);
#endif
	bonobo_ui_component_thaw (window->details->shell_ui, NULL);

	/* connect to the user level changed signal, so we can update the menu when the
	   user level changes */
	gtk_signal_connect_while_alive (GTK_OBJECT (nautilus_user_level_manager_get ()),
					"user_level_changed",
					user_level_changed_callback,
					window,
					GTK_OBJECT (window));

        nautilus_window_initialize_bookmarks_menu (window);
        nautilus_window_initialize_go_menu (window);
}

void
nautilus_window_remove_bookmarks_menu_callback (NautilusWindow *window)
{
        if (window->details->refresh_bookmarks_menu_idle_id != 0) {
                gtk_idle_remove (window->details->refresh_bookmarks_menu_idle_id);
		window->details->refresh_bookmarks_menu_idle_id = 0;
        }
}

void
nautilus_window_remove_go_menu_callback (NautilusWindow *window)
{
        if (window->details->refresh_go_menu_idle_id != 0) {
                gtk_idle_remove (window->details->refresh_go_menu_idle_id);
		window->details->refresh_go_menu_idle_id = 0;
        }
}

void
nautilus_window_remove_bookmarks_menu_items (NautilusWindow *window)
{
	remove_bookmarks_after (window, 
				NAUTILUS_MENU_PATH_BOOKMARKS_MENU, 
				NAUTILUS_MENU_PATH_BOOKMARK_ITEMS_PLACEHOLDER);
}

void
nautilus_window_remove_go_menu_items (NautilusWindow *window)
{
	remove_bookmarks_after (window, 
				NAUTILUS_MENU_PATH_GO_MENU, 
				NAUTILUS_MENU_PATH_HISTORY_ITEMS_PLACEHOLDER);
}

void
nautilus_window_update_find_menu_item (NautilusWindow *window)
{
	char *label_string;

	g_return_if_fail (NAUTILUS_IS_WINDOW (window));

	label_string = g_strdup 
		(nautilus_window_get_search_mode (window) 
			? _("_Browse") 
			: _("_Find"));

	nautilus_bonobo_set_label (window->details->shell_ui, 
				   "/menu/File/Toggle Find Mode",
				   label_string);
	g_free (label_string);

#ifndef UIH
	/* avoid "unused function" warnings */
	return;

	add_user_level_menu_item (0, 0, 0);
	bookmark_holder_free (0);
	show_bogus_bookmark_window (0);
#endif
}

static void
append_dynamic_bookmarks (NautilusWindow *window)
{
        NautilusBookmarkList *bookmarks;
	guint 	bookmark_count;
	guint	index;
	
	g_assert (NAUTILUS_IS_WINDOW (window));

	bookmarks = get_bookmark_list ();

	bookmark_count = nautilus_bookmark_list_length (bookmarks);

        /* Add separator before bookmarks, unless there are no bookmarks. */
        if (bookmark_count > 0) {
	        append_separator (window, NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_BOOKMARKS);
        }

	/* append new set of bookmarks */
	for (index = 0; index < bookmark_count; ++index)
	{
		char *path;

		path = g_strdup_printf ("/Bookmarks/Bookmark%d", index);
		append_bookmark_to_menu (window, 
		                         nautilus_bookmark_list_item_at (bookmarks, index), 
		                         path,
		                         TRUE); 
                g_free (path);
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
                        = gtk_idle_add (refresh_bookmarks_menu_idle_callback,
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
	GList *p;
	int index;
	char *path;
	
	g_assert (NAUTILUS_IS_WINDOW (window));

	/* Unregister any pending call to this function. */
	nautilus_window_remove_go_menu_callback (window);

	/* Remove old set of history items. */
	nautilus_window_remove_go_menu_items (window);

	/* Add in a new set of history items. */
	index = 0;
	p = nautilus_get_history_list ();
	if (p != NULL) {
		/* Append the separator when there's any history items */
		append_separator (window, NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_HISTORY);

		while (p != NULL) {
			path = g_strdup_printf ("/Go/History%d", index); 
	                append_bookmark_to_menu (window,
	                                         NAUTILUS_BOOKMARK (p->data),
	                                         path,
	                                         FALSE);
	                g_free (path);

	                ++index;
			p = p->next;
		}
	}
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
                        = gtk_idle_add (refresh_go_menu_idle_callback,
                                        window);
	}	
}

static void
update_user_level_menu_items (NautilusWindow *window)
{
	char *customize_string;
	int user_level;
	char *user_level_icon_name;
	
        g_assert (window != NULL);
        g_assert (NAUTILUS_IS_WINDOW (window));

 	/* Update the user radio group to reflect reality */
	user_level = nautilus_user_level_manager_get_user_level ();
	user_level_icon_name = get_user_level_icon_name (user_level, FALSE);
#if BONOBO_WORKAROUND
	/* the line below is disabled because of a bug in bonobo */
	nautilus_bonobo_set_icon (window->details->shell_ui,
				  NAUTILUS_MENU_PATH_USER_LEVEL,
				  user_level_icon_name);
#endif
	g_free (user_level_icon_name);


	customize_string = get_customize_user_level_settings_menu_string ();
	g_assert (customize_string != NULL);
	nautilus_bonobo_set_label (window->details->shell_ui,
				   NAUTILUS_MENU_PATH_USER_LEVEL_CUSTOMIZE,
				   customize_string);

	g_free (customize_string);
}


static guint
convert_verb_to_user_level (const char *verb)
{
        g_assert (verb != NULL);
	
	if (strcmp (verb, SWITCH_TO_BEGINNER_VERB) == 0) {
		return NAUTILUS_USER_LEVEL_NOVICE;
	}
	else if (strcmp (verb, SWITCH_TO_INTERMEDIATE_VERB) == 0) {
		return NAUTILUS_USER_LEVEL_INTERMEDIATE;
	}
	else if (strcmp (verb, SWITCH_TO_ADVANCED_VERB) == 0) {
		return NAUTILUS_USER_LEVEL_HACKER;
	}

	g_assert_not_reached ();

	return NAUTILUS_USER_LEVEL_NOVICE;
}

static const char *
convert_user_level_to_path (guint user_level)
{
	switch (user_level) {
	case NAUTILUS_USER_LEVEL_NOVICE:
		return NAUTILUS_MENU_PATH_NOVICE_ITEM;
		break;

	case NAUTILUS_USER_LEVEL_INTERMEDIATE:
		return NAUTILUS_MENU_PATH_INTERMEDIATE_ITEM;
		break;

	case NAUTILUS_USER_LEVEL_HACKER:
		return NAUTILUS_MENU_PATH_EXPERT_ITEM; 
		break;
	}

	g_assert_not_reached ();

	return "";
}


static char *
get_customize_user_level_string (void)
{
	char *user_level_string_for_display;
	char *title;
	
	user_level_string_for_display = 
		nautilus_user_level_manager_get_user_level_name_for_display 
			(nautilus_user_level_manager_get_user_level ());

	/* Localizers: This is the label for the menu item that brings up the
	 * user-level settings dialog. %s will be replaced with the name of a
	 * user level ("Beginner", "Intermediate", or "Advanced").
	 */
	title = g_strdup_printf ("Edit %s Settings", user_level_string_for_display);

	g_free (user_level_string_for_display);

	return title;
}

static char *
get_customize_user_level_settings_menu_string (void)
{
	char *title;
	char *ellipse_suffixed_title;

	title = get_customize_user_level_string ();
	g_assert (title != NULL);

	ellipse_suffixed_title = g_strdup_printf ("%s...", title);

	g_free (title);

	return ellipse_suffixed_title;
}

static void
update_preferences_dialog_title (void)
{
	char *dialog_title;
	
	dialog_title = get_customize_user_level_string ();
	g_assert (dialog_title != NULL);
	
	nautilus_global_preferences_set_dialog_title (dialog_title);

	g_free (dialog_title);
}
