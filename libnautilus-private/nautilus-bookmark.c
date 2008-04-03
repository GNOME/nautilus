/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-bookmark.c - implementation of individual bookmarks.

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
#include "nautilus-bookmark.h"

#include "nautilus-file.h"
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtkaccellabel.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkimagemenuitem.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkiconfactory.h>
#include <libgnome/gnome-macros.h>
#include <libgnome/gnome-util.h>
#include <gio/gio.h>
#include <libnautilus-private/nautilus-file.h>

enum {
	APPEARANCE_CHANGED,
	CONTENTS_CHANGED,
	LAST_SIGNAL
};

#define GENERIC_BOOKMARK_ICON_NAME	"gnome-fs-bookmark"
#define MISSING_BOOKMARK_ICON_NAME	"gnome-fs-bookmark-missing"

#define ELLIPSISED_MENU_ITEM_MIN_CHARS  32

static guint signals[LAST_SIGNAL];

struct NautilusBookmarkDetails
{
	char *name;
	gboolean has_custom_name;
	GFile *location;
	GIcon *icon;
	NautilusFile *file;
	
	char *scroll_file;
};

static void	  nautilus_bookmark_connect_file	  (NautilusBookmark	 *file);
static void	  nautilus_bookmark_disconnect_file	  (NautilusBookmark	 *file);

GNOME_CLASS_BOILERPLATE (NautilusBookmark, nautilus_bookmark,
			 GtkObject, GTK_TYPE_OBJECT)

/* GtkObject methods.  */

static void
nautilus_bookmark_finalize (GObject *object)
{
	NautilusBookmark *bookmark;

	g_assert (NAUTILUS_IS_BOOKMARK (object));

	bookmark = NAUTILUS_BOOKMARK (object);

	nautilus_bookmark_disconnect_file (bookmark);	

	g_free (bookmark->details->name);
	g_object_unref (bookmark->details->location);
	if (bookmark->details->icon) {
		g_object_unref (bookmark->details->icon);
	}
	g_free (bookmark->details->scroll_file);
	g_free (bookmark->details);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* Initialization.  */

static void
nautilus_bookmark_class_init (NautilusBookmarkClass *class)
{
	G_OBJECT_CLASS (class)->finalize = nautilus_bookmark_finalize;

	signals[APPEARANCE_CHANGED] =
		g_signal_new ("appearance_changed",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusBookmarkClass, appearance_changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	signals[CONTENTS_CHANGED] =
		g_signal_new ("contents_changed",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusBookmarkClass, contents_changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
				
}

static void
nautilus_bookmark_instance_init (NautilusBookmark *bookmark)
{
	bookmark->details = g_new0 (NautilusBookmarkDetails, 1);
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
int		    
nautilus_bookmark_compare_with (gconstpointer a, gconstpointer b)
{
	NautilusBookmark *bookmark_a;
	NautilusBookmark *bookmark_b;

	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (a), 1);
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (b), 1);

	bookmark_a = NAUTILUS_BOOKMARK (a);
	bookmark_b = NAUTILUS_BOOKMARK (b);

	if (eel_strcmp (bookmark_a->details->name,
			bookmark_b->details->name) != 0) {
		return 1;
	}

	if (!g_file_equal (bookmark_a->details->location,
			   bookmark_b->details->location)) {
		return 1;
	}
	
	return 0;
}

/**
 * nautilus_bookmark_compare_uris:
 *
 * Check whether the uris of two bookmarks are for the same location.
 * @a: first NautilusBookmark*.
 * @b: second NautilusBookmark*.
 * 
 * Return value: 0 if @a and @b have matching uri, 1 otherwise 
 * (GCompareFunc style)
 **/
int		    
nautilus_bookmark_compare_uris (gconstpointer a, gconstpointer b)
{
	NautilusBookmark *bookmark_a;
	NautilusBookmark *bookmark_b;

	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (a), 1);
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (b), 1);

	bookmark_a = NAUTILUS_BOOKMARK (a);
	bookmark_b = NAUTILUS_BOOKMARK (b);

	return !g_file_equal (bookmark_a->details->location,
			      bookmark_b->details->location);
}

NautilusBookmark *
nautilus_bookmark_copy (NautilusBookmark *bookmark)
{
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	return nautilus_bookmark_new_with_icon (
			bookmark->details->location,
			bookmark->details->name,
			bookmark->details->has_custom_name,
			bookmark->details->icon);
}

