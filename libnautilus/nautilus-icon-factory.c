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
  
   Authors: John Sullivan <sullivan@eazel.com>, Darin Adler <darin@eazel.com>
*/

#include <config.h>
#include "nautilus-icon-factory.h"

#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gnome.h>
#include <png.h>

#include <libgnomevfs/gnome-vfs-types.h>
#include <libgnomevfs/gnome-vfs-file-info.h>

#include <parser.h>
#include <xmlmemory.h>

#include "nautilus-string.h"
#include "nautilus-default-file-icon.h"
#include "nautilus-metadata.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-global-preferences.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-xml-extensions.h"

/* List of suffixes to search when looking for an icon file. */
static const char *icon_file_name_suffixes[] =
{
	".png",
	".PNG",
	".gif",
	".GIF"
};

#define ICON_NAME_DIRECTORY             "i-directory"
#define ICON_NAME_DIRECTORY_CLOSED      "i-dirclosed"
#define ICON_NAME_EXECUTABLE            "i-executable"
#define ICON_NAME_REGULAR               "i-regular"
#define ICON_NAME_CORE                  "i-core"
#define ICON_NAME_SOCKET                "i-sock"
#define ICON_NAME_FIFO                  "i-fifo"
#define ICON_NAME_CHARACTER_DEVICE      "i-chardev"
#define ICON_NAME_BLOCK_DEVICE          "i-blockdev"
#define ICON_NAME_BROKEN_SYMBOLIC_LINK  "i-brokenlink"

#define ICON_NAME_THUMBNAIL_LOADING     "loading"

#define EMBLEM_NAME_PREFIX              "emblem-"

#define EMBLEM_NAME_SYMBOLIC_LINK       "symbolic-link"

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

/* permissions for thumbnail directory */

#define THUMBNAIL_DIR_PERMISSIONS (GNOME_VFS_PERM_USER_ALL | GNOME_VFS_PERM_GROUP_ALL | GNOME_VFS_PERM_OTHER_ALL)

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

	/* thumbnail task state */
	GList *thumbnails;
	char *new_thumbnail_path;
	gboolean thumbnail_in_progress;
	
	/* id of timeout task for making thumbnails */
	int timeout_task_id;
} NautilusIconFactory;

typedef struct {
	GtkObjectClass parent_class;
} NautilusIconFactoryClass;

enum {
	ICONS_CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

/* A scalable icon, which is basically the name and path of an icon,
 * before we load the actual pixels of the icons's image.
 */
struct NautilusScalableIcon {
	guint ref_count;

