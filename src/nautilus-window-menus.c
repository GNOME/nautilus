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

#include "nautilus-bookmark-list.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-signaller.h"
#include "ntl-app.h"
#include "ntl-window-private.h"
#include "libnautilus-extensions/nautilus-undo-manager.h"

#include <libnautilus/nautilus-bonobo-ui.h>

#include <libnautilus-extensions/nautilus-bonobo-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>

static void                  activate_bookmark_in_menu_item      (BonoboUIHandler *uih, 
                                                                  gpointer user_data, 
                                                                  const char *path);
static void                  append_bookmark_to_menu             (NautilusWindow *window, 
                                                                  const NautilusBookmark *bookmark, 
                                                                  const char *menu_item_path);
static void                  clear_appended_bookmark_items       (NautilusWindow *window, 
                                                                  const char *menu_path, 
                                                                  const char *last_static_item_path);
static NautilusBookmarkList *get_bookmark_list                   (void);
static GtkWidget            *get_bookmarks_window                (void);

static void                  refresh_bookmarks_in_go_menu        (NautilusWindow *window);
static void                  refresh_bookmarks_in_bookmarks_menu (NautilusWindow *window);
static void                  update_eazel_theme_menu_item        (NautilusWindow *window);
static void                  update_undo_menu_item        	 (NautilusWindow *window);

/* Struct that stores all the info necessary to activate a bookmark. */
typedef struct {
        const NautilusBookmark *bookmark;
        NautilusWindow *window;
} BookmarkHolder;

/* Private menu definitions; others are in <libnautilus/nautilus-bonobo-ui.h>.
 * These are not part of the published set, either because they are
 * development-only (e.g. Debug) or because we expect to change them and
 * don't want other code relying on their existence.
 */
#define NAUTILUS_MENU_PATH_GENERAL_SETTINGS_ITEM	"/Settings/General Settings"
#define NAUTILUS_MENU_PATH_USE_EAZEL_THEME_ICONS_ITEM	"/Settings/Use Eazel Theme Icons"
#define NAUTILUS_MENU_PATH_DEBUG_MENU			"/Debug"
#define NAUTILUS_MENU_PATH_SHOW_COLOR_SELECTOR_ITEM	"/Debug/Show Color Selector"


static void
file_menu_new_window_callback (BonoboUIHandler *ui_handler, 
			       gpointer user_data, 
			       const char *path)
{
  NautilusWindow *current_mainwin;
  NautilusWindow *new_mainwin;

  g_return_if_fail (NAUTILUS_IS_WINDOW (user_data));
  
  current_mainwin = NAUTILUS_WINDOW (user_data);

  new_mainwin = nautilus_app_create_window (NAUTILUS_APP (current_mainwin->app));

  nautilus_window_goto_uri (new_mainwin, 
                            nautilus_window_get_requested_uri (current_mainwin));

  gtk_widget_show (GTK_WIDGET (new_mainwin));
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
	if (nautilus_undo_manager_can_undo ())
		nautilus_undo_manager_undo_last_transaction ();
}

static void
edit_menu_cut_callback (BonoboUIHandler *ui_handler, 
		  	gpointer user_data, 
		  	const char *path) 
{
	GtkWindow *main_window;

	g_assert (GTK_IS_WINDOW (user_data));
	main_window=GTK_WINDOW (user_data);

	if (GTK_IS_EDITABLE (main_window->focus_widget)) {
		gtk_editable_cut_clipboard (GTK_EDITABLE (main_window->focus_widget));
	}

}

static void
edit_menu_copy_callback (BonoboUIHandler *ui_handler, 
		   	 gpointer user_data,
		   	 const char *path) 
{
	GtkWindow *main_window;

	g_assert (GTK_IS_WINDOW (user_data));
	main_window=GTK_WINDOW (user_data);

	if (GTK_IS_EDITABLE (main_window->focus_widget)) {
		gtk_editable_copy_clipboard (GTK_EDITABLE (main_window->focus_widget));
	}

}


