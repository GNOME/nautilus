/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-icon-factory.c: Class for obtaining icons for files and other objects.
 
 * Copyright (C) 1999, 2000 Red Hat Inc.
   Copyright (C) 1999, 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: John Sullivan <sullivan@eazel.com>
*/

#include <config.h>
#include "nautilus-icon-factory.h"

#include <string.h>
#include <stdio.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-mime-info.h>
#include <libgnome/gnome-util.h>

#include "nautilus-string.h"
#include "nautilus-default-file-icon.h"
#include "nautilus-metadata.h"

#define ICON_NAME_DIRECTORY             "i-directory.png"
#define ICON_NAME_DIRECTORY_CLOSED      "i-dirclosed.png"
#define ICON_NAME_EXECUTABLE            "i-executable.png"
#define ICON_NAME_REGULAR               "i-regular.png"
#define ICON_NAME_CORE                  "i-core.png"
#define ICON_NAME_SOCKET                "i-sock.png"
#define ICON_NAME_FIFO                  "i-fifo.png"
#define ICON_NAME_CHARACTER_DEVICE      "i-chardev.png"
#define ICON_NAME_BLOCK_DEVICE          "i-blockdev.png"
#define ICON_NAME_BROKEN_SYMBOLIC_LINK  "i-brokenlink.png"

#define ICON_NAME_SYMBOLIC_LINK_OVERLAY "i-symlink.png"

/* This used to be called ICON_CACHE_MAX_ENTRIES, but it's misleading
 * to call it that, since we can have any number of entries in the
 * cache if the caller keeps the pixbuf around (we only get rid of
 * items from the cache after the caller unref's them).
*/
#define ICON_CACHE_COUNT                20

/* This is the number of milliseconds we wait before sweeping out
 * items from the cache.
 */
#define ICON_CACHE_SWEEP_TIMEOUT        (10 * 1000)

/* For now, images are used themselves as thumbnails when they are
 * below this threshold size. Later we might have to have a more
 * complex rule about when to use an image for itself.
 */
#define SELF_THUMBNAIL_SIZE_THRESHOLD   16384

/* This circular doubly-linked list structure is used to keep a list
 * of the most recently used items in the cache.
 */
typedef struct NautilusCircularList NautilusCircularList;
struct NautilusCircularList {
	NautilusCircularList *next;
	NautilusCircularList *prev;
};

/* The icon factory.
 * These are actually globals, but they're in a structure so we can
 * have multiple icon factories some day if we want to.
 */
typedef struct {
        char *theme_name;

	/* A hash table so we pass out the same scalable icon pointer
	 * every time someone asks for the same icon. Scalable icons
	 * are removed from this hash table when they are destroyed.
	 */
	GHashTable *scalable_icons;

	/* A hash table that contains a cache of actual images.
	 * A circular list of the most recently used images is kept
	 * around, and we don't let them go when we sweep the cache.
	 */
        GHashTable *icon_cache;
	NautilusCircularList recently_used_dummy_head;
	guint recently_used_count;
        guint sweep_timer;
        
	/* An overlay for symbolic link icons. This is probably going
	 * to go away when we switch to using little icon badges for
	 * various keywords.
	 */
        GdkPixbuf *symbolic_link_overlay;
} NautilusIconFactory;

/* A scalable icon, which is basically the name and path of an icon,
 * before we load the actual pixels of the icons's image.
 */
struct _NautilusScalableIcon {
	guint ref_count;

	char *uri;
	char *name;
	gboolean is_symbolic_link;
};

/* The key to a hash table that holds the scaled icons as pixbufs.
 */
typedef struct {
	NautilusScalableIcon *scalable_icon;
	guint size_in_pixels;

	NautilusCircularList recently_used_node;
} NautilusIconCacheKey;

/* forward declarations */

