/*  -*- Mode: C; c-set-style: linux; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * GNOME File Manager: Icon Cache
 * 
 * Copyright (C) 1999, 2000 Red Hat Inc.
 * Copyright (C) 2000 Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "fm-icon-cache.h"

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-mime-info.h>
#include <libgnome/gnome-util.h>

#include "fm-default-file-icon.h"

#define ICON_CACHE_MAX_ENTRIES 10
#define ICON_CACHE_SWEEP_TIMEOUT 10

/* This allows us to do smarter caching */
static guint use_counter = 0;

typedef struct {
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

struct _FMIconCache {
        char *theme_name;
        GHashTable *name_to_image;

        IconSet special_icon_sets[ICON_SET_SPECIAL_LAST];

        GdkPixbuf *symlink_overlay;
        
        guint sweep_timer;
};

/* forward declarations */
static GdkPixbuf *fm_icon_cache_scale (FMIconCache *factory,
		     		       GdkPixbuf *standard_sized_pixbuf,
		     		       guint size_in_pixels);


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
        if (icon_set != NULL) {
                if (free_name)
                        g_free (icon_set->name);
                if (icon_set->plain)
                        gdk_pixbuf_unref (icon_set->plain);
                if (icon_set->symlink)
                        gdk_pixbuf_unref (icon_set->symlink);
        }
}

FMIconCache *
fm_icon_cache_new(const char *theme_name)
{
        FMIconCache *factory;
        
        factory = g_new0 (FMIconCache, 1);

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
fm_icon_cache_destroy_icon_sets(gpointer key, gpointer value, gpointer user_data)
{
        icon_set_destroy(value, TRUE);
        return TRUE;
}

static void
fm_icon_cache_invalidate (FMIconCache *factory)
{
        int i;

        g_hash_table_foreach_remove (factory->name_to_image,
                                     fm_icon_cache_destroy_icon_sets,
                                     NULL);

        for (i = 0; i < ICON_SET_SPECIAL_LAST; i++)
                icon_set_destroy (&factory->special_icon_sets[i], FALSE);

        if (factory->symlink_overlay) {
                gdk_pixbuf_unref (factory->symlink_overlay);
                factory->symlink_overlay = NULL;
        }
}

void
fm_icon_cache_destroy (FMIconCache *factory)
{
        fm_icon_cache_invalidate (factory);
        g_hash_table_destroy (factory->name_to_image);

        g_free (factory->theme_name);
        g_free (factory);
}

static gboolean
icon_set_possibly_free (gpointer key, gpointer value, gpointer user_data)
{
        IconSet *is = value;

        if(is->last_use > (use_counter - ICON_CACHE_MAX_ENTRIES))
                return FALSE;

        if (is->plain && is->plain->ref_count <= 1) {
                gdk_pixbuf_unref (is->plain);
                is->plain = NULL;
        }

        if (is->symlink && is->symlink->ref_count <= 1) {
                gdk_pixbuf_unref (is->symlink);
                is->symlink = NULL;
        }

        if (!is->symlink && !is->plain) {
                g_free (is->name);
                return TRUE;
        }

        return FALSE;
}

static gboolean
fm_icon_cache_sweep(gpointer data)
{
        FMIconCache *factory = data;

        g_hash_table_foreach_remove(factory->name_to_image, icon_set_possibly_free, NULL);

        factory->sweep_timer = 0;

        return FALSE;
}

static void
fm_icon_cache_setup_sweep(FMIconCache *factory)
{
        if(factory->sweep_timer)
                return;

        if(g_hash_table_size(factory->name_to_image) < ICON_CACHE_MAX_ENTRIES)
                return;

        factory->sweep_timer = g_timeout_add(ICON_CACHE_SWEEP_TIMEOUT * 1000,
                                             fm_icon_cache_sweep, factory);
}

void
fm_icon_cache_set_theme(FMIconCache *factory, const char *theme_name)
{
        fm_icon_cache_invalidate(factory);
        g_free(factory->theme_name);
        factory->theme_name = g_strdup(theme_name);
}

static IconSet *
fm_icon_cache_get_icon_set_for_file (FMIconCache *factory, NautilusFile *file)
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
fm_icon_cache_load_file(FMIconCache *factory, const char *fn)
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
fm_icon_cache_load_icon(FMIconCache *factory, IconSet *is, gboolean is_symlink)
{
        GdkPixbuf *image;

        if(is_symlink)
                image = is->symlink;
        else
                image = is->plain;

        if (!image) {
                if (*is->name == '\0') {
                        /* This is the fallback icon set */
                        image = gdk_pixbuf_new_from_data ((guchar*)fm_default_file_icon,
                                                           ART_PIX_RGB,
                                                           fm_default_file_icon_has_alpha,
                                                           fm_default_file_icon_width,
                                                           fm_default_file_icon_height,
                                                           /* rowstride */
                                                           fm_default_file_icon_width*4,
                                                           NULL, /* don't destroy data */
                                                           NULL );
                } else {
                        /* need to load the file */
                        image = fm_icon_cache_load_file(factory, is->name);
                }
                if (is_symlink) {
                        if (!factory->symlink_overlay)
                                factory->symlink_overlay = fm_icon_cache_load_file(factory, "i-symlink.png");
                  
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

GdkPixbuf *
fm_icon_cache_get_icon_for_file (FMIconCache  *factory,
                                 NautilusFile *file,
                                 guint	       size_in_pixels)
{
        IconSet *set;
        gboolean is_symbolic_link;
        GdkPixbuf *image;

        g_return_val_if_fail (factory, NULL);
        g_return_val_if_fail (file, NULL);

        switch (nautilus_file_get_type (file)) {
        case GNOME_VFS_FILE_TYPE_UNKNOWN:
        case GNOME_VFS_FILE_TYPE_REGULAR:
        default:
                set = fm_icon_cache_get_icon_set_for_file (factory, file);
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

        is_symbolic_link = nautilus_file_is_symbolic_link (file);

        set->last_use = use_counter++;
        image = fm_icon_cache_load_icon (factory, set, is_symbolic_link);

        if (image == NULL) {
                set = &factory->special_icon_sets[ICON_SET_FALLBACK];
                set->last_use = use_counter++;
                image = fm_icon_cache_load_icon (factory, set, is_symbolic_link);
        }

        g_assert (image != NULL);

        /* First cut at handling multiple sizes. If size is other than standard,
         * scale the pixbuf here. Eventually we'll store icons at multiple sizes
         * rather than relying on scaling in every case (though we'll still need
         * scaling as a fallback). The scaled pixbufs also might want to be cached.
         * For now, assume that the icon found so far is of standard size.
         */
        if (size_in_pixels != NAUTILUS_ICON_SIZE_STANDARD)
        {
        	GdkPixbuf *non_standard_size_icon;

        	non_standard_size_icon = fm_icon_cache_scale (factory,
        						      image, 
        						      size_in_pixels);
        	gdk_pixbuf_unref (image);
        	image = non_standard_size_icon;
        }
  
        fm_icon_cache_setup_sweep (factory);
  
        return image;
}

FMIconCache *
fm_get_current_icon_cache (void)
{
        static FMIconCache *global_icon_cache = NULL;
        if (global_icon_cache == NULL)
                global_icon_cache = fm_icon_cache_new (NULL);
        return global_icon_cache;
}

static GdkPixbuf *
fm_icon_cache_scale (FMIconCache *factory,
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
					  ART_FILTER_NEAREST);	

	return result;
}
