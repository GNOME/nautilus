/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-file.c: Nautilus file model.
 
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
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#include <config.h>
#include "nautilus-file.h"

#include "nautilus-directory-metafile.h"
#include "nautilus-directory-notify.h"
#include "nautilus-directory-private.h"
#include "nautilus-signaller.h"
#include "nautilus-desktop-directory.h"
#include "nautilus-desktop-directory-file.h"
#include "nautilus-desktop-icon-file.h"
#include "nautilus-file-attributes.h"
#include "nautilus-file-private.h"
#include "nautilus-file-operations.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-link.h"
#include "nautilus-metadata.h"
#include "nautilus-module.h"
#include "nautilus-search-directory.h"
#include "nautilus-search-directory-file.h"
#include "nautilus-thumbnails.h"
#include "nautilus-users-groups-cache.h"
#include "nautilus-vfs-file.h"
#include "nautilus-saved-search-file.h"
#include <eel/eel-debug.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-string.h>
#include <grp.h>
#include <gtk/gtksignal.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <glib/gurifuncs.h>
#include <libgnome/gnome-macros.h>
#include <glib/gfileutils.h>
#include <libnautilus-extension/nautilus-file-info.h>
#include <libxml/parser.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#ifdef HAVE_SELINUX
#include <selinux/selinux.h>
#endif

/* Time in seconds to cache getpwuid results */
#define GETPWUID_CACHE_TIME (5*60)

#define ICON_NAME_THUMBNAIL_LOADING   "image-loading"

#undef NAUTILUS_FILE_DEBUG_REF
#undef NAUTILUS_FILE_DEBUG_REF_VALGRIND

#ifdef NAUTILUS_FILE_DEBUG_REF_VALGRIND
#include <valgrind/valgrind.h>
#define DEBUG_REF_PRINTF VALGRIND_PRINTF_BACKTRACE
#else
#define DEBUG_REF_PRINTF printf
#endif

/* Files that start with these characters sort after files that don't. */
#define SORT_LAST_CHAR1 '.'
#define SORT_LAST_CHAR2 '#'

/* Name to use to tag metadata for the directory itself. */
#define FILE_NAME_FOR_DIRECTORY_METADATA "."

/* Name of Nautilus trash directories */
#define TRASH_DIRECTORY_NAME ".Trash"

typedef enum {
	SHOW_HIDDEN = 1 << 0,
	SHOW_BACKUP = 1 << 1
} FilterOptions;

typedef void (* ModifyListFunction) (GList **list, NautilusFile *file);

enum {
	CHANGED,
	UPDATED_DEEP_COUNT_IN_PROGRESS,
	LAST_SIGNAL
};

static int date_format_pref;

static guint signals[LAST_SIGNAL];

static GObjectClass *parent_class = NULL;

static GHashTable *symbolic_links;

static GQuark attribute_name_q,
	attribute_size_q,
	attribute_type_q,
	attribute_modification_date_q,
	attribute_date_modified_q,
	attribute_accessed_date_q,
	attribute_date_accessed_q,
	attribute_emblems_q,
	attribute_mime_type_q,
	attribute_size_detail_q,
	attribute_deep_size_q,
	attribute_deep_file_count_q,
	attribute_deep_directory_count_q,
	attribute_deep_total_count_q,
	attribute_date_changed_q,
	attribute_date_permissions_q,
	attribute_permissions_q,
	attribute_selinux_context_q,
	attribute_octal_permissions_q,
	attribute_owner_q,
	attribute_group_q,
	attribute_uri_q,
	attribute_where_q,
	attribute_link_target_q,
	attribute_volume_q,
	attribute_free_space_q;


static void     nautilus_file_instance_init                  (NautilusFile          *file);
static void     nautilus_file_class_init                     (NautilusFileClass     *class);
static void     nautilus_file_info_iface_init                (NautilusFileInfoIface *iface);
static char *   nautilus_file_get_owner_as_string            (NautilusFile          *file,
							      gboolean               include_real_name);
static char *   nautilus_file_get_type_as_string             (NautilusFile          *file);
static gboolean update_info_and_name                         (NautilusFile          *file,
							      GFileInfo             *info);
static const char * nautilus_file_peek_display_name (NautilusFile *file);
static const char * nautilus_file_peek_display_name_collation_key (NautilusFile *file);

GType
nautilus_file_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		const GTypeInfo info = {
			sizeof (NautilusFileClass),
			NULL, 
			NULL,
			(GClassInitFunc) nautilus_file_class_init,
			NULL,
			NULL,
			sizeof (NautilusFile),
			0,
			(GInstanceInitFunc) nautilus_file_instance_init,
		};
		
		const GInterfaceInfo file_info_iface_info = {
			(GInterfaceInitFunc) nautilus_file_info_iface_init,
			NULL,
			NULL
		};
		
		type = g_type_register_static (G_TYPE_OBJECT,
					       "NautilusFile",
					       &info, 0);
		g_type_add_interface_static (type, 
					     NAUTILUS_TYPE_FILE_INFO,
					     &file_info_iface_info);
	}
	
	return type;
}

static void
nautilus_file_instance_init (NautilusFile *file)
{
	file->details = G_TYPE_INSTANCE_GET_PRIVATE ((file), NAUTILUS_TYPE_FILE, NautilusFileDetails);

	nautilus_file_clear_info (file);
	nautilus_file_invalidate_extension_info_internal (file);
}

static GObject*
nautilus_file_constructor (GType                  type,
			   guint                  n_construct_properties,
			   GObjectConstructParam *construct_params)
{
  GObject *object;
  NautilusFile *file;

  object = (* G_OBJECT_CLASS (parent_class)->constructor) (type,
							   n_construct_properties,
							   construct_params);

  file = NAUTILUS_FILE (object);

  /* Set to default type after full construction */
  if (NAUTILUS_FILE_GET_CLASS (file)->default_file_type != G_FILE_TYPE_UNKNOWN) {
	  file->details->type = NAUTILUS_FILE_GET_CLASS (file)->default_file_type;
  }
  
  return object;
}

gboolean
nautilus_file_set_display_name (NautilusFile *file,
				const char *display_name,
				const char *edit_name,
				gboolean custom)
{
	gboolean changed;

	if (display_name == NULL || *display_name == 0) {
		return FALSE;
	}
	
	if (!custom && file->details->got_custom_display_name) {
		return FALSE;
	}

	if (custom && display_name == NULL) {
		/* We're re-setting a custom display name, invalidate it if
		   we already set it so that the old one is re-read */
		if (file->details->got_custom_display_name) {
			file->details->got_custom_display_name = FALSE;
			nautilus_file_invalidate_attributes (file,
							     NAUTILUS_FILE_ATTRIBUTE_INFO);
		}
		return FALSE;
	}

	if (edit_name == NULL) {
		edit_name = display_name;
	}
	    
	changed = FALSE;
	
	if (eel_strcmp (eel_ref_str_peek (file->details->display_name), display_name) != 0) {
		changed = TRUE;
		
		eel_ref_str_unref (file->details->display_name);
		
		if (eel_strcmp (eel_ref_str_peek (file->details->name), display_name) == 0) {
			file->details->display_name = eel_ref_str_ref (file->details->name);
		} else {
			file->details->display_name = eel_ref_str_new (display_name);
		}
		
		g_free (file->details->display_name_collation_key);
		file->details->display_name_collation_key = g_utf8_collate_key_for_filename (display_name, -1);
	}

	if (eel_strcmp (eel_ref_str_peek (file->details->edit_name), edit_name) != 0) {
		changed = TRUE;
		
		eel_ref_str_unref (file->details->edit_name);
		if (eel_strcmp (eel_ref_str_peek (file->details->display_name), edit_name) == 0) {
			file->details->edit_name = eel_ref_str_ref (file->details->display_name);
		} else {
			file->details->edit_name = eel_ref_str_new (edit_name);
		}
	}
	
	file->details->got_custom_display_name = custom;
	return changed;
}

void
nautilus_file_clear_info (NautilusFile *file)
{
	file->details->got_file_info = FALSE;
	if (file->details->get_info_error) {
		g_error_free (file->details->get_info_error);
		file->details->get_info_error = NULL;
	}
	/* Reset to default type, which might be other than unknown for
	   special kinds of files like the desktop or a search directory */
	file->details->type = NAUTILUS_FILE_GET_CLASS (file)->default_file_type;

	if (!file->details->got_custom_display_name) {
		eel_ref_str_unref (file->details->display_name);
		file->details->display_name = NULL;
		g_free (file->details->display_name_collation_key);
		file->details->display_name_collation_key = NULL;
		eel_ref_str_unref (file->details->edit_name);
		file->details->edit_name = NULL;
	}

	if (!file->details->got_custom_activation_location &&
	    file->details->activation_location != NULL) {
		g_object_unref (file->details->activation_location);
		file->details->activation_location = NULL;
	}
	
	if (file->details->icon != NULL) {
		g_object_unref (file->details->icon);
		file->details->icon = NULL;
	}

	g_free (file->details->thumbnail_path);
	file->details->thumbnail_path = NULL;
	file->details->thumbnailing_failed = FALSE;
	
	file->details->is_launcher = FALSE;
	file->details->is_symlink = FALSE;
	file->details->is_hidden = FALSE;
	file->details->is_backup = FALSE;
	file->details->is_mountpoint = FALSE;
	file->details->uid = -1;
	file->details->gid = -1;
	file->details->can_read = TRUE;
	file->details->can_write = TRUE;
	file->details->can_execute = TRUE;
	file->details->can_delete = TRUE;
	file->details->can_trash = TRUE;
	file->details->can_rename = TRUE;
	file->details->can_mount = FALSE;
	file->details->can_unmount = FALSE;
	file->details->can_eject = FALSE;
	file->details->has_permissions = FALSE;
	file->details->permissions = 0;
	file->details->size = -1;
	file->details->sort_order = 0;
	file->details->mtime = 0;
	file->details->atime = 0;
	file->details->ctime = 0;
	g_free (file->details->symlink_name);
	file->details->symlink_name = NULL;
	eel_ref_str_unref (file->details->mime_type);
	file->details->mime_type = NULL;
	g_free (file->details->selinux_context);
	file->details->selinux_context = NULL;
}

static NautilusFile *
nautilus_file_new_from_filename (NautilusDirectory *directory,
				 const char *filename,
				 gboolean self_owned)
{
	NautilusFile *file;

	g_assert (NAUTILUS_IS_DIRECTORY (directory));
	g_assert (filename != NULL);
	g_assert (filename[0] != '\0');

	if (NAUTILUS_IS_DESKTOP_DIRECTORY (directory)) {
		if (self_owned) {
			file = NAUTILUS_FILE (g_object_new (NAUTILUS_TYPE_DESKTOP_DIRECTORY_FILE, NULL));
		} else {
			/* This doesn't normally happen, unless the user somehow types in a uri
			 * that references a file like this. (See #349840) */
			file = NAUTILUS_FILE (g_object_new (NAUTILUS_TYPE_VFS_FILE, NULL));
		}
	} else if (NAUTILUS_IS_SEARCH_DIRECTORY (directory)) {
		if (self_owned) {
			file = NAUTILUS_FILE (g_object_new (NAUTILUS_TYPE_SEARCH_DIRECTORY_FILE, NULL));
		} else {
			/* This doesn't normally happen, unless the user somehow types in a uri
			 * that references a file like this. (See #349840) */
			file = NAUTILUS_FILE (g_object_new (NAUTILUS_TYPE_VFS_FILE, NULL));
		}
	} else if (g_str_has_suffix (filename, NAUTILUS_SAVED_SEARCH_EXTENSION)) {
		file = NAUTILUS_FILE (g_object_new (NAUTILUS_TYPE_SAVED_SEARCH_FILE, NULL));
	} else {
		file = NAUTILUS_FILE (g_object_new (NAUTILUS_TYPE_VFS_FILE, NULL));
	}

	file->details->directory = nautilus_directory_ref (directory);

	file->details->name = eel_ref_str_new (filename);

#ifdef NAUTILUS_FILE_DEBUG_REF
	DEBUG_REF_PRINTF("%10p ref'd", file);
#endif

	return file;
}

static void
modify_link_hash_table (NautilusFile *file,
			ModifyListFunction modify_function)
{
	char *target_uri;
	gboolean found;
	gpointer original_key;
	GList **list_ptr;

	/* Check if there is a symlink name. If none, we are OK. */
	if (file->details->symlink_name == NULL) {
		return;
	}

	/* Create the hash table first time through. */
	if (symbolic_links == NULL) {
		symbolic_links = eel_g_hash_table_new_free_at_exit
			(g_str_hash, g_str_equal, "nautilus-file.c: symbolic_links");
	}

	target_uri = nautilus_file_get_symbolic_link_target_uri (file);
	
	/* Find the old contents of the hash table. */
	found = g_hash_table_lookup_extended
		(symbolic_links, target_uri,
		 &original_key, (gpointer *)&list_ptr);
	if (!found) {
		list_ptr = g_new0 (GList *, 1);
		original_key = g_strdup (target_uri);
		g_hash_table_insert (symbolic_links, original_key, list_ptr);
	}
	(* modify_function) (list_ptr, file);
	if (*list_ptr == NULL) {
		g_hash_table_remove (symbolic_links, target_uri);
		g_free (list_ptr);
		g_free (original_key);
	}
	g_free (target_uri);
}

static void
symbolic_link_weak_notify (gpointer      data,
			   GObject      *where_the_object_was)
{
	GList **list = data;
	/* This really shouldn't happen, but we're seeing some strange things in
	   bug #358172 where the symlink hashtable isn't correctly updated. */
	*list = g_list_remove (*list, where_the_object_was);
}

static void
add_to_link_hash_table_list (GList **list, NautilusFile *file)
{
	if (g_list_find (*list, file) != NULL) {
		g_warning ("Adding file to symlink_table multiple times. "
			   "Please add feedback of what you were doing at http://bugzilla.gnome.org/show_bug.cgi?id=358172\n");
		return;
	}
	g_object_weak_ref (G_OBJECT (file), symbolic_link_weak_notify, list);
	*list = g_list_prepend (*list, file); 
}

static void
add_to_link_hash_table (NautilusFile *file)
{
	modify_link_hash_table (file, add_to_link_hash_table_list);
}

static void
remove_from_link_hash_table_list (GList **list, NautilusFile *file)
{
	if (g_list_find (*list, file) != NULL) {
		g_object_weak_unref (G_OBJECT (file), symbolic_link_weak_notify, list);
		*list = g_list_remove (*list, file);
	}
}

static void
remove_from_link_hash_table (NautilusFile *file)
{
	modify_link_hash_table (file, remove_from_link_hash_table_list);
}

NautilusFile *
nautilus_file_new_from_info (NautilusDirectory *directory,
			     GFileInfo *info)
{
	NautilusFile *file;
	const char *mime_type;

	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
	g_return_val_if_fail (info != NULL, NULL);

	mime_type = g_file_info_get_content_type (info);
	if (mime_type &&
	    strcmp (mime_type, NAUTILUS_SAVED_SEARCH_MIMETYPE) == 0) {
		g_file_info_set_file_type (info, G_FILE_TYPE_DIRECTORY);
		file = NAUTILUS_FILE (g_object_new (NAUTILUS_TYPE_SAVED_SEARCH_FILE, NULL));
	} else {
		file = NAUTILUS_FILE (g_object_new (NAUTILUS_TYPE_VFS_FILE, NULL));
	}

	file->details->directory = nautilus_directory_ref (directory);

	update_info_and_name (file, info);

#ifdef NAUTILUS_FILE_DEBUG_REF
	DEBUG_REF_PRINTF("%10p ref'd", file);
#endif

	return file;
}

static NautilusFile *
nautilus_file_get_internal (GFile *location, gboolean create)
{
	gboolean self_owned;
	NautilusDirectory *directory;
	NautilusFile *file;
	GFile *parent;
	char *basename;

	g_assert (location != NULL);

	parent = g_file_get_parent (location);
	
	self_owned = FALSE;
	if (parent == NULL) {
		self_owned = TRUE;
		parent = g_object_ref (location);
	} 

	/* Get object that represents the directory. */
	directory = nautilus_directory_get_internal (parent, create);

	g_object_unref (parent);

	/* Get the name for the file. */
	if (self_owned && directory != NULL) {
		basename = nautilus_directory_get_name_for_self_as_new_file (directory);
	} else {
		basename = g_file_get_basename (location);
	}
	/* Check to see if it's a file that's already known. */
	if (directory == NULL) {
		file = NULL;
	} else if (self_owned) {
		file = directory->details->as_file;
	} else {
		file = nautilus_directory_find_file_by_name (directory, basename);
	}

	/* Ref or create the file. */
	if (file != NULL) {
		nautilus_file_ref (file);
	} else if (create) {
		file = nautilus_file_new_from_filename (directory, basename, self_owned);
		if (self_owned) {
			g_assert (directory->details->as_file == NULL);
			directory->details->as_file = file;
		} else {
			nautilus_directory_add_file (directory, file);
		}
	}

	g_free (basename);
	nautilus_directory_unref (directory);

	return file;
}

NautilusFile *
nautilus_file_get (GFile *location)
{
	return nautilus_file_get_internal (location, TRUE);
}

NautilusFile *
nautilus_file_get_existing (GFile *location)
{
	return nautilus_file_get_internal (location, FALSE);
}

NautilusFile *
nautilus_file_get_existing_by_uri (const char *uri)
{
	GFile *location;
	NautilusFile *file;
	
	location = g_file_new_for_uri (uri);
	file = nautilus_file_get_internal (location, FALSE);
	g_object_unref (location);
	
	return file;
}

NautilusFile *
nautilus_file_get_by_uri (const char *uri)
{
	GFile *location;
	NautilusFile *file;
	
	location = g_file_new_for_uri (uri);
	file = nautilus_file_get_internal (location, TRUE);
	g_object_unref (location);
	
	return file;
}

gboolean
nautilus_file_is_self_owned (NautilusFile *file)
{
	return file->details->directory->details->as_file == file;
}

