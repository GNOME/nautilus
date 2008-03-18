/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-thumbnails.h: Thumbnail code for icon factory.
 
   Copyright (C) 2000, 2001 Eazel, Inc.
   Copyright (C) 2002, 2003 Red Hat, Inc.
  
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
  
   Author: Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include "nautilus-thumbnails.h"

#include "nautilus-directory-notify.h"
#include "nautilus-global-preferences.h"
#include "nautilus-file-utilities.h"
#include <math.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtkmain.h>
#include <librsvg/rsvg.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <libgnomeui/gnome-thumbnail.h>

#include "nautilus-file-private.h"

/* turn this on to see messages about thumbnail creation */
#if 0
#define DEBUG_THUMBNAILS
#endif

/* Should never be a reasonable actual mtime */
#define INVALID_MTIME 0

/* Cool-off period between last file modification time and thumbnail creation */
#define THUMBNAIL_CREATION_DELAY_SECS 3

static gpointer thumbnail_thread_start (gpointer data);

/* structure used for making thumbnails, associating a uri with where the thumbnail is to be stored */

typedef struct {
	char *image_uri;
	char *mime_type;
	time_t original_file_mtime;
} NautilusThumbnailInfo;

struct NautilusThumbnailAsyncLoadHandle {
	GCancellable *cancellable;
	char *file_path;
	guint base_size;
	guint nominal_size;
	gboolean force_nominal; 
	NautilusThumbnailAsyncLoadFunc load_func;
	gpointer load_func_user_data;
};


/*
 * Thumbnail thread state.
 */

/* The id of the idle handler used to start the thumbnail thread, or 0 if no
   idle handler is currently registered. */
static guint thumbnail_thread_starter_id = 0;

/* Our mutex used when accessing data shared between the main thread and the
   thumbnail thread, i.e. the thumbnail_thread_is_running flag and the
   thumbnails_to_make list. */
static pthread_mutex_t thumbnails_mutex = PTHREAD_MUTEX_INITIALIZER;

/* A flag to indicate whether a thumbnail thread is running, so we don't
   start more than one. Lock thumbnails_mutex when accessing this. */
static volatile gboolean thumbnail_thread_is_running = FALSE;

/* Added in glib 2.14 */
#ifndef G_QUEUE_INIT
#define G_QUEUE_INIT { NULL, NULL, 0 }
#endif

/* The list of NautilusThumbnailInfo structs containing information about the
   thumbnails we are making. Lock thumbnails_mutex when accessing this. */
static volatile GQueue thumbnails_to_make = G_QUEUE_INIT;

/* Quickly check if uri is in thumbnails_to_make list */
static GHashTable *thumbnails_to_make_hash = NULL;

/* The currently thumbnailed icon. it also exists in the thumbnails_to_make list
 * to avoid adding it again. Lock thumbnails_mutex when accessing this. */
static NautilusThumbnailInfo *currently_thumbnailing = NULL;

static GnomeThumbnailFactory *thumbnail_factory = NULL;

static const char *types [] = {
	"image/x-bmp", "image/x-ico", "image/jpeg", "image/gif",
	"image/png", "image/pnm", "image/ras", "image/tga",
	"image/tiff", "image/wbmp", "image/bmp", "image/x-xbitmap",
	"image/x-xpixmap"
};
static GHashTable *image_mime_types = NULL;

static int thumbnail_icon_size = 0;

static gboolean
get_file_mtime (const char *file_uri, time_t* mtime)
{
	GFile *file;
	GFileInfo *info;

	*mtime = INVALID_MTIME;

	file = g_file_new_for_uri (file_uri);
	info = g_file_query_info (file, G_FILE_ATTRIBUTE_TIME_MODIFIED, 0, NULL, NULL);
	if (info) {
		*mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
		g_object_unref (info);
	}
	g_object_unref (file);
	
	return TRUE;
}

static void
free_thumbnail_info (NautilusThumbnailInfo *info)
{
	g_free (info->image_uri);
	g_free (info->mime_type);
	g_free (info);
}

