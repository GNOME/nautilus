/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

/* nautilus-bookmark-list.c - implementation of centralized list of bookmarks.
 */

#include <config.h>
#include "nautilus-bookmark-list.h"

#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-xml-extensions.h>
#include <gtk/gtksignal.h>
#include <libnautilus-private/nautilus-file-utilities.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-search-directory.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-util.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-volume-monitor.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <stdlib.h>

#define MAX_BOOKMARK_LENGTH 80

enum {
	CONTENTS_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];
static char *window_geometry;

/* forward declarations */

static void        destroy                              (GtkObject            *object);
static char * 	   nautilus_bookmark_list_get_file_path ();
static void	   nautilus_bookmark_list_class_init 	(NautilusBookmarkListClass *class);
static void        nautilus_bookmark_list_load_file     (NautilusBookmarkList *bookmarks);
static void        nautilus_bookmark_list_save_file     (NautilusBookmarkList *bookmarks);
static void        set_window_geometry_internal         (const char           *string);
static void        stop_monitoring_bookmark             (NautilusBookmarkList *bookmarks,
							 NautilusBookmark     *bookmark);
static void 	   bookmark_monitor_notify_cb 		(GnomeVFSMonitorHandle    *handle,
		   	    				 const gchar              *monitor_uri,
					                 const gchar              *info_uri,
		            				 GnomeVFSMonitorEventType  event_type,
		            				 gpointer                  user_data);


static char *
get_default_bookmark_name (const char *text_uri)
{
	char *title;

	title = nautilus_compute_title_for_uri (text_uri);
	title = eel_str_middle_truncate (title, MAX_BOOKMARK_LENGTH);	
	return title;

}

static NautilusBookmark *
new_bookmark_from_uri (const char *uri, const char *label)
{
	NautilusBookmark *new_bookmark;
	NautilusFile *file;
	char *name, *icon_name;
	gboolean has_label;

	has_label = FALSE;
	if (!label) { 
		name = get_default_bookmark_name (uri);
	} else {
		name = g_strdup (label);
		has_label = TRUE;
	}
	
	if (uri) {
		file = nautilus_file_get (uri);
		icon_name = NULL;
		if (nautilus_icon_factory_is_icon_ready_for_file (file)) {
			icon_name = nautilus_icon_factory_get_icon_for_file (file, FALSE);
		}
		if (!icon_name) {
			icon_name = g_strdup ("gnome-fs-directory");
		}
	
		new_bookmark = nautilus_bookmark_new_with_icon (uri, name, has_label, icon_name);
		nautilus_file_unref (file);
		g_free (icon_name);
		g_free (name);

		return new_bookmark;
	}
	
	g_free (name);
	return NULL;
}

static char *
nautilus_bookmark_list_get_file_path (void)
{
	char *file_path;
	file_path = g_build_filename (g_get_home_dir (),
			       		      ".gtk-bookmarks",
			       		      NULL);
	return file_path;
}

/* Initialization.  */

static void
nautilus_bookmark_list_class_init (NautilusBookmarkListClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);

	object_class->destroy = destroy;

	signals[CONTENTS_CHANGED] =
		g_signal_new ("contents_changed",
		              G_TYPE_FROM_CLASS (object_class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusBookmarkListClass, 
						   contents_changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
}

static void
nautilus_bookmark_list_init (NautilusBookmarkList *bookmarks)
{	
	
	char *file, *uri;
	GnomeVFSResult res;

	nautilus_bookmark_list_load_file (bookmarks);
	file = nautilus_bookmark_list_get_file_path ();
	uri = gnome_vfs_get_uri_from_local_path (file);
	res = gnome_vfs_monitor_add ( &bookmarks->handle,
				uri,
				GNOME_VFS_MONITOR_FILE,
				bookmark_monitor_notify_cb,
				bookmarks);
	g_free (uri);
	g_free (file);
}

EEL_CLASS_BOILERPLATE (NautilusBookmarkList, nautilus_bookmark_list, GTK_TYPE_OBJECT)

static void
stop_monitoring_one (gpointer data, gpointer user_data)
{
	g_assert (NAUTILUS_IS_BOOKMARK (data));
	g_assert (NAUTILUS_IS_BOOKMARK_LIST (user_data));

	stop_monitoring_bookmark (NAUTILUS_BOOKMARK_LIST (user_data), 
				  NAUTILUS_BOOKMARK (data));
}		  

static void
clear (NautilusBookmarkList *bookmarks)
{
	g_list_foreach (bookmarks->list, stop_monitoring_one, bookmarks);
	eel_g_object_list_free (bookmarks->list);
	bookmarks->list = NULL;
}

static void
destroy (GtkObject *object)
{
	if (NAUTILUS_BOOKMARK_LIST (object)->handle != NULL) {
		gnome_vfs_monitor_cancel (NAUTILUS_BOOKMARK_LIST (object)->handle);
		NAUTILUS_BOOKMARK_LIST (object)->handle = NULL;
	}
	clear (NAUTILUS_BOOKMARK_LIST (object));
}



static void
bookmark_in_list_changed_callback (NautilusBookmark *bookmark,
				   NautilusBookmarkList *bookmarks)
{
	g_assert (NAUTILUS_IS_BOOKMARK (bookmark));
	g_assert (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));

	/* Save changes so we'll have the good icon next time. */
	nautilus_bookmark_list_contents_changed (bookmarks);
}				   
				   