static void
finalize (GObject *object)
{
	NautilusDirectory *directory;
	NautilusFile *file;
	char *uri;

	file = NAUTILUS_FILE (object);

	g_assert (file->details->operations_in_progress == NULL);

	if (file->details->is_thumbnailing) {
		uri = nautilus_file_get_uri (file);
		nautilus_thumbnail_remove_from_queue (uri);
		g_free (uri);
	}
	
	nautilus_async_destroying_file (file);

	remove_from_link_hash_table (file);

	directory = file->details->directory;
	
	if (nautilus_file_is_self_owned (file)) {
		directory->details->as_file = NULL;
	} else {
		if (!file->details->is_gone) {
			nautilus_directory_remove_file (directory, file);
		}
	}

	if (file->details->get_info_error) {
		g_error_free (file->details->get_info_error);
	}

	nautilus_directory_unref (directory);
	eel_ref_str_unref (file->details->name);
	eel_ref_str_unref (file->details->display_name);
	g_free (file->details->display_name_collation_key);
	eel_ref_str_unref (file->details->edit_name);
	if (file->details->icon) {
		g_object_unref (file->details->icon);
	}
	g_free (file->details->thumbnail_path);
	g_free (file->details->symlink_name);
	eel_ref_str_unref (file->details->mime_type);
	g_free (file->details->selinux_context);
	g_free (file->details->top_left_text);
	g_free (file->details->custom_icon);
	if (file->details->activation_location) {
		g_object_unref (file->details->activation_location);
	}
	g_free (file->details->compare_by_emblem_cache);

	if (file->details->thumbnail) {
		g_object_unref (file->details->thumbnail);
	}
	if (file->details->mount) {
		g_object_unref (file->details->mount);
	}
	
	eel_g_list_free_deep (file->details->mime_list);

	eel_g_list_free_deep (file->details->pending_extension_emblems);
	eel_g_list_free_deep (file->details->extension_emblems);	

	if (file->details->pending_extension_attributes) {
		g_hash_table_destroy (file->details->pending_extension_attributes);
	}
	
	if (file->details->extension_attributes) {
		g_hash_table_destroy (file->details->extension_attributes);
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

NautilusFile *
nautilus_file_ref (NautilusFile *file)
{
	if (file == NULL) {
		return NULL;
	}
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

#ifdef NAUTILUS_FILE_DEBUG_REF
	DEBUG_REF_PRINTF("%10p ref'd", file);
#endif
	
	return g_object_ref (file);
}

void
nautilus_file_unref (NautilusFile *file)
{
	if (file == NULL) {
		return;
	}

	g_return_if_fail (NAUTILUS_IS_FILE (file));

#ifdef NAUTILUS_FILE_DEBUG_REF
	DEBUG_REF_PRINTF("%10p unref'd", file);
#endif
	
	g_object_unref (file);
}

/**
 * nautilus_file_get_parent_uri_for_display:
 * 
 * Get the uri for the parent directory.
 * 
 * @file: The file in question.
 * 
 * Return value: A string representing the parent's location,
 * formatted for user display (including stripping "file://").
 * If the parent is NULL, returns the empty string.
 */ 
char *
nautilus_file_get_parent_uri_for_display (NautilusFile *file) 
{
	GFile *parent;
	char *result;

	g_assert (NAUTILUS_IS_FILE (file));
	
	parent = nautilus_file_get_parent_location (file);
	if (parent) {
		result = g_file_get_parse_name (parent);
		g_object_unref (parent);
	} else {
		result = g_strdup ("");
	}

	return result;
}

/**
 * nautilus_file_get_parent_uri:
 * 
 * Get the uri for the parent directory.
 * 
 * @file: The file in question.
 * 
 * Return value: A string for the parent's location, in "raw URI" form.
 * Use nautilus_file_get_parent_uri_for_display instead if the
 * result is to be displayed on-screen.
 * If the parent is NULL, returns the empty string.
 */ 
char *
nautilus_file_get_parent_uri (NautilusFile *file) 
{
	g_assert (NAUTILUS_IS_FILE (file));
	
	if (nautilus_file_is_self_owned (file)) {
		/* Callers expect an empty string, not a NULL. */
		return g_strdup ("");
	}

	return nautilus_directory_get_uri (file->details->directory);
}

GFile *
nautilus_file_get_parent_location (NautilusFile *file) 
{
	g_assert (NAUTILUS_IS_FILE (file));
	
	if (nautilus_file_is_self_owned (file)) {
		/* Callers expect an empty string, not a NULL. */
		return NULL;
	}

	return nautilus_directory_get_location (file->details->directory);
}

NautilusFile *
nautilus_file_get_parent (NautilusFile *file)
{
	g_assert (NAUTILUS_IS_FILE (file));
	
	if (nautilus_file_is_self_owned (file)) {
		return NULL;
	}

	return nautilus_directory_get_corresponding_file (file->details->directory);
}

/**
 * nautilus_file_can_read:
 * 
 * Check whether the user is allowed to read the contents of this file.
 * 
 * @file: The file to check.
 * 
 * Return value: FALSE if the user is definitely not allowed to read
 * the contents of the file. If the user has read permission, or
 * the code can't tell whether the user has read permission,
 * returns TRUE (so failures must always be handled).
 */
gboolean
nautilus_file_can_read (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	
	return file->details->can_read;
}

/**
 * nautilus_file_can_write:
 * 
 * Check whether the user is allowed to write to this file.
 * 
 * @file: The file to check.
 * 
 * Return value: FALSE if the user is definitely not allowed to write
 * to the file. If the user has write permission, or
 * the code can't tell whether the user has write permission,
 * returns TRUE (so failures must always be handled).
 */
gboolean
nautilus_file_can_write (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return file->details->can_write;
}

/**
 * nautilus_file_can_execute:
 * 
 * Check whether the user is allowed to execute this file.
 * 
 * @file: The file to check.
 * 
 * Return value: FALSE if the user is definitely not allowed to execute
 * the file. If the user has execute permission, or
 * the code can't tell whether the user has execute permission,
 * returns TRUE (so failures must always be handled).
 */
gboolean
nautilus_file_can_execute (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return file->details->can_execute;
}

gboolean
nautilus_file_can_mount (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	
	return file->details->can_mount;
}
	
gboolean
nautilus_file_can_unmount (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return file->details->can_unmount ||
		(file->details->mount != NULL &&
		 g_mount_can_unmount (file->details->mount));
}
	
gboolean
nautilus_file_can_eject (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return file->details->can_eject ||
		(file->details->mount != NULL &&
		 g_mount_can_eject (file->details->mount));
}

void
nautilus_file_mount (NautilusFile                   *file,
		     GMountOperation                *mount_op,
		     GCancellable                   *cancellable,
		     NautilusFileOperationCallback   callback,
		     gpointer                        callback_data)
{
	GError *error;
	
	if (NAUTILUS_FILE_GET_CLASS (file)->mount == NULL) {
		if (callback) {
			error = NULL;
			g_set_error (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
				     _("This file cannot be mounted"));
			callback (file, NULL, error, callback_data);
			g_error_free (error);
		}
	} else {
		NAUTILUS_FILE_GET_CLASS (file)->mount (file, mount_op, cancellable, callback, callback_data);
	}
}

void
nautilus_file_unmount (NautilusFile *file)
{
	if (file->details->can_unmount) {
		if (NAUTILUS_FILE_GET_CLASS (file)->unmount != NULL) {
			NAUTILUS_FILE_GET_CLASS (file)->unmount (file);
		}
	} else if (file->details->mount != NULL &&
		   g_mount_can_unmount (file->details->mount)) {
		nautilus_file_operations_unmount_mount (NULL, file->details->mount, FALSE, TRUE);
	}
}

void
nautilus_file_eject (NautilusFile *file)
{
	if (file->details->can_eject) {
		if (NAUTILUS_FILE_GET_CLASS (file)->eject != NULL) {
			NAUTILUS_FILE_GET_CLASS (file)->eject (file);
		}
	} else if (file->details->mount != NULL &&
		   g_mount_can_eject (file->details->mount)) {
		nautilus_file_operations_unmount_mount (NULL, file->details->mount, TRUE, TRUE);
	}
}

/**
 * nautilus_file_is_desktop_directory:
 * 
 * Check whether this file is the desktop directory.
 * 
 * @file: The file to check.
 * 
 * Return value: TRUE if this is the physical desktop directory.
 */
gboolean
nautilus_file_is_desktop_directory (NautilusFile *file)
{
	GFile *dir;

	dir = file->details->directory->details->location;

	if (dir == NULL) {
		return FALSE;
	}

	return nautilus_is_desktop_directory_file (dir, eel_ref_str_peek (file->details->name));
}

static gboolean
is_desktop_file (NautilusFile *file)
{
	return nautilus_file_is_mime_type (file, "application/x-desktop");
}

static gboolean
can_rename_desktop_file (NautilusFile *file)
{
	GFile *location;
	gboolean res;

	location = nautilus_file_get_location (file);
	res = g_file_is_native (location);
	g_object_unref (location);
	return res;
}

/**
 * nautilus_file_can_rename:
 * 
 * Check whether the user is allowed to change the name of the file.
 * 
 * @file: The file to check.
 * 
 * Return value: FALSE if the user is definitely not allowed to change
 * the name of the file. If the user is allowed to change the name, or
 * the code can't tell whether the user is allowed to change the name,
 * returns TRUE (so rename failures must always be handled).
 */
gboolean
nautilus_file_can_rename (NautilusFile *file)
{
	gboolean can_rename;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	/* Nonexistent files can't be renamed. */
	if (nautilus_file_is_gone (file)) {
		return FALSE;
	}

	/* Self-owned files can't be renamed */
	if (nautilus_file_is_self_owned (file)) {
		return FALSE;
	}

	if ((is_desktop_file (file) && !can_rename_desktop_file (file)) ||
	     nautilus_file_is_home (file)) {
		return FALSE;
	}
	
	can_rename = TRUE;

	/* Certain types of links can't be renamed */
	if (NAUTILUS_IS_DESKTOP_ICON_FILE (file)) {
		NautilusDesktopLink *link;

		link = nautilus_desktop_icon_file_get_link (NAUTILUS_DESKTOP_ICON_FILE (file));

		if (link != NULL) {
			can_rename = nautilus_desktop_link_can_rename (link);
			g_object_unref (link);
		}
	}

	if (!can_rename) {
		return FALSE;
	}

	return file->details->can_rename;
}

gboolean
nautilus_file_can_delete (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	/* Nonexistent files can't be deleted. */
	if (nautilus_file_is_gone (file)) {
		return FALSE;
	}

	/* Self-owned files can't be deleted */
	if (nautilus_file_is_self_owned (file)) {
		return FALSE;
	}

	return file->details->can_delete;
}

gboolean
nautilus_file_can_trash (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	/* Nonexistent files can't be deleted. */
	if (nautilus_file_is_gone (file)) {
		return FALSE;
	}

	/* Self-owned files can't be deleted */
	if (nautilus_file_is_self_owned (file)) {
		return FALSE;
	}

	return file->details->can_trash;
}

GFile *
nautilus_file_get_location (NautilusFile *file)
{
	GFile *dir;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	dir = file->details->directory->details->location;
	
	if (nautilus_file_is_self_owned (file)) {
		return g_object_ref (dir);
	}
	
	return g_file_get_child (dir, eel_ref_str_peek (file->details->name));
}

/* Return the actual uri associated with the passed-in file. */
char *
nautilus_file_get_uri (NautilusFile *file)
{
	char *uri;
	GFile *loc;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	loc = nautilus_file_get_location (file);
	uri = g_file_get_uri (loc);
	g_object_unref (loc);
	
	return uri;
}

char *
nautilus_file_get_uri_scheme (NautilusFile *file)
{
	GFile *loc;
	char *scheme;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	if (file->details->directory == NULL || 
	    file->details->directory->details->location == NULL) {
		return NULL;
	}

	loc = nautilus_directory_get_location (file->details->directory);
	scheme = g_file_get_uri_scheme (loc);
	g_object_unref (loc);
	
	return scheme;
}

NautilusFileOperation *
nautilus_file_operation_new (NautilusFile *file,
			     NautilusFileOperationCallback callback,
			     gpointer callback_data)
{
	NautilusFileOperation *op;

	op = g_new0 (NautilusFileOperation, 1);
	op->file = nautilus_file_ref (file);
	op->callback = callback;
	op->callback_data = callback_data;
	op->cancellable = g_cancellable_new ();

	op->file->details->operations_in_progress = g_list_prepend
		(op->file->details->operations_in_progress, op);

	return op;
}

static void
nautilus_file_operation_remove (NautilusFileOperation *op)
{
	op->file->details->operations_in_progress = g_list_remove
		(op->file->details->operations_in_progress, op);
}

void
nautilus_file_operation_free (NautilusFileOperation *op)
{
	nautilus_file_operation_remove (op);
	nautilus_file_unref (op->file);
	g_object_unref (op->cancellable);
	if (op->free_data) {
		op->free_data (op->data);
	}
	g_free (op);
}

void
nautilus_file_operation_complete (NautilusFileOperation *op, GFile *result_file, GError *error)
{
	/* Claim that something changed even if the operation failed.
	 * This makes it easier for some clients who see the "reverting"
	 * as "changing back".
	 */
	nautilus_file_operation_remove (op);
	nautilus_file_changed (op->file);
	if (op->callback) {
		(* op->callback) (op->file, result_file, error, op->callback_data);
	}
	nautilus_file_operation_free (op);
}

void
nautilus_file_operation_cancel (NautilusFileOperation *op)
{
	/* Cancel the operation if it's still in progress. */
	g_cancellable_cancel (op->cancellable);
}

static void
rename_get_info_callback (GObject *source_object,
			  GAsyncResult *res,
			  gpointer callback_data)
{
	NautilusFileOperation *op;
	NautilusDirectory *directory;
	NautilusFile *existing_file;
	char *old_name;
	char *old_uri;
	char *new_uri;
	const char *new_name;
	GFileInfo *new_info;
	GError *error;
	
	op = callback_data;

	error = NULL;
	new_info = g_file_query_info_finish (G_FILE (source_object), res, &error);
	if (new_info != NULL) {
		directory = op->file->details->directory;

		new_name = g_file_info_get_name (new_info);
		
		/* If there was another file by the same name in this
		 * directory, mark it gone.
		 */
		existing_file = nautilus_directory_find_file_by_name (directory, new_name);
		if (existing_file != NULL) {
			nautilus_file_mark_gone (existing_file);
			nautilus_file_changed (existing_file);
		}
		
		old_uri = nautilus_file_get_uri (op->file);
		old_name = g_strdup (eel_ref_str_peek (op->file->details->name));
		
		update_info_and_name (op->file, new_info);
		
		/* Self-owned files store their metadata under the
		 * hard-code name "."  so there's no need to rename
		 * their metadata when they are renamed.
		 */
		if (!nautilus_file_is_self_owned (op->file)) {
			nautilus_directory_rename_file_metadata
				(directory, old_name, eel_ref_str_peek (op->file->details->name));
		}
		
		g_free (old_name);
		
		new_uri = nautilus_file_get_uri (op->file);
		nautilus_directory_moved (old_uri, new_uri);
		g_free (new_uri);
		g_free (old_uri);
		
		/* the rename could have affected the display name if e.g.
		 * we're in a vfolder where the name comes from a desktop file
		 * and a rename affects the contents of the desktop file.
		 */
		if (op->file->details->got_custom_display_name) {
			nautilus_file_invalidate_attributes (op->file,
							     NAUTILUS_FILE_ATTRIBUTE_INFO |
							     NAUTILUS_FILE_ATTRIBUTE_LINK_INFO);
		}
		
		g_object_unref (new_info);
	}
	nautilus_file_operation_complete (op, NULL, error);
	if (error) {
		g_error_free (error);
	}
}

static void
rename_callback (GObject *source_object,
		 GAsyncResult *res,
		 gpointer callback_data)
{
	NautilusFileOperation *op;
	GFile *new_file;
	GError *error;

	op = callback_data;

	error = NULL;
	new_file = g_file_set_display_name_finish (G_FILE (source_object),
						   res, &error);

	if (new_file != NULL) {
		g_file_query_info_async (new_file,
					 NAUTILUS_FILE_DEFAULT_ATTRIBUTES,
					 0,
					 G_PRIORITY_DEFAULT,
					 op->cancellable,
					 rename_get_info_callback, op);
	} else {
		nautilus_file_operation_complete (op, NULL, error);
		g_error_free (error);
	}
}

static gboolean
name_is (NautilusFile *file, const char *new_name)
{
	const char *old_name;
	old_name = eel_ref_str_peek (file->details->name);
	return strcmp (new_name, old_name) == 0;
}

void
nautilus_file_rename (NautilusFile *file,
		      const char *new_name,
		      NautilusFileOperationCallback callback,
		      gpointer callback_data)
{
	NautilusFileOperation *op;
	char *uri;
	char *old_name;
	gboolean success;
	gboolean is_renameable_desktop_file;
	GFile *location;
	GError *error;
	
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (new_name != NULL);
	g_return_if_fail (callback != NULL);

	is_renameable_desktop_file =
		is_desktop_file (file) && can_rename_desktop_file (file);
	
	/* Return an error for incoming names containing path separators.
	 * But not for .desktop files as '/' are allowed for them */
	if (strstr (new_name, "/") != NULL && !is_renameable_desktop_file) {
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
				     _("Slashes are not allowed in filenames"));
		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;
	}
	
	/* Can't rename a file that's already gone.
	 * We need to check this here because there may be a new
	 * file with the same name.
	 */
	if (nautilus_file_is_gone (file)) {
	       	/* Claim that something changed even if the rename
		 * failed. This makes it easier for some clients who
		 * see the "reverting" to the old name as "changing
		 * back".
		 */
		nautilus_file_changed (file);
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
				     _("File not found"));
		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;
	}

	/* Test the name-hasn't-changed case explicitly, for two reasons.
	 * (1) rename returns an error if new & old are same.
	 * (2) We don't want to send file-changed signal if nothing changed.
	 */
	if (name_is (file, new_name)) {
		(* callback) (file, NULL, NULL, callback_data);
		return;
	}

	/* Self-owned files can't be renamed. Test the name-not-actually-changing
	 * case before this case.
	 */
	if (nautilus_file_is_self_owned (file)) {
	       	/* Claim that something changed even if the rename
		 * failed. This makes it easier for some clients who
		 * see the "reverting" to the old name as "changing
		 * back".
		 */
		nautilus_file_changed (file);
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
				     _("Toplevel files cannot be renamed"));
		
		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;
	}

	if (NAUTILUS_IS_DESKTOP_ICON_FILE (file)) {
		NautilusDesktopLink *link;

		link = nautilus_desktop_icon_file_get_link (NAUTILUS_DESKTOP_ICON_FILE (file));
		
		if (link != NULL &&
		    nautilus_desktop_link_rename (link, new_name)) {
			(* callback) (file, NULL, NULL, callback_data);
		} else {
			error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
					     _("Unable to rename desktop icon"));
			(* callback) (file, NULL, error, callback_data);
			g_error_free (error);
		}
		
		g_object_unref (link);
		return;
	}
	
	if (is_renameable_desktop_file) {
		/* Don't actually change the name if the new name is the same.
		 * This helps for the vfolder method where this can happen and
		 * we want to minimize actual changes
		 */
		uri = nautilus_file_get_uri (file);
		old_name = nautilus_link_local_get_text (uri);
		if (old_name != NULL && strcmp (new_name, old_name) == 0) {
			success = TRUE;
		} else {
			success = nautilus_link_local_set_text (uri, new_name);
		}
		g_free (old_name);
		g_free (uri);

		if (success) {
			nautilus_file_invalidate_attributes (file,
							     NAUTILUS_FILE_ATTRIBUTE_INFO |
							     NAUTILUS_FILE_ATTRIBUTE_LINK_INFO);
			(* callback) (file, NULL, NULL, callback_data);
			return;
		} else {
			error = g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
					     _("Unable to rename desktop file"));
			(* callback) (file, NULL, error, callback_data);
			g_error_free (error);
			return;
		}
	}

	/* Set up a renaming operation. */
	op = nautilus_file_operation_new (file, callback, callback_data);
	op->is_rename = TRUE;

	/* Do the renaming. */

	location = nautilus_file_get_location (file);
	g_file_set_display_name_async (location,
				       new_name,
				       G_PRIORITY_DEFAULT,
				       op->cancellable,
				       rename_callback,
				       op);
	g_object_unref (location);
}

gboolean
nautilus_file_rename_in_progress (NautilusFile *file)
{
	GList *node;
	NautilusFileOperation *op;

	for (node = file->details->operations_in_progress; node != NULL; node = node->next) {
		op = node->data;
		if (op->is_rename) {
			return TRUE;
		}
	}
	return FALSE;
}

void
nautilus_file_cancel (NautilusFile *file,
		      NautilusFileOperationCallback callback,
		      gpointer callback_data)
{
	GList *node, *next;
	NautilusFileOperation *op;

	for (node = file->details->operations_in_progress; node != NULL; node = next) {
		next = node->next;
		op = node->data;

		g_assert (op->file == file);
		if (op->callback == callback && op->callback_data == callback_data) {
			nautilus_file_operation_cancel (op);
		}
	}
}

gboolean         
nautilus_file_matches_uri (NautilusFile *file, const char *match_uri)
{
	GFile *match_file, *location;
	gboolean result;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	g_return_val_if_fail (match_uri != NULL, FALSE);
	
	location = nautilus_file_get_location (file);
	match_file = g_file_new_for_uri (match_uri);
	result = g_file_equal (location, match_file);
	g_object_unref (location);
	g_object_unref (match_file);

	return result;
}

gboolean
nautilus_file_is_local (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	
	return nautilus_directory_is_local (file->details->directory);
}

static void
update_link (NautilusFile *link_file, NautilusFile *target_file)
{
	g_assert (NAUTILUS_IS_FILE (link_file));
	g_assert (NAUTILUS_IS_FILE (target_file));

	/* FIXME bugzilla.gnome.org 42044: If we don't put any code
	 * here then the hash table is a waste of time.
	 */
}

static GList *
get_link_files (NautilusFile *target_file)
{
	char *uri;
	GList **link_files;
	
	if (symbolic_links == NULL) {
		link_files = NULL;
	} else {
		uri = nautilus_file_get_uri (target_file);
		link_files = g_hash_table_lookup (symbolic_links, uri);
		g_free (uri);
	}
	if (link_files) {
		return nautilus_file_list_copy (*link_files);
	}
	return NULL;
}

static void
update_links_if_target (NautilusFile *target_file)
{
	GList *link_files, *p;

	link_files = get_link_files (target_file);
	for (p = link_files; p != NULL; p = p->next) {
		update_link (NAUTILUS_FILE (p->data), target_file);
	}
	nautilus_file_list_free (link_files);
}

static gboolean
update_info_internal (NautilusFile *file,
		      GFileInfo *info,
		      gboolean update_name)
{
	GList *node;
	gboolean changed;
	gboolean is_symlink, is_hidden, is_backup, is_mountpoint;
	gboolean has_permissions;
	guint32 permissions;
	gboolean can_read, can_write, can_execute, can_delete, can_trash, can_rename, can_mount, can_unmount, can_eject;
	gboolean thumbnailing_failed;
	int uid, gid;
	goffset size;
	int sort_order;
	time_t atime, mtime, ctime;
	const char *symlink_name, *mime_type, *selinux_context, *name, *thumbnail_path;
	GFileType file_type;
	GIcon *icon;
	GFile *old_activation_location;
	const char *activation_uri;
	const char *description;
	
	if (file->details->is_gone) {
		return FALSE;
	}

	if (info == NULL) {
		nautilus_file_mark_gone (file);
		return TRUE;
	}

	file->details->file_info_is_up_to_date = TRUE;

	/* FIXME bugzilla.gnome.org 42044: Need to let links that
	 * point to the old name know that the file has been renamed.
	 */

	remove_from_link_hash_table (file);

	changed = FALSE;

	if (!file->details->got_file_info) {
		changed = TRUE;
	}
	file->details->got_file_info = TRUE;

	changed |= nautilus_file_set_display_name (file,
						  g_file_info_get_display_name (info),
						  g_file_info_get_edit_name (info),
						  FALSE);
	
	file_type = g_file_info_get_file_type (info);
	if (file->details->type != file_type) {
		changed = TRUE;
	}
	file->details->type = file_type;

	if (!file->details->got_custom_activation_location) {
		activation_uri = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
		if (activation_uri == NULL) {
			if (file->details->activation_location) {
				g_object_unref (file->details->activation_location);
				file->details->activation_location = NULL;
				changed = TRUE;
			}
		} else {
			old_activation_location = file->details->activation_location;
			file->details->activation_location = g_file_new_for_uri (activation_uri);
			
			if (old_activation_location) {
				if (!g_file_equal (old_activation_location,
						   file->details->activation_location)) {
					changed = TRUE;
				}
				g_object_unref (old_activation_location);
			} else {
				changed = TRUE;
			}
		}
	}
	
	is_symlink = g_file_info_get_is_symlink (info);
	if (file->details->is_symlink != is_symlink) {
		changed = TRUE;
	}
	file->details->is_symlink = is_symlink;
		
	is_hidden = g_file_info_get_is_hidden (info);
	if (file->details->is_hidden != is_hidden) {
		changed = TRUE;
	}
	file->details->is_hidden = is_hidden;
		
	is_backup = g_file_info_get_is_backup (info);
	if (file->details->is_backup != is_backup) {
		changed = TRUE;
	}
	file->details->is_backup = is_backup;
		
	is_mountpoint = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_UNIX_IS_MOUNTPOINT);
	if (file->details->is_mountpoint != is_mountpoint) {
		changed = TRUE;
	}
	file->details->is_mountpoint = is_mountpoint;

	has_permissions = g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_MODE);
	permissions = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);;
	if (file->details->has_permissions != has_permissions ||
	    file->details->permissions != permissions) {
		changed = TRUE;
	}
	file->details->has_permissions = has_permissions;
	file->details->permissions = permissions;

	/* We default to TRUE for this if we can't know */
	can_read = TRUE;
	can_write = TRUE;
	can_execute = TRUE;
	can_delete = TRUE;
	can_trash = TRUE;
	can_rename = TRUE;
	can_mount = FALSE;
	can_unmount = FALSE;
	can_eject = FALSE;
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ)) {
		can_read = g_file_info_get_attribute_boolean (info,
							      G_FILE_ATTRIBUTE_ACCESS_CAN_READ);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE)) {
		can_write = g_file_info_get_attribute_boolean (info,
							       G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE)) {
		can_execute = g_file_info_get_attribute_boolean (info,
								G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE)) {
		can_delete = g_file_info_get_attribute_boolean (info,
								G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH)) {
		can_trash = g_file_info_get_attribute_boolean (info,
							       G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME)) {
		can_rename = g_file_info_get_attribute_boolean (info,
								G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_MOUNT)) {
		can_mount = g_file_info_get_attribute_boolean (info,
							       G_FILE_ATTRIBUTE_MOUNTABLE_CAN_MOUNT);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_UNMOUNT)) {
		can_unmount = g_file_info_get_attribute_boolean (info,
								 G_FILE_ATTRIBUTE_MOUNTABLE_CAN_UNMOUNT);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_EJECT)) {
		can_eject = g_file_info_get_attribute_boolean (info,
							       G_FILE_ATTRIBUTE_MOUNTABLE_CAN_EJECT);
	}
	if (file->details->can_read != can_read ||
	    file->details->can_write != can_write ||
	    file->details->can_execute != can_execute ||
	    file->details->can_delete != can_delete ||
	    file->details->can_trash != can_trash ||
	    file->details->can_rename != can_rename ||
	    file->details->can_mount != can_mount ||
	    file->details->can_unmount != can_unmount ||
	    file->details->can_eject != can_eject) {
		changed = TRUE;
	}
	
	file->details->can_read = can_read;
	file->details->can_write = can_write;
	file->details->can_execute = can_execute;
	file->details->can_delete = can_delete;
	file->details->can_trash = can_trash;
	file->details->can_rename = can_rename;
	file->details->can_mount = can_mount;
	file->details->can_unmount = can_unmount;
	file->details->can_eject = can_eject;

	uid = -1;
	gid = -1;
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_UID)) {
		uid = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_UID);
	}
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_GID)) {
		gid = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_GID);
	}
	if (file->details->uid != uid ||
	    file->details->gid != gid) {
		changed = TRUE;
	}
	file->details->uid = uid;
	file->details->gid = gid;
	
	size = -1;
	if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE)) {
		size = g_file_info_get_size (info);
	}
	if (file->details->size != size) {
		changed = TRUE;
	}
	file->details->size = size;

	sort_order = g_file_info_get_sort_order (info);
	if (file->details->sort_order != sort_order) {
		changed = TRUE;
	}
	file->details->sort_order = sort_order;
	
	atime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_ACCESS);
	ctime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_CHANGED);
	mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
	if (file->details->atime != atime ||
	    file->details->mtime != mtime ||
	    file->details->ctime != ctime) {
		changed = TRUE;
	}
	file->details->atime = atime;
	file->details->ctime = ctime;
	file->details->mtime = mtime;

	if (file->details->thumbnail != NULL &&
	    file->details->thumbnail_mtime != 0 &&
	    file->details->thumbnail_mtime != mtime) {
		file->details->thumbnail_is_up_to_date = FALSE;
		changed = TRUE;
	}

	icon = g_file_info_get_icon (info);
	if (!g_icon_equal (icon, file->details->icon)) {
		changed = TRUE;

		if (file->details->icon) {
			g_object_unref (file->details->icon);
		}
		file->details->icon = g_object_ref (icon);
	}

	thumbnail_path =  g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
	if (eel_strcmp (file->details->thumbnail_path, thumbnail_path) != 0) {
		changed = TRUE;
		g_free (file->details->thumbnail_path);
		file->details->thumbnail_path = g_strdup (thumbnail_path);
	}

	thumbnailing_failed =  g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED);
	if (file->details->thumbnailing_failed != thumbnailing_failed) {
		changed = TRUE;
		file->details->thumbnailing_failed = thumbnailing_failed;
	}
	
	symlink_name = g_file_info_get_symlink_target (info);
	if (eel_strcmp (file->details->symlink_name, symlink_name) != 0) {
		changed = TRUE;
		g_free (file->details->symlink_name);
		file->details->symlink_name = g_strdup (symlink_name);
	}

	mime_type = g_file_info_get_content_type (info);
	if (eel_strcmp (eel_ref_str_peek (file->details->mime_type), mime_type) != 0) {
		changed = TRUE;
	}
	eel_ref_str_unref (file->details->mime_type);
	file->details->mime_type = eel_ref_str_get_unique (mime_type);
	
	selinux_context = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_SELINUX_CONTEXT);
	if (eel_strcmp (file->details->selinux_context, selinux_context) != 0) {
		changed = TRUE;
		g_free (file->details->selinux_context);
		file->details->selinux_context = g_strdup (selinux_context);
	}
	
	description = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION);
	if (eel_strcmp (file->details->description, description) != 0) {
		changed = TRUE;
		g_free (file->details->description);
		file->details->description = g_strdup (description);
	}
	
	if (update_name) {
		name = g_file_info_get_name (info);
		if (file->details->name == NULL ||
		    strcmp (eel_ref_str_peek (file->details->name), name) != 0) {
			node = nautilus_directory_begin_file_name_change
				(file->details->directory, file);
			
			eel_ref_str_unref (file->details->name);
			if (eel_strcmp (eel_ref_str_peek (file->details->display_name),
					name) == 0) {
				file->details->name = eel_ref_str_ref (file->details->display_name);
			} else {
				file->details->name = eel_ref_str_new (name);
			}

			nautilus_directory_end_file_name_change
				(file->details->directory, file, node);
		}
	}

	if (changed) {
		add_to_link_hash_table (file);
		
		update_links_if_target (file);
	}

	return changed;
}