static GnomeThumbnailFactory *
get_thumbnail_factory (void)
{
	static GnomeThumbnailFactory *thumbnail_factory = NULL;

	if (thumbnail_factory == NULL) {
		thumbnail_factory = gnome_thumbnail_factory_new (GNOME_THUMBNAIL_SIZE_NORMAL);
	}

	return thumbnail_factory;
}


/* This function is added as a very low priority idle function to start the
   thread to create any needed thumbnails. It is added with a very low priority
   so that it doesn't delay showing the directory in the icon/list views.
   We want to show the files in the directory as quickly as possible. */
static gboolean
thumbnail_thread_starter_cb (gpointer data)
{
	pthread_attr_t thread_attributes;
	pthread_t thumbnail_thread;

	/* Don't do this in thread, since g_object_ref is not threadsafe */
	if (thumbnail_factory == NULL) {
		thumbnail_factory = get_thumbnail_factory ();
	}

	/* We create the thread in the detached state, as we don't need/want
	   to join with it at any point. */
	pthread_attr_init (&thread_attributes);
	pthread_attr_setdetachstate (&thread_attributes,
				     PTHREAD_CREATE_DETACHED);
#ifdef _POSIX_THREAD_ATTR_STACKSIZE
	pthread_attr_setstacksize (&thread_attributes, 128*1024);
#endif
#ifdef DEBUG_THUMBNAILS
	g_message ("(Main Thread) Creating thumbnails thread\n");
#endif
	/* We set a flag to indicate the thread is running, so we don't create
	   a new one. We don't need to lock a mutex here, as the thumbnail
	   thread isn't running yet. And we know we won't create the thread
	   twice, as we also check thumbnail_thread_starter_id before
	   scheduling this idle function. */
	thumbnail_thread_is_running = TRUE;
	pthread_create (&thumbnail_thread, &thread_attributes,
			thumbnail_thread_start, NULL);

	thumbnail_thread_starter_id = 0;

	return FALSE;
}

void
nautilus_update_thumbnail_file_copied (const char *source_file_uri,
				       const char *destination_file_uri)
{
	char *old_thumbnail_path;
	GdkPixbuf *pixbuf;
	GFileInfo *file_info;
	GnomeThumbnailFactory *factory;
	GFile *destination_file;
	
	old_thumbnail_path = gnome_thumbnail_path_for_uri (source_file_uri, GNOME_THUMBNAIL_SIZE_NORMAL);
	if (old_thumbnail_path != NULL &&
	    g_file_test (old_thumbnail_path, G_FILE_TEST_EXISTS)) {
		destination_file = g_file_new_for_uri (destination_file_uri);
		file_info = g_file_query_info (destination_file, G_FILE_ATTRIBUTE_TIME_MODIFIED, 0, NULL, NULL);
		g_object_unref (destination_file);
		if (file_info != NULL) {
			pixbuf = gdk_pixbuf_new_from_file (old_thumbnail_path, NULL);
			
			if (pixbuf && gnome_thumbnail_has_uri (pixbuf, source_file_uri)) {
				factory = get_thumbnail_factory ();
				gnome_thumbnail_factory_save_thumbnail (factory,
									pixbuf,
									destination_file_uri,
									g_file_info_get_attribute_uint64 (file_info, G_FILE_ATTRIBUTE_TIME_MODIFIED));
			}
			
			if (pixbuf) {
				g_object_unref (pixbuf);
			}
			g_object_unref (file_info);
		}
	}

	g_free (old_thumbnail_path);
}

void
nautilus_update_thumbnail_file_renamed (const char *source_file_uri,
					const char *destination_file_uri)
{
	nautilus_update_thumbnail_file_copied (source_file_uri, destination_file_uri);
	nautilus_remove_thumbnail_for_file (source_file_uri);
}

