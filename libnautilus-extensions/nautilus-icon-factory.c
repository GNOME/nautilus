/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-icon-factory.c: Class for obtaining icons for files and other objects.
 
   Copyright (C) 1999, 2000 Red Hat Inc.
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
  
   Authors: John Sullivan <sullivan@eazel.com>,
            Darin Adler <darin@eazel.com>,
	    Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include "nautilus-icon-factory.h"

#include "nautilus-default-file-icon.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file-utilities.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-global-preferences.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-icon-factory-private.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-link.h"
#include "nautilus-metadata.h"
#include "nautilus-scalable-font.h"
#include "nautilus-string.h"
#include "nautilus-theme.h"
#include "nautilus-thumbnails.h"
#include "nautilus-xml-extensions.h"
#include <gnome.h>
#include <gtk/gtksignal.h>
#include <libgnomevfs/gnome-vfs-file-info.h>
#include <libgnomevfs/gnome-vfs-mime-info.h>
#include <libgnomevfs/gnome-vfs-types.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <librsvg/rsvg.h>
#include <parser.h>
#include <stdio.h>
#include <string.h>
#include <xmlmemory.h>

/* List of suffixes to search when looking for an icon file. */
static const char *icon_file_name_suffixes[] =
{
	".svg",
	".SVG",
	"",
	".png",
	".PNG",
	".gif",
	".GIF",
	".xpm",
	".XPM"
};

#define ICON_NAME_DIRECTORY             "i-directory"
#define ICON_NAME_DIRECTORY_CLOSED      "i-dirclosed"
#define ICON_NAME_EXECUTABLE            "i-executable"
#define ICON_NAME_REGULAR               "i-regular"
#define ICON_NAME_SEARCH_RESULTS        "i-search"
#define ICON_NAME_WEB			"i-web"
#define ICON_NAME_SOCKET                "i-sock"
#define ICON_NAME_FIFO                  "i-fifo"
#define ICON_NAME_CHARACTER_DEVICE      "i-chardev"
#define ICON_NAME_BLOCK_DEVICE          "i-blockdev"
#define ICON_NAME_BROKEN_SYMBOLIC_LINK  "i-symlink"

#define ICON_NAME_THUMBNAIL_LOADING     "loading"

#define EMBLEM_NAME_PREFIX              "emblem-"

#define DEFAULT_ICON_THEME		"default"

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

/* extremely large images can eat up hundreds of megabytes of memory, so we
 * shouldn't automatically thumbnail when larges are too large.  Eventually,
 * we want this threshold to be user-settable, but for now it's hard-wired.
 */
 #define INHIBIT_THUMBNAIL_SIZE_THRESHOLD 1048576
 
/* This circular doubly-linked list structure is used to keep a list
 * of the most recently used items in the cache.
 */
typedef struct NautilusCircularList NautilusCircularList;
struct NautilusCircularList {
	NautilusCircularList *next;
	NautilusCircularList *prev;
};

/* maximum size for either dimension at the standard zoom level */
#define MAXIMUM_ICON_SIZE 96

/* FIXME bugzilla.eazel.com 1102: Embedded text should use preferences
 * to determine what font it uses.
 */
#define EMBEDDED_TEXT_FONT_FAMILY       _("helvetica")
#define EMBEDDED_TEXT_FONT_WEIGHT       _("medium")
#define EMBEDDED_TEXT_FONT_SLANT        NULL
#define EMBEDDED_TEXT_FONT_SET_WIDTH    NULL
#define EMBEDDED_TEXT_FONT_SIZE         9
#define EMBEDDED_TEXT_LINE_OFFSET       1
#define EMBEDDED_TEXT_EMPTY_LINE_HEIGHT 4

/* The icon factory.
 * These are just globals, but they're in an object so we can
 * connect signals and have multiple icon factories some day
 * if we want to.
 */
typedef struct {
	GtkObject object;

	/* name of current theme */
	char *theme_name;
	
	/* name of default theme, so it can be delegated */
	char *default_theme_name;
	
	/* the local_theme boolean is set if the theme was user-added (lives in ~/.nautilus) */
	gboolean local_theme;
	gboolean local_default_theme;
	
	/* A hash table so we pass out the same scalable icon pointer
	 * every time someone asks for the same icon. Scalable icons
	 * are removed from this hash table when they are destroyed.
	 */
	GHashTable *scalable_icons;

	/* A hash table so we can find a cached icon's data structure
	 * from the pixbuf.
	 */
	GHashTable *cache_icons;

	/* A hash table that contains the icons. A circular list of
	 * the most recently used icons is kept around, and we don't
	 * let them go when we sweep the cache.
	 */
	GHashTable *icon_cache;
	NautilusCircularList recently_used_dummy_head;
	guint recently_used_count;
        guint sweep_timer;
} NautilusIconFactory;

#define NAUTILUS_ICON_FACTORY(obj) \
	GTK_CHECK_CAST (obj, nautilus_icon_factory_get_type (), NautilusIconFactory)

typedef struct {
	GtkObjectClass parent_class;
} NautilusIconFactoryClass;

enum {
	ICONS_CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

/* A scalable icon, which is basically the name and path of an icon,
 * before we load the actual pixels of the icons's pixbuf.
 */
struct NautilusScalableIcon {
	guint ref_count;

	char *uri;
	char *name;
	char *modifier;
	char *embedded_text;
	gboolean aa_mode;
};

/* A request for an icon of a particular size. */
typedef struct {
	guint nominal_width;
	guint nominal_height;
	guint maximum_width;
	guint maximum_height;
} IconSizeRequest;

typedef struct {
	ArtIRect text_rect;
	NautilusEmblemAttachPoints attach_points;
} IconDetails;

/* The key to a hash table that holds the scaled icons as pixbufs. */
typedef struct {
	NautilusScalableIcon *scalable_icon;
	IconSizeRequest size;
} CacheKey;

/* The value in the same table. */
typedef struct {
	GdkPixbuf *pixbuf;
	IconDetails details;

	/* If true, outside clients have refs to the pixbuf. */
	gboolean outstanding;

	/* Number of internal clients with refs to the pixbuf. */
	guint internal_ref_count;

	/* Used to decide when to kick icons out of the cache. */
	NautilusCircularList recently_used_node;

	/* Used to know when to make a new thumbnail. */
	time_t cache_time;

	/* Type of icon. */
	gboolean custom;
	gboolean scaled;
} CacheIcon;

#define MINIMUM_EMBEDDED_TEXT_RECT_WIDTH	20.0
#define MINIMUM_EMBEDDED_TEXT_RECT_HEIGHT	20.0

static CacheIcon *fallback_icon;

/* forward declarations */

static guint      nautilus_icon_factory_get_type         (void);
static void       nautilus_icon_factory_initialize_class (NautilusIconFactoryClass *class);
static void       nautilus_icon_factory_initialize       (NautilusIconFactory      *factory);
static void       nautilus_icon_factory_destroy          (GtkObject                *object);
static void       icon_theme_changed_callback            (gpointer                  user_data);
static guint      nautilus_scalable_icon_hash            (gconstpointer             p);
static gboolean   nautilus_scalable_icon_equal           (gconstpointer             a,
							  gconstpointer             b);
static guint      cache_key_hash                         (gconstpointer             p);
static gboolean   cache_key_equal                        (gconstpointer             a,
							  gconstpointer             b);
static CacheIcon *get_icon_from_cache                    (NautilusScalableIcon     *scalable_icon,
							  const IconSizeRequest    *size,
							  gboolean                  picky,
							  gboolean                  custom);
static CacheIcon *load_icon_with_embedded_text           (NautilusScalableIcon     *scalable_icon,
							  const IconSizeRequest    *size);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusIconFactory,
				   nautilus_icon_factory,
				   GTK_TYPE_OBJECT)

static NautilusIconFactory *global_icon_factory = NULL;

static void
destroy_icon_factory (void)
{
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
					      icon_theme_changed_callback,
					      NULL);
	gtk_object_unref (GTK_OBJECT (global_icon_factory));
}