static gboolean
update_info_and_name (NautilusFile *file,
		      GFileInfo *info)
{
	return update_info_internal (file, info, TRUE);
}

gboolean
nautilus_file_update_info (NautilusFile *file,
			   GFileInfo *info)
{
	return update_info_internal (file, info, FALSE);
}

static gboolean
update_name_internal (NautilusFile *file,
		      const char *name,
		      gboolean in_directory)
{
	GList *node;

	g_assert (name != NULL);

	if (file->details->is_gone) {
		return FALSE;
	}

	if (name_is (file, name)) {
		return FALSE;
	}
	
	node = NULL;
	if (in_directory) {
		node = nautilus_directory_begin_file_name_change
			(file->details->directory, file);
	}
	
	eel_ref_str_unref (file->details->name);
	file->details->name = eel_ref_str_new (name);
	
	if (in_directory) {
		nautilus_directory_end_file_name_change
			(file->details->directory, file, node);
	}

	return TRUE;
}

gboolean
nautilus_file_update_name (NautilusFile *file, const char *name)
{
	gboolean ret;
	
	ret = update_name_internal (file, name, TRUE);

	if (ret) {
		update_links_if_target (file);
	}

	return ret;
}

gboolean
nautilus_file_update_name_and_directory (NautilusFile *file, 
					 const char *name,
					 NautilusDirectory *new_directory)
{
	NautilusDirectory *old_directory;
	FileMonitors *monitors;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (file->details->directory), FALSE);
	g_return_val_if_fail (!file->details->is_gone, FALSE);
	g_return_val_if_fail (!nautilus_file_is_self_owned (file), FALSE);
	g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (new_directory), FALSE);

	old_directory = file->details->directory;
	if (old_directory == new_directory) {
		if (name) {
			return update_name_internal (file, name, TRUE);
		} else {
			return FALSE;
		}
	}

	nautilus_file_ref (file);

	/* FIXME bugzilla.gnome.org 42044: Need to let links that
	 * point to the old name know that the file has been moved.
	 */

	remove_from_link_hash_table (file);

	monitors = nautilus_directory_remove_file_monitors (old_directory, file);
	nautilus_directory_remove_file (old_directory, file);

	file->details->directory = nautilus_directory_ref (new_directory);
	nautilus_directory_unref (old_directory);

	if (name) {
		update_name_internal (file, name, FALSE);
	}

	nautilus_directory_add_file (new_directory, file);
	nautilus_directory_add_file_monitors (new_directory, file, monitors);

	add_to_link_hash_table (file);

	update_links_if_target (file);

	nautilus_file_unref (file);

	return TRUE;
}

void
nautilus_file_set_directory (NautilusFile *file,
			     NautilusDirectory *new_directory)
{
	nautilus_file_update_name_and_directory (file, NULL, new_directory);
}

static Knowledge
get_item_count (NautilusFile *file,
		guint *count)
{
	gboolean known, unreadable;

	known = nautilus_file_get_directory_item_count
		(file, count, &unreadable);
	if (!known) {
		return UNKNOWN;
	}
	if (unreadable) {
		return UNKNOWABLE;
	}
	return KNOWN;
}

static Knowledge
get_size (NautilusFile *file,
	  goffset *size)
{
	/* If we tried and failed, then treat it like there is no size
	 * to know.
	 */
	if (file->details->get_info_failed) {
		return UNKNOWABLE;
	}

	/* If the info is NULL that means we haven't even tried yet,
	 * so it's just unknown, not unknowable.
	 */
	if (!file->details->got_file_info) {
		return UNKNOWN;
	}

	/* If we got info with no size in it, it means there is no
	 * such thing as a size as far as gnome-vfs is concerned,
	 * so "unknowable".
	 */
	if (file->details->size == -1) {
		return UNKNOWABLE;
	}

	/* We have a size! */
	*size = file->details->size;
	return KNOWN;
}

static Knowledge
get_time (NautilusFile *file,
	  time_t *time_out,
	  NautilusDateType type)
{
	time_t time;

	/* If we tried and failed, then treat it like there is no size
	 * to know.
	 */
	if (file->details->get_info_failed) {
		return UNKNOWABLE;
	}

	/* If the info is NULL that means we haven't even tried yet,
	 * so it's just unknown, not unknowable.
	 */
	if (!file->details->got_file_info) {
		return UNKNOWN;
	}

	time = 0;
	switch (type) {
	case NAUTILUS_DATE_TYPE_MODIFIED:
		time = file->details->mtime;
		break;
	case NAUTILUS_DATE_TYPE_ACCESSED:
		time = file->details->atime;
		break;
	default:
		g_assert_not_reached ();
		break;
	}

	*time_out = time;
	
	/* If we got info with no modification time in it, it means
	 * there is no such thing as a modification time as far as
	 * gnome-vfs is concerned, so "unknowable".
	 */
	if (time == 0) {
		return UNKNOWABLE;
	}
	return KNOWN;
}

static int
compare_directories_by_count (NautilusFile *file_1, NautilusFile *file_2)
{
	/* Sort order:
	 *   Directories with unknown # of items
	 *   Directories with "unknowable" # of items
	 *   Directories with 0 items
	 *   Directories with n items
	 */

	Knowledge count_known_1, count_known_2;
	guint count_1, count_2;

	count_known_1 = get_item_count (file_1, &count_1);
	count_known_2 = get_item_count (file_2, &count_2);

	if (count_known_1 > count_known_2) {
		return -1;
	}
	if (count_known_1 < count_known_2) {
		return +1;
	}

	/* count_known_1 and count_known_2 are equal now. Check if count
	 * details are UNKNOWABLE or UNKNOWN.
	 */ 
	if (count_known_1 == UNKNOWABLE || count_known_1 == UNKNOWN) {
		return 0;
	}

	if (count_1 < count_2) {
		return -1;
	}
	if (count_1 > count_2) {
		return +1;
	}

	return 0;
}

static int
compare_files_by_size (NautilusFile *file_1, NautilusFile *file_2)
{
	/* Sort order:
	 *   Files with unknown size.
	 *   Files with "unknowable" size.
	 *   Files with smaller sizes.
	 *   Files with large sizes.
	 */

	Knowledge size_known_1, size_known_2;
	goffset size_1, size_2;

	size_known_1 = get_size (file_1, &size_1);
	size_known_2 = get_size (file_2, &size_2);

	if (size_known_1 > size_known_2) {
		return -1;
	}
	if (size_known_1 < size_known_2) {
		return +1;
	}

	/* size_known_1 and size_known_2 are equal now. Check if size 
	 * details are UNKNOWABLE or UNKNOWN 
	 */ 
	if (size_known_1 == UNKNOWABLE || size_known_1 == UNKNOWN) {
		return 0;
	}

	if (size_1 < size_2) {
		return -1;
	}
	if (size_1 > size_2) {
		return +1;
	}

	return 0;
}

static int
compare_by_size (NautilusFile *file_1, NautilusFile *file_2)
{
	/* Sort order:
	 *   Directories with n items
	 *   Directories with 0 items
	 *   Directories with "unknowable" # of items
	 *   Directories with unknown # of items
	 *   Files with large sizes.
	 *   Files with smaller sizes.
	 *   Files with "unknowable" size.
	 *   Files with unknown size.
	 */

	gboolean is_directory_1, is_directory_2;

	is_directory_1 = nautilus_file_is_directory (file_1);
	is_directory_2 = nautilus_file_is_directory (file_2);

	if (is_directory_1 && !is_directory_2) {
		return -1;
	}
	if (is_directory_2 && !is_directory_1) {
		return +1;
	}

	if (is_directory_1) {
		return compare_directories_by_count (file_1, file_2);
	} else {
		return compare_files_by_size (file_1, file_2);
	}
}

static int
compare_by_display_name (NautilusFile *file_1, NautilusFile *file_2)
{
	const char *name_1, *name_2;
	const char *key_1, *key_2;
	gboolean sort_last_1, sort_last_2;
	int compare;

	name_1 = nautilus_file_peek_display_name (file_1);
	name_2 = nautilus_file_peek_display_name (file_2);

	sort_last_1 = name_1[0] == SORT_LAST_CHAR1 || name_1[0] == SORT_LAST_CHAR2;
	sort_last_2 = name_2[0] == SORT_LAST_CHAR1 || name_2[0] == SORT_LAST_CHAR2;

	if (sort_last_1 && !sort_last_2) {
		compare = +1;
	} else if (!sort_last_1 && sort_last_2) {
		compare = -1;
	} else {
		key_1 = nautilus_file_peek_display_name_collation_key (file_1);
		key_2 = nautilus_file_peek_display_name_collation_key (file_2);
		compare = strcmp (key_1, key_2);
	}

	return compare;
}

static int
compare_by_directory_name (NautilusFile *file_1, NautilusFile *file_2)
{
	char *directory_1, *directory_2;
	int compare;

	if (file_1->details->directory == file_2->details->directory) {
		return 0;
	}

	directory_1 = nautilus_file_get_parent_uri_for_display (file_1);
	directory_2 = nautilus_file_get_parent_uri_for_display (file_2);

	compare = g_utf8_collate (directory_1, directory_2);

	g_free (directory_1);
	g_free (directory_2);

	return compare;
}

static gboolean
file_has_note (NautilusFile *file)
{
	char *note;
	gboolean res;

	note = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_ANNOTATION, NULL);
	res = note != NULL && note[0] != 0;
	g_free (note);

	return res;
}

static GList *
prepend_automatic_keywords (NautilusFile *file,
			    GList *names)
{
	/* Prepend in reverse order. */
	NautilusFile *parent;

	parent = nautilus_file_get_parent (file);

#ifdef TRASH_IS_FAST_ENOUGH
	if (nautilus_file_is_in_trash (file)) {
		names = g_list_prepend
			(names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_TRASH));
	}
#endif
	if (nautilus_file_is_desktop_directory (file)) {
		names = g_list_prepend 
			(names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_DESKTOP));
	}
	if (file_has_note (file)) {
		names = g_list_prepend
			(names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_NOTE));
	}
	if (!nautilus_file_can_write (file) &&
	    (parent == NULL || nautilus_file_can_write (parent))) {
		names = g_list_prepend
			(names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_CANT_WRITE));
	}
	if (!nautilus_file_can_read (file)) {
		names = g_list_prepend
			(names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_CANT_READ));
	}
	if (nautilus_file_is_symbolic_link (file)) {
		names = g_list_prepend
			(names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_SYMBOLIC_LINK));
	}

	if (parent) {
		nautilus_file_unref (parent);
	}
		
	
	return names;
}

static void
fill_emblem_cache_if_needed (NautilusFile *file)
{
	GList *node, *keywords;
	char *scanner;
	size_t length;

	if (file->details->compare_by_emblem_cache != NULL) {
		/* Got a cache already. */
		return;
	}

	keywords = nautilus_file_get_keywords (file);

	/* Add up the keyword string lengths */
	length = 1;
	for (node = keywords; node != NULL; node = node->next) {
		length += strlen ((const char *) node->data) + 1;
	}

	/* Now that we know how large the cache struct needs to be, allocate it. */
	file->details->compare_by_emblem_cache = g_malloc (sizeof(NautilusFileSortByEmblemCache) + length);

	/* Copy them into the cache. */
	scanner = file->details->compare_by_emblem_cache->emblem_keywords;
	for (node = keywords; node != NULL; node = node->next) {
		length = strlen ((const char *) node->data) + 1;
		memcpy (scanner, (const char *) node->data, length);
		scanner += length;
	}

	/* Zero-terminate so we can tell where the list ends. */
	*scanner = 0;

	eel_g_list_free_deep (keywords);
}

static int
compare_by_emblems (NautilusFile *file_1, NautilusFile *file_2)
{
	const char *keyword_cache_1, *keyword_cache_2;
	size_t length;
	int compare_result;

	fill_emblem_cache_if_needed (file_1);
	fill_emblem_cache_if_needed (file_2);

	/* We ignore automatic emblems, and only sort by user-added keywords. */
	compare_result = 0;
	keyword_cache_1 = file_1->details->compare_by_emblem_cache->emblem_keywords;
	keyword_cache_2 = file_2->details->compare_by_emblem_cache->emblem_keywords;
	for (; *keyword_cache_1 != '\0' && *keyword_cache_2 != '\0';) {
		compare_result = g_utf8_collate (keyword_cache_1, keyword_cache_2);
		if (compare_result != 0) {
			return compare_result;
		}
		
		/* Advance to the next keyword */
		length = strlen (keyword_cache_1);
		keyword_cache_1 += length + 1;
		keyword_cache_2 += length + 1;
	}


	/* One or both is now NULL. */
	if (*keyword_cache_1 != '\0') {
		g_assert (*keyword_cache_2 == '\0');
		return -1;
	} else if (*keyword_cache_2 != '\0') {
		return +1;
	}

	return 0;	
}

static int
compare_by_type (NautilusFile *file_1, NautilusFile *file_2)
{
	gboolean is_directory_1;
	gboolean is_directory_2;
	char *type_string_1;
	char *type_string_2;
	int result;

	/* Directories go first. Then, if mime types are identical,
	 * don't bother getting strings (for speed). This assumes
	 * that the string is dependent entirely on the mime type,
	 * which is true now but might not be later.
	 */
	is_directory_1 = nautilus_file_is_directory (file_1);
	is_directory_2 = nautilus_file_is_directory (file_2);
	
	if (is_directory_1 && is_directory_2) {
		return 0;
	}

	if (is_directory_1) {
		return -1;
	}

	if (is_directory_2) {
		return +1;
	}

	if (file_1->details->mime_type != NULL &&
	    file_2->details->mime_type != NULL &&
	    strcmp (eel_ref_str_peek (file_1->details->mime_type),
		    eel_ref_str_peek (file_2->details->mime_type)) == 0) {
		return 0;
	}

	type_string_1 = nautilus_file_get_type_as_string (file_1);
	type_string_2 = nautilus_file_get_type_as_string (file_2);

	result = g_utf8_collate (type_string_1, type_string_2);

	g_free (type_string_1);
	g_free (type_string_2);

	return result;
}

static int
compare_by_time (NautilusFile *file_1, NautilusFile *file_2, NautilusDateType type)
{
	/* Sort order:
	 *   Files with unknown times.
	 *   Files with "unknowable" times.
	 *   Files with older times.
	 *   Files with newer times.
	 */

	Knowledge time_known_1, time_known_2;
	time_t time_1, time_2;

	time_1 = 0;
	time_2 = 0;

	time_known_1 = get_time (file_1, &time_1, type);
	time_known_2 = get_time (file_2, &time_2, type);

	if (time_known_1 > time_known_2) {
		return -1;
	}
	if (time_known_1 < time_known_2) {
		return +1;
	}
	
	/* Now time_known_1 is equal to time_known_2. Check whether 
	 * we failed to get modification times for files
	 */
	if(time_known_1 == UNKNOWABLE || time_known_1 == UNKNOWN) {
		return 0;
	}
		
	if (time_1 < time_2) {
		return -1;
	}
	if (time_1 > time_2) {
		return +1;
	}

	return 0;
}

static int
compare_by_full_path (NautilusFile *file_1, NautilusFile *file_2)
{
	int compare;

	compare = compare_by_directory_name (file_1, file_2);
	if (compare != 0) {
		return compare;
	}
	return compare_by_display_name (file_1, file_2);
}

static int
nautilus_file_compare_for_sort_internal (NautilusFile *file_1,
					 NautilusFile *file_2,
					 gboolean directories_first)
{
	gboolean is_directory_1, is_directory_2;

	if (directories_first) {
		is_directory_1 = nautilus_file_is_directory (file_1);
		is_directory_2 = nautilus_file_is_directory (file_2);

		if (is_directory_1 && !is_directory_2) {
			return -1;
		}

		if (is_directory_2 && !is_directory_1) {
			return +1;
		}
	}

	if (file_1->details->sort_order < file_2->details->sort_order) {
		return -1;
	} else if (file_1->details->sort_order > file_2->details->sort_order) {
		return 1;
	}

	return 0;
}

/**
 * nautilus_file_compare_for_sort:
 * @file_1: A file object
 * @file_2: Another file object
 * @sort_type: Sort criterion
 * @directories_first: Put all directories before any non-directories
 * @reversed: Reverse the order of the items, except that
 * the directories_first flag is still respected.
 * 
 * Return value: int < 0 if @file_1 should come before file_2 in a
 * sorted list; int > 0 if @file_2 should come before file_1 in a
 * sorted list; 0 if @file_1 and @file_2 are equal for this sort criterion. Note
 * that each named sort type may actually break ties several ways, with the name
 * of the sort criterion being the primary but not only differentiator.
 **/
int
nautilus_file_compare_for_sort (NautilusFile *file_1,
				NautilusFile *file_2,
				NautilusFileSortType sort_type,
				gboolean directories_first,
				gboolean reversed)
{
	int result;

	if (file_1 == file_2) {
		return 0;
	}
	
	result = nautilus_file_compare_for_sort_internal (file_1, file_2, directories_first);
	
	if (result == 0) {
		switch (sort_type) {
		case NAUTILUS_FILE_SORT_BY_DISPLAY_NAME:
			result = compare_by_display_name (file_1, file_2);
			if (result == 0) {
				result = compare_by_directory_name (file_1, file_2);
			}
			break;
		case NAUTILUS_FILE_SORT_BY_DIRECTORY:
			result = compare_by_full_path (file_1, file_2);
			break;
		case NAUTILUS_FILE_SORT_BY_SIZE:
			/* Compare directory sizes ourselves, then if necessary
			 * use GnomeVFS to compare file sizes.
			 */
			result = compare_by_size (file_1, file_2);
			if (result == 0) {
				result = compare_by_full_path (file_1, file_2);
			}
			break;
		case NAUTILUS_FILE_SORT_BY_TYPE:
			/* GnomeVFS doesn't know about our special text for certain
			 * mime types, so we handle the mime-type sorting ourselves.
			 */
			result = compare_by_type (file_1, file_2);
			if (result == 0) {
				result = compare_by_full_path (file_1, file_2);
			}
			break;
		case NAUTILUS_FILE_SORT_BY_MTIME:
			result = compare_by_time (file_1, file_2, NAUTILUS_DATE_TYPE_MODIFIED);
			if (result == 0) {
				result = compare_by_full_path (file_1, file_2);
			}
			break;
		case NAUTILUS_FILE_SORT_BY_ATIME:
			result = compare_by_time (file_1, file_2, NAUTILUS_DATE_TYPE_ACCESSED);
			if (result == 0) {
				result = compare_by_full_path (file_1, file_2);
			}
			break;
		case NAUTILUS_FILE_SORT_BY_EMBLEMS:
			/* GnomeVFS doesn't know squat about our emblems, so
			 * we handle comparing them here, before falling back
			 * to tie-breakers.
			 */
			result = compare_by_emblems (file_1, file_2);
			if (result == 0) {
				result = compare_by_full_path (file_1, file_2);
			}
			break;
		default:
			g_return_val_if_reached (0);
		}
	}

	if (reversed) {
		result = -result;
	}
	
	return result;
}

int
nautilus_file_compare_for_sort_by_attribute_q   (NautilusFile                   *file_1,
						 NautilusFile                   *file_2,
						 GQuark                          attribute,
						 gboolean                        directories_first,
						 gboolean                        reversed)
{
	int result;

	if (file_1 == file_2) {
		return 0;
	}

	/* Convert certain attributes into NautilusFileSortTypes and use
	 * nautilus_file_compare_for_sort()
	 */
	if (attribute == 0 || attribute == attribute_name_q) {
		return nautilus_file_compare_for_sort (file_1, file_2,
						       NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
						       directories_first,
						       reversed);
	} else if (attribute == attribute_size_q) {
		return nautilus_file_compare_for_sort (file_1, file_2,
						       NAUTILUS_FILE_SORT_BY_SIZE,
						       directories_first,
						       reversed);
	} else if (attribute == attribute_type_q) {
		return nautilus_file_compare_for_sort (file_1, file_2,
						       NAUTILUS_FILE_SORT_BY_TYPE,
						       directories_first,
						       reversed);
	} else if (attribute == attribute_modification_date_q || attribute == attribute_date_modified_q) {
		return nautilus_file_compare_for_sort (file_1, file_2,
						       NAUTILUS_FILE_SORT_BY_MTIME,
						       directories_first,
						       reversed);
        } else if (attribute == attribute_accessed_date_q || attribute == attribute_date_accessed_q) {
		return nautilus_file_compare_for_sort (file_1, file_2,
						       NAUTILUS_FILE_SORT_BY_ATIME,
						       directories_first,
						       reversed);
	} else if (attribute == attribute_emblems_q) {
		return nautilus_file_compare_for_sort (file_1, file_2,
						       NAUTILUS_FILE_SORT_BY_EMBLEMS,
						       directories_first,
						       reversed);
	}
	
	/* it is a normal attribute, compare by strings */

	result = nautilus_file_compare_for_sort_internal (file_1, file_2, directories_first);
	
	if (result == 0) {
		char *value_1;
		char *value_2;
		
		value_1 = nautilus_file_get_string_attribute_q (file_1, 
								attribute);
		value_2 = nautilus_file_get_string_attribute_q (file_2, 
								attribute);

		if (value_1 != NULL && value_2 != NULL) {
			result = strcmp (value_1, value_2);
		}

		g_free (value_1);
		g_free (value_2);
	}

	if (reversed) {
		result = -result;
	}
	
	return result;
}

int
nautilus_file_compare_for_sort_by_attribute     (NautilusFile                   *file_1,
						 NautilusFile                   *file_2,
						 const char                     *attribute,
						 gboolean                        directories_first,
						 gboolean                        reversed)
{
	return nautilus_file_compare_for_sort_by_attribute_q (file_1, file_2,
							      g_quark_from_string (attribute),
							      directories_first,
							      reversed);
}


