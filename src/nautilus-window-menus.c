/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-window-menus.h - implementation of nautilus window menu operations,
                             split into separate file just for convenience.

   Copyright (C) 2000 Eazel, Inc.

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

   Author: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>

#include "nautilus-bookmark-list.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-signaller.h"
#include "nautilus-application.h"
#include "nautilus-window-private.h"
#include "nautilus-property-browser.h"

#include <libnautilus-extensions/nautilus-undo-manager.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-user-level-manager.h>

static GtkWindow *bookmarks_window = NULL;

static void                  activate_bookmark_in_menu_item                 (BonoboUIHandler        *uih,
									     gpointer                user_data,
									     const char             *path);
static void                  append_bookmark_to_menu                        (NautilusWindow         *window,
									     const NautilusBookmark *bookmark,
									     const char             *menu_item_path,
									     gboolean		     is_in_bookmarks_menu);
static void                  clear_appended_bookmark_items                  (NautilusWindow         *window,
									     const char             *menu_path,
									     const char             *last_static_item_path);
static NautilusBookmarkList *get_bookmark_list                              (void);
static void                  refresh_bookmarks_in_go_menu                   (NautilusWindow         *window);
static void                  refresh_bookmarks_in_bookmarks_menu            (NautilusWindow         *window);
static void                  edit_bookmarks                                 (NautilusWindow         *window);




/* User level things */
static guint                 convert_menu_path_to_user_level                (const char             *path);
static const char *          convert_user_level_to_menu_path                (guint                   user_level);
static char *                get_customize_user_level_setttings_menu_string (void);
static void                  update_user_level_menu_items                   (NautilusWindow         *window);
static void                  user_level_changed_callback                    (GtkObject              *user_level_manager,
									     gpointer                user_data);
static char *                get_customize_user_level_string                (void);
static void		     update_preferences_dialog_title		    (void);

/* Struct that stores all the info necessary to activate a bookmark. */
typedef struct {
        const NautilusBookmark *bookmark;
        NautilusWindow *window;
        gboolean in_bookmarks_menu;
} BookmarkHolder;

static BookmarkHolder *
bookmark_holder_new (const NautilusBookmark *bookmark, 
		     NautilusWindow *window,
		     gboolean in_bookmarks_menu)
{
	BookmarkHolder *new_bookmark_holder;

	new_bookmark_holder = g_new (BookmarkHolder, 1);
	new_bookmark_holder->window = window;
	new_bookmark_holder->bookmark = bookmark;
	new_bookmark_holder->in_bookmarks_menu = in_bookmarks_menu;

	return new_bookmark_holder;
}

static void
bookmark_holder_free (BookmarkHolder *bookmark_holder)
{
	/* The bookmark and window are just passed around unreffed,
	 * so all we need to do is free the struct. If this turns out
	 * to be inadequate we can change bookmark_holder_new
	 * and _free later.
	 */
	g_free (bookmark_holder);
}

/* Private menu definitions; others are in <libnautilus/nautilus-bonobo-ui.h>.
 * These are not part of the published set, either because they are
 * development-only or because we expect to change them and
 * don't want other code relying on their existence.
 */
#define NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_RADIO_GROUP	"/Settings/UserLevelRadioGroup"
#define NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_NOVICE		"/Settings/User Level Novice"
#define NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_INTERMEDIATE	"/Settings/User Level Intermediate"
#define NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_HACKER		"/Settings/User Level Hacker"
#define NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_CUSTOMIZE	"/Settings/User Level Customize"
#define NAUTILUS_MENU_PATH_AFTER_USER_LEVEL_SEPARATOR		"/Settings/After User Level Separator"

#define NAUTILUS_MENU_PATH_CUSTOMIZE_ITEM			"/Settings/Customize"

static void
file_menu_new_window_callback (BonoboUIHandler *ui_handler, 
			       gpointer user_data, 
			       const char *path)
{
	NautilusWindow *current_window;
	NautilusWindow *new_window;
	
	current_window = NAUTILUS_WINDOW (user_data);
	new_window = nautilus_application_create_window (current_window->application);
	nautilus_window_goto_uri (new_window, current_window->location);
	gtk_widget_show (GTK_WIDGET (new_window));
}

