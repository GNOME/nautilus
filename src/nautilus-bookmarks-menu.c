/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bookmarks-menu.c - implementation of Bookmarks menu.

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

   Authors: John Sullivan <sullivan@eazel.com>
*/

#include "nautilus.h"
#include "nautilus-bookmarks-menu.h"

#include "nautilus-bookmarklist.h"
#include "nautilus-bookmarks-window.h"
#include <libnautilus/nautilus-gtk-extensions.h>
#include <libnautilus/nautilus-icon-factory.h>

/* object data strings */
#define LAST_STATIC_ITEM	"last static item"
#define OWNING_MENU		"owning menu"


/* forward declarations for static functions */

static void add_bookmark_cb				(GtkMenuItem *, gpointer);
static void bookmark_activated_cb			(GtkMenuItem *, gpointer);
static void edit_bookmarks_cb				(GtkMenuItem *, gpointer);
static void list_changed_cb				(NautilusBookmarklist *, 
						 	 gpointer);
static GtkWidget *get_bookmarks_window			(void);
static void nautilus_bookmarks_menu_append		(NautilusBookmarksMenu *,
							 GtkWidget *);
static void nautilus_bookmarks_menu_clear_bookmarks	(NautilusBookmarksMenu *);
static void nautilus_bookmarks_menu_fill		(NautilusBookmarksMenu *);
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
	/* All the work is done in nautilus_bookmarks_new(). This is a little
	 * unpleasant, but required so that the initialization code can access
	 * the window parameter to _new().
	 */
}

static void
add_bookmark_cb(GtkMenuItem* item, gpointer func_data)
{
	NautilusBookmarksMenu *bookmarks_menu;
	NautilusBookmark *bookmark;
	const char* current_uri;

	g_return_if_fail(NAUTILUS_IS_BOOKMARKS_MENU (func_data));

	bookmarks_menu = NAUTILUS_BOOKMARKS_MENU (func_data);

	current_uri = nautilus_window_get_requested_uri(bookmarks_menu->window);

	bookmark = nautilus_bookmark_new(current_uri);

	if (!nautilus_bookmarklist_contains(bookmarks, bookmark))
	{
		nautilus_bookmarklist_append(bookmarks, bookmark);
	}
	gtk_object_destroy(GTK_OBJECT(bookmark));
}

static void
bookmark_activated_cb(GtkMenuItem* item, gpointer func_data)
{
	NautilusBookmarksMenu  *menu;
	guint		  	item_index;

	g_return_if_fail(NAUTILUS_IS_BOOKMARKS_MENU(gtk_object_get_data(GTK_OBJECT(item), OWNING_MENU)));

	menu = NAUTILUS_BOOKMARKS_MENU(gtk_object_get_data(GTK_OBJECT(item), OWNING_MENU));
	item_index = GPOINTER_TO_UINT(func_data);

	/* FIXME: should check whether we know this to be an invalid uri.
	 * If so, don't try to go there, and put up an alert asking user if
	 * they want to edit bookmarks (or maybe remove this one).
	 */
	nautilus_window_goto_uri(menu->window, 
				 nautilus_bookmark_get_uri(
				 	nautilus_bookmarklist_item_at(bookmarks, item_index)));
}

static void
edit_bookmarks_cb(GtkMenuItem* item, gpointer ignored)
{
	nautilus_gtk_window_present (GTK_WINDOW (get_bookmarks_window()));
}

static GtkWidget *
get_bookmarks_window()
{
	static GtkWidget *bookmarks_window = NULL;
	if (bookmarks_window == NULL)
	{
		bookmarks_window = create_bookmarks_window(bookmarks);
	}
	g_assert(GTK_IS_WINDOW(bookmarks_window));
	return bookmarks_window;
}

static void
list_changed_cb(NautilusBookmarklist *bookmarks, gpointer data)
{
	NautilusBookmarksMenu *menu;
	
	g_return_if_fail(NAUTILUS_IS_BOOKMARKLIST(bookmarks));
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

	g_return_if_fail (NAUTILUS_IS_BOOKMARKS_MENU (menu));

	children = gtk_container_children (GTK_CONTAINER (menu));
	iter = children;

	found_dynamic_items = FALSE;
	while (iter != NULL)
	{
		if (found_dynamic_items)
		{
			gtk_container_remove (GTK_CONTAINER (menu), iter->data);
		}
		else if (gtk_object_get_data (iter->data, LAST_STATIC_ITEM))
		{
			found_dynamic_items = TRUE;
		}
		iter = g_list_next (iter);
	}
	
	g_list_free (children);
}