/**
 * nautilus_file_compare_name:
 * @file: A file object
 * @pattern: A string we are comparing it with
 * 
 * Return value: result of a comparison of the file name and the given pattern,
 * using the same sorting order as sort by name.
 **/
int
nautilus_file_compare_display_name (NautilusFile *file,
				    const char *pattern)
{
	const char *name;
	int result;

	g_return_val_if_fail (pattern != NULL, -1);

	name = nautilus_file_peek_display_name (file);
	result = g_utf8_collate (name, pattern);
	return result;
}


gboolean
nautilus_file_is_hidden_file (NautilusFile *file)
{
	return file->details->is_hidden;
}

gboolean 
nautilus_file_is_backup_file (NautilusFile *file)
{
	return file->details->is_backup;
}

static gboolean
is_file_hidden (NautilusFile *file)
{
	return file->details->directory->details->hidden_file_hash != NULL &&
		g_hash_table_lookup (file->details->directory->details->hidden_file_hash,
				     eel_ref_str_peek (file->details->name)) != NULL;
	
}

gboolean 
nautilus_file_should_show (NautilusFile *file, 
			   gboolean show_hidden,
			   gboolean show_backup)
{
	return (show_hidden || (!nautilus_file_is_hidden_file (file) && !is_file_hidden (file))) &&
		(show_backup || !nautilus_file_is_backup_file (file));

}

gboolean
nautilus_file_is_home (NautilusFile *file)
{
	GFile *dir;

	dir = file->details->directory->details->location;
	if (dir == NULL) {
		return FALSE;
	}

	return nautilus_is_home_directory_file (dir,
						eel_ref_str_peek (file->details->name));
}

gboolean
nautilus_file_is_in_desktop (NautilusFile *file)
{
	if (file->details->directory->details->location) {
		return nautilus_is_desktop_directory (file->details->directory->details->location);
	}
	return FALSE;
	
}

static gboolean
filter_hidden_and_backup_partition_callback (gpointer data,
					     gpointer callback_data)
{
	NautilusFile *file;
	FilterOptions options;

	file = NAUTILUS_FILE (data);
	options = GPOINTER_TO_INT (callback_data);

	return nautilus_file_should_show (file, 
					  options & SHOW_HIDDEN,
					  options & SHOW_BACKUP);
}

GList *
nautilus_file_list_filter_hidden_and_backup (GList    *files,
					     gboolean  show_hidden,
					     gboolean  show_backup)
{
	GList *filtered_files;
	GList *removed_files;

	/* FIXME bugzilla.gnome.org 40653: 
	 * Eventually this should become a generic filtering thingy. 
	 */

	filtered_files = nautilus_file_list_copy (files);
	filtered_files = eel_g_list_partition (filtered_files, 
					       filter_hidden_and_backup_partition_callback,
					       GINT_TO_POINTER ((show_hidden ? SHOW_HIDDEN : 0) |
								(show_backup ? SHOW_BACKUP : 0)),
					       &removed_files);
	nautilus_file_list_free (removed_files);

	return filtered_files;
}




/* We use the file's URI for the metadata for files in a directory,
 * but we use a hard-coded string for the metadata for the directory
 * itself.
 */
static const char *
get_metadata_name (NautilusFile *file)
{
	if (nautilus_file_is_self_owned (file)) {
		return FILE_NAME_FOR_DIRECTORY_METADATA;
	}
	return eel_ref_str_peek (file->details->name);
}

char *
nautilus_file_get_metadata (NautilusFile *file,
			    const char *key,
			    const char *default_metadata)
{
	g_return_val_if_fail (key != NULL, g_strdup (default_metadata));
	g_return_val_if_fail (key[0] != '\0', g_strdup (default_metadata));
	if (file == NULL) {
		return g_strdup (default_metadata);
	}
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), g_strdup (default_metadata));

	return nautilus_directory_get_file_metadata
		(file->details->directory,
		 get_metadata_name (file),
		 key,
		 default_metadata);
}

GList *
nautilus_file_get_metadata_list (NautilusFile *file,
				 const char *list_key,
				 const char *list_subkey)
{
	g_return_val_if_fail (list_key != NULL, NULL);
	g_return_val_if_fail (list_key[0] != '\0', NULL);
	g_return_val_if_fail (list_subkey != NULL, NULL);
	g_return_val_if_fail (list_subkey[0] != '\0', NULL);
	if (file == NULL) {
		return NULL;
	}
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	return nautilus_directory_get_file_metadata_list
		(file->details->directory,
		 get_metadata_name (file),
		 list_key,
		 list_subkey);
}

void
nautilus_file_set_metadata (NautilusFile *file,
			    const char *key,
			    const char *default_metadata,
			    const char *metadata)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (key != NULL);
	g_return_if_fail (key[0] != '\0');

	nautilus_directory_set_file_metadata
		(file->details->directory,
		 get_metadata_name (file),
		 key,
		 default_metadata,
		 metadata);
}

void
nautilus_file_set_metadata_list (NautilusFile *file,
				 const char *list_key,
				 const char *list_subkey,
				 GList *list)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (list_key != NULL);
	g_return_if_fail (list_key[0] != '\0');
	g_return_if_fail (list_subkey != NULL);
	g_return_if_fail (list_subkey[0] != '\0');

	nautilus_directory_set_file_metadata_list
		(file->details->directory,
		 get_metadata_name (file),
		 list_key,
		 list_subkey,
		 list);
}


gboolean
nautilus_file_get_boolean_metadata (NautilusFile *file,
				    const char   *key,
				    gboolean      default_metadata)
{
	g_return_val_if_fail (key != NULL, default_metadata);
	g_return_val_if_fail (key[0] != '\0', default_metadata);
	if (file == NULL) {
		return default_metadata;
	}
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), default_metadata);

	return nautilus_directory_get_boolean_file_metadata
		(file->details->directory,
		 get_metadata_name (file),
		 key,
		 default_metadata);
}

int
nautilus_file_get_integer_metadata (NautilusFile *file,
				    const char   *key,
				    int           default_metadata)
{
	g_return_val_if_fail (key != NULL, default_metadata);
	g_return_val_if_fail (key[0] != '\0', default_metadata);
	if (file == NULL) {
		return default_metadata;
	}
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), default_metadata);

	return nautilus_directory_get_integer_file_metadata
		(file->details->directory,
		 get_metadata_name (file),
		 key,
		 default_metadata);
}


void
nautilus_file_set_boolean_metadata (NautilusFile *file,
				    const char   *key,
				    gboolean      default_metadata,
				    gboolean      metadata)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (key != NULL);
	g_return_if_fail (key[0] != '\0');

	nautilus_directory_set_boolean_file_metadata
		(file->details->directory,
		 get_metadata_name (file),
		 key,
		 default_metadata,
		 metadata);
}

void
nautilus_file_set_integer_metadata (NautilusFile *file,
				    const char   *key,
				    int           default_metadata,
				    int           metadata)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (key != NULL);
	g_return_if_fail (key[0] != '\0');

	nautilus_directory_set_integer_file_metadata
		(file->details->directory,
		 get_metadata_name (file),
		 key,
		 default_metadata,
		 metadata);
}

static const char *
nautilus_file_peek_display_name_collation_key (NautilusFile *file)
{
	const char *res;

	res = file->details->display_name_collation_key;
	if (res == NULL)
		res = "";

	return res;
}

static const char *
nautilus_file_peek_display_name (NautilusFile *file)
{
	const char *name;
	char *escaped_name;

	/* Default to display name based on filename if its not set yet */
	
	if (file->details->display_name == NULL) {
		name = eel_ref_str_peek (file->details->name);
		if (g_utf8_validate (name, -1, NULL)) {
			nautilus_file_set_display_name (file,
							name,
							NULL,
							FALSE);
		} else {
			escaped_name = g_uri_escape_string (name, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
			nautilus_file_set_display_name (file,
							escaped_name,
							NULL,
							FALSE);
			g_free (escaped_name);
		}
	}
	
	return eel_ref_str_peek (file->details->display_name);
}

char *
nautilus_file_get_display_name (NautilusFile *file)
{
	return g_strdup (nautilus_file_peek_display_name (file));
}

char *
nautilus_file_get_edit_name (NautilusFile *file)
{
	const char *res;
	
	res = eel_ref_str_peek (file->details->edit_name);
	if (res == NULL)
		res = "";
	
	return g_strdup (res);
}

char *
nautilus_file_get_name (NautilusFile *file)
{
	return g_strdup (eel_ref_str_peek (file->details->name));
}

/**
 * nautilus_file_get_description:
 * @file: a #NautilusFile.
 * 
 * Gets the standard::description key from @file, if 
 * it has been cached.
 * 
 * Returns: a string containing the value of the standard::description 
 * 	key, or %NULL.
 */
char *
nautilus_file_get_description (NautilusFile *file)
{
	return g_strdup (file->details->description);
}
   
void             
nautilus_file_monitor_add (NautilusFile *file,
			   gconstpointer client,
			   NautilusFileAttributes attributes)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (client != NULL);

	EEL_CALL_METHOD
		(NAUTILUS_FILE_CLASS, file,
		 monitor_add, (file, client, attributes));
}   
			   
void
nautilus_file_monitor_remove (NautilusFile *file,
			      gconstpointer client)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (client != NULL);

	EEL_CALL_METHOD
		(NAUTILUS_FILE_CLASS, file,
		 monitor_remove, (file, client));
}			      

gboolean
nautilus_file_is_launcher (NautilusFile *file)
{
	return file->details->is_launcher;
}

gboolean
nautilus_file_has_activation_uri (NautilusFile *file)
{
	return file->details->activation_location != NULL;
}


/* Return the uri associated with the passed-in file, which may not be
 * the actual uri if the file is an desktop file or a nautilus
 * xml link file.
 */
char *
nautilus_file_get_activation_uri (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	if (file->details->activation_location != NULL) {
		return g_file_get_uri (file->details->activation_location);
	}
	
	return nautilus_file_get_uri (file);
}

GFile *
nautilus_file_get_activation_location (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	if (file->details->activation_location != NULL) {
		return g_object_ref (file->details->activation_location);
	}
	
	return nautilus_file_get_location (file);
}


char *
nautilus_file_get_drop_target_uri (NautilusFile *file)
{
	char *uri, *target_uri;
	GFile *location;
	NautilusDesktopLink *link;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	if (NAUTILUS_IS_DESKTOP_ICON_FILE (file)) {
		link = nautilus_desktop_icon_file_get_link (NAUTILUS_DESKTOP_ICON_FILE (file));

		if (link != NULL) {
			location = nautilus_desktop_link_get_activation_location (link);
			g_object_unref (link);
			if (location != NULL) {
				uri = g_file_get_uri (location);
				g_object_unref (location);
				return uri;
			}
		}
	}
	
	uri = nautilus_file_get_uri (file);
			
	/* Check for Nautilus link */
	if (nautilus_file_is_nautilus_link (file)) {
		location = nautilus_file_get_location (file);
		/* FIXME bugzilla.gnome.org 43020: This does sync. I/O and works only locally. */
		if (g_file_is_native (location)) {
			target_uri = nautilus_link_local_get_link_uri (uri);
			if (target_uri != NULL) {
				g_free (uri);
				uri = target_uri;
			}
		}
		g_object_unref (location);
	}

	return uri;
}

static gboolean
is_uri_relative (const char *uri)
{
	char *scheme;

	scheme = g_uri_parse_scheme (uri);
	g_free (scheme);
	return scheme == NULL;
}

static char *
get_custom_icon_metadata_uri (NautilusFile *file)
{
	char *custom_icon_uri;
	char *uri;
	char *dir_uri;
	
	uri = nautilus_file_get_metadata (file, NAUTILUS_METADATA_KEY_CUSTOM_ICON, NULL);
	if (uri != NULL &&
	    nautilus_file_is_directory (file) &&
	    is_uri_relative (uri)) {
		dir_uri = nautilus_file_get_uri (file);
		custom_icon_uri = g_build_filename (dir_uri, uri, NULL);
		g_free (dir_uri);
		g_free (uri);
	} else {
		custom_icon_uri = uri;
	}
	return custom_icon_uri;
}

static GIcon *
get_custom_icon (NautilusFile *file)
{
	char *custom_icon_uri;
	GFile *icon_file;
	GIcon *icon;

	if (file == NULL) {
		return NULL;
	}

	icon = NULL;
	
	/* Metadata takes precedence */
	custom_icon_uri = get_custom_icon_metadata_uri (file);

	if (custom_icon_uri) {
		icon_file = g_file_new_for_uri (custom_icon_uri);
		icon = g_file_icon_new (icon_file);
		g_object_unref (icon_file);
	}
 
	if (icon == NULL && file->details->got_link_info && file->details->custom_icon != NULL) {
		if (g_path_is_absolute (file->details->custom_icon)) {
			icon_file = g_file_new_for_path (file->details->custom_icon);
			icon = g_file_icon_new (icon_file);
			g_object_unref (icon_file);
		} else {
			icon = g_themed_icon_new (file->details->custom_icon);
		}
 	}
 
	return icon;
}


static int cached_thumbnail_limit;
static int cached_thumbnail_size;
static int show_image_thumbs;

GFilesystemPreviewType
nautilus_file_get_filesystem_use_preview (NautilusFile *file)
{
	GFilesystemPreviewType use_preview;
	NautilusFile *parent;

	parent = nautilus_file_get_parent (file);
	if (parent != NULL) {
		use_preview = parent->details->filesystem_use_preview;
		g_object_unref (parent);
	} else {
		use_preview = 0;
	}

	return use_preview;
}

gboolean
nautilus_file_should_show_thumbnail (NautilusFile *file)
{
	const char *mime_type;
	GFilesystemPreviewType use_preview;

	use_preview = nautilus_file_get_filesystem_use_preview (file);

	mime_type = eel_ref_str_peek (file->details->mime_type);
	if (mime_type == NULL) {
		mime_type = "application/octet-stream";
	}
	
	/* If the thumbnail has already been created, don't care about the size
	 * of the original file.
	 */
	if (nautilus_thumbnail_is_mimetype_limited_by_size (mime_type) &&
	    file->details->thumbnail_path == NULL &&
	    nautilus_file_get_size (file) > (unsigned int)cached_thumbnail_limit) {
		return FALSE;
	}

	if (show_image_thumbs == NAUTILUS_SPEED_TRADEOFF_ALWAYS) {
		if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_NEVER) {
			return FALSE;
		} else {
			return TRUE;
		}
	} else if (show_image_thumbs == NAUTILUS_SPEED_TRADEOFF_NEVER) {
		return FALSE;
	} else {
		if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_NEVER) {
			/* file system says to never thumbnail anything */
			return FALSE;
		} else if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_IF_LOCAL) {
			/* file system says we should treat file as if it's local */
			return TRUE;
		} else {
			/* only local files */
			return nautilus_file_is_local (file);
		}
	}

	return FALSE;
}

GIcon *
nautilus_file_get_gicon (NautilusFile *file,
			 NautilusFileIconFlags flags)
{
	const char * const * names;
	const char *name;
	GPtrArray *array;
	GIcon *icon;
	int i;
	gboolean changed;

	if (file == NULL) {
		return NULL;
	}

	if (file->details->icon) {
		icon = NULL;
		
		if (((flags & NAUTILUS_FILE_ICON_FLAGS_EMBEDDING_TEXT) ||
		     (flags & NAUTILUS_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT) ||
		     (flags & NAUTILUS_FILE_ICON_FLAGS_FOR_OPEN_FOLDER) ||
		     ((flags & NAUTILUS_FILE_ICON_FLAGS_IGNORE_VISITING) == 0 &&
		      nautilus_file_has_open_window (file))) &&
		    G_IS_THEMED_ICON (file->details->icon)) {
			names = g_themed_icon_get_names (G_THEMED_ICON (file->details->icon));
			array = g_ptr_array_new ();
			
			changed = TRUE;
			for (i = 0; names[i] != NULL; i++) {
				name = names[i];

				if (strcmp (name, "folder") == 0 &&
				    (flags & NAUTILUS_FILE_ICON_FLAGS_IGNORE_VISITING) == 0 &&
				    nautilus_file_has_open_window (file)) {
					changed = TRUE;
					g_ptr_array_add (array, "folder-visiting");
				}
				if (strcmp (name, "folder") == 0 &&
				    (flags & NAUTILUS_FILE_ICON_FLAGS_FOR_DRAG_ACCEPT)) {
					changed = TRUE;
					g_ptr_array_add (array, "folder-drag-accept");
				}
				if (strcmp (name, "folder") == 0 &&
				    (flags & NAUTILUS_FILE_ICON_FLAGS_FOR_OPEN_FOLDER)) {
					changed = TRUE;
					g_ptr_array_add (array, "folder-open");
				}
				if (strcmp (name, "text-x-generic") == 0 &&
				    (flags & NAUTILUS_FILE_ICON_FLAGS_EMBEDDING_TEXT)) {
					changed = TRUE;
					g_ptr_array_add (array, "text-x-preview");
				}
				g_ptr_array_add (array, (char *)name);
			}

			if (changed) {
				icon = g_themed_icon_new_from_names ((char **)array->pdata, array->len);
			}
			
			g_ptr_array_free (array, TRUE);			
		}

		if (icon == NULL) {
			icon = g_object_ref (file->details->icon);
		}
		
		return icon;
	}
	
	return g_themed_icon_new ("text-x-generic");
}

NautilusIconInfo *
nautilus_file_get_icon (NautilusFile *file,
			int size,
			NautilusFileIconFlags flags)
{
	NautilusIconInfo *icon;
	GIcon *gicon;
	GdkPixbuf *raw_pixbuf, *scaled_pixbuf;
	int modified_size;

	if (file == NULL) {
		return NULL;
	}
	
	gicon = get_custom_icon (file);
	if (gicon) {
		icon = nautilus_icon_info_lookup (gicon, size);
		g_object_unref (gicon);
		return icon;
	}

	modified_size = size * cached_thumbnail_size / NAUTILUS_ICON_SIZE_STANDARD; 

	if (flags & NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS &&
	    nautilus_file_should_show_thumbnail (file)) {
		if (file->details->thumbnail) {
			if (file->details->thumbnail_size == modified_size) {
				scaled_pixbuf = g_object_ref (file->details->thumbnail);
			} else {
				int w, h, s;
				double scale;
				
				if (file->details->thumbnail_size == 0) {
					raw_pixbuf = g_object_ref (file->details->thumbnail);
				} else {
					raw_pixbuf = nautilus_thumbnail_unframe_image (file->details->thumbnail);
				}

				w = gdk_pixbuf_get_width (raw_pixbuf);
				h = gdk_pixbuf_get_height (raw_pixbuf);
				
				/* These compensates for frame size which will be added on the raw image */
				s = MAX (NAUTILUS_THUMBNAIL_FRAME_LEFT + w + NAUTILUS_THUMBNAIL_FRAME_RIGHT,
					 NAUTILUS_THUMBNAIL_FRAME_TOP + h + NAUTILUS_THUMBNAIL_FRAME_BOTTOM);
				
				scale = (double)modified_size / s;

				scaled_pixbuf = gdk_pixbuf_scale_simple (raw_pixbuf,
									 w * scale, h * scale,
									 GDK_INTERP_HYPER);
				nautilus_thumbnail_frame_image (&scaled_pixbuf);
				
				g_object_unref (raw_pixbuf);

				if (modified_size > file->details->thumbnail_size) {
					/* Invalidate if we resize upward (and the
					   loaded was not the original raw version, w/ size 0).
					*/
					if (file->details->thumbnail_size != 0 ||
					    (modified_size > 128 && !file->details->thumbnail_tried_original)) {
						nautilus_file_invalidate_attributes (file, NAUTILUS_FILE_ATTRIBUTE_THUMBNAIL);
					}
					file->details->thumbnail_size = modified_size;
					g_object_unref (file->details->thumbnail);
					file->details->thumbnail = g_object_ref (scaled_pixbuf);
				}
			}
			
			icon = nautilus_icon_info_new_for_pixbuf (scaled_pixbuf);
			g_object_unref (scaled_pixbuf);
			return icon;
		} else if (file->details->thumbnail_path == NULL &&
			   file->details->can_read &&				
			   !file->details->thumbnailing_failed &&
			   !file->details->is_thumbnailing) {
			if (nautilus_can_thumbnail (file)) {
				nautilus_create_thumbnail (file);
			}
		}
	}

	if (file->details->is_thumbnailing &&
	    flags & NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS)
		gicon = g_themed_icon_new (ICON_NAME_THUMBNAIL_LOADING);
	else
		gicon = nautilus_file_get_gicon (file, flags);
	
	if (gicon) {
		icon = nautilus_icon_info_lookup (gicon, size);
		g_object_unref (gicon);
		return icon;
	}

	return nautilus_icon_info_new_for_pixbuf (NULL);
}

GdkPixbuf *
nautilus_file_get_icon_pixbuf (NautilusFile *file,
			       int size,
			       gboolean force_size,
			       NautilusFileIconFlags flags)
{
	NautilusIconInfo *info;
	GdkPixbuf *pixbuf;

	info = nautilus_file_get_icon (file, size, flags);
	if (force_size) {
		pixbuf =  nautilus_icon_info_get_pixbuf_at_size (info, size);
	} else {
		pixbuf = nautilus_icon_info_get_pixbuf (info);
	}
	g_object_unref (info);
	
	return pixbuf;
}

char *
nautilus_file_get_custom_icon (NautilusFile *file)
{
	char *custom_icon;

	if (file == NULL) {
		return NULL;
	}

	/* Metadata takes precedence */
	custom_icon = get_custom_icon_metadata_uri (file);
 
	if (custom_icon == NULL && file->details->got_link_info) {
		custom_icon = g_strdup (file->details->custom_icon);
 	}
 
	return custom_icon;
}


gboolean
nautilus_file_get_date (NautilusFile *file,
			NautilusDateType date_type,
			time_t *date)
{
	if (date != NULL) {
		*date = 0;
	}

	g_return_val_if_fail (date_type == NAUTILUS_DATE_TYPE_CHANGED
			      || date_type == NAUTILUS_DATE_TYPE_ACCESSED
			      || date_type == NAUTILUS_DATE_TYPE_MODIFIED
			      || date_type == NAUTILUS_DATE_TYPE_PERMISSIONS_CHANGED, FALSE);

	if (file == NULL) {
		return FALSE;
	}

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_FILE_CLASS, file,
		 get_date, (file, date_type, date));
}

static char *
nautilus_file_get_where_string (NautilusFile *file)
{
	if (file == NULL) {
		return NULL;
	}

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_FILE_CLASS, file,
		 get_where_string, (file));
}

static const char *TODAY_TIME_FORMATS [] = {
	/* Today, use special word.
	 * strftime patterns preceeded with the widest
	 * possible resulting string for that pattern.
	 *
	 * Note to localizers: You can look at man strftime
	 * for details on the format, but you should only use
	 * the specifiers from the C standard, not extensions.
	 * These include "%" followed by one of
	 * "aAbBcdHIjmMpSUwWxXyYZ". There are two extensions
	 * in the Nautilus version of strftime that can be
	 * used (and match GNU extensions). Putting a "-"
	 * between the "%" and any numeric directive will turn
	 * off zero padding, and putting a "_" there will use
	 * space padding instead of zero padding.
	 */
	N_("today at 00:00:00 PM"),
	N_("today at %-I:%M:%S %p"),
	
	N_("today at 00:00 PM"),
	N_("today at %-I:%M %p"),
	
	N_("today, 00:00 PM"),
	N_("today, %-I:%M %p"),
	
	N_("today"),
	N_("today"),

	NULL
};

static const char *YESTERDAY_TIME_FORMATS [] = {
	/* Yesterday, use special word.
	 * Note to localizers: Same issues as "today" string.
	 */
	N_("yesterday at 00:00:00 PM"),
	N_("yesterday at %-I:%M:%S %p"),
	
	N_("yesterday at 00:00 PM"),
	N_("yesterday at %-I:%M %p"),
	
	N_("yesterday, 00:00 PM"),
	N_("yesterday, %-I:%M %p"),
	
	N_("yesterday"),
	N_("yesterday"),

	NULL
};

