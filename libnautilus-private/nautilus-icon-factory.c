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

#define ICON_CACHE_MAX_ENTRIES 10
#define ICON_CACHE_SWEEP_TIMEOUT 10

/* This allows us to do smarter caching */
static guint use_counter = 0;

typedef struct {
	guint ref_count;
        char *name;
        GdkPixbuf *plain, *symlink;
        guint last_use;
} IconSet;

typedef enum {
        ICON_SET_DIRECTORY,
        ICON_SET_DIRECTORY_CLOSED,
        ICON_SET_EXECUTABLE,
        ICON_SET_REGULAR,
        ICON_SET_CORE,
        ICON_SET_SOCKET,
        ICON_SET_FIFO,
        ICON_SET_CHARACTER_DEVICE,
        ICON_SET_BLOCK_DEVICE,
        ICON_SET_BROKEN_SYMBOLIC_LINK,
        ICON_SET_FALLBACK,
        ICON_SET_SPECIAL_LAST
} SpecialIconSetType;

typedef struct {
        char *theme_name;
        GHashTable *name_to_image;

        IconSet special_icon_sets[ICON_SET_SPECIAL_LAST];

        GdkPixbuf *symlink_overlay;
        
        guint sweep_timer;
} NautilusIconFactory;

struct _NautilusScalableIcon {
	guint ref_count;

	char *uri;
	IconSet *icon_set;
	gboolean is_symbolic_link;
};

/* forward declarations */
static NautilusIconFactory * nautilus_get_current_icon_factory (void);
static GdkPixbuf *           nautilus_icon_factory_scale       (NautilusIconFactory *factory,
								GdkPixbuf           *standard_sized_pixbuf,
								guint                size_in_pixels);
static NautilusScalableIcon *scalable_icon_get                 (const char          *uri,
								IconSet             *icon_set,
								gboolean             is_symbolic_link);

static IconSet *
icon_set_new (const gchar *name)
{
        IconSet *new;

        new = g_new0 (IconSet, 1);
        new->name = g_strdup (name);

        return new;
}

static void
icon_set_destroy (IconSet *icon_set, gboolean free_name)
{
        if (icon_set == NULL)
		return;

	if (free_name)
		g_free (icon_set->name);
	if (icon_set->plain != NULL)
		gdk_pixbuf_unref (icon_set->plain);
	if (icon_set->symlink != NULL)
		gdk_pixbuf_unref (icon_set->symlink);
}

static NautilusIconFactory *
nautilus_icon_factory_new (const char *theme_name)
{
        NautilusIconFactory *factory;
        
        factory = g_new0 (NautilusIconFactory, 1);

        factory->theme_name = g_strdup (theme_name);
        factory->name_to_image = g_hash_table_new (g_str_hash, g_str_equal);
        factory->special_icon_sets[ICON_SET_DIRECTORY].name = "i-directory.png"; 
        factory->special_icon_sets[ICON_SET_DIRECTORY_CLOSED].name = "i-dirclosed.png"; 
        factory->special_icon_sets[ICON_SET_EXECUTABLE].name = "i-executable.png"; 
        factory->special_icon_sets[ICON_SET_REGULAR].name = "i-regular.png"; 
        factory->special_icon_sets[ICON_SET_CORE].name = "i-core.png"; 
        factory->special_icon_sets[ICON_SET_SOCKET].name = "i-sock.png"; 
        factory->special_icon_sets[ICON_SET_FIFO].name = "i-fifo.png"; 
        factory->special_icon_sets[ICON_SET_CHARACTER_DEVICE].name = "i-chardev.png"; 
        factory->special_icon_sets[ICON_SET_BLOCK_DEVICE].name = "i-blockdev.png"; 
        factory->special_icon_sets[ICON_SET_BROKEN_SYMBOLIC_LINK].name = "i-brokenlink.png"; 
        factory->special_icon_sets[ICON_SET_FALLBACK].name = "";
        
        return factory;
}

static gboolean
nautilus_icon_factory_destroy_icon_sets (gpointer key, gpointer value, gpointer user_data)
{
        icon_set_destroy (value, TRUE);
        return TRUE;
}

static void
nautilus_icon_factory_invalidate (NautilusIconFactory *factory)
{
        int i;

        g_hash_table_foreach_remove (factory->name_to_image,
                                     nautilus_icon_factory_destroy_icon_sets,
                                     NULL);

        for (i = 0; i < ICON_SET_SPECIAL_LAST; i++)
                icon_set_destroy (&factory->special_icon_sets[i], FALSE);

        if (factory->symlink_overlay) {
                gdk_pixbuf_unref (factory->symlink_overlay);
                factory->symlink_overlay = NULL;
        }
}