/**
 * nautilus_bookmarks_menu_exiting:
 * 
 * Last chance to save state before app exits.
 * Called when application exits; don't call from anywhere else.
 **/
void
nautilus_bookmarks_menu_exiting ()
{
	nautilus_bookmarks_window_save_geometry (get_bookmarks_window ());
}

/**
 * nautilus_bookmarks_menu_fill:
 * 
 * Starting with a completely empty menu, adds in the static items
 * then one for each bookmark. Broken out only for clarity; don't call outside of
 * nautilus_bookmarks_menu_new().
 * @menu: The freshly-created NautilusBookmarksMenu that needs to be filled in.
 **/
static void
nautilus_bookmarks_menu_fill (NautilusBookmarksMenu *menu)
{
	GtkWidget 	      *item; 
	gboolean   	       has_tearoff_item;
	GnomeUIInfo	       static_items[] = 
	{
		{
		GNOME_APP_UI_ITEM, 
		N_("_Add Bookmark"),
		N_("Add a bookmark for the current location to this menu."),
		(gpointer)add_bookmark_cb, menu, NULL,
		GNOME_APP_PIXMAP_NONE, NULL, 0, (GdkModifierType) 0, NULL
		},
		GNOMEUIINFO_ITEM_NONE(N_("_Edit Bookmarks..."), 
				      N_("Display a window that allows editing the bookmarks in this menu."), 
				      edit_bookmarks_cb),
		GNOMEUIINFO_END
		};

	g_return_if_fail(NAUTILUS_IS_BOOKMARKS_MENU(menu));
	g_return_if_fail(NAUTILUS_IS_WINDOW(menu->window));

	has_tearoff_item = gnome_preferences_get_menus_have_tearoff();
	if (has_tearoff_item)
	{
		nautilus_bookmarks_menu_append(menu, 
					       gtk_tearoff_menu_item_new());
	}

	gnome_app_fill_menu(GTK_MENU_SHELL(menu), 
			    static_items, 
			    (GNOME_APP(menu->window))->accel_group, 
			    TRUE,
			    has_tearoff_item ? 1 : 0);
	gnome_app_install_menu_hints(GNOME_APP(menu->window), static_items);

	item = gtk_menu_item_new();
	/* mark this menu item specially so we can recognize it later */
	gtk_object_set_data(GTK_OBJECT(item), 
			    LAST_STATIC_ITEM, 
			    GINT_TO_POINTER(TRUE));
	nautilus_bookmarks_menu_append(menu, item);

	gtk_signal_connect_while_alive(GTK_OBJECT(bookmarks),
			   	       "contents_changed",
			   	       GTK_SIGNAL_FUNC(list_changed_cb),
			   	       menu,
			   	       GTK_OBJECT(menu));

	nautilus_bookmarks_menu_repopulate(menu);		
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
	NautilusBookmarksMenu *new_bookmarks_menu;

	g_return_val_if_fail (NAUTILUS_IS_WINDOW (window), NULL);

	new_bookmarks_menu = gtk_type_new (NAUTILUS_TYPE_BOOKMARKS_MENU);
	new_bookmarks_menu->window = window;

	nautilus_bookmarks_menu_fill(new_bookmarks_menu);

	/* Register to find out about icon theme changes */
	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "theme_changed",
					       nautilus_bookmarks_menu_repopulate,
					       GTK_OBJECT (new_bookmarks_menu));

	return GTK_WIDGET (new_bookmarks_menu);
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
	guint	index;
	
	g_return_if_fail(NAUTILUS_IS_BOOKMARKS_MENU(menu));

	nautilus_bookmarks_menu_clear_bookmarks(menu);

	/* append new set of bookmarks */

	bookmark_count = nautilus_bookmarklist_length(bookmarks);
	for (index = 0; index < bookmark_count; ++index)
	{
		GtkWidget *item;
		
		item = nautilus_bookmark_menu_item_new (nautilus_bookmarklist_item_at(bookmarks, index));


		/* The signal will need to know both the menu that this item is
		 * attached to (to get at the window), and the bookmark
		 * that this item represents. The menu is stored as the item
		 * data. The bookmark is passed as an index. This assumes
		 * that the signal will not be called on this item if the
		 * menu is reordered (which is true since the menu is
		 * repopulated from scratch at that time) */
		gtk_object_set_data(GTK_OBJECT(item),
				    OWNING_MENU,
				    menu);
		gtk_signal_connect(GTK_OBJECT (item), "activate",
				   GTK_SIGNAL_FUNC (bookmark_activated_cb),
				   GUINT_TO_POINTER(index));
		nautilus_bookmarks_menu_append(menu, item);
	}	
}