void 
nautilus_remove_thumbnail_for_file (const char *file_uri)
{
	char *thumbnail_path;
	
	thumbnail_path = gnome_thumbnail_path_for_uri (file_uri, GNOME_THUMBNAIL_SIZE_NORMAL);
	if (thumbnail_path != NULL) {
		unlink (thumbnail_path);
	}
	g_free (thumbnail_path);
}

static GdkPixbuf *
nautilus_get_thumbnail_frame (void)
{
	char *image_path;
	static GdkPixbuf *thumbnail_frame = NULL;

	if (thumbnail_frame == NULL) {
		image_path = nautilus_pixmap_file ("thumbnail_frame.png");
		if (image_path != NULL) {
			thumbnail_frame = gdk_pixbuf_new_from_file (image_path, NULL);
		}
		g_free (image_path);
	}
	
	return thumbnail_frame;
}


void
nautilus_thumbnail_frame_image (GdkPixbuf **pixbuf)
{
	GdkPixbuf *pixbuf_with_frame, *frame;
	int left_offset, top_offset, right_offset, bottom_offset;
		
	/* The pixbuf isn't already framed (i.e., it was not made by
	 * an old Nautilus), so we must embed it in a frame.
	 */

	frame = nautilus_get_thumbnail_frame ();
	if (frame == NULL) {
		return;
	}
	
	left_offset = NAUTILUS_THUMBNAIL_FRAME_LEFT;
	top_offset = NAUTILUS_THUMBNAIL_FRAME_TOP;
	right_offset = NAUTILUS_THUMBNAIL_FRAME_RIGHT;
	bottom_offset = NAUTILUS_THUMBNAIL_FRAME_BOTTOM;
	
	pixbuf_with_frame = eel_embed_image_in_frame
		(*pixbuf, frame,
		 left_offset, top_offset, right_offset, bottom_offset);
	g_object_unref (*pixbuf);	

	*pixbuf = pixbuf_with_frame;
}

GdkPixbuf *
nautilus_thumbnail_unframe_image (GdkPixbuf *pixbuf)
{
	GdkPixbuf *pixbuf_without_frame, *frame;
	int left_offset, top_offset, right_offset, bottom_offset;
	int w, h;
		
	/* The pixbuf isn't already framed (i.e., it was not made by
	 * an old Nautilus), so we must embed it in a frame.
	 */

	frame = nautilus_get_thumbnail_frame ();
	if (frame == NULL) {
		return NULL;
	}
	
	left_offset = NAUTILUS_THUMBNAIL_FRAME_LEFT;
	top_offset = NAUTILUS_THUMBNAIL_FRAME_TOP;
	right_offset = NAUTILUS_THUMBNAIL_FRAME_RIGHT;
	bottom_offset = NAUTILUS_THUMBNAIL_FRAME_BOTTOM;

	w = gdk_pixbuf_get_width (pixbuf) - left_offset - right_offset;
	h = gdk_pixbuf_get_height (pixbuf) - top_offset - bottom_offset;
	pixbuf_without_frame =
		gdk_pixbuf_new (gdk_pixbuf_get_colorspace (pixbuf),
				gdk_pixbuf_get_has_alpha (pixbuf),
				gdk_pixbuf_get_bits_per_sample (pixbuf),
				w, h);

	gdk_pixbuf_copy_area (pixbuf,
			      left_offset, top_offset,
			      w, h,
			      pixbuf_without_frame, 0, 0);
	
	return pixbuf_without_frame;
}

typedef struct {
	gboolean is_thumbnail;
	guint base_size;
	guint nominal_size;
	gboolean force_nominal;
	int original_height;
	int original_width;
	double *scale_x_out;
	double *scale_y_out;
} ThumbnailLoadArgs;

