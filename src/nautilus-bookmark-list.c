/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bookmark-list.c - implementation of centralized list of bookmarks.

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

#include <config.h>
#include "nautilus-bookmark-list.h"

#include <stdlib.h>

#include <gtk/gtksignal.h>

#include <libnautilus/nautilus-file-utilities.h>
#include <libnautilus/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-gtk-extensions.h>
#include <libnautilus/nautilus-xml-extensions.h>

#include <parser.h>
#include <tree.h>
#include <xmlmemory.h>

enum {
	CONTENTS_CHANGED,
	LAST_SIGNAL
};

/* forward declarations */
static void        append_bookmark_node                 (gpointer              list_element,
							 gpointer              user_data);
static const char *nautilus_bookmark_list_get_file_path (NautilusBookmarkList *bookmarks);
static void        nautilus_bookmark_list_load_file     (NautilusBookmarkList *bookmarks);
static void        nautilus_bookmark_list_save_file     (NautilusBookmarkList *bookmarks);
static void        set_window_geometry_internal         (const char           *string);

static guint signals[LAST_SIGNAL];
static char *window_geometry;

/* Initialization.  */

static void
nautilus_bookmark_list_initialize_class (NautilusBookmarkListClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);

	signals[CONTENTS_CHANGED] =
		gtk_signal_new ("contents_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusBookmarkListClass, 
						   contents_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class,
				      signals,
				      LAST_SIGNAL);
}

static void
nautilus_bookmark_list_initialize (NautilusBookmarkList *bookmarks)
{
	nautilus_bookmark_list_load_file (bookmarks);
}

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusBookmarkList, nautilus_bookmark_list, GTK_TYPE_OBJECT)


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
	xmlNodePtr root_node, bookmark_node;
	NautilusBookmark *bookmark;

	g_assert (NAUTILUS_IS_BOOKMARK (data));

	bookmark = NAUTILUS_BOOKMARK (data);
	root_node = (xmlNodePtr) user_data;	

	bookmark_node = xmlNewChild (root_node, NULL, "bookmark", NULL);
	xmlSetProp (bookmark_node, "name", nautilus_bookmark_get_name (bookmark));
	xmlSetProp (bookmark_node, "uri", nautilus_bookmark_get_uri (bookmark));
}

/**
 * nautilus_bookmark_list_append:
 *
 * Append a bookmark to a bookmark list.
 * @bookmarks: NautilusBookmarkList to append to.
 * @bookmark: Bookmark to append a copy of.
 **/
void
nautilus_bookmark_list_append (NautilusBookmarkList *bookmarks, 
			      const NautilusBookmark *bookmark)
{
	g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));
	g_return_if_fail (NAUTILUS_IS_BOOKMARK (bookmark));

	bookmarks->list = g_list_append (bookmarks->list, 
					 nautilus_bookmark_copy (bookmark));
	nautilus_bookmark_list_contents_changed (bookmarks);
}

/**
 * nautilus_bookmark_list_contains:
 *
 * Check whether a bookmark with matching name and url is already in the list.
 * @bookmarks: NautilusBookmarkList to check contents of.
 * @bookmark: NautilusBookmark to match against.
 * 
 * Return value: TRUE if matching bookmark is in list, FALSE otherwise
 **/
gboolean
nautilus_bookmark_list_contains (NautilusBookmarkList *bookmarks, 
				const NautilusBookmark *bookmark)
{
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), FALSE);

	return g_list_find_custom (bookmarks->list, 
				   (gpointer)bookmark, 
				   nautilus_bookmark_compare_with) 
		!= NULL;
}

/**
 * nautilus_bookmark_list_contents_changed:
 * 
 * Save the bookmark list to disk, and emit the contents_changed signal.
 * @bookmarks: NautilusBookmarkList whose contents have been modified.
 **/ 
void
nautilus_bookmark_list_contents_changed (NautilusBookmarkList *bookmarks)
{
	g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));

	nautilus_bookmark_list_save_file (bookmarks);
	gtk_signal_emit (GTK_OBJECT (bookmarks), 
			 signals[CONTENTS_CHANGED]);
}

/**
 * nautilus_bookmark_list_delete_item_at:
 * 
 * Delete the bookmark at the specified position.
 * @bookmarks: the list of bookmarks.
 * @index: index, must be less than length of list.
 **/
void			
nautilus_bookmark_list_delete_item_at (NautilusBookmarkList *bookmarks, 
				      guint index)
{
	GList *doomed;

	g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));
	g_return_if_fail (index < g_list_length (bookmarks->list));

	doomed = g_list_nth (bookmarks->list, index);
	bookmarks->list = g_list_remove_link (bookmarks->list, doomed);

	g_assert (NAUTILUS_IS_BOOKMARK (doomed->data));
	gtk_object_unref (GTK_OBJECT (doomed->data));
	
	g_list_free (doomed);
	
	nautilus_bookmark_list_contents_changed (bookmarks);
}

static const char *
nautilus_bookmark_list_get_file_path (NautilusBookmarkList *bookmarks)
{
	/* currently hardwired */

	static char *file_path = NULL;
	if (file_path == NULL) {
		file_path = nautilus_make_path (nautilus_user_directory(),
						"bookmarks.xml");
	}

	return file_path;
}

/**
 * nautilus_bookmark_list_get_window_geometry:
 * 
 * Get a string representing the bookmark_list's window's geometry.
 * This is the value set earlier by nautilus_bookmark_list_set_window_geometry.
 * @bookmarks: the list of bookmarks associated with the window.
 * Return value: string representation of window's geometry, suitable for
 * passing to gnome_parse_geometry(), or NULL if
 * no window geometry has yet been saved for this bookmark list.
 **/