char *
nautilus_bookmark_get_name (NautilusBookmark *bookmark)
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	return g_strdup (bookmark->details->name);
}


gboolean
nautilus_bookmark_get_has_custom_name (NautilusBookmark *bookmark)
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK (bookmark), FALSE);

	return (bookmark->details->has_custom_name);
}


GdkPixbuf *	    
nautilus_bookmark_get_pixbuf (NautilusBookmark *bookmark,
			      GtkIconSize stock_size)
{
	GdkPixbuf *result;
	GIcon *icon;
	NautilusIconInfo *info;
	int pixel_size;

	
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	icon = nautilus_bookmark_get_icon (bookmark);
	if (icon == NULL) {
		return NULL;
	}
	
	pixel_size = nautilus_get_icon_size_for_stock_size (stock_size);
	info = nautilus_icon_info_lookup (icon, pixel_size);
	result = nautilus_icon_info_get_pixbuf_at_size (info, pixel_size);	
	g_object_unref (info);

	g_object_unref (icon);
	
	return result;
}

GIcon *
nautilus_bookmark_get_icon (NautilusBookmark *bookmark)
{
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	/* Try to connect a file in case file exists now but didn't earlier. */
	nautilus_bookmark_connect_file (bookmark);

	if (bookmark->details->icon) {
		return g_object_ref (bookmark->details->icon);
	}
	return NULL;
}

GFile *
nautilus_bookmark_get_location (NautilusBookmark *bookmark)
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	/* Try to connect a file in case file exists now but didn't earlier.
	 * This allows a bookmark to update its image properly in the case
	 * where a new file appears with the same URI as a previously-deleted
	 * file. Calling connect_file here means that attempts to activate the 
	 * bookmark will update its image if possible. 
	 */
	nautilus_bookmark_connect_file (bookmark);

	return g_object_ref (bookmark->details->location);
}

char *
nautilus_bookmark_get_uri (NautilusBookmark *bookmark)
{
	GFile *file;
	char *uri;

	file = nautilus_bookmark_get_location (bookmark);
	uri = g_file_get_uri (file);
	g_object_unref (file);
	return uri;
}


/**
 * nautilus_bookmark_set_name:
 *
 * Change the user-displayed name of a bookmark.
 * @new_name: The new user-displayed name for this bookmark, mustn't be NULL.
 * 
 * Returns: TRUE if the name changed else FALSE.
 **/
gboolean
nautilus_bookmark_set_name (NautilusBookmark *bookmark, const char *new_name)
{
	g_return_val_if_fail (new_name != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), FALSE);

	if (strcmp (new_name, bookmark->details->name) == 0) {
		return FALSE;
	}

	g_free (bookmark->details->name);
	bookmark->details->name = g_strdup (new_name);

	g_signal_emit (bookmark, signals[APPEARANCE_CHANGED], 0);

	return TRUE;
}

void
nautilus_bookmark_set_has_custom_name (NautilusBookmark *bookmark, gboolean has_custom_name)
{
	bookmark->details->has_custom_name = has_custom_name;
}

static gboolean
nautilus_bookmark_icon_is_different (NautilusBookmark *bookmark,
		   		     GIcon *new_icon)
{
	g_assert (NAUTILUS_IS_BOOKMARK (bookmark));
	g_assert (new_icon != NULL);

	if (bookmark->details->icon == NULL) {
		return TRUE;
	}
	
	return !g_icon_equal (bookmark->details->icon, new_icon) != 0;
}

/**
 * Update icon if there's a better one available.
 * Return TRUE if the icon changed.
 */
static gboolean
nautilus_bookmark_update_icon (NautilusBookmark *bookmark)
{
	GIcon *new_icon;

	g_assert (NAUTILUS_IS_BOOKMARK (bookmark));

	if (bookmark->details->file == NULL) {
		return FALSE;
	}

	if (!nautilus_file_is_not_yet_confirmed (bookmark->details->file) &&
	    nautilus_file_check_if_ready (bookmark->details->file,
					  NAUTILUS_FILE_ATTRIBUTES_FOR_ICON)) {
		new_icon = nautilus_file_get_gicon (bookmark->details->file, 0);
		if (nautilus_bookmark_icon_is_different (bookmark, new_icon)) {
			if (bookmark->details->icon) {
				g_object_unref (bookmark->details->icon);
			}
			bookmark->details->icon = new_icon;
			return TRUE;
		}
		g_object_unref (new_icon);
	}

	return FALSE;
}

