/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bookmarks-menu.c - implementation of Bookmarks menu.

   Copyright (C) 1999 Eazel, Inc.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include "nautilus.h"
#include "nautilus-bookmarks-menu.h"
#include "nautilus-bookmarklist.h"

/* object data strings */
#define LAST_STATIC_ITEM	"last static item"
#define WINDOW_TO_UPDATE	"window to update"


/* forward declarations for static functions */

static void add_bookmark_cb				(GtkMenuItem *, gpointer);
static void bookmark_activated_cb			(GtkMenuItem *, gpointer);
static void list_changed_cb				(NautilusBookmarklist *, 
						 	 gpointer);
static void nautilus_bookmarks_menu_append		(NautilusBookmarksMenu *,
							 GtkWidget *);
static void nautilus_bookmarks_menu_clear_bookmarks	(NautilusBookmarksMenu *);
static void nautilus_bookmarks_menu_repopulate		(NautilusBookmarksMenu *);

/* static variables */

static GtkMenuClass 	    *parent_class = NULL;
static NautilusBookmarklist *bookmarks = NULL;


/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
finalize (GtkObject *object)
{
	if (GTK_OBJECT_CLASS (parent_class)->finalize != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Initialization.  */

static void
class_init (NautilusBookmarksMenuClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	parent_class = gtk_type_class (GTK_TYPE_MENU);
	bookmarks = nautilus_bookmarklist_new();

	object_class->destroy = destroy;
	object_class->finalize = finalize;
}

static void
init (NautilusBookmarksMenu *bookmarks_menu)
{
	GtkWidget *item;

	if (gnome_preferences_get_menus_have_tearoff())
	{
		nautilus_bookmarks_menu_append(bookmarks_menu, 
					       gtk_tearoff_menu_item_new());
	}

	item = gtk_menu_item_new_with_label(_("Add Bookmark"));
	gtk_signal_connect(GTK_OBJECT (item), "activate",
			   GTK_SIGNAL_FUNC (add_bookmark_cb),
			   bookmarks_menu);
	nautilus_bookmarks_menu_append(bookmarks_menu, item);

	item = gtk_menu_item_new_with_label(_("Edit Bookmarks..."));
	/* FIXME: Implement this, currently marked insensitive until implemented.
	 * I will do this soon, but wanted to get in first cut at other bookmark
	 * stuff before leaving for holidays.
	 */
	gtk_widget_set_sensitive(GTK_WIDGET(item), FALSE);
	nautilus_bookmarks_menu_append(bookmarks_menu, item);

	item = gtk_menu_item_new();
	/* mark this menu item specially so we can recognize it later */
	gtk_object_set_data(GTK_OBJECT(item), 
			    LAST_STATIC_ITEM, 
			    GINT_TO_POINTER(TRUE));
	nautilus_bookmarks_menu_append(bookmarks_menu, item);

	gtk_signal_connect(GTK_OBJECT(bookmarks),
			   "contents_changed",
			   GTK_SIGNAL_FUNC(list_changed_cb),
			   bookmarks_menu);

	nautilus_bookmarks_menu_repopulate(bookmarks_menu);
}

static void
add_bookmark_cb(GtkMenuItem* item, gpointer func_data)
{
	NautilusBookmarksMenu *bookmarks_menu;
	NautilusBookmark *bookmark;
	const char* current_uri;

	g_return_if_fail(func_data != NULL);
	g_return_if_fail(NAUTILUS_IS_BOOKMARKS_MENU (func_data));

	bookmarks_menu = NAUTILUS_BOOKMARKS_MENU (func_data);

	current_uri = nautilus_window_get_requested_uri(bookmarks_menu->window);

	/* FIXME: initial name should be extracted from http document title (e.g.) */
	bookmark = nautilus_bookmark_new(current_uri, current_uri);
	nautilus_bookmarklist_append(bookmarks, bookmark);
	gtk_object_destroy(GTK_OBJECT(bookmark));
}

static void
bookmark_activated_cb(GtkMenuItem* item, gpointer func_data)
{
	NautilusWindow *window;
	NautilusBookmark *bookmark;

	g_return_if_fail(gtk_object_get_data(GTK_OBJECT(item), WINDOW_TO_UPDATE) != NULL);
	g_return_if_fail(NAUTILUS_IS_WINDOW(gtk_object_get_data(GTK_OBJECT(item), WINDOW_TO_UPDATE)));
	g_return_if_fail(func_data != NULL);
	g_return_if_fail(NAUTILUS_IS_BOOKMARK (func_data));

	window = NAUTILUS_WINDOW(gtk_object_get_data(GTK_OBJECT(item), WINDOW_TO_UPDATE));
	bookmark = NAUTILUS_BOOKMARK(func_data);

	/* FIXME: should check whether we know this to be an invalid uri.
	 * If so, don't try to go there, and put up an alert asking user if
	 * they want to edit bookmarks (or maybe remove this one).
	 */
	nautilus_window_goto_uri(window, nautilus_bookmark_get_uri(bookmark));

	/* FIXME: bookmark created for this signal is never destroyed. */
}

static void
list_changed_cb(NautilusBookmarklist *bookmarks, gpointer data)
{
	NautilusBookmarksMenu *menu;
	
	g_return_if_fail(bookmarks != NULL);
	g_return_if_fail(NAUTILUS_IS_BOOKMARKLIST(bookmarks));
	g_return_if_fail(data != NULL);
	g_return_if_fail(NAUTILUS_IS_BOOKMARKS_MENU(data));

	menu = NAUTILUS_BOOKMARKS_MENU(data);

	nautilus_bookmarks_menu_repopulate(menu);
}


GtkType
nautilus_bookmarks_menu_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static GtkTypeInfo info = {
			"NautilusBookmarksMenu",
			sizeof (NautilusBookmarksMenu),
			sizeof (NautilusBookmarksMenuClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			NULL,
			NULL,
			NULL
		};

		type = gtk_type_unique (GTK_TYPE_MENU, &info);
	}

	return type;
}

static void
nautilus_bookmarks_menu_append(NautilusBookmarksMenu *bookmarks_menu, GtkWidget *item)
{
	g_return_if_fail(bookmarks_menu != NULL);
	g_return_if_fail(GTK_IS_MENU_ITEM(item));

	gtk_widget_show(item);
	gtk_menu_append(&(bookmarks_menu->menu), item);
}

/**
 * nautilus_bookmarks_menu_clear_bookmarks:
 * 
 * Remove all bookmarks from the menu, leaving static items intact.
 * @menu: The NautilusBookmarksMenu whose bookmarks will be removed.
 **/
static void
nautilus_bookmarks_menu_clear_bookmarks (NautilusBookmarksMenu *menu)
{
	GList *children;
	GList *iter;
	gboolean found_dynamic_items;

	g_return_if_fail(menu != NULL);
	g_return_if_fail(NAUTILUS_IS_BOOKMARKS_MENU(menu));

	children = gtk_container_children(GTK_CONTAINER(menu));
	iter = children;

	found_dynamic_items = FALSE;
	while (iter != NULL)
	{
		if (found_dynamic_items)
		{
			gtk_container_remove(GTK_CONTAINER(menu), iter->data);
		}
		else if (gtk_object_get_data(iter->data, LAST_STATIC_ITEM))
		{
			found_dynamic_items = TRUE;
		}
		iter = g_list_next(iter);
	}
	
	g_list_free(children);
}

/**
 * nautilus_bookmarks_menu_new:
 * 
 * Create a new bookmarks menu for use in @window. It's the caller's
 * responsibility to install the menu into the window's menubar. 
 * @window: The NautilusWindow in which the menu will be installed
 *
 * Return value: A pointer to the new widget.
 **/
GtkWidget *
nautilus_bookmarks_menu_new (NautilusWindow *window)
{
	NautilusBookmarksMenu *new;

	g_return_val_if_fail (window != NULL, NULL);

	new = gtk_type_new (NAUTILUS_TYPE_BOOKMARKS_MENU);
	new->window = window;

	return GTK_WIDGET (new);
}

/**
 * nautilus_bookmarks_menu_repopulate:
 * 
 * Refresh list of bookmarks at end of menu to match centralized list.
 * @menu: The NautilusBookmarksMenu whose contents will be refreshed.
 **/
static void
nautilus_bookmarks_menu_repopulate (NautilusBookmarksMenu *menu)
{
	guint 	bookmark_count;
	gint	index;
	
	g_return_if_fail(menu != NULL);
	g_return_if_fail(NAUTILUS_IS_BOOKMARKS_MENU(menu));

	nautilus_bookmarks_menu_clear_bookmarks(menu);

	/* append new set of bookmarks */

	bookmark_count = nautilus_bookmarklist_length(bookmarks);
	for (index = 0; index < bookmark_count; ++index)
	{
		const NautilusBookmark *bookmark;
		GtkWidget *item;

		bookmark = nautilus_bookmarklist_item_at(bookmarks, index);
		item = gtk_menu_item_new_with_label(
			nautilus_bookmark_get_name(bookmark));
		/* remember the window this menu is attached to */
		gtk_object_set_data(GTK_OBJECT(item),
				    WINDOW_TO_UPDATE,
				    menu->window);
		gtk_signal_connect(GTK_OBJECT (item), "activate",
				   GTK_SIGNAL_FUNC (bookmark_activated_cb),
				   nautilus_bookmark_new(
				   	nautilus_bookmark_get_name(bookmark),
				   	nautilus_bookmark_get_uri(bookmark)));
		nautilus_bookmarks_menu_append(menu, item);
	}	
}