static void
file_menu_close_window_callback (BonoboUIHandler *ui_handler, 
			         gpointer user_data, 
			         const char *path)
{
        g_assert (NAUTILUS_IS_WINDOW (user_data));
	nautilus_window_close (NAUTILUS_WINDOW (user_data));
}

static void
file_menu_exit_callback (BonoboUIHandler *ui_handler, 
			 gpointer user_data, 
			 const char *path)
{
	gtk_main_quit ();
}

static void
edit_menu_undo_callback (BonoboUIHandler *ui_handler, 
		  	 gpointer user_data, 
		  	 const char *path) 
{
	nautilus_undo_manager_undo
		(NAUTILUS_WINDOW (user_data)->application->undo_manager);
}

static void
edit_menu_cut_callback (BonoboUIHandler *ui_handler, 
		  	gpointer user_data, 
		  	const char *path) 
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);
	if (GTK_IS_EDITABLE (window->focus_widget)) {
		gtk_editable_cut_clipboard (GTK_EDITABLE (window->focus_widget));
	}

}

static void
edit_menu_copy_callback (BonoboUIHandler *ui_handler, 
		   	 gpointer user_data,
		   	 const char *path) 
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);
	if (GTK_IS_EDITABLE (window->focus_widget)) {
		gtk_editable_copy_clipboard (GTK_EDITABLE (window->focus_widget));
	}

}


static void
edit_menu_paste_callback (BonoboUIHandler *ui_handler, 
		    	  gpointer user_data,
		    	  const char *path) 
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);
	if (GTK_IS_EDITABLE (window->focus_widget)) {
		gtk_editable_paste_clipboard (GTK_EDITABLE (window->focus_widget));
	}

}


static void
edit_menu_clear_callback (BonoboUIHandler *ui_handler, 
		    	  gpointer user_data,
		    	  const char *path) 
{
	GtkWindow *window;

	window = GTK_WINDOW (user_data);
	if (GTK_IS_EDITABLE (window->focus_widget)) {
		/* A negative index deletes until the end of the string */
		gtk_editable_delete_text (GTK_EDITABLE (window->focus_widget),0, -1);
	}

}

static void
go_menu_back_callback (BonoboUIHandler *ui_handler, 
		       gpointer user_data,
		       const char *path) 
{
	nautilus_window_go_back (NAUTILUS_WINDOW (user_data));
}

static void
go_menu_forward_callback (BonoboUIHandler *ui_handler, 
		       	  gpointer user_data,
		          const char *path) 
{
	nautilus_window_go_forward (NAUTILUS_WINDOW (user_data));
}

static void
go_menu_up_callback (BonoboUIHandler *ui_handler, 
		     gpointer user_data,
		     const char *path) 
{
	nautilus_window_go_up (NAUTILUS_WINDOW (user_data));
}

static void
go_menu_home_callback (BonoboUIHandler *ui_handler, 
		       gpointer user_data,
		       const char *path) 
{
	nautilus_window_go_home (NAUTILUS_WINDOW (user_data));
}

static void
bookmarks_menu_add_bookmark_callback (BonoboUIHandler *ui_handler, 
		       		      gpointer user_data,
		      		      const char *path)
{
        nautilus_window_add_bookmark_for_current_location (NAUTILUS_WINDOW (user_data));
}

static void
bookmarks_menu_edit_bookmarks_callback (BonoboUIHandler *ui_handler, 
		       		      	gpointer user_data,
		      		      	const char *path)
{
        edit_bookmarks (NAUTILUS_WINDOW (user_data));
}