/* Return a pointer to the single global icon factory. */
static NautilusIconFactory *
get_icon_factory (void)
{
        if (global_icon_factory == NULL) {
		global_icon_factory = NAUTILUS_ICON_FACTORY
			(gtk_object_new (nautilus_icon_factory_get_type (), NULL));
		gtk_object_ref (GTK_OBJECT (global_icon_factory));
		gtk_object_sink (GTK_OBJECT (global_icon_factory));

		/* Update to match the theme. */
		icon_theme_changed_callback (NULL);
		nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_THEME,
						   icon_theme_changed_callback,
						   NULL);

		g_atexit (destroy_icon_factory);
        }
        return global_icon_factory;
}

GtkObject *
nautilus_icon_factory_get (void)
{
	return GTK_OBJECT (get_icon_factory ());
}

static void
check_recently_used_list (void)
{
	NautilusIconFactory *factory;
	NautilusCircularList *head, *node, *next;
	int count;

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
}

static void
nautilus_icon_factory_initialize (NautilusIconFactory *factory)
{
	factory->scalable_icons = g_hash_table_new (nautilus_scalable_icon_hash,
						    nautilus_scalable_icon_equal);
	factory->cache_icons = g_hash_table_new (g_direct_hash,
						 g_direct_equal);
        factory->icon_cache = g_hash_table_new (cache_key_hash,
						cache_key_equal);
	
	/* Empty out the recently-used list. */
	factory->recently_used_dummy_head.next = &factory->recently_used_dummy_head;
	factory->recently_used_dummy_head.prev = &factory->recently_used_dummy_head;
}

static void
nautilus_icon_factory_initialize_class (NautilusIconFactoryClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);

	signals[ICONS_CHANGED]
		= gtk_signal_new ("icons_changed",
				  GTK_RUN_LAST,
				  object_class->type,
				  0,
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	object_class->destroy = nautilus_icon_factory_destroy;
}

static void
cache_key_destroy (CacheKey *key)
{
	nautilus_scalable_icon_unref (key->scalable_icon);
	g_free (key);
}

static void
mark_icon_not_outstanding (GdkPixbuf *pixbuf, gpointer callback_data)
{
	NautilusIconFactory *factory;
	CacheIcon *icon;

	g_assert (callback_data == NULL);

	factory = get_icon_factory ();

	icon = g_hash_table_lookup (factory->cache_icons, pixbuf);
        g_return_if_fail (icon != NULL);
	g_return_if_fail (icon->pixbuf == pixbuf);
	g_return_if_fail (icon->outstanding);

	icon->outstanding = FALSE;
}
 
static CacheIcon *
cache_icon_new (GdkPixbuf *pixbuf,
		gboolean custom,
		gboolean scaled,
		const IconDetails *details)
{
	NautilusIconFactory *factory;
	CacheIcon *icon;

	factory = get_icon_factory ();

	/* Just a check to see this is not reusing a pixbuf. */
	g_assert (g_hash_table_lookup (factory->cache_icons, pixbuf) == NULL);

	/* Grab the pixbuf since we are keeping it. */
	gdk_pixbuf_ref (pixbuf);
	gdk_pixbuf_set_last_unref_handler
		(pixbuf, mark_icon_not_outstanding, NULL);

	/* Make the icon. */
	icon = g_new0 (CacheIcon, 1);
	icon->pixbuf = pixbuf;
	icon->internal_ref_count = 1;
	icon->custom = custom;
	icon->scaled = scaled;
	if (details != NULL) {
		icon->details = *details;
	}

	/* Put it into the hash table. */
	g_hash_table_insert (factory->cache_icons, pixbuf, icon);
	return icon;
}

static void
cache_icon_ref (CacheIcon *icon)
{
	NautilusIconFactory *factory;

	factory = get_icon_factory ();

	g_assert (icon != NULL);
	g_assert (icon->internal_ref_count >= 1
		  || (icon->internal_ref_count == 0 && icon == fallback_icon));
	g_assert (g_hash_table_lookup (factory->cache_icons, icon->pixbuf) == icon);

	icon->internal_ref_count++;
}

static void
cache_icon_unref (CacheIcon *icon)
{
	NautilusIconFactory *factory;
	NautilusCircularList *node;

	factory = get_icon_factory ();

	g_assert (icon != NULL);
	g_assert (icon->internal_ref_count >= 1);
	g_assert (g_hash_table_lookup (factory->cache_icons, icon->pixbuf) == icon);

	if (icon->internal_ref_count > 1) {
		icon->internal_ref_count--;
		return;
	}
	icon->internal_ref_count = 0;

	check_recently_used_list ();

	/* If it's in the recently used list, free it from there */      
	node = &icon->recently_used_node;
	if (node->next != NULL) {
		g_assert (factory->recently_used_count >= 1);
		
		g_assert (node->next->prev == node);
		g_assert (node->prev->next == node);
		g_assert (node->next != node);
		g_assert (node->prev != node);

		node->next->prev = node->prev;
		node->prev->next = node->next;

		node->next = NULL;
		node->prev = NULL;

		factory->recently_used_count -= 1;
	}

	check_recently_used_list ();
	
	/* The fallback icon has life after death. */
	if (icon == fallback_icon) {
		return;
	}

	/* Remove from the cache icons table. */
	g_hash_table_remove (factory->cache_icons, icon->pixbuf);

	/* Since it's no longer in the cache, we don't need to notice the last unref. */
	gdk_pixbuf_set_last_unref_handler (icon->pixbuf, NULL, NULL);

	/* Let go of the pixbuf if we were holding a reference to it.
	 * If it was still outstanding, we didn't have a reference to it,
	 * and we were counting on the unref handler to catch it.
	 */
	if (!icon->outstanding) {
		gdk_pixbuf_unref (icon->pixbuf);
	}

	g_free (icon);
}

/* Destroy one pixbuf in the cache. */
static gboolean
nautilus_icon_factory_destroy_cached_icon (gpointer key, gpointer value, gpointer user_data)
{
	cache_key_destroy (key);
	cache_icon_unref (value);

	/* Tell the caller to remove the hash table entry. */
        return TRUE;
}

/* Reset the cache to the default state. */
static void
nautilus_icon_factory_clear (void)
{
	NautilusIconFactory *factory;
	NautilusCircularList *head;

	factory = get_icon_factory ();

        g_hash_table_foreach_remove (factory->icon_cache,
                                     nautilus_icon_factory_destroy_cached_icon,
                                     NULL);

	/* Empty out the recently-used list. */
	head = &factory->recently_used_dummy_head;
	g_assert (factory->recently_used_count == 0);
	g_assert (head->next == head);
	g_assert (head->prev == head);
}

static void
nautilus_icon_factory_destroy (GtkObject *object)
{
	NautilusIconFactory *factory;

	factory = NAUTILUS_ICON_FACTORY (object);

        nautilus_icon_factory_clear ();

	if (g_hash_table_size (factory->scalable_icons) != 0) {
		g_warning ("%d scalable icons still left when destroying icon factory",
			   g_hash_table_size (factory->scalable_icons));
	}
	if (g_hash_table_size (factory->icon_cache) != 0) {
		g_warning ("%d icon cache entries still left when destroying icon factory",
			   g_hash_table_size (factory->icon_cache));
	}

        g_hash_table_destroy (factory->scalable_icons);
        g_hash_table_destroy (factory->icon_cache);

        g_free (factory->theme_name);
        g_free (factory->default_theme_name);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
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
	if (icon->outstanding) {
		return FALSE;
	}

	/* Free the item. */
        return nautilus_icon_factory_destroy_cached_icon (key, value, NULL);
}