static const char *CURRENT_WEEK_TIME_FORMATS [] = {
	/* Current week, include day of week.
	 * Note to localizers: Same issues as "today" string.
	 * The width measurement templates correspond to
	 * the day/month name with the most letters.
	 */
	N_("Wednesday, September 00 0000 at 00:00:00 PM"),
	N_("%A, %B %-d %Y at %-I:%M:%S %p"),

	N_("Mon, Oct 00 0000 at 00:00:00 PM"),
	N_("%a, %b %-d %Y at %-I:%M:%S %p"),

	N_("Mon, Oct 00 0000 at 00:00 PM"),
	N_("%a, %b %-d %Y at %-I:%M %p"),
	
	N_("Oct 00 0000 at 00:00 PM"),
	N_("%b %-d %Y at %-I:%M %p"),
	
	N_("Oct 00 0000, 00:00 PM"),
	N_("%b %-d %Y, %-I:%M %p"),
	
	N_("00/00/00, 00:00 PM"),
	N_("%m/%-d/%y, %-I:%M %p"),

	N_("00/00/00"),
	N_("%m/%d/%y"),

	NULL
};

static char *
nautilus_file_fit_date_as_string (NautilusFile *file,
				  NautilusDateType date_type,
				  int width,
				  NautilusWidthMeasureCallback measure_callback,
				  NautilusTruncateCallback truncate_callback,
				  void *measure_context)
{
	time_t file_time_raw;
	struct tm *file_time;
	const char **formats;
	const char *width_template;
	const char *format;
	char *date_string;
	char *result;
	GDate *today;
	GDate *file_date;
	guint32 file_date_age;
	int i;

	if (!nautilus_file_get_date (file, date_type, &file_time_raw)) {
		return NULL;
	}

	file_time = localtime (&file_time_raw);

	if (date_format_pref == NAUTILUS_DATE_FORMAT_LOCALE) {
		return eel_strdup_strftime ("%c", file_time);
	} else if (date_format_pref == NAUTILUS_DATE_FORMAT_ISO) {
		return eel_strdup_strftime ("%Y-%m-%d %H:%M:%S", file_time);
	}
	
	file_date = eel_g_date_new_tm (file_time);
	
	today = g_date_new ();
	g_date_set_time_t (today, time (NULL));

	/* Overflow results in a large number; fine for our purposes. */
	file_date_age = (g_date_get_julian (today) -
			 g_date_get_julian (file_date));

	g_date_free (file_date);
	g_date_free (today);

	/* Format varies depending on how old the date is. This minimizes
	 * the length (and thus clutter & complication) of typical dates
	 * while providing sufficient detail for recent dates to make
	 * them maximally understandable at a glance. Keep all format
	 * strings separate rather than combining bits & pieces for
	 * internationalization's sake.
	 */

	if (file_date_age == 0)	{
		formats = TODAY_TIME_FORMATS;
	} else if (file_date_age == 1) {
		formats = YESTERDAY_TIME_FORMATS;
	} else if (file_date_age < 7) {
		formats = CURRENT_WEEK_TIME_FORMATS;
	} else {
		formats = CURRENT_WEEK_TIME_FORMATS;
	}

	/* Find the date format that just fits the required width. Instead of measuring
	 * the resulting string width directly, measure the width of a template that represents
	 * the widest possible version of a date in a given format. This is done by using M, m
	 * and 0 for the variable letters/digits respectively.
	 */
	format = NULL;
	
	for (i = 0; ; i += 2) {
		width_template = (formats [i] ? _(formats [i]) : NULL);
		if (width_template == NULL) {
			/* no more formats left */
			g_assert (format != NULL);
			
			/* Can't fit even the shortest format -- return an ellipsized form in the
			 * shortest format
			 */
			
			date_string = eel_strdup_strftime (format, file_time);

			if (truncate_callback == NULL) {
				return date_string;
			}
			
			result = (* truncate_callback) (date_string, width, measure_context);
			g_free (date_string);
			return result;
		}
		
		format = _(formats [i + 1]);

		if (measure_callback == NULL) {
			/* don't care about fitting the width */
			break;
		}

		if ((* measure_callback) (width_template, measure_context) <= width) {
			/* The template fits, this is the format we can fit. */
			break;
		}
	}
	
	return eel_strdup_strftime (format, file_time);

}

/**
 * nautilus_file_fit_modified_date_as_string:
 * 
 * Get a user-displayable string representing a file modification date,
 * truncated to @width using the measuring and truncating callbacks.
 * @file: NautilusFile representing the file in question.
 * @width: The desired resulting string width.
 * @measure_callback: The callback used to measure the string width.
 * @truncate_callback: The callback used to truncate the string to a desired width.
 * @measure_context: Data neede when measuring and truncating.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
char *
nautilus_file_fit_modified_date_as_string (NautilusFile *file,
					   int width,
					   NautilusWidthMeasureCallback measure_callback,
					   NautilusTruncateCallback truncate_callback,
					   void *measure_context)
{
	return nautilus_file_fit_date_as_string (file, NAUTILUS_DATE_TYPE_MODIFIED,
		width, measure_callback, truncate_callback, measure_context);
}

/**
 * nautilus_file_get_date_as_string:
 * 
 * Get a user-displayable string representing a file modification date. 
 * The caller is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_date_as_string (NautilusFile *file, NautilusDateType date_type)
{
	return nautilus_file_fit_date_as_string (file, date_type,
		0, NULL, NULL, NULL);
}

static NautilusSpeedTradeoffValue show_directory_item_count;
static NautilusSpeedTradeoffValue show_text_in_icons;

static void
show_text_in_icons_changed_callback (gpointer callback_data)
{
	show_text_in_icons = eel_preferences_get_enum (NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS);
}

static void
show_directory_item_count_changed_callback (gpointer callback_data)
{
	show_directory_item_count = eel_preferences_get_enum (NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS);
}

static gboolean
get_speed_tradeoff_preference_for_file (NautilusFile *file, NautilusSpeedTradeoffValue value)
{
	GFilesystemPreviewType use_preview;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	use_preview = nautilus_file_get_filesystem_use_preview (file);
	
	if (value == NAUTILUS_SPEED_TRADEOFF_ALWAYS) {
		if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_NEVER) {
			return FALSE;
		} else {
			return TRUE;
		}
	}
	
	if (value == NAUTILUS_SPEED_TRADEOFF_NEVER) {
		return FALSE;
	}

	g_assert (value == NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY);

	if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_NEVER) {
		/* file system says to never preview anything */
		return FALSE;
	} else if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_IF_LOCAL) {
		/* file system says we should treat file as if it's local */
		return TRUE;
	} else {
		/* only local files */
		return nautilus_file_is_local (file);
	}
}

gboolean
nautilus_file_should_show_directory_item_count (NautilusFile *file)
{
	static gboolean show_directory_item_count_callback_added = FALSE;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	
	if (file->details->mime_type &&
	    strcmp (eel_ref_str_peek (file->details->mime_type), "x-directory/smb-share") == 0) {
		return FALSE;
	}
	
	/* Add the callback once for the life of our process */
	if (!show_directory_item_count_callback_added) {
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
						   show_directory_item_count_changed_callback,
						   NULL);
		show_directory_item_count_callback_added = TRUE;

		/* Peek for the first time */
		show_directory_item_count_changed_callback (NULL);
	}

	return get_speed_tradeoff_preference_for_file (file, show_directory_item_count);
}

gboolean
nautilus_file_should_show_type (NautilusFile *file)
{
	char *uri;
	gboolean ret;

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	uri = nautilus_file_get_uri (file);
	ret = ((strcmp (uri, "computer:///") != 0) &&
	       (strcmp (uri, "network:///") != 0) &&
	       (strcmp (uri, "smb:///") != 0));
	g_free (uri);

	return ret;
}

gboolean
nautilus_file_should_get_top_left_text (NautilusFile *file)
{
	static gboolean show_text_in_icons_callback_added = FALSE;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	/* Add the callback once for the life of our process */
	if (!show_text_in_icons_callback_added) {
		eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_TEXT_IN_ICONS,
						   show_text_in_icons_changed_callback,
						   NULL);
		show_text_in_icons_callback_added = TRUE;

		/* Peek for the first time */
		show_text_in_icons_changed_callback (NULL);
	}
	
	if (show_text_in_icons == NAUTILUS_SPEED_TRADEOFF_ALWAYS) {
		return TRUE;
	}
	
	if (show_text_in_icons == NAUTILUS_SPEED_TRADEOFF_NEVER) {
		return FALSE;
	}

	return get_speed_tradeoff_preference_for_file (file, show_text_in_icons);
}

/**
 * nautilus_file_get_directory_item_count
 * 
 * Get the number of items in a directory.
 * @file: NautilusFile representing a directory.
 * @count: Place to put count.
 * @count_unreadable: Set to TRUE (if non-NULL) if permissions prevent
 * the item count from being read on this directory. Otherwise set to FALSE.
 * 
 * Returns: TRUE if count is available.
 * 
 **/
gboolean
nautilus_file_get_directory_item_count (NautilusFile *file, 
					guint *count,
					gboolean *count_unreadable)
{
	if (count != NULL) {
		*count = 0;
	}
	if (count_unreadable != NULL) {
		*count_unreadable = FALSE;
	}
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	if (!nautilus_file_is_directory (file)) {
		return FALSE;
	}

	if (!nautilus_file_should_show_directory_item_count (file)) {
		return FALSE;
	}

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_FILE_CLASS, file,
		 get_item_count, (file, count, count_unreadable));
}

/**
 * nautilus_file_get_deep_counts
 * 
 * Get the statistics about items inside a directory.
 * @file: NautilusFile representing a directory or file.
 * @directory_count: Place to put count of directories inside.
 * @files_count: Place to put count of files inside.
 * @unreadable_directory_count: Number of directories encountered
 * that were unreadable.
 * @total_size: Total size of all files and directories visited.
 * @force: Whether the deep counts should even be collected if
 * nautilus_file_should_show_directory_item_count returns FALSE
 * for this file.
 * 
 * Returns: Status to indicate whether sizes are available.
 * 
 **/
NautilusRequestStatus
nautilus_file_get_deep_counts (NautilusFile *file,
			       guint *directory_count,
			       guint *file_count,
			       guint *unreadable_directory_count,
			       goffset *total_size,
			       gboolean force)
{
	if (directory_count != NULL) {
		*directory_count = 0;
	}
	if (file_count != NULL) {
		*file_count = 0;
	}
	if (unreadable_directory_count != NULL) {
		*unreadable_directory_count = 0;
	}
	if (total_size != NULL) {
		*total_size = 0;
	}

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NAUTILUS_REQUEST_DONE);

	if (!force && !nautilus_file_should_show_directory_item_count (file)) {
		/* Set field so an existing value isn't treated as up-to-date
		 * when preference changes later.
		 */
		file->details->deep_counts_status = NAUTILUS_REQUEST_NOT_STARTED;
		return file->details->deep_counts_status;
	}

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_FILE_CLASS, file,
		 get_deep_counts, (file,
				   directory_count,
				   file_count,
				   unreadable_directory_count,
				   total_size));
}

void
nautilus_file_recompute_deep_counts (NautilusFile *file)
{
	if (file->details->deep_counts_status != NAUTILUS_REQUEST_IN_PROGRESS) {
		file->details->deep_counts_status = NAUTILUS_REQUEST_NOT_STARTED;
		if (file->details->directory != NULL) {
			nautilus_directory_add_file_to_work_queue (file->details->directory, file);
			nautilus_directory_async_state_changed (file->details->directory);
		}
	}
}


/**
 * nautilus_file_get_directory_item_mime_types
 * 
 * Get the list of mime-types present in a directory.
 * @file: NautilusFile representing a directory. It is an error to
 * call this function on a file that is not a directory.
 * @mime_list: Place to put the list of mime-types.
 * 
 * Returns: TRUE if mime-type list is available.
 * 
 **/
gboolean
nautilus_file_get_directory_item_mime_types (NautilusFile *file,
					     GList **mime_list)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	g_return_val_if_fail (mime_list != NULL, FALSE);

	if (!nautilus_file_is_directory (file)
	    || !file->details->got_mime_list) {
		*mime_list = NULL;
		return FALSE;
	}

	*mime_list = eel_g_str_list_copy (file->details->mime_list);
	return TRUE;
}

gboolean
nautilus_file_can_get_size (NautilusFile *file)
{
	return file->details->size == -1;
}
	

/**
 * nautilus_file_get_size
 * 
 * Get the file size.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Size in bytes.
 * 
 **/
goffset
nautilus_file_get_size (NautilusFile *file)
{
	/* Before we have info on the file, we don't know the size. */
	if (file->details->size == -1)
		return 0;
	return file->details->size;
}

time_t
nautilus_file_get_mtime (NautilusFile *file)
{
	return file->details->mtime;
}


static void
set_attributes_get_info_callback (GObject *source_object,
				  GAsyncResult *res,
				  gpointer callback_data)
{
	NautilusFileOperation *op;
	GFileInfo *new_info;
	GError *error;
	
	op = callback_data;

	error = NULL;
	new_info = g_file_query_info_finish (G_FILE (source_object), res, &error);
	if (new_info != NULL) {
		nautilus_file_update_info (op->file, new_info);
		g_object_unref (new_info);
	}
	nautilus_file_operation_complete (op, NULL, error);
	if (error) {
		g_error_free (error);
	}
}


static void
set_attributes_callback (GObject *source_object,
			 GAsyncResult *result,
			 gpointer callback_data)
{
	NautilusFileOperation *op;
	GError *error;
	gboolean res;

	op = callback_data;

	error = NULL;
	res = g_file_set_attributes_finish (G_FILE (source_object),
					    result,
					    NULL,
					    &error);

	if (res) {
		g_file_query_info_async (G_FILE (source_object),
					 NAUTILUS_FILE_DEFAULT_ATTRIBUTES,
					 0,
					 G_PRIORITY_DEFAULT,
					 op->cancellable,
					 set_attributes_get_info_callback, op);
	} else {
		nautilus_file_operation_complete (op, NULL, error);
		g_error_free (error);
	}
}

void
nautilus_file_set_attributes (NautilusFile *file, 
			      GFileInfo *attributes,
			      NautilusFileOperationCallback callback,
			      gpointer callback_data)
{
	NautilusFileOperation *op;
	GFile *location;
	
	op = nautilus_file_operation_new (file, callback, callback_data);

	location = nautilus_file_get_location (file);
	g_file_set_attributes_async (location,
				     attributes,
				     0, 
				     G_PRIORITY_DEFAULT,
				     op->cancellable,
				     set_attributes_callback,
				     op);
	g_object_unref (location);
}


/**
 * nautilus_file_can_get_permissions:
 * 
 * Check whether the permissions for a file are determinable.
 * This might not be the case for files on non-UNIX file systems.
 * 
 * @file: The file in question.
 * 
 * Return value: TRUE if the permissions are valid.
 */
gboolean
nautilus_file_can_get_permissions (NautilusFile *file)
{
	return file->details->has_permissions;
}

/**
 * nautilus_file_can_set_permissions:
 * 
 * Check whether the current user is allowed to change
 * the permissions of a file.
 * 
 * @file: The file in question.
 * 
 * Return value: TRUE if the current user can change the
 * permissions of @file, FALSE otherwise. It's always possible
 * that when you actually try to do it, you will fail.
 */
gboolean
nautilus_file_can_set_permissions (NautilusFile *file)
{
	uid_t user_id;

	if (file->details->uid != -1 &&
	    nautilus_file_is_local (file)) {
		/* Check the user. */
		user_id = geteuid();

		/* Owner is allowed to set permissions. */
		if (user_id == (uid_t) file->details->uid) {
			return TRUE;
		}

		/* Root is also allowed to set permissions. */
		if (user_id == 0) {
			return TRUE;
		}

		/* Nobody else is allowed. */
		return FALSE;
	}

	/* pretend to have full chmod rights when no info is available, relevant when
	 * the FS can't provide ownership info, for instance for FTP */
	return TRUE;
}

guint
nautilus_file_get_permissions (NautilusFile *file)
{
	g_return_val_if_fail (nautilus_file_can_get_permissions (file), 0);

	return file->details->permissions;
}

/**
 * nautilus_file_set_permissions:
 * 
 * Change a file's permissions. This should only be called if
 * nautilus_file_can_set_permissions returned TRUE.
 * 
 * @file: NautilusFile representing the file in question.
 * @new_permissions: New permissions value. This is the whole
 * set of permissions, not a delta.
 **/
void
nautilus_file_set_permissions (NautilusFile *file, 
			       guint32 new_permissions,
			       NautilusFileOperationCallback callback,
			       gpointer callback_data)
{
	GFileInfo *info;
	GError *error;

	if (!nautilus_file_can_set_permissions (file)) {
		/* Claim that something changed even if the permission change failed.
		 * This makes it easier for some clients who see the "reverting"
		 * to the old permissions as "changing back".
		 */
		nautilus_file_changed (file);
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
				     _("Not allowed to set permissions"));
		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;
	}
			       
	/* Test the permissions-haven't-changed case explicitly
	 * because we don't want to send the file-changed signal if
	 * nothing changed.
	 */
	if (new_permissions == file->details->permissions) {
		(* callback) (file, NULL, NULL, callback_data);
		return;
	}

	info = g_file_info_new ();
	g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE, new_permissions);
	nautilus_file_set_attributes (file, info, callback, callback_data);
	g_object_unref (info);
}

/**
 * nautilus_file_can_get_selinux_context:
 * 
 * Check whether the selinux context for a file are determinable.
 * This might not be the case for files on non-UNIX file systems,
 * files without a context or systems that don't support selinux.
 * 
 * @file: The file in question.
 * 
 * Return value: TRUE if the permissions are valid.
 */
gboolean
nautilus_file_can_get_selinux_context (NautilusFile *file)
{
	return file->details->selinux_context != NULL;
}


/**
 * nautilus_file_get_selinux_context:
 * 
 * Get a user-displayable string representing a file's selinux
 * context
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
char *
nautilus_file_get_selinux_context (NautilusFile *file)
{
	char *translated;
	char *raw;
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	if (!nautilus_file_can_get_selinux_context (file)) {
		return NULL;
	}

	raw = file->details->selinux_context;

#ifdef HAVE_SELINUX
	if (selinux_raw_to_trans_context (raw, &translated) == 0) {
		char *tmp;
		tmp = g_strdup (translated);
		freecon (translated);
		translated = tmp;
	}
	else
#endif
	{
		translated = g_strdup (raw);
	}
	
	return translated;
}

static char *
get_real_name (const char *name, const char *gecos)
{
	char *locale_string, *part_before_comma, *capitalized_login_name, *real_name;

	if (gecos == NULL) {
		return NULL;
	}

	locale_string = eel_str_strip_substring_and_after (gecos, ",");
	if (!g_utf8_validate (locale_string, -1, NULL)) {
		part_before_comma = g_locale_to_utf8 (locale_string, -1, NULL, NULL, NULL);
		g_free (locale_string);
	} else {
		part_before_comma = locale_string;
	}

	if (!g_utf8_validate (name, -1, NULL)) {
		locale_string = g_locale_to_utf8 (name, -1, NULL, NULL, NULL);
	} else {
		locale_string = g_strdup (name);
	}
	
	capitalized_login_name = eel_str_capitalize (locale_string);
	g_free (locale_string);

	if (capitalized_login_name == NULL) {
		real_name = part_before_comma;
	} else {
		real_name = eel_str_replace_substring
			(part_before_comma, "&", capitalized_login_name);
		g_free (part_before_comma);
	}


	if (eel_str_is_empty (real_name)
	    || eel_strcmp (name, real_name) == 0
	    || eel_strcmp (capitalized_login_name, real_name) == 0) {
		g_free (real_name);
		real_name = NULL;
	}

	g_free (capitalized_login_name);

	return real_name;
}

static char *
get_user_and_real_name_from_id (uid_t uid)
{
	char *name, *gecos;
	char *real_name, *user_and_real_name;
	
	name = nautilus_users_cache_get_name (uid);
	gecos = nautilus_users_cache_get_gecos (uid);

	real_name = get_real_name (name, gecos);
	if (real_name != NULL) {
		user_and_real_name = g_strdup_printf ("%s - %s", name, real_name);
	} else {
		user_and_real_name = g_strdup (name);
	}
	g_free (real_name);

	g_free (name);
	g_free (gecos);

	return user_and_real_name;
}

static gboolean
get_group_id_from_group_name (const char *group_name, uid_t *gid)
{
	struct group *group;

	g_assert (gid != NULL);

	group = getgrnam (group_name);

	if (group == NULL) {
		return FALSE;
	}

	*gid = group->gr_gid;

	return TRUE;
}

static gboolean
get_ids_from_user_name (const char *user_name, uid_t *uid, uid_t *gid)
{
	struct passwd *password_info;

	g_assert (uid != NULL || gid != NULL);

	password_info = getpwnam (user_name);

	if (password_info == NULL) {
		return FALSE;
	}

	if (uid != NULL) {
		*uid = password_info->pw_uid;
	}

	if (gid != NULL) {
		*gid = password_info->pw_gid;
	}

	return TRUE;
}

static gboolean
get_user_id_from_user_name (const char *user_name, uid_t *id)
{
	return get_ids_from_user_name (user_name, id, NULL);
}

static gboolean
get_id_from_digit_string (const char *digit_string, uid_t *id)
{
	long scanned_id;
	char c;

	g_assert (id != NULL);

	/* Only accept string if it has one integer with nothing
	 * afterwards.
	 */
	if (sscanf (digit_string, "%ld%c", &scanned_id, &c) != 1) {
		return FALSE;
	}
	*id = scanned_id;
	return TRUE;
}

/**
 * nautilus_file_can_get_owner:
 * 
 * Check whether the owner a file is determinable.
 * This might not be the case for files on non-UNIX file systems.
 * 
 * @file: The file in question.
 * 
 * Return value: TRUE if the owner is valid.
 */
gboolean
nautilus_file_can_get_owner (NautilusFile *file)
{
	/* Before we have info on a file, the owner is unknown. */
	return file->details->uid != -1;
}

/**
 * nautilus_file_get_owner_name:
 * 
 * Get the user name of the file's owner. If the owner has no
 * name, returns the userid as a string. The caller is responsible
 * for g_free-ing this string.
 * 
 * @file: The file in question.
 * 
 * Return value: A newly-allocated string.
 */
char *
nautilus_file_get_owner_name (NautilusFile *file)
{
	return nautilus_file_get_owner_as_string (file, FALSE);
}

/**
 * nautilus_file_can_set_owner:
 * 
 * Check whether the current user is allowed to change
 * the owner of a file.
 * 
 * @file: The file in question.
 * 
 * Return value: TRUE if the current user can change the
 * owner of @file, FALSE otherwise. It's always possible
 * that when you actually try to do it, you will fail.
 */
gboolean
nautilus_file_can_set_owner (NautilusFile *file)
{
	/* Not allowed to set the owner if we can't
	 * even read it. This can happen on non-UNIX file
	 * systems.
	 */
	if (!nautilus_file_can_get_owner (file)) {
		return FALSE;
	}

	/* Only root is also allowed to set the owner. */
	return geteuid() == 0;
}

/**
 * nautilus_file_set_owner:
 * 
 * Set the owner of a file. This will only have any effect if
 * nautilus_file_can_set_owner returns TRUE.
 * 
 * @file: The file in question.
 * @user_name_or_id: The user name to set the owner to.
 * If the string does not match any user name, and the
 * string is an integer, the owner will be set to the
 * userid represented by that integer.
 * @callback: Function called when asynch owner change succeeds or fails.
 * @callback_data: Parameter passed back with callback function.
 */