static void
settings_menu_user_level_radio_group_callback (BonoboUIHandler	*ui_handler, 
					       gpointer		user_data,
					       const char	*path)
{
	NautilusWindow	*window;
 	guint		old_user_level;
 	guint		new_user_level;

        g_assert (path != NULL);

	window = NAUTILUS_WINDOW (user_data);

	/* FIXME bugzilla.eazel.com 916: Workaround for Bonobo bug. */
	if (window->updating_bonobo_radio_menu_item) {
		return;
	}

	/* Make sure it changed.  This check is needed cause the stupid
	 * menu radio group triggers two callbacks whenever the active
	 * button changes - one for the previously active button and one
	 * for the new one.  Of course, we only care about the new one.
	 */
	old_user_level = nautilus_user_level_manager_get_user_level ();
	new_user_level = convert_menu_path_to_user_level (path);

	if (old_user_level == new_user_level) {
		return;
	}

	nautilus_user_level_manager_set_user_level (new_user_level);
}

static void
settings_menu_user_level_customize_callback (BonoboUIHandler *ui_handler, 
					     gpointer user_data,
					     const char *path)
{
	nautilus_global_preferences_show_dialog ();
}

static void
settings_menu_customize_callback (BonoboUIHandler *ui_handler, 
				  gpointer user_data,
				  const char *path)
{
	nautilus_property_browser_new ();
}

static void
help_menu_about_nautilus_callback (BonoboUIHandler *ui_handler, 
		       		   gpointer user_data,
		      		   const char *path)
{
	static GtkWidget *aboot = NULL;

	if (aboot == NULL) {
		const char *authors[] = {
			"Darin Adler",
			"Pavel Císler",
			"Ramiro Estrugo",
			"Andy Hertzfeld",
			"Elliot Lee",
			"Ettore Perazzoli",
			"Gene Ragan",
			"Arlo Rose",
			"Rebecca Schulman",
			"Maciej Stachowiak",
			"John Sullivan",
			NULL
		};

		aboot = gnome_about_new(_("Nautilus"),
					VERSION,
					"Copyright (C) 1999, 2000",
					authors,
					_("The Gnome Shell"),
					"nautilus/About_Image.png");

		gnome_dialog_close_hides (GNOME_DIALOG (aboot), TRUE);
	}

	nautilus_gtk_window_present (GTK_WINDOW (aboot));
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
	GtkWidget *dialog;
	const char *uri;
	char *prompt;

	uri = nautilus_bookmark_get_uri (holder->bookmark);

	if (holder->in_bookmarks_menu) {
		prompt = g_strdup_printf (_("The location \"%s\" does not exist. Do you "
					    "want to remove any bookmarks with this "
					    "location from your list?"), uri);
		dialog = nautilus_yes_no_dialog_parented (prompt,
					   	          _("Remove"),
					   	          GNOME_STOCK_BUTTON_CANCEL,
					   	          GTK_WINDOW (holder->window));

		nautilus_gtk_signal_connect_free_data
			(GTK_OBJECT (nautilus_gnome_dialog_get_button_by_index 
					(GNOME_DIALOG (dialog), GNOME_OK)),
			 "clicked",
			 remove_bookmarks_for_uri,
			 g_strdup (uri));

		gtk_window_set_title (GTK_WINDOW (dialog), _("Bookmark for Bad Location"));			 
		gnome_dialog_set_default (GNOME_DIALOG (dialog), GNOME_CANCEL);
	} else {
		prompt = g_strdup_printf (_("The location \"%s\" no longer exists. "
					    "It was probably moved, deleted, or renamed."), uri);
		dialog = nautilus_info_dialog_parented (prompt, GTK_WINDOW (holder->window));
		gtk_window_set_title (GTK_WINDOW (dialog), _("Go To Bad Location"));
	}
	
	g_free (prompt);
}

static gboolean
uri_known_not_to_exist (const char *uri) 
{
	NautilusFile *file;

	/* Don't assume anything about schemes other than "file://" */
	if (!nautilus_str_has_prefix (uri, "file://")) {
		return FALSE;
	}
	
	file = nautilus_file_get (uri);

	/* Couldn't make a NautilusFile, so uri must have been bogus. */
	if (file == NULL) {
		return TRUE;
	}

	nautilus_file_unref (file);

	return FALSE;
}

