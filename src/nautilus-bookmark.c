/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bookmark.c - implementation of individual bookmarks.

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
#include "nautilus-bookmark.h"


static GtkObjectClass *parent_class = NULL;

/* GtkObject methods.  */

static void
nautilus_bookmark_destroy (GtkObject *object)
{
	NautilusBookmark *bookmark;

	g_return_if_fail(object != NULL);
	g_return_if_fail(NAUTILUS_IS_BOOKMARK (object));

	bookmark = NAUTILUS_BOOKMARK(object);
	g_string_free(bookmark->name, TRUE);
	g_string_free(bookmark->uri, TRUE);

	/* Chain up */
	if (GTK_OBJECT_CLASS (parent_class)->destroy != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

static void
nautilus_bookmark_finalize (GtkObject *object)
{
	/* Chain up */
	if (GTK_OBJECT_CLASS (parent_class)->finalize != NULL)
		(* GTK_OBJECT_CLASS (parent_class)->finalize) (object);
}


/* Initialization.  */

static void
class_init (NautilusBookmarkClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	parent_class = gtk_type_class (GTK_TYPE_OBJECT);

	object_class->destroy = nautilus_bookmark_destroy;
	object_class->finalize = nautilus_bookmark_finalize;
}

static void
init (NautilusBookmark *bookmark)
{
	g_assert(bookmark->name == NULL);
	g_assert(bookmark->uri == NULL);

	bookmark->name = g_string_new(NULL);
	bookmark->uri = g_string_new(NULL);
}


GtkType
nautilus_bookmark_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		static GtkTypeInfo info = {
			"NautilusBookmark",
			sizeof (NautilusBookmark),
			sizeof (NautilusBookmarkClass),
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
 * nautilus_bookmark_compare_with:
 *
 * Check whether two bookmarks are considered identical.
 * @a: first NautilusBookmark*.
 * @b: second NautilusBookmark*.
 * 
 * Return value: 0 if @a and @b have same name and uri, 1 otherwise 
 * (GCompareFunc style)
 **/
gint		    
nautilus_bookmark_compare_with (gconstpointer a, gconstpointer b)
{
	NautilusBookmark *bookmark_a;
	NautilusBookmark *bookmark_b;

	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK(a), FALSE);
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK(b), FALSE);

	bookmark_a = NAUTILUS_BOOKMARK(a);
	bookmark_b = NAUTILUS_BOOKMARK(b);

	if (strcmp(nautilus_bookmark_get_name(bookmark_a),
		   nautilus_bookmark_get_name(bookmark_b)) != 0)
	{
		return 1;
	}
	
	if (strcmp(nautilus_bookmark_get_uri(bookmark_a),
		   nautilus_bookmark_get_uri(bookmark_b)) != 0)
	{
		return 1;
	}
	
	return 0;
}

const gchar *
nautilus_bookmark_get_name (const NautilusBookmark *bookmark)
{
	return bookmark->name->str;
}

const gchar *
nautilus_bookmark_get_uri (const NautilusBookmark *bookmark)
{
	return bookmark->uri->str;
}

NautilusBookmark *
nautilus_bookmark_new (const gchar *name, const gchar *uri)
{
	NautilusBookmark *new_bookmark;

	new_bookmark = gtk_type_new (NAUTILUS_TYPE_BOOKMARK);
	g_string_assign(new_bookmark->name, name);
	g_string_assign(new_bookmark->uri, uri);

	return new_bookmark;
}

