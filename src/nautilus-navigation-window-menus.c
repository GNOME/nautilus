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

#include "nautilus-bookmarklist.h"
#include "nautilus-bookmarks-window.h"
#include "nautilus-signaller.h"
#include "ntl-window-private.h"

#include <libnautilus/nautilus-gtk-extensions.h>
#include <libnautilus/nautilus-icon-factory.h>

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
static NautilusBookmarklist *get_bookmark_list                   ();
static GtkWidget            *get_bookmarks_window                ();
static void                  refresh_bookmarks_in_go_menu        (NautilusWindow *window);
static void                  refresh_bookmarks_in_bookmarks_menu (NautilusWindow *window);


/* Struct that stores all the info necessary to activate a bookmark. */
typedef struct {
        const NautilusBookmark *bookmark;
        NautilusWindow *window;
} BookmarkHolder;


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

	bookmark_holder = g_new (BookmarkHolder, 1);
	bookmark_holder->window = window;
	bookmark_holder->bookmark = bookmark;

        /* FIXME: Need to get the bookmark's icon in here, but how? */
        bonobo_ui_handler_menu_new_item (window->uih,
                                         menu_item_path,
                                         nautilus_bookmark_get_name (bookmark),
                                         _("Go to the specified location"),
                                         -1,
                                         BONOBO_UI_HANDLER_PIXMAP_NONE,
                                         NULL,
                                         0,
                                         0,
                                         activate_bookmark_in_menu_item,
                                         bookmark_holder);
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
	GList *children;
	GList *iterator;
	gboolean found_dynamic_items;

	g_assert (NAUTILUS_IS_WINDOW (window));

	children = bonobo_ui_handler_menu_get_child_paths (window->uih, menu_path);
	iterator = children;
	found_dynamic_items = FALSE;

	while (iterator != NULL)
	{
                if (found_dynamic_items) {
                        BookmarkHolder *holder;
                        BonoboUIHandlerCallbackFunc func;

                        bonobo_ui_handler_menu_get_callback (window->uih,
                                                             iterator->data,
                                                             &func,
                                                             (gpointer *)&holder);
                        if (holder != NULL) {
                                g_free (holder);
                        }
                        bonobo_ui_handler_menu_remove (window->uih, iterator->data);
		}
		else if (strcmp (iterator->data, last_static_item_path) == 0) {
			found_dynamic_items = TRUE;
		}
		iterator = g_list_next (iterator);
	}

	g_assert (found_dynamic_items);
        g_list_foreach (children, (GFunc)g_free, NULL);
	g_list_free (children);
}

static NautilusBookmarklist *
get_bookmark_list ()
{
        static NautilusBookmarklist *bookmarks = NULL;

        if (bookmarks == NULL) {
                bookmarks = nautilus_bookmarklist_new ();
        }

        return bookmarks;
}

static GtkWidget *
get_bookmarks_window ()
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
nautilus_bookmarks_exiting ()
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

	bookmark = nautilus_bookmark_new (nautilus_window_get_requested_uri(window));

	if (!nautilus_bookmarklist_contains (get_bookmark_list (), bookmark))
	{
		nautilus_bookmarklist_append (get_bookmark_list (), bookmark);
	}
	gtk_object_destroy(GTK_OBJECT(bookmark));
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
void 
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
					       "theme_changed",
					       refresh_bookmarks_in_bookmarks_menu,
					       GTK_OBJECT (window));

}

/**
 * nautilus_window_initialize_go_menu
 * 
 * Wire up signals so we'll be notified when history list changes.
 */
void 
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
					       "theme_changed",
					       refresh_bookmarks_in_go_menu,
					       GTK_OBJECT (window));

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
        NautilusBookmarklist *bookmarks;
	guint 	bookmark_count;
	guint	index;
	
	g_assert (NAUTILUS_IS_WINDOW (window));

	bookmarks = get_bookmark_list ();

	/* Remove old set of bookmarks. */
	clear_appended_bookmark_items (window, "/Bookmarks", "/Bookmarks/Edit Bookmarks...");

	bookmark_count = nautilus_bookmarklist_length (bookmarks);

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
		                         nautilus_bookmarklist_item_at (bookmarks, index), 
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
	GSList *iterator;
	int index;

	g_assert (NAUTILUS_IS_WINDOW (window));

	/* Remove old set of history items. */
	clear_appended_bookmark_items (window, "/Go", "/Go/Home");

        /* Add separator before history items */
        append_separator_to_menu (window, "/Go/Separator");                                                                                  

	/* Add in a new set of history items. */
	index = 0;
	for (iterator = nautilus_get_history_list (); iterator != NULL; iterator = g_slist_next (iterator))
	{
		char *path;

		path = g_strdup_printf ("/Go/History%d", index); 
                append_bookmark_to_menu (window,
                                         NAUTILUS_BOOKMARK (iterator->data),
                                         path);
                g_free (path);

                ++index;
	}	
}

