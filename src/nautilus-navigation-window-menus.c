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
static void                  append_separator_to_menu            (NautilusWindow *window, 
                                                                  const char *separator_path);
static void                  clear_appended_bookmark_items       (NautilusWindow *window, 
                                                                  const char *menu_path, 
                                                                  const char *last_static_item_path);
static NautilusBookmarkList *get_bookmark_list                   (void);
static GtkWidget            *get_bookmarks_window                (void);
static void                  nautilus_window_about_cb            (GtkWidget *widget,
                                                                  NautilusWindow *window);
static void                  debug_menu_show_color_picker_cb     (GtkWidget *widget, 
                                                                  NautilusWindow *window);
static void                  refresh_bookmarks_in_go_menu        (NautilusWindow *window);
static void                  refresh_bookmarks_in_bookmarks_menu (NautilusWindow *window);
static void                  update_eazel_theme_menu_item        (NautilusWindow *window);


/* Struct that stores all the info necessary to activate a bookmark. */
typedef struct {
        const NautilusBookmark *bookmark;
        NautilusWindow *window;
} BookmarkHolder;


/* menu definitions */

static void
file_menu_close_cb (GtkWidget *widget,
                    gpointer data)
{
        g_assert (NAUTILUS_IS_WINDOW (data));
	nautilus_window_close (NAUTILUS_WINDOW (data));
}

static void
file_menu_new_window_cb (GtkWidget *widget,
                         gpointer data)
{
  NautilusWindow *current_mainwin;
  NautilusWindow *new_mainwin;

  g_return_if_fail(NAUTILUS_IS_WINDOW(data));
  
  current_mainwin = NAUTILUS_WINDOW(data);

  new_mainwin = nautilus_app_create_window(NAUTILUS_APP(current_mainwin->app));

  nautilus_window_goto_uri(new_mainwin, 
                           nautilus_window_get_requested_uri(current_mainwin));

  gtk_widget_show(GTK_WIDGET(new_mainwin));
}

static void
file_menu_exit_cb (GtkWidget *widget,
                   gpointer data)
{
	gtk_main_quit ();
}

static void
general_settings_cb (GtkWidget *widget,
                     GtkWindow *mainwin)
{
	nautilus_global_preferences_show_dialog ();
}