static void
edit_menu_paste_callback (BonoboUIHandler *ui_handler, 
		    	  gpointer user_data,
		    	  const char *path) 
{
	GtkWindow *main_window;

	main_window=GTK_WINDOW (user_data);
	if (GTK_IS_EDITABLE (main_window->focus_widget)) {
		gtk_editable_paste_clipboard (GTK_EDITABLE (main_window->focus_widget));
	}

}


static void
edit_menu_clear_callback (BonoboUIHandler *ui_handler, 
		    	  gpointer user_data,
		    	  const char *path) 
{
	GtkWindow *main_window;
	main_window=GTK_WINDOW (user_data);
	if (GTK_IS_EDITABLE (main_window->focus_widget)) {
		/* A negative index deletes until the end of the string */
		gtk_editable_delete_text (GTK_EDITABLE (main_window->focus_widget),0, -1);
	}

}

static void
go_menu_back_callback (BonoboUIHandler *ui_handler, 
		       gpointer user_data,
		       const char *path) 
{
	g_assert (NAUTILUS_IS_WINDOW (user_data));
	nautilus_window_go_back (NAUTILUS_WINDOW (user_data));
}

static void
go_menu_forward_callback (BonoboUIHandler *ui_handler, 
		       	  gpointer user_data,
		          const char *path) 
{
	g_assert (NAUTILUS_IS_WINDOW (user_data));
	nautilus_window_go_forward (NAUTILUS_WINDOW (user_data));
}

static void
go_menu_up_callback (BonoboUIHandler *ui_handler, 
		     gpointer user_data,
		     const char *path) 
{
	g_assert (NAUTILUS_IS_WINDOW (user_data));
	nautilus_window_go_up (NAUTILUS_WINDOW (user_data));
}

static void
go_menu_home_callback (BonoboUIHandler *ui_handler, 
		       gpointer user_data,
		       const char *path) 
{
	g_assert (NAUTILUS_IS_WINDOW (user_data));
	nautilus_window_go_home (NAUTILUS_WINDOW (user_data));
}

static void
bookmarks_menu_add_bookmark_callback (BonoboUIHandler *ui_handler, 
		       		      gpointer user_data,
		      		      const char *path)
{
	g_return_if_fail(NAUTILUS_IS_WINDOW (user_data));

        nautilus_window_add_bookmark_for_current_location (NAUTILUS_WINDOW (user_data));
}

static void
bookmarks_menu_edit_bookmarks_callback (BonoboUIHandler *ui_handler, 
		       		      	gpointer user_data,
		      		      	const char *path)
{
        g_return_if_fail (NAUTILUS_IS_WINDOW (user_data));

        nautilus_window_edit_bookmarks (NAUTILUS_WINDOW (user_data));
}

static void
settings_menu_general_settings_callback (BonoboUIHandler *ui_handler, 
		       		      	 gpointer user_data,
		      		      	 const char *path)
{
	nautilus_global_preferences_show_dialog ();
}

static void
settings_menu_use_eazel_theme_icons_callback (BonoboUIHandler *ui_handler, 
		       		      	      gpointer user_data,
		      		      	      const char *path)
{
	char *current_theme;
	char *new_theme;

	current_theme = nautilus_preferences_get (NAUTILUS_PREFERENCES_ICON_THEME, "default");	
	if (nautilus_strcmp (current_theme, "eazel") == 0) {
		new_theme = "default";
	} else {
		new_theme = "eazel";
	}

	nautilus_preferences_set (NAUTILUS_PREFERENCES_ICON_THEME, new_theme);
	
	g_free (current_theme);
}

static void
help_menu_about_nautilus_callback (BonoboUIHandler *ui_handler, 
		       		   gpointer user_data,
		      		   const char *path)
{
  static GtkWidget *aboot = NULL;

  if (aboot == NULL)
  {
    const char *authors[] = {
      "Darin Adler",
      "Ramiro Estrugo",
      "Andy Hertzfeld",
      "Elliot Lee",
      "Ettore Perazzoli",
      "Maciej Stachowiak",
      "John Sullivan",
      NULL
    };

    aboot = gnome_about_new(_("Nautilus"),
                            VERSION,
                            "Copyright (C) 1999, 2000",
                            authors,
                            _("The Cool Shell Program"),
                            "nautilus/nautilus3.jpg");

    gnome_dialog_close_hides (GNOME_DIALOG (aboot), TRUE);
  }

  nautilus_gtk_window_present (GTK_WINDOW (aboot));
}

