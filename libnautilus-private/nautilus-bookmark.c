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

#include <gtk/gtkaccellabel.h>
#include <gtk/gtksignal.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gtkpixmapmenuitem.h>

#include "nautilus-file-utilities.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-icon-factory.h"
#include "nautilus-string.h"

#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>

enum {
	CHANGED,
	LAST_SIGNAL
};

#define GENERIC_BOOKMARK_ICON_NAME	"i-bookmark"
#define MISSING_BOOKMARK_ICON_NAME	"i-bookmark-missing"

static guint signals[LAST_SIGNAL];

struct NautilusBookmarkDetails
{
	char *name;
	char *uri;
	NautilusScalableIcon *icon;
	NautilusFile *file;
};



static void       nautilus_bookmark_initialize_class      (NautilusBookmarkClass  *class);
static void       nautilus_bookmark_initialize            (NautilusBookmark       *bookmark);
static GtkWidget *create_pixmap_widget_for_bookmark       (NautilusBookmark *bookmark);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusBookmark, nautilus_bookmark, GTK_TYPE_OBJECT)

/* GtkObject methods.  */

static void
nautilus_bookmark_destroy (GtkObject *object)
{
	NautilusBookmark *bookmark;

	g_assert (NAUTILUS_IS_BOOKMARK (object));

	bookmark = NAUTILUS_BOOKMARK(object);

	g_free (bookmark->details->name);
	g_free (bookmark->details->uri);
	if (bookmark->details->icon != NULL) {
		nautilus_scalable_icon_unref (bookmark->details->icon);
	}
	nautilus_file_unref (bookmark->details->file);
	
	g_free (bookmark->details);

	/* Chain up */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


/* Initialization.  */

static void
nautilus_bookmark_initialize_class (NautilusBookmarkClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);

	object_class->destroy = nautilus_bookmark_destroy;

	signals[CHANGED] =
		gtk_signal_new ("changed",
				GTK_RUN_LAST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusBookmarkClass, changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
				
}

static void
nautilus_bookmark_initialize (NautilusBookmark *bookmark)
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

	if (strcmp (bookmark_a->details->name,
		    bookmark_b->details->name) != 0) {
		return 1;
	}
	
	if (strcmp (bookmark_a->details->uri,
		    bookmark_b->details->uri) != 0) {
		return 1;
	}
	
	return 0;
}

NautilusBookmark *
nautilus_bookmark_copy (NautilusBookmark *bookmark)
{
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	return nautilus_bookmark_new_with_icon (
			bookmark->details->uri,
			bookmark->details->name,
			bookmark->details->icon);
}

char *
nautilus_bookmark_get_name (NautilusBookmark *bookmark)
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	return g_strdup (bookmark->details->name);
}

gboolean	    
nautilus_bookmark_get_pixmap_and_mask (NautilusBookmark *bookmark,
				       guint icon_size,
				       GdkPixmap **pixmap_return,
				       GdkBitmap **mask_return)
{
	GdkPixbuf *pixbuf;

	pixbuf = nautilus_bookmark_get_pixbuf (bookmark, icon_size);
	if (pixbuf == NULL) {
		return FALSE;
	}

	gdk_pixbuf_render_pixmap_and_mask (pixbuf, pixmap_return, mask_return, 100);
	gdk_pixbuf_unref (pixbuf);

	return TRUE;
}

GdkPixbuf *	    
nautilus_bookmark_get_pixbuf (NautilusBookmark *bookmark,
			      guint icon_size)
{
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	if (bookmark->details->icon == NULL) {
		return NULL;
	}
	
	return nautilus_icon_factory_get_pixbuf_for_icon
		(bookmark->details->icon, icon_size, icon_size, icon_size, icon_size, NULL);
}

NautilusScalableIcon *
nautilus_bookmark_get_icon (NautilusBookmark *bookmark)
{
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	if (bookmark->details->icon != NULL) {
		nautilus_scalable_icon_ref (bookmark->details->icon);
	}
	return bookmark->details->icon;
}

