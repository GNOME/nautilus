/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-icon-factory.c: Class for obtaining icons for files and other objects.
 
   Copyright (C) 1999, 2000 Red Hat Inc.
   Copyright (C) 1999, 2000, 2001 Eazel, Inc.
  
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
  
   Authors: John Sullivan <sullivan@eazel.com>,
            Darin Adler <darin@bentspoon.com>,
	    Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include "nautilus-icon-factory.h"

#include "nautilus-default-file-icon.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-icon-factory-private.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-link.h"
#include "nautilus-theme.h"
#include "nautilus-thumbnails.h"
#include "nautilus-trash-monitor.h"
#include <eel/eel-debug.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-pango-extensions.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtksettings.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>
#include <libgnome/gnome-util.h>
#include <libgnome/gnome-macros.h>
#include <libgnomeui/gnome-icon-lookup.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-mime-handlers.h>
#include <libgnomevfs/gnome-vfs-mime-monitor.h>
#include <libgnomevfs/gnome-vfs-ops.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <librsvg/rsvg.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define CACHE_SELF_CHECKS 0

#define ICON_NAME_THUMBNAIL_LOADING     "gnome-fs-loading-icon"
#define ICON_NAME_TRASH_EMPTY		"gnome-fs-trash-empty"
#define ICON_NAME_TRASH_FULL		"gnome-fs-trash-full"

#define NAUTILUS_EMBLEM_NAME_PREFIX "emblem-"

/* This used to be called ICON_CACHE_MAX_ENTRIES, but it's misleading
 * to call it that, since we can have any number of entries in the
 * cache if the caller keeps the pixbuf around (we only get rid of
 * items from the cache after the caller unref's them).
*/
#define ICON_CACHE_COUNT                128

/* This is the number of milliseconds we wait before sweeping out
 * items from the cache.
 */
#define ICON_CACHE_SWEEP_TIMEOUT        (10 * 1000)

/* This circular doubly-linked list structure is used to keep a list
 * of the most recently used items in the cache.
 */
typedef struct CircularList CircularList;
struct CircularList {
	CircularList *next;
	CircularList *prev;
};

/* The key to a hash table that holds CacheIcons. */
typedef struct {
	char *name; /* Icon name or absolute filename */
	char *modifier;
	guint nominal_size;
} CacheKey;

/* The value in the same table. */
typedef struct {
	guint ref_count;
	
	GdkPixbuf *pixbuf;
	GnomeIconData *icon_data;
	time_t mtime; /* Only used for absolute filenames */

	CircularList recently_used_node;
} CacheIcon;

/* The icon factory.
 * These are just globals, but they're in an object so we can
 * connect signals and have multiple icon factories some day
 * if we want to.
 */
typedef struct {
	GObject object;

	/* A hash table that contains the icons. A circular list of
	 * the most recently used icons is kept around, and we don't
	 * let them go when we sweep the cache.
	 */
	GHashTable *icon_cache;

	/* frames to use for thumbnail icons */
	GdkPixbuf *thumbnail_frame;

	/* Used for icon themes according to the freedesktop icon spec. */
	GnomeIconTheme *icon_theme;
	GnomeThumbnailFactory *thumbnail_factory;

	CircularList recently_used_dummy_head;
	guint recently_used_count;
        guint sweep_timer;

	CacheIcon *fallback_icon;
} NautilusIconFactory;

#define NAUTILUS_ICON_FACTORY(obj) \
	GTK_CHECK_CAST (obj, nautilus_icon_factory_get_type (), NautilusIconFactory)

typedef struct {
	GObjectClass parent_class;
} NautilusIconFactoryClass;

enum {
	ICONS_CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];


static int cached_thumbnail_limit;
static int show_image_thumbs;

/* forward declarations */

static GType      nautilus_icon_factory_get_type         (void);
static void       nautilus_icon_factory_class_init       (NautilusIconFactoryClass *class);
static void       nautilus_icon_factory_instance_init    (NautilusIconFactory      *factory);
static void       nautilus_icon_factory_finalize         (GObject                  *object);
static void       thumbnail_limit_changed_callback       (gpointer                  user_data);
static void       show_thumbnails_changed_callback       (gpointer                  user_data);
static void       mime_type_data_changed_callback        (GnomeVFSMIMEMonitor	   *monitor,
							  gpointer                  user_data);
static guint      cache_key_hash                         (gconstpointer             p);
static gboolean   cache_key_equal                        (gconstpointer             a,
							  gconstpointer             b);
static void       cache_key_destroy                      (CacheKey                 *key);
static void       cache_icon_unref                       (CacheIcon                *icon);
static CacheIcon *cache_icon_new                         (GdkPixbuf                *pixbuf,
							  GnomeIconData            *icon_data);
static CacheIcon *get_icon_from_cache                    (const char               *icon,
							  const char               *modifier,
							  guint                     nominal_size);
static void nautilus_icon_factory_clear                  (void);

GNOME_CLASS_BOILERPLATE (NautilusIconFactory,
			 nautilus_icon_factory,
			 GObject, G_TYPE_OBJECT);

static NautilusIconFactory *global_icon_factory = NULL;

static void
destroy_icon_factory (void)
{
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT,
					 thumbnail_limit_changed_callback,
					 NULL);
	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
					 show_thumbnails_changed_callback,
					 NULL);
	g_object_unref (global_icon_factory);
}