static void
thumbnail_loader_size_prepared (GdkPixbufLoader *loader,
				int width,
				int height,
				ThumbnailLoadArgs *args)
{
	int size = MAX (width, height);

	args->original_width = width;
	args->original_height = height;

	if (args->force_nominal) {
		args->base_size = size;                        
	} else if (args->base_size == 0) {
		if (args->is_thumbnail) {
			args->base_size = 128 * NAUTILUS_ICON_SIZE_STANDARD / thumbnail_icon_size;
		} else {
			if (size > args->nominal_size * thumbnail_icon_size / NAUTILUS_ICON_SIZE_STANDARD) {
				args->base_size = size * NAUTILUS_ICON_SIZE_STANDARD / thumbnail_icon_size;
			} else if (size > NAUTILUS_ICON_SIZE_STANDARD) {
				args->base_size = args->nominal_size;
			} else {
				/* Don't scale up small icons */
				args->base_size = NAUTILUS_ICON_SIZE_STANDARD;
			}
		}
	}

	if (args->base_size != args->nominal_size) {
		double scale;

		scale = (double) args->nominal_size / args->base_size;

		if ((int) (width * scale) > NAUTILUS_ICON_MAXIMUM_SIZE ||
		    (int) (height * scale) > NAUTILUS_ICON_MAXIMUM_SIZE) {
			scale = MIN ((double) NAUTILUS_ICON_MAXIMUM_SIZE / width,
				     (double) NAUTILUS_ICON_MAXIMUM_SIZE / height);
		}

		width = MAX (1, floor (width * scale + 0.5));
		height = MAX (1, floor (height * scale + 0.5));

		gdk_pixbuf_loader_set_size (loader, width, height);
	}

}

static void
thumbnail_loader_area_prepared (GdkPixbufLoader *loader,
				ThumbnailLoadArgs *args)
{
	GdkPixbuf *pixbuf;

	pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);

	*args->scale_x_out = (double) gdk_pixbuf_get_width (pixbuf) / args->original_width;
	*args->scale_y_out = (double) gdk_pixbuf_get_height (pixbuf) / args->original_height;
}

static GdkPixbuf *
get_pixbuf_from_data (const unsigned char *buffer,
		      gsize buflen,
		      const char *path,
		      guint base_size,
		      guint nominal_size,
		      gboolean force_nominal,
		      double *scale_x_out,
		      double *scale_y_out)
{
	GdkPixbufLoader *loader;
	GdkPixbuf *pixbuf;
	ThumbnailLoadArgs args;
	GError *error;

	if (thumbnail_icon_size == 0) {
		eel_preferences_add_auto_integer (NAUTILUS_PREFERENCES_ICON_VIEW_THUMBNAIL_SIZE,
						  &thumbnail_icon_size);
	}

	loader = gdk_pixbuf_loader_new ();
	g_signal_connect (loader, "size-prepared",
			  G_CALLBACK (thumbnail_loader_size_prepared),
			  &args);
	g_signal_connect (loader, "area-prepared",
			  G_CALLBACK (thumbnail_loader_area_prepared),
			  &args);

	args.is_thumbnail = strstr (path, "/.thumbnails/") != NULL;
	args.base_size = base_size;
	args.nominal_size = nominal_size;
	args.force_nominal = force_nominal;
	args.scale_x_out = scale_x_out;
	args.scale_y_out = scale_y_out;

	error = NULL;

	if (!gdk_pixbuf_loader_write (loader, buffer, buflen, &error)) {
		g_message ("Failed to write %s to thumbnail pixbuf loader: %s", path, error->message);

		gdk_pixbuf_loader_close (loader, NULL);
		g_object_unref (G_OBJECT (loader));
		g_error_free (error);

		return NULL;
	}

	error = NULL;

	if (!gdk_pixbuf_loader_close (loader, &error) ||
	    /* Seems we have to check this even if it returned TRUE (#403255) */
	    error != NULL) {
		g_message ("Failed to close thumbnail pixbuf loader for %s: %s", path, error->message);

		g_object_unref (G_OBJECT (loader));
		g_error_free (error);

		return NULL;
	}

	pixbuf = g_object_ref (gdk_pixbuf_loader_get_pixbuf (loader));

	g_object_unref (G_OBJECT (loader));

	return pixbuf;
}


/* routine to load an image from the passed-in path
 */