char *
nautilus_bookmark_get_uri (NautilusBookmark *bookmark)
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	return g_strdup (bookmark->details->uri);
}


/**
 * nautilus_bookmark_set_name:
 *
 * Change the user-displayed name of a bookmark.
 * @new_name: The new user-displayed name for this bookmark, mustn't be NULL.
 * 
 **/
void
nautilus_bookmark_set_name (NautilusBookmark *bookmark, const char *new_name)
{
	g_return_if_fail(NAUTILUS_IS_BOOKMARK (bookmark));
	g_return_if_fail (new_name != NULL);

	if (strcmp (new_name, bookmark->details->name) == 0) {
		return;
	}

	g_free (bookmark->details->name);
	bookmark->details->name = g_strdup (new_name);

	gtk_signal_emit (GTK_OBJECT (bookmark), signals[CHANGED]);
}

static gboolean
nautilus_bookmark_icon_is_different (NautilusBookmark *bookmark,
		   		     NautilusScalableIcon *new_icon)
{
	char *new_uri, *new_name;
	char *old_uri, *old_name;
	gboolean result;

	g_assert (NAUTILUS_IS_BOOKMARK (bookmark));
	g_assert (new_icon != NULL);

	/* Bookmarks only care about the uri and name. */
	nautilus_scalable_icon_get_text_pieces 
		(new_icon, &new_uri, &new_name, NULL, NULL);

	if (bookmark->details->icon == NULL) {
		result = !nautilus_str_is_empty (new_uri)
			|| !nautilus_str_is_empty (new_name);
	} else {
		nautilus_scalable_icon_get_text_pieces 
			(bookmark->details->icon, &old_uri, &old_name, NULL, NULL);

		result = nautilus_strcmp (old_uri, new_uri) != 0
			|| nautilus_strcmp (old_name, new_name) != 0;

		g_free (old_uri);
		g_free (old_name);
	}

	g_free (new_uri);
	g_free (new_name);

	return result;
}

/**
 * Update icon if there's a better one available.
 * Return TRUE if the icon changed.
 */
static gboolean
nautilus_bookmark_update_icon (NautilusBookmark *bookmark)
{
	NautilusScalableIcon *new_icon;

	g_assert (NAUTILUS_IS_BOOKMARK (bookmark));

	if (bookmark->details->file == NULL) {
		return FALSE;
	}

	if (nautilus_icon_factory_is_icon_ready_for_file (bookmark->details->file)) {
		new_icon = nautilus_icon_factory_get_icon_for_file (bookmark->details->file,
								    NULL, FALSE);
		if (nautilus_bookmark_icon_is_different (bookmark, new_icon)) {
			if (bookmark->details->icon != NULL) {
				nautilus_scalable_icon_unref (bookmark->details->icon);
			}
			nautilus_scalable_icon_ref (new_icon);
			bookmark->details->icon = new_icon;

			return TRUE;
		}
	}

	return FALSE;
}

static void
bookmark_file_changed_callback (NautilusFile *file, NautilusBookmark *bookmark)
{
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (NAUTILUS_IS_BOOKMARK (bookmark));
	g_assert (file == bookmark->details->file);

	/* Check whether the file knows about a better icon. */
	if (nautilus_bookmark_update_icon (bookmark)) {
		gtk_signal_emit (GTK_OBJECT (bookmark), signals[CHANGED]);
	}
}