/* Return a pointer to the single global icon factory. */
static NautilusIconFactory *
get_icon_factory (void)
{
        if (global_icon_factory == NULL) {
		nautilus_global_preferences_init ();

		global_icon_factory = NAUTILUS_ICON_FACTORY
			(g_object_new (nautilus_icon_factory_get_type (), NULL));

		thumbnail_limit_changed_callback (NULL);
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT,
					      thumbnail_limit_changed_callback,
					      NULL);

		show_thumbnails_changed_callback (NULL);
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
					      show_thumbnails_changed_callback,
					      NULL);

		g_signal_connect (gnome_vfs_mime_monitor_get (),
				  "data_changed",
				  G_CALLBACK (mime_type_data_changed_callback),
				  NULL);

		eel_debug_call_at_shutdown (destroy_icon_factory);
        }
        return global_icon_factory;
}

GObject *
nautilus_icon_factory_get (void)
{
	return G_OBJECT (get_icon_factory ());
}

static void
icon_theme_changed_callback (GnomeIconTheme *icon_theme,
			     gpointer user_data)
{
	NautilusIconFactory *factory;

	nautilus_icon_factory_clear ();

	factory = user_data;

	g_signal_emit (factory,
		       signals[ICONS_CHANGED], 0);
}

GnomeIconTheme *
nautilus_icon_factory_get_icon_theme (void)
{
	NautilusIconFactory *factory;

	factory = get_icon_factory ();

	return g_object_ref (factory->icon_theme);
}

GnomeThumbnailFactory *
nautilus_icon_factory_get_thumbnail_factory ()
{
	NautilusIconFactory *factory;

	factory = get_icon_factory ();

	return g_object_ref (factory->thumbnail_factory);
}


static void
check_recently_used_list (void)
{
#if CACHE_SELF_CHECKS
	NautilusIconFactory *factory;
	CircularList *head, *node, *next;
	guint count;

	factory = get_icon_factory ();

	head = &factory->recently_used_dummy_head;
	
	count = 0;
	
	node = head;
	while (1) {
		next = node->next;
		g_assert (next != NULL);
		g_assert (next->prev == node);

		if (next == head) {
			break;
		}

		count += 1;

		node = next;
	}

	g_assert (count == factory->recently_used_count);
#endif
}


/* load the thumbnail frame */
static void
load_thumbnail_frame (NautilusIconFactory *factory)
{
	char *image_path;
	
	image_path = nautilus_theme_get_image_path ("thumbnail_frame.png");
	if (factory->thumbnail_frame != NULL) {
		g_object_unref (factory->thumbnail_frame);
	}
	factory->thumbnail_frame = gdk_pixbuf_new_from_file (image_path, NULL);
	g_free (image_path);
}

static void
nautilus_icon_factory_instance_init (NautilusIconFactory *factory)
{
	GdkPixbuf *pixbuf;
	
        factory->icon_cache = g_hash_table_new_full (cache_key_hash,
						     cache_key_equal,
						     (GDestroyNotify)cache_key_destroy,
						     (GDestroyNotify)cache_icon_unref);
	
	factory->icon_theme = gnome_icon_theme_new ();
	gnome_icon_theme_set_allow_svg (factory->icon_theme, TRUE);
	g_signal_connect_object (factory->icon_theme,
				 "changed",
				 G_CALLBACK (icon_theme_changed_callback),
				 factory, 0);


	factory->thumbnail_factory = gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);
	load_thumbnail_frame (factory);

	/* Empty out the recently-used list. */
	factory->recently_used_dummy_head.next = &factory->recently_used_dummy_head;
	factory->recently_used_dummy_head.prev = &factory->recently_used_dummy_head;
	
	pixbuf = gdk_pixbuf_new_from_data (nautilus_default_file_icon,
					   GDK_COLORSPACE_RGB,
					   TRUE,
					   8,
					   nautilus_default_file_icon_width,
					   nautilus_default_file_icon_height,
					   nautilus_default_file_icon_width * 4, /* stride */
					   NULL, /* don't destroy data */
					   NULL);
	
	factory->fallback_icon = cache_icon_new (pixbuf, NULL);
}

static void
nautilus_icon_factory_class_init (NautilusIconFactoryClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);

	signals[ICONS_CHANGED]
		= g_signal_new ("icons_changed",
		                G_TYPE_FROM_CLASS (object_class),
		                G_SIGNAL_RUN_LAST,
		                0,
		                NULL, NULL,
		                g_cclosure_marshal_VOID__VOID,
		                G_TYPE_NONE, 0);

	object_class->finalize = nautilus_icon_factory_finalize;
}

static void
cache_key_destroy (CacheKey *key)
{
	g_free (key->name);
	g_free (key);
}

static CacheIcon *
cache_icon_new (GdkPixbuf     *pixbuf,
		GnomeIconData *icon_data)
{
	CacheIcon *icon;

	/* Grab the pixbuf since we are keeping it. */
	g_object_ref (pixbuf);

	/* Make the icon. */
	icon = g_new0 (CacheIcon, 1);
	icon->ref_count = 1;
	icon->pixbuf = pixbuf;
	icon->icon_data = icon_data;
	icon->mtime = 0;
	
	return icon;
}