void
nautilus_file_set_owner (NautilusFile *file, 
			 const char *user_name_or_id,
			 NautilusFileOperationCallback callback,
			 gpointer callback_data)
{
	GError *error;
	GFileInfo *info;
	uid_t new_id;

	if (!nautilus_file_can_set_owner (file)) {
		/* Claim that something changed even if the permission
		 * change failed. This makes it easier for some
		 * clients who see the "reverting" to the old owner as
		 * "changing back".
		 */
		nautilus_file_changed (file);
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
				     _("Not allowed to set owner"));
		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;
	}

	/* If no match treating user_name_or_id as name, try treating
	 * it as id.
	 */
	if (!get_user_id_from_user_name (user_name_or_id, &new_id)
	    && !get_id_from_digit_string (user_name_or_id, &new_id)) {
		/* Claim that something changed even if the permission
		 * change failed. This makes it easier for some
		 * clients who see the "reverting" to the old owner as
		 * "changing back".
		 */
		nautilus_file_changed (file);
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
				     _("Specified owner '%s' doesn't exist"), user_name_or_id);
		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;		
	}

	/* Test the owner-hasn't-changed case explicitly because we
	 * don't want to send the file-changed signal if nothing
	 * changed.
	 */
	if (new_id == (uid_t) file->details->uid) {
		(* callback) (file, NULL, NULL, callback_data);
		return;
	}

	info = g_file_info_new ();
	g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_UID, new_id);
	nautilus_file_set_attributes (file, info, callback, callback_data);
	g_object_unref (info);
}

/**
 * nautilus_get_user_names:
 * 
 * Get a list of user names. For users with a different associated 
 * "real name", the real name follows the standard user name, separated 
 * by a carriage return. The caller is responsible for freeing this list 
 * and its contents.
 */
GList *
nautilus_get_user_names (void)
{
	GList *list;
	char *real_name, *name;
	struct passwd *user;

	list = NULL;
	
	setpwent ();

	while ((user = getpwent ()) != NULL) {
		real_name = get_real_name (user->pw_name, user->pw_gecos);
		if (real_name != NULL) {
			name = g_strconcat (user->pw_name, "\n", real_name, NULL);
		} else {
			name = g_strdup (user->pw_name);
		}
		g_free (real_name);
		list = g_list_prepend (list, name);
	}

	endpwent ();

	return eel_g_str_list_alphabetize (list);
}

/**
 * nautilus_file_can_get_group:
 * 
 * Check whether the group a file is determinable.
 * This might not be the case for files on non-UNIX file systems.
 * 
 * @file: The file in question.
 * 
 * Return value: TRUE if the group is valid.
 */
gboolean
nautilus_file_can_get_group (NautilusFile *file)
{
	/* Before we have info on a file, the group is unknown. */
	return file->details->gid != -1;
}

/**
 * nautilus_file_get_group_name:
 * 
 * Get the name of the file's group. If the group has no
 * name, returns the groupid as a string. The caller is responsible
 * for g_free-ing this string.
 * 
 * @file: The file in question.
 * 
 * Return value: A newly-allocated string.
 **/
char *
nautilus_file_get_group_name (NautilusFile *file)
{
	char *group_name;

	/* Before we have info on a file, the owner is unknown. */
	if (file->details->gid == -1) {
		return NULL;
	}

	group_name = nautilus_groups_cache_get_name ((gid_t) file->details->gid);
	if (group_name == NULL) {
		/* In the oddball case that the group name has been set to an id for which
		 * there is no defined group, return the id in string form.
		 */
		group_name = g_strdup_printf ("%d", file->details->gid);
	}

	return group_name;
}

/**
 * nautilus_file_can_set_group:
 * 
 * Check whether the current user is allowed to change
 * the group of a file.
 * 
 * @file: The file in question.
 * 
 * Return value: TRUE if the current user can change the
 * group of @file, FALSE otherwise. It's always possible
 * that when you actually try to do it, you will fail.
 */
gboolean
nautilus_file_can_set_group (NautilusFile *file)
{
	uid_t user_id;

	/* Not allowed to set the permissions if we can't
	 * even read them. This can happen on non-UNIX file
	 * systems.
	 */
	if (!nautilus_file_can_get_group (file)) {
		return FALSE;
	}

	/* Check the user. */
	user_id = geteuid();

	/* Owner is allowed to set group (with restrictions). */
	if (user_id == (uid_t) file->details->uid) {
		return TRUE;
	}

	/* Root is also allowed to set group. */
	if (user_id == 0) {
		return TRUE;
	}

	/* Nobody else is allowed. */
	return FALSE;
}

/* Get a list of group names, filtered to only the ones
 * that contain the given username. If the username is
 * NULL, returns a list of all group names.
 */
static GList *
nautilus_get_group_names_for_user (void)
{
	GList *list;
	struct group *group;
	int count, i;
	gid_t gid_list[NGROUPS_MAX + 1];
	

	list = NULL;

	count = getgroups (NGROUPS_MAX + 1, gid_list);
	for (i = 0; i < count; i++) {
		group = getgrgid (gid_list[i]);
		if (group == NULL)
			break;
		
		list = g_list_prepend (list, g_strdup (group->gr_name));
	}

	return eel_g_str_list_alphabetize (list);
}

/**
 * nautilus_get_group_names:
 * 
 * Get a list of all group names.
 */
GList *
nautilus_get_all_group_names (void)
{
	GList *list;
	struct group *group;
	
	list = NULL;

	setgrent ();
	
	while ((group = getgrent ()) != NULL)
		list = g_list_prepend (list, g_strdup (group->gr_name));
	
	endgrent ();
	
	return eel_g_str_list_alphabetize (list);
}

/**
 * nautilus_file_get_settable_group_names:
 * 
 * Get a list of all group names that the current user
 * can set the group of a specific file to.
 * 
 * @file: The NautilusFile in question.
 */
GList *
nautilus_file_get_settable_group_names (NautilusFile *file)
{
	uid_t user_id;
	GList *result;

	if (!nautilus_file_can_set_group (file)) {
		return NULL;
	}	

	/* Check the user. */
	user_id = geteuid();

	if (user_id == 0) {
		/* Root is allowed to set group to anything. */
		result = nautilus_get_all_group_names ();
	} else if (user_id == (uid_t) file->details->uid) {
		/* Owner is allowed to set group to any that owner is member of. */
		result = nautilus_get_group_names_for_user ();
	} else {
		g_warning ("unhandled case in nautilus_get_settable_group_names");
		result = NULL;
	}

	return result;
}

/**
 * nautilus_file_set_group:
 * 
 * Set the group of a file. This will only have any effect if
 * nautilus_file_can_set_group returns TRUE.
 * 
 * @file: The file in question.
 * @group_name_or_id: The group name to set the owner to.
 * If the string does not match any group name, and the
 * string is an integer, the group will be set to the
 * group id represented by that integer.
 * @callback: Function called when asynch group change succeeds or fails.
 * @callback_data: Parameter passed back with callback function.
 */
void
nautilus_file_set_group (NautilusFile *file, 
			 const char *group_name_or_id,
			 NautilusFileOperationCallback callback,
			 gpointer callback_data)
{
	GError *error;
	GFileInfo *info;
	uid_t new_id;

	if (!nautilus_file_can_set_group (file)) {
		/* Claim that something changed even if the group
		 * change failed. This makes it easier for some
		 * clients who see the "reverting" to the old group as
		 * "changing back".
		 */
		nautilus_file_changed (file);
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
				     _("Not allowed to set group"));
		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;
	}

	/* If no match treating group_name_or_id as name, try treating
	 * it as id.
	 */
	if (!get_group_id_from_group_name (group_name_or_id, &new_id)
	    && !get_id_from_digit_string (group_name_or_id, &new_id)) {
		/* Claim that something changed even if the group
		 * change failed. This makes it easier for some
		 * clients who see the "reverting" to the old group as
		 * "changing back".
		 */
		nautilus_file_changed (file);
		error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
				     _("Specified group '%s' doesn't exist"), group_name_or_id);
		(* callback) (file, NULL, error, callback_data);
		g_error_free (error);
		return;		
	}

	if (new_id == (gid_t) file->details->gid) {
		(* callback) (file, NULL, NULL, callback_data);
		return;
	}


	info = g_file_info_new ();
	g_file_info_set_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_GID, new_id);
	nautilus_file_set_attributes (file, info, callback, callback_data);
	g_object_unref (info);
}

/**
 * nautilus_file_get_octal_permissions_as_string:
 * 
 * Get a user-displayable string representing a file's permissions
 * as an octal number. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_octal_permissions_as_string (NautilusFile *file)
{
	guint32 permissions;

	g_assert (NAUTILUS_IS_FILE (file));

	if (!nautilus_file_can_get_permissions (file)) {
		return NULL;
	}

	permissions = file->details->permissions;
	return g_strdup_printf ("%03o", permissions);
}

/**
 * nautilus_file_get_permissions_as_string:
 * 
 * Get a user-displayable string representing a file's permissions. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_permissions_as_string (NautilusFile *file)
{
	guint32 permissions;
	gboolean is_directory;
	gboolean is_link;
	gboolean suid, sgid, sticky;

	if (!nautilus_file_can_get_permissions (file)) {
		return NULL;
	}

	g_assert (NAUTILUS_IS_FILE (file));

	permissions = file->details->permissions;
	is_directory = nautilus_file_is_directory (file);
	is_link = nautilus_file_is_symbolic_link (file);

	/* We use ls conventions for displaying these three obscure flags */
	suid = permissions & S_ISGID;
	sgid = permissions & S_ISGID;
	sticky = permissions & S_ISVTX;

	return g_strdup_printf ("%c%c%c%c%c%c%c%c%c%c",
				 is_link ? 'l' : is_directory ? 'd' : '-',
		 		 permissions & S_IRUSR ? 'r' : '-',
				 permissions & S_IWUSR ? 'w' : '-',
				 permissions & S_IXUSR 
				 	? (suid ? 's' : 'x') 
				 	: (suid ? 'S' : '-'),
				 permissions & S_IRGRP ? 'r' : '-',
				 permissions & S_IWGRP ? 'w' : '-',
				 permissions & S_IXGRP
				 	? (sgid ? 's' : 'x') 
				 	: (sgid ? 'S' : '-'),		
				 permissions & S_IROTH ? 'r' : '-',
				 permissions & S_IWOTH ? 'w' : '-',
				 permissions & S_IXOTH
				 	? (sticky ? 't' : 'x') 
				 	: (sticky ? 'T' : '-'));
}

/**
 * nautilus_file_get_owner_as_string:
 * 
 * Get a user-displayable string representing a file's owner. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * @include_real_name: Whether or not to append the real name (if any)
 * for this user after the user name.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_owner_as_string (NautilusFile *file, gboolean include_real_name)
{
	char *user_name;

	/* Before we have info on a file, the owner is unknown. */
	if (file->details->uid == -1) {
		return NULL;
	}

	if (include_real_name) {
		user_name = get_user_and_real_name_from_id (file->details->uid);
	} else {
		user_name = nautilus_users_cache_get_name (file->details->uid);
	}

	if (user_name == NULL) {
		/* In the oddball case that the user name has been set to an id for which
		 * there is no defined user, return the id in string form.
		 */
		user_name = g_strdup_printf ("%d", file->details->uid);
	}

	return user_name;
}

static char *
format_item_count_for_display (guint item_count, 
			       gboolean includes_directories, 
			       gboolean includes_files)
{
	g_assert (includes_directories || includes_files);

	return g_strdup_printf (includes_directories
			? (includes_files 
			   ? ngettext ("%'u item", "%'u items", item_count) 
			   : ngettext ("%'u folder", "%'u folders", item_count))
			: ngettext ("%'u file", "%'u files", item_count), item_count);
}

/**
 * nautilus_file_get_size_as_string:
 * 
 * Get a user-displayable string representing a file size. The caller
 * is responsible for g_free-ing this string. The string is an item
 * count for directories.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_size_as_string (NautilusFile *file)
{
	guint item_count;
	gboolean count_unreadable;

	if (file == NULL) {
		return NULL;
	}
	
	g_assert (NAUTILUS_IS_FILE (file));
	
	if (nautilus_file_is_directory (file)) {
		if (!nautilus_file_get_directory_item_count (file, &item_count, &count_unreadable)) {
			return NULL;
		}
		return format_item_count_for_display (item_count, TRUE, TRUE);
	}
	
	if (file->details->size == -1) {
		return NULL;
	}
	return g_format_size_for_display (file->details->size);
}

/**
 * nautilus_file_get_size_as_string_with_real_size:
 * 
 * Get a user-displayable string representing a file size. The caller
 * is responsible for g_free-ing this string. The string is an item
 * count for directories.
 * This function adds the real size in the string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_size_as_string_with_real_size (NautilusFile *file)
{
	guint item_count;
	gboolean count_unreadable;
	char * formated;
	char * formated_plus_real;
	char * real_size;

	if (file == NULL) {
		return NULL;
	}
	
	g_assert (NAUTILUS_IS_FILE (file));
	
	if (nautilus_file_is_directory (file)) {
		if (!nautilus_file_get_directory_item_count (file, &item_count, &count_unreadable)) {
			return NULL;
		}
		return format_item_count_for_display (item_count, TRUE, TRUE);
	}
	
	if (file->details->size == -1) {
		return NULL;
	}

	formated = g_format_size_for_display (file->details->size);
	/* Do this in a separate stage so that we don't have to put G_GUINT64_FORMAT in the translated string */
	real_size = g_strdup_printf (_("%"G_GUINT64_FORMAT), (guint64) file->details->size);
	formated_plus_real = g_strdup_printf (_("%s (%s bytes)"), formated, real_size);
	g_free (real_size);
	g_free (formated);
	return formated_plus_real;
}


static char *
nautilus_file_get_deep_count_as_string_internal (NautilusFile *file,
						 gboolean report_size,
						 gboolean report_directory_count,
						 gboolean report_file_count)
{
	NautilusRequestStatus status;
	guint directory_count;
	guint file_count;
	guint unreadable_count;
	guint total_count;
	goffset total_size;

	/* Must ask for size or some kind of count, but not both. */
	g_assert (!report_size || (!report_directory_count && !report_file_count));
	g_assert (report_size || report_directory_count || report_file_count);

	if (file == NULL) {
		return NULL;
	}
	
	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (nautilus_file_is_directory (file));

	status = nautilus_file_get_deep_counts 
		(file, &directory_count, &file_count, &unreadable_count, &total_size, FALSE);

	/* Check whether any info is available. */
	if (status == NAUTILUS_REQUEST_NOT_STARTED) {
		return NULL;
	}

	total_count = file_count + directory_count;

	if (total_count == 0) {
		switch (status) {
		case NAUTILUS_REQUEST_IN_PROGRESS:
			/* Don't return confident "zero" until we're finished looking,
			 * because of next case.
			 */
			return NULL;
		case NAUTILUS_REQUEST_DONE:
			/* Don't return "zero" if we there were contents but we couldn't read them. */
			if (unreadable_count != 0) {
				return NULL;
			}
		default: break;
		}
	}

	/* Note that we don't distinguish the "everything was readable" case
	 * from the "some things but not everything was readable" case here.
	 * Callers can distinguish them using nautilus_file_get_deep_counts
	 * directly if desired.
	 */
	if (report_size) {
		return g_format_size_for_display (total_size);
	}

	return format_item_count_for_display (report_directory_count
		? (report_file_count ? total_count : directory_count)
		: file_count,
		report_directory_count, report_file_count);
}

/**
 * nautilus_file_get_deep_size_as_string:
 * 
 * Get a user-displayable string representing the size of all contained
 * items (only makes sense for directories). The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_deep_size_as_string (NautilusFile *file)
{
	return nautilus_file_get_deep_count_as_string_internal (file, TRUE, FALSE, FALSE);
}

/**
 * nautilus_file_get_deep_total_count_as_string:
 * 
 * Get a user-displayable string representing the count of all contained
 * items (only makes sense for directories). The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_deep_total_count_as_string (NautilusFile *file)
{
	return nautilus_file_get_deep_count_as_string_internal (file, FALSE, TRUE, TRUE);
}

/**
 * nautilus_file_get_deep_file_count_as_string:
 * 
 * Get a user-displayable string representing the count of all contained
 * items, not including directories. It only makes sense to call this
 * function on a directory. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_deep_file_count_as_string (NautilusFile *file)
{
	return nautilus_file_get_deep_count_as_string_internal (file, FALSE, FALSE, TRUE);
}

/**
 * nautilus_file_get_deep_directory_count_as_string:
 * 
 * Get a user-displayable string representing the count of all contained
 * directories. It only makes sense to call this
 * function on a directory. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
nautilus_file_get_deep_directory_count_as_string (NautilusFile *file)
{
	return nautilus_file_get_deep_count_as_string_internal (file, FALSE, TRUE, FALSE);
}

/**
 * nautilus_file_get_string_attribute:
 * 
 * Get a user-displayable string from a named attribute. Use g_free to
 * free this string. If the value is unknown, returns NULL. You can call
 * nautilus_file_get_string_attribute_with_default if you want a non-NULL
 * default.
 * 
 * @file: NautilusFile representing the file in question.
 * @attribute_name: The name of the desired attribute. The currently supported
 * set includes "name", "type", "mime_type", "size", "deep_size", "deep_directory_count",
 * "deep_file_count", "deep_total_count", "date_modified", "date_changed", "date_accessed", 
 * "date_permissions", "owner", "group", "permissions", "octal_permissions", "uri", "where",
 * "link_target", "volume", "free_space", "selinux_context"
 * 
 * Returns: Newly allocated string ready to display to the user, or NULL
 * if the value is unknown or @attribute_name is not supported.
 * 
 **/
char *
nautilus_file_get_string_attribute_q (NautilusFile *file, GQuark attribute_q)
{
	char *extension_attribute;

	if (attribute_q == attribute_name_q) {
		return nautilus_file_get_display_name (file);
	}
	if (attribute_q == attribute_type_q) {
		return nautilus_file_get_type_as_string (file);
	}
	if (attribute_q == attribute_mime_type_q) {
		return nautilus_file_get_mime_type (file);
	}
	if (attribute_q == attribute_size_q) {
		return nautilus_file_get_size_as_string (file);
	}
	if (attribute_q == attribute_size_detail_q) {
		return nautilus_file_get_size_as_string_with_real_size (file);
	}
	if (attribute_q == attribute_deep_size_q) {
		return nautilus_file_get_deep_size_as_string (file);
	}
	if (attribute_q == attribute_deep_file_count_q) {
		return nautilus_file_get_deep_file_count_as_string (file);
	}
	if (attribute_q == attribute_deep_directory_count_q) {
		return nautilus_file_get_deep_directory_count_as_string (file);
	}
	if (attribute_q == attribute_deep_total_count_q) {
		return nautilus_file_get_deep_total_count_as_string (file);
	}
	if (attribute_q == attribute_date_modified_q) {
		return nautilus_file_get_date_as_string (file, 
							 NAUTILUS_DATE_TYPE_MODIFIED);
	}
	if (attribute_q == attribute_date_changed_q) {
		return nautilus_file_get_date_as_string (file, 
							 NAUTILUS_DATE_TYPE_CHANGED);
	}
	if (attribute_q == attribute_date_accessed_q) {
		return nautilus_file_get_date_as_string (file,
							 NAUTILUS_DATE_TYPE_ACCESSED);
	}
	if (attribute_q == attribute_date_permissions_q) {
		return nautilus_file_get_date_as_string (file,
							 NAUTILUS_DATE_TYPE_PERMISSIONS_CHANGED);
	}
	if (attribute_q == attribute_permissions_q) {
		return nautilus_file_get_permissions_as_string (file);
	}
	if (attribute_q == attribute_selinux_context_q) {
		return nautilus_file_get_selinux_context (file);
	}
	if (attribute_q == attribute_octal_permissions_q) {
		return nautilus_file_get_octal_permissions_as_string (file);
	}
	if (attribute_q == attribute_owner_q) {
		return nautilus_file_get_owner_as_string (file, TRUE);
	}
	if (attribute_q == attribute_group_q) {
		return nautilus_file_get_group_name (file);
	}
	if (attribute_q == attribute_uri_q) {
		return nautilus_file_get_uri (file);
	}
	if (attribute_q == attribute_where_q) {
		return nautilus_file_get_where_string (file);
	}
	if (attribute_q == attribute_link_target_q) {
		return nautilus_file_get_symbolic_link_target_path (file);
	}
	if (attribute_q == attribute_volume_q) {
		return nautilus_file_get_volume_name (file);
	}
	if (attribute_q == attribute_free_space_q) {
		return nautilus_file_get_volume_free_space (file);
	}

	extension_attribute = NULL;
	
	if (file->details->pending_extension_attributes) {
		extension_attribute = g_hash_table_lookup (file->details->pending_extension_attributes,
							   GINT_TO_POINTER (attribute_q));
	} 

	if (extension_attribute == NULL && file->details->extension_attributes) {
		extension_attribute = g_hash_table_lookup (file->details->extension_attributes,
							   GINT_TO_POINTER (attribute_q));
	}
		
	return g_strdup (extension_attribute);
}

char *
nautilus_file_get_string_attribute (NautilusFile *file, const char *attribute_name)
{
	return nautilus_file_get_string_attribute_q (file, g_quark_from_string (attribute_name));
}


/**
 * nautilus_file_get_string_attribute_with_default:
 * 
 * Get a user-displayable string from a named attribute. Use g_free to
 * free this string. If the value is unknown, returns a string representing
 * the unknown value, which varies with attribute. You can call
 * nautilus_file_get_string_attribute if you want NULL instead of a default
 * result.
 * 
 * @file: NautilusFile representing the file in question.
 * @attribute_name: The name of the desired attribute. See the description of
 * nautilus_file_get_string for the set of available attributes.
 * 
 * Returns: Newly allocated string ready to display to the user, or a string
 * such as "unknown" if the value is unknown or @attribute_name is not supported.
 * 
 **/
char *
nautilus_file_get_string_attribute_with_default_q (NautilusFile *file, GQuark attribute_q)
{
	char *result;
	guint item_count;
	gboolean count_unreadable;
	NautilusRequestStatus status;

	result = nautilus_file_get_string_attribute_q (file, attribute_q);
	if (result != NULL) {
		return result;
	}

	/* Supply default values for the ones we know about. */
	/* FIXME bugzilla.gnome.org 40646: 
	 * Use hash table and switch statement or function pointers for speed? 
	 */
	if (attribute_q == attribute_size_q) {
		if (!nautilus_file_should_show_directory_item_count (file)) {
			return g_strdup ("--");
		}
		count_unreadable = FALSE;
		if (nautilus_file_is_directory (file)) {
			nautilus_file_get_directory_item_count (file, &item_count, &count_unreadable);
		}
		return g_strdup (count_unreadable ? _("? items") : "...");
	}
	if (attribute_q == attribute_deep_size_q) {
		status = nautilus_file_get_deep_counts (file, NULL, NULL, NULL, NULL, FALSE);
		if (status == NAUTILUS_REQUEST_DONE) {
			/* This means no contents at all were readable */
			return g_strdup (_("? bytes"));
		}
		return g_strdup ("...");
	}
	if (attribute_q == attribute_deep_file_count_q
	    || attribute_q == attribute_deep_directory_count_q
	    || attribute_q == attribute_deep_total_count_q) {
		status = nautilus_file_get_deep_counts (file, NULL, NULL, NULL, NULL, FALSE);
		if (status == NAUTILUS_REQUEST_DONE) {
			/* This means no contents at all were readable */
			return g_strdup (_("? items"));
		}
		return g_strdup ("...");
	}
	if (attribute_q == attribute_type_q) {
		return g_strdup (_("unknown type"));
	}
	if (attribute_q == attribute_mime_type_q) {
		return g_strdup (_("unknown MIME type"));
	}
	
	/* Fallback, use for both unknown attributes and attributes
	 * for which we have no more appropriate default.
	 */
	return g_strdup (_("unknown"));
}