static void
activate_bookmark_in_menu_item (BonoboUIHandler *uih, gpointer user_data, const char *path)
{
        BookmarkHolder *holder;

        holder = (BookmarkHolder *)user_data;

	if (uri_known_not_to_exist (nautilus_bookmark_get_uri (holder->bookmark))) {
		show_bogus_bookmark_window (holder);
	} else {
	        nautilus_window_goto_uri (holder->window, 
	        			  nautilus_bookmark_get_uri (holder->bookmark));
        }	
}

static void
append_bookmark_to_menu (NautilusWindow *window, 
                         const NautilusBookmark *bookmark, 
                         const char *menu_item_path,
                         gboolean is_bookmarks_menu)
{
	BookmarkHolder *bookmark_holder;	
	GdkPixbuf *pixbuf;
	BonoboUIHandlerPixmapType pixmap_type;
	char *name;

	/* FIXME bugzilla.eazel.com 705:
	 * This can make remote calls to try to get the icon. We
	 * need to save the icon in some way that it isn't retrieved each
	 * time, at least for remote ones.
	 */
	pixbuf = nautilus_bookmark_get_pixbuf (bookmark, NAUTILUS_ICON_SIZE_FOR_MENUS);

	/* Set up pixmap type based on result of function.  If we fail, set pixmap type to none */
	if (pixbuf != NULL) {
		pixmap_type = BONOBO_UI_HANDLER_PIXMAP_PIXBUF_DATA;
	} else {
		pixmap_type = BONOBO_UI_HANDLER_PIXMAP_NONE;
	}

	bookmark_holder = bookmark_holder_new (bookmark, window, is_bookmarks_menu);

	/* We double the underscores here to escape them so Bonobo will know they are
	 * not keyboard accelerator character prefixes. If we ever find we need to
	 * escape more than just the underscores, we'll add a menu helper function
	 * instead of a string utility. (Like maybe escaping control characters.)
	 */
	name = nautilus_str_double_underscores
		(nautilus_bookmark_get_name (bookmark));
 	bonobo_ui_handler_menu_new_item (window->ui_handler,
					 menu_item_path,
					 name,
					 _("Go to the specified location"),
					 -1,
					 pixmap_type,
					 pixbuf,
					 0,
					 0,
					 NULL,
					 NULL);
	g_free (name);

	/* We must use "set_callback" since we have a destroy-notify function. */
	bonobo_ui_handler_menu_set_callback
		(window->ui_handler, menu_item_path,
		 activate_bookmark_in_menu_item,
		 bookmark_holder, (GDestroyNotify) bookmark_holder_free);
}

/**
 * clear_appended_bookmark_items
 * 
 * Remove the dynamic menu items from the end of this window's menu. Each dynamic
 * item should have callback data that's either NULL or is a BookmarkHolder *.
 * @window: The NautilusWindow whose menu should be cleaned.
 * @menu_path: The BonoboUIHandler-style path for the menu to be cleaned (e.g. "/Go").
 * @last_static_item_path: The BonoboUIHandler-style path for the last menu item to
 * leave in place (e.g. "/Go/Home"). All menu items after this one will be removed.
 */
static void
clear_appended_bookmark_items (NautilusWindow *window, 
                               const char *menu_path, 
                               const char *last_static_item_path)
{
	GList *children, *p;
	gboolean found_dynamic_items;

	g_assert (NAUTILUS_IS_WINDOW (window));

	children = bonobo_ui_handler_menu_get_child_paths (window->ui_handler, menu_path);
	found_dynamic_items = FALSE;

	for (p = children; p != NULL; p = p->next) {
                if (found_dynamic_items) {
                        bonobo_ui_handler_menu_remove (window->ui_handler, p->data);
		} else if (strcmp ((const char *) p->data, last_static_item_path) == 0) {
			found_dynamic_items = TRUE;
		}
	}

	g_assert (found_dynamic_items);
        nautilus_g_list_free_deep (children);
}