#if 0

static void
nautilus_icon_factory_destroy (NautilusIconFactory *factory)
{
        nautilus_icon_factory_invalidate (factory);
        g_hash_table_destroy (factory->name_to_image);

        g_free (factory->theme_name);
        g_free (factory);
}

#endif

static gboolean
icon_set_possibly_free (gpointer key, gpointer value, gpointer user_data)
{
        IconSet *is = value;

        if (is->last_use > (use_counter - ICON_CACHE_MAX_ENTRIES))
                return FALSE;

        if (is->plain && is->plain->ref_count <= 1) {
                gdk_pixbuf_unref (is->plain);
                is->plain = NULL;
        }

        if (is->symlink && is->symlink->ref_count <= 1) {
                gdk_pixbuf_unref (is->symlink);
                is->symlink = NULL;
        }

        if (is->symlink == NULL && is->plain == NULL && is->ref_count == 0) {
                g_free (is->name);
                return TRUE;
        }

        return FALSE;
}

static gboolean
nautilus_icon_factory_sweep(gpointer data)
{
        NautilusIconFactory *factory;

	factory = data;

        g_hash_table_foreach_remove (factory->name_to_image, icon_set_possibly_free, NULL);
        factory->sweep_timer = 0;

        return FALSE;
}

static void
nautilus_icon_factory_setup_sweep(NautilusIconFactory *factory)
{
        if (factory->sweep_timer)
                return;

        if (g_hash_table_size (factory->name_to_image) < ICON_CACHE_MAX_ENTRIES)
                return;

        factory->sweep_timer = g_timeout_add (ICON_CACHE_SWEEP_TIMEOUT * 1000,
					      nautilus_icon_factory_sweep, factory);
}

void
nautilus_icon_factory_set_theme(const char *theme_name)
{
	NautilusIconFactory *factory;

	factory = nautilus_get_current_icon_factory ();
        nautilus_icon_factory_invalidate (factory);
        g_free (factory->theme_name);
        factory->theme_name = g_strdup (theme_name);
}

static IconSet *
nautilus_icon_factory_get_icon_set_for_file (NautilusIconFactory *factory, NautilusFile *file)
{
        IconSet *icon_set;
        const char *mime_type;
        const char *icon_name;

        mime_type = nautilus_file_get_mime_type (file);
        icon_name = NULL;
        if (mime_type)
                icon_name = gnome_mime_get_value (mime_type, "icon-filename");

        if (icon_name) {
                icon_set = g_hash_table_lookup (factory->name_to_image, icon_name);
                if (!icon_set) {
                        icon_set = icon_set_new (icon_name);
                        g_hash_table_insert (factory->name_to_image, icon_set->name, icon_set);
                }
        } else {
                /* We can't get a name, so we have to do some faking to figure out what set to load */
                if (nautilus_file_is_executable (file))
                        icon_set = &factory->special_icon_sets[ICON_SET_EXECUTABLE];
                else
                        icon_set = &factory->special_icon_sets[ICON_SET_REGULAR];
        }

        return icon_set;
}

static GdkPixbuf *
nautilus_icon_factory_load_file(NautilusIconFactory *factory, const char *fn)
{
        char *file_name = NULL;
        char cbuf[128];
        GdkPixbuf *image;

        if(*fn != '/') {
                if(factory->theme_name) {
                        g_snprintf(cbuf, sizeof(cbuf), "nautilus/%s/%s", factory->theme_name, fn);
                        
                        file_name = gnome_pixmap_file(cbuf);
                }
                
                if(!file_name) {
                        g_snprintf(cbuf, sizeof(cbuf), "nautilus/%s", fn);
                        file_name = gnome_pixmap_file(cbuf);
                }
        }
        
        image = gdk_pixbuf_new_from_file(file_name?file_name:fn);
        g_free(file_name);

        return image;
}

/* Splats one on top of the other, putting the src pixbuf in the lower left corner of the dest pixbuf */
static void
my_gdk_pixbuf_composite(GdkPixbuf *dest, GdkPixbuf *src)
{
        int dx, dy, dw, dh;

        dw = MIN(dest->art_pixbuf->width, src->art_pixbuf->width);
        dh = MIN(dest->art_pixbuf->width, src->art_pixbuf->width);
        dx = dw - src->art_pixbuf->width;
        dy = dh - src->art_pixbuf->height;

	gdk_pixbuf_composite(src, dest, dx, dy, dw, dh, 0, 0, 1, 1, ART_FILTER_BILINEAR, 255);
}

