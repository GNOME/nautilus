/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bookmarks-menu.h - public interface for creating a Bookmarks menu.

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

#ifndef NAUTILUS_BOOKMARKS_MENU_H
#define NAUTILUS_BOOKMARKS_MENU_H 1

#include <gnome.h>
#include "nautilus-bookmarklist.h"

typedef struct _NautilusBookmarksMenu NautilusBookmarksMenu;

#define NAUTILUS_TYPE_BOOKMARKS_MENU \
	(nautilus_bookmarks_menu_get_type ())
#define NAUTILUS_BOOKMARKS_MENU(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_BOOKMARKS_MENU, NautilusBookmarksMenu))
#define NAUTILUS_BOOKMARKS_MENU_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_BOOKMARKS_MENU, NautilusBookmarksMenuClass))
#define NAUTILUS_IS_BOOKMARKS_MENU(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_BOOKMARKS_MENU))
#define NAUTILUS_IS_BOOKMARKS_MENU_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_BOOKMARKS_MENU))

struct _NautilusBookmarksMenu {
	GtkMenu menu;
	NautilusWindow *window;
};

struct _NautilusBookmarksMenuClass {
	GtkMenuClass 	      parent_class;
};
typedef struct _NautilusBookmarksMenuClass NautilusBookmarksMenuClass;


GtkType		nautilus_bookmarks_menu_get_type	(void);
GtkWidget      *nautilus_bookmarks_menu_new		(NautilusWindow *window);

#endif /* NAUTILUS_BOOKMARKS_MENU_H */