static void
stop_monitoring_bookmark (NautilusBookmarkList *bookmarks,
			  NautilusBookmark *bookmark)
{
	g_signal_handlers_disconnect_by_func (bookmark,
					      bookmark_in_list_changed_callback,
					      bookmarks);
}

static void
insert_bookmark_internal (NautilusBookmarkList *bookmarks,
			  NautilusBookmark *bookmark,
			  int index)
{
	bookmarks->list = g_list_insert (bookmarks->list, bookmark, index);

	g_signal_connect_object (bookmark, "appearance_changed",
				 G_CALLBACK (bookmark_in_list_changed_callback), bookmarks, 0);
	g_signal_connect_object (bookmark, "contents_changed",
				 G_CALLBACK (bookmark_in_list_changed_callback), bookmarks, 0);
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
			       NautilusBookmark *bookmark)
{
	g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));
	g_return_if_fail (NAUTILUS_IS_BOOKMARK (bookmark));

	insert_bookmark_internal (bookmarks, 
				  nautilus_bookmark_copy (bookmark), 
				  -1);

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
				 NautilusBookmark *bookmark)
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
	g_signal_emit (bookmarks, 
			 signals[CONTENTS_CHANGED], 0);
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
	stop_monitoring_bookmark (bookmarks, NAUTILUS_BOOKMARK (doomed->data));
	g_object_unref (doomed->data);
	
	g_list_free_1 (doomed);
	
	nautilus_bookmark_list_contents_changed (bookmarks);
}

/**
 * nautilus_bookmark_list_delete_items_with_uri:
 * 
 * Delete all bookmarks with the given uri.
 * @bookmarks: the list of bookmarks.
 * @uri: The uri to match.
 **/
void			
nautilus_bookmark_list_delete_items_with_uri (NautilusBookmarkList *bookmarks, 
				      	      const char *uri)
{
	GList *node, *next;
	gboolean list_changed;
	char *bookmark_uri;

	g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));
	g_return_if_fail (uri != NULL);

	list_changed = FALSE;
	for (node = bookmarks->list; node != NULL;  node = next) {
		next = node->next;

		bookmark_uri = nautilus_bookmark_get_uri (NAUTILUS_BOOKMARK (node->data));
		if (eel_strcmp (bookmark_uri, uri) == 0) {
			bookmarks->list = g_list_remove_link (bookmarks->list, node);
			stop_monitoring_bookmark (bookmarks, NAUTILUS_BOOKMARK (node->data));
			g_object_unref (node->data);
			g_list_free_1 (node);
			list_changed = TRUE;
		}
		g_free (bookmark_uri);
	}

	if (list_changed) {
		nautilus_bookmark_list_contents_changed (bookmarks);
	}
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
				    NautilusBookmark* new_bookmark,
				    guint index)
{
	g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));
	g_return_if_fail (index <= g_list_length (bookmarks->list));

	insert_bookmark_internal (bookmarks, 
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
NautilusBookmark *
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
	char *filename, *contents;
	GError **error = NULL;

	filename = nautilus_bookmark_list_get_file_path ();

	/* Wipe out old list. */
	clear (bookmarks);

	if (!g_file_test (filename,
			  G_FILE_TEST_EXISTS)) {
		g_free (filename);
		return;
	}

	/* Read new list from file */
	if (g_file_get_contents (filename, &contents, NULL, error)) {
        	char **lines;
      		int i;
		lines = g_strsplit (contents, "\n", -1);
      	 	for (i = 0; lines[i]; i++) {
	  		if (lines[i][0]) {
				/* gtk 2.7/2.8 might have labels appended to bookmarks which are separated by a space */
				/* we must seperate the bookmark uri and the potential label */
 				char *space, *label;

				label = NULL;
      				space = strchr (lines[i], ' ');
      				if (space) {
					*space = '\0';
					label = g_strdup (space + 1);
				}	
				insert_bookmark_internal (bookmarks, 
						          new_bookmark_from_uri (lines[i], label), 
						          -1);

				if (label) {
					g_free (label);
				}
			}
		}
      		g_free (contents);
       		g_strfreev (lines);
	}
	g_free (filename);
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
	NautilusBookmarkList *list;

	list = NAUTILUS_BOOKMARK_LIST (g_object_new (NAUTILUS_TYPE_BOOKMARK_LIST, NULL));
	g_object_ref (list);
	gtk_object_sink (GTK_OBJECT (list));

	return list;
}