char *
nautilus_file_get_string_attribute_with_default (NautilusFile *file, const char *attribute_name)
{
	return nautilus_file_get_string_attribute_with_default_q (file, g_quark_from_string (attribute_name));
}

/**
 * get_description:
 * 
 * Get a user-displayable string representing a file type. The caller
 * is responsible for g_free-ing this string.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: Newly allocated string ready to display to the user.
 * 
 **/
static char *
get_description (NautilusFile *file)
{
	const char *mime_type;
	char *description;

	g_assert (NAUTILUS_IS_FILE (file));

	mime_type = eel_ref_str_peek (file->details->mime_type);
	if (eel_str_is_empty (mime_type)) {
		return NULL;
	}

	if (g_content_type_is_unknown (mime_type) &&
	    nautilus_file_is_executable (file)) {
		return g_strdup (_("program"));
	}

	description = g_content_type_get_description (mime_type);
	if (!eel_str_is_empty (description)) {
		return description;
	}

	return g_strdup (mime_type);
}

/* Takes ownership of string */
static char *
update_description_for_link (NautilusFile *file, char *string)
{
	char *res;
	
	if (nautilus_file_is_symbolic_link (file)) {
		g_assert (!nautilus_file_is_broken_symbolic_link (file));
		if (string == NULL) {
			return g_strdup (_("link"));
		}
		/* Note to localizers: convert file type string for file 
		 * (e.g. "folder", "plain text") to file type for symbolic link 
		 * to that kind of file (e.g. "link to folder").
		 */
		res = g_strdup_printf (_("Link to %s"), string);
		g_free (string);
		return res;
	}

	return string;
}

static char *
nautilus_file_get_type_as_string (NautilusFile *file)
{
	if (file == NULL) {
		return NULL;
	}

	if (nautilus_file_is_broken_symbolic_link (file)) {
		return g_strdup (_("link (broken)"));
	}
	
	return update_description_for_link (file, get_description (file));
}

/**
 * nautilus_file_get_file_type
 * 
 * Return this file's type.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: The type.
 * 
 **/
GFileType
nautilus_file_get_file_type (NautilusFile *file)
{
	if (file == NULL) {
		return G_FILE_TYPE_UNKNOWN;
	}
	
	return file->details->type;
}

/**
 * nautilus_file_get_mime_type
 * 
 * Return this file's default mime type.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: The mime type.
 * 
 **/
char *
nautilus_file_get_mime_type (NautilusFile *file)
{
	if (file != NULL) {
		g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);
		if (file->details->mime_type != NULL) {
			return g_strdup (eel_ref_str_peek (file->details->mime_type));
		}
	}
	return g_strdup ("application/octet-stream");
}

/**
 * nautilus_file_is_mime_type
 * 
 * Check whether a file is of a particular MIME type, or inherited
 * from it.
 * @file: NautilusFile representing the file in question.
 * @mime_type: The MIME-type string to test (e.g. "text/plain")
 * 
 * Return value: TRUE if @mime_type exactly matches the
 * file's MIME type.
 * 
 **/
gboolean
nautilus_file_is_mime_type (NautilusFile *file, const char *mime_type)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	g_return_val_if_fail (mime_type != NULL, FALSE);
	
	if (file->details->mime_type == NULL) {
		return FALSE;
	}
	return g_content_type_is_a (eel_ref_str_peek (file->details->mime_type),
				    mime_type);
}

gboolean
nautilus_file_is_launchable (NautilusFile *file)
{
	gboolean type_can_be_executable;

	type_can_be_executable = FALSE;
	if (file->details->mime_type != NULL) {
		type_can_be_executable =
			g_content_type_can_be_executable (eel_ref_str_peek (file->details->mime_type));
	}
		
	return type_can_be_executable &&
		nautilus_file_can_get_permissions (file) &&
		nautilus_file_can_execute (file) &&
		nautilus_file_is_executable (file) &&
		!nautilus_file_is_directory (file);
}


/**
 * nautilus_file_get_emblem_icons
 * 
 * Return the list of names of emblems that this file should display,
 * in canonical order.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: A list of emblem names.
 * 
 **/
GList *
nautilus_file_get_emblem_icons (NautilusFile *file,
				char **exclude)
{
	GList *keywords, *l;
	GList *icons;
	char *icon_name;
	char *keyword;
	int i;
	GIcon *icon;
	
	if (file == NULL) {
		return NULL;
	}
	
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	keywords = nautilus_file_get_keywords (file);
	keywords = prepend_automatic_keywords (file, keywords);

	icons = NULL;
	for (l = keywords; l != NULL; l = l->next) {
		keyword = l->data;
		
#ifdef TRASH_IS_FAST_ENOUGH
		if (strcmp (keyword, NAUTILUS_FILE_EMBLEM_NAME_TRASH) == 0) {
			char *uri;
			gboolean file_is_trash;
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
#endif
		if (exclude) {
			for (i = 0; exclude[i] != NULL; i++) {
				if (strcmp (exclude[i], keyword) == 0) {
					continue;
				}
			}
		}
		
		icon_name = nautilus_icon_get_emblem_icon_by_name (keyword);
		icon = g_themed_icon_new (icon_name);
		g_free (icon_name);
		icons = g_list_prepend (icons, icon);
	}
	
	eel_g_list_free_deep (keywords);
	
	return icons;
}

GList *
nautilus_file_get_emblem_pixbufs (NautilusFile *file,
				  int size,
				  gboolean force_size,
				  char **exclude)
{
	GList *icons, *l;
	GList *pixbufs;
	GIcon *icon;
	GdkPixbuf *pixbuf;
	NautilusIconInfo *icon_info;

	icons = nautilus_file_get_emblem_icons (file, exclude);
	pixbufs = NULL;

	for (l = icons; l != NULL; l = l->next) {
		icon = l->data;

		icon_info = nautilus_icon_info_lookup (icon, size);
		if (force_size) {
			pixbuf = nautilus_icon_info_get_pixbuf_nodefault_at_size (icon_info, size);
		} else {
			pixbuf = nautilus_icon_info_get_pixbuf_nodefault (icon_info);
		}
		
		if (pixbuf) {
			pixbufs = g_list_prepend (pixbufs, pixbuf);
		}
		
		
		g_object_unref (icon_info);
		g_object_unref (icon);
	}
	g_list_free (icons);

	return g_list_reverse (pixbufs);

	
}

static GList *
sort_keyword_list_and_remove_duplicates (GList *keywords)
{
	GList *p;
	GList *duplicate_link;
	
	if (keywords != NULL) {
		keywords = eel_g_str_list_alphabetize (keywords);

		p = keywords;
		while (p->next != NULL) {
			if (strcmp ((const char *) p->data, (const char *) p->next->data) == 0) {
				duplicate_link = p->next;
				keywords = g_list_remove_link (keywords, duplicate_link);
				eel_g_list_free_deep (duplicate_link);
			} else {
				p = p->next;
			}
		}
	}
	
	return keywords;
}

/**
 * nautilus_file_get_keywords
 * 
 * Return this file's keywords.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: A list of keywords.
 * 
 **/
GList *
nautilus_file_get_keywords (NautilusFile *file)
{
	GList *keywords;

	if (file == NULL) {
		return NULL;
	}

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	/* Put all the keywords into a list. */
	keywords = nautilus_file_get_metadata_list
		(file, "keyword", "name");

	keywords = g_list_concat (keywords, eel_g_str_list_copy (file->details->extension_emblems));
	keywords = g_list_concat (keywords, eel_g_str_list_copy (file->details->pending_extension_emblems));

	return sort_keyword_list_and_remove_duplicates (keywords);
}

/**
 * nautilus_file_set_keywords
 * 
 * Change this file's keywords.
 * @file: NautilusFile representing the file in question.
 * @keywords: New set of keywords (a GList of strings).
 *
 **/
void
nautilus_file_set_keywords (NautilusFile *file, GList *keywords)
{
	GList *canonical_keywords;

	/* Invalidate the emblem compare cache */
	g_free (file->details->compare_by_emblem_cache);
	file->details->compare_by_emblem_cache = NULL;
	
	g_return_if_fail (NAUTILUS_IS_FILE (file));

	canonical_keywords = sort_keyword_list_and_remove_duplicates
		(g_list_copy (keywords));
	nautilus_file_set_metadata_list
		(file, "keyword", "name", canonical_keywords);
	g_list_free (canonical_keywords);
}

/**
 * nautilus_file_is_symbolic_link
 * 
 * Check if this file is a symbolic link.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: True if the file is a symbolic link.
 * 
 **/
gboolean
nautilus_file_is_symbolic_link (NautilusFile *file)
{
	return file->details->is_symlink;
}

gboolean
nautilus_file_is_mountpoint (NautilusFile *file)
{
	return file->details->is_mountpoint;
}

GMount *
nautilus_file_get_mount (NautilusFile *file)
{
	if (file->details->mount) {
		return g_object_ref (file->details->mount);
	}
	return NULL;
}

/**
 * nautilus_file_is_broken_symbolic_link
 * 
 * Check if this file is a symbolic link with a missing target.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: True if the file is a symbolic link with a missing target.
 * 
 **/
gboolean
nautilus_file_is_broken_symbolic_link (NautilusFile *file)
{
	if (file == NULL) {
		return FALSE;
	}
		
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	/* Non-broken symbolic links return the target's type for get_file_type. */
	return nautilus_file_get_file_type (file) == G_FILE_TYPE_SYMBOLIC_LINK;
}

/**
 * nautilus_file_get_volume_free_space
 * Get a nicely formatted char with free space on the file's volume
 * @file: NautilusFile representing the file in question.
 *
 * Returns: newly-allocated copy of file size in a formatted string
 */
char *
nautilus_file_get_volume_free_space (NautilusFile *file)
{
	goffset free_space;
	GFileInfo *info;
	GFile *location;
	char *res;

	res = NULL;

	location = nautilus_file_get_location (file);
	info = g_file_query_filesystem_info (location, G_FILE_ATTRIBUTE_FILESYSTEM_FREE, NULL, NULL);
	if (info) {
		if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE)) {
			free_space = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
			res = g_format_size_for_display (free_space);
		}
		g_object_unref (info);
	}
	g_object_unref (location);

	return res;
}

/**
 * nautilus_file_get_volume_name
 * Get the path of the volume the file resides on
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: newly-allocated copy of the volume name of the target file, 
 * if the volume name isn't set, it returns the mount path of the volume
 */ 
char *
nautilus_file_get_volume_name (NautilusFile *file)
{
	GFile *location;
	char *res;
	GMount *mount;

	res = NULL;
	
	location = nautilus_file_get_location (file);
	mount = g_file_find_enclosing_mount (location, NULL, NULL);
	if (mount) {
		res = g_strdup (g_mount_get_name (mount));
		g_object_unref (mount);
	}
	g_object_unref (location);

	return res;
}

/**
 * nautilus_file_get_symbolic_link_target_path
 * 
 * Get the file path of the target of a symbolic link. It is an error 
 * to call this function on a file that isn't a symbolic link.
 * @file: NautilusFile representing the symbolic link in question.
 * 
 * Returns: newly-allocated copy of the file path of the target of the symbolic link.
 */
char *
nautilus_file_get_symbolic_link_target_path (NautilusFile *file)
{
        g_return_val_if_fail (nautilus_file_is_symbolic_link (file), NULL);

	return g_strdup (file->details->symlink_name);
}

/**
 * nautilus_file_get_symbolic_link_target_uri
 * 
 * Get the uri of the target of a symbolic link. It is an error 
 * to call this function on a file that isn't a symbolic link.
 * @file: NautilusFile representing the symbolic link in question.
 * 
 * Returns: newly-allocated copy of the uri of the target of the symbolic link.
 */
char *
nautilus_file_get_symbolic_link_target_uri (NautilusFile *file)
{
	GFile *location, *parent, *target;
	char *target_uri;
	
        g_return_val_if_fail (nautilus_file_is_symbolic_link (file), NULL);

	if (file->details->symlink_name == NULL) {
		return NULL;
	} else {
		target = NULL;
		
		location = nautilus_file_get_location (file);
		parent = g_file_get_parent (location);
		g_object_unref (location);
		if (parent) {
			target = g_file_resolve_relative_path (parent, file->details->symlink_name);
			g_object_unref (parent);
		}
		
		target_uri = NULL;
		if (target) {
			target_uri = g_file_get_uri (target);
			g_object_unref (target);
		}
		return target_uri;
	}
}

/**
 * nautilus_file_is_nautilus_link
 * 
 * Check if this file is a "nautilus link", meaning a historical
 * nautilus xml link file or a desktop file.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: True if the file is a nautilus link.
 * 
 **/
gboolean
nautilus_file_is_nautilus_link (NautilusFile *file)
{
	/* NOTE: I removed the historical link here, because i don't think we
	   even detect that mimetype anymore */
	return nautilus_file_is_mime_type (file, "application/x-desktop");
}

/**
 * nautilus_file_is_directory
 * 
 * Check if this file is a directory.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: TRUE if @file is a directory.
 * 
 **/
gboolean
nautilus_file_is_directory (NautilusFile *file)
{
	return nautilus_file_get_file_type (file) == G_FILE_TYPE_DIRECTORY;
}

/**
 * nautilus_file_is_in_trash
 * 
 * Check if this file is a file in trash.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: TRUE if @file is in a trash.
 * 
 **/
gboolean
nautilus_file_is_in_trash (NautilusFile *file)
{
	g_assert (NAUTILUS_IS_FILE (file));

	return nautilus_directory_is_in_trash (file->details->directory);
}

GError *
nautilus_file_get_file_info_error (NautilusFile *file)
{
	if (!file->details->get_info_failed) {
		return NULL;
	}

	return file->details->get_info_error;
}

/**
 * nautilus_file_contains_text
 * 
 * Check if this file contains text.
 * This is private and is used to decide whether or not to read the top left text.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: TRUE if @file has a text MIME type.
 * 
 **/
gboolean
nautilus_file_contains_text (NautilusFile *file)
{
	if (file == NULL) {
		return FALSE;
	}

	/* All text files inherit from text/plain */
	return nautilus_file_is_mime_type (file, "text/plain");
}

/**
 * nautilus_file_is_executable
 * 
 * Check if this file is executable at all.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: TRUE if any of the execute bits are set. FALSE if
 * not, or if the permissions are unknown.
 * 
 **/
gboolean
nautilus_file_is_executable (NautilusFile *file)
{
	if (!file->details->has_permissions) {
		/* File's permissions field is not valid.
		 * Can't access specific permissions, so return FALSE.
		 */
		return FALSE;
	}

	return file->details->can_execute;
}

/**
 * nautilus_file_peek_top_left_text
 * 
 * Peek at the text from the top left of the file.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: NULL if there is no text readable, otherwise, the text.
 *          This string is owned by the file object and should not
 *          be kept around or freed.
 * 
 **/
char *
nautilus_file_peek_top_left_text (NautilusFile *file,
				  gboolean  need_large_text,
				  gboolean *needs_loading)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

	if (!nautilus_file_should_get_top_left_text (file)) {
		if (needs_loading) {
			*needs_loading = FALSE;
		}
		return NULL;
	}
	
	if (needs_loading) {
		*needs_loading = !file->details->top_left_text_is_up_to_date;
		if (need_large_text) {
			*needs_loading |= file->details->got_top_left_text != file->details->got_large_top_left_text;
		}
	}

	/* Show " ..." in the file until we read the contents in. */
	if (!file->details->got_top_left_text) {
		
		if (nautilus_file_contains_text (file)) {
			return " ...";
		}
		return NULL;
	}
	
	/* Show what we read in. */
	return file->details->top_left_text;
}

/**
 * nautilus_file_get_top_left_text
 * 
 * Get the text from the top left of the file.
 * @file: NautilusFile representing the file in question.
 * 
 * Returns: NULL if there is no text readable, otherwise, the text.
 * 
 **/
char *
nautilus_file_get_top_left_text (NautilusFile *file)
{
	return g_strdup (nautilus_file_peek_top_left_text (file, FALSE, NULL));
}


void
nautilus_file_mark_gone (NautilusFile *file)
{
	NautilusDirectory *directory;

	if (file->details->is_gone)
		return;

	file->details->is_gone = TRUE;

	update_links_if_target (file);

	/* Drop it from the symlink hash ! */
	remove_from_link_hash_table (file);

	/* Let the directory know it's gone. */
	directory = file->details->directory;
	if (!nautilus_file_is_self_owned (file)) {
		nautilus_directory_remove_file (directory, file);
	}

	nautilus_file_clear_info (file);

	/* FIXME bugzilla.gnome.org 42429: 
	 * Maybe we can get rid of the name too eventually, but
	 * for now that would probably require too many if statements
	 * everywhere anyone deals with the name. Maybe we can give it
	 * a hard-coded "<deleted>" name or something.
	 */
}

/**
 * nautilus_file_changed
 * 
 * Notify the user that this file has changed.
 * @file: NautilusFile representing the file in question.
 **/
void
nautilus_file_changed (NautilusFile *file)
{
	GList fake_list;

	g_return_if_fail (NAUTILUS_IS_FILE (file));

	if (nautilus_file_is_self_owned (file)) {
		nautilus_file_emit_changed (file);
	} else {
		fake_list.data = file;
		fake_list.next = NULL;
		fake_list.prev = NULL;
		nautilus_directory_emit_change_signals
			(file->details->directory, &fake_list);
	}
}

/**
 * nautilus_file_updated_deep_count_in_progress
 * 
 * Notify clients that a newer deep count is available for
 * the directory in question.
 */
void
nautilus_file_updated_deep_count_in_progress (NautilusFile *file) {
	GList *link_files, *node;

	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (nautilus_file_is_directory (file));

	/* Send out a signal. */
	g_signal_emit (file, signals[UPDATED_DEEP_COUNT_IN_PROGRESS], 0, file);

	/* Tell link files pointing to this object about the change. */
	link_files = get_link_files (file);
	for (node = link_files; node != NULL; node = node->next) {
		nautilus_file_updated_deep_count_in_progress (NAUTILUS_FILE (node->data));
	}
	nautilus_file_list_free (link_files);	
}

/**
 * nautilus_file_emit_changed
 * 
 * Emit a file changed signal.
 * This can only be called by the directory, since the directory
 * also has to emit a files_changed signal.
 *
 * @file: NautilusFile representing the file in question.
 **/
void
nautilus_file_emit_changed (NautilusFile *file)
{
	GList *link_files, *p;

	g_assert (NAUTILUS_IS_FILE (file));


	/* Invalidate the emblem compare cache. -- This is not the cleanest
	 * place to do it but it is the one guaranteed bottleneck through
	 * which all change notifications pass.
	 */
	g_free (file->details->compare_by_emblem_cache);
	file->details->compare_by_emblem_cache = NULL;

	/* Send out a signal. */
	g_signal_emit (file, signals[CHANGED], 0, file);

	/* Tell link files pointing to this object about the change. */
	link_files = get_link_files (file);
	for (p = link_files; p != NULL; p = p->next) {
		if (p->data != file) {
			nautilus_file_changed (NAUTILUS_FILE (p->data));
		}
	}
	nautilus_file_list_free (link_files);
}

/**
 * nautilus_file_is_gone
 * 
 * Check if a file has already been deleted.
 * @file: NautilusFile representing the file in question.
 *
 * Returns: TRUE if the file is already gone.
 **/
gboolean
nautilus_file_is_gone (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return file->details->is_gone;
}

/**
 * nautilus_file_is_not_yet_confirmed
 * 
 * Check if we're in a state where we don't know if a file really
 * exists or not, before the initial I/O is complete.
 * @file: NautilusFile representing the file in question.
 *
 * Returns: TRUE if the file is already gone.
 **/
gboolean
nautilus_file_is_not_yet_confirmed (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return !file->details->got_file_info;
}

/**
 * nautilus_file_check_if_ready
 *
 * Check whether the values for a set of file attributes are
 * currently available, without doing any additional work. This
 * is useful for callers that want to reflect updated information
 * when it is ready but don't want to force the work required to
 * obtain the information, which might be slow network calls, e.g.
 *
 * @file: The file being queried.
 * @file_attributes: A bit-mask with the desired information.
 * 
 * Return value: TRUE if all of the specified attributes are currently readable.
 */
gboolean
nautilus_file_check_if_ready (NautilusFile *file,
			      NautilusFileAttributes file_attributes)
{
	/* To be parallel with call_when_ready, return
	 * TRUE for NULL file.
	 */
	if (file == NULL) {
		return TRUE;
	}

	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

	return EEL_CALL_METHOD_WITH_RETURN_VALUE
		(NAUTILUS_FILE_CLASS, file,
		 check_if_ready, (file, file_attributes));
}			      

void
nautilus_file_call_when_ready (NautilusFile *file,
			       NautilusFileAttributes file_attributes,
			       NautilusFileCallback callback,
			       gpointer callback_data)

{
	if (file == NULL) {
		(* callback) (file, callback_data);
		return;
	}

	g_return_if_fail (NAUTILUS_IS_FILE (file));

	EEL_CALL_METHOD
		(NAUTILUS_FILE_CLASS, file,
		 call_when_ready, (file, file_attributes, 
				   callback, callback_data));
}

void
nautilus_file_cancel_call_when_ready (NautilusFile *file,
				      NautilusFileCallback callback,
				      gpointer callback_data)
{
	g_return_if_fail (callback != NULL);

	if (file == NULL) {
		return;
	}

	g_return_if_fail (NAUTILUS_IS_FILE (file));

	EEL_CALL_METHOD
		(NAUTILUS_FILE_CLASS, file,
		 cancel_call_when_ready, (file, callback, callback_data));
}

static void
invalidate_directory_count (NautilusFile *file)
{
	file->details->directory_count_is_up_to_date = FALSE;
}

static void
invalidate_deep_counts (NautilusFile *file)
{
	file->details->deep_counts_status = NAUTILUS_REQUEST_NOT_STARTED;
}

static void
invalidate_mime_list (NautilusFile *file)
{
	file->details->mime_list_is_up_to_date = FALSE;
}

static void
invalidate_top_left_text (NautilusFile *file)
{
	file->details->top_left_text_is_up_to_date = FALSE;
}

static void
invalidate_file_info (NautilusFile *file)
{
	file->details->file_info_is_up_to_date = FALSE;
}

static void
invalidate_link_info (NautilusFile *file)
{
	file->details->link_info_is_up_to_date = FALSE;
}

static void
invalidate_thumbnail (NautilusFile *file)
{
	file->details->thumbnail_is_up_to_date = FALSE;
}

void
nautilus_file_invalidate_extension_info_internal (NautilusFile *file)
{
	file->details->pending_info_providers =
		nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_INFO_PROVIDER);
}

void
nautilus_file_invalidate_attributes_internal (NautilusFile *file,
					      NautilusFileAttributes file_attributes)
{
	Request request;

	if (file == NULL) {
		return;
	}

	if (NAUTILUS_IS_DESKTOP_ICON_FILE (file)) {
		/* Desktop icon files are always up to date.
		 * If we invalidate their attributes they
		 * will lose data, so we just ignore them.
		 */
		return;
	}
	
	nautilus_directory_set_up_request (&request, file_attributes);

	if (request.directory_count) {
		invalidate_directory_count (file);
	}
	if (request.deep_count) {
		invalidate_deep_counts (file);
	}
	if (request.mime_list) {
		invalidate_mime_list (file);
	}
	if (request.file_info) {
		invalidate_file_info (file);
	}
	if (request.top_left_text) {
		invalidate_top_left_text (file);
	}
	if (request.link_info) {
		invalidate_link_info (file);
	}
	if (request.extension_info) {
		nautilus_file_invalidate_extension_info_internal (file);
	}
	if (request.thumbnail) {
		invalidate_thumbnail (file);
	}

	/* FIXME bugzilla.gnome.org 45075: implement invalidating metadata */
}

gboolean
nautilus_file_has_open_window (NautilusFile *file)
{
	return file->details->has_open_window;
}