static GdkPixbuf *
nautilus_icon_factory_load_icon(NautilusIconFactory *factory, IconSet *is, gboolean is_symlink)
{
        GdkPixbuf *image;

        if(is_symlink)
                image = is->symlink;
        else
                image = is->plain;

        if (!image) {
                if (*is->name == '\0') {
                        /* This is the fallback icon set */
                        image = gdk_pixbuf_new_from_data ((guchar*)nautilus_default_file_icon,
                                                           ART_PIX_RGB,
                                                           nautilus_default_file_icon_has_alpha,
                                                           nautilus_default_file_icon_width,
                                                           nautilus_default_file_icon_height,
                                                           /* rowstride */
                                                           nautilus_default_file_icon_width*4,
                                                           NULL, /* don't destroy data */
                                                           NULL );
                } else {
                        /* need to load the file */
                        image = nautilus_icon_factory_load_file(factory, is->name);
                }
                if (is_symlink) {
                        if (!factory->symlink_overlay)
                                factory->symlink_overlay = nautilus_icon_factory_load_file(factory, "i-symlink.png");
                  
                        if(factory->symlink_overlay)
                                my_gdk_pixbuf_composite(image, factory->symlink_overlay);
                        is->symlink = image;
                } else
                        is->plain = image;
        }

        if (image)
                gdk_pixbuf_ref(image); /* Returned value is owned by caller */

        return image;
}

static NautilusScalableIcon *
scalable_icon_new (const char *uri,
		   IconSet *icon_set,
		   gboolean is_symbolic_link)
{
	NautilusScalableIcon *icon;

	g_return_val_if_fail (icon_set != NULL, NULL);

	icon = g_new (NautilusScalableIcon, 1);
	icon->ref_count = 1;
	icon->uri = g_strdup (uri);
	icon->icon_set = icon_set;
	icon->is_symbolic_link = is_symbolic_link;

	icon_set->ref_count++;

	return icon;
}

static NautilusScalableIcon *
scalable_icon_get (const char *uri,
		   IconSet *icon_set,
		   gboolean is_symbolic_link)
{
	/* FIXME: These should come from a hash table. */
	return scalable_icon_new (uri, icon_set, is_symbolic_link);
}

void
nautilus_scalable_icon_unref (NautilusScalableIcon *icon)
{
	g_return_if_fail (icon->ref_count != 0);

	if (--icon->ref_count != 0)
		return;

	g_free (icon->uri);

	g_assert (icon->icon_set->ref_count != 0);
	icon->icon_set->ref_count--;

	g_free (icon);
}

NautilusScalableIcon *
nautilus_icon_factory_get_icon_for_file (NautilusFile *file)
{
	NautilusIconFactory *factory;
        IconSet *set;
	char *uri;
        gboolean is_symbolic_link;
	NautilusScalableIcon *scalable_icon;
	
 	factory = nautilus_get_current_icon_factory ();
	
	/* Get an icon set based on the file's type. */
        switch (nautilus_file_get_type (file)) {
        case GNOME_VFS_FILE_TYPE_UNKNOWN:
        case GNOME_VFS_FILE_TYPE_REGULAR:
        default:
                set = nautilus_icon_factory_get_icon_set_for_file (factory, file);
                break;
        case GNOME_VFS_FILE_TYPE_DIRECTORY:
                set = &factory->special_icon_sets[ICON_SET_DIRECTORY];
                break;
        case GNOME_VFS_FILE_TYPE_FIFO:
                set = &factory->special_icon_sets[ICON_SET_FIFO];
                break;
        case GNOME_VFS_FILE_TYPE_SOCKET:
                set = &factory->special_icon_sets[ICON_SET_SOCKET];
                break;
        case GNOME_VFS_FILE_TYPE_CHARDEVICE:
                set = &factory->special_icon_sets[ICON_SET_CHARACTER_DEVICE];
                break;
        case GNOME_VFS_FILE_TYPE_BLOCKDEVICE:
                set = &factory->special_icon_sets[ICON_SET_BLOCK_DEVICE];
                break;
        case GNOME_VFS_FILE_TYPE_BROKENSYMLINK:
                set = &factory->special_icon_sets[ICON_SET_BROKEN_SYMBOLIC_LINK];
                break;
        }
	
	/* Also record whether it's a symbolic link or not.
	 * Later, we'll probably use a separate icon badge for this,
	 * but for now, we'll keep it.
	 */
        is_symbolic_link = nautilus_file_is_symbolic_link (file);
	
	/* Use the image itself as a custom icon. */
	if (nautilus_has_prefix (nautilus_file_get_mime_type (file), "image/")
	    && nautilus_file_get_size (file) < 10000)
		uri = nautilus_file_get_uri (file);
	else
		uri = NULL;
	
	/* Create the icon or find it in the cache if it's already there. */
	scalable_icon = scalable_icon_get (uri, set, is_symbolic_link);
	g_free (uri);
	
        nautilus_icon_factory_setup_sweep (factory);
	
	return scalable_icon;
}

