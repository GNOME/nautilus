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
  
   Authors: John Sullivan <sullivan@eazel.com>, Darin Adler <darin@eazel.com>,
	    Andy Hertzfeld <andy@eazel.com>
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
#include <libgnomevfs/gnome-vfs-mime-info.h>

#include <parser.h>
#include <xmlmemory.h>

#include <librsvg/rsvg.h>

#include "nautilus-default-file-icon.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file-utilities.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-global-preferences.h"
#include "nautilus-graphic-effects.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-link.h"
#include "nautilus-metadata.h"
#include "nautilus-string.h"
#include "nautilus-xml-extensions.h"

/* List of suffixes to search when looking for an icon file. */
static const char *icon_file_name_suffixes[] =
{
	".svg",
	".SVG",
	"",
	".png",
	".PNG",
	".gif",
	".GIF"
};
#define ICON_NAME_DIRECTORY             "i-directory"
#define ICON_NAME_DIRECTORY_CLOSED      "i-dirclosed"
#define ICON_NAME_EXECUTABLE            "i-executable"
#define ICON_NAME_REGULAR               "i-regular"
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

/* A structure to hold the icon meta-info, like the text rectangle and emblem attach points */

typedef struct {
	ArtIRect text_rect;
	gboolean has_attach_points;
	GdkPoint attach_points[MAX_ATTACH_POINTS];		
} IconInfo;

/* The key to a hash table that holds the scaled icons as pixbufs.
 * In a way, it's not really completely a key, because part of the
 * data is stored in here, including the LRU chain.
 */
typedef struct {
	NautilusScalableIcon *scalable_icon;
	IconSizeRequest size;

	NautilusCircularList recently_used_node;
	time_t	 cache_time;
	gboolean aa_mode;
	gboolean custom;
	gboolean scaled;
	IconInfo icon_info;
} IconCacheKey;

#define MINIMUM_EMBEDDED_TEXT_RECT_WIDTH	20.0
#define MINIMUM_EMBEDDED_TEXT_RECT_HEIGHT	20.0

/* forward declarations */

static void                  icon_theme_changed_callback             (gpointer                  user_data);
static GtkType               nautilus_icon_factory_get_type          (void);
static void                  nautilus_icon_factory_initialize_class  (NautilusIconFactoryClass *class);
static void                  nautilus_icon_factory_initialize        (NautilusIconFactory      *factory);
static NautilusIconFactory * nautilus_get_current_icon_factory       (void);
static char *                nautilus_icon_factory_get_thumbnail_uri (NautilusFile             *file,
								      gboolean			anti_aliased);
static NautilusIconFactory * nautilus_icon_factory_new               (const char               *theme_name);
static void                  nautilus_icon_factory_set_theme         (const char               *theme_name);
static guint                 nautilus_scalable_icon_hash             (gconstpointer             p);
static gboolean              nautilus_scalable_icon_equal            (gconstpointer             a,
								      gconstpointer             b);
static void                  icon_cache_key_destroy                  (IconCacheKey             *key);
static guint                 icon_cache_key_hash                     (gconstpointer             p);
static gboolean              icon_cache_key_equal                    (gconstpointer             a,
								      gconstpointer             b);
static gboolean              vfs_file_exists                         (const char               *file_name);
static GdkPixbuf *           get_image_from_cache                    (NautilusScalableIcon     *scalable_icon,
								      const IconSizeRequest    *size,
								      gboolean                  picky,
								      gboolean                  custom,
								      IconInfo                 *icon_info);
static gboolean              check_for_thumbnails                    (NautilusIconFactory      *factory);
static int                   nautilus_icon_factory_make_thumbnails   (gpointer                  data);
static GdkPixbuf *           load_image_with_embedded_text           (NautilusScalableIcon     *scalable_icon,
								      const IconSizeRequest    *size);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusIconFactory, nautilus_icon_factory, GTK_TYPE_OBJECT)