static NautilusIconFactory * nautilus_get_current_icon_factory (void);
static NautilusIconFactory * nautilus_icon_factory_new         (const char           *theme_name);
static GdkPixbuf *           nautilus_icon_factory_scale       (GdkPixbuf            *standard_sized_image,
								guint                 size_in_pixels);
static NautilusScalableIcon *nautilus_scalable_icon_get        (const char           *uri,
								const char           *name,
								gboolean              is_symbolic_link);
static guint                 nautilus_scalable_icon_hash       (gconstpointer         p);
static gboolean              nautilus_scalable_icon_equal      (gconstpointer         a,
								gconstpointer         b);
static void                  nautilus_icon_cache_key_destroy   (NautilusIconCacheKey *key);
static guint                 nautilus_icon_cache_key_hash      (gconstpointer         p);
static gboolean              nautilus_icon_cache_key_equal     (gconstpointer         a,
								gconstpointer         b);

/* Return a pointer to the single global icon factory. */
NautilusIconFactory *
nautilus_get_current_icon_factory (void)
{
        static NautilusIconFactory *global_icon_factory = NULL;
        if (global_icon_factory == NULL)
                global_icon_factory = nautilus_icon_factory_new (NULL);
        return global_icon_factory;
}

/* Create the icon factory. */
static NautilusIconFactory *
nautilus_icon_factory_new (const char *theme_name)
{
        NautilusIconFactory *factory;
        
        factory = g_new0 (NautilusIconFactory, 1);

        factory->theme_name = g_strdup (theme_name);
	factory->scalable_icons = g_hash_table_new (nautilus_scalable_icon_hash,
						    nautilus_scalable_icon_equal);
        factory->icon_cache = g_hash_table_new (nautilus_icon_cache_key_hash,
						nautilus_icon_cache_key_equal);

	/* Empty out the recently-used list. */
	factory->recently_used_dummy_head.next = &factory->recently_used_dummy_head;
	factory->recently_used_dummy_head.prev = &factory->recently_used_dummy_head;

        return factory;
}

/* Destroy one image in the cache. */
static gboolean
nautilus_icon_factory_destroy_cached_image (gpointer key, gpointer value, gpointer user_data)
{
        nautilus_icon_cache_key_destroy (key);
	gdk_pixbuf_unref (value);
        return TRUE;
}

/* Reset the cache to the default state. */
static void
nautilus_icon_factory_clear (void)
{
	NautilusIconFactory *factory;

	factory = nautilus_get_current_icon_factory ();

        g_hash_table_foreach_remove (factory->icon_cache,
                                     nautilus_icon_factory_destroy_cached_image,
                                     NULL);

	/* Empty out the recently-used list. */
	factory->recently_used_dummy_head.next = &factory->recently_used_dummy_head;
	factory->recently_used_dummy_head.prev = &factory->recently_used_dummy_head;
	factory->recently_used_count = 0;

        if (factory->symbolic_link_overlay != NULL) {
                gdk_pixbuf_unref (factory->symbolic_link_overlay);
                factory->symbolic_link_overlay = NULL;
        }
}

#if 0

static void
nautilus_icon_factory_destroy (NautilusIconFactory *factory)
{
        nautilus_icon_factory_clear ();
        g_hash_table_destroy (factory->icon_cache);

        g_free (factory->theme_name);
        g_free (factory);
}

#endif

static gboolean
nautilus_icon_factory_possibly_free_cached_image (gpointer key,
						  gpointer value,
						  gpointer user_data)
{
        NautilusIconCacheKey *icon_key;
	GdkPixbuf *image;

	/* Don't free a cache entry that is in the recently used list. */
	icon_key = key;
        if (icon_key->recently_used_node.next != NULL)
                return FALSE;

	/* Don't free a cache entry if the image is still in use. */
	image = value;
	if (image->ref_count > 1)
		return FALSE;

	/* Free the item. */
        return nautilus_icon_factory_destroy_cached_image (key, value, NULL);
}