static NautilusBookmarkList *
get_bookmark_list (void)
{
        static NautilusBookmarkList *bookmarks = NULL;

        if (bookmarks == NULL) {
                bookmarks = nautilus_bookmark_list_new ();
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
	NautilusBookmark *bookmark;

	g_return_if_fail(NAUTILUS_IS_WINDOW (window));

	g_assert (strcmp (nautilus_bookmark_get_uri (window->current_location_bookmark), 
		          window->location) == 0);
	/* Use the first bookmark in the history list rather than creating a new one. */
	bookmark = window->current_location_bookmark;

	if (!nautilus_bookmark_list_contains (get_bookmark_list (), bookmark)) {
		/* 
		 * Only append bookmark if there's not already a copy of this one in there.
		 * Maybe it would be better to remove the other one and always append?
		 * This won't be a sensible rule if we have hierarchical menus.
		 */
		nautilus_bookmark_list_append (get_bookmark_list (), bookmark);
	}
}

static void
edit_bookmarks (NautilusWindow *window)
{
        nautilus_gtk_window_present
		(get_or_create_bookmarks_window (GTK_OBJECT (window)));
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
        /* Add current set of bookmarks */
        refresh_bookmarks_in_bookmarks_menu (window);

	/* Recreate bookmarks part of menu if bookmark list changes
	 * or if icon theme changes.
	 */
	gtk_signal_connect_object_while_alive (GTK_OBJECT (get_bookmark_list ()),
			                       "contents_changed",
			                       refresh_bookmarks_in_bookmarks_menu,
			   	               GTK_OBJECT (window));
	 
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       refresh_bookmarks_in_bookmarks_menu,
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
			                       refresh_bookmarks_in_go_menu,
			   	               GTK_OBJECT (window));
	 
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       refresh_bookmarks_in_go_menu,
					       GTK_OBJECT (window));

}

static void
append_separator (NautilusWindow *window, const char *separator_path)
{
        bonobo_ui_handler_menu_new_separator (window->ui_handler,
                                              separator_path,
                                              -1);
}

