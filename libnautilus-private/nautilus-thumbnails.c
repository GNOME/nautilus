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

#include "nautilus-file-private.h"

/* turn this on to see messages about thumbnail creation */
#if 0
#define DEBUG_THUMBNAILS
#endif

/* The time we allow 'convert' to convert an image, in milliseconds
   (1/1000ths of a second). If it hasn't finished by this time, we kill it. */
#define THUMBNAIL_CONVERT_TIMEOUT	60000


/* permissions for thumbnail directory */
#define THUMBNAIL_DIR_PERMISSIONS (GNOME_VFS_PERM_USER_ALL \
				   | GNOME_VFS_PERM_GROUP_ALL \
				   | GNOME_VFS_PERM_OTHER_ALL)

#define THUMBNAIL_PLACEHOLDER_PERMISSIONS (GNOME_VFS_PERM_USER_READ | \
					   GNOME_VFS_PERM_USER_WRITE | \
					   GNOME_VFS_PERM_GROUP_READ | \
					   GNOME_VFS_PERM_GROUP_WRITE | \
					   GNOME_VFS_PERM_OTHER_READ)

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



/* utility to test whether a file exists using vfs */
static gboolean
vfs_file_exists (const char *file_uri)
{
	gboolean result;
	GnomeVFSURI *uri;
	
	uri = gnome_vfs_uri_new (file_uri);
	if (uri == NULL) {
		return FALSE;
	}	

	/* FIXME bugzilla.gnome.org 43137: The synchronous I/O here
	 * means this call is unsuitable for use on anything that
	 * might be remote.
	 */
	result = gnome_vfs_uri_exists (uri);
	gnome_vfs_uri_unref (uri);

	return result;
}

static gboolean
uri_is_local (const char *uri)
{
	gboolean is_local;
	GnomeVFSURI *vfs_uri;

	vfs_uri = gnome_vfs_uri_new (uri);
	is_local = gnome_vfs_uri_is_local (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);

	return is_local;
}

/* this functions looks for a password in a uri and changes it for 6 'x' */

static char *
obfuscate_password (const char *escaped_uri)
{
	const char *passwd_start, *passwd_end;
        char *new_uri, *new_uri_temp;

	passwd_start = strchr (escaped_uri, ':');
	g_assert (passwd_start != NULL);
	passwd_start = strchr (passwd_start + 1, ':'); /* The fisrt ':' is for the protocol */
	if (passwd_start == NULL) { /* There's no password */
		return g_strdup (escaped_uri);
	}
	passwd_end = strchr (passwd_start, '@');

	/* This URL has no valid password */
	if (passwd_end == NULL || passwd_start == NULL || passwd_end <= passwd_start) {
		return g_strdup (escaped_uri);
	} else {
		new_uri_temp = g_strndup (escaped_uri, passwd_start - escaped_uri);
		new_uri = g_strdup_printf ("%s:xxxxxx%s", new_uri_temp, passwd_end);
		g_free (new_uri_temp);
		return new_uri;
	}
}

/* utility routine that, given the uri of an image, constructs the uri to the corresponding thumbnail */

static char *
make_thumbnail_uri (const char *image_uri)
{
	char *directory_name, *last_slash;
	char *escaped_uri, *protected_uri;
	char *thumbnail_path, *thumbnail_base, *thumbnail_uri;
	
	/* Copy the image uri and change the last '/' character to '\0',
	   so we have the directory part and the basename part. */
	directory_name = g_strdup (image_uri);
	last_slash = strrchr (directory_name, '/');
	*last_slash = '\0';

	/* Convert '/' characters in the directory part to "%2F", so we can
	   use one directory to represent the full path,
	   e.g. "file:///home/damon" becomes "file:%2F%2F%2Fhome%2Fdamon". */
	escaped_uri = gnome_vfs_escape_slashes (directory_name);

	/* Try to obfuscate any password embedded in the uri, so it can't be
	   spotted by looking the the ~/.nautilus/thumbnails directory. */
	protected_uri = obfuscate_password (escaped_uri);
	g_free (escaped_uri);

	/* Create the directory in which the thumbnail file will be stored,
	   e.g. "/home/damon/.nautilus/thumbnails/file:%2F%2F%2Fhome%2Fdamon".
	*/
	thumbnail_path = g_strdup_printf ("%s/.nautilus/thumbnails/%s",
					  g_get_home_dir(), protected_uri);
	/* Turn it into a uri, i.e. prefix with 'file:///' and escape any
	   invalid characters. */
	thumbnail_base = gnome_vfs_get_uri_from_local_path (thumbnail_path);
	g_free (thumbnail_path);
	g_free (protected_uri);
		
	/* append the file name, and a .png suffix if necessary. */
	if (eel_istr_has_suffix (image_uri, ".png")) {
		thumbnail_uri = g_strdup_printf ("%s/%s", thumbnail_base,
						 last_slash + 1);
	} else {
		thumbnail_uri = g_strdup_printf ("%s/%s.png", thumbnail_base,
						 last_slash + 1);
	}
	g_free(thumbnail_base);

	g_free (directory_name);

	return thumbnail_uri;
}