/* Sweep the cache, freeing any images that are not in use and are
 * also not recently used.
 */
static gboolean
nautilus_icon_factory_sweep (gpointer user_data)
{
        NautilusIconFactory *factory;

	factory = user_data;

	g_hash_table_foreach_remove (factory->icon_cache,
				     nautilus_icon_factory_possibly_free_cached_image,
				     NULL);

        factory->sweep_timer = 0;

        return FALSE;
}

/* Schedule a timer to do a sweep. */
static void
nautilus_icon_factory_schedule_sweep (void)
{
	NautilusIconFactory *factory;

	factory = nautilus_get_current_icon_factory ();

        if (factory->sweep_timer != 0)
                return;

        factory->sweep_timer = g_timeout_add (ICON_CACHE_SWEEP_TIMEOUT,
					      nautilus_icon_factory_sweep,
					      factory);
}

/* Change the theme. */
void
nautilus_icon_factory_set_theme (const char *theme_name)
{
	NautilusIconFactory *factory;

	factory = nautilus_get_current_icon_factory ();

        nautilus_icon_factory_clear ();

        g_free (factory->theme_name);
        factory->theme_name = g_strdup (theme_name);
}

/* Use the MIME type to get the icon name. */
static const char *
nautilus_icon_factory_get_icon_name_for_regular_file (NautilusFile *file)
{
        const char *mime_type;
        const char *icon_name;

        mime_type = nautilus_file_get_mime_type (file);
        if (mime_type != NULL) {
                icon_name = gnome_mime_get_value (mime_type, "icon-filename");
		if (icon_name != NULL)
			return icon_name;
	}

	/* GNOME didn't give us a file name, so we have to fall back on special icon sets. */
	if (nautilus_file_is_executable (file))
		return ICON_NAME_EXECUTABLE;
	return ICON_NAME_REGULAR;
}

/* Get the icon name for a file. */
static const char *
nautilus_icon_factory_get_icon_name_for_file (NautilusFile *file)
{
	/* Get an icon name based on the file's type. */
        switch (nautilus_file_get_type (file)) {
        case GNOME_VFS_FILE_TYPE_DIRECTORY:
                return ICON_NAME_DIRECTORY;
        case GNOME_VFS_FILE_TYPE_FIFO:
                return ICON_NAME_FIFO;
        case GNOME_VFS_FILE_TYPE_SOCKET:
		return ICON_NAME_SOCKET;
        case GNOME_VFS_FILE_TYPE_CHARDEVICE:
		return ICON_NAME_CHARACTER_DEVICE;
        case GNOME_VFS_FILE_TYPE_BLOCKDEVICE:
		return ICON_NAME_BLOCK_DEVICE;
        case GNOME_VFS_FILE_TYPE_BROKENSYMLINK:
                return ICON_NAME_BROKEN_SYMBOLIC_LINK;
        case GNOME_VFS_FILE_TYPE_REGULAR:
        case GNOME_VFS_FILE_TYPE_UNKNOWN:
        default:
                return nautilus_icon_factory_get_icon_name_for_regular_file (file);
        }
}

/* Given the icon name, load the pixbuf. */
static GdkPixbuf *
nautilus_icon_factory_load_file (const char *name)
{
 	NautilusIconFactory *factory;
	char *file_name;
	char *partial_path;
        GdkPixbuf *image;
	
 	factory = nautilus_get_current_icon_factory ();
	
	if (name[0] == '/')
		file_name = g_strdup (name);
	else {
		/* Get theme version of icon. */
		file_name = NULL;
                if (factory->theme_name != NULL) {
                        partial_path = g_strdup_printf ("nautilus/%s/%s",
							factory->theme_name, name);
                        file_name = gnome_pixmap_file (partial_path);
			g_free (partial_path);
                }
                
		/* Get non-theme version of icon. */
                if (file_name == NULL) {
                        partial_path = g_strdup_printf ("nautilus/%s", name);
                        file_name = gnome_pixmap_file (partial_path);
			g_free (partial_path);
                }

		/* Can't find icon. Don't try to read it with a partial path. */
		if (file_name == NULL)
			return NULL;
        }
        
	/* Load the image. */
        image = gdk_pixbuf_new_from_file (file_name);
        g_free (file_name);
        return image;
}

