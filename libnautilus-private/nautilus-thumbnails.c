/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-thumbnails.h: Thumbnail code for icon factory.
 
   Copyright (C) 2000, 2001 Eazel, Inc.
  
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
#include "nautilus-icon-factory-private.h"
#include "nautilus-icon-factory.h"
#include "nautilus-theme.h"
#include "nautilus-thumbnails-jpeg.h"
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtkmain.h>
#include <libgnomevfs/gnome-vfs.h>
#include <librsvg/rsvg.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>

#include "nautilus-file-private.h"

/* turn this on to see messages about thumbnail creation */
#if 0
#define DEBUG_THUMBNAILS
#endif

/* Should never be a reasonable actual mtime */
#define INVALID_MTIME 0

static gpointer thumbnail_thread_start (gpointer data);

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

/* The list of NautilusThumbnailInfo structs containing information about the
   thumbnails we are making. Lock thumbnails_mutex when accessing this. */
static volatile GList *thumbnails_to_make = NULL;

static GnomeThumbnailFactory *thumbnail_factory = NULL;

static gboolean
get_file_mtime (const char *file_uri, time_t* mtime)
{
	GnomeVFSFileInfo *file_info;

	/* gather the info and then compare modification times */
	file_info = gnome_vfs_file_info_new ();
	gnome_vfs_get_file_info (file_uri, file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

	if (file_info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME)
		*mtime = file_info->mtime;
	else
		*mtime = INVALID_MTIME;

	gnome_vfs_file_info_unref (file_info);
	
	return TRUE;
}


/* structure used for making thumbnails, associating a uri with where the thumbnail is to be stored */

typedef struct {
	char *image_uri;
	char *mime_type;
	time_t original_file_mtime;
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

	return strcmp (info_a->image_uri, info_b->image_uri) != 0;
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
		thumbnail_factory = nautilus_icon_factory_get_thumbnail_factory ();
	}

	/* We create the thread in the detached state, as we don't need/want
	   to join with it at any point. */
	pthread_attr_init (&thread_attributes);
	pthread_attr_setdetachstate (&thread_attributes,
				     PTHREAD_CREATE_DETACHED);
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
nautilus_update_thumbnail_file_renamed (const char *old_file_uri, const char *new_file_uri)
{
	char *old_thumbnail_path;
	GdkPixbuf *pixbuf;
	GnomeVFSFileInfo *file_info;
	GnomeThumbnailFactory *factory;
	
	old_thumbnail_path = gnome_thumbnail_path_for_uri (old_file_uri, GNOME_THUMBNAIL_SIZE_NORMAL);
	if (old_thumbnail_path != NULL &&
	    g_file_test (old_thumbnail_path, G_FILE_TEST_EXISTS)) {
		file_info = gnome_vfs_file_info_new ();
		if (gnome_vfs_get_file_info (new_file_uri,
					     file_info,
					     GNOME_VFS_FILE_INFO_DEFAULT) == GNOME_VFS_OK) {
			pixbuf = gdk_pixbuf_new_from_file (old_thumbnail_path, NULL);
			
			if (pixbuf && gnome_thumbnail_has_uri (pixbuf, old_file_uri)) {
				factory = nautilus_icon_factory_get_thumbnail_factory ();
				gnome_thumbnail_factory_save_thumbnail (factory,
									pixbuf,
									new_file_uri,
									file_info->mtime);
				g_object_unref (factory);
			}
			
			if (pixbuf) {
				g_object_unref (pixbuf);
			}
			
			unlink (old_thumbnail_path);
		}
		gnome_vfs_file_info_unref (file_info);
	}

	g_free (old_thumbnail_path);
}

void 
nautilus_remove_thumbnail_for_file (const char *old_file_uri)
{
	char *old_thumbnail_path;
	
	old_thumbnail_path = gnome_thumbnail_path_for_uri (old_file_uri, GNOME_THUMBNAIL_SIZE_NORMAL);
	if (old_thumbnail_path != NULL) {
		unlink (old_thumbnail_path);
	}
	g_free (old_thumbnail_path);
}

/* routine to load an image from the passed-in path, and then embed it in
 * a frame if necessary
 */
GdkPixbuf *
nautilus_thumbnail_load_framed_image (const char *path)
{
	GdkPixbuf *pixbuf, *pixbuf_with_frame, *frame;
	gboolean got_frame_offsets;
	char *frame_offset_str;
	int left_offset, top_offset, right_offset, bottom_offset;
	char c;
	
	pixbuf = gdk_pixbuf_new_from_file (path, NULL);
	if (pixbuf == NULL) {
		return NULL;
	}
	
	/* The pixbuf isn't already framed (i.e., it was not made by
	 * an old Nautilus), so we must embed it in a frame.
	 */

	frame = nautilus_icon_factory_get_thumbnail_frame ();
	if (frame == NULL) {
		return pixbuf;
	}
	
	got_frame_offsets = FALSE;
	frame_offset_str = nautilus_theme_get_theme_data ("thumbnails", "FRAME_OFFSETS");
	if (frame_offset_str != NULL) {
		if (sscanf (frame_offset_str, " %d , %d , %d , %d %c",
			    &left_offset, &top_offset, &right_offset, &bottom_offset, &c) == 4) {
			got_frame_offsets = TRUE;
		}
		g_free (frame_offset_str);
	}
	if (!got_frame_offsets) {
		/* use nominal values since the info in the theme couldn't be found */
		left_offset = 3;
		top_offset = 3;
		right_offset = 6;
		bottom_offset = 6;
	}
	
	pixbuf_with_frame = eel_embed_image_in_frame
		(pixbuf, frame,
		 left_offset, top_offset, right_offset, bottom_offset);
	g_object_unref (pixbuf);	

	return pixbuf_with_frame;
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

	file = nautilus_file_get ((char *) image_uri);
#ifdef DEBUG_THUMBNAILS
	g_message ("(Thumbnail Thread) Notifying file changed file:%p uri: %s\n", file, (char*) image_uri);
#endif

	if (file != NULL) {
		nautilus_file_changed (file);
		nautilus_file_unref (file);
	}
	g_free (image_uri);

	GDK_THREADS_LEAVE ();

	return FALSE;
}

void
nautilus_create_thumbnail (NautilusFile *file)
{
	time_t file_mtime = 0;
	NautilusThumbnailInfo *info;

	info = g_new0 (NautilusThumbnailInfo, 1);
	info->image_uri = nautilus_file_get_uri (file);
	info->mime_type = nautilus_file_get_mime_type (file);
	
	/* Hopefully the NautilusFile will already have the image file mtime,
	   so we can just use that. Otherwise we have to get it ourselves. */
	if (file->details->info
	    && file->details->file_info_is_up_to_date
	    && file->details->info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME) {
		file_mtime = file->details->info->mtime;
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

	/* Check if it is already in the list of thumbnails to make. */
	if (g_list_find_custom ((GList*) thumbnails_to_make, info,
				compare_thumbnail_info) == NULL) {
		/* Add the thumbnail to the list. */
#ifdef DEBUG_THUMBNAILS
		g_message ("(Main Thread) Adding thumbnail: %s\n",
			   info->image_uri);
#endif
		thumbnails_to_make = g_list_append ((GList*) thumbnails_to_make, info);

		/* If the thumbnail thread isn't running, and we haven't
		   scheduled an idle function to start it up, do that now.
		   We don't want to start it until all the other work is done,
		   so the GUI will be updated as quickly as possible.*/
		if (thumbnail_thread_is_running == FALSE
		    && thumbnail_thread_starter_id == 0) {
			thumbnail_thread_starter_id = g_idle_add_full (G_PRIORITY_LOW, thumbnail_thread_starter_cb, NULL, NULL);
		}
	} else {
		g_free (info->image_uri);
		g_free (info->mime_type);
		g_free (info);
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
		   creating it and once after. */
		if (thumbnails_to_make && info == thumbnails_to_make->data) {
			thumbnails_to_make = g_list_remove ((GList*) thumbnails_to_make, info);
			g_free (info->image_uri);
			g_free (info->mime_type);
			g_free (info);
		}

		/* If there are no more thumbnails to make, reset the
		   thumbnail_thread_is_running flag, unlock the mutex, and
		   exit the thread. */
		if (thumbnails_to_make == NULL) {
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
		info = thumbnails_to_make->data;

		/*********************************
		 * MUTEX UNLOCKED
		 *********************************/

#ifdef DEBUG_THUMBNAILS
		g_message ("(Thumbnail Thread) Unlocking mutex\n");
#endif
		pthread_mutex_unlock (&thumbnails_mutex);


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
								info->original_file_mtime);
		} else {
			gnome_thumbnail_factory_create_failed_thumbnail (thumbnail_factory, 
									 info->image_uri,
									 info->original_file_mtime);
		}
		/* We need to call nautilus_file_changed(), but I don't think that is
		   thread safe. So add an idle handler and do it from the main loop. */
		g_idle_add_full (G_PRIORITY_HIGH_IDLE,
				 thumbnail_thread_notify_file_changed,
				 g_strdup (info->image_uri), NULL);
	}
}
