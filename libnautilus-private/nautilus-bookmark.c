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
#include <libgnomeui/gtkpixmapmenuitem.h>

#include "nautilus-icon-factory.h"
#include "nautilus-string.h"
#include "nautilus-gtk-macros.h"

#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>


struct NautilusBookmarkDetails
{
	char *name;
	char *uri;
};



static void       nautilus_bookmark_initialize_class      (NautilusBookmarkClass  *class);
static void       nautilus_bookmark_initialize            (NautilusBookmark       *bookmark);
static GtkWidget *create_pixmap_widget_for_bookmark       (const NautilusBookmark *bookmark);

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

	if (strcmp (nautilus_bookmark_get_name(bookmark_a),
		    nautilus_bookmark_get_name(bookmark_b)) != 0) {
		return 1;
	}
	
	if (strcmp (nautilus_bookmark_get_uri(bookmark_a),
		    nautilus_bookmark_get_uri(bookmark_b)) != 0) {
		return 1;
	}
	
	return 0;
}

NautilusBookmark *
nautilus_bookmark_copy (const NautilusBookmark *bookmark)
{
	g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	return nautilus_bookmark_new (
			nautilus_bookmark_get_uri (bookmark),
			nautilus_bookmark_get_name (bookmark));
}

const char *
nautilus_bookmark_get_name (const NautilusBookmark *bookmark)
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	return bookmark->details->name;
}

gboolean	    
nautilus_bookmark_get_pixmap_and_mask (const NautilusBookmark *bookmark,
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
nautilus_bookmark_get_pixbuf (const NautilusBookmark *bookmark,
			      guint icon_size)
{
	NautilusFile *file;
	GdkPixbuf *pixbuf;

	file = nautilus_file_get (nautilus_bookmark_get_uri (bookmark));

	/* FIXME bugzilla.eazel.com 461: This might be a bookmark that points 
	 * to nothing, or maybe its uri cannot be converted to a NautilusFile 
	 * for some other reason. It should get some sort of generic icon, but 
	 * for now it gets none.
	 */
	if (file == NULL) {
		return NULL;
	}

	pixbuf = nautilus_icon_factory_get_pixbuf_for_file (file, icon_size);
	nautilus_file_unref (file);

	return pixbuf;
}

const char *
nautilus_bookmark_get_uri (const NautilusBookmark *bookmark)
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	return bookmark->details->uri;
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

	g_free (bookmark->details->name);
	bookmark->details->name = g_strdup (new_name);
}

/**
 * nautilus_bookmark_new:
 *
 * Create a new NautilusBookmark from a text uri and a display name.
 * @uri: Any uri, even a malformed or non-existent one.
 * @name: A string to display to the user as the bookmark's name.
 * 
 * Return value: A newly allocated NautilusBookmark.
 * 
 **/
NautilusBookmark *
nautilus_bookmark_new (const char *uri, const char *name)
{
	NautilusBookmark *new_bookmark;

	new_bookmark = gtk_type_new (NAUTILUS_TYPE_BOOKMARK);

	new_bookmark->details->name = g_strdup (name);
	new_bookmark->details->uri = g_strdup (uri);

	return new_bookmark;
}

static GtkWidget *
create_pixmap_widget_for_bookmark (const NautilusBookmark *bookmark)
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
 * nautilus_bookmark_menu_item_new:
 * 
 * Return a menu item representing a bookmark.
 * @bookmark: The bookmark the menu item represents.
 * Return value: A newly-created bookmark, not yet shown.
 **/ 
GtkWidget *
nautilus_bookmark_menu_item_new (const NautilusBookmark *bookmark)
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
	accel_label = gtk_accel_label_new (nautilus_bookmark_get_name (bookmark));
	gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

	gtk_container_add (GTK_CONTAINER (menu_item), accel_label);
	gtk_accel_label_set_accel_widget (GTK_ACCEL_LABEL (accel_label), menu_item);
	gtk_widget_show (accel_label);

	return menu_item;
}