static void
bookmark_file_changed_callback (NautilusFile *file, NautilusBookmark *bookmark)
{
	GFile *location;
	gboolean should_emit_appearance_changed_signal;
	gboolean should_emit_contents_changed_signal;
	char *display_name;
		
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (NAUTILUS_IS_BOOKMARK (bookmark));
	g_assert (file == bookmark->details->file);

	should_emit_appearance_changed_signal = FALSE;
	should_emit_contents_changed_signal = FALSE;
	location = nautilus_file_get_location (file);

	if (!g_file_equal (bookmark->details->location, location) &&
	    !nautilus_file_is_in_trash (file)) {
		g_object_unref (bookmark->details->location);
		bookmark->details->location = location;
		should_emit_contents_changed_signal = TRUE;
	} else {
		g_object_unref (location);
	}

	if (nautilus_file_is_gone (file) ||
	    nautilus_file_is_in_trash (file)) {
		/* The file we were monitoring has been trashed, deleted,
		 * or moved in a way that we didn't notice. Make 
		 * a spanking new NautilusFile object for this 
		 * location so if a new file appears in this place 
		 * we will notice.
		 */
		nautilus_bookmark_disconnect_file (bookmark);
		should_emit_appearance_changed_signal = TRUE;		
	} else if (nautilus_bookmark_update_icon (bookmark)) {
		/* File hasn't gone away, but it has changed
		 * in a way that affected its icon.
		 */
		should_emit_appearance_changed_signal = TRUE;
	}

	if (!bookmark->details->has_custom_name) {
		display_name = nautilus_file_get_display_name (file);

		if (strcmp (bookmark->details->name, display_name) != 0) {
			g_free (bookmark->details->name);
			bookmark->details->name = display_name;
			should_emit_appearance_changed_signal = TRUE;
		} else {
			g_free (display_name);
		}
	}

	if (should_emit_appearance_changed_signal) {
		g_signal_emit (bookmark, signals[APPEARANCE_CHANGED], 0);
	}

	if (should_emit_contents_changed_signal) {
		g_signal_emit (bookmark, signals[CONTENTS_CHANGED], 0);
	}
}

/**
 * nautilus_bookmark_set_icon_to_default:
 * 
 * Reset the icon to either the missing bookmark icon or the generic
 * bookmark icon, depending on whether the file still exists.
 */
static void
nautilus_bookmark_set_icon_to_default (NautilusBookmark *bookmark)
{
	const char *icon_name;


	if (bookmark->details->icon) {
		g_object_unref (bookmark->details->icon);
	}

	if (nautilus_bookmark_uri_known_not_to_exist (bookmark)) {
		icon_name = MISSING_BOOKMARK_ICON_NAME;
	} else {
		icon_name = GENERIC_BOOKMARK_ICON_NAME;
	}
	
	bookmark->details->icon = g_themed_icon_new (icon_name);
}

/**
 * nautilus_bookmark_new:
 *
 * Create a new NautilusBookmark from a text uri and a display name.
 * The initial icon for the bookmark will be based on the information 
 * already available without any explicit action on NautilusBookmark's
 * part.
 * 
 * @uri: Any uri, even a malformed or non-existent one.
 * @name: A string to display to the user as the bookmark's name.
 * 
 * Return value: A newly allocated NautilusBookmark.
 * 
 **/
NautilusBookmark *
nautilus_bookmark_new (GFile *location, const char *name)
{
	return nautilus_bookmark_new_with_icon (location, name, TRUE, NULL);
}

static void
nautilus_bookmark_disconnect_file (NautilusBookmark *bookmark)
{
	g_assert (NAUTILUS_IS_BOOKMARK (bookmark));
	
	if (bookmark->details->file != NULL) {
		g_signal_handlers_disconnect_by_func (bookmark->details->file,
						      G_CALLBACK (bookmark_file_changed_callback),
						      bookmark);
		nautilus_file_unref (bookmark->details->file);
		bookmark->details->file = NULL;
	}

	if (bookmark->details->icon != NULL) {
		g_object_unref (bookmark->details->icon);
		bookmark->details->icon = NULL;
	}
}