static void
save_search_for_uri (const char *uri)
{
	NautilusDirectory *directory;
	NautilusSearchDirectory *search;

	directory = nautilus_directory_get (uri);

	g_assert (NAUTILUS_IS_SEARCH_DIRECTORY (directory));

	search = NAUTILUS_SEARCH_DIRECTORY (directory);

	nautilus_search_directory_save_search (search);
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
	char *tmp_filename, *filename;
	NautilusBookmark *bookmark;
	FILE *file;
	int fd;
	int saved_errno;

	filename = nautilus_bookmark_list_get_file_path ();
	tmp_filename = g_strconcat(filename, "XXXXXX", NULL); 
 
 	/* First, write a temporary file */                                                                                     
	fd = g_mkstemp (tmp_filename);
  	if (fd == -1) {
		g_warning ("make %s failed", tmp_filename);
      		saved_errno = errno;
      		goto io_error;
    	}

  	if ((file = fdopen (fd, "w")) != NULL) {
      		GList *l;

	      	for (l = bookmarks->list; l; l = l->next) {
			char *uri;
			char *bookmark_string;
			bookmark = NAUTILUS_BOOKMARK (l->data);

			uri = nautilus_bookmark_get_uri (bookmark);
			
			/* make sure we save label if it has one for compatibility with GTK 2.7 and 2.8 */
			if (nautilus_bookmark_get_has_custom_name (bookmark)) {
				char *label;
				label = nautilus_bookmark_get_name (bookmark);
				bookmark_string = g_strconcat (uri, " ", label, NULL);
				g_free (label); 
			} else {
				bookmark_string = g_strdup (uri);
			}
			if (fputs (bookmark_string, file) == EOF || fputs ("\n", file) == EOF) {
	    			saved_errno = errno;
				g_warning ("writing %s to file failed", bookmark_string); 
				g_free (bookmark_string);
	    			goto io_error;
			}	
			g_free (bookmark_string);

			/*
			 * Rather gross hack to save out searches at the same
			 * time as the bookmarks.
			 */
			if (eel_uri_is_search (uri))
				save_search_for_uri (uri);

			g_free (uri);
		}

		if (fclose (file) == EOF) {
	  		saved_errno = errno;
			g_warning ("fclose file failed");
	  		goto io_error;
		}


		/* temporarily disable bookmark file monitoring when writing file */
		if (bookmarks->handle != NULL) {
			gnome_vfs_monitor_cancel (bookmarks->handle);
			bookmarks->handle = NULL;
		}

      		if (rename (tmp_filename, filename) == -1) {
			g_warning ("rename failed");
	  		saved_errno = errno;
	  		goto io_error;
		}

      		goto out;
    	} else {
      		saved_errno = errno;

      		/* fdopen() failed, so we can't do much error checking here anyway */
      		close (fd);
    	}

io_error:
	g_warning ("Bookmark saving failed (%d)", saved_errno );


	if (fd != -1) {
		unlink (tmp_filename); /* again, not much error checking we can do here */
	}	

out:	
	
	/* re-enable bookmark file monitoring */
	gnome_vfs_monitor_add (&bookmarks->handle,
			        filename,
				GNOME_VFS_MONITOR_FILE,
				bookmark_monitor_notify_cb,
				bookmarks);
	
	g_free (filename);
  	g_free (tmp_filename);

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

static void 
bookmark_monitor_notify_cb (GnomeVFSMonitorHandle    *handle,
		   	    const gchar              *monitor_uri,
		            const gchar              *info_uri,
		            GnomeVFSMonitorEventType  event_type,
		            gpointer                  user_data)
{
	if (event_type == GNOME_VFS_MONITOR_EVENT_CHANGED ||
	    event_type == GNOME_VFS_MONITOR_EVENT_CREATED) {
		g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (NAUTILUS_BOOKMARK_LIST(user_data)));
		nautilus_bookmark_list_load_file (NAUTILUS_BOOKMARK_LIST(user_data));
		g_signal_emit (user_data, 
			       signals[CONTENTS_CHANGED], 0);
	}
}