/* Remove icons whose URI field matches the passed-in URI. */
static gboolean
nautilus_icon_factory_remove_if_uri_matches (gpointer key,
					     gpointer value,
					     gpointer user_data)
{
        char *image_uri;
        CacheKey *cache_key;
	CacheIcon *icon;
	
	cache_key = key;
	icon = value;
        image_uri = user_data;

	/* See if the the uri's match - if not, just return. */
	if (cache_key->scalable_icon->uri != NULL
	    && strcmp (cache_key->scalable_icon->uri, image_uri)) {
		return FALSE;
	}
	
	/* Free the item. */
        return nautilus_icon_factory_destroy_cached_icon (key, value, NULL);
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
nautilus_icon_factory_schedule_sweep (void)
{
	NautilusIconFactory *factory;

	factory = get_icon_factory ();

        if (factory->sweep_timer != 0) {
                return;
	}

        factory->sweep_timer = g_timeout_add (ICON_CACHE_SWEEP_TIMEOUT,
					      nautilus_icon_factory_sweep,
					      factory);
}

/* Clear a specific icon from the cache. */
void
nautilus_icon_factory_remove_by_uri (const char *image_uri)
{
	NautilusIconFactory *factory;

	/* build the key and look it up in the icon cache */

	factory = get_icon_factory ();
	g_hash_table_foreach_remove (factory->icon_cache,
				     nautilus_icon_factory_remove_if_uri_matches,
				     (gpointer) image_uri);
}

/* utility to check if a theme is local or not */
static void
check_local_theme (const char *theme_name, gboolean *result_ptr)
{
	char *user_directory, *themes_directory, *this_theme_directory;
	
	if (theme_name == NULL) {
		*result_ptr = FALSE;
		return;
	}
	
	user_directory = nautilus_get_user_directory ();
	themes_directory = nautilus_make_path (user_directory, "themes");
	this_theme_directory = nautilus_make_path (themes_directory, theme_name);
	
	*result_ptr = g_file_exists (this_theme_directory);
	
	g_free (user_directory);
	g_free (themes_directory);
	g_free (this_theme_directory);
}

/* Change the theme. */
static void
set_theme (const char *theme_name)
{
	NautilusIconFactory *factory;

	factory = get_icon_factory ();

	if (nautilus_strcmp (theme_name, factory->theme_name) == 0) {
		return;
	}

        nautilus_icon_factory_clear ();

        g_free (factory->theme_name);
        factory->theme_name = g_strdup (theme_name);
	check_local_theme (theme_name, &factory->local_theme);

	/* now set up the default theme */
        g_free (factory->default_theme_name);	
	factory->default_theme_name = nautilus_theme_get_theme_data ("icon-images", "DEFAULT_THEME");
	check_local_theme (factory->default_theme_name, &factory->local_default_theme);
			
	/* we changed the theme, so emit the icons_changed signal */
	gtk_signal_emit (GTK_OBJECT (factory),
			 signals[ICONS_CHANGED]);
}


/* Use the MIME type to get the icon name. */
static const char *
nautilus_icon_factory_get_icon_name_for_regular_file (NautilusFile *file)
{
	char *mime_type, *uri;
	const char *icon_name;
	gboolean is_text_file, use_web_icon;
	
	/* force plain text files to use the generic document icon so we can have the text-in-icons feature;
		eventually, we want to force other types of text files as well */
        
	mime_type = nautilus_file_get_mime_type (file);
	is_text_file = mime_type != NULL && !nautilus_strcasecmp (mime_type, "text/plain");
	
	if (mime_type != NULL && !is_text_file) {
		icon_name = gnome_vfs_mime_get_value (mime_type, "icon-filename");
		if (icon_name != NULL) {
			g_free (mime_type);
			return icon_name;
		}
	}

	/* gnome_vfs_mime didn't give us an icon name, so we have to fall back on default icons. */
	if (nautilus_file_is_executable (file) & !is_text_file) {
		g_free (mime_type);		
		return ICON_NAME_EXECUTABLE;
	}

	/* if it's an http uri and and html document, use a generic web icon instead of the generic icon  */
	uri = nautilus_file_get_uri (file);
        use_web_icon = nautilus_istr_has_prefix (uri, "http:") && !nautilus_strcmp (mime_type, "text/html");
        g_free (uri);
        g_free (mime_type);	
	
	return use_web_icon ? ICON_NAME_WEB : ICON_NAME_REGULAR;
}

/* Use the MIME type to get the icon name. */
static const char *
nautilus_icon_factory_get_icon_name_for_directory (NautilusFile *file)
{
	char *mime_type;
	
	mime_type = nautilus_file_get_mime_type (file);
	
	if (mime_type != NULL && !nautilus_strcasecmp (mime_type, "x-directory/search")) {
		g_free (mime_type);
		return ICON_NAME_SEARCH_RESULTS;
	}
	else {
		g_free (mime_type);
		return ICON_NAME_DIRECTORY;
	}
}


/* Get the icon name for a file. */
static const char *
nautilus_icon_factory_get_icon_name_for_file (NautilusFile *file)
{	
	/* Get an icon name based on the file's type. */
        switch (nautilus_file_get_file_type (file)) {
        case GNOME_VFS_FILE_TYPE_DIRECTORY:
		return nautilus_icon_factory_get_icon_name_for_directory (file);
        case GNOME_VFS_FILE_TYPE_FIFO:
                return ICON_NAME_FIFO;
        case GNOME_VFS_FILE_TYPE_SOCKET:
		return ICON_NAME_SOCKET;
        case GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE:
		return ICON_NAME_CHARACTER_DEVICE;
        case GNOME_VFS_FILE_TYPE_BLOCK_DEVICE:
		return ICON_NAME_BLOCK_DEVICE;
        case GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK:
        	/* Non-broken symbolic links return the target's type. */
                return ICON_NAME_BROKEN_SYMBOLIC_LINK;
        case GNOME_VFS_FILE_TYPE_REGULAR:
        case GNOME_VFS_FILE_TYPE_UNKNOWN:
        default:
                return nautilus_icon_factory_get_icon_name_for_regular_file (file);
        }
}

static char *
make_full_icon_path (const char *path, const char *suffix, gboolean local_theme)
{
	char *partial_path, *full_path;
	char *user_directory, *themes_directory;
	
	if (path[0] == '/') {
		return g_strconcat (path, suffix, NULL);
	}

	/* Build a path for this icon, depending on the local_theme boolean. */
	partial_path = g_strconcat (path, suffix, NULL);
	if (local_theme) {
		user_directory = nautilus_get_user_directory ();
		themes_directory = nautilus_make_path (user_directory, "themes");
		full_path = nautilus_make_path (themes_directory, partial_path);
		g_free (user_directory);
		g_free (themes_directory);
	} else {
		full_path = nautilus_pixmap_file (partial_path);
	}
	
	g_free (partial_path);
	return full_path;
}

/* utility routine to parse the attach points string to set up the array in icon_info */
static void
parse_attach_points (NautilusEmblemAttachPoints *attach_points, const char *attach_point_string)
{
	char **point_array;
	int i, x_offset, y_offset;

	attach_points->num_points = 0;
	if (attach_point_string == NULL) {
		return;
	}
			
	/* Split the attach point string into a string array, then process
	 * each point with sscanf in a loop.
	 */
	point_array = g_strsplit (attach_point_string, "|", MAX_ATTACH_POINTS); 
	
	for (i = 0; point_array[i] != NULL; i++) {
		if (sscanf (point_array[i], " %d , %d , %*s", &x_offset, &y_offset) == 2) {
			attach_points->points[attach_points->num_points].x = x_offset;
			attach_points->points[attach_points->num_points].y = y_offset;
			attach_points->num_points++;
		} else {
			g_warning ("bad attach point specification: %s", point_array[i]);
		}
	}

	g_strfreev (point_array);
}

/* Pick a particular icon to use, trying all the various suffixes.
 * Return the path of the icon or NULL if no icon is found.
 */
static char *
get_themed_icon_file_path (const char *theme_name,
			   const char *icon_name,
			   guint icon_size,
			   gboolean aa_mode,
			   IconDetails *details)
{
	int i;
	gboolean include_size;
	char *themed_icon_name, *partial_path, *path, *aa_path, *xml_path;
	xmlDocPtr doc;
	xmlNodePtr node;
	char *size_as_string, *property;
	ArtIRect parsed_rect;
	NautilusIconFactory *factory;
	char *user_directory;
	gboolean local_theme;
	
	g_assert (icon_name != NULL);

	if (theme_name == NULL || icon_name[0] == '/') {
		themed_icon_name = g_strdup (icon_name);
	} else {
		themed_icon_name = g_strconcat (theme_name, "/", icon_name, NULL);
	}

	include_size = icon_size != NAUTILUS_ICON_SIZE_STANDARD;
	factory = get_icon_factory ();
	local_theme = factory->local_theme && theme_name != NULL;
	
	/* Try each suffix. */
	for (i = 0; i < NAUTILUS_N_ELEMENTS (icon_file_name_suffixes); i++) {
		if (include_size && strcasecmp(icon_file_name_suffixes[i], ".svg")) {
			/* Build a path for this icon. */
			partial_path = g_strdup_printf ("%s-%u",
							themed_icon_name,
							icon_size);
		} else {
			partial_path = g_strdup (themed_icon_name);
		}
		
		/* if we're in anti-aliased mode, try for an optimized one first */
		if (aa_mode) {
			aa_path = g_strconcat (partial_path, "-aa", NULL);
			path = make_full_icon_path (aa_path,
						    icon_file_name_suffixes[i], local_theme);
			g_free (aa_path);
		
			/* Return the path if the file exists. */
			if (path != NULL && g_file_exists (path)) {
				break;
			}
			
			g_free (path);
			path = NULL;
		}
						
		path = make_full_icon_path (partial_path,
					    icon_file_name_suffixes[i], local_theme);
		g_free (partial_path);

		/* Return the path if the file exists. */
		if (path != NULL && g_file_exists (path)) {
			break;
		}
		g_free (path);
		path = NULL;
	}

	/* Open the XML file to get the text rectangle and emblem attach points */
	if (path != NULL && details != NULL) {
		memset (&details->text_rect, 0, sizeof (details->text_rect));

		xml_path = make_full_icon_path (themed_icon_name, ".xml", local_theme);

		doc = xmlParseFile (xml_path);
		g_free (xml_path);
		
		size_as_string = g_strdup_printf ("%u", icon_size);
		node = nautilus_xml_get_root_child_by_name_and_property
			(doc, "ICON", "SIZE", size_as_string);
		g_free (size_as_string);
		
		property = xmlGetProp (node, "EMBEDDED_TEXT_RECTANGLE");		
		if (property != NULL) {
			
			if (sscanf (property,
				    " %d , %d , %d , %d %*s",
				    &parsed_rect.x0,
				    &parsed_rect.y0,
				    &parsed_rect.x1,
				    &parsed_rect.y1) == 4) {
				details->text_rect = parsed_rect;
			}
			xmlFree (property);
		}

		property = xmlGetProp (node, "ATTACH_POINTS");
		parse_attach_points (&details->attach_points, property);	
		xmlFree (property);
		
		xmlFreeDoc (doc);
	}

	/* If we still haven't found anything, and we're looking for an emblem,
	 * check out the user's home directory, since it might be an emblem
	 * that they've added there.
	 */
	if (path == NULL && nautilus_str_has_prefix (icon_name, "emblem-")) {
		for (i = 0; i < NAUTILUS_N_ELEMENTS (icon_file_name_suffixes); i++) {
			user_directory = nautilus_get_user_directory ();
			path = g_strdup_printf ("%s/emblems/%s%s", 
						user_directory,
						icon_name + 7, 
						icon_file_name_suffixes[i]);
			g_free (user_directory);
			
			if (g_file_exists (path)) {
				break;
			}
			
			g_free (path);
			path = NULL;
		}		
	}
	g_free (themed_icon_name);

	return path;
}

/* Choose the file name to load, taking into account theme
 * vs. non-theme icons. Also fill in info in the icon structure based
 * on what's found in the XML file.
 */
static char *
get_icon_file_path (const char *name,
		    const char *modifier,
		    guint size_in_pixels,
		    gboolean aa_mode,
		    IconDetails *details)
{
	NautilusIconFactory *icon_factory;
	const char *theme_to_use;
	char *path;
	char *name_with_modifier;

	if (name == NULL) {
		return NULL;
	}

	icon_factory = get_icon_factory ();
	theme_to_use = NULL;
 	
 	
	/* Check and see if there is a theme icon to use.
	 * If there's a default theme specified, try it, too.
	 * This decision must be based on whether there's a non-size-
	 * specific theme icon.
	 */
	if (icon_factory->theme_name != NULL) {
		path = get_themed_icon_file_path (icon_factory->theme_name,
						  name,
						  NAUTILUS_ICON_SIZE_STANDARD,
						  aa_mode,
						  details);		
		if (path != NULL) {
			theme_to_use = icon_factory->theme_name;
			g_free (path);
		} else if (icon_factory->default_theme_name != NULL) {
			path = get_themed_icon_file_path (icon_factory->default_theme_name,
						  name,
						  NAUTILUS_ICON_SIZE_STANDARD,
						  aa_mode,
						  details);		
			if (path != NULL) {
				theme_to_use = icon_factory->default_theme_name;
				g_free (path);
			}
		}
	}

	/* Now we know whether or not to use the theme. */

	/* If there's a modifier, try the modified icon first. */
	if (modifier && modifier[0] != '\0') {
		name_with_modifier = g_strconcat (name, "-", modifier, NULL);
		path = get_themed_icon_file_path (theme_to_use,
						  name_with_modifier,
						  size_in_pixels, 
						  aa_mode,
						  details);
		g_free (name_with_modifier);
		if (path != NULL) {
			return path;
		}
	}
	
	return get_themed_icon_file_path (theme_to_use,
					  name,
					  size_in_pixels,
					  aa_mode,
					  details);
}

static void
icon_theme_changed_callback (gpointer user_data)
{
	char *theme_preference, *icon_theme;

	/* Consult the user preference and the Nautilus theme. In the
	 * long run, we sould just get rid of the user preference.
	 */
	theme_preference = nautilus_preferences_get
		(NAUTILUS_PREFERENCES_THEME, DEFAULT_ICON_THEME);
	icon_theme = nautilus_theme_get_theme_data ("icons", "ICON_THEME");
	
	set_theme (icon_theme == NULL ? theme_preference : icon_theme);
	
	g_free (theme_preference);
	g_free (icon_theme);
}

/* Decompose a scalable icon into its text pieces. */
void
nautilus_scalable_icon_get_text_pieces (NautilusScalableIcon *icon,
				 	char **uri_return,
				 	char **name_return,
				 	char **modifier_return,
				 	char **embedded_text_return)
{
	g_return_if_fail (icon != NULL);

	if (uri_return != NULL) {
		*uri_return = g_strdup (icon->uri);
	}
	if (name_return != NULL) {
		*name_return = g_strdup (icon->name);
	}
	if (modifier_return != NULL) {
		*modifier_return = g_strdup (icon->modifier);
	}
	if (embedded_text_return != NULL) {
		*embedded_text_return = g_strdup (icon->embedded_text);
	}
}				 

/* Get or create a scalable icon from text pieces. */
NautilusScalableIcon *
nautilus_scalable_icon_new_from_text_pieces (const char *uri,
			    	      	     const char *name,
			    	      	     const char *modifier,
			    	      	     const char *embedded_text,
					     gboolean	anti_aliased)
{
	GHashTable *hash_table;
	NautilusScalableIcon cache_key, *icon;
	NautilusIconFactory *factory;
	
	factory = get_icon_factory ();
	/* Make empty strings canonical. */
	if (uri != NULL && uri[0] == '\0') {
		uri = NULL;
	}
	if (name != NULL && name[0] == '\0') {
		name = NULL;
	}
	if (modifier != NULL && modifier[0] == '\0') {
		modifier = NULL;
	}
	if (embedded_text != NULL && embedded_text[0] == '\0') {
		embedded_text = NULL;
	}

	/* Get at the hash table. */
	hash_table = get_icon_factory ()->scalable_icons;

	/* Check to see if it's already in the table. */
	cache_key.uri = (char *) uri;
	cache_key.name = (char *) name;
	cache_key.modifier = (char *) modifier;
	cache_key.embedded_text = (char *) embedded_text;
	cache_key.aa_mode = anti_aliased;
	
	icon = g_hash_table_lookup (hash_table, &cache_key);
	if (icon == NULL) {
		/* Not in the table, so create it and put it in. */
		icon = g_new0 (NautilusScalableIcon, 1);
		icon->uri = g_strdup (uri);
		icon->name = g_strdup (name);
		icon->modifier = g_strdup (modifier);
		icon->embedded_text = g_strdup (embedded_text);
		icon->aa_mode = anti_aliased;
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
	
	if (--icon->ref_count != 0) {
		return;
	}

	hash_table = get_icon_factory ()->scalable_icons;
	g_hash_table_remove (hash_table, icon);
	
	g_free (icon->uri);
	g_free (icon->name);
	g_free (icon->modifier);
	g_free (icon->embedded_text);
	g_free (icon);
}

static guint
nautilus_scalable_icon_hash (gconstpointer p)
{
	const NautilusScalableIcon *icon;
	guint hash;

	icon = p;
	hash = 0;

	if (icon->uri != NULL) {
		hash = g_str_hash (icon->uri);
	}

	hash <<= 4;
	if (icon->name != NULL) {
		hash ^= g_str_hash (icon->name);
	}

	hash <<= 4;
	if (icon->modifier != NULL) {
		hash ^= g_str_hash (icon->modifier);
	}

	hash <<= 4;
	if (icon->embedded_text != NULL) {
		hash ^= g_str_hash (icon->embedded_text);
	}

	if (icon->aa_mode) {
		hash ^= 1;
	}
	
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
		&& nautilus_strcmp (icon_a->modifier, icon_b->modifier) == 0
		&& nautilus_strcmp (icon_a->embedded_text, icon_b->embedded_text) == 0
		&& icon_a->aa_mode == icon_b->aa_mode;
}


static gboolean
should_display_image_file_as_itself (NautilusFile *file)
{
	NautilusSpeedTradeoffValue preference_value;
	
	preference_value = nautilus_preferences_get_enum
		(NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS, 
		 NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY);

	if (preference_value == NAUTILUS_SPEED_TRADEOFF_ALWAYS) {
		return TRUE;
	}
	
	if (preference_value == NAUTILUS_SPEED_TRADEOFF_NEVER) {
		return FALSE;
	}

	g_assert (preference_value == NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY);
	return nautilus_file_is_local (file);
}

/* key routine to get the scalable icon for a file */
NautilusScalableIcon *
nautilus_icon_factory_get_icon_for_file (NautilusFile *file, const char* modifier, gboolean anti_aliased)
{
	char *uri, *file_uri, *file_path, *image_uri, *icon_name, *mime_type, *top_left_text;
 	int file_size;
 	NautilusScalableIcon *scalable_icon;
	
	if (file == NULL) {
		return NULL;
	}

	/* if there is a custom image in the metadata, use that. */
	uri = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL);
	file_uri = nautilus_file_get_uri (file);
	
	/* if the file is an image, either use the image itself as the icon if it's small enough,
	   or use a thumbnail if one exists.  If it's too large, don't try to thumbnail it at all. 
	   If a thumbnail is required, but does not yet exist,  put an entry on the thumbnail queue so we
	   eventually make one */

	/* also, dont make thumbnails for images in the thumbnails directory */  
	if (uri == NULL) {		
		mime_type = nautilus_file_get_mime_type (file);
		file_size = nautilus_file_get_size (file);
		
		if (nautilus_istr_has_prefix (mime_type, "image/") && should_display_image_file_as_itself (file)) {
			if (file_size < SELF_THUMBNAIL_SIZE_THRESHOLD) {
				uri = nautilus_file_get_uri (file);				
			} else if (strstr (file_uri, "/.thumbnails/") == NULL && file_size < INHIBIT_THUMBNAIL_SIZE_THRESHOLD) {
				uri = nautilus_get_thumbnail_uri (file, anti_aliased);
				if (uri == NULL) {
					uri = get_icon_file_path
						(ICON_NAME_THUMBNAIL_LOADING, NULL,
						 NAUTILUS_ICON_SIZE_STANDARD, FALSE, NULL);
				}
			}
		}
		g_free (mime_type);		
	}
	
	/* Handle nautilus link xml files, which may specify their own image */	
	icon_name = NULL;
	if (nautilus_file_is_nautilus_link (file)) {
		/* FIXME bugzilla.eazel.com 2563: This does sync. I/O and only works for local paths. */
		file_path = gnome_vfs_get_local_path_from_uri (file_uri);
		if (file_path != NULL) {
			image_uri = nautilus_link_local_get_image_uri (file_path);
			if (image_uri != NULL) {
				/* FIXME bugzilla.eazel.com 2564: Lame hack. We only support file:// URIs? */
				if (nautilus_istr_has_prefix (image_uri, "file://")) {
					if (uri == NULL) {
						uri = image_uri;
					} else {
						g_free (image_uri);
					}
				} else {
					icon_name = image_uri;
				}
			}
			g_free (file_path);
		}
	}
			
	/* handle SVG files */
	if (uri == NULL && nautilus_file_is_mime_type (file, "image/svg")) {
		uri = g_strdup (file_uri);
	}
	
	/* Get the generic icon set for this file. */
        g_free (file_uri);
        if (icon_name == NULL) {
		icon_name = g_strdup (nautilus_icon_factory_get_icon_name_for_file (file));
	}

	top_left_text = nautilus_file_get_top_left_text (file);
	
	/* Create the icon or find it in the cache if it's already there. */
	scalable_icon = nautilus_scalable_icon_new_from_text_pieces 
		(uri, icon_name, modifier, top_left_text, anti_aliased);

	g_free (uri);
	g_free (icon_name);
	g_free (top_left_text);
	
	return scalable_icon;
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
	attributes = g_list_prepend (attributes,
				     NAUTILUS_FILE_ATTRIBUTE_TOP_LEFT_TEXT);

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

NautilusScalableIcon *
nautilus_icon_factory_get_emblem_icon_by_name (const char *emblem_name, gboolean anti_aliased)
{
	NautilusScalableIcon *scalable_icon;
	char *name_with_prefix;

	name_with_prefix = g_strconcat (EMBLEM_NAME_PREFIX, emblem_name, NULL);
	scalable_icon = nautilus_scalable_icon_new_from_text_pieces 
		(NULL, name_with_prefix, NULL, NULL, anti_aliased);
	g_free (name_with_prefix);	

	return scalable_icon;
}

GList *
nautilus_icon_factory_get_emblem_icons_for_file (NautilusFile *file, gboolean anti_aliased, NautilusStringList *exclude)
{
	GList *icons, *emblem_names, *p;
	NautilusScalableIcon *icon;

	icons = NULL;

	emblem_names = nautilus_file_get_emblem_names (file);
	for (p = emblem_names; p != NULL; p = p->next) {
		if (nautilus_string_list_contains (exclude, p->data)) {
			continue;
		}
		icon = nautilus_icon_factory_get_emblem_icon_by_name (p->data, anti_aliased);
		icons = g_list_prepend (icons, icon);
	}
	nautilus_g_list_free_deep (emblem_names);

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

/* This loads an SVG image, scaling it to the appropriate size. */
static GdkPixbuf *
load_pixbuf_svg (const char *path, guint size_in_pixels)
{
	FILE *f;
	GdkPixbuf *pixbuf;
	
	f = fopen (path, "rb");
	if (f == NULL) {
		return NULL;
	}
	pixbuf = rsvg_render_file (f, ((double) size_in_pixels) / NAUTILUS_ICON_SIZE_STANDARD);
	fclose (f);
	
	return pixbuf;
}

static gboolean
path_represents_svg_image (const char *path) 
{
	char *uri;
	GnomeVFSFileInfo file_info;
	gboolean is_svg;

	/* Sync. file I/O is OK here because this is used only for installed
	 * icons, not for the general case which could include icons on devices
	 * other than the local hard disk.
	 */

	uri = gnome_vfs_get_uri_from_local_path (path);
	gnome_vfs_file_info_init (&file_info);
	gnome_vfs_get_file_info (uri, &file_info, GNOME_VFS_FILE_INFO_GET_MIME_TYPE);
	g_free (uri);
	is_svg = nautilus_strcmp (file_info.mime_type, "image/svg") == 0;
	gnome_vfs_file_info_clear (&file_info);

	return is_svg;
}

/* This load function returns NULL if the icon is not available at
 * this size.
 */
static CacheIcon *
load_specific_icon (NautilusScalableIcon *scalable_icon,
		    guint size_in_pixels,
		    gboolean custom)
{
	IconDetails details;
	GdkPixbuf *pixbuf;
	char *path;
	CacheIcon *icon;

	memset (&details, 0, sizeof (details));
	pixbuf = NULL;

	/* Get the path. */
	if (custom) {
		/* We don't support custom icons that are not local here. */
		path = gnome_vfs_get_local_path_from_uri (scalable_icon->uri);
	} else {
		path = get_icon_file_path (scalable_icon->name,
					   scalable_icon->modifier,
					   size_in_pixels,
					   scalable_icon->aa_mode,
					   &details);					   		
	}

	/* Get the icon. */
	if (path != NULL) {
		if (path_represents_svg_image (path)) {
			pixbuf = load_pixbuf_svg (path, size_in_pixels);
		} else {
			/* Custom non-svg icons exist at one size.
			 * Non-custom icons have their size encoded in their path.
			 */
			if (!(custom && size_in_pixels != NAUTILUS_ICON_SIZE_STANDARD)) {
				pixbuf = gdk_pixbuf_new_from_file (path);
			}
		}
		
		g_free (path);
	}

	/* If we got nothing, we can free the icon. */
	if (pixbuf == NULL) {
		return NULL;
	}

	/* Since we got something, we can create a cache icon. */
	icon = cache_icon_new (pixbuf, custom, FALSE, &details);
	gdk_pixbuf_unref (pixbuf);
	return icon;
}

static void
destroy_fallback_icon (void)
{
	CacheIcon *icon;

	icon = fallback_icon;
	g_assert (icon->internal_ref_count == 0);
	cache_icon_ref (icon);
	fallback_icon = NULL;
	cache_icon_unref (icon);
}

/* This load function is not allowed to return NULL. */
static CacheIcon *
load_icon_for_scaling (NautilusScalableIcon *scalable_icon,
		       guint requested_size,
		       guint *actual_size_result)
{
	CacheIcon *icon;
	guint actual_size;
	IconSizeRequest size_request;
	GdkPixbuf *pixbuf;

	size_request.maximum_width = MAXIMUM_ICON_SIZE * requested_size / NAUTILUS_ZOOM_LEVEL_STANDARD;
	size_request.maximum_height = size_request.maximum_width;

	/* First check for a custom image. */
	actual_size = 0;
	while (get_next_icon_size_to_try (requested_size, &actual_size)) {
		size_request.nominal_width = actual_size;
		size_request.nominal_height = actual_size;

		icon = get_icon_from_cache
			(scalable_icon, &size_request, TRUE, TRUE);
		if (icon != NULL) {
			*actual_size_result = actual_size;
			return icon;
		}
	}
	
	/* Next, go for the normal image. */
	actual_size = 0;
	while (get_next_icon_size_to_try (requested_size, &actual_size)) {
		size_request.nominal_width = actual_size;
		size_request.nominal_height = actual_size;

		icon = get_icon_from_cache
			(scalable_icon, &size_request, TRUE, FALSE);
		if (icon != NULL) {
			*actual_size_result = actual_size;
			return icon;
		}
	}

	/* Finally, fall back on the hard-coded image. */
	if (fallback_icon != NULL) {
		cache_icon_ref (fallback_icon);
	} else {
		pixbuf = gdk_pixbuf_new_from_data
			(nautilus_default_file_icon,
			 GDK_COLORSPACE_RGB,
			 TRUE,
			 8,
			 nautilus_default_file_icon_width,
			 nautilus_default_file_icon_height,
			 nautilus_default_file_icon_width * 4, /* stride */
			 NULL, /* don't destroy data */
			 NULL);
		fallback_icon = cache_icon_new (pixbuf, FALSE, FALSE, NULL);
		g_atexit (destroy_fallback_icon);
	}

	*actual_size_result = NAUTILUS_ICON_SIZE_STANDARD;
        return fallback_icon;
}

/* Consumes the icon and returns a scaled one if the pixbuf is too big.
 * Note that this does an unref on the icon and returns a new one.
 */
static CacheIcon *
scale_icon (CacheIcon *icon,
	    double scale_x,
	    double scale_y)
{
	int width, height;
	int rect_width, rect_height;
	int i, num_points;
	GdkPixbuf *scaled_pixbuf;
	IconDetails scaled_details;
	CacheIcon *scaled_icon;

	g_assert (!icon->scaled);

	width = gdk_pixbuf_get_width (icon->pixbuf);
	height = gdk_pixbuf_get_height (icon->pixbuf);

	/* Check for no-scaling case. */
	if ((int) (width * scale_x) == width
	    && (int) (height * scale_y) == height) {
		return NULL;
	}

	width *= scale_x;
	if (width < 1) {
		width = 1;
	}
	height *= scale_y;
	if (height < 1) {
		height = 1;
	}

	scaled_pixbuf = gdk_pixbuf_scale_simple
		(icon->pixbuf, width, height, GDK_INTERP_BILINEAR);

	rect_width = (icon->details.text_rect.x1 - icon->details.text_rect.x0) * scale_x;
	rect_height = (icon->details.text_rect.y1 - icon->details.text_rect.y0) * scale_y;
	
	scaled_details.text_rect.x0 = icon->details.text_rect.x0 * scale_x;
	scaled_details.text_rect.y0 = icon->details.text_rect.x0 * scale_y;
	scaled_details.text_rect.x1 = scaled_details.text_rect.x0 + rect_width;
	scaled_details.text_rect.y1 = scaled_details.text_rect.y0 + rect_height;

	num_points = icon->details.attach_points.num_points;
	scaled_details.attach_points.num_points = num_points;
	for (i = 0; i < num_points; i++) {
		scaled_details.attach_points.points[i].x = icon->details.attach_points.points[i].x * scale_x;
		scaled_details.attach_points.points[i].y = icon->details.attach_points.points[i].y * scale_y;
	}
	
	scaled_icon = cache_icon_new (scaled_pixbuf,
				      icon->custom,
				      TRUE,
				      &scaled_details);
	gdk_pixbuf_unref (scaled_pixbuf);
	return scaled_icon;
}

static void
revise_scale_factors_if_too_big (GdkPixbuf *pixbuf,
				 const IconSizeRequest *size,
				 double *scale_x,
				 double *scale_y)
{
	int width, height;
	double y_distortion;

	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);

	if ((int) (width * *scale_x) <= size->maximum_width
	    && (int) (height * *scale_y) <= size->maximum_height) {
		return;
	}

	y_distortion = *scale_y / *scale_x;

	*scale_x = MIN ((double) size->maximum_width / width,
			(double) size->maximum_height / (height / y_distortion));
	*scale_y = *scale_x * y_distortion;
}

/* Returns a scaled icon if this one is too big. */
static CacheIcon *
scale_down_if_too_big (CacheIcon *icon,
		       const IconSizeRequest *size)
{
	double scale_x, scale_y;

	scale_x = 1.0;
	scale_y = 1.0;
	revise_scale_factors_if_too_big (icon->pixbuf, size, &scale_x, &scale_y);
	return scale_icon (icon, scale_x, scale_y);
}

/* This load function is not allowed to return NULL. */
static CacheIcon *
load_icon_scale_if_necessary (NautilusScalableIcon *scalable_icon,
			      const IconSizeRequest *size)
{
	CacheIcon *icon, *scaled_icon;
	guint nominal_actual_size;
	double scale_x, scale_y;
	
	/* Load the icon that's closest in size to what we want. */
	icon = load_icon_for_scaling (scalable_icon,
				      size->nominal_width,
				      &nominal_actual_size);
	
	/* Scale the pixbuf to the size we want. */
	scale_x = (double) size->nominal_width / nominal_actual_size;
	scale_y = (double) size->nominal_height / nominal_actual_size;
	revise_scale_factors_if_too_big (icon->pixbuf, size, &scale_x, &scale_y);
	scaled_icon = scale_icon (icon, scale_x, scale_y);
	if (scaled_icon == NULL) {
		return icon;
	}

	/* Mark this icon as scaled, too. */
	cache_icon_unref (icon);
	g_assert (scaled_icon->scaled);
	return scaled_icon;
}

/* Move this item to the head of the recently-used list,
 * bumping the last item off that list if necessary.
 */
static void
mark_recently_used (NautilusCircularList *node)
{
	NautilusIconFactory *factory;
	NautilusCircularList *head, *last_node;

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

	check_recently_used_list ();
}

/* Utility routine that checks if a cached icon has changed since it
 * was cached. It returns TRUE if the icon is still valid, and
 * removes it from the cache if it's not.
 */
static gboolean
cached_icon_still_valid (const char *file_uri, time_t cached_time)
{
	GnomeVFSURI *vfs_uri;
	GnomeVFSFileInfo file_info;
	GnomeVFSResult result;
	gboolean is_local, is_valid;

	/* If there's no specific file, simply return TRUE. */
	if (file_uri == NULL) {
		return TRUE;
	}
		
	/* FIXME bugzilla.eazel.com 2566: if the URI is remote, assume
	 * it's valid to avoid delay of testing. Eventually we'll have
	 * to make this async to fix this.
	 */
	vfs_uri = gnome_vfs_uri_new (file_uri);
	is_local = gnome_vfs_uri_is_local (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);	
	if (!is_local) {
		return TRUE;
	}
	
	/* Gather the info and then compare modification times. */
	gnome_vfs_file_info_init (&file_info);
	result = gnome_vfs_get_file_info (file_uri, &file_info, GNOME_VFS_FILE_INFO_DEFAULT);
	is_valid = result == GNOME_VFS_OK && file_info.mtime <= cached_time;
	gnome_vfs_file_info_clear (&file_info);

	/* if it's not valid, remove it from the cache */
	if (!is_valid) {
		nautilus_icon_factory_remove_by_uri (file_uri);	
	}
	
	return is_valid;
}

/* Get the icon, handling the caching.
 * If @picky is true, then only an unscaled icon is acceptable.
 * Also, if @picky is true, the icon must be a custom icon if
 * @custom is true or a standard icon is @custom is false.
 */
static CacheIcon *
get_icon_from_cache (NautilusScalableIcon *scalable_icon,
		     const IconSizeRequest *size,
		     gboolean picky,
		     gboolean custom)
{
	NautilusIconFactory *factory;
	GHashTable *hash_table;
	CacheKey lookup_key, *key;
	CacheIcon *icon, *scaled_icon;
	gpointer key_in_table, value;
	
	g_return_val_if_fail (scalable_icon != NULL, NULL);

	key = NULL;
	icon = NULL;
	
	factory = get_icon_factory ();
	hash_table = factory->icon_cache;

	/* Check to see if it's already in the table. */
	lookup_key.scalable_icon = scalable_icon;
	lookup_key.size = *size;

	if (g_hash_table_lookup_extended (hash_table, &lookup_key,
					  &key_in_table, &value)) {
		/* Found it in the table. */
		g_assert (key_in_table != NULL);
		g_assert (value != NULL);
		key = key_in_table;
		icon = value;

		/* If we're going to be picky, then don't accept anything
		 * other than exactly what we are looking for.
		 */
		if (picky && (icon->scaled || custom != icon->custom)) {
			return NULL;
		}

		/* Check if the cached image is good before using it. */
		if (!cached_icon_still_valid (scalable_icon->uri,
					      icon->cache_time)) {
			icon = NULL;
		}
	}
	
	if (icon == NULL) {
		/* Not in the table, so load the image. */
		/* If we're picky, then we want the image only if this exact
		 * nominal size is available.
		 */
		if (picky) {
			g_assert (scalable_icon->embedded_text == NULL);

			/* Actual icons have nominal sizes that are square! */
			if (size->nominal_width != size->nominal_height) {
				return NULL;
			}

			/* Get the image. */
			icon = load_specific_icon (scalable_icon,
						   size->nominal_width,
						   custom);
			if (icon == NULL) {
				return NULL;
			}

			/* Now we have the image, but is it bigger
			 * than the maximum size? If so we scale it,
			 * but we don't call it "scaled" for caching
			 * purposese.
			 */
			scaled_icon = scale_down_if_too_big (icon, size);
			if (scaled_icon != NULL) {
				scaled_icon->scaled = FALSE;
				cache_icon_unref (icon);
				icon = scaled_icon;
			}
		} else {
			if (scalable_icon->embedded_text != NULL) {
				icon = load_icon_with_embedded_text (scalable_icon, size);
			} else {
				icon = load_icon_scale_if_necessary (scalable_icon, size);
			}
			g_assert (icon != NULL);
		}

		/* Create the key and icon for the hash table. */
		key = g_new (CacheKey, 1);
		nautilus_scalable_icon_ref (scalable_icon);
		key->scalable_icon = scalable_icon;
		key->size = *size;
		
		/* Add the item to the hash table. */
		g_assert (g_hash_table_lookup (hash_table, key) == NULL);
		g_hash_table_insert (hash_table, key, icon);
	}

	/* Hand back a ref to the caller. */
	cache_icon_ref (icon);

	/* Since this item was used, keep it in the cache longer. */
	mark_recently_used (&icon->recently_used_node);

	/* Come back later and sweep the cache. */
	nautilus_icon_factory_schedule_sweep ();

        return icon;
}

GdkPixbuf *
nautilus_icon_factory_get_pixbuf_for_icon (NautilusScalableIcon *scalable_icon,
					   guint nominal_width,
					   guint nominal_height,
					   guint maximum_width,
					   guint maximum_height,
					   NautilusEmblemAttachPoints *attach_points)
{
	IconSizeRequest size;
	CacheIcon *icon;
	GdkPixbuf *pixbuf;
	
	size.nominal_width = nominal_width;
	size.nominal_height = nominal_width;
	size.maximum_width = maximum_width;
	size.maximum_height = maximum_height;
	icon = get_icon_from_cache (scalable_icon, &size,
				    FALSE, scalable_icon->uri != NULL);

	if (attach_points != NULL) {
		*attach_points = icon->details.attach_points;
	}
	
	/* The first time we hand out an icon we just leave it with a
	 * single ref (we'll get called back for the unref), but
	 * subsequent times we add additional refs.
	 */
	pixbuf = icon->pixbuf;
	if (!icon->outstanding) {
		icon->outstanding = TRUE;
	} else {
		gdk_pixbuf_ref (pixbuf);
	}
	cache_icon_unref (icon);

	return pixbuf;
}

static guint
cache_key_hash (gconstpointer p)
{
	const CacheKey *key;

	key = p;
	return (((((((GPOINTER_TO_UINT (key->scalable_icon) << 4)
		     ^ key->size.nominal_width) << 4)
		   ^ key->size.nominal_height) << 4)
		 ^ key->size.maximum_width) << 4)
		^ key->size.maximum_height;
}

static gboolean
cache_key_equal (gconstpointer a, gconstpointer b)
{
	const CacheKey *key_a, *key_b;

	key_a = a;
	key_b = b;

	return key_a->scalable_icon == key_b->scalable_icon
		&& key_a->size.nominal_width == key_b->size.nominal_width
		&& key_a->size.nominal_height == key_b->size.nominal_height
		&& key_a->size.maximum_width == key_b->size.maximum_width
		&& key_a->size.maximum_height == key_b->size.maximum_height;
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
					   const char *modifer,
					   guint size_in_pixels,
					   gboolean anti_aliased)
{
	NautilusScalableIcon *icon;
	GdkPixbuf *pixbuf;

	/* Get the pixbuf for this file. */
	icon = nautilus_icon_factory_get_icon_for_file (file, modifer, anti_aliased);
	if (icon == NULL) {
		return NULL;
	}
	pixbuf = nautilus_icon_factory_get_pixbuf_for_icon
		(icon,
		 size_in_pixels, size_in_pixels,
		 size_in_pixels, size_in_pixels, NULL);
	nautilus_scalable_icon_unref (icon);

	return pixbuf;
}

/* Convenience cover for nautilus_icon_factory_get_icon_for_file,
 * nautilus_icon_factory_get_pixbuf_for_icon,
 * and gdk_pixbuf_render_pixmap_and_mask.
 */
void
nautilus_icon_factory_get_pixmap_and_mask_for_file (NautilusFile *file,
						    const char *modifer,
						    guint size_in_pixels,
						    GdkPixmap **pixmap,
						    GdkBitmap **mask)
{
	GdkPixbuf *pixbuf;

	g_return_if_fail (pixmap != NULL);
	g_return_if_fail (mask != NULL);

	*pixmap = NULL;
	*mask = NULL;

	pixbuf = nautilus_icon_factory_get_pixbuf_for_file (file, modifer, size_in_pixels, FALSE);
	if (pixbuf == NULL) {
		return;
	}
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, pixmap, mask, NAUTILUS_STANDARD_ALPHA_THRESHHOLD);
	gdk_pixbuf_unref (pixbuf);
}

static gboolean
embedded_text_rect_usable (const ArtIRect *embedded_text_rect)
{
	if (art_irect_empty (embedded_text_rect)) {
		return FALSE;
	}

	if (embedded_text_rect->x1 - embedded_text_rect->x0 
	    < MINIMUM_EMBEDDED_TEXT_RECT_WIDTH ||
	    embedded_text_rect->y1 - embedded_text_rect->y0 
	    < MINIMUM_EMBEDDED_TEXT_RECT_HEIGHT) {
		return FALSE;
	}

	return TRUE;
}

/* FIXME bugzilla.eazel.com 2783: Icon factory needs support for speciiy whether
 * embedded text Embedded text should be smoother or not.
 */
static gboolean smooth_graphics = TRUE;

static GdkPixbuf *
embed_text (GdkPixbuf *pixbuf_without_text,
	    const ArtIRect *embedded_text_rect,
	    const char *text)
{
	NautilusScalableFont *smooth_font;
	static GdkFont *font;
	GdkPixbuf *pixbuf_with_text;
	
	g_return_val_if_fail (pixbuf_without_text != NULL, NULL);
	g_return_val_if_fail (embedded_text_rect != NULL, NULL);
	
	/* Quick out for the case where there's no place to embed the
	 * text or the place is too small or there's no text.
	 */
	if (!embedded_text_rect_usable (embedded_text_rect) || nautilus_strlen (text) == 0) {
		return NULL;
	}
		
	pixbuf_with_text = gdk_pixbuf_copy (pixbuf_without_text);
		
	if (smooth_graphics) {
		smooth_font = NAUTILUS_SCALABLE_FONT
			(nautilus_scalable_font_new
			 (EMBEDDED_TEXT_FONT_FAMILY,
			  EMBEDDED_TEXT_FONT_WEIGHT,
			  EMBEDDED_TEXT_FONT_SLANT,
			  EMBEDDED_TEXT_FONT_SET_WIDTH));

		nautilus_scalable_font_draw_text_lines
			(smooth_font,
			 pixbuf_with_text,
			 embedded_text_rect->x0,
			 embedded_text_rect->y0,
			 embedded_text_rect,
			 EMBEDDED_TEXT_FONT_SIZE,
			 EMBEDDED_TEXT_FONT_SIZE,
			 text,
			 GTK_JUSTIFY_LEFT,
			 EMBEDDED_TEXT_LINE_OFFSET,
			 EMBEDDED_TEXT_EMPTY_LINE_HEIGHT,
			 NAUTILUS_RGB_COLOR_BLACK,
			 255,
			 FALSE);
		
		gtk_object_unref (GTK_OBJECT (smooth_font));
	} else {
		/* Get the font the first time through. */
		if (font == NULL) {
			/* FIXME bugzilla.eazel.com 1102: Embedded text should use preferences to determine
			 * the font it uses
			 */
			font = gdk_fontset_load (_("-*-helvetica-medium-r-normal-*-10-*-*-*-*-*-*-*"));
			g_return_val_if_fail (font != NULL, NULL);
		}
		
		nautilus_gdk_pixbuf_draw_text
			(pixbuf_with_text, font, 1.0, embedded_text_rect, 
			 text, NAUTILUS_RGB_COLOR_BLACK, 0xFF);
	}

	return pixbuf_with_text;
}

static CacheIcon *
load_icon_with_embedded_text (NautilusScalableIcon *scalable_icon,
			      const IconSizeRequest *size)
{
	NautilusScalableIcon *scalable_icon_without_text;
	CacheIcon *icon_without_text, *icon_with_text;
	GdkPixbuf *pixbuf_with_text;
	IconDetails details;

	g_assert (scalable_icon->embedded_text != NULL);
	
	/* Get the icon without text. */
	scalable_icon_without_text = nautilus_scalable_icon_new_from_text_pieces
		(scalable_icon->uri,
		 scalable_icon->name,
		 scalable_icon->modifier,
		 NULL,
		 scalable_icon->aa_mode);
	icon_without_text = get_icon_from_cache
		(scalable_icon_without_text, size,
		 FALSE, FALSE);
	nautilus_scalable_icon_unref (scalable_icon_without_text);
	
	/* Create a pixbuf with the text in it. */
	pixbuf_with_text = embed_text (icon_without_text->pixbuf,
				       &icon_without_text->details.text_rect,
				       scalable_icon->embedded_text);
	if (pixbuf_with_text == NULL) {
		return icon_without_text;
	}

	/* Create an icon from the new pixbuf. */
	details = icon_without_text->details;
	memset (&details.text_rect, 0, sizeof (details.text_rect));
	icon_with_text = cache_icon_new (pixbuf_with_text,
					 icon_without_text->custom,
					 icon_without_text->scaled,
					 &details);
	cache_icon_unref (icon_without_text);
	gdk_pixbuf_unref (pixbuf_with_text);

	return icon_with_text;
}

/* Convenience function for unrefing and then freeing an entire list. */
void
nautilus_scalable_icon_list_free (GList *icon_list)
{
	nautilus_g_list_free_deep_custom
		(icon_list, (GFunc) nautilus_scalable_icon_unref, NULL);
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
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (12), 20);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (19), 20);
	NAUTILUS_CHECK_INTEGER_RESULT (get_larger_icon_size (20), 24);
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
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (20), 12);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (21), 20);
	NAUTILUS_CHECK_INTEGER_RESULT (get_smaller_icon_size (24), 20);
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
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0, 12), "TRUE,20");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0, 20), "TRUE,24");
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
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (36, 24), "TRUE,20");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (36, 20), "TRUE,12");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (36, 12), "FALSE,12");

	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 0), "TRUE,48");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 48), "TRUE,72");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 72), "TRUE,96");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 96), "TRUE,192");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 192), "TRUE,36");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 36), "TRUE,24");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 24), "TRUE,20");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 20), "TRUE,12");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (37, 12), "FALSE,12");

	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 0), "TRUE,192");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 192), "TRUE,96");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 96), "TRUE,72");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 72), "TRUE,48");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 48), "TRUE,36");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 36), "TRUE,24");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 24), "TRUE,20");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 20), "TRUE,12");
	NAUTILUS_CHECK_STRING_RESULT (self_test_next_icon_size_to_try (0xFFFFFFFF, 12), "FALSE,12");
}

#endif /* ! NAUTILUS_OMIT_SELF_CHECK */