/* Splats one on top of the other, putting the src image
 * in the lower left corner of the dest image.
 */
static void
nautilus_gdk_pixbuf_composite_corner (GdkPixbuf *dest, GdkPixbuf *src)
{
        int dx, dy, dw, dh;

        dw = MIN (dest->art_pixbuf->width, src->art_pixbuf->width);
        dh = MIN (dest->art_pixbuf->width, src->art_pixbuf->width);
        dx = dw - src->art_pixbuf->width;
        dy = dh - src->art_pixbuf->height;

	gdk_pixbuf_composite (src, dest, dx, dy, dw, dh, 0, 0, 1, 1, ART_FILTER_BILINEAR, 255);
}

/* Given the icon name, load the pixbuf, falling back to the fallback
 * icon if necessary. Also composite the symbolic link symbol as needed.
 */
static GdkPixbuf *
nautilus_icon_factory_load_icon (const char *name, gboolean is_symbolic_link)
{
        GdkPixbuf *image;
	NautilusIconFactory *factory;

	/* Load the image. */
	image = nautilus_icon_factory_load_file (name);
	if (image == NULL)
		/* This is the fallback icon. */
		image = gdk_pixbuf_new_from_data (nautilus_default_file_icon,
						  ART_PIX_RGB,
						  nautilus_default_file_icon_has_alpha,
						  nautilus_default_file_icon_width,
						  nautilus_default_file_icon_height,
						  nautilus_default_file_icon_width * 4, /* stride */
						  NULL, /* don't destroy data */
						  NULL);

	/* Overlay the symbolic link symbol on top of the image. */
	if (is_symbolic_link) {
		factory = nautilus_get_current_icon_factory ();
		if (factory->symbolic_link_overlay == NULL)
			factory->symbolic_link_overlay = nautilus_icon_factory_load_file
				(ICON_NAME_SYMBOLIC_LINK_OVERLAY);
		if (factory->symbolic_link_overlay != NULL)
			nautilus_gdk_pixbuf_composite_corner
				(image, factory->symbolic_link_overlay);
	}

        return image;
}

/* Get or create a scalable icon. */
static NautilusScalableIcon *
nautilus_scalable_icon_get (const char *uri,
			    const char *name,
			    gboolean is_symbolic_link)
{
	GHashTable *hash_table;
	NautilusScalableIcon icon_key, *icon;

	/* Get at the hash table. */
	hash_table = nautilus_get_current_icon_factory ()->scalable_icons;

	/* Check to see if it's already in the table. */
	icon_key.uri = (char *)uri;
	icon_key.name = (char *)name;
	icon_key.is_symbolic_link = is_symbolic_link;
	icon = g_hash_table_lookup (hash_table, &icon_key);
	if (icon == NULL) {
		/* Not in the table, so create it and put it in. */
		icon = g_new0 (NautilusScalableIcon, 1);
		icon->uri = g_strdup (uri);
		icon->name = g_strdup (name);
		icon->is_symbolic_link = is_symbolic_link;
		g_hash_table_insert (hash_table, icon, icon);
	}

	/* Grab a reference and return it. */
	nautilus_scalable_icon_ref (icon);
	return icon;
}

void
nautilus_scalable_icon_ref (NautilusScalableIcon *icon)
{
	g_return_if_fail (icon != NULL);

	icon->ref_count++;
}

