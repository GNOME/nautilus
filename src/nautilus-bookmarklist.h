/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bookmarklist.h - interface for centralized list of bookmarks.

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

#ifndef NAUTILUS_BOOKMARKLIST_H
#define NAUTILUS_BOOKMARKLIST_H 1

#include <gnome.h>
#include "nautilus-bookmark.h"

typedef struct _NautilusBookmarklist NautilusBookmarklist;

#define NAUTILUS_TYPE_BOOKMARKLIST \
	(nautilus_bookmarklist_get_type ())
#define NAUTILUS_BOOKMARKLIST(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_BOOKMARKLIST, NautilusBookmarklist))
#define NAUTILUS_BOOKMARKLIST_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_BOOKMARKLIST, NautilusBookmarklistClass))
#define NAUTILUS_IS_BOOKMARKLIST(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_BOOKMARKLIST))
#define NAUTILUS_IS_BOOKMARKLIST_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_BOOKMARKLIST))

struct _NautilusBookmarklist {
	GtkObject object;
	GList *list;
};

struct _NautilusBookmarklistClass {
	GtkObjectClass 		   parent_class;

	void (* contents_changed) (NautilusBookmarklist *bookmarks);
};

typedef struct _NautilusBookmarklistClass NautilusBookmarklistClass;


GtkType			nautilus_bookmarklist_get_type	(void);
NautilusBookmarklist   *nautilus_bookmarklist_new	(void);
void			nautilus_bookmarklist_append	(NautilusBookmarklist *bookmarks, 
							 const NautilusBookmark *bookmark);
gboolean		nautilus_bookmarklist_contains	(NautilusBookmarklist *bookmarks,
							 const NautilusBookmark *bookmark);
void			nautilus_bookmarklist_contents_changed	
							(NautilusBookmarklist *bookmarks);
void			nautilus_bookmarklist_delete_item_at
							(NautilusBookmarklist *bookmarks,
							 guint index);
void			nautilus_bookmarklist_insert_item
							(NautilusBookmarklist *bookmarks,
							 const NautilusBookmark *bookmark,
							 guint index);
guint			nautilus_bookmarklist_length	(NautilusBookmarklist *bookmarks);
const NautilusBookmark *nautilus_bookmarklist_item_at	(NautilusBookmarklist *bookmarks, 
							 guint index);

#endif /* NAUTILUS_BOOKMARKLIST_H */