static GnomeUIInfo file_menu_info[] = {
  {
    GNOME_APP_UI_ITEM,
    N_("New Window"), N_("Create a new window"),
    file_menu_new_window_cb, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    'N', GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_MENU_CLOSE_ITEM(file_menu_close_cb, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_EXIT_ITEM(file_menu_exit_cb, NULL),
  GNOMEUIINFO_END
};

/* FIXME: These all need implementation, though we might end up doing that
 * separately for each content view (and merging with the insensitive items here)
 */
static GnomeUIInfo edit_menu_info[] = {
  GNOMEUIINFO_MENU_UNDO_ITEM(NULL, NULL),
  GNOMEUIINFO_SEPARATOR,
  GNOMEUIINFO_MENU_CUT_ITEM(NULL, NULL),
  GNOMEUIINFO_MENU_COPY_ITEM(NULL, NULL),
  GNOMEUIINFO_MENU_PASTE_ITEM(NULL, NULL),
  GNOMEUIINFO_MENU_CLEAR_ITEM(NULL, NULL),
  GNOMEUIINFO_SEPARATOR,
  /* Didn't use standard SELECT_ALL_ITEM 'cuz it didn't have accelerator */
  {
    GNOME_APP_UI_ITEM,
    N_("Select All"), NULL,
    NULL, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    'A', GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo go_menu_info[] = {
  {
    GNOME_APP_UI_ITEM,
    N_("Back"), N_("Go to the previous visited location"),
    nautilus_window_back_cb, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    'B', GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM,
    N_("Forward"), N_("Go to the next visited location"),
    nautilus_window_forward_cb, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    'F', GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM,
    N_("Up"), N_("Go to the location that contains this one"),
    nautilus_window_up_cb, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    'U', GDK_CONTROL_MASK, NULL
  },
  {
    GNOME_APP_UI_ITEM,
    N_("Home"), N_("Go to the home location"),
    nautilus_window_home_cb, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    'H', GDK_CONTROL_MASK, NULL
  },
  GNOMEUIINFO_END
};

static void
add_bookmark_cb (GtkMenuItem* item, gpointer func_data)
{
	g_return_if_fail(NAUTILUS_IS_WINDOW (func_data));

        nautilus_window_add_bookmark_for_current_location (NAUTILUS_WINDOW (func_data));
}

/* handle the OK button being pushed on the color selector */
/* for now, just vanquish it, since it's only for testing */
static void
debug_color_confirm (GtkWidget *widget)
{
	gtk_widget_destroy (gtk_widget_get_toplevel (widget));
}

static void 
debug_menu_show_color_picker_cb (GtkWidget *btn, NautilusWindow *window)
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
edit_bookmarks_cb(GtkMenuItem* item, gpointer user_data)
{
        g_return_if_fail (NAUTILUS_IS_WINDOW (user_data));

        nautilus_window_edit_bookmarks (NAUTILUS_WINDOW (user_data));
}

static GnomeUIInfo bookmarks_menu_info[] = {
  { 
    GNOME_APP_UI_ITEM, 
    N_("_Add Bookmark"),
    N_("Add a bookmark for the current location to this menu."),
    (gpointer)add_bookmark_cb, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL, 0, (GdkModifierType) 0, NULL
  },
  GNOMEUIINFO_ITEM_NONE(N_("_Edit Bookmarks..."), 
  		        N_("Display a window that allows editing the bookmarks in this menu."), 
		        edit_bookmarks_cb),
  GNOMEUIINFO_END
};

static void
use_eazel_theme_icons_cb (GtkCheckMenuItem *item, gpointer user_data)
{
	char *current_theme;
	char *new_theme;

	current_theme = nautilus_preferences_get (nautilus_preferences_get_global_preferences (),
						  NAUTILUS_PREFERENCES_ICON_THEME,
						  "default");	
	if (nautilus_strcmp (current_theme, "eazel") == 0) {
		new_theme = "default";
	} else {
		new_theme = "eazel";
	}

	nautilus_preferences_set (nautilus_preferences_get_global_preferences (),
				  NAUTILUS_PREFERENCES_ICON_THEME,
				  new_theme);
	
	g_free (current_theme);
}

static GnomeUIInfo settings_menu_info[] = {
  {
    GNOME_APP_UI_ITEM,
    N_("_General Settings..."), N_("Customize various aspects of Nautilus's appearance and behavior"),
    general_settings_cb, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL,
    0, 0, NULL
  },
  GNOMEUIINFO_SEPARATOR,
  { 
    GNOME_APP_UI_TOGGLEITEM, 
    N_("Use _Eazel Theme Icons"),
    N_("Select whether to use standard or Eazel icons"), 
    use_eazel_theme_icons_cb, NULL, NULL,
    GNOME_APP_PIXMAP_NONE, NULL, 
    0, (GdkModifierType)0, NULL 
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo help_menu_info[] = {
  {
    GNOME_APP_UI_ITEM,
    N_("About Nautilus..."), N_("Info about the Nautilus program"),
    nautilus_window_about_cb, NULL, NULL,
    GNOME_APP_PIXMAP_STOCK, GNOME_STOCK_MENU_ABOUT,
    0, 0, NULL
  },
  GNOMEUIINFO_END
};

static GnomeUIInfo debug_menu_info [] = {
	GNOMEUIINFO_ITEM_NONE (N_("Show Color selector..."), N_("Show the color picker window"), debug_menu_show_color_picker_cb),
	GNOMEUIINFO_END
};


static GnomeUIInfo main_menu[] = {
  GNOMEUIINFO_MENU_FILE_TREE (file_menu_info),
  GNOMEUIINFO_MENU_EDIT_TREE (edit_menu_info),
  GNOMEUIINFO_SUBTREE(N_("_Go"), go_menu_info),
  GNOMEUIINFO_SUBTREE(N_("_Bookmarks"), bookmarks_menu_info),
  GNOMEUIINFO_MENU_SETTINGS_TREE (settings_menu_info),
  GNOMEUIINFO_MENU_HELP_TREE (help_menu_info),
  GNOMEUIINFO_SUBTREE(N_("_Debug"), debug_menu_info),
  GNOMEUIINFO_END
};


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

static void
append_separator_to_menu (NautilusWindow *window, const char *separator_path)
{
        bonobo_ui_handler_menu_new_item (window->uih,
                                         separator_path,
                                         NULL,
                                         NULL,
                                         -1,
                                         BONOBO_UI_HANDLER_PIXMAP_NONE,
                                         NULL,
                                         0,
                                         0,
                                         NULL,
                                         NULL);
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
 * nautilus_window_about_cb:
 * 
 * Display about box, creating it first if necessary. Callback used when 
 * user selects "About Nautilus".
 * @widget: ignored
 * @window: ignored
 **/
static void
nautilus_window_about_cb (GtkWidget *widget,
                          NautilusWindow *window)
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
        BonoboUIHandlerMenuItem *menu_items;

        ui_handler = window->uih;
        g_assert (ui_handler != NULL);
        
        bonobo_ui_handler_create_menubar (ui_handler);

        /* Convert the menu items from their GnomeUIInfo form.
         * Maybe we should eliminate this initial form altogether.
         */
        menu_items = bonobo_ui_handler_menu_parse_uiinfo_list_with_data (main_menu, window);
        bonobo_ui_handler_menu_add_list (ui_handler, "/", menu_items);
        bonobo_ui_handler_menu_free_list (menu_items);

        /* Desensitize the items that aren't implemented at this level.
         * Some (hopefully all) will be overridden by implementations by the
         * different content views.
         */
        bonobo_ui_handler_menu_set_sensitivity(ui_handler, "/Edit/Undo", FALSE);
        bonobo_ui_handler_menu_set_sensitivity(ui_handler, "/Edit/Cut", FALSE);
        bonobo_ui_handler_menu_set_sensitivity(ui_handler, "/Edit/Copy", FALSE);
        bonobo_ui_handler_menu_set_sensitivity(ui_handler, "/Edit/Paste", FALSE);
        bonobo_ui_handler_menu_set_sensitivity(ui_handler, "/Edit/Clear", FALSE);
        bonobo_ui_handler_menu_set_sensitivity(ui_handler, "/Edit/Select All", FALSE);

        /* Set initial toggle state of Eazel theme menu item */
        update_eazel_theme_menu_item (window);
        
        /* Sign up to be notified of icon theme changes so Use Eazel Theme Icons
         * menu item will show correct toggle state. */        
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       update_eazel_theme_menu_item,
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
	clear_appended_bookmark_items (window, "/Bookmarks", "/Bookmarks/Edit Bookmarks...");

	bookmark_count = nautilus_bookmark_list_length (bookmarks);

        /* Add separator before bookmarks, unless there are no bookmarks. */
        if (bookmark_count > 0) {
                append_separator_to_menu (window, "/Bookmarks/Separator");
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
	clear_appended_bookmark_items (window, "/Go", "/Go/Home");

        /* Add separator before history items */
        append_separator_to_menu (window, "/Go/Separator");                                                                                  

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
		"/Settings/Use Eazel Theme Icons",
		nautilus_eat_strcmp (nautilus_preferences_get (nautilus_preferences_get_global_preferences (),
							       NAUTILUS_PREFERENCES_ICON_THEME,
							       "default"), 
				     "eazel") == 0);
}
