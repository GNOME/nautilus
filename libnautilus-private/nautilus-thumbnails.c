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
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* permissions for thumbnail directory */
#define THUMBNAIL_DIR_PERMISSIONS (GNOME_VFS_PERM_USER_ALL \
				   | GNOME_VFS_PERM_GROUP_ALL \
				   | GNOME_VFS_PERM_OTHER_ALL)

#define THUMBNAIL_PLACEHOLDER_PERMISSIONS (GNOME_VFS_PERM_USER_READ | \
					   GNOME_VFS_PERM_USER_WRITE | \
					   GNOME_VFS_PERM_GROUP_READ | \
					   GNOME_VFS_PERM_GROUP_WRITE | \
					   GNOME_VFS_PERM_OTHER_READ)
/* thumbnail task state */
static GList *thumbnails;
static char *new_thumbnail_uri;
static gboolean thumbnail_in_progress;
	
/* id of timeout task for making thumbnails */
static int thumbnail_timeout_id;

static int make_thumbnails (gpointer data);

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

	/* FIXME bugzilla.eazel.com 3137: The synchronous I/O here
	 * means this call is unsuitable for use on anything that
	 * might be remote.
	 */
	result = gnome_vfs_uri_exists (uri);
	gnome_vfs_uri_unref (uri);

	return result;
}

/* FIXME Unportable Linux-specific hack-o-rama */
#include <sys/vfs.h>
#include <errno.h>
#define NFS_SUPER_MAGIC       0x6969
static gboolean
is_nfs_filesystem (const char *uri)
{
	char *local_path;
	gboolean retval;
	struct statfs sb;

	retval = FALSE;
	
	local_path = gnome_vfs_get_local_path_from_uri (uri);

	if (statfs (local_path, &sb) < 0) {
		g_warning ("Could not statfs %s: %s",
			   local_path, strerror (errno));
	} else {
		retval = (sb.f_type == NFS_SUPER_MAGIC);
	}
	
	g_free (local_path);

	return retval;
}

static gboolean
uri_is_local (const char *uri)
{
	gboolean is_local;
	GnomeVFSURI *vfs_uri;

	if (is_nfs_filesystem (uri))
		return FALSE;
	
	vfs_uri = gnome_vfs_uri_new (uri);
	is_local = gnome_vfs_uri_is_local (vfs_uri);
	gnome_vfs_uri_unref (vfs_uri);

	return is_local;
}

static gboolean
prefer_global_thumbnails_location (const char *image_uri)
{
	static int public_metadata_preference;
	static gboolean public_metadata_preference_registered;

	if (!public_metadata_preference_registered) {
		eel_preferences_add_auto_integer (NAUTILUS_PREFERENCES_USE_PUBLIC_METADATA,
						  &public_metadata_preference);
		public_metadata_preference_registered = TRUE;
	}

	if (public_metadata_preference == NAUTILUS_SPEED_TRADEOFF_NEVER) {
		return TRUE;
	}

	if (public_metadata_preference == NAUTILUS_SPEED_TRADEOFF_ALWAYS) {
		return FALSE;
	}

	g_assert (public_metadata_preference == NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY);
	return !uri_is_local (image_uri);
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
make_thumbnail_uri (const char *image_uri, gboolean directory_only, gboolean use_local_directory,
		    gboolean anti_aliased, gboolean create_parents_if_needed)
{
	char *thumbnail_uri, *thumbnail_path;
	char *directory_name = g_strdup (image_uri);
	char *last_slash = strrchr (directory_name, '/');
	char *dot_pos, *slash_pos;
	
	*last_slash = '\0';
	
	/* Either use the local directory or one in the user's home directory, 
	 * as selected by the passed in flag and possibly overridden by user
	 * preference.
	 */
	/* FIXME: Most callers set use_local_directory to the result of 
	 * uri_is_local (image_uri), which is then done again in
	 * prefer_global_thumbnails_location. This should be cleaned up.
	 */
	if (use_local_directory && !prefer_global_thumbnails_location (image_uri)) {
		thumbnail_uri = g_strdup_printf ("%s/.thumbnails", directory_name);
	} else  {
		GnomeVFSResult result;
		GnomeVFSURI *thumbnail_directory_uri;
	        	
		char *escaped_uri = gnome_vfs_escape_slashes (directory_name);
		char *protected_uri = obfuscate_password (escaped_uri);
		g_free (escaped_uri);
		thumbnail_path = g_strdup_printf ("%s/.nautilus/thumbnails/%s", g_get_home_dir(), protected_uri);
		thumbnail_uri = gnome_vfs_get_uri_from_local_path (thumbnail_path);
		g_free (thumbnail_path);
		g_free (protected_uri);
		
		/* we must create the directory if it doesn't exist */
			
		thumbnail_directory_uri = gnome_vfs_uri_new (thumbnail_uri);
		
		if (!create_parents_if_needed) {
			if (!gnome_vfs_uri_exists (thumbnail_directory_uri)) {
				gnome_vfs_uri_unref (thumbnail_directory_uri);
				return NULL;
			}
		}
		/* FIXME bugzilla.eazel.com 3137: synchronous I/O - it
                   looks like the URI will be local-only, but best to
                   make sure. */

		result = eel_make_directory_and_parents (thumbnail_directory_uri, THUMBNAIL_DIR_PERMISSIONS);
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
			slash_pos = strrchr (thumbnail_uri, '/');
			if (dot_pos && dot_pos > slash_pos) {
				*dot_pos = '\0';
				thumbnail_uri = g_strdup_printf ("%s.aa.%s", old_uri, dot_pos + 1);
			} else {
				thumbnail_uri = g_strconcat (old_uri, ".aa", NULL);				
			}
			g_free (old_uri);
		}
		
		/* append an image suffix if the correct one isn't already present */
		if (!eel_istr_has_suffix (image_uri, ".png")) {		
			char *old_uri = thumbnail_uri;
			thumbnail_uri = g_strdup_printf ("%s.png", thumbnail_uri);
			g_free(old_uri);			
		}
	}
			
	g_free (directory_name);
	return thumbnail_uri;
}