void
nautilus_scalable_icon_unref (NautilusScalableIcon *icon)
{
	GHashTable *hash_table;

	g_return_if_fail (icon != NULL);
	g_return_if_fail (icon->ref_count != 0);
	
	if (--icon->ref_count != 0)
		return;

	hash_table = nautilus_get_current_icon_factory ()->scalable_icons;
	g_hash_table_remove (hash_table, icon);
	
	g_free (icon->uri);
	g_free (icon->name);
	g_free (icon);
}

static guint
nautilus_scalable_icon_hash (gconstpointer p)
{
	const NautilusScalableIcon *icon;
	guint hash;

	icon = p;
	hash = 0;

	if (icon->uri != NULL)
		hash = g_str_hash (icon->uri);

	hash <<= 4;
	if (icon->name != NULL)
		hash ^= g_str_hash (icon->name);

	hash <<= 1;
	hash |= icon->is_symbolic_link;

	return hash;
}

static gboolean
nautilus_scalable_icon_equal (gconstpointer a,
			      gconstpointer b)
{
	const NautilusScalableIcon *icon_a, *icon_b;

	icon_a = a;
	icon_b = b;

	return nautilus_strcmp (icon_a->uri, icon_b->uri) == 0
		&& nautilus_strcmp (icon_a->name, icon_b->name) == 0
		&& icon_a->is_symbolic_link == icon_b->is_symbolic_link;
}

NautilusScalableIcon *
nautilus_icon_factory_get_icon_for_file (NautilusFile *file)
{
	char *uri;
        const char *name;
        gboolean is_symbolic_link;
	NautilusScalableIcon *scalable_icon;
	
	if (file == NULL)
		return NULL;
	
	/* If there is a custom image in the metadata, use that.
	 * Otherwise, consider using the image itself as a custom icon.
	 * The check for using the image is currently based on the file
	 * size, but that will change to a more sophisticated scheme later.
	 */
	uri = nautilus_file_get_metadata(file, NAUTILUS_CUSTOM_ICON_METADATA_KEY, NULL);
	if (uri == NULL
	    && nautilus_has_prefix (nautilus_file_get_mime_type (file), "image/")
	    && nautilus_file_get_size (file) < SELF_THUMBNAIL_SIZE_THRESHOLD)
		uri = nautilus_file_get_uri (file);
	
	/* Get the generic icon set for this file. */
        name = nautilus_icon_factory_get_icon_name_for_file (file);
	
	/* Also record whether it's a symbolic link or not.
	 * Later, we'll probably use a separate icon badge for this,
	 * outside the icon factory machinery. But for now, we'll keep it.
	 */
        is_symbolic_link = nautilus_file_is_symbolic_link (file);
	
	/* Create the icon or find it in the cache if it's already there. */
	scalable_icon = nautilus_scalable_icon_get (uri, name, is_symbolic_link);
	g_free (uri);
	
	return scalable_icon;
}

static GdkPixbuf *
nautilus_icon_factory_create_image_for_icon (NautilusScalableIcon *scalable_icon,
					     guint size_in_pixels)
{
	NautilusIconFactory *factory;
        GdkPixbuf *image, *standard_size_image;

        /* First cut at handling multiple sizes. If size is other than standard,
         * scale the pixbuf here. Eventually we'll read in icon files at multiple
	 * sizes rather than relying on scaling in every case (though we'll still
	 * need scaling as a fallback).
         */
        if (size_in_pixels != NAUTILUS_ICON_SIZE_STANDARD)
        {
		standard_size_image = nautilus_icon_factory_get_pixbuf_for_icon
			(scalable_icon, NAUTILUS_ICON_SIZE_STANDARD);
		image = nautilus_icon_factory_scale
			(standard_size_image, size_in_pixels);
        	gdk_pixbuf_unref (standard_size_image);
		return image;
        }
  
	factory = nautilus_get_current_icon_factory (); 

	/* FIXME: This works only with file:// images, because there's
	 * no convenience function for loading an image with gnome-vfs
	 * and gdk-pixbuf.
	 */
	image = NULL;
	if (nautilus_has_prefix (scalable_icon->uri, "file://"))
		image = gdk_pixbuf_new_from_file (scalable_icon->uri + 7);
	
	/* If there was no suitable custom icon URI, then use the icon set. */
	if (image == NULL)
		image = nautilus_icon_factory_load_icon
			(scalable_icon->name, scalable_icon->is_symbolic_link);
	
        return image;
}

