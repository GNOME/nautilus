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

#include "nautilus-bookmark.h"
#include "nautilus-icon-factory.h"
#include "nautilus-string.h"

#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-uri.h>

struct _NautilusBookmarkDetails
{
	gchar 	  *name;
	gchar     *uri;
};



static GtkWidget *create_pixmap_widget_for_bookmark 	(const NautilusBookmark *bookmark);

static GtkObjectClass *parent_class = NULL;

/* GtkObject methods.  */

static void
nautilus_bookmark_destroy (GtkObject *object)
{
	NautilusBookmark *bookmark;

	g_return_if_fail(NAUTILUS_IS_BOOKMARK (object));

	bookmark = NAUTILUS_BOOKMARK(object);

	g_free (bookmark->details->name);
	g_free (bookmark->details->uri);
	g_free (bookmark->details);

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
	bookmark->details = g_new0 (NautilusBookmarkDetails, 1);
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

	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK(a), 1);
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK(b), 1);

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

NautilusBookmark *
nautilus_bookmark_copy (const NautilusBookmark *bookmark)
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	return nautilus_bookmark_new_with_name(
			nautilus_bookmark_get_uri(bookmark),
			nautilus_bookmark_get_name(bookmark));
}

const gchar *
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
	NautilusFile *file;
	NautilusScalableIcon *scalable_icon;
	GdkPixbuf *pixbuf;

	file = nautilus_file_get (nautilus_bookmark_get_uri (bookmark));

	/* FIXME: This might be a bookmark that points to nothing, or
	 * maybe its uri cannot be converted to a NautilusFile for some
	 * other reason. It should get some sort of generic icon, but for
	 * now it gets none.
	 */
	if (file == NULL)
		return FALSE;

	scalable_icon = nautilus_icon_factory_get_icon_for_file	(file, NULL);
	nautilus_file_unref (file);

	pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
		(scalable_icon, icon_size);
	nautilus_scalable_icon_unref (scalable_icon);

	gdk_pixbuf_render_pixmap_and_mask (pixbuf, pixmap_return, mask_return, 100);
	gdk_pixbuf_unref (pixbuf);

	return TRUE;
}

const gchar *
nautilus_bookmark_get_uri (const NautilusBookmark *bookmark)
{
	g_return_val_if_fail(NAUTILUS_IS_BOOKMARK (bookmark), NULL);

	return bookmark->details->uri;
}

/**
 * nautilus_bookmark_new_with_name:
 *
 * Create a new NautilusBookmark from a text uri and a display name.
 * @uri: Any uri, even a malformed or non-existent one.
 * @name: A string to display to the user as the bookmark's name.
 * 
 * Return value: A newly allocated NautilusBookmark.
 * 
 **/
NautilusBookmark *
nautilus_bookmark_new_with_name (const gchar *uri, const gchar *name)
{
	NautilusBookmark *new_bookmark;

	new_bookmark = gtk_type_new (NAUTILUS_TYPE_BOOKMARK);

	new_bookmark->details->name = g_strdup(name);
	new_bookmark->details->uri = g_strdup(uri);

	return new_bookmark;
}

/**
 * nautilus_bookmark_new:
 *
 * Create a new NautilusBookmark from just a text uri.
 * @uri: Any uri, even a malformed or non-existent one.
 * 
 * Return value: A newly allocated NautilusBookmark, whose display
 * name is chosen using default rules based on the uri.
 * 
 **/
NautilusBookmark *
nautilus_bookmark_new (const gchar *uri)
{
	/* Use default rules to determine the displayed name */

	NautilusBookmark *result;
	GnomeVFSURI *vfs_uri;

	result = NULL;
	
	/* For now, the only default rule is to use the file/directory name
	 * rather than the whole path. */
	/* FIXME: This needs to do better (use just file names for file:// urls,
	 * use page names for http://, etc.)
	 */

	vfs_uri = gnome_vfs_uri_new (uri);
	if (vfs_uri != NULL)
	{
		if (strcmp (vfs_uri->method_string, "file") == 0)
		{
			gchar *short_name;

			short_name = gnome_vfs_uri_extract_short_name (vfs_uri);
			result = nautilus_bookmark_new_with_name (uri, short_name);
			g_free (short_name);
		}
		
		gnome_vfs_uri_unref (vfs_uri);
	}

	if (result == NULL)
	{
		result = nautilus_bookmark_new_with_name (uri, uri);
	}

	return result;
}

static GtkWidget *
create_pixmap_widget_for_bookmark (const NautilusBookmark *bookmark)
{
	GdkPixmap *gdk_pixmap;
	GdkBitmap *mask;

	if (!nautilus_bookmark_get_pixmap_and_mask (bookmark, 
						    NAUTILUS_ICON_SIZE_SMALLER,
					  	    &gdk_pixmap, 
					  	    &mask))
	{
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
	if (pixmap_widget != NULL)
	{
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