static void
nautilus_bookmark_set_icon_to_default (NautilusBookmark *bookmark)
{
	const char *icon_name;

	if (bookmark->details->icon != NULL) {
		nautilus_scalable_icon_unref (bookmark->details->icon);
	}

	if (nautilus_bookmark_uri_known_not_to_exist (bookmark)) {
		icon_name = MISSING_BOOKMARK_ICON_NAME;
	} else {
		icon_name = GENERIC_BOOKMARK_ICON_NAME;
	}
	bookmark->details->icon = nautilus_scalable_icon_new_from_text_pieces 
		(NULL, icon_name, NULL, NULL, FALSE);
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
nautilus_bookmark_new (const char *uri, const char *name)
{
	return nautilus_bookmark_new_with_icon (uri, name, NULL);
}

NautilusBookmark *
nautilus_bookmark_new_with_icon (const char *uri, const char *name, 
				 NautilusScalableIcon *icon)
{
	NautilusBookmark *new_bookmark;

	new_bookmark = gtk_type_new (NAUTILUS_TYPE_BOOKMARK);

	new_bookmark->details->name = g_strdup (name);
	new_bookmark->details->uri = g_strdup (uri);

	if (icon != NULL) {
		nautilus_scalable_icon_ref (icon);
	}
	new_bookmark->details->icon = icon;

	new_bookmark->details->file = nautilus_file_get (uri);

	/* Set initial icon based on available information. */
	if (!nautilus_bookmark_update_icon (new_bookmark)) {
		if (new_bookmark->details->icon == NULL) {
			nautilus_bookmark_set_icon_to_default (new_bookmark);
		}
	}

	if (new_bookmark->details->file != NULL) {
		gtk_signal_connect_while_alive (GTK_OBJECT (new_bookmark->details->file),
						"changed",
						bookmark_file_changed_callback,
						new_bookmark,
						GTK_OBJECT (new_bookmark));
	}

	return new_bookmark;
}				 

static GtkWidget *
create_pixmap_widget_for_bookmark (NautilusBookmark *bookmark)
{
	GdkPixmap *gdk_pixmap;
	GdkBitmap *mask;

	if (!nautilus_bookmark_get_pixmap_and_mask (bookmark, 
						    NAUTILUS_ICON_SIZE_FOR_MENUS,
					  	    &gdk_pixmap, 
					  	    &mask)) {
		return NULL;
	}

	return gtk_pixmap_new (gdk_pixmap, mask);
}

/**
 * nautilus_bookmarnuk_menu_item_new:
 * 
 * Return a menu item representing a bookmark.
 * @bookmark: The bookmark the menu item represents.
 * Return value: A newly-created bookmark, not yet shown.
 **/ 
GtkWidget *
nautilus_bookmark_menu_item_new (NautilusBookmark *bookmark)
{
	GtkWidget *menu_item;
	GtkWidget *pixmap_widget;
	GtkWidget *accel_label;

	/* Could check gnome_preferences_get_menus_have_icons here, but these
	 * are more important than stock menu icons, since they're connected to
	 * user data. For now let's not let them be turn-offable and see if
	 * anyone objects strenuously.
	 */
	menu_item = gtk_pixmap_menu_item_new ();

	pixmap_widget = create_pixmap_widget_for_bookmark (bookmark);
	if (pixmap_widget != NULL) {
		gtk_widget_show (pixmap_widget);
		gtk_pixmap_menu_item_set_pixmap (GTK_PIXMAP_MENU_ITEM (menu_item), pixmap_widget);
	}
	accel_label = gtk_accel_label_new (bookmark->details->name);
	gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

	gtk_container_add (GTK_CONTAINER (menu_item), accel_label);
	gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label), menu_item);
	gtk_widget_show (accel_label);

	return menu_item;
}

gboolean
nautilus_bookmark_uri_known_not_to_exist (NautilusBookmark *bookmark)
{
	char *path_name;
	gboolean exists;

	/* Convert to a path, returning FALSE if not local. */
	path_name = nautilus_get_local_path_from_uri (bookmark->details->uri);
	if (path_name == NULL) {
		return FALSE;
	}

	/* Now check if the file exists (sync. call OK because it is local). */
	exists = g_file_exists (path_name);
	g_free (path_name);
	return !exists;
}