/* Move this item to the head of the recently-used list,
 * bumping the last item off that list if necessary.
 */
static void
nautilus_icon_factory_mark_recently_used (NautilusCircularList *node)
{
	NautilusIconFactory *factory;
	NautilusCircularList *head, *last_node;

	factory = nautilus_get_current_icon_factory ();
	head = &factory->recently_used_dummy_head;

	/* Move the node to the start of the list. */
	if (node->prev != head) {
		if (node->next != NULL) {
			/* Remove the node from its current position in the list. */
			node->next->prev = node->prev;
			node->prev->next = node->next;
		} else {
			/* Node was not already in the list, so add it.
			 * If the list is already full, remove the last node.
			 */
			if (factory->recently_used_count < ICON_CACHE_COUNT)
				factory->recently_used_count++;
			else {
				/* Remove the last node. */
				last_node = head->prev;

				g_assert (last_node != head);
				g_assert (last_node != node);

				head->prev = last_node->prev;
				last_node->prev->next = head;

				last_node->prev = NULL;
				last_node->next = NULL;
			}
		}
		
		/* Insert the node at the head of the list. */
		node->prev = head;
		node->next = head->next;
		node->next->prev = node;
		head->next = node;
	}
}

GdkPixbuf *
nautilus_icon_factory_get_pixbuf_for_icon (NautilusScalableIcon *scalable_icon,
					   guint size_in_pixels)
{
	NautilusIconFactory *factory;
	GHashTable *hash_table;
	NautilusIconCacheKey lookup_key, *key;
        GdkPixbuf *image;
	gpointer key_in_table, value;

	g_return_val_if_fail(scalable_icon, NULL);

	factory = nautilus_get_current_icon_factory ();
	hash_table = factory->icon_cache;

	/* Check to see if it's already in the table. */
	lookup_key.scalable_icon = scalable_icon;
	lookup_key.size_in_pixels = size_in_pixels;
	if (g_hash_table_lookup_extended (hash_table, &lookup_key, &key_in_table, &value)) {
		/* Found it in the table. */
		key = key_in_table;
		image = value;
	} else {
		/* Not in the table, so create the image and put it in. */
		image = nautilus_icon_factory_create_image_for_icon
			(scalable_icon, size_in_pixels);

		/* Create the key for the table. */
		key = g_new0 (NautilusIconCacheKey, 1);
		nautilus_scalable_icon_ref (scalable_icon);
		key->scalable_icon = scalable_icon;
		key->size_in_pixels = size_in_pixels;

		/* Add the item to the hash table. */
		g_hash_table_insert (hash_table, key, image);
	}

	/* Since this item was used, keep it in the cache longer. */
	nautilus_icon_factory_mark_recently_used (&key->recently_used_node);

	/* Come back later and sweep the cache. */
	nautilus_icon_factory_schedule_sweep ();

	/* Grab a ref for the caller. */
	gdk_pixbuf_ref (image);
        return image;
}

static void
nautilus_icon_cache_key_destroy (NautilusIconCacheKey *key)
{
	nautilus_scalable_icon_unref (key->scalable_icon);
}

static guint
nautilus_icon_cache_key_hash (gconstpointer p)
{
	const NautilusIconCacheKey *key;

	key = p;
	return GPOINTER_TO_UINT (key->scalable_icon) ^ key->size_in_pixels;
}

