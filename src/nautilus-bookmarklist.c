/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bookmarklist.c - implementation of centralized list of bookmarks.

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
#include "nautilus-bookmarklist.h"

enum {
	CONTENTS_CHANGED,
	LAST_SIGNAL
};


static GtkObjectClass *parent_class = NULL;
static guint bookmarklist_signals[LAST_SIGNAL] = { 0 };


/* GtkObject methods.  */

static void
nautilus_bookmarklist_destroy (GtkObject *object)
{
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
nautilus_bookmarklist_finalize (GtkObject *object)
{
	if (GTK_OBJECT_CLASS (parent_class)->finalize != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Initialization.  */

static void
class_init (NautilusBookmarklistClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

	bookmarklist_signals[CONTENTS_CHANGED] =
		gtk_signal_new ("contents_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusBookmarklistClass, 
						   contents_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, bookmarklist_signals, LAST_SIGNAL);

	object_class->destroy = nautilus_bookmarklist_destroy;
	object_class->finalize = nautilus_bookmarklist_finalize;
}

static void
init (NautilusBookmarklist *bookmarks)
{
	bookmarks->list = NULL;
}

GtkType
nautilus_bookmarklist_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static GtkTypeInfo info = {
			"NautilusBookmarklist",
			sizeof (NautilusBookmarklist),
			sizeof (NautilusBookmarklistClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			NULL,
			NULL,
			NULL
		};

		type = gtk_type_unique (GTK_TYPE_OBJECT, &info);
	}

	return type;
}


/**
 * nautilus_bookmarklist_append:
 *
 * Append a bookmark to a bookmarklist.
 * @bookmarks: NautilusBookmarklist to append to.
 * @bookmark: Bookmark to append a copy of.
 **/
void
nautilus_bookmarklist_append (NautilusBookmarklist *bookmarks, 
			      const NautilusBookmark *bookmark)
{
	g_return_if_fail (NAUTILUS_IS_BOOKMARKLIST (bookmarks));
	g_return_if_fail (NAUTILUS_IS_BOOKMARK (bookmark));

	bookmarks->list = g_list_append(bookmarks->list, 
					nautilus_bookmark_copy(bookmark));
	nautilus_bookmarklist_contents_changed(bookmarks);
}

/**
 * nautilus_bookmarklist_contains:
 *
 * Check whether a bookmark with matching name and url is already in the list.
 * @bookmarks: NautilusBookmarklist to check contents of.
 * @bookmark: NautilusBookmark to match against.
 * 
 * Return value: TRUE if matching bookmark is in list, FALSE otherwise
 **/
gboolean
nautilus_bookmarklist_contains (NautilusBookmarklist *bookmarks, 
			      const NautilusBookmark *bookmark)
{
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARKLIST (bookmarks), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), FALSE);

	return g_list_find_custom(bookmarks->list, 
				  (gpointer)bookmark, 
				  nautilus_bookmark_compare_with) 
		!= NULL;
}

/**
 * nautilus_bookmarklist_contents_changed:
 * 
 * Emit the contents_changed signal.
 * @bookmarks: NautilusBookmarklist whose contents have been modified.
 **/ 
void
nautilus_bookmarklist_contents_changed(NautilusBookmarklist *bookmarks)
{
	g_return_if_fail (NAUTILUS_IS_BOOKMARKLIST (bookmarks));

	gtk_signal_emit(GTK_OBJECT (bookmarks), 
			bookmarklist_signals[CONTENTS_CHANGED]);
}

/**
 * nautilus_bookmarklist_item_at:
 * 
 * Get the bookmark at the specified position.
 * @bookmarks: the list of bookmarks.
 * @index: index, must be less than length of list.
 * 
 * Return value: the bookmark at position @index in @bookmarks.
 **/
const NautilusBookmark *
nautilus_bookmarklist_item_at (NautilusBookmarklist *bookmarks, guint index)
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARKLIST(bookmarks), NULL);
	g_return_val_if_fail(index < g_list_length(bookmarks->list), NULL);

	return NAUTILUS_BOOKMARK(g_list_nth_data(bookmarks->list, index));
}

/**
 * nautilus_bookmarklist_length:
 * 
 * Get the number of bookmarks in the list.
 * @bookmarks: the list of bookmarks.
 * 
 * Return value: the length of the bookmark list.
 **/
guint
nautilus_bookmarklist_length (NautilusBookmarklist *bookmarks)
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARKLIST(bookmarks), 0);

	return g_list_length(bookmarks->list);
}

/**
 * nautilus_bookmarklist_new:
 * 
 * Create a new bookmarklist, initially empty.
 * FIXME: needs to read initial contents from disk
 * 
 * Return value: A pointer to the new widget.
 **/
NautilusBookmarklist *
nautilus_bookmarklist_new ()
{
	NautilusBookmarklist *new;

	new = gtk_type_new (NAUTILUS_TYPE_BOOKMARKLIST);

	return new;
}