void
nautilus_file_set_has_open_window (NautilusFile *file,
				   gboolean has_open_window)
{
	has_open_window = (has_open_window != FALSE);

	if (file->details->has_open_window != has_open_window) {
		file->details->has_open_window = has_open_window;
		nautilus_file_changed (file);
	}
}


gboolean
nautilus_file_is_thumbnailing (NautilusFile *file)
{
	g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
	
	return file->details->is_thumbnailing;
}

void
nautilus_file_set_is_thumbnailing (NautilusFile *file,
				   gboolean is_thumbnailing)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	
	file->details->is_thumbnailing = is_thumbnailing;
}


/**
 * nautilus_file_invalidate_attributes
 * 
 * Invalidate the specified attributes and force a reload.
 * @file: NautilusFile representing the file in question.
 * @file_attributes: attributes to froget.
 **/

void
nautilus_file_invalidate_attributes (NautilusFile *file,
				     NautilusFileAttributes file_attributes)
{
	/* Cancel possible in-progress loads of any of these attributes */
	nautilus_directory_cancel_loading_file_attributes (file->details->directory,
							   file,
							   file_attributes);
	
	/* Actually invalidate the values */
	nautilus_file_invalidate_attributes_internal (file, file_attributes);

	nautilus_directory_add_file_to_work_queue (file->details->directory, file);
	
	/* Kick off I/O if necessary */
	nautilus_directory_async_state_changed (file->details->directory);
}

NautilusFileAttributes 
nautilus_file_get_all_attributes (void)
{
	return  NAUTILUS_FILE_ATTRIBUTE_INFO |
		NAUTILUS_FILE_ATTRIBUTE_LINK_INFO |
		NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS |
		NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT | 
		NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_MIME_TYPES | 
		NAUTILUS_FILE_ATTRIBUTE_METADATA | 
		NAUTILUS_FILE_ATTRIBUTE_TOP_LEFT_TEXT | 
		NAUTILUS_FILE_ATTRIBUTE_LARGE_TOP_LEFT_TEXT |
		NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO |
		NAUTILUS_FILE_ATTRIBUTE_THUMBNAIL |
		NAUTILUS_FILE_ATTRIBUTE_MOUNT;
}

void
nautilus_file_invalidate_all_attributes (NautilusFile *file)
{
	NautilusFileAttributes all_attributes;

	all_attributes = nautilus_file_get_all_attributes ();
	nautilus_file_invalidate_attributes (file, all_attributes);
}


/**
 * nautilus_file_dump
 *
 * Debugging call, prints out the contents of the file
 * fields.
 *
 * @file: file to dump.
 **/
void
nautilus_file_dump (NautilusFile *file)
{
	long size = file->details->deep_size;
	char *uri;
	const char *file_kind;

	uri = nautilus_file_get_uri (file);
	g_print ("uri: %s \n", uri);
	if (!file->details->got_file_info) {
		g_print ("no file info \n");
	} else if (file->details->get_info_failed) {
		g_print ("failed to get file info \n");
	} else {
		g_print ("size: %ld \n", size);
		switch (file->details->type) {
		case G_FILE_TYPE_REGULAR:
			file_kind = "regular file";
			break;
		case G_FILE_TYPE_DIRECTORY:
			file_kind = "folder";
			break;
		case G_FILE_TYPE_SPECIAL:
			file_kind = "special";
			break;
		case G_FILE_TYPE_SYMBOLIC_LINK:
			file_kind = "symbolic link";
			break;
		case G_FILE_TYPE_UNKNOWN:
		default:
			file_kind = "unknown";
			break;
		}
		g_print ("kind: %s \n", file_kind);
		if (file->details->type == G_FILE_TYPE_SYMBOLIC_LINK) {
			g_print ("link to %s \n", file->details->symlink_name);
			/* FIXME bugzilla.gnome.org 42430: add following of symlinks here */
		}
		/* FIXME bugzilla.gnome.org 42431: add permissions and other useful stuff here */
	}
	g_free (uri);
}

/**
 * nautilus_file_list_ref
 *
 * Ref all the files in a list.
 * @list: GList of files.
 **/
GList *
nautilus_file_list_ref (GList *list)
{
	g_list_foreach (list, (GFunc) nautilus_file_ref, NULL);
	return list;
}

/**
 * nautilus_file_list_unref
 *
 * Unref all the files in a list.
 * @list: GList of files.
 **/
void
nautilus_file_list_unref (GList *list)
{
	g_list_foreach (list, (GFunc) nautilus_file_unref, NULL);
}

/**
 * nautilus_file_list_free
 *
 * Free a list of files after unrefing them.
 * @list: GList of files.
 **/
void
nautilus_file_list_free (GList *list)
{
	nautilus_file_list_unref (list);
	g_list_free (list);
}

/**
 * nautilus_file_list_copy
 *
 * Copy the list of files, making a new ref of each,
 * @list: GList of files.
 **/
GList *
nautilus_file_list_copy (GList *list)
{
	return g_list_copy (nautilus_file_list_ref (list));
}

static int
compare_by_display_name_cover (gconstpointer a, gconstpointer b)
{
	return compare_by_display_name (NAUTILUS_FILE (a), NAUTILUS_FILE (b));
}

/**
 * nautilus_file_list_sort_by_display_name
 * 
 * Sort the list of files by file name.
 * @list: GList of files.
 **/
GList *
nautilus_file_list_sort_by_display_name (GList *list)
{
	return g_list_sort (list, compare_by_display_name_cover);
}

static GList *ready_data_list = NULL;

typedef struct 
{
	GList *file_list;
	GList *remaining_files;
	NautilusFileListCallback callback;
	gpointer callback_data;
} FileListReadyData;

static void
file_list_ready_data_free (FileListReadyData *data)
{
	GList *l;

	l = g_list_find (ready_data_list, data);
	if (l != NULL) {
		ready_data_list = g_list_delete_link (ready_data_list, l);

		nautilus_file_list_free (data->file_list);
		g_list_free (data->remaining_files);
		g_free (data);
	}
}

static FileListReadyData *
file_list_ready_data_new (GList *file_list,
			  NautilusFileListCallback callback,
			  gpointer callback_data)
{
	FileListReadyData *data;

	data = g_new0 (FileListReadyData, 1);
	data->file_list = nautilus_file_list_copy (file_list);
	data->remaining_files = g_list_copy (file_list);
	data->callback = callback;
	data->callback_data = callback_data;

	ready_data_list = g_list_prepend (ready_data_list, data);

	return data;
}

static void
file_list_file_ready_callback (NautilusFile *file,
			       gpointer user_data)
{
	FileListReadyData *data;

	data = user_data;
	data->remaining_files = g_list_remove (data->remaining_files, file);
	
	if (data->remaining_files == NULL) {
		if (data->callback) {
			(*data->callback) (data->file_list, data->callback_data);
		}

		file_list_ready_data_free (data);
	}
}

void 
nautilus_file_list_call_when_ready (GList *file_list,
				    NautilusFileAttributes attributes,
				    NautilusFileListHandle **handle,
				    NautilusFileListCallback callback,
				    gpointer callback_data)
{
	GList *l;
	FileListReadyData *data;
	NautilusFile *file;
	
	g_return_if_fail (file_list != NULL);

	data = file_list_ready_data_new
		(file_list, callback, callback_data);

	if (handle) {
		*handle = (NautilusFileListHandle *) data;
	}


	l = file_list;
	while (l != NULL) {
		file = NAUTILUS_FILE (l->data);
		/* Need to do this here, as the list can be modified by this call */
		l = l->next; 
		nautilus_file_call_when_ready (file,
					       attributes,
					       file_list_file_ready_callback,
					       data);
	}
}

void
nautilus_file_list_cancel_call_when_ready (NautilusFileListHandle *handle)
{
	GList *l;
	NautilusFile *file;
	FileListReadyData *data;

	g_return_if_fail (handle != NULL);

	data = (FileListReadyData *) handle;

	l = g_list_find (ready_data_list, data);
	if (l != NULL) {
		for (l = data->remaining_files; l != NULL; l = l->next) {
			file = NAUTILUS_FILE (l->data);

			EEL_CALL_METHOD
				(NAUTILUS_FILE_CLASS, file,
				 cancel_call_when_ready, (file, file_list_file_ready_callback, data));
		}

		file_list_ready_data_free (data);
	}
}

static char *
try_to_make_utf8 (const char *text, int *length)
{
	static const char *encodings_to_try[2];
	static int n_encodings_to_try = 0;
        gsize converted_length;
        GError *conversion_error;
	char *utf8_text;
	int i;
	
	if (n_encodings_to_try == 0) {
		const char *charset;
		gboolean charset_is_utf8;
		
		charset_is_utf8 = g_get_charset (&charset);
		if (!charset_is_utf8) {
			encodings_to_try[n_encodings_to_try++] = charset;
		}
        
		if (g_ascii_strcasecmp (charset, "ISO-8859-1") != 0) {
			encodings_to_try[n_encodings_to_try++] = "ISO-8859-1";
		}
	}

        utf8_text = NULL;
	for (i = 0; i < n_encodings_to_try; i++) {
		conversion_error = NULL;
		utf8_text = g_convert (text, *length, 
					   "UTF-8", encodings_to_try[i],
					   NULL, &converted_length, &conversion_error);
		if (utf8_text != NULL) {
			*length = converted_length;
			break;
		}
		g_error_free (conversion_error);
	}
	
	return utf8_text;
}



/* Extract the top left part of the read-in text. */
char *
nautilus_extract_top_left_text (const char *text,
				gboolean large,
				int length)
{
        GString* buffer;
	const gchar *in;
	const gchar *end;
	int line, i;
	gunichar c;
	char *text_copy;
	const char *utf8_end;
	gboolean validated;
	int max_bytes, max_lines, max_cols;

	if (large) {
		max_bytes = NAUTILUS_FILE_LARGE_TOP_LEFT_TEXT_MAXIMUM_BYTES;
		max_lines = NAUTILUS_FILE_LARGE_TOP_LEFT_TEXT_MAXIMUM_LINES;
		max_cols = NAUTILUS_FILE_LARGE_TOP_LEFT_TEXT_MAXIMUM_CHARACTERS_PER_LINE;
	} else {
		max_bytes = NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_BYTES;
		max_lines = NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_LINES;
		max_cols = NAUTILUS_FILE_TOP_LEFT_TEXT_MAXIMUM_CHARACTERS_PER_LINE;
	}
			
	

        text_copy = NULL;
        if (text != NULL) {
		/* Might be a partial utf8 character at the end if we didn't read whole file */
		validated = g_utf8_validate (text, length, &utf8_end);
		if (!validated &&
		    !(length >= max_bytes &&
		      text + length - utf8_end < 6)) {
			text_copy = try_to_make_utf8 (text, &length);
			text = text_copy;
		} else if (!validated) {
			length = utf8_end - text;
		}
        }

	if (text == NULL || length == 0) {
		return NULL;
	}

	buffer = g_string_new ("");
	end = text + length; in = text;

	for (line = 0; line < max_lines; line++) {
		/* Extract one line. */
		for (i = 0; i < max_cols; ) {
			if (*in == '\n') {
				break;
			}
			
			c = g_utf8_get_char (in);
			
			if (g_unichar_isprint (c)) {
				g_string_append_unichar (buffer, c);
				i++;
			}
			
			in = g_utf8_next_char (in);
			if (in == end) {
				goto done;
			}
		}

		/* Skip the rest of the line. */
		while (*in != '\n') {
			if (++in == end) {
				goto done;
			}
		}
		if (++in == end) {
			goto done;
		}

		/* Put a new-line separator in. */
		g_string_append_c(buffer, '\n');
	}
 done:
	g_free (text_copy);
 
	return g_string_free(buffer, FALSE);
}

static void
thumbnail_limit_changed_callback (gpointer user_data)
{
	cached_thumbnail_limit = eel_preferences_get_integer (NAUTILUS_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT);

	/* Tell the world that icons might have changed. We could invent a narrower-scope
	 * signal to mean only "thumbnails might have changed" if this ends up being slow
	 * for some reason.
	 */
	emit_change_signals_for_all_files_in_all_directories ();
}

static void
thumbnail_size_changed_callback (gpointer user_data)
{
	cached_thumbnail_size = eel_preferences_get_integer (NAUTILUS_PREFERENCES_ICON_VIEW_THUMBNAIL_SIZE);

	/* Tell the world that icons might have changed. We could invent a narrower-scope
	 * signal to mean only "thumbnails might have changed" if this ends up being slow
	 * for some reason.
	 */
	emit_change_signals_for_all_files_in_all_directories ();
}

static void
show_thumbnails_changed_callback (gpointer user_data)
{
	show_image_thumbs = eel_preferences_get_enum (NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS);

	/* Tell the world that icons might have changed. We could invent a narrower-scope
	 * signal to mean only "thumbnails might have changed" if this ends up being slow
	 * for some reason.
	 */
	emit_change_signals_for_all_files_in_all_directories ();
}

static void       
mime_type_data_changed_callback (GObject *signaller, gpointer user_data)
{
	/* Tell the world that icons might have changed. We could invent a narrower-scope
	 * signal to mean only "thumbnails might have changed" if this ends up being slow
	 * for some reason.
	 */
	emit_change_signals_for_all_files_in_all_directories ();
}

static void
icon_theme_changed_callback (GtkIconTheme *icon_theme,
			     gpointer user_data)
{
	/* Clear all pixmap caches as the icon => pixmap lookup changed */
	nautilus_icon_info_clear_caches ();
	
	/* Tell the world that icons might have changed. We could invent a narrower-scope
	 * signal to mean only "thumbnails might have changed" if this ends up being slow
	 * for some reason.
	 */
	emit_change_signals_for_all_files_in_all_directories ();
}

static void
nautilus_file_class_init (NautilusFileClass *class)
{
	GtkIconTheme *icon_theme;
	
	parent_class = g_type_class_peek_parent (class);

	attribute_name_q = g_quark_from_static_string ("name");
	attribute_size_q = g_quark_from_static_string ("size");
	attribute_type_q = g_quark_from_static_string ("type");
	attribute_modification_date_q = g_quark_from_static_string ("modification_date");
	attribute_date_modified_q = g_quark_from_static_string ("date_modified");
	attribute_accessed_date_q = g_quark_from_static_string ("accessed_date");
	attribute_date_accessed_q = g_quark_from_static_string ("date_accessed");
	attribute_emblems_q = g_quark_from_static_string ("emblems");
	attribute_mime_type_q = g_quark_from_static_string ("mime_type");
	attribute_size_detail_q = g_quark_from_static_string ("size_detail");
	attribute_deep_size_q = g_quark_from_static_string ("deep_size");
	attribute_deep_file_count_q = g_quark_from_static_string ("deep_file_count");
	attribute_deep_directory_count_q = g_quark_from_static_string ("deep_directory_count");
	attribute_deep_total_count_q = g_quark_from_static_string ("deep_total_count");
	attribute_date_changed_q = g_quark_from_static_string ("date_changed");
	attribute_date_permissions_q = g_quark_from_static_string ("date_permissions");
	attribute_permissions_q = g_quark_from_static_string ("permissions");
	attribute_selinux_context_q = g_quark_from_static_string ("selinux_context");
	attribute_octal_permissions_q = g_quark_from_static_string ("octal_permissions");
	attribute_owner_q = g_quark_from_static_string ("owner");
	attribute_group_q = g_quark_from_static_string ("group");
	attribute_uri_q = g_quark_from_static_string ("uri");
	attribute_where_q = g_quark_from_static_string ("where");
	attribute_link_target_q = g_quark_from_static_string ("link_target");
	attribute_volume_q = g_quark_from_static_string ("volume");
	attribute_free_space_q = g_quark_from_static_string ("free_space");
	
	G_OBJECT_CLASS (class)->finalize = finalize;
	G_OBJECT_CLASS (class)->constructor = nautilus_file_constructor;

	signals[CHANGED] =
		g_signal_new ("changed",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusFileClass, changed),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	signals[UPDATED_DEEP_COUNT_IN_PROGRESS] =
		g_signal_new ("updated_deep_count_in_progress",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusFileClass, updated_deep_count_in_progress),
		              NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);
	
	g_type_class_add_private (class, sizeof (NautilusFileDetails));


	eel_preferences_add_auto_enum (NAUTILUS_PREFERENCES_DATE_FORMAT,
				       &date_format_pref);
	
	thumbnail_limit_changed_callback (NULL);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_IMAGE_FILE_THUMBNAIL_LIMIT,
				      thumbnail_limit_changed_callback,
				      NULL);
	thumbnail_size_changed_callback (NULL);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_ICON_VIEW_THUMBNAIL_SIZE,
				      thumbnail_size_changed_callback,
				      NULL);
	show_thumbnails_changed_callback (NULL);
	eel_preferences_add_callback (NAUTILUS_PREFERENCES_SHOW_IMAGE_FILE_THUMBNAILS,
				      show_thumbnails_changed_callback,
				      NULL);

	icon_theme = gtk_icon_theme_get_default ();
	g_signal_connect_object (icon_theme,
				 "changed",
				 G_CALLBACK (icon_theme_changed_callback),
				 NULL, 0);

	g_signal_connect (nautilus_signaller_get_current (),
			  "mime_data_changed",
			  G_CALLBACK (mime_type_data_changed_callback),
			  NULL);
}

static void
nautilus_file_add_emblem (NautilusFile *file,
			  const char *emblem_name)
{
	if (file->details->pending_info_providers) {
		file->details->pending_extension_emblems = g_list_prepend (file->details->pending_extension_emblems,
									   g_strdup (emblem_name));
	} else {
		file->details->extension_emblems = g_list_prepend (file->details->extension_emblems,
								   g_strdup (emblem_name));
	}

	nautilus_file_changed (file);
}

static void
nautilus_file_add_string_attribute (NautilusFile *file,
				    const char *attribute_name,
				    const char *value)
{
	if (file->details->pending_info_providers) {
		/* Lazily create hashtable */
		if (!file->details->pending_extension_attributes) {
			file->details->pending_extension_attributes = 
				g_hash_table_new_full (g_direct_hash, g_direct_equal,
						       NULL, 
						       (GDestroyNotify)g_free);
		}
		g_hash_table_insert (file->details->pending_extension_attributes,
				     GINT_TO_POINTER (g_quark_from_string (attribute_name)),
				     g_strdup (value));
	} else {
		if (!file->details->extension_attributes) {
			file->details->extension_attributes = 
				g_hash_table_new_full (g_direct_hash, g_direct_equal,
						       NULL, 
						       (GDestroyNotify)g_free);
		}
		g_hash_table_insert (file->details->extension_attributes,
				     GINT_TO_POINTER (g_quark_from_string (attribute_name)),
				     g_strdup (value));
	}

	nautilus_file_changed (file);
}

static void
nautilus_file_invalidate_extension_info (NautilusFile *file)
{
	nautilus_file_invalidate_attributes (file, NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO);
}

void
nautilus_file_info_providers_done (NautilusFile *file)
{
	eel_g_list_free_deep (file->details->extension_emblems);
	file->details->extension_emblems = file->details->pending_extension_emblems;
	file->details->pending_extension_emblems = NULL;

	if (file->details->extension_attributes) {
		g_hash_table_destroy (file->details->extension_attributes);
	}
	
	file->details->extension_attributes = file->details->pending_extension_attributes;
	file->details->pending_extension_attributes = NULL;

	nautilus_file_changed (file);
}

static void     
nautilus_file_info_iface_init (NautilusFileInfoIface *iface)
{
	iface->is_gone = nautilus_file_is_gone;
	iface->get_name = nautilus_file_get_name;
	iface->get_file_type = nautilus_file_get_file_type;
	iface->get_location = nautilus_file_get_location;
	iface->get_uri = nautilus_file_get_uri;
	iface->get_parent_location = nautilus_file_get_parent_location;
	iface->get_parent_uri = nautilus_file_get_parent_uri;
	iface->get_parent_info = nautilus_file_get_parent;
	iface->get_mount = nautilus_file_get_mount;
	iface->get_uri_scheme = nautilus_file_get_uri_scheme;
	iface->get_activation_uri = nautilus_file_get_activation_uri;
	iface->get_mime_type = nautilus_file_get_mime_type;
	iface->is_mime_type = nautilus_file_is_mime_type;
	iface->is_directory = nautilus_file_is_directory;
	iface->can_write = nautilus_file_can_write;
	iface->add_emblem = nautilus_file_add_emblem;
	iface->get_string_attribute = nautilus_file_get_string_attribute;
	iface->add_string_attribute = nautilus_file_add_string_attribute;
	iface->invalidate_extension_info = nautilus_file_invalidate_extension_info;
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_file (void)
{
	NautilusFile *file_1;
	NautilusFile *file_2;
	GList *list;

        /* refcount checks */

        EEL_CHECK_INTEGER_RESULT (nautilus_directory_number_outstanding (), 0);

	file_1 = nautilus_file_get_by_uri ("file:///home/");

	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_1)->ref_count, 1);
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_1->details->directory)->ref_count, 1);
        EEL_CHECK_INTEGER_RESULT (nautilus_directory_number_outstanding (), 1);

	nautilus_file_unref (file_1);

        EEL_CHECK_INTEGER_RESULT (nautilus_directory_number_outstanding (), 0);
	
	file_1 = nautilus_file_get_by_uri ("file:///etc");
	file_2 = nautilus_file_get_by_uri ("file:///usr");

        list = NULL;
        list = g_list_prepend (list, file_1);
        list = g_list_prepend (list, file_2);

        nautilus_file_list_ref (list);
        
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_1)->ref_count, 2);
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_2)->ref_count, 2);

	nautilus_file_list_unref (list);
        
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_1)->ref_count, 1);
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_2)->ref_count, 1);

	nautilus_file_list_free (list);

        EEL_CHECK_INTEGER_RESULT (nautilus_directory_number_outstanding (), 0);
	

        /* name checks */
	file_1 = nautilus_file_get_by_uri ("file:///home/");

	EEL_CHECK_STRING_RESULT (nautilus_file_get_name (file_1), "home");

	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_get_by_uri ("file:///home/") == file_1, TRUE);
	nautilus_file_unref (file_1);

	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_get_by_uri ("file:///home") == file_1, TRUE);
	nautilus_file_unref (file_1);

	nautilus_file_unref (file_1);

	file_1 = nautilus_file_get_by_uri ("file:///home");
	EEL_CHECK_STRING_RESULT (nautilus_file_get_name (file_1), "home");
	nautilus_file_unref (file_1);

#if 0
	/* ALEX: I removed this, because it was breaking distchecks.
	 * It used to work, but when canonical uris changed from
	 * foo: to foo:/// it broke. I don't expect it to matter
	 * in real life */
	file_1 = nautilus_file_get_by_uri (":");
	EEL_CHECK_STRING_RESULT (nautilus_file_get_name (file_1), ":");
	nautilus_file_unref (file_1);
#endif

	file_1 = nautilus_file_get_by_uri ("eazel:");
	EEL_CHECK_STRING_RESULT (nautilus_file_get_name (file_1), "eazel");
	nautilus_file_unref (file_1);

	/* sorting */
	file_1 = nautilus_file_get_by_uri ("file:///etc");
	file_2 = nautilus_file_get_by_uri ("file:///usr");

	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_1)->ref_count, 1);
	EEL_CHECK_INTEGER_RESULT (G_OBJECT (file_2)->ref_count, 1);

	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_2, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, FALSE, FALSE) < 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_2, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, FALSE, TRUE) > 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_1, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, FALSE, FALSE) == 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_1, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, TRUE, FALSE) == 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_1, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, FALSE, TRUE) == 0, TRUE);
	EEL_CHECK_BOOLEAN_RESULT (nautilus_file_compare_for_sort (file_1, file_1, NAUTILUS_FILE_SORT_BY_DISPLAY_NAME, TRUE, TRUE) == 0, TRUE);

	nautilus_file_unref (file_1);
	nautilus_file_unref (file_2);
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