static void
nautilus_bookmark_connect_file (NautilusBookmark *bookmark)
{
	char *display_name;
	
	g_assert (NAUTILUS_IS_BOOKMARK (bookmark));

	if (bookmark->details->file != NULL) {
		return;
	}

	if (!nautilus_bookmark_uri_known_not_to_exist (bookmark)) {
		bookmark->details->file = nautilus_file_get (bookmark->details->location);
		g_assert (!nautilus_file_is_gone (bookmark->details->file));

		g_signal_connect_object (bookmark->details->file, "changed",
					 G_CALLBACK (bookmark_file_changed_callback), bookmark, 0);
	}	

	/* Set icon based on available information; don't force network i/o
	 * to get any currently unknown information. 
	 */
	if (!nautilus_bookmark_update_icon (bookmark)) {
		if (bookmark->details->icon == NULL || bookmark->details->file == NULL) {
			nautilus_bookmark_set_icon_to_default (bookmark);
		}
	}
	
	if (!bookmark->details->has_custom_name &&
	    bookmark->details->file && 
	    nautilus_file_check_if_ready (bookmark->details->file, NAUTILUS_FILE_ATTRIBUTE_INFO)) {
		    display_name = nautilus_file_get_display_name (bookmark->details->file);
		    if (strcmp (bookmark->details->name, display_name) != 0) {
			    g_free (bookmark->details->name);
			    bookmark->details->name = display_name;
		    } else {
			    g_free (display_name);
		    }
	}
}

NautilusBookmark *
nautilus_bookmark_new_with_icon (GFile *location, const char *name, gboolean has_custom_name,
				 GIcon *icon)
{
	NautilusBookmark *new_bookmark;

	new_bookmark = NAUTILUS_BOOKMARK (g_object_new (NAUTILUS_TYPE_BOOKMARK, NULL));
	g_object_ref_sink (new_bookmark);

	new_bookmark->details->name = g_strdup (name);
	new_bookmark->details->location = g_object_ref (location);
	new_bookmark->details->has_custom_name = has_custom_name;
	if (icon) {
		new_bookmark->details->icon = g_object_ref (icon);
	}

	nautilus_bookmark_connect_file (new_bookmark);

	return new_bookmark;
}				 

static GtkWidget *
create_image_widget_for_bookmark (NautilusBookmark *bookmark)
{
	GdkPixbuf *pixbuf;
	GtkWidget *widget;

	pixbuf = nautilus_bookmark_get_pixbuf (bookmark, GTK_ICON_SIZE_MENU);
	if (pixbuf == NULL) {
		return NULL;
	}

        widget = gtk_image_new_from_pixbuf (pixbuf);

	g_object_unref (pixbuf);
	return widget;
}

/**
 * nautilus_bookmark_menu_item_new:
 * 
 * Return a menu item representing a bookmark.
 * @bookmark: The bookmark the menu item represents.
 * Return value: A newly-created bookmark, not yet shown.
 **/ 
GtkWidget *
nautilus_bookmark_menu_item_new (NautilusBookmark *bookmark)
{
	GtkWidget *menu_item;
	GtkWidget *image_widget;
	GtkLabel *label;
	
	menu_item = gtk_image_menu_item_new_with_label (bookmark->details->name);
	label = GTK_LABEL (GTK_BIN (menu_item)->child);
	gtk_label_set_use_underline (label, FALSE);
	gtk_label_set_ellipsize (label, PANGO_ELLIPSIZE_END);
	gtk_label_set_max_width_chars (label, ELLIPSISED_MENU_ITEM_MIN_CHARS);

	image_widget = create_image_widget_for_bookmark (bookmark);
	if (image_widget != NULL) {
		gtk_widget_show (image_widget);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item),
					       image_widget);
	}

	return menu_item;
}

gboolean
nautilus_bookmark_uri_known_not_to_exist (NautilusBookmark *bookmark)
{
	char *path_name;
	gboolean exists;

	/* Convert to a path, returning FALSE if not local. */
	if (!g_file_is_native (bookmark->details->location)) {
		return FALSE;
	}
	path_name = g_file_get_path (bookmark->details->location);

	/* Now check if the file exists (sync. call OK because it is local). */
	exists = g_file_test (path_name, G_FILE_TEST_EXISTS);
	g_free (path_name);

	return !exists;
}

void
nautilus_bookmark_set_scroll_pos (NautilusBookmark      *bookmark,
				  const char            *uri)
{
	g_free (bookmark->details->scroll_file);
	bookmark->details->scroll_file = g_strdup (uri);
}

char *
nautilus_bookmark_get_scroll_pos (NautilusBookmark      *bookmark)
{
	return g_strdup (bookmark->details->scroll_file);
}