/* utility routine that takes two uris and returns true if the first file has been modified later than the second */
/* FIXME bugzilla.eazel.com 2565: it makes synchronous file info calls, so for now, it returns FALSE if either of the uri's are non-local */
static gboolean
first_file_more_recent (const char* file_uri, const char* other_file_uri)
{
	gboolean more_recent;
	
	GnomeVFSFileInfo *file_info, *other_file_info;

	/* if either file is remote, return FALSE.  Eventually we'll make this async to fix this */
	if (!uri_is_local (file_uri) || !uri_is_local (other_file_uri)) {
		return FALSE;
	}
	
	/* gather the info and then compare modification times */
	file_info = gnome_vfs_file_info_new ();
	gnome_vfs_get_file_info (file_uri, file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);
	
	other_file_info = gnome_vfs_file_info_new ();
	gnome_vfs_get_file_info (other_file_uri, other_file_info, GNOME_VFS_FILE_INFO_FOLLOW_LINKS);

	more_recent = file_info->mtime > other_file_info->mtime;

	gnome_vfs_file_info_unref (file_info);
	gnome_vfs_file_info_unref (other_file_info);

	return more_recent;
}

/* structure used for making thumbnails, associating a uri with where the thumbnail is to be stored */

typedef struct {
	char *thumbnail_uri;
	gboolean is_local;
	gboolean anti_aliased;
	pid_t thumbnail_task;
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
gboolean nautilus_thumbnail_has_invalid_thumbnail (NautilusFile *file,
						   gboolean anti_aliased)
{
	char *file_uri, *thumbnail_uri, *invalid_thumbnail_uri;
	gboolean is_invalid;
	
	file_uri = nautilus_file_get_uri (file);
	
	
	thumbnail_uri = make_thumbnail_uri (file_uri, FALSE, uri_is_local (file_uri), anti_aliased, TRUE);
	invalid_thumbnail_uri = make_invalid_thumbnail_uri (thumbnail_uri);
	
	is_invalid = vfs_file_exists (invalid_thumbnail_uri);
	
	g_free (file_uri);
	g_free (thumbnail_uri);
	g_free (invalid_thumbnail_uri);
	return is_invalid;
}

/* routine that takes a uri of a large image file and returns the uri of its corresponding thumbnail.
   If no thumbnail is available, put the image on the thumbnail queue so one is eventually made. */
/* FIXME bugzilla.eazel.com 642: 
 * Most of this thumbnail machinery belongs in NautilusFile, not here.
 */

char *
nautilus_get_thumbnail_uri (NautilusFile *file, gboolean anti_aliased)
{
	GnomeVFSResult result;
	char *thumbnail_uri;
	char *file_uri;
	gboolean can_write;
	NautilusFile *destination_file;
	gboolean local_flag = TRUE;
	gboolean  remake_thumbnail = FALSE;
	
	file_uri = nautilus_file_get_uri (file);
		
	thumbnail_uri = make_thumbnail_uri (file_uri, FALSE, uri_is_local (file_uri), anti_aliased, TRUE);
		
	/* if the thumbnail file already exists locally, simply return the uri */
	
	/* note: thumbnail_uri is always local here, so it's not a disaster that we make a synchronous call below.
	   Eventually, we'll want to do everything asynchronously, when we have time to restructure the thumbnail engine */
	if (vfs_file_exists (thumbnail_uri)) {
		
		/* see if the file changed since it was thumbnailed by comparing the modification time */
		remake_thumbnail = first_file_more_recent (file_uri, thumbnail_uri);
		
		/* if the file hasn't changed, return the thumbnail uri */
		if (!remake_thumbnail) {
			g_free (file_uri);
			return thumbnail_uri;
		} else {
			nautilus_icon_factory_remove_by_uri (thumbnail_uri);

			/* the thumbnail uri is always local, so we can do synchronous I/O to delete it */
 			gnome_vfs_unlink (thumbnail_uri);
		}
	}
	
	/* now try it globally */
	if (!remake_thumbnail) {
		g_free (thumbnail_uri);
		thumbnail_uri = make_thumbnail_uri (file_uri, FALSE, FALSE, anti_aliased, TRUE);
		
		/* if the thumbnail file already exists in the common area,  return that uri, */
		/* the uri is guaranteed to be local */
		if (vfs_file_exists (thumbnail_uri)) {
		
			/* see if the file changed since it was thumbnailed by comparing the modification time */
			remake_thumbnail = first_file_more_recent(file_uri, thumbnail_uri);
		
			/* if the file hasn't changed, return the thumbnail uri */
			if (!remake_thumbnail) {
				g_free (file_uri);
				return thumbnail_uri;
			} else {
				nautilus_icon_factory_remove_by_uri (thumbnail_uri);

				/* the uri is guaranteed to be local */
				gnome_vfs_unlink (thumbnail_uri);
			}
		}
	}
	
        /* make the thumbnail directory if necessary, at first try it locally */
	g_free (thumbnail_uri);
	local_flag = TRUE;
	thumbnail_uri = make_thumbnail_uri (file_uri, TRUE, local_flag, anti_aliased, TRUE);
				
	/* FIXME bugzilla.eazel.com 3137: more potentially losing
	   synch I/O - this could be remote */

	result = gnome_vfs_make_directory (thumbnail_uri, THUMBNAIL_DIR_PERMISSIONS);
	
	/* the directory could already exist, but we better make sure we can write to it */
	destination_file = nautilus_file_get (thumbnail_uri);
	can_write = FALSE;
	if (destination_file != NULL) {
		can_write = nautilus_file_can_write (destination_file);
		nautilus_file_unref (destination_file);
	}

	/* if we can't make if locally, try it in the global place */
	if (!can_write || (result != GNOME_VFS_OK && result != GNOME_VFS_ERROR_FILE_EXISTS)) {	
		g_free (thumbnail_uri);
		local_flag = FALSE;
		thumbnail_uri = make_thumbnail_uri (file_uri, TRUE, local_flag, anti_aliased, TRUE);
		/* this is guaranteed to be local, so synch I/O can be tolerated here */
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
		
		if (g_list_find_custom (thumbnails, info, compare_thumbnail_info) == NULL) {
			thumbnails = g_list_append (thumbnails, info);
		}
	
		if (thumbnail_timeout_id == 0) {
			thumbnail_timeout_id = gtk_timeout_add
				(400, make_thumbnails, NULL);
		}
	}
	
	g_free (thumbnail_uri);
	
	/* Return NULL to indicate the thumbnail is loading. */
	return NULL;
}

static void 
nautilus_update_thumbnail_file_renamed_one (const char *old_file_uri, const char *new_file_uri,
	gboolean anti_aliased)
{
	gboolean is_local;
	char *old_thumbnail_uri, *new_thumbnail_uri;
	
	is_local = uri_is_local (old_file_uri);
	
	old_thumbnail_uri = make_thumbnail_uri (old_file_uri, FALSE, is_local, anti_aliased, FALSE);
	if (old_thumbnail_uri != NULL && vfs_file_exists (old_thumbnail_uri)) {
		new_thumbnail_uri = make_thumbnail_uri (new_file_uri, FALSE, is_local, anti_aliased, FALSE);

		g_assert (new_thumbnail_uri != NULL);

		gnome_vfs_move (old_thumbnail_uri, new_thumbnail_uri, FALSE);

		g_free (new_thumbnail_uri);
	}

	g_free (old_thumbnail_uri);
}

/* update the thumbnail after the thumbnailed file got renamed */
void 
nautilus_update_thumbnail_file_renamed (const char *old_file_uri, const char *new_file_uri)
{
	/* rename both the AA and non-AA thumbnails, if they exist */
	nautilus_update_thumbnail_file_renamed_one (old_file_uri, new_file_uri, FALSE);
	nautilus_update_thumbnail_file_renamed_one (old_file_uri, new_file_uri, TRUE);
}

static void 
nautilus_remove_thumbnail_for_file_one (const char *old_file_uri, gboolean anti_aliased)
{
	char *thumbnail_uri;
	
	thumbnail_uri = make_thumbnail_uri (old_file_uri, FALSE, uri_is_local (old_file_uri), anti_aliased, FALSE);
	if (thumbnail_uri != NULL && vfs_file_exists (thumbnail_uri)) {
		gnome_vfs_unlink (thumbnail_uri);
	}

	g_free (thumbnail_uri);
}

/* remove the thumbnail after the thumbnailed file got deleted */
void 
nautilus_remove_thumbnail_for_file (const char *old_file_uri)
{
	/* remove both the AA and non-AA thumbnails, if they exist */
	nautilus_remove_thumbnail_for_file_one (old_file_uri, FALSE);
	nautilus_remove_thumbnail_for_file_one (old_file_uri, TRUE);
}

/* check_for_thumbnails is a utility that checks to see if the current thumbnail task has terminated.
   If it has, remove the thumbnail info from the queue and return TRUE; if it's still in progress, return FALSE.
*/

static gboolean 
check_for_thumbnails (void)
{
	NautilusThumbnailInfo *info;
	GList *head;
	NautilusFile *file;
	int status;
	char *current_thumbnail, *invalid_uri;
	gboolean task_terminated;
	gboolean need_update;
	GnomeVFSResult result;
	GnomeVFSHandle *handle;
	
	info = (NautilusThumbnailInfo*) thumbnails->data;
	
	task_terminated = waitpid (info->thumbnail_task, &status, WNOHANG) != 0;	
	if (task_terminated) {
		/* the thumbnail task has completed, so update the current entry from the list */
		file = nautilus_file_get (info->thumbnail_uri);

		current_thumbnail = make_thumbnail_uri (info->thumbnail_uri, FALSE, info->is_local,
			info->anti_aliased, TRUE);
		
		/* if a thumbnail wasn't successfully made, create a placeholder to flag that we tried */
		need_update = TRUE;
		if (!vfs_file_exists (current_thumbnail)) {
			invalid_uri = make_invalid_thumbnail_uri (current_thumbnail);
			result = gnome_vfs_create (&handle, invalid_uri, GNOME_VFS_OPEN_WRITE,
						   FALSE, THUMBNAIL_PLACEHOLDER_PERMISSIONS);

			if (result == GNOME_VFS_OK) {
				gnome_vfs_close (handle);
			}
			
			g_free (invalid_uri);
		}
		
		/* update the file's icon */		
		if (file != NULL && need_update) {
			nautilus_file_changed (file);
		}
		
		if (file != NULL) {
			nautilus_file_unref (file);
		}
		
		g_free (current_thumbnail);
		g_free (info->thumbnail_uri);
		g_free (info);
		
		/* remove it from the queue */
		head = thumbnails;
		thumbnails = g_list_remove_link (thumbnails, head);
		g_list_free_1 (head);
		
		return TRUE;
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
	image_path = nautilus_theme_get_image_path (anti_aliased ? "thumbnail_frame.aa.png" : "thumbnail_frame.png");
	frame_image = gdk_pixbuf_new_from_file (image_path);
	g_free (image_path);
	return frame_image;
}

static int
make_thumbnails (gpointer data)
{
	NautilusThumbnailInfo *info;
	GList *next_thumbnail = thumbnails;
	GdkPixbuf *scaled_image, *framed_image, *thumbnail_image_frame;
	char *frame_offset_str;
	int left_offset, top_offset, right_offset, bottom_offset;
	
	/* if the queue is empty, there's nothing more to do */
	if (next_thumbnail == NULL) {
		gtk_timeout_remove (thumbnail_timeout_id);
		thumbnail_timeout_id = 0;
		return FALSE;
	}
	
	info = (NautilusThumbnailInfo *) next_thumbnail->data;
	
	/* see which state we're in.  If a thumbnail isn't in progress, start one up.  Otherwise,
	   check if the pending one is completed.  */	
	if (thumbnail_in_progress) {
		if (check_for_thumbnails ()) {
			thumbnail_in_progress = FALSE;
		}
	} else {
		/* start up a task to make the thumbnail corresponding to the queue element. */
			
		/* First, compute the path name of the target thumbnail */
		g_free (new_thumbnail_uri);
		new_thumbnail_uri = make_thumbnail_uri (info->thumbnail_uri, FALSE, info->is_local,
							 info->anti_aliased, TRUE);
		
		/* fork a task to make the thumbnail, using gdk-pixbuf to do the scaling */
		if (!(info->thumbnail_task = fork())) {
			GdkPixbuf* full_size_image;
			NautilusFile *file;
			GnomeVFSFileSize file_size;
			char *thumbnail_path;
			
			file = nautilus_file_get (info->thumbnail_uri);
			file_size = nautilus_file_get_size (file);
			full_size_image = NULL;

			if (nautilus_file_is_mime_type (file, "image/svg")) {
				thumbnail_path = gnome_vfs_get_local_path_from_uri (info->thumbnail_uri);
				if (thumbnail_path != NULL) {
					FILE *f = fopen (thumbnail_path, "rb");
					if (f != NULL) {
						full_size_image = rsvg_render_file (f, 1.0);
						fclose (f);
					}
				}
#ifdef HAVE_LIBJPEG
			} else if (nautilus_file_is_mime_type (file, "image/jpeg")) {
				if (info->thumbnail_uri != NULL) {
					full_size_image = nautilus_thumbnail_load_scaled_jpeg
						(info->thumbnail_uri, 96, 96);
				}
#endif
			} else {
				if (info->thumbnail_uri != NULL) {
					full_size_image = eel_gdk_pixbuf_load (info->thumbnail_uri);
				}
			}
			nautilus_file_unref (file);
			
			if (full_size_image != NULL) {				
				thumbnail_image_frame = load_thumbnail_frame (info->anti_aliased);
									
				/* scale the content image as necessary */	
				scaled_image = eel_gdk_pixbuf_scale_down_to_fit (full_size_image, 96, 96);	
				gdk_pixbuf_unref (full_size_image);
				
				/* embed the content image in the frame, if necessary  */
				if (file_size > SELF_THUMBNAIL_SIZE_THRESHOLD) {

					frame_offset_str = nautilus_theme_get_theme_data ("thumbnails", "FRAME_OFFSETS");
					if (frame_offset_str != NULL) {
						sscanf (frame_offset_str," %d , %d , %d , %d %*s",
							&left_offset, &top_offset, &right_offset, &bottom_offset);
					} else {
						/* use nominal values since the info in the theme couldn't be found */
						left_offset = 3; top_offset = 3;
						right_offset = 6; bottom_offset = 6;
					}
					
					framed_image = eel_embed_image_in_frame (scaled_image, thumbnail_image_frame,
										 left_offset, top_offset, right_offset, bottom_offset);
					g_free (frame_offset_str);
				
					gdk_pixbuf_unref (scaled_image);
					gdk_pixbuf_unref (thumbnail_image_frame);
				} else {
					framed_image = scaled_image;
				}
				
				thumbnail_path = gnome_vfs_get_local_path_from_uri (new_thumbnail_uri);
				if (thumbnail_path == NULL
				    || !eel_gdk_pixbuf_save_to_file (framed_image, thumbnail_path)) {
					g_warning ("error saving thumbnail %s", thumbnail_path);
				}
				g_free (thumbnail_path);
				gdk_pixbuf_unref (framed_image);
			} else {
				/* gdk-pixbuf couldn't load the image, so trying using ImageMagick */
				char *temp_str;

				thumbnail_path = gnome_vfs_get_local_path_from_uri (new_thumbnail_uri);
				if (thumbnail_path != NULL) {
					temp_str = g_strdup_printf ("png:%s", thumbnail_path);
					g_free (thumbnail_path);
					
					thumbnail_path = gnome_vfs_get_local_path_from_uri (info->thumbnail_uri);
					if (thumbnail_path != NULL) {
						
						/* scale the image */
						execlp ("convert", "convert", "-geometry",  "96x96", thumbnail_path, temp_str, NULL);
					}
				}
				
				/* we don't come back from this call, so no point in freeing anything up */
			}
			
			_exit(0);
		}
		thumbnail_in_progress = TRUE;
	}
	
	return TRUE;  /* we're not done yet */
}