GdkPixbuf *
nautilus_thumbnail_load_image (const char *path,
			       guint base_size,
			       guint nominal_size,
			       gboolean force_nominal,
			       double *scale_x_out,
			       double *scale_y_out)
{
	GdkPixbuf *pixbuf;
	guchar *buffer;
	gsize buflen;
	GError *error;

	error = NULL;
	
	if (!g_file_get_contents (path, (gchar **) &buffer, &buflen, &error)) {
		g_message ("Failed to load %s into memory: %s", path, error->message);

		g_error_free (error);

		return NULL;
	}

	pixbuf = get_pixbuf_from_data (buffer, buflen, path,
				       base_size, nominal_size, force_nominal,
				       scale_x_out, scale_y_out);

	g_free (buffer);

	return pixbuf;
}

static void
async_thumbnail_read_image (GObject *source_object,
			    GAsyncResult *res,
			    gpointer callback_data)
{
	NautilusThumbnailAsyncLoadHandle *handle = callback_data;
	GdkPixbuf *pixbuf;
	double scale_x, scale_y;
	gsize file_size;
	char *file_contents;

	pixbuf = NULL;
	scale_x = scale_y = 1.0;

	if (g_file_load_contents_finish (G_FILE (source_object),
					 res,
					 &file_contents, &file_size,
					 NULL, NULL)) {
		pixbuf = get_pixbuf_from_data (file_contents, file_size,
					       handle->file_path,
					       handle->base_size,
					       handle->nominal_size,
					       handle->force_nominal,
					       &scale_x, &scale_y);
		g_free (file_contents);
	}

	handle->load_func (handle,
			   handle->file_path,
			   pixbuf, scale_x, scale_y,
			   handle->load_func_user_data);

	gdk_pixbuf_unref (pixbuf);

	g_object_unref (handle->cancellable);
	g_free (handle->file_path);
	g_free (handle);
}

NautilusThumbnailAsyncLoadHandle *
nautilus_thumbnail_load_image_async (const char *path,
				     guint base_size,
				     guint nominal_size,
				     gboolean force_nominal,
				     NautilusThumbnailAsyncLoadFunc load_func,
				     gpointer load_func_user_data)
{
	NautilusThumbnailAsyncLoadHandle *handle;
	GFile *location;


	handle = g_new (NautilusThumbnailAsyncLoadHandle, 1);
	handle->cancellable = g_cancellable_new ();
	handle->file_path = g_strdup (path);
	handle->base_size = base_size;
	handle->nominal_size = nominal_size;
	handle->force_nominal = force_nominal;
	handle->load_func = load_func;
	handle->load_func_user_data = load_func_user_data;


	location = g_file_new_for_path (path);
	g_file_load_contents_async (location, handle->cancellable,
				    async_thumbnail_read_image,
				    handle);
	g_object_unref (location);
	
	return handle;
}

void
nautilus_thumbnail_load_image_cancel (NautilusThumbnailAsyncLoadHandle *handle)
{
	g_assert (handle != NULL);

	g_cancellable_cancel  (handle->cancellable);
}

void
nautilus_thumbnail_remove_from_queue (const char *file_uri)
{
	GList *node;
	
#ifdef DEBUG_THUMBNAILS
	g_message ("(Remove from queue) Locking mutex\n");
#endif
	pthread_mutex_lock (&thumbnails_mutex);

	/*********************************
	 * MUTEX LOCKED
	 *********************************/

	if (thumbnails_to_make_hash) {
		node = g_hash_table_lookup (thumbnails_to_make_hash, file_uri);
		
		if (node && node->data != currently_thumbnailing) {
			g_hash_table_remove (thumbnails_to_make_hash, file_uri);
			free_thumbnail_info (node->data);
			g_queue_delete_link ((GQueue *)&thumbnails_to_make, node);
		}
	}
	
	/*********************************
	 * MUTEX UNLOCKED
	 *********************************/
	
#ifdef DEBUG_THUMBNAILS
	g_message ("(Remove from queue) Unlocking mutex\n");
#endif
	pthread_mutex_unlock (&thumbnails_mutex);
}