static gboolean
get_file_mtime (const char *file_uri, time_t* mtime)
{
	GnomeVFSFileInfo *file_info;

	if (!uri_is_local (file_uri)) {
		*mtime = INVALID_MTIME;
		return FALSE;
	}
	
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

/* utility to create a placeholder thumbnail uri (which indicates that a
 * previous thumbnailing attempt has failed)
 */
/* FIXME: A .x extension might exist on a real file, and we might
 * recognize it by magic number even if it doesn't have the right
 * extension.
 */
static char *
make_invalid_thumbnail_uri (const char *thumbnail_uri)
{
	return g_strconcat (thumbnail_uri, ".x", NULL);
}

/* return true if there's a placeholder thumbnail present for the passed in
 * file, which indicates that a previous thumbnailing attempt failed and
 * we should use the mime-type icon instead
 */
gboolean
nautilus_thumbnail_has_invalid_thumbnail (NautilusFile *file)
{
	char *file_uri, *thumbnail_uri, *invalid_thumbnail_uri;
	gboolean is_invalid;
	
	file_uri = nautilus_file_get_uri (file);
	
	
	thumbnail_uri = make_thumbnail_uri (file_uri);
	invalid_thumbnail_uri = make_invalid_thumbnail_uri (thumbnail_uri);
	
	is_invalid = vfs_file_exists (invalid_thumbnail_uri);
	
	g_free (file_uri);
	g_free (thumbnail_uri);
	g_free (invalid_thumbnail_uri);
	return is_invalid;
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


/* Routine that takes a uri of a large image file and returns the uri of its
   corresponding thumbnail. If no thumbnail is available, put the image on the
   thumbnail queue so one is eventually made, and return NULL. It will call
   nautilus_file_changed() when the thumbnail is ready or we know we can't
   create one for the image. (Note that this function will probably be called
   again at this point, to get the uri of the newly-created thumbnail.) */
/* FIXME bugzilla.gnome.org 40642: 
 * Most of this thumbnail machinery belongs in NautilusFile, not here.
 */
char *
nautilus_get_thumbnail_uri (NautilusFile *file)
{
	char *file_uri, *thumbnail_uri;
	GnomeVFSFileInfo *file_info;
	GnomeVFSResult result;
	NautilusThumbnailInfo *info;
	time_t file_mtime = INVALID_MTIME;
	time_t thumbnail_mtime = INVALID_MTIME;
	gboolean remake_thumbnail = FALSE;

	/* We have to check if the thumbnail exists and its mtime matches that
	   of the file. If it does, we return it. If not, we return NULL
	   and place the thumbnail in the queue to be made. */
	file_uri = nautilus_file_get_uri (file);
	thumbnail_uri = make_thumbnail_uri (file_uri);
#ifdef DEBUG_THUMBNAILS
	g_message ("(Main Thread) Checking if thumbnail exists: %s\n",
		   file_uri);
#endif

	/* Check if the thumbnail file exists and gets its mtime in one go. */
	file_info = gnome_vfs_file_info_new ();
	result = gnome_vfs_get_file_info (thumbnail_uri, file_info,
					  GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	if (result == GNOME_VFS_OK) {
		thumbnail_mtime = file_info->mtime;
	} else {
		/* If the thumbnail file doesn't exist then we need to make
		   it. */
		remake_thumbnail = TRUE;
	}
	gnome_vfs_file_info_unref (file_info);

	/* Hopefully the NautilusFile will already have the image file mtime,
	   so we can just use that. Otherwise we have to get it ourselves. */
	if (file->details->info
	    && file->details->file_info_is_up_to_date
	    && file->details->info->valid_fields & GNOME_VFS_FILE_INFO_FIELDS_MTIME) {
		file_mtime = file->details->info->mtime;
	} else {
		get_file_mtime (file_uri, &file_mtime);
	}

	/* If either mtime is not available for whatever reason, we
	   don't remake the thumbnail. If both are available and don't
	   match we do remake the thumbnail. */
#ifdef DEBUG_THUMBNAILS
	g_message ("(Main Thread) file mtime: %li thumbnail mtime: %li\n",
		   file_mtime, thumbnail_mtime);
#endif
	if (file_mtime != INVALID_MTIME && thumbnail_mtime != INVALID_MTIME
	    && file_mtime != thumbnail_mtime) {
		remake_thumbnail = TRUE;
	}

	/* If we don't need to make/remake the thumbnail, return the uri. */
	if (!remake_thumbnail) {
#ifdef DEBUG_THUMBNAILS
		g_message ("(Main Thread) mtimes match - returning thumbnail uri\n");
#endif
		g_free (file_uri);
		return thumbnail_uri;
	}

#ifdef DEBUG_THUMBNAILS
	g_message ("(Main Thread) mtimes don't match. Recreating thumbnail\n");
#endif
	nautilus_icon_factory_remove_by_uri (thumbnail_uri);

	info = g_new0 (NautilusThumbnailInfo, 1);
	info->image_uri = file_uri;
	info->mime_type = nautilus_file_get_mime_type (file);
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
	}

	/*********************************
	 * MUTEX UNLOCKED
	 *********************************/

#ifdef DEBUG_THUMBNAILS
	g_message ("(Main Thread) Unlocking mutex\n");
#endif
	pthread_mutex_unlock (&thumbnails_mutex);

	g_free (thumbnail_uri);

	return NULL;
}


/* Creates the thumbnail directory, and any parent directories, if it doesn't
   already exist. Returns TRUE on success. */
static gboolean
nautilus_thumbnail_create_directory (const char *thumbnail_uri)
{
	GnomeVFSURI *thumbnail_vfs_uri, *thumbnail_directory_uri;
	GnomeVFSResult result;

	thumbnail_vfs_uri = gnome_vfs_uri_new (thumbnail_uri);
	thumbnail_directory_uri = gnome_vfs_uri_get_parent (thumbnail_vfs_uri);
	result = eel_make_directory_and_parents (thumbnail_directory_uri,
						 THUMBNAIL_DIR_PERMISSIONS);
	gnome_vfs_uri_unref (thumbnail_directory_uri);
	gnome_vfs_uri_unref (thumbnail_vfs_uri);

	if (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_FILE_EXISTS) {
#ifdef DEBUG_THUMBNAILS
		g_message ("(Thumbnail Thread) Couldn't create thumbnail directory for: %s\n", thumbnail_uri);
#endif
		return FALSE;
	}

	return TRUE;
}

void
nautilus_update_thumbnail_file_renamed (const char *old_file_uri, const char *new_file_uri)
{
	char *old_thumbnail_uri, *new_thumbnail_uri;
	
	old_thumbnail_uri = make_thumbnail_uri (old_file_uri);
	if (old_thumbnail_uri != NULL && vfs_file_exists (old_thumbnail_uri)) {
		new_thumbnail_uri = make_thumbnail_uri (new_file_uri);

		g_assert (new_thumbnail_uri != NULL);

		if (nautilus_thumbnail_create_directory (new_thumbnail_uri))
			gnome_vfs_move (old_thumbnail_uri, new_thumbnail_uri,
					FALSE);

		g_free (new_thumbnail_uri);
	}

	g_free (old_thumbnail_uri);
}

void 
nautilus_remove_thumbnail_for_file (const char *old_file_uri)
{
	char *thumbnail_uri;
	
	thumbnail_uri = make_thumbnail_uri (old_file_uri);
	if (thumbnail_uri != NULL && vfs_file_exists (thumbnail_uri)) {
		gnome_vfs_unlink (thumbnail_uri);
	}

	g_free (thumbnail_uri);
}

/* Here is a heuristic compatibility routine to determine if a pixbuf
 * already has a frame around it or not.
 *
 * This only happens with thumbnails generated by earlier versions of
 * Nautilus, which used a fixed frame, so we can test for a few pixels
 * to detect it. This is biased toward being quick and saying yes,
 * since it's not that big a deal if we're wrong, and it looks better
 * to have no frame than two frames.
 */

static gboolean
pixel_matches_value (const guchar *pixels, guchar value)
{
	g_return_val_if_fail (pixels != NULL, FALSE);

	return pixels[0] == value
		&& pixels[1] == value
		&& pixels[2] == value;
}

static gboolean
pixbuf_is_framed (GdkPixbuf *pixbuf)
{
	const guchar *pixels;
	int row_stride;

	g_return_val_if_fail (pixbuf != NULL, FALSE);
	
	if (gdk_pixbuf_get_height (pixbuf) < 6
	    || gdk_pixbuf_get_width (pixbuf) < 6
	    || gdk_pixbuf_get_n_channels (pixbuf) != 4) {
		return FALSE;
	}

	pixels = gdk_pixbuf_get_pixels (pixbuf);
	row_stride = gdk_pixbuf_get_rowstride (pixbuf);

	g_assert (row_stride >= 12);

	return     pixel_matches_value (pixels,                      0xFF)
		&& pixel_matches_value (pixels +     row_stride + 4, 0x00)
		&& pixel_matches_value (pixels + 2 * row_stride + 8, 0xBB);
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
	if (pixbuf == NULL || pixbuf_is_framed (pixbuf)) {
		return pixbuf;
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

/* This is a timeout function that is run if 'convert' hasn't finished in a
   reasonable time. It kills the process with SIGKILL. Note that this is only
   a last resort, just in case convert is hanging. */
static gboolean
thumbnail_thread_convert_timeout_cb (gpointer data)
{
	pid_t *child_process_id = data;

#ifdef DEBUG_THUMBNAILS
	g_message ("Convert is hanging - killing\n");
#endif

	kill (*child_process_id, SIGKILL);

	return FALSE;
}


/* This runs the "convert" program from ImageMagick to try to create a
   thumbnail. It is used when gdk-pixbuf and librsvg can't handle the image
   format. It does a fork(), exec() and synchronous waitpid().
   We can do everything synchronously since we have our own thread.
   It returns TRUE on success, i.e. the thumbnail was created and is valid. */
static gboolean
thumbnail_thread_run_convert (NautilusThumbnailInfo *info,
			      char *thumbnail_path)
{
	char *image_path;
	pid_t pid;
	GdkPixbuf *pixbuf;
	GError *error = NULL;
	guint timeout_id;

	image_path = gnome_vfs_get_local_path_from_uri (info->image_uri);
	if (image_path == NULL)
		return FALSE;

	/* Fork a new process to exec "convert". */
	pid = fork ();

	/* If fork() failed, return FALSE. */
	if (pid == -1)
		return FALSE;

	/* The child process runs "convert" to convert the image to the
	   96x96 PNG thumbnail. */
	if (pid == 0) {
#ifdef DEBUG_THUMBNAILS
		g_message ("### Running convert %s -> %s\n",
			   image_path, thumbnail_path);
#endif

#if 1
		/* Redirect stdout to the path of the new thumbnail. We do
		   this because some versions of convert have problems with
		   the '%' characters that we use in thumbnail paths, so
		   we can't pass it in as a filename. */
		if (freopen (thumbnail_path, "w", stdout) == NULL) {
#ifdef DEBUG_THUMBNAILS
			g_message ("freopen failed!\n");
#endif
			return FALSE;
		}
	
		execlp ("convert", "convert",
			"-geometry",  "96x96",
			image_path, "png:-",
			NULL);
#else
		/* This was what the old version did, though it didn't work
		   for me on RedHat 7.1. convert complained about not finding
		   the files, and seemed confused by the '%' chars. */
		execlp ("convert", "convert",
			"-geometry",  "96x96",
			image_path, thumbnail_path,
			NULL);
#endif

		/* We exit() here just in case an error occurred when calling
		   execlp(). */
		_exit (0);
	}

	/* This is the parent process. First add a timeout in the mainloop to
	   kill the child convert process if it hasn't finished in a reasonable
	   amount of time. */
	timeout_id = g_timeout_add (THUMBNAIL_CONVERT_TIMEOUT, 
				    thumbnail_thread_convert_timeout_cb,
				    &pid);

	/* Now wait synchronously until the child exits. */
	for (;;) {
		/* We loop around in case we get EINTR. */
		pid_t terminated_pid = waitpid (pid, NULL, 0);

		/* If our child process exited, then we can continue. */
		if (terminated_pid == pid)
			break;

		/* If we get any error except EINTR, we shouldn't wait again
		   so we flag an error and break out of the loop. If waitpid()
		   returned -1 and errno was EINTR we loop round and call
		   waitpid() again. */
		if (terminated_pid != -1 || errno != EINTR) {
#ifdef DEBUG_THUMBNAILS
			g_message ("convert waitpid failed!\n");
#endif
			return FALSE;
		}
	}

	/* Remove our timeout, if it still exists. */
	g_source_remove (timeout_id);

#ifdef DEBUG_THUMBNAILS
	g_message ("=== Convert finished\n");
#endif

	/* Now check if the thumbnail created by convert exists and is valid.
	   I'm not sure how reliable convert is. For now we try to load the
	   thumbnail back in to check it is OK. Maybe we could just check for
	   an empty file. Note that we redirected to stdout, so if an error
	   occurred an empty file will probably be left there. */
	pixbuf = gdk_pixbuf_new_from_file (thumbnail_path, &error);
	if (error) {
#ifdef DEBUG_THUMBNAILS
		g_message ("gdk-pixbuf error: %s\n", error->message);
#endif
		g_error_free (error);
	}

	if (pixbuf != NULL) {
#ifdef DEBUG_THUMBNAILS
		g_message ("convert succeeded!\n");
#endif
		g_object_unref (pixbuf);
		return TRUE;
	} else {
#ifdef DEBUG_THUMBNAILS
		g_message ("convert failed: %s -> %s!\n",
			   image_path, thumbnail_path);
#endif
		return FALSE;
	}
}


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


static void
thumbnail_thread_finish_thumbnail (NautilusThumbnailInfo *info,
				   char *thumbnail_uri,
				   char *thumbnail_path)
{
#ifdef DEBUG_THUMBNAILS
	g_message ("(Thumbnail Thread) Finishing thumbnail\n");
#endif

	/* Set the mtime of the new thumbnail file to the same as the image
	   file, so we know when we need to update the thumbnail. */
	if (info->original_file_mtime != INVALID_MTIME) {
		GnomeVFSFileInfo *file_info;

		file_info = gnome_vfs_file_info_new ();
		file_info->mtime = info->original_file_mtime;
		/* we don't care about atime, but gnome-vfs
		 * makes us set it along with mtime.
		 * FIXME if we weren't lame, we would
		 * perhaps read the old atime and set it back,
		 * but we're lame.
		 */
		file_info->atime = info->original_file_mtime;

		gnome_vfs_set_file_info (thumbnail_uri,
					 file_info,
					 GNOME_VFS_SET_FILE_INFO_TIME);
							 
		gnome_vfs_file_info_unref (file_info);
	}
}


/* Creating the thumbnail failed, so we remove the thumbnail file if it exists,
   and create a special file to flag that we failed to create it. */
static void
thumbnail_thread_cancel_thumbnail (NautilusThumbnailInfo *info,
				   char *thumbnail_uri,
				   char *thumbnail_path)
{
	char *invalid_uri;
	GnomeVFSResult result;
	GnomeVFSHandle *handle;

#ifdef DEBUG_THUMBNAILS
	g_message ("(Thumbnail Thread) Cancelling thumbnail\n");
#endif

	/* Remove any invalid thumbnail that may have been created. */
	if (g_file_test (thumbnail_path, G_FILE_TEST_EXISTS)) {
		unlink (thumbnail_path);
	}

	/* Create a special file to flag that we tried and failed to create
	   a thumbnail for this image. */
	invalid_uri = make_invalid_thumbnail_uri (thumbnail_uri);
	result = gnome_vfs_create (&handle, invalid_uri, GNOME_VFS_OPEN_WRITE,
				   FALSE, THUMBNAIL_PLACEHOLDER_PERMISSIONS);
	if (result == GNOME_VFS_OK) {
		gnome_vfs_close (handle);
	} else {
#ifdef DEBUG_THUMBNAILS
		g_message ("(Thumbnail Thread) Error creating invalid thumbnail file: %s\n", invalid_uri);
#endif
	}
	g_free (invalid_uri);
}


/* This creates one thumbnail in the thumbnail thread. */
static void
thumbnail_thread_make_thumbnail (NautilusThumbnailInfo *info)
{
	GdkPixbuf* full_size_image = NULL;
	char *thumbnail_uri, *thumbnail_path;
	gboolean success = TRUE;
			
#ifdef DEBUG_THUMBNAILS
	g_message ("(Thumbnail Thread) In make_thumbnail: %s\n",
		   info->image_uri);
#endif

	/* Create the URI to save the thumbnail icon in, and the corresponding
	   local path. */
	thumbnail_uri = make_thumbnail_uri (info->image_uri);
	if (thumbnail_uri == NULL) {
		return;
	}

	thumbnail_path = gnome_vfs_get_local_path_from_uri (thumbnail_uri);
	if (thumbnail_path == NULL) {
		g_free (thumbnail_uri);
		return;
	}
				
	/* Create the thumbnail directory, if it doesn't already exist. */
	if (!nautilus_thumbnail_create_directory (thumbnail_uri)) {
		/* If we couldn't create the directory, just return. */
		return;
	}

	/* For SVG images we use librsvg to create a full-size pixbuf.
	   For JPEGs we use special, fast code to create a reduced-size pixbuf
	   though we still need to scale it afterwards.
	   For other images we try to load them with gdk-pixbuf. If that fails
	   we try to use the ImageMagick "convert" program to convert them to
	   png format at the desired size. */
	if (eel_strcasecmp (info->mime_type, "image/svg") == 0) {
		char *image_path = gnome_vfs_get_local_path_from_uri (info->image_uri);
		if (image_path != NULL) {
			full_size_image = rsvg_pixbuf_from_file_at_max_size (image_path, 96, 96, NULL);
			g_free (image_path);
		}
#ifdef HAVE_LIBJPEG
	} else if (eel_strcasecmp (info->mime_type, "image/jpeg") == 0) {
		full_size_image = nautilus_thumbnail_load_scaled_jpeg
			(info->image_uri, 96, 96);
#endif
	} else {
		full_size_image = eel_gdk_pixbuf_load (info->image_uri);
	}
			
	/* If we have managed to create a pixbuf from the image, scale it to
	   thumbnail size and save it. Otherwise fall back on running
	   "convert" to create the thumbnail. */
	if (full_size_image != NULL) {
		GdkPixbuf *scaled_image;

		/* Scale the image to thumbnail size. */	
		scaled_image = eel_gdk_pixbuf_scale_down_to_fit (full_size_image, 96, 96);	
		g_object_unref (full_size_image);

		/* We trust gdk-pixbuf to save the image correctly.
		   So if it fails we output a warning. */
#ifdef DEBUG_THUMBNAILS
		g_message ("Saving thumbnail to: %s\n", thumbnail_path);
#endif
		if (!eel_gdk_pixbuf_save_to_file (scaled_image,
						  thumbnail_path)) {
			success = FALSE;
			g_warning ("error saving thumbnail %s",
				   thumbnail_path);
		}
		g_object_unref (scaled_image);
	} else {
		success = thumbnail_thread_run_convert (info, thumbnail_path);
	}
			
	/* If we created the thumbnail successfully, set the mtime of the
	   thumbnail to match the image file, so we know when we need to
	   remake it. If we failed to create a thumbnail then remove the
	   thumbnail file and create a special file to flag that we tried
	   and failed. */
	if (success) {
		thumbnail_thread_finish_thumbnail (info, thumbnail_uri,
						   thumbnail_path);
	} else {
		thumbnail_thread_cancel_thumbnail (info, thumbnail_uri,
						   thumbnail_path);
	}

	/* We need to call nautilus_file_changed(), but I don't think that is
	   thread safe. So add an idle handler and do it from the main loop. */
	g_idle_add_full (G_PRIORITY_HIGH_IDLE,
			 thumbnail_thread_notify_file_changed,
			 g_strdup (info->image_uri), NULL);

	g_free (thumbnail_path);
	g_free (thumbnail_uri);
}



/* thumbnail_thread is invoked as a separate thread to to make thumbnails. */
static gpointer
thumbnail_thread_start (gpointer data)
{
	NautilusThumbnailInfo *info = NULL;

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
		thumbnail_thread_make_thumbnail (info);
	}
}