static  void
cache_icon_ref (CacheIcon *icon)
{
	g_assert (icon != NULL);
	g_assert (icon->ref_count >= 1);

	icon->ref_count++;
}

static void
cache_icon_unref (CacheIcon *icon)
{
	CircularList *node;
        NautilusIconFactory *factory;
	
	g_assert (icon != NULL);
	g_assert (icon->ref_count >= 1);

	if (icon->ref_count > 1) {
		icon->ref_count--;
		return;
	}
	
	icon->ref_count = 0;

	factory = get_icon_factory ();
	
	check_recently_used_list ();

	/* If it's in the recently used list, free it from there */      
	node = &icon->recently_used_node;
	if (node->next != NULL) {
#if CACHE_SELF_CHECKS
		g_assert (factory->recently_used_count >= 1);
		
		g_assert (node->next->prev == node);
		g_assert (node->prev->next == node);
		g_assert (node->next != node);
		g_assert (node->prev != node);
#endif
		node->next->prev = node->prev;
		node->prev->next = node->next;

		node->next = NULL;
		node->prev = NULL;

		factory->recently_used_count -= 1;
	}
	
	check_recently_used_list ();
	
	g_object_unref (icon->pixbuf);
	
	if (icon->icon_data) {
		gnome_icon_data_free (icon->icon_data);
		icon->icon_data = NULL;
	}

	g_free (icon);
}


static gboolean
nautilus_icon_factory_possibly_free_cached_icon (gpointer key,
						 gpointer value,
						 gpointer user_data)
{
        CacheIcon *icon;
	
	icon = value;
	
	/* Don't free a cache entry that is in the recently used list. */
        if (icon->recently_used_node.next != NULL) {
                return FALSE;
	}

	/* Don't free a cache entry if the pixbuf is still in use. */
	if (G_OBJECT (icon->pixbuf)->ref_count > 1) {
		return FALSE;
	}

	/* Free the item. */
        return TRUE;
}


/* Sweep the cache, freeing any icons that are not in use and are
 * also not recently used.
 */
static gboolean
nautilus_icon_factory_sweep (gpointer user_data)
{
        NautilusIconFactory *factory;

	factory = user_data;

	g_hash_table_foreach_remove (factory->icon_cache,
				     nautilus_icon_factory_possibly_free_cached_icon,
				     NULL);

	factory->sweep_timer = 0;
	return FALSE;
}

/* Schedule a timer to do a sweep. */
static void
nautilus_icon_factory_schedule_sweep (NautilusIconFactory *factory)
{
        if (factory->sweep_timer != 0) {
                return;
	}

        factory->sweep_timer = g_timeout_add (ICON_CACHE_SWEEP_TIMEOUT,
					      nautilus_icon_factory_sweep,
					      factory);
}

/* Move this item to the head of the recently-used list,
 * bumping the last item off that list if necessary.
 */