static void
new_top_level_menu (NautilusWindow *window, 
		    const char *menu_path,
		    const char *title)
{
	/* Note that we don't bother with hints for menu titles.
	 * We can revisit this anytime if someone thinks they're useful.
	 */
	bonobo_ui_handler_menu_new_subtree (window->ui_handler,
					    menu_path,
					    title,
					    NULL,
					    -1,
					    BONOBO_UI_HANDLER_PIXMAP_NONE,
					    NULL,
					    0,
					    0);
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
        BonoboUIHandler *ui_handler;

        ui_handler = window->ui_handler;
        g_assert (ui_handler != NULL);
        
        bonobo_ui_handler_create_menubar (ui_handler);

	/* File menu */
        new_top_level_menu (window, NAUTILUS_MENU_PATH_FILE_MENU, _("_File"));

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_NEW_WINDOW_ITEM,
        				 _("_New Window"),
        				 _("Create a new window"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_STOCK,
        				 GNOME_STOCK_MENU_NEW,
        				 GNOME_KEY_NAME_NEW,
        				 GNOME_KEY_MOD_NEW,
        				 file_menu_new_window_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_CLOSE_ITEM,
        				 _("_Close Window"),
        				 _("Close this window"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_STOCK,
        				 GNOME_STOCK_MENU_CLOSE,
        				 GNOME_KEY_NAME_CLOSE,
        				 GNOME_KEY_MOD_CLOSE,
        				 file_menu_close_window_callback,
        				 window);

        append_separator (window, NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_EXIT);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_EXIT_ITEM,
        				 _("_Exit"),
        				 _("Exit from Nautilus"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_STOCK,
        				 GNOME_STOCK_MENU_EXIT,
        				 GNOME_KEY_NAME_EXIT,
        				 GNOME_KEY_MOD_EXIT,
        				 file_menu_exit_callback,
        				 NULL);


	/* Edit menu */
        new_top_level_menu (window, NAUTILUS_MENU_PATH_EDIT_MENU, _("_Edit"));

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_UNDO_ITEM,
        				 _("_Undo"),
        				 _("Undo the last text change"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_STOCK,
        				 GNOME_STOCK_MENU_UNDO,
        				 GNOME_KEY_NAME_UNDO,
        				 GNOME_KEY_MOD_UNDO,
        				 edit_menu_undo_callback,
        				 window);

        append_separator (window, NAUTILUS_MENU_PATH_SEPARATOR_AFTER_UNDO);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_CUT_ITEM,
        				 _("_Cut Text"),
        				 _("Cuts the selected text to the clipboard"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_STOCK,
        				 GNOME_STOCK_MENU_CUT,
        				 GNOME_KEY_NAME_CUT,
        				 GNOME_KEY_MOD_CUT,
        				 edit_menu_cut_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_COPY_ITEM,
        				 _("_Copy Text"),
        				 _("Copies the selected text to the clipboard"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_STOCK,
        				 GNOME_STOCK_MENU_COPY,
        				 GNOME_KEY_NAME_COPY,
        				 GNOME_KEY_MOD_COPY,
        				 edit_menu_copy_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_PASTE_ITEM,
        				 _("_Paste Text"),
        				 _("Pastes the text stored on the clipboard"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_STOCK,
        				 GNOME_STOCK_MENU_PASTE,
        				 GNOME_KEY_NAME_PASTE,
        				 GNOME_KEY_MOD_PASTE,
        				 edit_menu_paste_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_CLEAR_ITEM,
        				 _("C_lear Text"),
        				 _("Removes the selected text without putting it on the clipboard"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 GNOME_KEY_NAME_CLEAR,
        				 GNOME_KEY_MOD_CLEAR,
        				 edit_menu_clear_callback,
        				 window);

        append_separator (window, NAUTILUS_MENU_PATH_SEPARATOR_AFTER_CLEAR);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_SELECT_ALL_ITEM,
        				 _("_Select All"),
        				 NULL,	/* No hint since it's insensitive here */
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 'A',	/* Keyboard shortcut applies to overriders too */
        				 GDK_CONTROL_MASK,
        				 NULL,
        				 NULL);

	/* Go menu */
        new_top_level_menu (window, NAUTILUS_MENU_PATH_GO_MENU, _("_Go"));

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_BACK_ITEM,
        				 _("_Back"),
        				 _("Go to the previous visited location"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 'B',
        				 GDK_CONTROL_MASK,
        				 go_menu_back_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_FORWARD_ITEM,
        				 _("_Forward"),
        				 _("Go to the next visited location"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 'F',
        				 GDK_CONTROL_MASK,
        				 go_menu_forward_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_UP_ITEM,
        				 _("_Up"),
        				 _("Go to the location that contains this one"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 'U',
        				 GDK_CONTROL_MASK,
        				 go_menu_up_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_HOME_ITEM,
        				 _("_Home"),
        				 _("Go to the home location"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 'H',
        				 GDK_CONTROL_MASK,
        				 go_menu_home_callback,
        				 window);

        append_separator (window, NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_HISTORY);

        
	/* Bookmarks */
        new_top_level_menu (window, NAUTILUS_MENU_PATH_BOOKMARKS_MENU, _("_Bookmarks"));

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_ADD_BOOKMARK_ITEM,
        				 _("_Add Bookmark"),
        				 _("Add a bookmark for the current location to this menu"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 0,
        				 0,
        				 bookmarks_menu_add_bookmark_callback,
        				 window);

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_EDIT_BOOKMARKS_ITEM,
        				 _("_Edit Bookmarks..."),
        				 _("Display a window that allows editing the bookmarks in this menu"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 0,
        				 0,
        				 bookmarks_menu_edit_bookmarks_callback,
        				 window);
	/* Settings */
        new_top_level_menu (window, NAUTILUS_MENU_PATH_SETTINGS_MENU, _("_Settings"));

	/* User level */
	window->updating_bonobo_radio_menu_item = TRUE;

	bonobo_ui_handler_menu_new_radiogroup (ui_handler, NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_RADIO_GROUP);
	
	bonobo_ui_handler_menu_new_radioitem (ui_handler,
					      NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_NOVICE,
					      _("Novice"),
					      _("Novice User Level"),
					      -1, 0, 0,
					      settings_menu_user_level_radio_group_callback, 
					      window);
	
	bonobo_ui_handler_menu_new_radioitem (ui_handler,
					      NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_INTERMEDIATE,
					      _("Intermediate"),
					      _("Intermediate User Level"),
					      -1, 0, 0,
					      settings_menu_user_level_radio_group_callback, 
					      window);
	
	bonobo_ui_handler_menu_new_radioitem (ui_handler,
					      NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_HACKER,
					      _("Hacker"),
					      _("Hacker User Level"),
					      -1, 0, 0,
					      settings_menu_user_level_radio_group_callback, 
					      window);

	bonobo_ui_handler_menu_new_item (ui_handler,
					 NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_CUSTOMIZE,
					 _("Customize Settings..."),
					 _("Customize Settings for the Current User Level"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 0,
        				 0,
        				 settings_menu_user_level_customize_callback,
        				 NULL);

	bonobo_ui_handler_menu_new_separator (ui_handler,
					      NAUTILUS_MENU_PATH_AFTER_USER_LEVEL_SEPARATOR,
					      -1);

	/* Make sure the dialog title matched the user level */
	update_preferences_dialog_title ();

	update_user_level_menu_items (window);

	/* Register to find out about user level changes in order to update the customize label */
	gtk_signal_connect (GTK_OBJECT (nautilus_user_level_manager_get ()),
			    "user_level_changed",
			    user_level_changed_callback,
			    window);
	
	window->updating_bonobo_radio_menu_item = FALSE;

	/* Customize */
	bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_CUSTOMIZE_ITEM,
        				 _("_Customize..."),
        				 _("Displays the Property Browser, to add properties to objects and customize appearance"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 0,
        				 0,
        				 settings_menu_customize_callback,
        				 NULL);

	/* Help */
        new_top_level_menu (window, NAUTILUS_MENU_PATH_HELP_MENU, _("_Help"));

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_ABOUT_ITEM,
        				 _("_About Nautilus..."),
        				 _("Displays information about the Nautilus program"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_STOCK,
        				 GNOME_STOCK_MENU_ABOUT,
        				 0,
        				 0,
        				 help_menu_about_nautilus_callback,
        				 NULL);

        /* Desensitize the items that aren't implemented at this level.
         * Some (hopefully all) will be overridden by implementations by the
         * different content views.
         */
	bonobo_ui_handler_menu_set_sensitivity (ui_handler, 
        				        NAUTILUS_MENU_PATH_SELECT_ALL_ITEM, 
						FALSE);

	/* Connect to undo manager so it will handle the menu item. */
	nautilus_undo_manager_set_up_bonobo_ui_handler_undo_item
		(window->application->undo_manager,
		 ui_handler, NAUTILUS_MENU_PATH_UNDO_ITEM,
		 _("_Undo"), _("Undo the last text change"));
	
        nautilus_window_initialize_bookmarks_menu (window);
        nautilus_window_initialize_go_menu (window);
}

/**
 * refresh_bookmarks_in_bookmarks_menu:
 * 
 * Refresh list of bookmarks at end of Bookmarks menu to match centralized list.
 * @window: The NautilusWindow whose Bookmarks menu will be refreshed.
 **/
static void
refresh_bookmarks_in_bookmarks_menu (NautilusWindow *window)
{
        NautilusBookmarkList *bookmarks;
	guint 	bookmark_count;
	guint	index;
	
	g_assert (NAUTILUS_IS_WINDOW (window));

	bookmarks = get_bookmark_list ();

	/* Remove old set of bookmarks. */
	clear_appended_bookmark_items (window, 
				       NAUTILUS_MENU_PATH_BOOKMARKS_MENU, 
				       NAUTILUS_MENU_PATH_EDIT_BOOKMARKS_ITEM);

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

/**
 * refresh_bookmarks_in_go_menu:
 * 
 * Refresh list of bookmarks at end of Go menu to match centralized history list.
 * @window: The NautilusWindow whose Go menu will be refreshed.
 **/
static void
refresh_bookmarks_in_go_menu (NautilusWindow *window)
{
	GSList *p;
	int index;

	g_assert (NAUTILUS_IS_WINDOW (window));

	/* Remove old set of history items. */
	clear_appended_bookmark_items (window, 
				       NAUTILUS_MENU_PATH_GO_MENU, 
				       NAUTILUS_MENU_PATH_SEPARATOR_BEFORE_HISTORY);

	/* Add in a new set of history items. */
	index = 0;
	for (p = nautilus_get_history_list (); p != NULL; p = p->next) {
		char *path;

		path = g_strdup_printf ("/Go/History%d", index); 
                append_bookmark_to_menu (window,
                                         NAUTILUS_BOOKMARK (p->data),
                                         path,
                                         FALSE);
                g_free (path);

                ++index;
	}	
}

static void
user_level_changed_callback (GtkObject	*user_level_manager,
			     gpointer	user_data)
{
	g_return_if_fail (user_data != NULL);
	g_return_if_fail (NAUTILUS_IS_WINDOW (user_data));

	update_user_level_menu_items (NAUTILUS_WINDOW (user_data));

	/* Hide the customize dialog for novice user level */
	if (nautilus_user_level_manager_get_user_level () == 0) {
		nautilus_global_preferences_hide_dialog ();
	}
	/* Otherwise update its title to reflect the user level */
	else {
		nautilus_global_preferences_dialog_update ();
		update_preferences_dialog_title ();
	}
}

static void
update_user_level_menu_items (NautilusWindow *window)
{
	char *customize_string;
	int user_level;

        g_assert (window != NULL);
        g_assert (NAUTILUS_IS_WINDOW (window));

	window->updating_bonobo_radio_menu_item = TRUE;

	customize_string = get_customize_user_level_setttings_menu_string ();
	g_assert (customize_string != NULL);

	user_level = nautilus_user_level_manager_get_user_level ();

 	/* Update the user radio group to reflect reality */
	bonobo_ui_handler_menu_set_radio_state (window->ui_handler, 
						convert_user_level_to_menu_path (user_level),
						TRUE);

	/* FIXME bugzilla.eazel.com 1247: 
	 * We want to hide the customize button for the novice user level.
	 * It cant find a bonobo ui handler call to hide a menu item, so make it 
	 * insensitive for now.
	 */
	bonobo_ui_handler_menu_set_sensitivity (window->ui_handler,
						NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_CUSTOMIZE,
						(user_level > 0));


 	/* Update the "Customize Settings..." item to reflect the user level to customize */
	bonobo_ui_handler_menu_set_label (window->ui_handler,
					  NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_CUSTOMIZE,
					  customize_string);

	g_free (customize_string);
	window->updating_bonobo_radio_menu_item = FALSE;
}

static guint
convert_menu_path_to_user_level (const char *path)
{
        g_assert (path != NULL);
	
	if (strcmp (path, NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_NOVICE) == 0) {
		return 0;
	}
	else if (strcmp (path, NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_INTERMEDIATE) == 0) {
		return 1;
	}
	else if (strcmp (path, NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_HACKER) == 0) {
		return 2;
	}

	g_assert_not_reached ();

	return 0;
}

static const char *
convert_user_level_to_menu_path (guint user_level)
{
	switch (user_level) {
	case 0:
		return NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_NOVICE;
		break;

	case 1:
		return NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_INTERMEDIATE;
		break;

	case 2:
		return NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_HACKER;
		break;
	}

	g_assert_not_reached ();

	return NAUTILUS_MENU_PATH_SETTINGS_USER_LEVEL_NOVICE;
}

static char *
get_customize_user_level_string (void)
{
	char *user_level_string;
	char *capitalized_user_level_string;
	char *title;
	
	user_level_string = nautilus_user_level_manager_get_user_level_as_string ();
	g_assert (user_level_string != NULL);

	capitalized_user_level_string = nautilus_str_capitalize (user_level_string);
	g_assert (capitalized_user_level_string != NULL);
	
	g_free (user_level_string);

	title = g_strdup_printf ("Customize %s Settings", capitalized_user_level_string);

	g_free (capitalized_user_level_string);

	return title;
}

static char *
get_customize_user_level_setttings_menu_string (void)
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