static gboolean
nautilus_icon_cache_key_equal (gconstpointer a, gconstpointer b)
{
	const NautilusIconCacheKey *key_a, *key_b;

	key_a = a;
	key_b = b;

	return key_a->scalable_icon == key_b->scalable_icon
		&& key_a->size_in_pixels == key_b->size_in_pixels;
}

static GdkPixbuf *
nautilus_icon_factory_scale (GdkPixbuf *standard_sized_image,
			     guint size_in_pixels)
{
	int old_width, old_height, new_width, new_height;
	
	old_width = gdk_pixbuf_get_width (standard_sized_image);
	old_height = gdk_pixbuf_get_height (standard_sized_image);

	new_width = (old_width * size_in_pixels) / NAUTILUS_ICON_SIZE_STANDARD;
	new_height = (old_height * size_in_pixels) / NAUTILUS_ICON_SIZE_STANDARD;

	return gdk_pixbuf_scale_simple (standard_sized_image, 
					new_width, 
					new_height, 
					ART_FILTER_BILINEAR);
}

/* Return nominal icon size for given zoom level.
 * @zoom_level: zoom level for which to find matching icon size.
 * 
 * Return value: icon size between NAUTILUS_ICON_SIZE_SMALLEST and
 * NAUTILUS_ICON_SIZE_LARGEST, inclusive.
 */
guint
nautilus_get_icon_size_for_zoom_level (NautilusZoomLevel zoom_level)
{
	switch (zoom_level) {
	case NAUTILUS_ZOOM_LEVEL_SMALLEST:
		return NAUTILUS_ICON_SIZE_SMALLEST;
	case NAUTILUS_ZOOM_LEVEL_SMALLER:
		return NAUTILUS_ICON_SIZE_SMALLER;
	case NAUTILUS_ZOOM_LEVEL_SMALL:
		return NAUTILUS_ICON_SIZE_SMALL;
	case NAUTILUS_ZOOM_LEVEL_STANDARD:
		return NAUTILUS_ICON_SIZE_STANDARD;
	case NAUTILUS_ZOOM_LEVEL_LARGE:
		return NAUTILUS_ICON_SIZE_LARGE;
	case NAUTILUS_ZOOM_LEVEL_LARGER:
		return NAUTILUS_ICON_SIZE_LARGER;
	case NAUTILUS_ZOOM_LEVEL_LARGEST:
		return NAUTILUS_ICON_SIZE_LARGEST;
	default:
		g_assert_not_reached ();
		return NAUTILUS_ICON_SIZE_STANDARD;
	}
}

/* Convenience cover for nautilus_icon_factory_get_icon_for_file
 * and nautilus_icon_factory_get_pixbuf_for_icon.
 */
GdkPixbuf *
nautilus_icon_factory_get_pixbuf_for_file (NautilusFile *file,
					   guint size_in_pixels)
{
	NautilusScalableIcon *icon;
	GdkPixbuf *pixbuf;

	icon = nautilus_icon_factory_get_icon_for_file (file);
	pixbuf = nautilus_icon_factory_get_pixbuf_for_icon (icon, size_in_pixels);
	nautilus_scalable_icon_unref (icon);
	return pixbuf;
}

/* Convenience cover for nautilus_icon_factory_get_icon_for_file,
 * nautilus_icon_factory_get_pixbuf_for_icon,
 * and gdk_pixbuf_render_pixmap_and_mask.
 */
void
nautilus_icon_factory_get_pixmap_and_mask_for_file (NautilusFile *file,
						    guint size_in_pixels,
						    GdkPixmap **pixmap,
						    GdkBitmap **mask)
{
	GdkPixbuf *pixbuf;

	if(!file) {
		*pixmap = NULL;
		*mask = NULL;
	}
	g_return_if_fail(file);

	pixbuf = nautilus_icon_factory_get_pixbuf_for_file (file, size_in_pixels);
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, pixmap, mask, 128);
	gdk_pixbuf_unref (pixbuf);
}