static void
mark_recently_used (CircularList *node)
{
	NautilusIconFactory *factory;
	CircularList *head, *last_node;

	check_recently_used_list ();

	factory = get_icon_factory ();
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
			if (factory->recently_used_count < ICON_CACHE_COUNT) {
				factory->recently_used_count += 1;
			} else {
				/* Remove the last node. */
				last_node = head->prev;

#if CACHE_SELF_CHECKS
				g_assert (last_node != head);
				g_assert (last_node != node);
#endif
				
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

	check_recently_used_list ();
}

static gboolean
remove_all (gpointer key, gpointer value, gpointer user_data)
{
	/* Tell the caller to remove the hash table entry. */
        return TRUE;
}

/* Reset the cache to the default state. */
static void
nautilus_icon_factory_clear (void)
{
	NautilusIconFactory *factory;
	CircularList *head;
	
	factory = get_icon_factory ();

        g_hash_table_foreach_remove (factory->icon_cache,
				     remove_all,
                                     NULL);
	
	/* Empty out the recently-used list. */
	head = &factory->recently_used_dummy_head;

	/* fallback_icon hangs around, but we don't know if it
	 * was ever inserted in the list
	 */
	g_assert (factory->recently_used_count == 0 ||
		  factory->recently_used_count == 1);

	if (factory->recently_used_count == 1) {
		/* make sure this one is the fallback_icon */
		g_assert (head->next == head->prev);
		g_assert (&factory->fallback_icon->recently_used_node == head->next);
	}

}

static void
nautilus_icon_factory_finalize (GObject *object)
{
	NautilusIconFactory *factory;

	factory = NAUTILUS_ICON_FACTORY (object);

	if (factory->icon_cache) {
		g_hash_table_destroy (factory->icon_cache);
		factory->icon_cache = NULL;
	}
	
	if (factory->thumbnail_frame != NULL) {
		g_object_unref (factory->thumbnail_frame);
		factory->thumbnail_frame = NULL;
	}

	if (factory->fallback_icon) {
		g_assert (factory->fallback_icon->ref_count == 1);
		cache_icon_unref (factory->fallback_icon);
	}
	
	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
thumbnail_limit_changed_callback (gpointer user_data)
{
	cached_thumbnail_limit = eel_preferences_get_integer (NAUTILUS_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT);

	/* Tell the world that icons might have changed. We could invent a narrower-scope
	 * signal to mean only "thumbnails might have changed" if this ends up being slow
	 * for some reason.
	 */
	nautilus_icon_factory_clear ();
	g_signal_emit (global_icon_factory,
			 signals[ICONS_CHANGED], 0);
}

static void
show_thumbnails_changed_callback (gpointer user_data)
{
	show_image_thumbs = eel_preferences_get_enum (NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS);

	nautilus_icon_factory_clear ();
	/* If the user disabled thumbnailing, remove all outstanding thumbnails */ 
	if (show_image_thumbs == NAUTILUS_SPEED_TRADEOFF_NEVER) {
		nautilus_thumbnail_remove_all_from_queue ();
	}
	g_signal_emit (global_icon_factory,
		       signals[ICONS_CHANGED], 0);
}

static void       
mime_type_data_changed_callback (GnomeVFSMIMEMonitor *monitor, gpointer user_data)
{
	g_assert (monitor != NULL);
	g_assert (user_data == NULL);

	/* We don't know which data changed, so we have to assume that
	 * any or all icons might have changed.
	 */
	nautilus_icon_factory_clear ();
	g_signal_emit (get_icon_factory (), 
			 signals[ICONS_CHANGED], 0);
}				 

static char *
nautilus_remove_icon_file_name_suffix (const char *icon_name)
{
	guint i;
	const char *suffix;
	static const char *icon_file_name_suffixes[] = { ".svg", ".svgz", ".png", ".jpg" };

	for (i = 0; i < G_N_ELEMENTS (icon_file_name_suffixes); i++) {
		suffix = icon_file_name_suffixes[i];
		if (eel_str_has_suffix (icon_name, suffix)) {
			return eel_str_strip_trailing_str (icon_name, suffix);
		}
	}
	return g_strdup (icon_name);
}

static char *
image_uri_to_name_or_uri (const char *image_uri)
{
	char *icon_path;

	icon_path = gnome_vfs_get_local_path_from_uri (image_uri);
	if (icon_path == NULL && image_uri[0] == '/') {
		icon_path = g_strdup (image_uri);
	}
	if (icon_path != NULL) {
		return icon_path;
	} else if (strpbrk (image_uri, ":/") == NULL) {
		return nautilus_remove_icon_file_name_suffix (image_uri);
	}
	return NULL;
}

static gboolean
mimetype_limited_by_size (const char *mime_type)
{
        guint i;
        static GHashTable *formats = NULL;
        static const char *types [] = {
          "image/x-bmp", "image/x-ico", "image/jpeg", "image/gif",
          "image/png", "image/pnm", "image/ras", "image/tga",
          "image/tiff", "image/wbmp", "image/x-xbitmap",
          "image/x-xpixmap"
        };

	if (formats == NULL) {
		formats = eel_g_hash_table_new_free_at_exit
			(g_str_hash, g_str_equal,
			 "nautilus-icon-factory.c: mimetype_limited_by_size");
		
                for (i = 0; i < G_N_ELEMENTS (types); i++) {
                        g_hash_table_insert (formats,
                                             (gpointer) types [i],
                                             GUINT_TO_POINTER (1));
		}
        }

        if (g_hash_table_lookup (formats, mime_type)) {
                return TRUE;
	}

        return FALSE;
}

static gboolean
should_show_thumbnail (NautilusFile *file, const char *mime_type)
{
	if (mimetype_limited_by_size (mime_type) &&
	    nautilus_file_get_size (file) > (unsigned int)cached_thumbnail_limit) {
		return FALSE;
	}
	
	if (show_image_thumbs == NAUTILUS_SPEED_TRADEOFF_ALWAYS) {
		return TRUE;
	} else if (show_image_thumbs == NAUTILUS_SPEED_TRADEOFF_NEVER) {
		return FALSE;
	} else {
		/* only local files */
		return nautilus_file_is_local (file);
	}

	return FALSE;
}

/* key routine to get the icon for a file */
char *
nautilus_icon_factory_get_icon_for_file (NautilusFile *file, gboolean embedd_text)
{
 	char *custom_uri, *file_uri, *icon_name, *mime_type, *custom_icon;
	NautilusIconFactory *factory;
	GnomeIconLookupResultFlags lookup_result;
	GnomeVFSFileInfo *file_info;
	GnomeThumbnailFactory *thumb_factory;
	gboolean show_thumb;
	GnomeIconLookupFlags lookup_flags;
	
	if (file == NULL) {
		return NULL;
	}

	factory = get_icon_factory ();
	
	custom_icon = NULL;
 
 	/* if there is a custom image in the metadata or link info, use that. */
 	custom_uri = nautilus_file_get_custom_icon (file);
	if (custom_uri) {
		custom_icon = image_uri_to_name_or_uri (custom_uri);
	}
 	g_free (custom_uri);

	file_uri = nautilus_file_get_uri (file);
	
	if (strcmp (file_uri, EEL_TRASH_URI) == 0) {
		g_free (file_uri);

		return  g_strdup (nautilus_trash_monitor_is_empty ()
				  ? ICON_NAME_TRASH_EMPTY : ICON_NAME_TRASH_FULL);
	}
	
	mime_type = nautilus_file_get_mime_type (file);
	
	file_info = nautilus_file_peek_vfs_file_info (file);
	
	show_thumb = should_show_thumbnail (file, mime_type);	
	
	if (show_thumb) {
		thumb_factory = factory->thumbnail_factory;
	} else {
		thumb_factory = NULL;
	}

	lookup_flags = GNOME_ICON_LOOKUP_FLAGS_SHOW_SMALL_IMAGES_AS_THEMSELVES;
	if (embedd_text) {
		lookup_flags |= GNOME_ICON_LOOKUP_FLAGS_EMBEDDING_TEXT;
	}
	icon_name = gnome_icon_lookup (factory->icon_theme,
				       thumb_factory,
				       file_uri,
				       custom_icon,
				       nautilus_file_peek_vfs_file_info (file),
				       mime_type,
				       lookup_flags,
				       &lookup_result);


	/* Create thumbnails if we can, and if the looked up icon isn't a thumbnail
	   or an absolute pathname (custom icon or image as itself) */
	if (show_thumb &&
	    !(lookup_result & GNOME_ICON_LOOKUP_RESULT_FLAGS_THUMBNAIL) &&
	    icon_name[0] != '/' && file_info &&
	    gnome_thumbnail_factory_can_thumbnail (factory->thumbnail_factory,
						   file_uri,
						   mime_type,
						   file_info->mtime)) {
		nautilus_create_thumbnail (file);
		g_free (icon_name);
		icon_name = g_strdup (ICON_NAME_THUMBNAIL_LOADING);
	}
	
        g_free (file_uri);
        g_free (custom_icon);
	g_free (mime_type);
	
	return icon_name;
}

/**
 * nautilus_icon_factory_get_required_file_attributes
 * 
 * Get the list of file attributes required to obtain a file's icon.
 * Callers must free this list.
 */
GList *
nautilus_icon_factory_get_required_file_attributes (void)
{
	GList *attributes;

	attributes = g_list_prepend (NULL, NAUTILUS_FILE_ATTRIBUTE_CUSTOM_ICON);
	attributes = g_list_prepend (attributes,
				     NAUTILUS_FILE_ATTRIBUTE_MIME_TYPE);

	return attributes;
}


/**
 * nautilus_icon_factory_is_icon_ready_for_file
 * 
 * Check whether a NautilusFile has enough information to report
 * what its icon should be.
 * 
 * @file: The NautilusFile in question.
 */
gboolean
nautilus_icon_factory_is_icon_ready_for_file (NautilusFile *file)
{
	GList *attributes;
	gboolean result;

	attributes = nautilus_icon_factory_get_required_file_attributes ();
	result = nautilus_file_check_if_ready (file, attributes);
	g_list_free (attributes);

	return result;
}

char *
nautilus_icon_factory_get_emblem_icon_by_name (const char *emblem_name)
{
	char *name_with_prefix;

	name_with_prefix = g_strconcat (NAUTILUS_EMBLEM_NAME_PREFIX, emblem_name, NULL);

	return name_with_prefix;
}

GList *
nautilus_icon_factory_get_emblem_icons_for_file (NautilusFile *file,
						 EelStringList *exclude)
{
	GList *icons, *emblem_names, *node;
	char *uri, *name;
	char *icon;
	gboolean file_is_trash;

	icons = NULL;

	emblem_names = nautilus_file_get_emblem_names (file);
	for (node = emblem_names; node != NULL; node = node->next) {
		name = node->data;
		if (strcmp (name, NAUTILUS_FILE_EMBLEM_NAME_TRASH) == 0) {
			/* Leave out the trash emblem for the trash itself, since
			 * putting a trash emblem on a trash icon is gilding the
			 * lily.
			 */
			uri = nautilus_file_get_uri (file);
			file_is_trash = strcmp (uri, EEL_TRASH_URI) == 0;
			g_free (uri);
			if (file_is_trash) {
				continue;
			}
		}
		if (eel_string_list_contains (exclude, name)) {
			continue;
		}
		icon = nautilus_icon_factory_get_emblem_icon_by_name (name);
		icons = g_list_prepend (icons, icon);
	}
	eel_g_list_free_deep (emblem_names);

	return g_list_reverse (icons);
}

static guint
get_larger_icon_size (guint size)
{
	if (size < NAUTILUS_ICON_SIZE_SMALLEST) {
		return NAUTILUS_ICON_SIZE_SMALLEST;
	}
	if (size < NAUTILUS_ICON_SIZE_FOR_MENUS) {
		return NAUTILUS_ICON_SIZE_FOR_MENUS;
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
	if (size > NAUTILUS_ICON_SIZE_FOR_MENUS) {
		return NAUTILUS_ICON_SIZE_FOR_MENUS;
	}
	return NAUTILUS_ICON_SIZE_SMALLEST;
}


static void
scale_icon_data (GnomeIconData *icon_data,
		 double scale_x,
		 double scale_y)
{
	int num_points, i;
	
	if (icon_data->has_embedded_rect) {
		icon_data->x0 = icon_data->x0 * scale_x;
		icon_data->y0 = icon_data->y0 * scale_y;
		icon_data->x1 = icon_data->x1 * scale_x;
		icon_data->y1 = icon_data->y1 * scale_y;
	}
	
	num_points = icon_data->n_attach_points;
	for (i = 0; i < num_points; i++) {
		icon_data->attach_points[i].x = icon_data->attach_points[i].x * scale_x;
		icon_data->attach_points[i].y = icon_data->attach_points[i].y * scale_y;
	}
}


/* This loads an SVG image, scaling it to the appropriate size. */
static GdkPixbuf *
load_pixbuf_svg (const char *path,
		 guint size_in_pixels,
		 guint base_size,
		 GnomeIconData *icon_data)
{
	double zoom;
	int width, height;
	GdkPixbuf *pixbuf;

	if (base_size != 0) {
		zoom = (double)size_in_pixels / base_size;

		pixbuf = rsvg_pixbuf_from_file_at_zoom_with_max (path, zoom, zoom, NAUTILUS_ICON_MAXIMUM_SIZE, NAUTILUS_ICON_MAXIMUM_SIZE, NULL);
	} else {
		pixbuf = rsvg_pixbuf_from_file_at_max_size (path,
							    size_in_pixels,
							    size_in_pixels,
							    NULL);
	}

	if (pixbuf == NULL) {
		return NULL;
	}

	if (icon_data != NULL) {
		width = gdk_pixbuf_get_width (pixbuf);
		height = gdk_pixbuf_get_height (pixbuf);
		scale_icon_data (icon_data, width / 1000.0, height / 1000.0);
	}
	return pixbuf;
}

static gboolean
path_represents_svg_image (const char *path) 
{
	/* Synchronous mime sniffing is a really bad idea here
	 * since it's only useful for people adding custom icons,
	 * and if they're doing that, they can behave themselves
	 * and use a .svg extension.
	 */
	return path != NULL && (strstr (path, ".svg") != NULL || strstr (path, ".svgz") != NULL);
}

static GdkPixbuf *
scale_icon (GdkPixbuf *pixbuf,
	    double *scale)
{
	guint width, height;

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	if ((int) (width * *scale) > NAUTILUS_ICON_MAXIMUM_SIZE ||
	    (int) (height * *scale) > NAUTILUS_ICON_MAXIMUM_SIZE) {
		*scale = MIN ((double) NAUTILUS_ICON_MAXIMUM_SIZE / width,
			      (double) NAUTILUS_ICON_MAXIMUM_SIZE / height);
	}

	width = floor (width * *scale + 0.5);
	height = floor (height * *scale + 0.5);
	
	return gdk_pixbuf_scale_simple (pixbuf, width, height, GDK_INTERP_BILINEAR);
}

static GdkPixbuf *
load_icon_file (char          *filename,
		guint          base_size,
		guint          nominal_size,
		GnomeIconData *icon_data)
{
	GdkPixbuf *pixbuf, *scaled_pixbuf;
	int width, height, size;
	double scale;
	gboolean is_thumbnail;

	if (path_represents_svg_image (filename)) {
		pixbuf = load_pixbuf_svg (filename,
					  nominal_size,
					  base_size,
					  icon_data);
	} else {
		is_thumbnail = strstr (filename, "/.thumbnails/")  != NULL;

		/* FIXME: Maybe we shouldn't have to load the file each time
		 * Not sure if that is important */
		if (is_thumbnail) {
			pixbuf = nautilus_thumbnail_load_framed_image (filename);
		} else {
			pixbuf = gdk_pixbuf_new_from_file (filename, NULL);
		}

		if (pixbuf == NULL) {
			return NULL;
		}
		
		if (base_size == 0) {
			if (is_thumbnail) {
				base_size = 128 * NAUTILUS_ICON_SIZE_STANDARD / NAUTILUS_ICON_SIZE_THUMBNAIL;
			} else {
				width = gdk_pixbuf_get_width (pixbuf); 
				height = gdk_pixbuf_get_height (pixbuf);
				size = MAX (width, height);
				if (size > NAUTILUS_ICON_SIZE_STANDARD + 5) {
					base_size = size;
				} else {
					/* Don't scale up small icons */
					base_size = NAUTILUS_ICON_SIZE_STANDARD;
				}
			}
		}
		
		if (base_size != nominal_size) {
			scale = (double)nominal_size/base_size;
			scaled_pixbuf = scale_icon (pixbuf, &scale);
			if (icon_data != NULL) {
				scale_icon_data (icon_data, scale, scale);
			}
			g_object_unref (pixbuf);
			pixbuf = scaled_pixbuf;
		}
	}

	return pixbuf;
}

static CacheIcon *
create_normal_cache_icon (const char *icon,
			  const char *modifier,
			  guint       nominal_size)
{
	NautilusIconFactory *factory;
	char *filename;
	char *name_with_modifier;
	const GnomeIconData *src_icon_data;
	GnomeIconData *icon_data;
	CacheIcon *cache_icon;
	GdkPixbuf *pixbuf;
	int base_size;
	struct stat statbuf;
	time_t mtime;
		
	factory = get_icon_factory ();

	icon_data = NULL;
	filename = NULL;

	mtime = 0;
	
	base_size = 0;
	if (icon[0] == '/') {
		/* FIXME: maybe we should add modifier to the filename
		 *        before the extension */
		if (stat (icon, &statbuf) == 0 &&
		    S_ISREG (statbuf.st_mode)) {
			filename = g_strdup (icon);
			mtime = statbuf.st_mtime;
		}
	} else {
		if (modifier) {
			name_with_modifier = g_strconcat (icon, "-", modifier, NULL);
		} else {
			name_with_modifier = (char *)icon;
		}

		filename = gnome_icon_theme_lookup_icon (factory->icon_theme,
							 name_with_modifier,
							 nominal_size,
							 &src_icon_data,
							 &base_size);
		if (name_with_modifier != icon) {
			g_free (name_with_modifier);
		}

		/* Make a copy of the icon data */
		icon_data = NULL;
		if (src_icon_data) {
			icon_data = gnome_icon_data_dup (src_icon_data);
		}
	}

	if (filename == NULL) {
		return NULL;
	}

	pixbuf = load_icon_file (filename,
				 base_size,
				 nominal_size,
				 icon_data);
	g_free (filename);
	if (pixbuf == NULL) {
		return NULL;
	}
	
	cache_icon = cache_icon_new (pixbuf, icon_data);
	cache_icon->mtime = mtime;
	
	return cache_icon;
}


/* Get the icon, handling the caching.
 * If @picky is true, then only an unscaled icon is acceptable.
 * Also, if @picky is true, the icon must be a custom icon if
 * @custom is true or a standard icon is @custom is false.
 */
static CacheIcon *
get_icon_from_cache (const char *icon,
		     const char *modifier,
		     guint       nominal_size)
{
	NautilusIconFactory *factory;
	GHashTable *hash_table;
	CacheKey lookup_key;
	CacheKey *key;
	CacheIcon *cached_icon;
	gpointer key_in_table, value;
	struct stat statbuf;
	
	g_return_val_if_fail (icon != NULL, NULL);

	key = NULL;
	cached_icon = NULL;
	
	factory = get_icon_factory ();
	hash_table = factory->icon_cache;

	/* Check to see if it's already in the table. */
	lookup_key.name = (char *)icon;
	lookup_key.modifier = (char *)modifier;
	lookup_key.nominal_size = nominal_size;

	if (g_hash_table_lookup_extended (hash_table, &lookup_key,
					  &key_in_table, &value)) {
		/* Found it in the table. */
		g_assert (key_in_table != NULL);
		g_assert (value != NULL);
		key = key_in_table;
		cached_icon = value;
	}

	/* Make sure that thumbnails and image-as-itself icons gets
	   reloaded when they change: */
	if (cached_icon && icon[0] == '/') {
		if (stat (icon, &statbuf) != 0 ||
		    !S_ISREG (statbuf.st_mode) ||
		    statbuf.st_mtime != cached_icon->mtime) {
			cached_icon = NULL;
		}
	}

	if (cached_icon == NULL) {
		/* Not in the table, so load the image. */
		
		/*
		g_print ("cache miss for %s:%s:%s:%d\n",
			 icon, modifier?modifier:"", embedded_text?"<tl>":"", nominal_size);
		*/
		
		cached_icon = create_normal_cache_icon (icon,
							modifier,
							nominal_size);
		/* Try to fallback without modifier */
		if (cached_icon == NULL && modifier != NULL) {
			cached_icon = create_normal_cache_icon (icon,
								NULL,
								nominal_size);
		}
		
		if (cached_icon == NULL) {
			cached_icon = factory->fallback_icon;
			cache_icon_ref (cached_icon);
		}
		
		/* Create the key and icon for the hash table. */
		key = g_new (CacheKey, 1);
		key->name = g_strdup (icon);
		key->modifier = g_strdup (modifier);
		key->nominal_size = nominal_size;

		g_hash_table_insert (hash_table, key, cached_icon);
	}

	/* Hand back a ref to the caller. */
	cache_icon_ref (cached_icon);

	/* Since this item was used, keep it in the cache longer. */
	mark_recently_used (&cached_icon->recently_used_node);

	/* Come back later and sweep the cache. */
	nautilus_icon_factory_schedule_sweep (factory);
	
        return cached_icon;
}

GdkPixbuf *
nautilus_icon_factory_get_pixbuf_for_icon (const char                  *icon,
					   const char                  *modifier,
					   guint                        nominal_size,
					   NautilusEmblemAttachPoints  *attach_points,
					   GdkRectangle                *embedded_text_rect,
					   gboolean                     wants_default,
					   char                       **display_name)
{
	NautilusIconFactory *factory;
	CacheIcon *cached_icon;
	GnomeIconData *icon_data;
	GdkPixbuf *pixbuf;
	int i;
	
	factory = get_icon_factory ();
	cached_icon = get_icon_from_cache (icon,
					   modifier,
					   nominal_size);

	if (attach_points != NULL) {
		if (cached_icon->icon_data != NULL) {
			icon_data = cached_icon->icon_data;
			attach_points->num_points = MIN (icon_data->n_attach_points,
							 MAX_ATTACH_POINTS);
			for (i = 0; i < attach_points->num_points; i++) {
				attach_points->points[i].x = icon_data->attach_points[i].x;
				attach_points->points[i].y = icon_data->attach_points[i].y;
			}
		} else {
			attach_points->num_points = 0;
		}
	}
	if (embedded_text_rect) {
		if (cached_icon->icon_data != NULL &&
		    cached_icon->icon_data->has_embedded_rect) {
			embedded_text_rect->x = cached_icon->icon_data->x0;
			embedded_text_rect->y = cached_icon->icon_data->y0;
			embedded_text_rect->width = cached_icon->icon_data->x1 - cached_icon->icon_data->x0;
			embedded_text_rect->height = cached_icon->icon_data->y1 - cached_icon->icon_data->y0;
		} else {
			embedded_text_rect->x = 0;
			embedded_text_rect->y = 0;
			embedded_text_rect->width = 0;
			embedded_text_rect->height = 0;
		}
	}

	if (display_name != NULL) {
		if (cached_icon->icon_data != NULL &&
		    cached_icon->icon_data->display_name != NULL) {
			*display_name = g_strdup (cached_icon->icon_data->display_name);
		} else {
			*display_name = NULL;
		}
	}
	
	/* if we don't want a default icon and one is returned, return NULL instead */
	if (!wants_default && cached_icon == factory->fallback_icon) {
		cache_icon_unref (cached_icon);
		return NULL;
	}
	
	pixbuf = cached_icon->pixbuf;
	g_object_ref (pixbuf);
	cache_icon_unref (cached_icon);

	return pixbuf;
}

static guint
cache_key_hash (gconstpointer p)
{
	const CacheKey *key;
	guint hash;

	key = p;

	hash =  g_str_hash (key->name) ^
		(key->nominal_size << 4);
	
	if (key->modifier) {
		hash ^= g_str_hash (key->modifier);
	}
		
	return hash;
}

static gboolean
cache_key_equal (gconstpointer a, gconstpointer b)
{
	const CacheKey *key_a, *key_b;

	key_a = a;
	key_b = b;

	return eel_strcmp (key_a->name, key_b->name) == 0 &&
		key_a->nominal_size ==  key_b->nominal_size &&
		eel_strcmp (key_a->modifier, key_b->modifier) == 0;
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
	}
	g_return_val_if_fail (FALSE, NAUTILUS_ICON_SIZE_STANDARD);
}

/* Convenience cover for nautilus_icon_factory_get_icon_for_file
 * and nautilus_icon_factory_get_pixbuf_for_icon.
 */
GdkPixbuf *
nautilus_icon_factory_get_pixbuf_for_file (NautilusFile *file,
					   const char *modifier,
					   guint size_in_pixels)
{
	char *icon;
	GdkPixbuf *pixbuf;


	/* Get the pixbuf for this file. */
	icon = nautilus_icon_factory_get_icon_for_file (file, FALSE);
	if (icon == NULL) {
		return NULL;
	}

	pixbuf = nautilus_icon_factory_get_pixbuf_for_icon (icon, modifier,
							    size_in_pixels,
							    NULL, NULL,
							    TRUE, NULL);
	
	g_free (icon);

	return pixbuf;
}

/* Convenience routine for getting a pixbuf from an icon name. */
GdkPixbuf *
nautilus_icon_factory_get_pixbuf_from_name (const char *icon_name,
					    const char *modifier,
					    guint size_in_pixels,
					    char **display_name)
{
	return nautilus_icon_factory_get_pixbuf_for_icon (icon_name, modifier,
							  size_in_pixels,
							  NULL, NULL,
							  TRUE, display_name);
}
									  
GdkPixbuf *
nautilus_icon_factory_get_thumbnail_frame (void)
{
	return get_icon_factory ()->thumbnail_frame;
}

gboolean
nautilus_icon_factory_remove_from_cache (const char *icon_name,
					 const char *modifier,
					 guint size)
{
	GHashTable *hash_table;
	NautilusIconFactory *factory;
	CacheKey lookup_key;

	factory = get_icon_factory ();
	hash_table = factory->icon_cache;

	/* Check to see if it's already in the table. */
	lookup_key.name = (char *)icon_name;
	lookup_key.modifier = (char *)modifier;
	lookup_key.nominal_size = size;
	
	return g_hash_table_remove (hash_table, &lookup_key);
}
					 
#if ! defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_icon_factory (void)
{
	EEL_CHECK_INTEGER_RESULT (nautilus_get_icon_size_for_zoom_level (0), 12);
	EEL_CHECK_INTEGER_RESULT (nautilus_get_icon_size_for_zoom_level (1), 24);
	EEL_CHECK_INTEGER_RESULT (nautilus_get_icon_size_for_zoom_level (2), 36);
	EEL_CHECK_INTEGER_RESULT (nautilus_get_icon_size_for_zoom_level (3), 48);
	EEL_CHECK_INTEGER_RESULT (nautilus_get_icon_size_for_zoom_level (4), 72);
	EEL_CHECK_INTEGER_RESULT (nautilus_get_icon_size_for_zoom_level (5), 96);
	EEL_CHECK_INTEGER_RESULT (nautilus_get_icon_size_for_zoom_level (6), 192);

	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (0), 12);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (1), 12);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (11), 12);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (12), 20);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (19), 20);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (20), 24);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (23), 24);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (24), 36);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (35), 36);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (36), 48);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (47), 48);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (48), 72);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (71), 72);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (72), 96);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (95), 96);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (96), 192);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (191), 192);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (192), 192);
	EEL_CHECK_INTEGER_RESULT (get_larger_icon_size (0xFFFFFFFF), 192);

	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (0), 12);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (1), 12);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (11), 12);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (12), 12);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (20), 12);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (21), 20);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (24), 20);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (25), 24);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (36), 24);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (37), 36);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (48), 36);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (49), 48);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (72), 48);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (73), 72);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (96), 72);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (97), 96);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (192), 96);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (193), 192);
	EEL_CHECK_INTEGER_RESULT (get_smaller_icon_size (0xFFFFFFFF), 192);
}

#endif /* ! NAUTILUS_OMIT_SELF_CHECK */