void
nautilus_thumbnail_remove_all_from_queue (void)
{
	NautilusThumbnailInfo *info;
	GList *l, *next;
	
#ifdef DEBUG_THUMBNAILS
	g_message ("(Remove all from queue) Locking mutex\n");
#endif
	pthread_mutex_lock (&thumbnails_mutex);

	/*********************************
	 * MUTEX LOCKED
	 *********************************/

	l = thumbnails_to_make.head;
	while (l != NULL) {
		info = l->data;
		next = l->next;
		if (info != currently_thumbnailing) {
			g_hash_table_remove (thumbnails_to_make_hash, 
					     info->image_uri);
			free_thumbnail_info (info);
			g_queue_delete_link ((GQueue *)&thumbnails_to_make, l);
		}

		l = next;
	}
	
	/*********************************
	 * MUTEX UNLOCKED
	 *********************************/
	
#ifdef DEBUG_THUMBNAILS
	g_message ("(Remove all from queue) Unlocking mutex\n");
#endif
	pthread_mutex_unlock (&thumbnails_mutex);
}

void
nautilus_thumbnail_prioritize (const char *file_uri)
{
	GList *node;

#ifdef DEBUG_THUMBNAILS
	g_message ("(Prioritize) Locking mutex\n");
#endif
	pthread_mutex_lock (&thumbnails_mutex);

	/*********************************
	 * MUTEX LOCKED
	 *********************************/

	if (thumbnails_to_make_hash) {
		node = g_hash_table_lookup (thumbnails_to_make_hash, file_uri);
		
		if (node && node->data != currently_thumbnailing) {
			g_queue_unlink ((GQueue *)&thumbnails_to_make, node);
			g_queue_push_head_link ((GQueue *)&thumbnails_to_make, node);
		}
	}
	
	/*********************************
	 * MUTEX UNLOCKED
	 *********************************/
	
#ifdef DEBUG_THUMBNAILS
	g_message ("(Prioritize) Unlocking mutex\n");
#endif
	pthread_mutex_unlock (&thumbnails_mutex);
}


/***************************************************************************
 * Thumbnail Thread Functions.
 ***************************************************************************/


/* This is a one-shot idle callback called from the main loop to call
   notify_file_changed() for a thumbnail. It frees the uri afterwards.
   We do this in an idle callback as I don't think nautilus_file_changed() is
   thread-safe. */
static gboolean
thumbnail_thread_notify_file_changed (gpointer image_uri)
{
	NautilusFile *file;

	GDK_THREADS_ENTER ();

	file = nautilus_file_get_by_uri ((char *) image_uri);
#ifdef DEBUG_THUMBNAILS
	g_message ("(Thumbnail Thread) Notifying file changed file:%p uri: %s\n", file, (char*) image_uri);
#endif

	if (file != NULL) {
		nautilus_file_set_is_thumbnailing (file, FALSE);
		nautilus_file_invalidate_attributes (file,
						     NAUTILUS_FILE_ATTRIBUTE_THUMBNAIL |
						     NAUTILUS_FILE_ATTRIBUTE_INFO);
		nautilus_file_unref (file);
	}
	g_free (image_uri);

	GDK_THREADS_LEAVE ();

	return FALSE;
}

static GHashTable *
get_types_table (void)
{
	int i;

	if (image_mime_types == NULL) {
		image_mime_types = g_hash_table_new (g_str_hash, g_str_equal);
		for (i = 0; i < G_N_ELEMENTS (types); i++) {
			g_hash_table_insert (image_mime_types,
					     (gpointer) types [i],
					     GUINT_TO_POINTER (1));
		}
	}

	return image_mime_types;
}

gboolean
nautilus_thumbnail_is_mimetype_limited_by_size (const char *mime_type)
{
	GHashTable *image_mime_types;
	
	image_mime_types = get_types_table ();
        if (g_hash_table_lookup (image_mime_types, mime_type)) {
                return TRUE;
	}

        return FALSE;
}

