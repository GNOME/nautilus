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

#include <gnome-xml/parser.h>
#include <gnome-xml/tree.h>

enum {
	CONTENTS_CHANGED,
	LAST_SIGNAL
};


/* forward declarations */
static void 	    append_bookmark_node 		(gpointer list_element, 
							 gpointer user_data);
static void 	    destroy_bookmark	 		(gpointer list_element, 
							 gpointer user_data);
static const gchar *nautilus_bookmarklist_get_file_path (NautilusBookmarklist *bookmarks);
static void 	    nautilus_bookmarklist_load_file 	(NautilusBookmarklist *bookmarks);
static void 	    nautilus_bookmarklist_save_file 	(NautilusBookmarklist *bookmarks);


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
nautilus_bookmarklist_class_init (NautilusBookmarklistClass *class)
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
nautilus_bookmarklist_init (NautilusBookmarklist *bookmarks)
{
	bookmarks->list = NULL;
	nautilus_bookmarklist_load_file(bookmarks);
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
			(GtkClassInitFunc) nautilus_bookmarklist_class_init,
			(GtkObjectInitFunc) nautilus_bookmarklist_init,
			NULL,
			NULL,
			NULL
		};

		type = gtk_type_unique (GTK_TYPE_OBJECT, &info);
	}

	return type;
}


/**
 * append_bookmark_node:
 * 
 * Foreach function; add a single bookmark xml node to a root node.
 * @data: a NautilusBookmark * that is the data of a GList node
 * @user_data: the xmlNodePtr to add a node to.
 **/
static void
append_bookmark_node (gpointer data, gpointer user_data)
{
	xmlNodePtr	  root_node, bookmark_node;
	NautilusBookmark *bookmark;

	g_return_if_fail(NAUTILUS_IS_BOOKMARK(data));

	bookmark = NAUTILUS_BOOKMARK(data);
	root_node = (xmlNodePtr)user_data;	

	bookmark_node = xmlNewChild(root_node, NULL, "bookmark", NULL);
	xmlSetProp(bookmark_node, "name", nautilus_bookmark_get_name(bookmark));
	xmlSetProp(bookmark_node, "uri", nautilus_bookmark_get_uri(bookmark));
}

/**
 * destroy_bookmark:
 * 
 * Foreach function; destroy a bookmark that was the data of a GList node.
 * @data: a NautilusBookmark * that is the data of a GList node.
 * @user_data: ignored.
 **/
static void
destroy_bookmark (gpointer data, gpointer user_data)
{
	g_return_if_fail(NAUTILUS_IS_BOOKMARK(data));

	gtk_object_destroy(GTK_OBJECT(data));
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
 * Save the bookmarklist to disk, and emit the contents_changed signal.
 * @bookmarks: NautilusBookmarklist whose contents have been modified.
 **/ 
void
nautilus_bookmarklist_contents_changed (NautilusBookmarklist *bookmarks)
{
	g_return_if_fail (NAUTILUS_IS_BOOKMARKLIST (bookmarks));

	nautilus_bookmarklist_save_file(bookmarks);
	gtk_signal_emit(GTK_OBJECT (bookmarks), 
			bookmarklist_signals[CONTENTS_CHANGED]);
}

/**
 * nautilus_bookmarklist_delete_item_at:
 * 
 * Delete the bookmark at the specified position.
 * @bookmarks: the list of bookmarks.
 * @index: index, must be less than length of list.
 **/
void			
nautilus_bookmarklist_delete_item_at (NautilusBookmarklist *bookmarks, 
				      guint index)
{
	GList *doomed;

	g_return_if_fail(NAUTILUS_IS_BOOKMARKLIST (bookmarks));
	g_return_if_fail(index < g_list_length(bookmarks->list));

	doomed = g_list_nth (bookmarks->list, index);
	/* FIXME: free the bookmark here */
	bookmarks->list = g_list_remove_link (bookmarks->list, doomed);
	
	nautilus_bookmarklist_contents_changed(bookmarks);
}

static const gchar *
nautilus_bookmarklist_get_file_path (NautilusBookmarklist *bookmarks)
{
	/* currently hardwired */
	static gchar *file_path = NULL;
	if (file_path == NULL)
	{
		/* FIXME: directory shouldn't be hardwired here; 
		 * file name is debatable. 
		 */   
		file_path = g_strconcat(g_get_home_dir(), 
				        G_DIR_SEPARATOR_S,
				        ".gnomad",
				        G_DIR_SEPARATOR_S,
				        "bookmarks.xml", 
				        NULL);
	}

	return file_path;
}

/**
 * nautilus_bookmarklist_insert_item:
 * 
 * Insert a bookmark at a specified position.
 * @bookmarks: the list of bookmarks.
 * @index: the position to insert the bookmark at.
 * @new_bookmark: the bookmark to insert a copy of.
 **/
void			
nautilus_bookmarklist_insert_item (NautilusBookmarklist *bookmarks,
				   const NautilusBookmark* new_bookmark,
				   guint index)
{
	g_return_if_fail(NAUTILUS_IS_BOOKMARKLIST (bookmarks));
	g_return_if_fail(index <= g_list_length(bookmarks->list));

	bookmarks->list = g_list_insert(bookmarks->list, 
					nautilus_bookmark_copy(new_bookmark), 
					index);

	nautilus_bookmarklist_contents_changed(bookmarks);
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
 * nautilus_bookmarklist_load_file:
 * 
 * Reads bookmarks from file, clobbering contents in memory.
 * @bookmarks: the list of bookmarks to fill with file contents.
 **/
static void
nautilus_bookmarklist_load_file (NautilusBookmarklist *bookmarks)
{
	xmlDocPtr	doc;
	xmlNodePtr	node;

	/* Wipe out old list. */
	g_list_foreach(bookmarks->list, destroy_bookmark, NULL);
	g_list_free(bookmarks->list);
	bookmarks->list = NULL;

	/* Read new list from file */
	doc = xmlParseFile(nautilus_bookmarklist_get_file_path(bookmarks));

	if (doc == NULL)
		return;

	node = doc->root->childs;
	while (node != NULL)
	{
		if (strcmp(node->name, "bookmark") == 0)
		{
			/* FIXME: should only accept bookmarks with both a name and uri? */
			bookmarks->list = g_list_append(
				bookmarks->list,
				nautilus_bookmark_new(
					xmlGetProp(node, "name"),
					xmlGetProp(node, "uri")));
		}
		node = node->next;
	}

	xmlFreeDoc(doc);
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

/**
 * nautilus_bookmarklist_save_file:
 * 
 * Save bookmarks to disk.
 * @bookmarks: the list of bookmarks to save.
 **/
static void
nautilus_bookmarklist_save_file (NautilusBookmarklist *bookmarks)
{
	xmlDocPtr	doc;
	xmlNodePtr	tree, subtree;

	doc = xmlNewDoc("1.0");
	doc->root = xmlNewDocNode(doc, NULL, "bookmarks", NULL);

	g_list_foreach (bookmarks->list, append_bookmark_node, doc->root);

	xmlSaveFile(nautilus_bookmarklist_get_file_path(bookmarks), doc);
	xmlFreeDoc(doc);
}