GdkPixbuf *
nautilus_icon_factory_get_pixbuf_for_icon (NautilusScalableIcon *scalable_icon,
					   guint	         size_in_pixels)
{
	NautilusIconFactory *factory;
        IconSet *set;
        GdkPixbuf *image;

	factory = nautilus_get_current_icon_factory (); 

	/* FIXME: This works only with file:// images, because there's
	 * no convenience function for loading an image with gnome-vfs
	 * and gdk-pixbuf.
	 */
	image = NULL;
	if (nautilus_has_prefix (scalable_icon->uri, "file://"))
		image = gdk_pixbuf_new_from_file (scalable_icon->uri + 7);
	
	/* If there was no suitable custom icon URI, then use the icon set. */
	if (image == NULL) {
		set = scalable_icon->icon_set;
		set->last_use = use_counter++;
		image = nautilus_icon_factory_load_icon
			(factory, set, scalable_icon->is_symbolic_link);
	}
	
	/* If the icon set failed, then use the fallback set. */
        if (image == NULL) {
		g_warning ("failed to load icon, using fallback icon set");
                set = &factory->special_icon_sets[ICON_SET_FALLBACK];
                set->last_use = use_counter++;
                image = nautilus_icon_factory_load_icon
			(factory, set, scalable_icon->is_symbolic_link);
        }
	
        g_assert (image != NULL);

        /* First cut at handling multiple sizes. If size is other than standard,
         * scale the pixbuf here. Eventually we'll store icons at multiple sizes
         * rather than relying on scaling in every case (though we'll still need
         * scaling as a fallback). We'll also cache the scaled pixbufs.
         * For now, assume that the icon found so far is of standard size.
         */
        if (size_in_pixels != NAUTILUS_ICON_SIZE_STANDARD)
        {
        	GdkPixbuf *scaled_icon;

        	scaled_icon = nautilus_icon_factory_scale
			(factory, image, size_in_pixels);
        	gdk_pixbuf_unref (image);
        	image = scaled_icon;
        }
  
        nautilus_icon_factory_setup_sweep (factory);
  
        return image;
}

NautilusIconFactory *
nautilus_get_current_icon_factory (void)
{
        static NautilusIconFactory *global_icon_factory = NULL;
        if (global_icon_factory == NULL)
                global_icon_factory = nautilus_icon_factory_new (NULL);
        return global_icon_factory;
}

static GdkPixbuf *
nautilus_icon_factory_scale (NautilusIconFactory *factory,
		     GdkPixbuf *standard_sized_pixbuf,
		     guint size_in_pixels)
{
	GdkPixbuf *result;
	int old_width, old_height, new_width, new_height;
	
	g_return_val_if_fail (standard_sized_pixbuf != NULL, NULL);

	old_width = gdk_pixbuf_get_width (standard_sized_pixbuf);
	old_height = gdk_pixbuf_get_height (standard_sized_pixbuf);

	new_width = (old_width * size_in_pixels) / NAUTILUS_ICON_SIZE_STANDARD;
	new_height = (old_height * size_in_pixels) / NAUTILUS_ICON_SIZE_STANDARD;

	/* This creates scaled icon with ref. count of 1. */
	result = gdk_pixbuf_scale_simple (standard_sized_pixbuf, 
					  new_width, 
					  new_height, 
					  ART_FILTER_BILINEAR);

	return result;
}


/* 
 * Return nominal icon size for given zoom level.
 * @zoom_level: zoom level for which to find matching icon size.
 * 
 * Return value: icon size between NAUTILUS_ICON_SIZE_SMALLEST and
 * NAUTILUS_ICON_SIZE_LARGEST, inclusive.
 */
guint
nautilus_icon_size_for_zoom_level (NautilusZoomLevel zoom_level)
{
	switch (zoom_level)
	{
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
			g_assert_not_reached();
			return NAUTILUS_ICON_SIZE_STANDARD;
	}
}