gboolean
nautilus_can_thumbnail (NautilusFile *file)
{
	GnomeThumbnailFactory *factory;
	gboolean res;
	char *uri;
	time_t mtime;
	char *mime_type;
		
	uri = nautilus_file_get_uri (file);
	mime_type = nautilus_file_get_mime_type (file);
	mtime = nautilus_file_get_mtime (file);
	
	factory = get_thumbnail_factory ();
	res = gnome_thumbnail_factory_can_thumbnail (factory,
						     uri,
						     mime_type,
						     mtime);
	g_free (mime_type);
	g_free (uri);

	return res;
}

void
nautilus_create_thumbnail (NautilusFile *file)
{
	time_t file_mtime = 0;
	NautilusThumbnailInfo *info;
	NautilusThumbnailInfo *existing_info;
	GList *existing, *node;

	nautilus_file_set_is_thumbnailing (file, TRUE);

	info = g_new0 (NautilusThumbnailInfo, 1);
	info->image_uri = nautilus_file_get_uri (file);
	info->mime_type = nautilus_file_get_mime_type (file);
	
	/* Hopefully the NautilusFile will already have the image file mtime,
	   so we can just use that. Otherwise we have to get it ourselves. */
	if (file->details->got_file_info &&
	    file->details->file_info_is_up_to_date &&
	    file->details->mtime != 0) {
		file_mtime = file->details->mtime;
	} else {
		get_file_mtime (info->image_uri, &file_mtime);
	}
	
	info->original_file_mtime = file_mtime;


#ifdef DEBUG_THUMBNAILS
	g_message ("(Main Thread) Locking mutex\n");
#endif
	pthread_mutex_lock (&thumbnails_mutex);
	
	/*********************************
	 * MUTEX LOCKED
	 *********************************/

	if (thumbnails_to_make_hash == NULL) {
		thumbnails_to_make_hash = g_hash_table_new (g_str_hash,
							    g_str_equal);
	}

	/* Check if it is already in the list of thumbnails to make. */
	existing = g_hash_table_lookup (thumbnails_to_make_hash, info->image_uri);
	if (existing == NULL) {
		/* Add the thumbnail to the list. */
#ifdef DEBUG_THUMBNAILS
		g_message ("(Main Thread) Adding thumbnail: %s\n",
			   info->image_uri);
#endif
		g_queue_push_tail ((GQueue *)&thumbnails_to_make, info);
		node = g_queue_peek_tail_link ((GQueue *)&thumbnails_to_make);
		g_hash_table_insert (thumbnails_to_make_hash,
				     info->image_uri,
				     node);
		/* If the thumbnail thread isn't running, and we haven't
		   scheduled an idle function to start it up, do that now.
		   We don't want to start it until all the other work is done,
		   so the GUI will be updated as quickly as possible.*/
		if (thumbnail_thread_is_running == FALSE &&
		    thumbnail_thread_starter_id == 0) {
			thumbnail_thread_starter_id = g_idle_add_full (G_PRIORITY_LOW, thumbnail_thread_starter_cb, NULL, NULL);
		}
	} else {
#ifdef DEBUG_THUMBNAILS
		g_message ("(Main Thread) Updating non-current mtime: %s\n",
			   info->image_uri);
#endif
		/* The file in the queue might need a new original mtime */
		existing_info = existing->data;
		existing_info->original_file_mtime = info->original_file_mtime;
		free_thumbnail_info (info);
	}   

	/*********************************
	 * MUTEX UNLOCKED
	 *********************************/

#ifdef DEBUG_THUMBNAILS
	g_message ("(Main Thread) Unlocking mutex\n");
#endif
	pthread_mutex_unlock (&thumbnails_mutex);
}

