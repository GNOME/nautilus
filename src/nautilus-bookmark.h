/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bookmark.h - interface for individual bookmarks.

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

#ifndef NAUTILUS_BOOKMARK_H
#define NAUTILUS_BOOKMARK_H 1

#include <gnome.h>

typedef struct _NautilusBookmark NautilusBookmark;

#define NAUTILUS_TYPE_BOOKMARK \
	(nautilus_bookmark_get_type ())
#define NAUTILUS_BOOKMARK(obj) \
	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_BOOKMARK, NautilusBookmark))
#define NAUTILUS_BOOKMARK_CLASS(klass) \
	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_BOOKMARK, NautilusBookmarkClass))
#define NAUTILUS_IS_BOOKMARK(obj) \
	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_BOOKMARK))
#define NAUTILUS_IS_BOOKMARK_CLASS(klass) \
	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_BOOKMARK))

struct _NautilusBookmark {
	GtkObject object;
	GString *name;
	GString *uri;
};

struct _NautilusBookmarkClass {
	GtkObjectClass parent_class;
};

typedef struct _NautilusBookmarkClass NautilusBookmarkClass;


GtkType		    nautilus_bookmark_get_type	   (void);
NautilusBookmark   *nautilus_bookmark_new	   (const gchar *name,
						    const gchar *uri);
const gchar	   *nautilus_bookmark_get_name	   (const NautilusBookmark *);
const gchar	   *nautilus_bookmark_get_uri	   (const NautilusBookmark *);

gint		    nautilus_bookmark_compare_with (gconstpointer a, gconstpointer b);

#endif /* NAUTILUS_BOOKMARK_H */