const char *
nautilus_bookmark_list_get_window_geometry (NautilusBookmarkList *bookmarks)
{
	return window_geometry;
}

/**
 * nautilus_bookmark_list_insert_item:
 * 
 * Insert a bookmark at a specified position.
 * @bookmarks: the list of bookmarks.
 * @index: the position to insert the bookmark at.
 * @new_bookmark: the bookmark to insert a copy of.
 **/
void			
nautilus_bookmark_list_insert_item (NautilusBookmarkList *bookmarks,
				    const NautilusBookmark* new_bookmark,
				    guint index)
{
	g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));
	g_return_if_fail (index <= g_list_length (bookmarks->list));

	bookmarks->list = g_list_insert (bookmarks->list, 
					 nautilus_bookmark_copy (new_bookmark), 
					 index);

	nautilus_bookmark_list_contents_changed (bookmarks);
}

/**
 * nautilus_bookmark_list_item_at:
 * 
 * Get the bookmark at the specified position.
 * @bookmarks: the list of bookmarks.
 * @index: index, must be less than length of list.
 * 
 * Return value: the bookmark at position @index in @bookmarks.
 **/
const NautilusBookmark *
nautilus_bookmark_list_item_at (NautilusBookmarkList *bookmarks, guint index)
{
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks), NULL);
	g_return_val_if_fail (index < g_list_length (bookmarks->list), NULL);

	return NAUTILUS_BOOKMARK (g_list_nth_data (bookmarks->list, index));
}

/**
 * nautilus_bookmark_list_length:
 * 
 * Get the number of bookmarks in the list.
 * @bookmarks: the list of bookmarks.
 * 
 * Return value: the length of the bookmark list.
 **/
guint
nautilus_bookmark_list_length (NautilusBookmarkList *bookmarks)
{
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK_LIST(bookmarks), 0);

	return g_list_length (bookmarks->list);
}

/**
 * nautilus_bookmark_list_load_file:
 * 
 * Reads bookmarks from file, clobbering contents in memory.
 * @bookmarks: the list of bookmarks to fill with file contents.
 **/
static void
nautilus_bookmark_list_load_file (NautilusBookmarkList *bookmarks)
{
	xmlDocPtr doc;
	xmlNodePtr node;

	/* Wipe out old list. */
	nautilus_gtk_object_list_free (bookmarks->list);
	bookmarks->list = NULL;

	/* Read new list from file */
	doc = xmlParseFile (nautilus_bookmark_list_get_file_path (bookmarks));
	for (node = nautilus_xml_get_root_children (doc);
	     node != NULL;
	     node = node->next) {

		if (strcmp(node->name, "bookmark") == 0) {
			xmlChar *xml_name;
			xmlChar *xml_uri;
			
			/* Maybe should only accept bookmarks with both a name and uri? */
			xml_name = xmlGetProp (node, "name");
			xml_uri = xmlGetProp (node, "uri");
			
			bookmarks->list = g_list_append
				(bookmarks->list,
				 nautilus_bookmark_new_with_name (xml_uri, xml_name));
			
			xmlFree (xml_name);
			xmlFree (xml_uri);
		} else if (strcmp (node->name, "window") == 0) {
			xmlChar *geometry_string;
			
			geometry_string = xmlGetProp (node, "geometry");
			set_window_geometry_internal (geometry_string);
			xmlFree (geometry_string);
		}
	}
	
	xmlFreeDoc(doc);
}

/**
 * nautilus_bookmark_list_new:
 * 
 * Create a new bookmark_list, with contents read from disk.
 * 
 * Return value: A pointer to the new widget.
 **/
NautilusBookmarkList *
nautilus_bookmark_list_new (void)
{
	return gtk_type_new (NAUTILUS_TYPE_BOOKMARK_LIST);
}

/**
 * nautilus_bookmark_list_save_file:
 * 
 * Save bookmarks to disk.
 * @bookmarks: the list of bookmarks to save.
 **/
static void
nautilus_bookmark_list_save_file (NautilusBookmarkList *bookmarks)
{
	xmlDocPtr doc;
	xmlNodePtr root, node;

	doc = xmlNewDoc ("1.0");
	root = xmlNewDocNode (doc, NULL, "bookmarks", NULL);
	xmlDocSetRootElement (doc, root);

	/* save window position */
	if (window_geometry != NULL) {
		node = xmlNewChild (root, NULL, "window", NULL);
		xmlSetProp (node, "geometry", window_geometry);
	}

	/* save bookmarks */
	g_list_foreach (bookmarks->list, append_bookmark_node, root);

	xmlSaveFile (nautilus_bookmark_list_get_file_path (bookmarks), doc);
	xmlFreeDoc (doc);
}

/**
 * nautilus_bookmark_list_set_window_geometry:
 * 
 * Set a bookmarks window's geometry (position & size), in string form. This is
 * stored to disk by this class, and can be retrieved later in
 * the same session or in a future session.
 * @bookmarks: the list of bookmarks associated with the window.
 * @geometry: the new window geometry string.
 **/
void
nautilus_bookmark_list_set_window_geometry (NautilusBookmarkList *bookmarks,
					    const char *geometry)
{
	g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));
	g_return_if_fail (geometry != NULL);

	set_window_geometry_internal (geometry);

	nautilus_bookmark_list_save_file(bookmarks);
}

static void
set_window_geometry_internal (const char *string)
{
	g_free (window_geometry);
	window_geometry = g_strdup (string);
}