/* thumbnail_thread is invoked as a separate thread to to make thumbnails. */
static gpointer
thumbnail_thread_start (gpointer data)
{
	NautilusThumbnailInfo *info = NULL;
	GdkPixbuf *pixbuf;
	time_t current_orig_mtime = 0;
	time_t current_time;
	GList *node;

	/* We loop until there are no more thumbails to make, at which point
	   we exit the thread. */
	for (;;) {
#ifdef DEBUG_THUMBNAILS
		g_message ("(Thumbnail Thread) Locking mutex\n");
#endif
		pthread_mutex_lock (&thumbnails_mutex);

		/*********************************
		 * MUTEX LOCKED
		 *********************************/

		/* Pop the last thumbnail we just made off the head of the
		   list and free it. I did this here so we only have to lock
		   the mutex once per thumbnail, rather than once before
		   creating it and once after.
		   Don't pop the thumbnail off the queue if the original file
		   mtime of the request changed. Then we need to redo the thumbnail.
		*/
		if (currently_thumbnailing &&
		    currently_thumbnailing->original_file_mtime == current_orig_mtime) {
			g_assert (info == currently_thumbnailing);
			node = g_hash_table_lookup (thumbnails_to_make_hash, info->image_uri);
			g_assert (node != NULL);
			g_hash_table_remove (thumbnails_to_make_hash, info->image_uri);
			free_thumbnail_info (info);
			g_queue_delete_link ((GQueue *)&thumbnails_to_make, node);
		}
		currently_thumbnailing = NULL;

		/* If there are no more thumbnails to make, reset the
		   thumbnail_thread_is_running flag, unlock the mutex, and
		   exit the thread. */
		if (g_queue_is_empty ((GQueue *)&thumbnails_to_make)) {
#ifdef DEBUG_THUMBNAILS
			g_message ("(Thumbnail Thread) Exiting\n");
#endif
			thumbnail_thread_is_running = FALSE;
			pthread_mutex_unlock (&thumbnails_mutex);
			pthread_exit (NULL);
		}

		/* Get the next one to make. We leave it on the list until it
		   is created so the main thread doesn't add it again while we
		   are creating it. */
		info = g_queue_peek_head ((GQueue *)&thumbnails_to_make);
		currently_thumbnailing = info;
		current_orig_mtime = info->original_file_mtime;
		/*********************************
		 * MUTEX UNLOCKED
		 *********************************/

#ifdef DEBUG_THUMBNAILS
		g_message ("(Thumbnail Thread) Unlocking mutex\n");
#endif
		pthread_mutex_unlock (&thumbnails_mutex);

		time (&current_time);

		/* Don't try to create a thumbnail if the file was modified recently.
		   This prevents constant re-thumbnailing of changing files. */ 
		if (current_time < current_orig_mtime + THUMBNAIL_CREATION_DELAY_SECS &&
		    current_time >= current_orig_mtime) {
#ifdef DEBUG_THUMBNAILS
			g_message ("(Thumbnail Thread) Skipping: %s\n",
				   info->image_uri);
#endif
			/* Reschedule thumbnailing via a change notification */
			g_timeout_add (1000, thumbnail_thread_notify_file_changed,
				       g_strdup (info->image_uri));
 			continue;
		}

		/* Create the thumbnail. */
#ifdef DEBUG_THUMBNAILS
		g_message ("(Thumbnail Thread) Creating thumbnail: %s\n",
			   info->image_uri);
#endif

		pixbuf = gnome_thumbnail_factory_generate_thumbnail (thumbnail_factory,
								     info->image_uri,
								     info->mime_type);

		if (pixbuf) {
			gnome_thumbnail_factory_save_thumbnail (thumbnail_factory,
								pixbuf,
								info->image_uri,
								current_orig_mtime);
			g_object_unref (pixbuf);
		} else {
			gnome_thumbnail_factory_create_failed_thumbnail (thumbnail_factory, 
									 info->image_uri,
									 current_orig_mtime);
		}
		/* We need to call nautilus_file_changed(), but I don't think that is
		   thread safe. So add an idle handler and do it from the main loop. */
		g_idle_add_full (G_PRIORITY_HIGH_IDLE,
				 thumbnail_thread_notify_file_changed,
				 g_strdup (info->image_uri), NULL);
	}
}