	char *uri;
	char *name;
};

/* The key to a hash table that holds the scaled icons as pixbufs.
 * In a way, it's not really completely a key, because part of the
 * data is stored in here, including the LRU chain.
 */
typedef struct {
	NautilusScalableIcon *scalable_icon;
	guint size_in_pixels_x;
	guint size_in_pixels_y;

	NautilusCircularList recently_used_node;

	gboolean custom;
	gboolean scaled;
	ArtIRect text_rect;
} NautilusIconCacheKey;

/* forward declarations */

static void                  icon_theme_changed_callback             (NautilusPreferences      *preferences,
								      const char               *name,
								      NautilusPreferencesType   type,
								      gconstpointer             value,
								      gpointer                  user_data);
static GtkType               nautilus_icon_factory_get_type          (void);
static void                  nautilus_icon_factory_initialize_class  (NautilusIconFactoryClass *class);
static void                  nautilus_icon_factory_initialize        (NautilusIconFactory      *factory);
static NautilusIconFactory * nautilus_get_current_icon_factory       (void);
static char *                nautilus_icon_factory_get_thumbnail_uri (NautilusFile             *file);
static NautilusIconFactory * nautilus_icon_factory_new               (const char               *theme_name);
static void                  nautilus_icon_factory_set_theme         (const char               *theme_name);
static NautilusScalableIcon *nautilus_scalable_icon_get              (const char               *uri,
								      const char               *name);
static guint                 nautilus_scalable_icon_hash             (gconstpointer             p);
static gboolean              nautilus_scalable_icon_equal            (gconstpointer             a,
								      gconstpointer             b);
static void                  nautilus_icon_cache_key_destroy         (NautilusIconCacheKey     *key);
static guint                 nautilus_icon_cache_key_hash            (gconstpointer             p);
static gboolean              nautilus_icon_cache_key_equal           (gconstpointer             a,
								      gconstpointer             b);
static gboolean              vfs_file_exists                         (const char               *file_name);
static GdkPixbuf *           get_image_from_cache                    (NautilusScalableIcon     *scalable_icon,
								      guint                     size_in_pixels_x,
								      guint                     size_in_pixels_y,
								      gboolean                  picky,
								      gboolean                  custom,
								      ArtIRect                 *text_rect);
static gboolean              check_for_thumbnails                    (NautilusIconFactory      *factory);
static int                   nautilus_icon_factory_make_thumbnails   (gpointer                  data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusIconFactory, nautilus_icon_factory, GTK_TYPE_OBJECT)

/* Return a pointer to the single global icon factory. */
static NautilusIconFactory *
nautilus_get_current_icon_factory (void)
{
        static NautilusIconFactory *global_icon_factory = NULL;
        if (global_icon_factory == NULL) {
		char *theme_preference;

		/* No guarantee that nautilus preferences have been set
		 * up properly, so we have to initialize them all here just
		 * to be sure that the icon_theme preference will work.
		 */
		nautilus_global_preferences_initialize ();
		theme_preference
			= nautilus_preferences_get_string (nautilus_preferences_get_global_preferences (),
					       	 	   NAUTILUS_PREFERENCES_ICON_THEME);
                global_icon_factory = nautilus_icon_factory_new (theme_preference);
                g_free (theme_preference);

		nautilus_preferences_add_callback (nautilus_preferences_get_global_preferences (),
						   NAUTILUS_PREFERENCES_ICON_THEME,
						   icon_theme_changed_callback,
						   NULL);	

        }
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

	signals[ICONS_CHANGED]
		= gtk_signal_new ("icons_changed",
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
}

#if 0

static void
nautilus_icon_factory_destroy (NautilusIconFactory *factory)
{
	nautilus_preferences_remove_callback (nautilus_preferences_get_global_preferences (),
					      NAUTILUS_PREFERENCES_ICON_THEME,
					      icon_theme_changed_callback,
					      NULL);

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
        if (icon_key->recently_used_node.next != NULL) {
                return FALSE;
	}

	/* Don't free a cache entry if the image is still in use. */
	image = value;
	if (image->ref_count > 1) {
		return FALSE;
	}

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

        if (factory->sweep_timer != 0) {
                return;
	}

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

	gtk_signal_emit (GTK_OBJECT (factory),
			 signals[ICONS_CHANGED]);
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
        switch (nautilus_file_get_file_type (file)) {
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

/* Pick a particular icon to use, trying all the various suffixes.
 * Return the path of the icon or NULL if no icon is found.
 */
static char *
get_themed_icon_file_path (const char *theme_name,
			   const char *icon_name,
			   guint icon_size,
			   ArtIRect *text_rect)
{
	int i;
	gboolean include_size;
	char *themed_icon_name, *partial_path, *path, *xml_path;
	xmlDocPtr doc;
	xmlNodePtr node;
	char *size_as_string, *property;
	ArtIRect parsed_rect;

	if (theme_name == NULL) {
		themed_icon_name = g_strdup (icon_name);
	} else {
		themed_icon_name = g_strconcat (theme_name, "/", icon_name, NULL);
	}

	include_size = icon_size != NAUTILUS_ICON_SIZE_STANDARD;

	/* Try each suffix. */
	for (i = 0; i < NAUTILUS_N_ELEMENTS (icon_file_name_suffixes); i++) {

		/* Build a path for this icon. */
		partial_path = g_strdup_printf ("nautilus/%s%s%.0u%s",
						themed_icon_name,
						include_size ? "-" : "",
						include_size ? icon_size : 0,
						icon_file_name_suffixes[i]);
		path = gnome_pixmap_file (partial_path);
		g_free (partial_path);

		/* Return the path if the file exists. */
		if (path != NULL && g_file_exists (path)) {
			break;
		}
		g_free (path);
	}

	/* Open the XML file to get the text rectangle. */
	if (path != NULL && text_rect != NULL) {
		memset (text_rect, 0, sizeof (*text_rect));

		partial_path = g_strdup_printf ("nautilus/%s.xml",
						themed_icon_name);
		xml_path = gnome_pixmap_file (partial_path);
		g_free (partial_path);

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
				*text_rect = parsed_rect;
			}
			xmlFree (property);
		}

		xmlFreeDoc (doc);
	}

	return path;
}

/* Choose the file name to load, taking into account theme vs. non-theme icons. */
static char *
get_icon_file_path (const char *name, guint size_in_pixels, ArtIRect *text_rect)
{
	gboolean use_theme_icon;
	const char *theme_name;
	char *path;

	use_theme_icon = FALSE;
 	theme_name = nautilus_get_current_icon_factory ()->theme_name;
	
	/* Check and see if there is a theme icon to use.
	 * This decision must be based on whether there's a non-size-
	 * specific theme icon.
	 */
	if (theme_name != NULL) {
		path = get_themed_icon_file_path (theme_name,
						  name,
						  NAUTILUS_ICON_SIZE_STANDARD,
						  NULL);		
		if (path != NULL) {
			use_theme_icon = TRUE;
			g_free (path);
		}
	}
	
	/* Now we know whether or not to use the theme. */
	return get_themed_icon_file_path (use_theme_icon ? theme_name : NULL,
					  name,
					  size_in_pixels,
					  text_rect);
}

static void
icon_theme_changed_callback (NautilusPreferences *preferences,
         		     const char *name,
         		     GtkFundamentalType type,
         		     gconstpointer value,
         		     gpointer user_data)
{
	g_assert (NAUTILUS_IS_PREFERENCES (preferences));
	g_assert (strcmp (name, NAUTILUS_PREFERENCES_ICON_THEME) == 0);
	g_assert (type == NAUTILUS_PREFERENCE_STRING);
	g_assert (value != NULL);
	g_assert (user_data == NULL);

	nautilus_icon_factory_set_theme ((char *) value);
}

/* Get or create a scalable icon. */
static NautilusScalableIcon *
nautilus_scalable_icon_get (const char *uri,
			    const char *name)
{
	GHashTable *hash_table;
	NautilusScalableIcon icon_key, *icon;

	/* Get at the hash table. */
	hash_table = nautilus_get_current_icon_factory ()->scalable_icons;

	/* Check to see if it's already in the table. */
	icon_key.uri = (char *) uri;
	icon_key.name = (char *) name;
	icon = g_hash_table_lookup (hash_table, &icon_key);
	if (icon == NULL) {
		/* Not in the table, so create it and put it in. */
		icon = g_new0 (NautilusScalableIcon, 1);
		icon->uri = g_strdup (uri);
		icon->name = g_strdup (name);
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

	if (icon->uri != NULL) {
		hash = g_str_hash (icon->uri);
	}

	hash <<= 4;
	if (icon->name != NULL) {
		hash ^= g_str_hash (icon->name);
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
		&& nautilus_strcmp (icon_a->name, icon_b->name) == 0;
}

NautilusScalableIcon *
nautilus_icon_factory_get_icon_for_file (NautilusFile *file)
{
	char *uri;
        const char *name;
	NautilusScalableIcon *scalable_icon;
	
	if (file == NULL) {
		return NULL;
	}
	
	/* if there is a custom image in the metadata, use that. */
	uri = nautilus_file_get_metadata (file, NAUTILUS_CUSTOM_ICON_METADATA_KEY, NULL);
	
	/* if the file is an image, either use the image itself as the icon if it's small enough,
	   or use a thumbnail if one exists.  If a thumbnail is required, but does not yet exist,
	   put an entry on the thumbnail queue so we eventually make one */
	   
	if (uri == NULL && nautilus_str_has_prefix (nautilus_file_get_mime_type (file), "image/")) {
		if (nautilus_file_get_size (file) < SELF_THUMBNAIL_SIZE_THRESHOLD) {
			uri = nautilus_file_get_uri (file);
		} else {
			uri = nautilus_icon_factory_get_thumbnail_uri (file);
		}
	}
	
	/* Get the generic icon set for this file. */
        name = nautilus_icon_factory_get_icon_name_for_file (file);
	
	/* Create the icon or find it in the cache if it's already there. */
	scalable_icon = nautilus_scalable_icon_get (uri, name);
	g_free (uri);
	
	return scalable_icon;
}

static void
add_emblem (GList **icons, const char *name)
{
	char *name_with_prefix;

	name_with_prefix = g_strconcat (EMBLEM_NAME_PREFIX, name, NULL);
	*icons = g_list_prepend (*icons, nautilus_scalable_icon_get (NULL, name_with_prefix));
	g_free (name_with_prefix);
}

GList *
nautilus_icon_factory_get_emblem_icons_for_file (NautilusFile *file)
{
	GList *icons, *keywords, *p;

	icons = NULL;

	/* One icon for the symbolic link. */
	if (nautilus_file_is_symbolic_link (file)) {
		add_emblem (&icons, EMBLEM_NAME_SYMBOLIC_LINK);
	}

	/* One icon for each keyword. */
	keywords = nautilus_file_get_keywords (file);
	for (p = keywords; p != NULL; p = p->next) {
		add_emblem (&icons, p->data);
	}

	nautilus_g_list_free_deep (keywords);

	return g_list_reverse (icons);
}

/* utility to test whether a file exists using vfs */
static gboolean
vfs_file_exists (const char *file_uri)
{
	GnomeVFSResult result;
	GnomeVFSFileInfo *file_info;

	file_info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (file_uri, file_info, 0, NULL);
	gnome_vfs_file_info_unref (file_info);
	return result == GNOME_VFS_OK;
}

/* utility routine that, given the uri of an image, constructs the uri to the corresponding thumbnail */

static char *
make_thumbnail_path (const char *image_uri, gboolean directory_only)
{
	char *thumbnail_uri;
	char *temp_str = g_strdup (image_uri);
	char *last_slash = strrchr (temp_str, '/');
	*last_slash = '\0';
	
	if (directory_only) {
		thumbnail_uri = g_strdup_printf ("%s/.thumbnails", temp_str);
	} else {
		if (nautilus_str_has_suffix (image_uri, ".png")
		    || nautilus_str_has_suffix (image_uri, ".PNG")) {
			thumbnail_uri = g_strdup_printf ("%s/.thumbnails/%s", temp_str, last_slash + 1);
		} else {
			thumbnail_uri = g_strdup_printf ("%s/.thumbnails/%s.png", temp_str, last_slash + 1);
		}
	}
	g_free (temp_str);
	return thumbnail_uri;
}

/* structure used for making thumbnails, associating a uri with the requesting controller */

typedef struct {
	char *thumbnail_uri;
} NautilusThumbnailInfo;

/* GCompareFunc-style function for comparing NautilusThumbnailInfos.
 * Returns 0 if they refer to the same uri.
 */
static int
compare_thumbnail_info (gconstpointer a, gconstpointer b)
{
	NautilusThumbnailInfo *info_a;
	NautilusThumbnailInfo *info_b;

	info_a = (NautilusThumbnailInfo *)a;
	info_b = (NautilusThumbnailInfo *)b;

	return strcmp (info_a->thumbnail_uri, info_b->thumbnail_uri) != 0;
}

/* routine that takes a uri of a large image file and returns the uri of its corresponding thumbnail.
   If no thumbnail is available, put the image on the thumbnail queue so one is eventually made. */
/* FIXME: Most of this thumbnail machinery belongs in NautilusFile, not here.
 */

static char *
nautilus_icon_factory_get_thumbnail_uri (NautilusFile *file)
{
	NautilusIconFactory *factory;
	GnomeVFSResult result;
	char *thumbnail_uri;
	char *file_uri;

	file_uri = nautilus_file_get_uri (file);
	
	/* compose the uri for the thumbnail */
	thumbnail_uri = make_thumbnail_path (file_uri, FALSE);
		
	/* if the thumbnail file already exists, simply return the uri */
	if (vfs_file_exists (thumbnail_uri)) {
		g_free (file_uri);
		return thumbnail_uri;
	}
	
        /* make the thumbnail directory if necessary */
	g_free (thumbnail_uri);
	thumbnail_uri = make_thumbnail_path (file_uri, TRUE);
	result = gnome_vfs_make_directory (thumbnail_uri, THUMBNAIL_DIR_PERMISSIONS);

	/* the thumbnail needs to be created, so add an entry to the thumbnail list */
 
	/* FIXME: need to handle error by making directory elsewhere */
	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_FILEEXISTS) {
		g_warning ("error when making thumbnail directory: %d\n", result);	
	} else {
		NautilusThumbnailInfo *info = g_new0 (NautilusThumbnailInfo, 1);
		info->thumbnail_uri = file_uri;
		
		factory = nautilus_get_current_icon_factory ();		
		if (factory->thumbnails) {
			if (g_list_find_custom (factory->thumbnails, info, compare_thumbnail_info) == NULL) {
				factory->thumbnails = g_list_prepend (factory->thumbnails, info);
			}
		} else {
			factory->thumbnails = g_list_alloc ();
			factory->thumbnails->data = info;
		}
	
		if (factory->timeout_task_id == 0) {
			factory->timeout_task_id = gtk_timeout_add (400, (GtkFunction) nautilus_icon_factory_make_thumbnails, NULL);
		}
	}
	g_free (thumbnail_uri);
	
	/* return the uri to the "loading image" icon */
	return get_icon_file_path (ICON_NAME_THUMBNAIL_LOADING,
				   NAUTILUS_ICON_SIZE_STANDARD,
				   NULL);
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
		     gboolean custom,
		     ArtIRect *text_rect)
{
	g_assert (text_rect != NULL);

	if (custom) {
		/* Custom icon. */

		/* FIXME: This works only with file:// images, because there's
		 * no convenience function for loading an image with gnome-vfs
		 * and gdk-pixbuf.
		 */
		if (size_in_pixels == NAUTILUS_ICON_SIZE_STANDARD
		    && nautilus_str_has_prefix (scalable_icon->uri, "file://")) {
			memset (text_rect, 0, sizeof (*text_rect));
			return gdk_pixbuf_new_from_file (scalable_icon->uri + 7);
		}

		return NULL;
	} else {
		/* Standard icon. */
		char *path;
		GdkPixbuf *image;
		
		path = get_icon_file_path (scalable_icon->name,
					   size_in_pixels,
					   text_rect);
		if (path == NULL) {
			return NULL;
		}
		image = gdk_pixbuf_new_from_file (path);
		g_free (path);
		return image;
	}
}

/* This load function is not allowed to return NULL. */
static GdkPixbuf *
load_image_for_scaling (NautilusScalableIcon *scalable_icon,
			guint requested_size,
			guint *actual_size_result,
			gboolean *custom,
			ArtIRect *text_rect)
{
        GdkPixbuf *image;
	guint actual_size;
	static GdkPixbuf *fallback_image;

	/* First check for a custom image. */
	actual_size = 0;
	while (get_next_icon_size_to_try (requested_size, &actual_size)) {
		image = get_image_from_cache (scalable_icon,
					      actual_size,
					      actual_size,
					      TRUE,
					      TRUE,
					      text_rect);
		if (image != NULL) {
			*actual_size_result = actual_size;
			*custom = TRUE;
			return image;
		}
	}
	
	/* Next, go for the normal image. */
	actual_size = 0;
	while (get_next_icon_size_to_try (requested_size, &actual_size)) {
		image = get_image_from_cache (scalable_icon,
					      actual_size,
					      actual_size,
					      TRUE,
					      FALSE,
					      text_rect);
		if (image != NULL) {
			*actual_size_result = actual_size;
			*custom = FALSE;
			return image;
		}
	}

	/* Finally, fall back on the hard-coded image. */
	if (fallback_image == NULL) {
		fallback_image = gdk_pixbuf_new_from_data
			(nautilus_default_file_icon,
			 ART_PIX_RGB,
			 TRUE,
			 nautilus_default_file_icon_width,
			 nautilus_default_file_icon_height,
			 nautilus_default_file_icon_width * 4, /* stride */
			 NULL, /* don't destroy data */
			 NULL);
	}
	gdk_pixbuf_ref (fallback_image);

	memset (text_rect, 0, sizeof (*text_rect));
	*actual_size_result = NAUTILUS_ICON_SIZE_STANDARD;
	*custom = FALSE;
        return fallback_image;
}

/* This load function is not allowed to return NULL. */
static GdkPixbuf *
load_image_scale_if_necessary (NautilusScalableIcon *scalable_icon,
			       guint requested_size_x,
			       guint requested_size_y,
			       gboolean *scaled,
			       gboolean *custom,
			       ArtIRect *text_rect)
{
        GdkPixbuf *image, *scaled_image;
	guint actual_size;
	int scaled_width, scaled_height;
	
	/* Load the image for the icon that's closest in size to what we want. */
	image = load_image_for_scaling (scalable_icon, requested_size_x,
					&actual_size, custom, text_rect);
        if (requested_size_x == actual_size && requested_size_y == actual_size) {
		*scaled = FALSE;
		return image;
	}
	
	/* Scale the image to the size we want. */
	scaled_width = gdk_pixbuf_get_width (image) * requested_size_x / actual_size;
	scaled_height = gdk_pixbuf_get_height (image) * requested_size_y / actual_size;
	scaled_image = gdk_pixbuf_scale_simple
		(image, scaled_width, scaled_height, ART_FILTER_BILINEAR);

	/* Scale the text rectangle to the same size. */
	text_rect->x0 = text_rect->x0 * requested_size_x / actual_size;
	text_rect->y0 = text_rect->y0 * requested_size_y / actual_size;
	text_rect->x1 = text_rect->x1 * requested_size_x / actual_size;
	text_rect->y1 = text_rect->y1 * requested_size_y / actual_size;
	
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
		      guint size_in_pixels_x,
		      guint size_in_pixels_y,
		      gboolean picky,
		      gboolean custom,
		      ArtIRect *text_rect)
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
	lookup_key.size_in_pixels_x = size_in_pixels_x;
	lookup_key.size_in_pixels_y = size_in_pixels_y;
	if (g_hash_table_lookup_extended (hash_table, &lookup_key,
					  &key_in_table, &value)) {
		/* Found it in the table. */
		key = key_in_table;

		/* If we're going to be picky, then don't accept anything
		 * other than exactly what we are looking for.
		 */
		if (picky && (key->scaled || custom != key->custom)) {
			return NULL;
		}

		image = value;
		g_assert (image != NULL);
	} else {
		gboolean got_scaled_image;
		gboolean got_custom_image;
		ArtIRect key_text_rect;
		
		/* Not in the table, so load the image. */
		if (picky) {
			if (size_in_pixels_x != size_in_pixels_y) {
				return NULL;
			}
			image = load_specific_image (scalable_icon,
						     size_in_pixels_x,
						     custom,
						     &key_text_rect);
			if (image == NULL) {
				return NULL;
			}

			got_scaled_image = FALSE;
			got_custom_image = custom;
		} else {
			image = load_image_scale_if_necessary (scalable_icon,
							       size_in_pixels_x,
							       size_in_pixels_y,
							       &got_scaled_image,
							       &got_custom_image,
							       &key_text_rect);
			g_assert (image != NULL);
		}

		/* Create the key for the table. */
		key = g_new0 (NautilusIconCacheKey, 1);
		nautilus_scalable_icon_ref (scalable_icon);
		key->scalable_icon = scalable_icon;
		key->size_in_pixels_x = size_in_pixels_x;
		key->size_in_pixels_y = size_in_pixels_y;
		key->scaled = got_scaled_image;
		key->custom = got_custom_image;
		key->text_rect = key_text_rect;
		
		/* Add the item to the hash table. */
		g_hash_table_insert (hash_table, key, image);
	}

	/* Return the text rect if the caller asked for it. */
	if (text_rect != NULL) {
		*text_rect = key->text_rect;
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
					   guint size_in_pixels_x, guint size_in_pixels_y,
					   ArtIRect *text_rect)
{
	return get_image_from_cache (scalable_icon,
				     size_in_pixels_x, size_in_pixels_y,
				     FALSE, FALSE, text_rect);
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
	return (((GPOINTER_TO_UINT (key->scalable_icon) << 4)
		 ^ key->size_in_pixels_x) << 4)
		^ key->size_in_pixels_y;
}

static gboolean
nautilus_icon_cache_key_equal (gconstpointer a, gconstpointer b)
{
	const NautilusIconCacheKey *key_a, *key_b;

	key_a = a;
	key_b = b;

	return key_a->scalable_icon == key_b->scalable_icon
		&& key_a->size_in_pixels_x == key_b->size_in_pixels_x
		&& key_a->size_in_pixels_y == key_b->size_in_pixels_y;
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
	pixbuf = nautilus_icon_factory_get_pixbuf_for_icon (icon,
							    size_in_pixels,
							    size_in_pixels,
							    NULL);
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

/* Convenience function for unrefing and then freeing an entire list. */
void
nautilus_scalable_icon_list_free (GList *icon_list)
{
	g_list_foreach (icon_list, (GFunc) nautilus_scalable_icon_unref, NULL);
	g_list_free (icon_list);
}

/* utility routine for saving a pixbuf to a png file.
 * This was adapted from Iain Holmes' code in gnome-iconedit, and probably
 * should be in a utility library, possibly in gdk-pixbuf itself.
 */
static gboolean
save_pixbuf_to_file (GdkPixbuf *pixbuf, char *filename)
{
	FILE *handle;
  	char *buffer;
	gboolean has_alpha;
	int width, height, depth, rowstride;
  	guchar *pixels;
  	png_structp png_ptr;
  	png_infop info_ptr;
  	png_text text[2];
  	int i;

	g_return_val_if_fail (pixbuf != NULL, FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (filename[0] != '\0', FALSE);

        handle = fopen (filename, "wb");
        if (handle == NULL) {
        	return FALSE;
	}

	png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (png_ptr == NULL) {
		fclose (handle);
		return FALSE;
	}

	info_ptr = png_create_info_struct (png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct (&png_ptr, (png_infopp)NULL);
		fclose (handle);
	    	return FALSE;
	}

	if (setjmp (png_ptr->jmpbuf)) {
		png_destroy_write_struct (&png_ptr, &info_ptr);
		fclose (handle);
		return FALSE;
	}

	png_init_io (png_ptr, handle);

        has_alpha = gdk_pixbuf_get_has_alpha (pixbuf);
	width = gdk_pixbuf_get_width (pixbuf);
	height = gdk_pixbuf_get_height (pixbuf);
	depth = gdk_pixbuf_get_bits_per_sample (pixbuf);
	pixels = gdk_pixbuf_get_pixels (pixbuf);
	rowstride = gdk_pixbuf_get_rowstride (pixbuf);

	png_set_IHDR (png_ptr, info_ptr, width, height,
			depth, PNG_COLOR_TYPE_RGB_ALPHA,
			PNG_INTERLACE_NONE,
			PNG_COMPRESSION_TYPE_DEFAULT,
			PNG_FILTER_TYPE_DEFAULT);

	/* Some text to go with the png image */
	text[0].key = "Title";
	text[0].text = filename;
	text[0].compression = PNG_TEXT_COMPRESSION_NONE;
	text[1].key = "Software";
	text[1].text = "Nautilus Thumbnail";
	text[1].compression = PNG_TEXT_COMPRESSION_NONE;
	png_set_text (png_ptr, info_ptr, text, 2);

	/* Write header data */
	png_write_info (png_ptr, info_ptr);

	/* if there is no alpha in the data, allocate buffer to expand into */
	if (has_alpha) {
		buffer = NULL;
	} else {
		buffer = g_malloc(4 * width);
	}
	
	/* pump the raster data into libpng, one scan line at a time */	
	for (i = 0; i < height; i++) {
		if (has_alpha) {
			png_bytep row_pointer = pixels;
			png_write_row (png_ptr, row_pointer);
		} else {
			/* expand RGB to RGBA using an opaque alpha value */
			int x;
			char *buffer_ptr = buffer;
			char *source_ptr = pixels;
			for (x = 0; x < width; x++) {
				*buffer_ptr++ = *source_ptr++;
				*buffer_ptr++ = *source_ptr++;
				*buffer_ptr++ = *source_ptr++;
				*buffer_ptr++ = 255;
			}
			png_write_row (png_ptr, (png_bytep) buffer);		
		}
		pixels += rowstride;
	}
	
	png_write_end (png_ptr, info_ptr);
	png_destroy_write_struct (&png_ptr, &info_ptr);
	
	g_free (buffer);
		
	fclose (handle);
	return TRUE;
}

/* check_for_thumbnails is a utility that checks to see if any of the thumbnails in the pending
   list have been created yet.  If it finds one, it removes the elements from the queue and
   returns true, otherwise it returns false */

static gboolean 
check_for_thumbnails (NautilusIconFactory *factory)
{
	char *current_thumbnail;
	NautilusThumbnailInfo *info;
	GList *stop_element;
	GList *next_thumbnail;
	NautilusFile *file;

	for (next_thumbnail = factory->thumbnails;
	     next_thumbnail != NULL;
	     next_thumbnail = next_thumbnail->next) {
		info = (NautilusThumbnailInfo*) next_thumbnail->data;
		current_thumbnail = make_thumbnail_path (info->thumbnail_uri, FALSE);
		if (vfs_file_exists (current_thumbnail)) {
			/* we found one, so update the icon and remove all of the elements up to and including
			   this one from the pending list. */
			g_free (current_thumbnail);
			file = nautilus_file_get (info->thumbnail_uri);
			if (file != NULL) {
				nautilus_file_changed (file);
				nautilus_file_unref (file);
			}
			
			stop_element = next_thumbnail->next;
			while (factory->thumbnails != stop_element) {
				info = (NautilusThumbnailInfo *) factory->thumbnails->data;
				g_free (info->thumbnail_uri);
				g_free (info);
				factory->thumbnails = g_list_remove_link (factory->thumbnails, factory->thumbnails);
			}
			return TRUE;
		}
	    
		g_free (current_thumbnail);
	}
	
	return FALSE;
}

/* make_thumbnails is invoked periodically as a timer task to launch a task to make thumbnails */

static int
nautilus_icon_factory_make_thumbnails (gpointer data)
{
	pid_t thumbnail_pid;
	NautilusThumbnailInfo *info;
	NautilusIconFactory *factory = nautilus_get_current_icon_factory();
	GList *next_thumbnail = factory->thumbnails;
	
	/* if the queue is empty, there's nothing more to do */
	
	if (next_thumbnail == NULL) {
		gtk_timeout_remove (factory->timeout_task_id);
		factory->timeout_task_id = 0;
		return FALSE;
	}
	
	info = (NautilusThumbnailInfo *) next_thumbnail->data;
	
	/* see which state we're in.  If a thumbnail isn't in progress, start one up.  Otherwise,
	   check if the pending one is completed.  */	
	if (factory->thumbnail_in_progress) {
		if (check_for_thumbnails(factory)) {
			factory->thumbnail_in_progress = FALSE;
		}
	} 
	else {
		/* start up a task to make the thumbnail corresponding to the queue element. */
			
		/* First, compute the path name of the target thumbnail */
		g_free (factory->new_thumbnail_path);
		factory->new_thumbnail_path = make_thumbnail_path (info->thumbnail_uri, FALSE);

		/* fork a task to make the thumbnail, using gdk-pixbuf to do the scaling */
		if (!(thumbnail_pid = fork())) {
			GdkPixbuf* full_size_image;

			full_size_image = gdk_pixbuf_new_from_file (info->thumbnail_uri + 7);
			if (full_size_image != NULL) {
				GdkPixbuf *scaled_image;
				int scaled_width, scaled_height;
				int full_width = gdk_pixbuf_get_width (full_size_image);
				int full_height = gdk_pixbuf_get_height (full_size_image);
					
				if (full_width > full_height) {
					scaled_width = 96;
					scaled_height = full_height * 96 / full_width;
				} else {
					scaled_height = 96;
					scaled_width = full_width * 96 / full_height;
				}

				scaled_image = gdk_pixbuf_scale_simple (full_size_image,
									scaled_width, scaled_height,
									ART_FILTER_BILINEAR);
					
				gdk_pixbuf_unref (full_size_image);
				if (!save_pixbuf_to_file (scaled_image, factory->new_thumbnail_path + 7))
					g_warning ("error saving thumbnail %s", factory->new_thumbnail_path + 7);	
				gdk_pixbuf_unref (scaled_image);
			}
			else {
				/* gdk-pixbuf couldn't load the image, so trying using ImageMagick */
				char *temp_str = g_strdup_printf ("png:%s", factory->new_thumbnail_path + 7);
				execlp ("convert", "convert", "-geometry",  "96x96", info->thumbnail_uri + 7, temp_str, NULL);
				g_free (temp_str);
			}
			
			_exit(0);
		}
		factory->thumbnail_in_progress = TRUE;
	}
	
	return TRUE;  /* we're not done yet */
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
}

#endif /* ! NAUTILUS_OMIT_SELF_CHECK */