/* Return a pointer to the single global icon factory. */
static NautilusIconFactory *
nautilus_get_current_icon_factory (void)
{
        static NautilusIconFactory *global_icon_factory = NULL;
        if (global_icon_factory == NULL) {
		char *theme_preference;

		theme_preference = nautilus_preferences_get (NAUTILUS_PREFERENCES_THEME,
							     DEFAULT_ICON_THEME);
		g_assert (theme_preference != NULL);

                global_icon_factory = nautilus_icon_factory_new (theme_preference);
                g_free (theme_preference);

		nautilus_preferences_add_callback (NAUTILUS_PREFERENCES_THEME,
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
        factory->icon_cache = g_hash_table_new (icon_cache_key_hash,
						icon_cache_key_equal);
	
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
        icon_cache_key_destroy (key);
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

/* No one ever destroys the icon factory.
 * There's no public API for doing so.
 * If they did, we'd have to get this right.
 */

static void
nautilus_icon_factory_destroy (NautilusIconFactory *factory)
{
	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_THEME,
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
        IconCacheKey *icon_key;
	GdkPixbuf *image;

	/* Don't free a cache entry that is in the recently used list. */
	icon_key = key;
        if (icon_key->recently_used_node.next != NULL) {
                return FALSE;
	}

	/* Don't free a cache entry if the image is still in use. */
	image = value;

	/* FIXME bugzilla.eazel.com 640: 
	 * We treat all entries as "in use", until we get a hook we can use
	 * in GdkPixbuf. We are waiting for the "final" hook right now. The
	 * one that's in there is not approved of by the Gtk maintainers.
	 */
	return FALSE;
#if 0
	if (image->ref_count > 1) {
		return FALSE;
	}

	/* Free the item. */
        return nautilus_icon_factory_destroy_cached_image (key, value, NULL);
#endif
}

/* remove images whose uri field matches the passed-in uri */

static gboolean
nautilus_icon_factory_remove_image_uri (gpointer key,
					gpointer value,
					gpointer user_data)
{
        char *image_uri;
        IconCacheKey *icon_key;
	NautilusCircularList *node;
	NautilusIconFactory *factory;
	
	icon_key = key;
        image_uri = user_data;

	/* see if the the uri's match - if not, just return */
	if (icon_key->scalable_icon->uri && strcmp(icon_key->scalable_icon->uri, image_uri)) {
		return FALSE;
	}
	
	/* if it's in the recently used list, free it from there */      
	node = &icon_key->recently_used_node;
	if (node->next != NULL) {
		node->next->prev = node->prev;
		node->prev->next = node->next;
		
		factory = nautilus_get_current_icon_factory ();		
		factory->recently_used_count -= 1;
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

/* clear a specific image from the cache */

static void
nautilus_icon_factory_clear_image(const char *image_uri)
{
	NautilusIconFactory *factory;

	/* build the key and look it up in the icon cache */

	factory = nautilus_get_current_icon_factory ();
	g_hash_table_foreach_remove (factory->icon_cache,
				     nautilus_icon_factory_remove_image_uri,
				     (gpointer) image_uri);
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
        char *mime_type;
        const char *icon_name;
	gboolean is_text_file;
	
	/* force plain text files to use the generic document icon so we can have the text-in-icons feature;
	  eventually, we want to force other types of text files as well */
        mime_type = nautilus_file_get_mime_type (file);
        is_text_file = mime_type != NULL && !nautilus_strcasecmp (mime_type, "text/plain");
	
	if (mime_type != NULL && !is_text_file) {
                icon_name = gnome_vfs_mime_get_value (mime_type, "icon-filename");
		g_free (mime_type);
		if (icon_name != NULL) {
			return icon_name;
		}
	}

	/* gnome_vfs_mime didn't give us an icon name, so we have to
         * fall back on default icons.
	 */
	if (nautilus_file_is_executable (file) & !is_text_file) {
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
        case GNOME_VFS_FILE_TYPE_CHARACTER_DEVICE:
		return ICON_NAME_CHARACTER_DEVICE;
        case GNOME_VFS_FILE_TYPE_BLOCK_DEVICE:
		return ICON_NAME_BLOCK_DEVICE;
        case GNOME_VFS_FILE_TYPE_SYMBOLIC_LINK:
                return ICON_NAME_BROKEN_SYMBOLIC_LINK;
        case GNOME_VFS_FILE_TYPE_REGULAR:
        case GNOME_VFS_FILE_TYPE_UNKNOWN:
        default:
                return nautilus_icon_factory_get_icon_name_for_regular_file (file);
        }
}

static char *
make_full_icon_path (const char *path, const char *suffix)
{
	char *partial_path, *full_path;

	if (path[0] == '/') {
		return g_strconcat (path, suffix, NULL);
	}

	/* Build a path for this icon. */
	partial_path = g_strconcat (path, suffix, NULL);
	full_path = nautilus_pixmap_file (partial_path);
	g_free (partial_path);
	return full_path;
}

/* utility routine to parse the attach points string to set up the array in icon_info */
static void
parse_attach_points (IconInfo *icon_info, const char* attach_point_string)
{
	char *text_piece;
	char **point_array;
	int index, x_offset, y_offset;
				
	/* split the attach point string into a string array, then process
	   each point with sscanf in a loop */	
	point_array = g_strsplit (attach_point_string, "|", MAX_ATTACH_POINTS); 
	
	for (index = 0; (text_piece = point_array[index]) != NULL; index++) {
		if (sscanf (text_piece, " %d , %d , %*s", &x_offset, &y_offset) == 2) {
			icon_info->attach_points[index].x = x_offset;
			icon_info->attach_points[index].y = y_offset;
		} else {
			g_warning ("bad attach point specification: %s", text_piece);
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
			   IconInfo *icon_info,
			   gboolean aa_mode)
{
	int i;
	gboolean include_size;
	char *themed_icon_name, *partial_path, *path, *aa_path, *xml_path;
	xmlDocPtr doc;
	xmlNodePtr node;
	char *size_as_string, *property;
	ArtIRect parsed_rect;
	NautilusIconFactory *factory;
	
	g_assert (icon_name != NULL);

	if (theme_name == NULL || icon_name[0] == '/') {
		themed_icon_name = g_strdup (icon_name);
	} else {
		themed_icon_name = g_strconcat (theme_name, "/", icon_name, NULL);
	}

	if (icon_info != NULL) {
		icon_info->has_attach_points = FALSE;	
	}
	
	include_size = icon_size != NAUTILUS_ICON_SIZE_STANDARD;
	factory = (NautilusIconFactory*) nautilus_icon_factory_get();
	
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
			aa_path = g_strdup_printf ("%s-aa", partial_path);
			path = make_full_icon_path (aa_path,
					    icon_file_name_suffixes[i]);
			g_free (aa_path);
		
			/* Return the path if the file exists. */
			if (path != NULL && g_file_exists (path)) {
				break;
			}
			
			g_free (path);
			path = NULL;
		}
						
		path = make_full_icon_path (partial_path,
					    icon_file_name_suffixes[i]);
		g_free (partial_path);

		/* Return the path if the file exists. */
		if (path != NULL && g_file_exists (path)) {
			break;
		}
		g_free (path);
		path = NULL;
	}

	/* Open the XML file to get the text rectangle and emblem attach points */
	if (path != NULL && icon_info != NULL) {
		memset (&icon_info->text_rect, 0, sizeof (icon_info->text_rect));

		xml_path = make_full_icon_path (themed_icon_name, ".xml");

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
				icon_info->text_rect = parsed_rect;
			}
			xmlFree (property);
		}

		property = xmlGetProp (node, "ATTACH_POINTS");
		if (property != NULL) {			
			icon_info->has_attach_points = TRUE;
			parse_attach_points (icon_info, property);	
			xmlFree (property);
		} else  {
			icon_info->has_attach_points = FALSE;		
		}			
		
		xmlFreeDoc (doc);
	}

	/* if we still haven't found anything, and we're looking for an emblem,
	   check out the user's home directory, since it might be an emblem
	   that they've added there */
	   
	if (path == NULL && nautilus_str_has_prefix (icon_name, "emblem-")) {
		for (i = 0; i < NAUTILUS_N_ELEMENTS (icon_file_name_suffixes); i++) {
			char *user_directory;
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

/* Choose the file name to load, taking into account theme vs. non-theme icons. */
static char *
get_icon_file_path (const char *name,
		    const char* modifier,
		    guint size_in_pixels,
		    IconInfo *icon_info,
		    gboolean aa_mode)
{
	gboolean use_theme_icon;
	const char *theme_name;
	char *path;

	if (name == NULL) {
		return NULL;
	}

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
						  NULL, 
						  aa_mode);		
		if (path != NULL) {
			use_theme_icon = TRUE;
			g_free (path);
		}
	}

	/* Now we know whether or not to use the theme. */
	/* if there's a modifier, try using that first */
	
	if (modifier && modifier[0] != '\0') {
		char* modified_name = g_strdup_printf ("%s-%s", name, modifier);
		path = get_themed_icon_file_path (use_theme_icon ? theme_name : NULL,
						  modified_name,
						  size_in_pixels, 
						  icon_info,
						  aa_mode);
		g_free (modified_name);
		if (path != NULL) {
		    return path;
		}
	}
	
	return get_themed_icon_file_path (use_theme_icon ? theme_name : NULL,
					  name,
					  size_in_pixels,
					  icon_info,
					  aa_mode);
}

static void
icon_theme_changed_callback (gpointer user_data)
{
	char *theme_preference;

	theme_preference = nautilus_preferences_get (NAUTILUS_PREFERENCES_THEME,
						     DEFAULT_ICON_THEME);

	g_assert (theme_preference != NULL);

	nautilus_icon_factory_set_theme (theme_preference);

	g_free (theme_preference);

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
	NautilusScalableIcon icon_key, *icon;
	NautilusIconFactory *factory;
	
	factory = (NautilusIconFactory*) nautilus_icon_factory_get ();
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
	hash_table = nautilus_get_current_icon_factory ()->scalable_icons;

	/* Check to see if it's already in the table. */
	icon_key.uri = (char *) uri;
	icon_key.name = (char *) name;
	icon_key.modifier = (char *) modifier;
	icon_key.embedded_text = (char *) embedded_text;
	icon_key.aa_mode = anti_aliased;
	
	icon = g_hash_table_lookup (hash_table, &icon_key);
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

	hash_table = nautilus_get_current_icon_factory ()->scalable_icons;
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
	char *uri, *file_uri, *image_uri, *icon_name, *mime_type, *top_left_text;
 	NautilusScalableIcon *scalable_icon;
	
	if (file == NULL) {
		return NULL;
	}

	/* if there is a custom image in the metadata, use that. */
	uri = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL);
	file_uri = nautilus_file_get_uri (file);
	
	/* if the file is an image, either use the image itself as the icon if it's small enough,
	   or use a thumbnail if one exists.  If a thumbnail is required, but does not yet exist,
	   put an entry on the thumbnail queue so we eventually make one */

	/* also, dont make thumbnails for images in the thumbnails directory */  
	if (uri == NULL) {		
		mime_type = nautilus_file_get_mime_type (file);
		if (nautilus_istr_has_prefix (mime_type, "image/") && should_display_image_file_as_itself (file)) {
			if (nautilus_file_get_size (file) < SELF_THUMBNAIL_SIZE_THRESHOLD) {
				uri = nautilus_file_get_uri (file);				
			} else if (strstr(file_uri, "/.thumbnails/") == NULL) {
				uri = nautilus_icon_factory_get_thumbnail_uri (file, anti_aliased);
			}
		}
		g_free (mime_type);		
	}
	
	/* Handle nautilus link xml files, which may specify their own image */	
	icon_name = NULL;
	if (nautilus_link_is_link_file (file)) {
		/* FIXME: This does sync. I/O. */
		image_uri = nautilus_link_get_image_uri (file_uri);
		if (image_uri != NULL) {
			/* FIXME: Lame hack. We only support file:// URIs? */
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
				     NAUTILUS_FILE_ATTRIBUTE_FAST_MIME_TYPE);
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
nautilus_icon_factory_get_emblem_icons_for_file (NautilusFile *file, gboolean anti_aliased)
{
	GList *icons, *emblem_names, *p;
	NautilusScalableIcon *icon;

	icons = NULL;

	emblem_names = nautilus_file_get_emblem_names (file);
	for (p = emblem_names; p != NULL; p = p->next) {
		icon = nautilus_icon_factory_get_emblem_icon_by_name (p->data, anti_aliased);
		icons = g_list_prepend (icons, icon);
	}
	nautilus_g_list_free_deep (emblem_names);

	return g_list_reverse (icons);
}

/* utility to test whether a file exists using vfs */
static gboolean
vfs_file_exists (const char *file_uri)
{
	GnomeVFSResult result;
	GnomeVFSFileInfo *file_info;

	file_info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (file_uri, file_info, 0);
	gnome_vfs_file_info_unref (file_info);
	return result == GNOME_VFS_OK;
}

/* utility copied from Nautilus directory */

static GnomeVFSResult
nautilus_make_directory_and_parents (GnomeVFSURI *uri, guint permissions)
{
	GnomeVFSResult result;
	GnomeVFSURI *parent_uri;

	/* Make the directory, and return right away unless there's
	   a possible problem with the parent.
	*/
	result = gnome_vfs_make_directory_for_uri (uri, permissions);
	if (result != GNOME_VFS_ERROR_NOT_FOUND) {
		return result;
	}

	/* If we can't get a parent, we are done. */
	parent_uri = gnome_vfs_uri_get_parent (uri);
	if (parent_uri == NULL) {
		return result;
	}

	/* If we can get a parent, use a recursive call to create
	   the parent and its parents.
	*/
	result = nautilus_make_directory_and_parents (parent_uri, permissions);
	gnome_vfs_uri_unref (parent_uri);
	if (result != GNOME_VFS_OK) {
		return result;
	}

	/* A second try at making the directory after the parents
	   have all been created.
	*/
	result = gnome_vfs_make_directory_for_uri (uri, permissions);
	return result;
}

/* utility routine that, given the uri of an image, constructs the uri to the corresponding thumbnail */

static char *
make_thumbnail_path (const char *image_uri, gboolean directory_only, gboolean use_local_directory, gboolean anti_aliased)
{
	char *thumbnail_uri, *thumbnail_path;
	char *directory_name = g_strdup (image_uri);
	char *last_slash = strrchr (directory_name, '/');
	char *dot_pos;
	
	*last_slash = '\0';
	
	/* either use the local directory or one in the user's home directory, as selected by the passed in flag */
	if (use_local_directory)
		thumbnail_uri =  g_strdup_printf ("%s/.thumbnails", directory_name);
	else  {
		GnomeVFSResult result;
		GnomeVFSURI  *thumbnail_directory_uri;
	        	
	        char *escaped_uri = nautilus_str_escape_slashes (directory_name);		
		thumbnail_path = g_strdup_printf ("%s/.nautilus/thumbnails/%s", g_get_home_dir(), escaped_uri);
		thumbnail_uri = nautilus_get_uri_from_local_path (thumbnail_path);
		g_free (thumbnail_path);
		g_free(escaped_uri);
		
		/* we must create the directory if it doesnt exist */
			
		thumbnail_directory_uri = gnome_vfs_uri_new (thumbnail_uri);
		result = nautilus_make_directory_and_parents (thumbnail_directory_uri, THUMBNAIL_DIR_PERMISSIONS);
		gnome_vfs_uri_unref (thumbnail_directory_uri);
	}
	
	/* append the file name if necessary */
	if (!directory_only) {
		char* old_uri = thumbnail_uri;
		thumbnail_uri = g_strdup_printf ("%s/%s", thumbnail_uri, last_slash + 1);
		g_free(old_uri);			
	
		/* append the anti-aliased suffix if necessary */
		if (anti_aliased) {
			char *old_uri = thumbnail_uri;
			dot_pos = strrchr (thumbnail_uri, '.');
			if (dot_pos) {
				*dot_pos = '\0';
				thumbnail_uri = g_strdup_printf ("%s.aa.%s", old_uri, dot_pos + 1);
			} else {
				thumbnail_uri = g_strconcat (old_uri, ".aa", NULL);				
			}
			g_free (old_uri);
		}
		
		/* append an image suffix if the correct one isn't already present */
		if (!nautilus_istr_has_suffix (image_uri, ".png")) {		
			char* old_uri = thumbnail_uri;
			thumbnail_uri = g_strdup_printf ("%s.png", thumbnail_uri);
			g_free(old_uri);			
		}
	}
			
	g_free (directory_name);
	return thumbnail_uri;
}

/* utility routine that takes two uris and returns true if the first file has been modified later than the second */
/* FIXME: it makes synchronous file info calls, so for now, it returns FALSE if either of the uri's are non-local */
static gboolean
first_file_more_recent(const char* file_uri, const char* other_file_uri)
{
	GnomeVFSURI *vfs_uri, *other_vfs_uri;
	gboolean more_recent, is_local;
	
	GnomeVFSFileInfo file_info, other_file_info;

	/* if either file is remote, return FALSE.  Eventually we'll make this async to fix this */
	vfs_uri = gnome_vfs_uri_new(file_uri);
	other_vfs_uri = gnome_vfs_uri_new(other_file_uri);
	is_local = gnome_vfs_uri_is_local (vfs_uri) && gnome_vfs_uri_is_local (other_vfs_uri);
	gnome_vfs_uri_unref(vfs_uri);
	gnome_vfs_uri_unref(other_vfs_uri);
	
	if (!is_local) {
		return FALSE;
	}
	
	/* gather the info and then compare modification times */
	gnome_vfs_file_info_init (&file_info);
	gnome_vfs_get_file_info (file_uri, &file_info, GNOME_VFS_FILE_INFO_DEFAULT);
	
	gnome_vfs_file_info_init (&other_file_info);
	gnome_vfs_get_file_info (other_file_uri, &other_file_info, GNOME_VFS_FILE_INFO_DEFAULT);

	more_recent = file_info.mtime > other_file_info.mtime;

	gnome_vfs_file_info_clear (&file_info);
	gnome_vfs_file_info_clear (&other_file_info);

	return more_recent;
}

/* structure used for making thumbnails, associating a uri with where the thumbnail is to be stored */

typedef struct {
	char *thumbnail_uri;
	gboolean is_local;
	gboolean anti_aliased;
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
/* FIXME bugzilla.eazel.com 642: 
 * Most of this thumbnail machinery belongs in NautilusFile, not here.
 */

static char *
nautilus_icon_factory_get_thumbnail_uri (NautilusFile *file, gboolean anti_aliased)
{
	NautilusIconFactory *factory;
	GnomeVFSResult result;
	char *thumbnail_uri;
	char *file_uri;
	gboolean local_flag = TRUE;
	gboolean  remake_thumbnail = FALSE;
	
	file_uri = nautilus_file_get_uri (file);
	
	/* compose the uri for the thumbnail locally */
	thumbnail_uri = make_thumbnail_path (file_uri, FALSE, TRUE, anti_aliased);
		
	/* if the thumbnail file already exists locally, simply return the uri */
	if (vfs_file_exists (thumbnail_uri)) {
		
		/* see if the file changed since it was thumbnailed by comparing the modification time */
		remake_thumbnail = first_file_more_recent(file_uri, thumbnail_uri);
		
		/* if the file hasn't changed, return the thumbnail uri */
		if (!remake_thumbnail) {
			g_free (file_uri);
			return thumbnail_uri;
		} else {
			nautilus_icon_factory_clear_image(thumbnail_uri);
			gnome_vfs_unlink(thumbnail_uri);
		}
	}
	
	/* now try it globally */
	if (!remake_thumbnail) {
		g_free (thumbnail_uri);
		thumbnail_uri = make_thumbnail_path (file_uri, FALSE, FALSE, anti_aliased);
		
		/* if the thumbnail file already exists in the common area,  return that uri */
		if (vfs_file_exists (thumbnail_uri)) {
		
			/* see if the file changed since it was thumbnailed by comparing the modification time */
			remake_thumbnail = first_file_more_recent(file_uri, thumbnail_uri);
		
			/* if the file hasn't changed, return the thumbnail uri */
			if (!remake_thumbnail) {
				g_free (file_uri);
				return thumbnail_uri;
			} else {
				nautilus_icon_factory_clear_image(thumbnail_uri);
				gnome_vfs_unlink(thumbnail_uri);
			}
		}
	}
	
        /* make the thumbnail directory if necessary, at first try it locally */
	g_free (thumbnail_uri);
	local_flag = TRUE;
	thumbnail_uri = make_thumbnail_path (file_uri, TRUE, local_flag, anti_aliased);
	result = gnome_vfs_make_directory (thumbnail_uri, THUMBNAIL_DIR_PERMISSIONS);

	/* if we can't make if locally, try it in the global place */
	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_FILE_EXISTS) {	
		g_free (thumbnail_uri);
		local_flag = FALSE;
		thumbnail_uri = make_thumbnail_path (file_uri, TRUE, local_flag, anti_aliased);
		result = gnome_vfs_make_directory (thumbnail_uri, THUMBNAIL_DIR_PERMISSIONS);	
	}
	
	/* the thumbnail needs to be created (or recreated), so add an entry to the thumbnail list */
 
	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_FILE_EXISTS) {

		g_warning ("error when making thumbnail directory %d, for %s", result, thumbnail_uri);	
	} else {
		NautilusThumbnailInfo *info = g_new0 (NautilusThumbnailInfo, 1);
		info->thumbnail_uri = file_uri;
		info->is_local = local_flag;
		info->anti_aliased = anti_aliased;
		
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
				   NULL,
				   NAUTILUS_ICON_SIZE_STANDARD,
				   NULL,
				   FALSE);
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
load_specific_image_svg (const char *path, guint size_in_pixels)
{
	FILE *f;
	GdkPixbuf *result;

	f = fopen (path, "rb");
	if (f == NULL) {
		return NULL;
	}
	result = rsvg_render_file (f, size_in_pixels *
				   (1.0 / NAUTILUS_ICON_SIZE_STANDARD));
	fclose (f);

	return result;
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

	uri = nautilus_get_uri_from_local_path (path);
	gnome_vfs_file_info_init (&file_info);
	gnome_vfs_get_file_info (uri, &file_info, GNOME_VFS_FILE_INFO_GET_MIME_TYPE);
	g_free (uri);
	is_svg = nautilus_strcmp (file_info.mime_type, "image/svg") == 0;
	gnome_vfs_file_info_clear (&file_info);

	return is_svg;
}

/* This load function returns NULL if the icon is not available at this size. */
static GdkPixbuf *
load_specific_image (NautilusScalableIcon *scalable_icon,
		     guint size_in_pixels,
		     gboolean custom,
		     IconInfo *icon_info)
{
	char *image_path;
	GdkPixbuf *pixbuf;
	
	g_assert (icon_info != NULL);

	if (custom) {
		/* Custom icon. */

		memset (&icon_info->text_rect, 0, sizeof (icon_info->text_rect));
		icon_info->has_attach_points = FALSE;
		
		/* FIXME bugzilla.eazel.com 643: we can't load svgs asynchronously, so this
		 *  only works for local files
		 */
		
		/* we use the suffix instead of mime-type here since it may be non-local */	
		if (nautilus_istr_has_suffix (scalable_icon->uri, ".svg")) {
			image_path = nautilus_get_local_path_from_uri (scalable_icon->uri);		
			pixbuf = load_specific_image_svg (image_path, size_in_pixels);
			g_free (image_path);
			return pixbuf;
		}
		
		if (size_in_pixels == NAUTILUS_ICON_SIZE_STANDARD && scalable_icon->uri != NULL) {
			return nautilus_gdk_pixbuf_load (scalable_icon->uri);
		}
		
		return NULL;
	} else {
		/* Standard icon. */
		char *path;
		GdkPixbuf *image;
		
		path = get_icon_file_path (scalable_icon->name,
					   scalable_icon->modifier,
					   size_in_pixels,
					   icon_info,
					   scalable_icon->aa_mode);
					   		
		if (path == NULL) {
			return NULL;
		}

		if (path_represents_svg_image (path)) {
			image = load_specific_image_svg (path, size_in_pixels);
		} else {
			image = gdk_pixbuf_new_from_file (path);
		}

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
			IconInfo *icon_info)
{
        GdkPixbuf *image;
	guint actual_size;
	IconSizeRequest size_request;
	static GdkPixbuf *fallback_image;

	size_request.maximum_width = MAXIMUM_ICON_SIZE * requested_size / NAUTILUS_ZOOM_LEVEL_STANDARD;
	size_request.maximum_height = size_request.maximum_width;

	/* First check for a custom image. */
	actual_size = 0;
	while (get_next_icon_size_to_try (requested_size, &actual_size)) {
		size_request.nominal_width = actual_size;
		size_request.nominal_height = actual_size;

		image = get_image_from_cache (scalable_icon,
					      &size_request,
					      TRUE,
					      TRUE,
					      icon_info);
		if (image != NULL) {
			*actual_size_result = actual_size;
			*custom = TRUE;
			return image;
		}
	}
	
	/* Next, go for the normal image. */
	actual_size = 0;
	while (get_next_icon_size_to_try (requested_size, &actual_size)) {
		size_request.nominal_width = actual_size;
		size_request.nominal_height = actual_size;

		image = get_image_from_cache (scalable_icon,
					      &size_request,
					      TRUE,
					      FALSE,
					      icon_info);
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
			 GDK_COLORSPACE_RGB,
			 TRUE,
			 8,
			 nautilus_default_file_icon_width,
			 nautilus_default_file_icon_height,
			 nautilus_default_file_icon_width * 4, /* stride */
			 NULL, /* don't destroy data */
			 NULL);
	}
	gdk_pixbuf_ref (fallback_image);

	memset (&icon_info->text_rect, 0, sizeof (icon_info->text_rect));
	*actual_size_result = NAUTILUS_ICON_SIZE_STANDARD;
	*custom = FALSE;
        return fallback_image;
}

/* Consumes the image and returns a scaled one if the image is too big.
 * Note that this does an unref on the image and returns a new one.
 */
static GdkPixbuf *
scale_image_and_info (GdkPixbuf *image,
			   IconInfo *icon_info,
			   double scale_x,
			   double scale_y)
{
	int width, height;
	int rect_width, rect_height;
	int index;
	GdkPixbuf *scaled_image;

	width = gdk_pixbuf_get_width (image);
	height = gdk_pixbuf_get_height (image);

	/* Check for no-scaling case. */
	if ((int) (width * scale_x) == width
	    && (int) (height * scale_y) == height) {
		return gdk_pixbuf_ref (image);
	}

	width *= scale_x;
	if (width < 1) {
		width = 1;
	}
	height *= scale_y;
	if (height < 1) {
		height = 1;
	}

	rect_width = (icon_info->text_rect.x1 - icon_info->text_rect.x0) * scale_x;
	rect_height = (icon_info->text_rect.y1 - icon_info->text_rect.y0) * scale_y;
	
	scaled_image = gdk_pixbuf_scale_simple
		(image, width, height, GDK_INTERP_BILINEAR);
	gdk_pixbuf_unref (image);

	icon_info->text_rect.x0 *= scale_x;
	icon_info->text_rect.y0 *= scale_y;
	icon_info->text_rect.x1 = icon_info->text_rect.x0 + rect_width;
	icon_info->text_rect.y1 = icon_info->text_rect.y0 + rect_height;

	if (icon_info->has_attach_points) {
		for (index = 0; index < MAX_ATTACH_POINTS; index++) {
			icon_info->attach_points[index].x *= scale_x;
			icon_info->attach_points[index].y *= scale_y;
		}
	}
	
	return scaled_image;
}

static void
revise_scale_factors_if_too_big (GdkPixbuf *image,
				 const IconSizeRequest *size,
				 double *scale_x,
				 double *scale_y)
{
	int width, height;
	double y_distortion;

	width = gdk_pixbuf_get_width (image);
	height = gdk_pixbuf_get_height (image);

	if ((int) (width * *scale_x) <= size->maximum_width
	    && (int) (height * *scale_y) <= size->maximum_height) {
		return;
	}

	y_distortion = *scale_y / *scale_x;

	*scale_x = MIN ((double) size->maximum_width / width,
			(double) size->maximum_height / (height / y_distortion));
	*scale_y = *scale_x * y_distortion;
}

/* Consumes the image and returns a scaled one if the image is too big.
 * Note that this does an unref on the image and returns a new one.
 */
static GdkPixbuf *
scale_image_down_if_too_big (GdkPixbuf *image,
			     const IconSizeRequest *size,
			     IconInfo *icon_info)
{
	double scale_x, scale_y;

	scale_x = 1.0;
	scale_y = 1.0;
	revise_scale_factors_if_too_big (image, size, &scale_x, &scale_y);
	return scale_image_and_info (image, icon_info, scale_x, scale_y);
}

/* This load function is not allowed to return NULL. */
static GdkPixbuf *
load_image_scale_if_necessary (NautilusScalableIcon *scalable_icon,
			       const IconSizeRequest *size,
			       gboolean *scaled,
			       gboolean *custom,
			       IconInfo *icon_info)
{
        GdkPixbuf *image;
	guint nominal_actual_size;
	double scale_x, scale_y;
	
	/* Load the image for the icon that's closest in size to what we want. */
	image = load_image_for_scaling (scalable_icon, size->nominal_width,
					&nominal_actual_size, custom, icon_info);
        if (size->nominal_width == nominal_actual_size
	    && size->nominal_height == nominal_actual_size) {
		*scaled = FALSE;
		return scale_image_down_if_too_big (image, size, icon_info);
	}
	
	/* Scale the image to the size we want. */
	*scaled = TRUE;
	scale_x = (double) size->nominal_width / nominal_actual_size;
	scale_y = (double) size->nominal_height / nominal_actual_size;
	revise_scale_factors_if_too_big (image, size, &scale_x, &scale_y);
	return scale_image_and_info (image, icon_info, scale_x, scale_y);
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

/* utility routine that checks if a cached image has changed since it was cached.
 * It returns TRUE if the image is still valid, and removes it from the cache if it's not */
 
 static gboolean
 cached_image_still_valid (const char *file_uri, time_t cached_time)
 {
	GnomeVFSURI *vfs_uri;
	GnomeVFSFileInfo file_info;
	gboolean is_local, is_valid;

	/* if there's no specific file, simply return TRUE */
	if (file_uri == NULL)
		return TRUE;
		
	/* FIXME: if the URI is remote, assume it's valid to avoid delay of testing.  Eventually we'll make this async to fix this */
	vfs_uri = gnome_vfs_uri_new(file_uri);
	is_local = gnome_vfs_uri_is_local (vfs_uri);
	gnome_vfs_uri_unref(vfs_uri);	
	if (!is_local) {
		return TRUE;
	}
	
	/* gather the info and then compare modification times */
	gnome_vfs_file_info_init (&file_info);
	gnome_vfs_get_file_info (file_uri, &file_info, GNOME_VFS_FILE_INFO_DEFAULT);
	
	is_valid = file_info.mtime <= cached_time;
	gnome_vfs_file_info_clear (&file_info);

	/* if it's not valid, remove it from the cache */
	if (!is_valid) {
		nautilus_icon_factory_clear_image (file_uri);	
	}
	
	return is_valid;
 }
 
/* Get the image for icon, handling the caching.
 * If @picky is true, then only an unscaled icon is acceptable.
 * Also, if @picky is true, the icon must be a custom icon if
 * @custom is true or a standard icon is @custom is false.
 */
static GdkPixbuf *
get_image_from_cache (NautilusScalableIcon *scalable_icon,
		      const IconSizeRequest *size,
		      gboolean picky,
		      gboolean custom,
		      IconInfo *icon_info)
{
	NautilusIconFactory *factory;
	GHashTable *hash_table;
	IconCacheKey lookup_key, *key;
        GdkPixbuf *image;
	gpointer key_in_table, value;
	gboolean found_image;
	
	g_return_val_if_fail (scalable_icon != NULL, NULL);

	factory = nautilus_get_current_icon_factory ();
	hash_table = factory->icon_cache;

	/* Check to see if it's already in the table. */
	lookup_key.scalable_icon = scalable_icon;
	lookup_key.size = *size;
	found_image = FALSE;

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
		found_image = cached_image_still_valid (scalable_icon->uri, key->cache_time);
		g_assert (image != NULL);
	}
	
	if (!found_image)
		{
		gboolean got_scaled_image;
		gboolean got_custom_image;
		IconInfo key_icon_info;
		
		key_icon_info.has_attach_points = FALSE;
		
		/* Not in the table, so load the image. */
		/* If we're picky, then we want the image only if this exact
		 * nominal size is available.
		 */
		if (picky) {
			g_assert (scalable_icon->embedded_text == NULL);

			/* Actual icons have nominal sizes that are square! */
			if (size->nominal_width
			    != size->nominal_height) {
				return NULL;
			}

			/* Get the image. */
			image = load_specific_image (scalable_icon,
						     size->nominal_width,
						     custom,
						     &key_icon_info);
			if (image == NULL) {
				return NULL;
			}

			/* Now we have the image, but is it bigger than
			 * the maximum size? If so we scale it, even but we don't
			 * call it "scaled" for caching purposese.
			 */
			image = scale_image_down_if_too_big (image, size, &key_icon_info);

			got_scaled_image = FALSE;
			got_custom_image = custom;
		} else {
			if (scalable_icon->embedded_text != NULL) {
				image = load_image_with_embedded_text (scalable_icon, size);

				/* None of these matters for an icon with text already embedded.
				 * So we fill in with arbitrary values.
				 */
				got_scaled_image = FALSE;
				got_custom_image = FALSE;
				memset (&key_icon_info.text_rect, 0, sizeof (key_icon_info.text_rect));
			} else {
				image = load_image_scale_if_necessary
					(scalable_icon,
					 size,
					 &got_scaled_image,
					 &got_custom_image,
					 &key_icon_info);
			}
			g_assert (image != NULL);
		}

		/* Add the embedded text. */

		/* Create the key for the table. */
		key = g_new0 (IconCacheKey, 1);
		nautilus_scalable_icon_ref (scalable_icon);
		key->scalable_icon = scalable_icon;
		key->size = *size;
		key->scaled = got_scaled_image;
		key->custom = got_custom_image;
		key->icon_info = key_icon_info;
		key->cache_time = time(NULL);
		
		/* Add the item to the hash table. */
		g_hash_table_insert (hash_table, key, image);
	}

	/* Return the icon info if the caller asked for it. */
	if (icon_info != NULL) {
		*icon_info = key->icon_info;
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
					   guint nominal_width,
					   guint nominal_height,
					   guint maximum_width,
					   guint maximum_height,
					   EmblemAttachPoints *attach_data)
{
	IconSizeRequest size;
	IconInfo icon_info;
	GdkPixbuf *pixbuf;
	int index;
	
	size.nominal_width = nominal_width;
	size.nominal_height = nominal_width;
	size.maximum_width = maximum_width;
	size.maximum_height = maximum_height;
	pixbuf = get_image_from_cache (scalable_icon, &size,
				     FALSE, scalable_icon->uri != NULL,
				     &icon_info);
	if (attach_data != NULL) {
		attach_data->has_attach_points = icon_info.has_attach_points;
		for (index = 0; index < MAX_ATTACH_POINTS; index++) {
			attach_data->attach_points[index] = icon_info.attach_points[index];
		}
	}
	
	return pixbuf;
}


static void
icon_cache_key_destroy (IconCacheKey *key)
{
	nautilus_scalable_icon_unref (key->scalable_icon);
}

static guint
icon_cache_key_hash (gconstpointer p)
{
	const IconCacheKey *key;

	key = p;
	return (((((((GPOINTER_TO_UINT (key->scalable_icon) << 4)
		     ^ key->size.nominal_width) << 4)
		   ^ key->size.nominal_height) << 4)
		 ^ key->size.maximum_width) << 4)
		^ key->size.maximum_height;
}

static gboolean
icon_cache_key_equal (gconstpointer a, gconstpointer b)
{
	const IconCacheKey *key_a, *key_b;

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
	gdk_pixbuf_render_pixmap_and_mask (pixbuf, pixmap, mask, 128);
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

static GdkPixbuf *
embed_text (GdkPixbuf *pixbuf_without_text,
	    const ArtIRect *embedded_text_rect,
	    const char *text)
{

	static GdkFont *font;

	GdkPixbuf *pixbuf_with_text;

	g_return_val_if_fail (pixbuf_without_text != NULL, NULL);
	g_return_val_if_fail (embedded_text_rect != NULL, gdk_pixbuf_ref (pixbuf_without_text));

	/* Get the font the first time through. */
	if (font == NULL) {
		/* FIXME bugzilla.eazel.com 1102: Embedded text should use preferences to determine
		 * the font it uses
		 */
		
		/* for anti-aliased text, we choose a large font and scale it down */
		font = gdk_font_load ("-*-helvetica-medium-r-normal-*-24-*-*-*-*-*-*-*");
		g_return_val_if_fail (font != NULL, gdk_pixbuf_ref (pixbuf_without_text));
	}

	/* Quick out for the case where there's no place to embed the
	 * text or the place is too small or there's no text.
	 */
	if (!embedded_text_rect_usable (embedded_text_rect) || nautilus_strlen (text) == 0) {
		return gdk_pixbuf_ref (pixbuf_without_text);
	}

	pixbuf_with_text = gdk_pixbuf_copy (pixbuf_without_text);

	nautilus_gdk_pixbuf_draw_text (pixbuf_with_text, font, .3333, embedded_text_rect, text, 0xFF);
	return pixbuf_with_text;
}

static GdkPixbuf *
load_image_with_embedded_text (NautilusScalableIcon *scalable_icon,
			       const IconSizeRequest *size)
{
	NautilusScalableIcon *scalable_icon_without_text;
	GdkPixbuf *pixbuf_without_text, *pixbuf;
	IconInfo icon_info;

	g_assert (scalable_icon->embedded_text != NULL);

	scalable_icon_without_text = nautilus_scalable_icon_new_from_text_pieces
		(scalable_icon->uri,
		 scalable_icon->name,
		 scalable_icon->modifier,
		 NULL,
		 scalable_icon->aa_mode);
	
	pixbuf_without_text = get_image_from_cache
		(scalable_icon_without_text, size,
		 FALSE, FALSE, &icon_info);
	nautilus_scalable_icon_unref (scalable_icon_without_text);
	
	pixbuf = embed_text (pixbuf_without_text,
			     &icon_info. text_rect,
			     scalable_icon->embedded_text);
	gdk_pixbuf_unref (pixbuf_without_text);

	return pixbuf;
}

/* Convenience function for unrefing and then freeing an entire list. */
void
nautilus_scalable_icon_list_free (GList *icon_list)
{
	nautilus_g_list_free_deep_custom (icon_list, (GFunc) nautilus_scalable_icon_unref, NULL);
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
		current_thumbnail = make_thumbnail_path (info->thumbnail_uri, FALSE, info->is_local, info->anti_aliased);
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

static GdkPixbuf*
load_thumbnail_frame (gboolean anti_aliased)
{
	char *image_path;
	GdkPixbuf *frame_image;
				
	/* load the thumbnail frame */
	image_path = nautilus_pixmap_file (anti_aliased ? "thumbnail_frame.aa.png" : "thumbnail_frame.png");
	frame_image = gdk_pixbuf_new_from_file (image_path);
	g_free (image_path);
	return frame_image;
}

static int
nautilus_icon_factory_make_thumbnails (gpointer data)
{
	pid_t thumbnail_pid;
	NautilusThumbnailInfo *info;
	NautilusIconFactory *factory = nautilus_get_current_icon_factory();
	GList *next_thumbnail = factory->thumbnails;
	GdkPixbuf *scaled_image, *framed_image, *thumbnail_image_frame;
	
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
		factory->new_thumbnail_path = make_thumbnail_path (info->thumbnail_uri, FALSE, info->is_local, info->anti_aliased);

		/* fork a task to make the thumbnail, using gdk-pixbuf to do the scaling */
		if (!(thumbnail_pid = fork())) {
			GdkPixbuf* full_size_image;
			NautilusFile *file;
			char *thumbnail_path;
			
			file = nautilus_file_get (info->thumbnail_uri);
			full_size_image = NULL;

			if (nautilus_file_is_mime_type (file, "image/svg")) {
				thumbnail_path = nautilus_get_local_path_from_uri (info->thumbnail_uri);
				if (thumbnail_path != NULL) {
					FILE *f = fopen (thumbnail_path, "rb");
					if (f != NULL) {
						full_size_image = rsvg_render_file (f, 1.0);
						fclose (f);
					}
				}
			} else  {
				if (info->thumbnail_uri != NULL)
					full_size_image = nautilus_gdk_pixbuf_load (info->thumbnail_uri);						
			}
			nautilus_file_unref (file);
			
			if (full_size_image != NULL) {				
				thumbnail_image_frame = load_thumbnail_frame(info->anti_aliased);
									
				/* scale the content image as necessary */	
				scaled_image = nautilus_gdk_pixbuf_scale_to_fit(full_size_image, 96, 96);	
				
				/* embed the content image in the frame */
				/* FIXME: the offset numbers are dependent on the frame image - we need to make them adjustable */
				framed_image = nautilus_embed_image_in_frame (scaled_image, thumbnail_image_frame, 3, 3, 6, 6);
				
				gdk_pixbuf_unref (scaled_image);
				gdk_pixbuf_unref (thumbnail_image_frame);
				
				thumbnail_path = nautilus_get_local_path_from_uri (factory->new_thumbnail_path);			
				if (!save_pixbuf_to_file (framed_image, thumbnail_path)) {
					g_warning ("error saving thumbnail %s", thumbnail_path);
				}
				g_free (thumbnail_path);
				gdk_pixbuf_unref (framed_image);
			}
			else {
				/* gdk-pixbuf couldn't load the image, so trying using ImageMagick */
				char *temp_str;
				thumbnail_path = nautilus_get_local_path_from_uri (factory->new_thumbnail_path);
				temp_str = g_strdup_printf ("png:%s", thumbnail_path);
				g_free (thumbnail_path);
				
				thumbnail_path = nautilus_get_local_path_from_uri (info->thumbnail_uri);
				
				/* scale the image */
				execlp ("convert", "convert", "-geometry",  "96x96", thumbnail_path, temp_str, NULL);
				
				/* we don't come back from this call, so no point in freeing anything up */
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
