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

#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-mime-info.h>
#include <libgnome/gnome-util.h>

#include "nautilus-string.h"
#include "nautilus-default-file-icon.h"
#include "nautilus-metadata.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-gtk-macros.h"

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
 * These are just globals, but they're in an object so we can
 * connect signals and have multiple icon factories some day
 * if we want to.
 */
typedef struct {
	GtkObject object;

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

typedef struct {
	GtkObjectClass parent_class;
} NautilusIconFactoryClass;

enum {
	THEME_CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

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
 * In a way, it's not really completely a key, because part of the
 * data is stored in here, including the LRU chain.
 */
typedef struct {
	NautilusScalableIcon *scalable_icon;
	guint size_in_pixels;

	NautilusCircularList recently_used_node;

	gboolean custom;
	gboolean scaled;
} NautilusIconCacheKey;

/* forward declarations */

static GtkType               nautilus_icon_factory_get_type         (void);
static void                  nautilus_icon_factory_initialize_class (NautilusIconFactoryClass *class);
static void                  nautilus_icon_factory_initialize       (NautilusIconFactory      *factory);
static NautilusIconFactory * nautilus_get_current_icon_factory      (void);
static NautilusIconFactory * nautilus_icon_factory_new              (const char               *theme_name);
static NautilusScalableIcon *nautilus_scalable_icon_get             (const char               *uri,
								     const char               *name,
								     gboolean                  is_symbolic_link);
static guint                 nautilus_scalable_icon_hash            (gconstpointer             p);
static gboolean              nautilus_scalable_icon_equal           (gconstpointer             a,
								     gconstpointer             b);
static void                  nautilus_icon_cache_key_destroy        (NautilusIconCacheKey     *key);
static guint                 nautilus_icon_cache_key_hash           (gconstpointer             p);
static gboolean              nautilus_icon_cache_key_equal          (gconstpointer             a,
								     gconstpointer             b);
static GdkPixbuf *           get_image_from_cache                   (NautilusScalableIcon     *scalable_icon,
								     guint                     size_in_pixels,
								     gboolean                  picky,
								     gboolean                  custom);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusIconFactory, nautilus_icon_factory, GTK_TYPE_OBJECT)

/* Return a pointer to the single global icon factory. */
static NautilusIconFactory *
nautilus_get_current_icon_factory (void)
{
        static NautilusIconFactory *global_icon_factory = NULL;
        if (global_icon_factory == NULL)
                global_icon_factory = nautilus_icon_factory_new (NULL);
        return global_icon_factory;
}

GtkObject *
nautilus_icon_factory_get (void)
{
	return GTK_OBJECT (nautilus_get_current_icon_factory ());
}

/* Create the icon factory. */
static NautilusIconFactory *
nautilus_icon_factory_new (const char *theme_name)
{
        NautilusIconFactory *factory;
        
        factory = (NautilusIconFactory *) gtk_object_new (nautilus_icon_factory_get_type (), NULL);

        factory->theme_name = g_strdup (theme_name);

        return factory;
}

static void
nautilus_icon_factory_initialize (NautilusIconFactory *factory)
{
	factory->scalable_icons = g_hash_table_new (nautilus_scalable_icon_hash,
						    nautilus_scalable_icon_equal);
        factory->icon_cache = g_hash_table_new (nautilus_icon_cache_key_hash,
						nautilus_icon_cache_key_equal);

	/* Empty out the recently-used list. */
	factory->recently_used_dummy_head.next = &factory->recently_used_dummy_head;
	factory->recently_used_dummy_head.prev = &factory->recently_used_dummy_head;
}

static void
nautilus_icon_factory_initialize_class (NautilusIconFactoryClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);

	signals[THEME_CHANGED]
		= gtk_signal_new ("theme_changed",
				  GTK_RUN_LAST,
				  object_class->type,
				  0,
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
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

/* Get the name of the current theme. */
char *
nautilus_icon_factory_get_theme (void)
{
	return g_strdup (nautilus_get_current_icon_factory ()->theme_name);
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

	gtk_signal_emit (GTK_OBJECT (factory),
			 signals[THEME_CHANGED]);
}

/* Use the MIME type to get the icon name. */
static const char *
nautilus_icon_factory_get_icon_name_for_regular_file (NautilusFile *file)
{
	char *file_name;
	gboolean is_core;
        const char *mime_type;
        const char *icon_name;

	file_name = nautilus_file_get_name (file);
	is_core = strcmp (file_name, "core") == 0;
	g_free (file_name);
	if (is_core) {
		return ICON_NAME_CORE;
	}

        mime_type = nautilus_file_get_mime_type (file);
        if (mime_type != NULL) {
                icon_name = gnome_mime_get_value (mime_type, "icon-filename");
		if (icon_name != NULL) {
			return icon_name;
		}
	}

	/* GNOME didn't give us a file name, so we have to fall back on special icon sets. */
	if (nautilus_file_is_executable (file)) {
		return ICON_NAME_EXECUTABLE;
	}
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

/* Remove the suffix, add a size, and re-add the suffix. */
static char *
add_size_to_image_name (const char *name, guint size)
{
	const char *suffix;
	char *name_without_suffix;
	char *name_with_size;

	suffix = strrchr (name, '.');
	if (suffix == NULL)
		return g_strdup_printf ("%s-%u", name, size);

	name_without_suffix = g_strndup (name, suffix - name);
	name_with_size = g_strdup_printf ("%s-%u%s", name_without_suffix, size, suffix);
	g_free (name_without_suffix);
	return name_with_size;
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
		return NULL;

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

static guint
get_larger_icon_size (guint size)
{
	if (size < NAUTILUS_ICON_SIZE_SMALLEST) {
		return NAUTILUS_ICON_SIZE_SMALLEST;
	}
	if (size < NAUTILUS_ICON_SIZE_SMALLER) {
		return NAUTILUS_ICON_SIZE_SMALLER;
	}
	if (size < NAUTILUS_ICON_SIZE_SMALL) {
		return NAUTILUS_ICON_SIZE_SMALL;
	}
	if (size < NAUTILUS_ICON_SIZE_STANDARD) {
		return NAUTILUS_ICON_SIZE_STANDARD;
	}
	if (size < NAUTILUS_ICON_SIZE_LARGE) {
		return NAUTILUS_ICON_SIZE_LARGE;
	}
	if (size < NAUTILUS_ICON_SIZE_LARGER) {
		return NAUTILUS_ICON_SIZE_LARGER;
	}
	return NAUTILUS_ICON_SIZE_LARGEST;
}

static guint
get_smaller_icon_size (guint size)
{
	if (size > NAUTILUS_ICON_SIZE_LARGEST) {
		return NAUTILUS_ICON_SIZE_LARGEST;
	}
	if (size > NAUTILUS_ICON_SIZE_LARGER) {
		return NAUTILUS_ICON_SIZE_LARGER;
	}
	if (size > NAUTILUS_ICON_SIZE_LARGE) {
		return NAUTILUS_ICON_SIZE_LARGE;
	}
	if (size > NAUTILUS_ICON_SIZE_STANDARD) {
		return NAUTILUS_ICON_SIZE_STANDARD;
	}
	if (size > NAUTILUS_ICON_SIZE_SMALL) {
		return NAUTILUS_ICON_SIZE_SMALL;
	}
	if (size > NAUTILUS_ICON_SIZE_SMALLER) {
		return NAUTILUS_ICON_SIZE_SMALLER;
	}
	return NAUTILUS_ICON_SIZE_SMALLEST;
}

/* Return true if there is another size to try.
 * Set the size pointed to by @current_size to 0 to start.
 */
static gboolean
get_next_icon_size_to_try (guint target_size, guint *current_size)
{
	guint size;

	/* Get next larger size. */
	size = *current_size;
	if (size == 0 || size >= target_size) {
		if (size == 0 && target_size != 0) {
			size = target_size - 1;
		}
		if (size < NAUTILUS_ICON_SIZE_LARGEST) {
			*current_size = get_larger_icon_size (size);
			return TRUE;
		}
		size = target_size;
	}

	/* Already hit the largest size, get the next smaller size instead. */
	if (size > NAUTILUS_ICON_SIZE_SMALLEST) {
		*current_size = get_smaller_icon_size (size);
		return TRUE;
	}

	/* Tried them all. */
	return FALSE;
}

/* This load function returns NULL if the icon is not available at this size. */
static GdkPixbuf *
load_specific_image (NautilusScalableIcon *scalable_icon,
		     guint size_in_pixels,
		     gboolean custom)
{
	if (custom) {
		/* Custom image. */
		if (size_in_pixels != NAUTILUS_ICON_SIZE_STANDARD) {
			return NULL;
		}

		/* FIXME: This works only with file:// images, because there's
		 * no convenience function for loading an image with gnome-vfs
		 * and gdk-pixbuf.
		 */
		if (!nautilus_has_prefix (scalable_icon->uri, "file://")) {
			return NULL;
		}

		return gdk_pixbuf_new_from_file (scalable_icon->uri + 7);
	} else {
		char *name;
		GdkPixbuf *image;

		/* Standard image at a particular size. */
		name = add_size_to_image_name (scalable_icon->name, size_in_pixels);
		image = nautilus_icon_factory_load_icon
			(name, scalable_icon->is_symbolic_link);
		if (image != NULL)
			return image;

		/* Standard image at standard size. */
		if (size_in_pixels != NAUTILUS_ICON_SIZE_STANDARD) {
			return NULL;
		}
		return nautilus_icon_factory_load_icon
			(scalable_icon->name, scalable_icon->is_symbolic_link);
	}
}

/* This load function is not allowed to return NULL. */
static GdkPixbuf *
load_image_for_scaling (NautilusScalableIcon *scalable_icon,
			guint requested_size,
			guint *actual_size_result,
			gboolean *custom)
{
        GdkPixbuf *image;
	guint actual_size;
	static GdkPixbuf *fallback_image;

	/* First check for a custom image. */
	actual_size = 0;
	while (get_next_icon_size_to_try (requested_size, &actual_size)) {
		image = get_image_from_cache (scalable_icon, actual_size, TRUE, TRUE);
		if (image != NULL) {
			*actual_size_result = actual_size;
			*custom = TRUE;
			return image;
		}
	}
	
	/* Next, go for the normal image. */
	actual_size = 0;
	while (get_next_icon_size_to_try (requested_size, &actual_size)) {
		image = get_image_from_cache (scalable_icon, actual_size, TRUE, FALSE);
		if (image != NULL) {
			*actual_size_result = actual_size;
			*custom = FALSE;
			return image;
		}
	}

	/* Finally, fall back on the hard-coded image. */
	if (fallback_image == NULL)
		fallback_image = gdk_pixbuf_new_from_data
			(nautilus_default_file_icon,
			 ART_PIX_RGB,
			 nautilus_default_file_icon_has_alpha,
			 nautilus_default_file_icon_width,
			 nautilus_default_file_icon_height,
			 nautilus_default_file_icon_width * 4, /* stride */
			 NULL, /* don't destroy data */
			 NULL);
	gdk_pixbuf_ref (fallback_image);
	*actual_size_result = NAUTILUS_ICON_SIZE_STANDARD;
	*custom = FALSE;
        return fallback_image;
}

/* This load function is not allowed to return NULL. */
static GdkPixbuf *
load_image_scale_if_necessary (NautilusScalableIcon *scalable_icon,
			       guint requested_size,
			       gboolean *scaled,
			       gboolean *custom)
{
        GdkPixbuf *image, *scaled_image;
	guint actual_size;
	int scaled_width, scaled_height;
	
	/* Load the image for the icon that's closest in size to what we want. */
	image = load_image_for_scaling (scalable_icon, requested_size,
					&actual_size, custom);
        if (requested_size == actual_size) {
		*scaled = FALSE;
		return image;
	}
	
	/* Scale the image to the size we want. */
	scaled_width = (gdk_pixbuf_get_width (image) * requested_size) / actual_size;
	scaled_height = (gdk_pixbuf_get_height (image) * requested_size) / actual_size;
	scaled_image = gdk_pixbuf_scale_simple
		(image, scaled_width, scaled_height, ART_FILTER_BILINEAR);
	
	gdk_pixbuf_unref (image);
	*scaled = TRUE;
	return scaled_image;
}

/* Move this item to the head of the recently-used list,
 * bumping the last item off that list if necessary.
 */
static void
mark_recently_used (NautilusCircularList *node)
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

/* Get the image for icon, handling the caching.
 * If @picky is true, then only an unscaled icon is acceptable.
 * Also, if @picky is true, the icon must be a custom icon if
 * @custom is true or a standard icon is @custom is false.
 */
static GdkPixbuf *
get_image_from_cache (NautilusScalableIcon *scalable_icon,
		      guint size_in_pixels,
		      gboolean picky,
		      gboolean custom)
{
	NautilusIconFactory *factory;
	GHashTable *hash_table;
	NautilusIconCacheKey lookup_key, *key;
        GdkPixbuf *image;
	gpointer key_in_table, value;

	g_return_val_if_fail (scalable_icon != NULL, NULL);

	factory = nautilus_get_current_icon_factory ();
	hash_table = factory->icon_cache;

	/* Check to see if it's already in the table. */
	lookup_key.scalable_icon = scalable_icon;
	lookup_key.size_in_pixels = size_in_pixels;
	if (g_hash_table_lookup_extended (hash_table, &lookup_key,
					  &key_in_table, &value)) {
		/* Found it in the table. */
		key = key_in_table;
		image = value;
		g_assert (image != NULL);

		/* If we're going to be picky, then don't accept anything
		 * other than exactly what we are looking for.
		 */
		if (picky && (key->scaled || custom != key->custom))
			return NULL;
	} else {
		gboolean got_scaled_image;
		gboolean got_custom_image;
		
		/* Not in the table, so load the image. */
		if (picky) {
			image = load_specific_image (scalable_icon, size_in_pixels, custom);
			if (image == NULL) {
				return NULL;
			}

			got_scaled_image = FALSE;
			got_custom_image = custom;
		} else {
			image = load_image_scale_if_necessary (scalable_icon, size_in_pixels,
							       &got_scaled_image,
							       &got_custom_image);
			g_assert (image != NULL);
		}

		/* Create the key for the table. */
		key = g_new0 (NautilusIconCacheKey, 1);
		nautilus_scalable_icon_ref (scalable_icon);
		key->scalable_icon = scalable_icon;
		key->size_in_pixels = size_in_pixels;
		key->scaled = got_scaled_image;
		key->custom = got_custom_image;
		
		/* Add the item to the hash table. */
		g_hash_table_insert (hash_table, key, image);
	}

	/* Since this item was used, keep it in the cache longer. */
	mark_recently_used (&key->recently_used_node);

	/* Come back later and sweep the cache. */
	nautilus_icon_factory_schedule_sweep ();

	/* Grab a ref for the caller. */
	gdk_pixbuf_ref (image);
        return image;
}

GdkPixbuf *
nautilus_icon_factory_get_pixbuf_for_icon (NautilusScalableIcon *scalable_icon,
					   guint size_in_pixels)
{
	return get_image_from_cache (scalable_icon, size_in_pixels,
				     FALSE, FALSE);
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

	g_return_val_if_fail (file != NULL, NULL);

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

	g_return_if_fail (pixmap != NULL);
	g_return_if_fail (mask != NULL);

	*pixmap = NULL;
	*mask = NULL;

	g_return_if_fail (file != NULL);

	pixbuf = nautilus_icon_factory_get_pixbuf_for_file (file, size_in_pixels);
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, pixmap, mask, 128);
	gdk_pixbuf_unref (pixbuf);
}

#if ! defined (NAUTILUS_OMIT_SELF_CHECK)

static char *
self_test_next_icon_size_to_try (guint start_size, guint current_size)
{
	gboolean got_next_size;

	got_next_size = get_next_icon_size_to_try (start_size, &current_size);
	return g_strdup_printf ("%s,%d", got_next_size ? "TRUE" : "FALSE", current_size);
}

void
nautilus_self_check_icon_factory (void)
{
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_get_icon_size_for_zoom_level (0), 12);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_get_icon_size_for_zoom_level (1), 24);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_get_icon_size_for_zoom_level (2), 36);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_get_icon_size_for_zoom_level (3), 48);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_get_icon_size_for_zoom_level (4), 72);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_get_icon_size_for_zoom_level (5), 96);
	NAUTILUS_CHECK_INTEGER_RESULT (nautilus_get_icon_size_for_zoom_level (6), 192);

	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (0), 12);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (1), 12);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (11), 12);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (12), 24);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (23), 24);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (24), 36);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (35), 36);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (36), 48);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (47), 48);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (48), 72);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (71), 72);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (72), 96);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (95), 96);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (96), 192);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (191), 192);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (192), 192);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (0xFFFFFFFF), 192);

	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (0), 12);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (1), 12);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (11), 12);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (12), 12);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (24), 12);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (25), 24);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (36), 24);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (37), 36);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (48), 36);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (49), 48);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (72), 48);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (73), 72);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (96), 72);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (97), 96);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (192), 96);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (193), 192);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (0xFFFFFFFF), 192);

	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0, 0), "TRUE,12");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0, 12), "TRUE,24");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0, 24), "TRUE,36");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0, 36), "TRUE,48");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0, 48), "TRUE,72");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0, 72), "TRUE,96");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0, 96), "TRUE,192");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0, 192), "FALSE,192");

	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (36, 0), "TRUE,36");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (36, 36), "TRUE,48");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (36, 48), "TRUE,72");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (36, 72), "TRUE,96");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (36, 96), "TRUE,192");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (36, 192), "TRUE,24");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (36, 24), "TRUE,12");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (36, 12), "FALSE,12");

	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 0), "TRUE,48");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 48), "TRUE,72");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 72), "TRUE,96");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 96), "TRUE,192");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 192), "TRUE,36");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 36), "TRUE,24");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 24), "TRUE,12");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 12), "FALSE,12");

	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 0), "TRUE,192");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 192), "TRUE,96");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 96), "TRUE,72");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 72), "TRUE,48");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 48), "TRUE,36");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 36), "TRUE,24");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 24), "TRUE,12");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 12), "FALSE,12");

	NAUTILUS_CHECK_STRING_RESULT (add_size_to_image_name ("", 0), "-0");
	NAUTILUS_CHECK_STRING_RESULT (add_size_to_image_name (".", 0), "-0.");
	NAUTILUS_CHECK_STRING_RESULT (add_size_to_image_name ("a", 12), "a-12");
	NAUTILUS_CHECK_STRING_RESULT (add_size_to_image_name ("a.png", 12), "a-12.png");
}

#endif /* ! NAUTILUS_OMIT_SELF_CHECK */