/* handle the OK button being pushed on the color selector */
/* for now, just vanquish it, since it's only for testing */
static void
debug_color_confirm (GtkWidget *widget)
{
	gtk_widget_destroy (gtk_widget_get_toplevel (widget));
}

static void 
debug_menu_show_color_picker_callback (BonoboUIHandler *ui_handler, 
		       		       gpointer user_data,
		      		       const char *path)
{
	GtkWidget *c;

	c = gtk_color_selection_dialog_new (_("Color selector"));
	gtk_signal_connect (GTK_OBJECT (GTK_COLOR_SELECTION_DIALOG (c)->ok_button),
			    "clicked", GTK_SIGNAL_FUNC (debug_color_confirm), c);
	gtk_widget_hide (GTK_COLOR_SELECTION_DIALOG (c)->cancel_button);
	gtk_widget_hide (GTK_COLOR_SELECTION_DIALOG (c)->help_button);
	gtk_widget_show (c);
}

static void
activate_bookmark_in_menu_item (BonoboUIHandler *uih, gpointer user_data, const char *path)
{
        BookmarkHolder *holder = (BookmarkHolder *)user_data;
	
        nautilus_window_goto_uri (holder->window, 
                                  nautilus_bookmark_get_uri (holder->bookmark));
}

static void
append_bookmark_to_menu (NautilusWindow *window, 
                         const NautilusBookmark *bookmark, 
                         const char *menu_item_path)
{
	BookmarkHolder *bookmark_holder;	
	GdkPixbuf *pixbuf;
	BonoboUIHandlerPixmapType pixmap_type;
	char *name;

	/* Attempt to retrieve icon and mask for bookmark */
	pixbuf = nautilus_bookmark_get_pixbuf (bookmark, NAUTILUS_ICON_SIZE_SMALLER);

	/* Set up pixmap type based on result of function.  If we fail, set pixmap type to none */
	if (pixbuf != NULL) {
		pixmap_type = BONOBO_UI_HANDLER_PIXMAP_PIXBUF_DATA;
	} else {
		pixmap_type = BONOBO_UI_HANDLER_PIXMAP_NONE;
	}
	
	bookmark_holder = g_new (BookmarkHolder, 1);
	bookmark_holder->window = window;
	bookmark_holder->bookmark = bookmark;

	/* We double the underscores here to escape them so Bonobo will know they are
	 * not keyboard accelerator character prefixes. If we ever find we need to
	 * escape more than just the underscores, we'll add a menu helper function
	 * instead of a string utility. (Like maybe escaping control characters.)
	 */
	name = nautilus_str_double_underscores
		(nautilus_bookmark_get_name (bookmark));
 	bonobo_ui_handler_menu_new_item (window->uih,
					 menu_item_path,
					 name,
					 _("Go to the specified location"),
					 -1,
					 pixmap_type,
					 pixbuf,
					 0,
					 0,
					 activate_bookmark_in_menu_item,
					 bookmark_holder);
	g_free (name);
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

	children = bonobo_ui_handler_menu_get_child_paths (window->uih, menu_path);
	found_dynamic_items = FALSE;

	for (p = children; p != NULL; p = p->next) {
                if (found_dynamic_items) {
                        BookmarkHolder *holder;
                        BonoboUIHandlerCallbackFunc func;

                        bonobo_ui_handler_menu_get_callback (window->uih,
                                                             p->data,
                                                             &func,
                                                             (gpointer *)&holder);
                        g_free (holder);
                        bonobo_ui_handler_menu_remove (window->uih, p->data);
		}
		else if (strcmp ((const char *) p->data, last_static_item_path) == 0) {
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

static GtkWidget *
get_bookmarks_window (void)
{
	static GtkWidget *bookmarks_window = NULL;
	if (bookmarks_window == NULL)
	{
		bookmarks_window = create_bookmarks_window (get_bookmark_list ());
	}
	g_assert (GTK_IS_WINDOW (bookmarks_window));
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
	nautilus_bookmarks_window_save_geometry (get_bookmarks_window ());
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
		          nautilus_window_get_requested_uri(window)) == 0);
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

void
nautilus_window_edit_bookmarks (NautilusWindow *window)
{
        nautilus_gtk_window_present (GTK_WINDOW (get_bookmarks_window ()));
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
        bonobo_ui_handler_menu_new_separator (window->uih,
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
	bonobo_ui_handler_menu_new_subtree (window->uih,
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

        ui_handler = window->uih;
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
        				 NULL);

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

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_GENERAL_SETTINGS_ITEM,
        				 _("_General Settings..."),
        				 _("Customize various aspects of Nautilus's appearance and behavior"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 0,
        				 0,
        				 settings_menu_general_settings_callback,
        				 NULL);

	/* It's called SEPARATOR_AFTER_USER_LEVELS because "General Settings" is
	 * going to expand into the user level choices plus the choice that brings
	 * up the user-level-details customizing dialog.
	 */
        append_separator (window, NAUTILUS_MENU_PATH_SEPARATOR_AFTER_USER_LEVELS);

        bonobo_ui_handler_menu_new_toggleitem (ui_handler,
        				       NAUTILUS_MENU_PATH_USE_EAZEL_THEME_ICONS_ITEM,
        				       _("Use _Eazel Theme Icons"),
        				       _("Select whether to use standard or Eazel icons"),
        				       -1,
        				       0,
        				       0,
        				       settings_menu_use_eazel_theme_icons_callback,
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

	/* Debug */
        new_top_level_menu (window, NAUTILUS_MENU_PATH_DEBUG_MENU, _("_Debug"));

        bonobo_ui_handler_menu_new_item (ui_handler,
        				 NAUTILUS_MENU_PATH_SHOW_COLOR_SELECTOR_ITEM,
        				 _("Show _Color selector..."),
        				 _("Show the color picker window"),
        				 -1,
        				 BONOBO_UI_HANDLER_PIXMAP_NONE,
        				 NULL,
        				 0,
        				 0,
        				 debug_menu_show_color_picker_callback,
        				 NULL);


        /* Desensitize the items that aren't implemented at this level.
         * Some (hopefully all) will be overridden by implementations by the
         * different content views.
         */
	bonobo_ui_handler_menu_set_sensitivity(ui_handler, 
        				       NAUTILUS_MENU_PATH_SELECT_ALL_ITEM, 
        				       FALSE);

        /* Set initial toggle state of Eazel theme menu item */
        update_eazel_theme_menu_item (window);

        /* Set inital state of undo menu */
	update_undo_menu_item(window);
        
        /* Sign up to be notified of icon theme changes so Use Eazel Theme Icons
         * menu item will show correct toggle state. */        
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       update_eazel_theme_menu_item,
					       GTK_OBJECT (window));	

	/* Connect to UndoManager so that we are notified when an undo transcation has occurred */
	gtk_signal_connect_object_while_alive (GTK_OBJECT(nautilus_undo_manager_get_undo_manager ()),
					       "undo_transaction_occurred",
					       update_undo_menu_item,
					       GTK_OBJECT (window));	

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
		                         path); 
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
                                         path);
                g_free (path);

                ++index;
	}	
}

static void 
update_eazel_theme_menu_item (NautilusWindow *window)
{
        g_assert (NAUTILUS_IS_WINDOW (window));

	/* Change the state of the menu item without invoking our callback function. */
	nautilus_bonobo_ui_handler_menu_set_toggle_appearance (
		window->uih,
		NAUTILUS_MENU_PATH_USE_EAZEL_THEME_ICONS_ITEM,
		nautilus_eat_strcmp (nautilus_preferences_get (NAUTILUS_PREFERENCES_ICON_THEME,
							       "default"), 
				     "eazel") == 0);
}


/* Toggle sensitivity based on undo manager state */
static void 
update_undo_menu_item (NautilusWindow *window)
{
        g_assert (NAUTILUS_IS_WINDOW (window));

	bonobo_ui_handler_menu_set_sensitivity(window->uih, NAUTILUS_MENU_PATH_UNDO_ITEM, 
        				       nautilus_undo_manager_can_undo ());
}

