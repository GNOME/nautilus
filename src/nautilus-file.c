/*
 *  nautilus-file.c: Nautilus file model.
 *
 *  Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public
 *  License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Author: Darin Adler <darin@bentspoon.com>
 */

#include "nautilus-file.h"

#ifndef NAUTILUS_COMPILATION
#define NAUTILUS_COMPILATION
#endif
#include <libnautilus-extension/nautilus-extension-private.h>

#include <eel/eel-vfs-extensions.h>
#include <gdesktop-enums.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gnome-autoar/gnome-autoar.h>
#include <grp.h>
#include <gtk/gtk.h>
#include <libxml/parser.h>
#include <sys/types.h>
#include <limits.h>
#include <locale.h>
#include <pwd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define DEBUG_FLAG NAUTILUS_DEBUG_FILE
#include "nautilus-debug.h"

#include "nautilus-directory-notify.h"
#include "nautilus-directory-private.h"
#include "nautilus-enums.h"
#include "nautilus-file-operations.h"
#include "nautilus-file-private.h"
#include "nautilus-file-undo-manager.h"
#include "nautilus-file-undo-operations.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-icon-info.h"
#include "nautilus-lib-self-check-functions.h"
#include "nautilus-metadata.h"
#include "nautilus-module.h"
#include "nautilus-scheme.h"
#include "nautilus-signaller.h"
#include "nautilus-tag-manager.h"
#include "nautilus-thumbnails.h"
#include "nautilus-ui-utilities.h"
#include "nautilus-vfs-file.h"
#include "nautilus-video-mime-types.h"

#ifdef HAVE_SELINUX
#include <selinux/selinux.h>
#endif

/* Time in seconds to cache getpwuid results */
#define GETPWUID_CACHE_TIME (5 * 60)

#define ICON_NAME_THUMBNAIL_LOADING   "image-loading"

#undef NAUTILUS_FILE_DEBUG_REF
#undef NAUTILUS_FILE_DEBUG_REF_VALGRIND

#ifdef NAUTILUS_FILE_DEBUG_REF_VALGRIND
#include <valgrind/valgrind.h>
#define DEBUG_REF_PRINTF VALGRIND_PRINTF_BACKTRACE
#else
#define DEBUG_REF_PRINTF printf
#endif

#define MEGA_TO_BASE_RATE 1000000

/* Files that start with these characters sort after files that don't. */
#define SORT_LAST_CHAR1 '.'
#define SORT_LAST_CHAR2 '#'

/* Name of Nautilus trash directories */
#define TRASH_DIRECTORY_NAME ".Trash"

#define METADATA_ID_IS_LIST_MASK (1U << 31)

typedef enum
{
    SHOW_HIDDEN = 1 << 0,
} FilterOptions;

typedef enum
{
    NAUTILUS_DATE_FORMAT_REGULAR = 0,
    NAUTILUS_DATE_FORMAT_REGULAR_WITH_TIME = 1,
    NAUTILUS_DATE_FORMAT_FULL = 2,
} NautilusDateFormat;

typedef void (*ModifyListFunction) (GList       **list,
                                    NautilusFile *file);

enum
{
    CHANGED,
    UPDATED_DEEP_COUNT_IN_PROGRESS,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

static GHashTable *symbolic_links;

static guint64 cached_thumbnail_limit;
static NautilusSpeedTradeoffValue show_file_thumbs;
static gboolean use_24_hour;

static NautilusSpeedTradeoffValue show_directory_item_count;

static GQuark attribute_name_q,
              attribute_size_q,
              attribute_type_q,
              attribute_detailed_type_q,
              attribute_modification_date_q,
              attribute_date_modified_q,
              attribute_date_modified_full_q,
              attribute_date_modified_with_time_q,
              attribute_accessed_date_q,
              attribute_date_accessed_q,
              attribute_date_accessed_full_q,
              attribute_date_created_q,
              attribute_date_created_full_q,
              attribute_mime_type_q,
              attribute_size_detail_q,
              attribute_deep_size_q,
              attribute_deep_file_count_q,
              attribute_deep_directory_count_q,
              attribute_deep_total_count_q,
              attribute_search_relevance_q,
              attribute_trashed_on_q,
              attribute_trashed_on_full_q,
              attribute_trash_orig_path_q,
              attribute_recency_q,
              attribute_permissions_q,
              attribute_selinux_context_q,
              attribute_octal_permissions_q,
              attribute_owner_q,
              attribute_group_q,
              attribute_uri_q,
              attribute_where_q,
              attribute_link_target_q,
              attribute_volume_q,
              attribute_free_space_q,
              attribute_starred_q;

static void     nautilus_file_info_iface_init (NautilusFileInfoInterface *iface);
static char *nautilus_file_get_owner_as_string (NautilusFile *file,
                                                gboolean      include_real_name);
static char *nautilus_file_get_type_as_string (NautilusFile *file);
static const char *nautilus_file_get_type_as_string_no_extra_text (NautilusFile *file);
static char *nautilus_file_get_detailed_type_as_string (NautilusFile *file);
static gboolean update_info_and_name (NautilusFile *file,
                                      GFileInfo    *info);
static const char *nautilus_file_peek_display_name (NautilusFile *file);
static const char *nautilus_file_peek_display_name_collation_key (NautilusFile *file);
static void file_mount_unmounted (GMount  *mount,
                                  gpointer data);
static void metadata_hash_free (GHashTable *hash);

G_DEFINE_TYPE_WITH_CODE (NautilusFile, nautilus_file, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_FILE_INFO,
                                                nautilus_file_info_iface_init)
                         G_ADD_PRIVATE (NautilusFile));

static void
nautilus_file_init (NautilusFile *file)
{
    file->details = nautilus_file_get_instance_private (file);

    nautilus_file_clear_info (file);
    nautilus_file_invalidate_extension_info_internal (file);

    file->details->free_space = -1;
}

static GObject *
nautilus_file_constructor (GType                  type,
                           guint                  n_construct_properties,
                           GObjectConstructParam *construct_params)
{
    GObject *object;
    NautilusFile *file;

    object = (*G_OBJECT_CLASS (nautilus_file_parent_class)->constructor)(type,
                                                                         n_construct_properties,
                                                                         construct_params);

    file = NAUTILUS_FILE (object);

    /* Set to default type after full construction */
    if (NAUTILUS_FILE_GET_CLASS (file)->default_file_type != G_FILE_TYPE_UNKNOWN)
    {
        file->details->type = NAUTILUS_FILE_GET_CLASS (file)->default_file_type;
    }

    return object;
}

gboolean
nautilus_file_set_display_name (NautilusFile *file,
                                const char   *display_name,
                                const char   *edit_name,
                                gboolean      custom)
{
    gboolean changed;

    if (custom && display_name == NULL)
    {
        /* We're re-setting a custom display name, invalidate it if
         *  we already set it so that the old one is re-read */
        if (file->details->got_custom_display_name)
        {
            file->details->got_custom_display_name = FALSE;
            nautilus_file_invalidate_attributes (file,
                                                 NAUTILUS_FILE_ATTRIBUTE_INFO);
        }
        return FALSE;
    }

    if (display_name == NULL || *display_name == 0)
    {
        return FALSE;
    }

    if (!custom && file->details->got_custom_display_name)
    {
        return FALSE;
    }

    if (edit_name == NULL)
    {
        edit_name = display_name;
    }

    changed = FALSE;

    if (g_strcmp0 (file->details->display_name, display_name) != 0)
    {
        changed = TRUE;

        g_clear_pointer (&file->details->display_name, g_ref_string_release);

        if (g_strcmp0 (file->details->name, display_name) == 0)
        {
            file->details->display_name = g_ref_string_acquire (file->details->name);
        }
        else
        {
            file->details->display_name = g_ref_string_new (display_name);
        }

        g_free (file->details->display_name_collation_key);
        file->details->display_name_collation_key = g_utf8_collate_key_for_filename (display_name, -1);
    }

    if (g_strcmp0 (file->details->edit_name, edit_name) != 0)
    {
        changed = TRUE;

        g_clear_pointer (&file->details->edit_name, g_ref_string_release);
        if (g_strcmp0 (file->details->display_name, edit_name) == 0)
        {
            file->details->edit_name = g_ref_string_acquire (file->details->display_name);
        }
        else
        {
            file->details->edit_name = g_ref_string_new (edit_name);
        }
    }

    file->details->got_custom_display_name = custom;
    return changed;
}

static void
nautilus_file_clear_display_name (NautilusFile *file)
{
    g_clear_pointer (&file->details->display_name, g_ref_string_release);
    g_free (file->details->display_name_collation_key);
    file->details->display_name_collation_key = NULL;
    g_clear_pointer (&file->details->edit_name, g_ref_string_release);
}

static gboolean
foreach_metadata_free (gpointer key,
                       gpointer value,
                       gpointer user_data)
{
    guint id;

    id = GPOINTER_TO_UINT (key);

    if (id & METADATA_ID_IS_LIST_MASK)
    {
        g_strfreev ((char **) value);
    }
    else
    {
        g_free ((char *) value);
    }
    return TRUE;
}


static void
metadata_hash_free (GHashTable *hash)
{
    g_hash_table_foreach_remove (hash,
                                 foreach_metadata_free,
                                 NULL);
    g_hash_table_destroy (hash);
}

static gboolean
_g_strv_equal (GStrv a,
               GStrv b)
{
    if (g_strv_length (a) != g_strv_length (b))
    {
        return FALSE;
    }

    for (int i = 0; a[i] != NULL; i++)
    {
        if (strcmp (a[i], b[i]) != 0)
        {
            return FALSE;
        }
    }

    return TRUE;
}

static gboolean
metadata_hash_equal (GHashTable *hash1,
                     GHashTable *hash2)
{
    GHashTableIter iter;
    gpointer key1, value1, value2;
    guint id;

    if (hash1 == NULL && hash2 == NULL)
    {
        return TRUE;
    }

    if (hash1 == NULL || hash2 == NULL)
    {
        return FALSE;
    }

    if (g_hash_table_size (hash1) !=
        g_hash_table_size (hash2))
    {
        return FALSE;
    }

    g_hash_table_iter_init (&iter, hash1);
    while (g_hash_table_iter_next (&iter, &key1, &value1))
    {
        value2 = g_hash_table_lookup (hash2, key1);
        if (value2 == NULL)
        {
            return FALSE;
        }
        id = GPOINTER_TO_UINT (key1);
        if (id & METADATA_ID_IS_LIST_MASK)
        {
            if (!_g_strv_equal ((char **) value1, (char **) value2))
            {
                return FALSE;
            }
        }
        else
        {
            if (strcmp ((char *) value1, (char *) value2) != 0)
            {
                return FALSE;
            }
        }
    }

    return TRUE;
}

static void
clear_metadata (NautilusFile *file)
{
    if (file->details->metadata)
    {
        metadata_hash_free (file->details->metadata);
        file->details->metadata = NULL;
    }
}

static GHashTable *
get_metadata_from_info (GFileInfo *info)
{
    GHashTable *metadata;
    char **attrs;
    guint id;
    int i;
    GFileAttributeType type;
    gpointer value;

    attrs = g_file_info_list_attributes (info, "metadata");

    metadata = g_hash_table_new (NULL, NULL);

    for (i = 0; attrs[i] != NULL; i++)
    {
        id = nautilus_metadata_get_id (attrs[i] + strlen ("metadata::"));
        if (id == 0)
        {
            continue;
        }

        if (!g_file_info_get_attribute_data (info, attrs[i],
                                             &type, &value, NULL))
        {
            continue;
        }

        if (type == G_FILE_ATTRIBUTE_TYPE_STRING)
        {
            g_hash_table_insert (metadata, GUINT_TO_POINTER (id),
                                 g_strdup ((char *) value));
        }
        else if (type == G_FILE_ATTRIBUTE_TYPE_STRINGV)
        {
            id |= METADATA_ID_IS_LIST_MASK;
            g_hash_table_insert (metadata, GUINT_TO_POINTER (id),
                                 g_strdupv ((char **) value));
        }
    }

    g_strfreev (attrs);

    return metadata;
}

gboolean
nautilus_file_update_metadata_from_info (NautilusFile *file,
                                         GFileInfo    *info)
{
    gboolean changed = FALSE;

    if (g_file_info_has_namespace (info, "metadata"))
    {
        GHashTable *metadata;

        metadata = get_metadata_from_info (info);
        if (!metadata_hash_equal (metadata,
                                  file->details->metadata))
        {
            changed = TRUE;
            clear_metadata (file);
            file->details->metadata = metadata;
        }
        else
        {
            metadata_hash_free (metadata);
        }
    }
    else if (file->details->metadata)
    {
        changed = TRUE;
        clear_metadata (file);
    }
    return changed;
}

void
nautilus_file_clear_info (NautilusFile *file)
{
    file->details->got_file_info = FALSE;
    if (file->details->get_info_error)
    {
        g_error_free (file->details->get_info_error);
        file->details->get_info_error = NULL;
    }
    /* Reset to default type, which might be other than unknown for
     *  special kinds of files like the desktop or a search directory */
    file->details->type = NAUTILUS_FILE_GET_CLASS (file)->default_file_type;

    if (!file->details->got_custom_display_name)
    {
        nautilus_file_clear_display_name (file);
    }

    if (file->details->activation_uri != NULL)
    {
        g_free (file->details->activation_uri);
        file->details->activation_uri = NULL;
    }

    if (file->details->icon != NULL)
    {
        g_object_unref (file->details->icon);
        file->details->icon = NULL;
    }

    g_free (file->details->thumbnail_path);
    file->details->thumbnail_path = NULL;
    file->details->thumbnailing_failed = FALSE;

    file->details->is_symlink = FALSE;
    file->details->is_hidden = FALSE;
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
    file->details->can_start = FALSE;
    file->details->can_start_degraded = FALSE;
    file->details->can_stop = FALSE;
    file->details->start_stop_type = G_DRIVE_START_STOP_TYPE_UNKNOWN;
    file->details->can_poll_for_media = FALSE;
    file->details->is_media_check_automatic = FALSE;
    file->details->has_permissions = FALSE;
    file->details->permissions = 0;
    file->details->size = -1;
    file->details->sort_order = 0;
    file->details->mtime = 0;
    file->details->atime = 0;
    file->details->btime = 0;
    file->details->trash_time = 0;
    file->details->recency = 0;
    g_free (file->details->symlink_name);
    file->details->symlink_name = NULL;
    g_clear_pointer (&file->details->mime_type, g_ref_string_release);
    g_free (file->details->selinux_context);
    file->details->selinux_context = NULL;
    g_free (file->details->description);
    file->details->description = NULL;
    g_clear_pointer (&file->details->owner, g_ref_string_release);
    g_clear_pointer (&file->details->owner_real, g_ref_string_release);
    g_clear_pointer (&file->details->group, g_ref_string_release);

    g_clear_pointer (&file->details->filesystem_id, g_ref_string_release);

    clear_metadata (file);
}

NautilusDirectory *
nautilus_file_get_directory (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    return file->details->directory;
}

void
nautilus_file_set_directory (NautilusFile      *file,
                             NautilusDirectory *directory)
{
    char *parent_uri;

    g_clear_object (&file->details->directory);
    g_free (file->details->directory_name_collation_key);

    file->details->directory = nautilus_directory_ref (directory);

    parent_uri = nautilus_file_get_parent_uri (file);
    file->details->directory_name_collation_key = g_utf8_collate_key_for_filename (parent_uri, -1);
    g_free (parent_uri);
}

static NautilusFile *
nautilus_file_new_from_filename (NautilusDirectory *directory,
                                 const char        *filename,
                                 gboolean           self_owned)
{
    NautilusFile *file;

    g_assert (NAUTILUS_IS_DIRECTORY (directory));
    g_assert (filename != NULL);
    g_assert (filename[0] != '\0');

    file = nautilus_directory_new_file_from_filename (directory, filename, self_owned);
    file->details->name = g_ref_string_new (filename);

#ifdef NAUTILUS_FILE_DEBUG_REF
    DEBUG_REF_PRINTF ("%10p ref'd", file);
#endif

    return file;
}

static void
modify_link_hash_table (NautilusFile       *file,
                        ModifyListFunction  modify_function)
{
    char *target_uri;
    gboolean found;
    gpointer original_key;
    GList **list_ptr;

    /* Check if there is a symlink name. If none, we are OK. */
    if (file->details->symlink_name == NULL || !nautilus_file_is_symbolic_link (file))
    {
        return;
    }

    /* Create the hash table first time through. */
    if (symbolic_links == NULL)
    {
        symbolic_links = g_hash_table_new (g_str_hash, g_str_equal);
    }

    target_uri = nautilus_file_get_symbolic_link_target_uri (file);

    /* Find the old contents of the hash table. */
    found = g_hash_table_lookup_extended
                (symbolic_links, target_uri,
                &original_key, (gpointer *) &list_ptr);
    if (!found)
    {
        list_ptr = g_new0 (GList *, 1);
        original_key = g_strdup (target_uri);
        g_hash_table_insert (symbolic_links, original_key, list_ptr);
    }
    (*modify_function)(list_ptr, file);
    if (*list_ptr == NULL)
    {
        g_hash_table_remove (symbolic_links, target_uri);
        g_free (list_ptr);
        g_free (original_key);
    }
    g_free (target_uri);
}

static void
symbolic_link_weak_notify (gpointer  data,
                           GObject  *where_the_object_was)
{
    GList **list = data;
    /* This really shouldn't happen, but we're seeing some strange things in
     *  bug #358172 where the symlink hashtable isn't correctly updated. */
    *list = g_list_remove (*list, where_the_object_was);
}

static void
add_to_link_hash_table_list (GList        **list,
                             NautilusFile  *file)
{
    if (g_list_find (*list, file) != NULL)
    {
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
remove_from_link_hash_table_list (GList        **list,
                                  NautilusFile  *file)
{
    if (g_list_find (*list, file) != NULL)
    {
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
                             GFileInfo         *info)
{
    NautilusFile *file;

    g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
    g_return_val_if_fail (info != NULL, NULL);

    file = NAUTILUS_FILE (g_object_new (NAUTILUS_TYPE_VFS_FILE, NULL));
    nautilus_file_set_directory (file, directory);

    update_info_and_name (file, info);

#ifdef NAUTILUS_FILE_DEBUG_REF
    DEBUG_REF_PRINTF ("%10p ref'd", file);
#endif

    return file;
}

static NautilusFileInfo *
nautilus_file_get_internal (GFile    *location,
                            gboolean  create)
{
    gboolean self_owned;
    NautilusDirectory *directory;
    NautilusFile *file;
    GFile *parent;
    char *basename;

    g_assert (location != NULL);

    parent = g_file_get_parent (location);

    self_owned = FALSE;
    if (parent == NULL)
    {
        self_owned = TRUE;
        parent = g_object_ref (location);
    }

    /* Get object that represents the directory. */
    directory = nautilus_directory_get_internal (parent, create);

    g_object_unref (parent);

    /* Get the name for the file. */
    if (self_owned && directory != NULL)
    {
        basename = nautilus_directory_get_name_for_self_as_new_file (directory);
    }
    else
    {
        basename = g_file_get_basename (location);
    }
    /* Check to see if it's a file that's already known. */
    if (directory == NULL)
    {
        file = NULL;
    }
    else if (self_owned)
    {
        file = directory->details->as_file;
    }
    else
    {
        file = nautilus_directory_find_file_by_name (directory, basename);
    }

    /* Ref or create the file. */
    if (file != NULL)
    {
        nautilus_file_ref (file);
    }
    else if (create)
    {
        file = nautilus_file_new_from_filename (directory, basename, self_owned);
        if (self_owned)
        {
            g_assert (directory->details->as_file == NULL);
            directory->details->as_file = file;
        }
        else
        {
            nautilus_directory_add_file (directory, file);
        }
    }

    g_free (basename);
    nautilus_directory_unref (directory);

    return NAUTILUS_FILE_INFO (file);
}

NautilusFile *
nautilus_file_get (GFile *location)
{
    g_return_val_if_fail (G_IS_FILE (location), NULL);

    return NAUTILUS_FILE (nautilus_file_get_internal (location, TRUE));
}

NautilusFile *
nautilus_file_get_existing (GFile *location)
{
    g_return_val_if_fail (G_IS_FILE (location), NULL);

    return NAUTILUS_FILE (nautilus_file_get_internal (location, FALSE));
}

NautilusFile *
nautilus_file_get_existing_by_uri (const char *uri)
{
    g_autoptr (GFile) location = NULL;

    location = g_file_new_for_uri (uri);

    return nautilus_file_get_existing (location);
}

NautilusFile *
nautilus_file_get_by_uri (const char *uri)
{
    g_autoptr (GFile) location = NULL;

    location = g_file_new_for_uri (uri);

    return nautilus_file_get (location);
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

    if (file->details->is_thumbnailing)
    {
        uri = nautilus_file_get_uri (file);
        nautilus_thumbnail_remove_from_queue (uri);
        g_free (uri);
    }

    nautilus_async_destroying_file (file);

    remove_from_link_hash_table (file);

    directory = file->details->directory;

    if (nautilus_file_is_self_owned (file))
    {
        directory->details->as_file = NULL;
    }
    else
    {
        if (!file->details->is_gone)
        {
            nautilus_directory_remove_file (directory, file);
        }
    }

    if (file->details->get_info_error)
    {
        g_error_free (file->details->get_info_error);
    }

    nautilus_directory_unref (directory);
    g_clear_pointer (&file->details->name, g_ref_string_release);
    g_clear_pointer (&file->details->display_name, g_ref_string_release);
    g_free (file->details->display_name_collation_key);
    g_free (file->details->directory_name_collation_key);
    g_clear_pointer (&file->details->edit_name, g_ref_string_release);
    if (file->details->icon)
    {
        g_object_unref (file->details->icon);
    }
    g_free (file->details->thumbnail_path);
    g_free (file->details->symlink_name);
    g_clear_pointer (&file->details->mime_type, g_ref_string_release);
    g_clear_pointer (&file->details->owner, g_ref_string_release);
    g_clear_pointer (&file->details->owner_real, g_ref_string_release);
    g_clear_pointer (&file->details->group, g_ref_string_release);
    g_free (file->details->selinux_context);
    g_free (file->details->description);
    g_free (file->details->activation_uri);
    g_clear_object (&file->details->custom_icon);

    if (file->details->thumbnail)
    {
        g_object_unref (file->details->thumbnail);
    }

    g_clear_object (&file->details->mount);

    g_clear_pointer (&file->details->filesystem_id, g_ref_string_release);
    g_clear_pointer (&file->details->filesystem_type, g_ref_string_release);
    g_free (file->details->trash_orig_path);

    g_list_free_full (file->details->mime_list, g_free);
    g_list_free_full (file->details->pending_extension_emblems, g_free);
    g_list_free_full (file->details->extension_emblems, g_free);
    g_list_free_full (file->details->pending_info_providers, g_object_unref);

    if (file->details->pending_extension_attributes)
    {
        g_hash_table_destroy (file->details->pending_extension_attributes);
    }

    if (file->details->extension_attributes)
    {
        g_hash_table_destroy (file->details->extension_attributes);
    }

    if (file->details->metadata)
    {
        metadata_hash_free (file->details->metadata);
    }

    g_free (file->details->fts_snippet);

    G_OBJECT_CLASS (nautilus_file_parent_class)->finalize (object);
}

NautilusFile *
nautilus_file_ref (NautilusFile *file)
{
    if (file == NULL)
    {
        return NULL;
    }
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

#ifdef NAUTILUS_FILE_DEBUG_REF
    DEBUG_REF_PRINTF ("%10p ref'd", file);
#endif

    return g_object_ref (file);
}

void
nautilus_file_unref (NautilusFile *file)
{
    if (file == NULL)
    {
        return;
    }

    g_return_if_fail (NAUTILUS_IS_FILE (file));

#ifdef NAUTILUS_FILE_DEBUG_REF
    DEBUG_REF_PRINTF ("%10p unref'd", file);
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
 * formatted for user display (including stripping "file://"
 * and adding trailing slash).
 * If the parent is NULL, returns the empty string.
 */
char *
nautilus_file_get_parent_uri_for_display (NautilusFile *file)
{
    g_autoptr (GFile) parent = NULL;
    char *result;

    g_assert (NAUTILUS_IS_FILE (file));

    parent = nautilus_file_get_parent_location (file);
    if (parent)
    {
        g_autofree gchar *parse_name = g_file_get_parse_name (parent);

        /* Ensure a trailing slash to emphasize it is a directory */
        if (g_str_has_suffix (parse_name, G_DIR_SEPARATOR_S))
        {
            result = g_steal_pointer (&parse_name);
        }
        else
        {
            result = g_strconcat (parse_name, G_DIR_SEPARATOR_S, NULL);
        }
    }
    else
    {
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
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    return nautilus_file_info_get_parent_uri (NAUTILUS_FILE_INFO (file));
}

GFile *
nautilus_file_get_parent_location (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    return nautilus_file_info_get_parent_location (NAUTILUS_FILE_INFO (file));
}

NautilusFile *
nautilus_file_get_parent (NautilusFile *file)
{
    NautilusFileInfo *file_info;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    file_info = NAUTILUS_FILE_INFO (file);

    return NAUTILUS_FILE (nautilus_file_info_get_parent_info (file_info));
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

    return nautilus_file_info_can_write (NAUTILUS_FILE_INFO (file));
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

gboolean
nautilus_file_can_start (NautilusFile *file)
{
    gboolean ret;
    GDrive *drive;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    ret = FALSE;

    if (file->details->can_start)
    {
        ret = TRUE;
        goto out;
    }

    if (file->details->mount != NULL)
    {
        drive = g_mount_get_drive (file->details->mount);
        if (drive != NULL)
        {
            ret = g_drive_can_start (drive);
            g_object_unref (drive);
        }
    }

out:
    return ret;
}

gboolean
nautilus_file_can_start_degraded (NautilusFile *file)
{
    gboolean ret;
    GDrive *drive;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    ret = FALSE;

    if (file->details->can_start_degraded)
    {
        ret = TRUE;
        goto out;
    }

    if (file->details->mount != NULL)
    {
        drive = g_mount_get_drive (file->details->mount);
        if (drive != NULL)
        {
            ret = g_drive_can_start_degraded (drive);
            g_object_unref (drive);
        }
    }

out:
    return ret;
}

gboolean
nautilus_file_can_poll_for_media (NautilusFile *file)
{
    gboolean ret;
    GDrive *drive;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    ret = FALSE;

    if (file->details->can_poll_for_media)
    {
        ret = TRUE;
        goto out;
    }

    if (file->details->mount != NULL)
    {
        drive = g_mount_get_drive (file->details->mount);
        if (drive != NULL)
        {
            ret = g_drive_can_poll_for_media (drive);
            g_object_unref (drive);
        }
    }

out:
    return ret;
}

gboolean
nautilus_file_is_media_check_automatic (NautilusFile *file)
{
    gboolean ret;
    GDrive *drive;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    ret = FALSE;

    if (file->details->is_media_check_automatic)
    {
        ret = TRUE;
        goto out;
    }

    if (file->details->mount != NULL)
    {
        drive = g_mount_get_drive (file->details->mount);
        if (drive != NULL)
        {
            ret = g_drive_is_media_check_automatic (drive);
            g_object_unref (drive);
        }
    }

out:
    return ret;
}


gboolean
nautilus_file_can_stop (NautilusFile *file)
{
    gboolean ret;
    GDrive *drive;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    ret = FALSE;

    if (file->details->can_stop)
    {
        ret = TRUE;
        goto out;
    }

    if (file->details->mount != NULL)
    {
        drive = g_mount_get_drive (file->details->mount);
        if (drive != NULL)
        {
            ret = g_drive_can_stop (drive);
            g_object_unref (drive);
        }
    }

out:
    return ret;
}

GDriveStartStopType
nautilus_file_get_start_stop_type (NautilusFile *file)
{
    GDriveStartStopType ret;
    GDrive *drive;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    ret = file->details->start_stop_type;
    if (ret != G_DRIVE_START_STOP_TYPE_UNKNOWN)
    {
        goto out;
    }

    if (file->details->mount != NULL)
    {
        drive = g_mount_get_drive (file->details->mount);
        if (drive != NULL)
        {
            ret = g_drive_get_start_stop_type (drive);
            g_object_unref (drive);
        }
    }

out:
    return ret;
}

void
nautilus_file_mount (NautilusFile                  *file,
                     GMountOperation               *mount_op,
                     GCancellable                  *cancellable,
                     NautilusFileOperationCallback  callback,
                     gpointer                       callback_data)
{
    GError *error;

    if (NAUTILUS_FILE_GET_CLASS (file)->mount == NULL)
    {
        if (callback)
        {
            error = NULL;
            g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                 _("This file cannot be mounted"));
            callback (file, NULL, error, callback_data);
            g_error_free (error);
        }
    }
    else
    {
        NAUTILUS_FILE_GET_CLASS (file)->mount (file, mount_op, cancellable, callback, callback_data);
    }
}

typedef struct
{
    NautilusFile *file;
    NautilusFileOperationCallback callback;
    gpointer callback_data;
} UnmountData;

static void
unmount_done (void *callback_data)
{
    UnmountData *data;

    data = (UnmountData *) callback_data;
    if (data->callback)
    {
        data->callback (data->file, NULL, NULL, data->callback_data);
    }
    nautilus_file_unref (data->file);
    g_free (data);
}

void
nautilus_file_unmount (NautilusFile                  *file,
                       GMountOperation               *mount_op,
                       GCancellable                  *cancellable,
                       NautilusFileOperationCallback  callback,
                       gpointer                       callback_data)
{
    GError *error;
    UnmountData *data;

    if (file->details->can_unmount)
    {
        if (NAUTILUS_FILE_GET_CLASS (file)->unmount != NULL)
        {
            NAUTILUS_FILE_GET_CLASS (file)->unmount (file, mount_op, cancellable, callback, callback_data);
        }
        else
        {
            if (callback)
            {
                error = NULL;
                g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     _("This file cannot be unmounted"));
                callback (file, NULL, error, callback_data);
                g_error_free (error);
            }
        }
    }
    else if (file->details->mount != NULL &&
             g_mount_can_unmount (file->details->mount))
    {
        GtkWindow *parent;

        parent = gtk_mount_operation_get_parent (GTK_MOUNT_OPERATION (mount_op));

        data = g_new0 (UnmountData, 1);
        data->file = nautilus_file_ref (file);
        data->callback = callback;
        data->callback_data = callback_data;
        nautilus_file_operations_unmount_mount_full (parent, file->details->mount, mount_op, FALSE, TRUE, unmount_done, data);
    }
    else if (callback)
    {
        callback (file, NULL, NULL, callback_data);
    }
}

void
nautilus_file_eject (NautilusFile                  *file,
                     GMountOperation               *mount_op,
                     GCancellable                  *cancellable,
                     NautilusFileOperationCallback  callback,
                     gpointer                       callback_data)
{
    GError *error;
    UnmountData *data;

    if (file->details->can_eject)
    {
        if (NAUTILUS_FILE_GET_CLASS (file)->eject != NULL)
        {
            NAUTILUS_FILE_GET_CLASS (file)->eject (file, mount_op, cancellable, callback, callback_data);
        }
        else
        {
            if (callback)
            {
                error = NULL;
                g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     _("This file cannot be ejected"));
                callback (file, NULL, error, callback_data);
                g_error_free (error);
            }
        }
    }
    else if (file->details->mount != NULL &&
             g_mount_can_eject (file->details->mount))
    {
        GtkWindow *parent;

        parent = gtk_mount_operation_get_parent (GTK_MOUNT_OPERATION (mount_op));

        data = g_new0 (UnmountData, 1);
        data->file = nautilus_file_ref (file);
        data->callback = callback;
        data->callback_data = callback_data;
        nautilus_file_operations_unmount_mount_full (parent, file->details->mount, mount_op, TRUE, TRUE, unmount_done, data);
    }
    else if (callback)
    {
        callback (file, NULL, NULL, callback_data);
    }
}

void
nautilus_file_start (NautilusFile                  *file,
                     GMountOperation               *start_op,
                     GCancellable                  *cancellable,
                     NautilusFileOperationCallback  callback,
                     gpointer                       callback_data)
{
    GError *error;

    if ((file->details->can_start || file->details->can_start_degraded) &&
        NAUTILUS_FILE_GET_CLASS (file)->start != NULL)
    {
        NAUTILUS_FILE_GET_CLASS (file)->start (file, start_op, cancellable, callback, callback_data);
    }
    else
    {
        if (callback)
        {
            error = NULL;
            g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                 _("This file cannot be started"));
            callback (file, NULL, error, callback_data);
            g_error_free (error);
        }
    }
}

static void
file_stop_callback (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      callback_data)
{
    NautilusFileOperation *op;
    gboolean stopped;
    GError *error;

    op = callback_data;

    error = NULL;
    stopped = g_drive_stop_finish (G_DRIVE (source_object),
                                   res, &error);

    if (!stopped &&
        error->domain == G_IO_ERROR &&
        (error->code == G_IO_ERROR_FAILED_HANDLED ||
         error->code == G_IO_ERROR_CANCELLED))
    {
        g_error_free (error);
        error = NULL;
    }

    nautilus_file_operation_complete (op, NULL, error);
    if (error)
    {
        g_error_free (error);
    }
}

void
nautilus_file_stop (NautilusFile                  *file,
                    GMountOperation               *mount_op,
                    GCancellable                  *cancellable,
                    NautilusFileOperationCallback  callback,
                    gpointer                       callback_data)
{
    GError *error;

    if (NAUTILUS_FILE_GET_CLASS (file)->stop != NULL)
    {
        if (file->details->can_stop)
        {
            NAUTILUS_FILE_GET_CLASS (file)->stop (file, mount_op, cancellable, callback, callback_data);
        }
        else
        {
            if (callback)
            {
                error = NULL;
                g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     _("This file cannot be stopped"));
                callback (file, NULL, error, callback_data);
                g_error_free (error);
            }
        }
    }
    else
    {
        GDrive *drive;

        drive = NULL;
        if (file->details->mount != NULL)
        {
            drive = g_mount_get_drive (file->details->mount);
        }

        if (drive != NULL && g_drive_can_stop (drive))
        {
            NautilusFileOperation *op;

            op = nautilus_file_operation_new (file, callback, callback_data);
            if (cancellable)
            {
                g_object_unref (op->cancellable);
                op->cancellable = g_object_ref (cancellable);
            }

            g_drive_stop (drive,
                          G_MOUNT_UNMOUNT_NONE,
                          mount_op,
                          op->cancellable,
                          file_stop_callback,
                          op);
        }
        else
        {
            if (callback)
            {
                error = NULL;
                g_set_error_literal (&error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                                     _("This file cannot be stopped"));
                callback (file, NULL, error, callback_data);
                g_error_free (error);
            }
        }

        if (drive != NULL)
        {
            g_object_unref (drive);
        }
    }
}

void
nautilus_file_poll_for_media (NautilusFile *file)
{
    if (file->details->can_poll_for_media)
    {
        if (NAUTILUS_FILE_GET_CLASS (file)->stop != NULL)
        {
            NAUTILUS_FILE_GET_CLASS (file)->poll_for_media (file);
        }
    }
    else if (file->details->mount != NULL)
    {
        GDrive *drive;
        drive = g_mount_get_drive (file->details->mount);
        if (drive != NULL)
        {
            g_drive_poll_for_media (drive,
                                    NULL,              /* cancellable */
                                    NULL,              /* GAsyncReadyCallback */
                                    NULL);             /* user_data */
            g_object_unref (drive);
        }
    }
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
    if (nautilus_file_is_gone (file))
    {
        return FALSE;
    }

    /* Self-owned files can't be renamed */
    if (nautilus_file_is_self_owned (file))
    {
        return FALSE;
    }

    if (nautilus_file_is_home (file))
    {
        return FALSE;
    }

    can_rename = TRUE;

    if (!can_rename)
    {
        return FALSE;
    }

    return file->details->can_rename;
}

gboolean
nautilus_file_can_delete (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    /* Nonexistent files can't be deleted. */
    if (nautilus_file_is_gone (file))
    {
        return FALSE;
    }

    /* Self-owned files can't be deleted */
    if (nautilus_file_is_self_owned (file))
    {
        return FALSE;
    }

    return file->details->can_delete;
}

gboolean
nautilus_file_can_trash (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    /* Nonexistent files can't be deleted. */
    if (nautilus_file_is_gone (file))
    {
        return FALSE;
    }

    /* Self-owned files can't be deleted */
    if (nautilus_file_is_self_owned (file))
    {
        return FALSE;
    }

    return file->details->can_trash;
}

GFile *
nautilus_file_get_location (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    return nautilus_file_info_get_location (NAUTILUS_FILE_INFO (file));
}

/* Return the actual uri associated with the passed-in file. */
char *
nautilus_file_get_uri (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    return nautilus_file_info_get_uri (NAUTILUS_FILE_INFO (file));
}

char *
nautilus_file_get_uri_scheme (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    return nautilus_file_info_get_uri_scheme (NAUTILUS_FILE_INFO (file));
}


gboolean
nautilus_file_opens_in_view (NautilusFile *file)
{
    return nautilus_file_is_directory (file);
}

NautilusFileOperation *
nautilus_file_operation_new (NautilusFile                  *file,
                             NautilusFileOperationCallback  callback,
                             gpointer                       callback_data)
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
    GList *l;
    NautilusFile *file;

    op->file->details->operations_in_progress = g_list_remove
                                                    (op->file->details->operations_in_progress, op);


    for (l = op->files; l != NULL; l = l->next)
    {
        file = NAUTILUS_FILE (l->data);
        file->details->operations_in_progress = g_list_remove
                                                    (file->details->operations_in_progress, op);
    }
}

void
nautilus_file_operation_free (NautilusFileOperation *op)
{
    nautilus_file_operation_remove (op);

    if (op->files == NULL)
    {
        nautilus_file_unref (op->file);
    }
    else
    {
        nautilus_file_list_free (op->files);
    }

    g_object_unref (op->cancellable);
    if (op->free_data)
    {
        op->free_data (op->data);
    }

    if (op->undo_info != NULL)
    {
        nautilus_file_undo_manager_set_action (op->undo_info);
        g_object_unref (op->undo_info);
    }

    g_free (op);
}

void
nautilus_file_operation_complete (NautilusFileOperation *op,
                                  GFile                 *result_file,
                                  GError                *error)
{
    /* Claim that something changed even if the operation failed.
     * This makes it easier for some clients who see the "reverting"
     * as "changing back".
     */
    nautilus_file_operation_remove (op);

    if (op->files == NULL)
    {
        nautilus_file_changed (op->file);
    }

    if (op->callback)
    {
        (*op->callback)(op->file, result_file, error, op->callback_data);
    }

    if (error != NULL)
    {
        g_clear_object (&op->undo_info);
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
rename_get_info_callback (GObject      *source_object,
                          GAsyncResult *res,
                          gpointer      callback_data)
{
    NautilusFileOperation *op;
    NautilusDirectory *directory;
    NautilusFile *existing_file;
    char *old_uri;
    char *new_uri;
    const char *new_name;
    GFileInfo *new_info;
    GError *error;

    op = callback_data;

    error = NULL;
    new_info = g_file_query_info_finish (G_FILE (source_object), res, &error);
    if (new_info != NULL)
    {
        g_autoptr (GFile) old_location = NULL;
        g_autoptr (GFile) new_location = NULL;

        directory = op->file->details->directory;

        new_name = g_file_info_get_name (new_info);

        /* If there was another file by the same name in this
         * directory and it is not the same file that we are
         * renaming, mark it gone.
         */
        existing_file = nautilus_directory_find_file_by_name (directory, new_name);
        if (existing_file != NULL && existing_file != op->file)
        {
            nautilus_file_mark_gone (existing_file);
            nautilus_file_changed (existing_file);
        }

        old_location = nautilus_file_get_location (op->file);
        old_uri = g_file_get_uri (old_location);

        update_info_and_name (op->file, new_info);

        new_location = nautilus_file_get_location (op->file);
        new_uri = g_file_get_uri (new_location);

        nautilus_directory_moved (old_uri, new_uri);
        nautilus_tag_manager_update_moved_uris (nautilus_tag_manager_get (),
                                                old_location,
                                                new_location);

        g_free (new_uri);
        g_free (old_uri);

        g_object_unref (new_info);
    }
    nautilus_file_operation_complete (op, NULL, error);
    if (error)
    {
        g_error_free (error);
    }
}

static void
rename_callback (GObject      *source_object,
                 GAsyncResult *res,
                 gpointer      callback_data)
{
    NautilusFileOperation *op;
    GFile *new_file;
    GError *error;

    op = callback_data;

    error = NULL;
    new_file = g_file_set_display_name_finish (G_FILE (source_object),
                                               res, &error);

    if (new_file != NULL)
    {
        if (op->undo_info != NULL)
        {
            nautilus_file_undo_info_rename_set_data_post (NAUTILUS_FILE_UNDO_INFO_RENAME (op->undo_info),
                                                          new_file);
        }
        g_file_query_info_async (new_file,
                                 NAUTILUS_FILE_DEFAULT_ATTRIBUTES,
                                 0,
                                 G_PRIORITY_DEFAULT,
                                 op->cancellable,
                                 rename_get_info_callback, op);
    }
    else
    {
        nautilus_file_operation_complete (op, NULL, error);
        g_error_free (error);
    }
}

static gboolean
name_is (NautilusFile *file,
         const char   *new_name)
{
    const char *old_name;
    old_name = file->details->name;
    return strcmp (new_name, old_name) == 0;
}

static const char *
nautilus_file_can_rename_file (NautilusFile                  *file,
                               const char                    *new_name,
                               NautilusFileOperationCallback  callback,
                               gpointer                       callback_data)
{
    GError *error;

    /* Return an error for incoming names containing path separators.
     * But not for .desktop files as '/' are allowed for them */
    if (strstr (new_name, "/") != NULL)
    {
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                             _("Slashes are not allowed in filenames"));
        if (callback != NULL)
        {
            (*callback)(file, NULL, error, callback_data);
        }
        g_error_free (error);
        return NULL;
    }

    /* Can't rename a file that's already gone.
     * We need to check this here because there may be a new
     * file with the same name.
     */
    if (nautilus_file_rename_handle_file_gone (file, callback, callback_data))
    {
        return NULL;
    }

    /* Test the name-hasn't-changed case explicitly, for two reasons.
     * (1) rename returns an error if new & old are same.
     * (2) We don't want to send file-changed signal if nothing changed.
     */
    if (name_is (file, new_name))
    {
        if (callback != NULL)
        {
            (*callback)(file, NULL, NULL, callback_data);
        }
        return NULL;
    }

    /* Self-owned files can't be renamed. Test the name-not-actually-changing
     * case before this case.
     */
    if (nautilus_file_is_self_owned (file))
    {
        /* Claim that something changed even if the rename
         * failed. This makes it easier for some clients who
         * see the "reverting" to the old name as "changing
         * back".
         */
        nautilus_file_changed (file);
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                             _("Toplevel files cannot be renamed"));

        if (callback != NULL)
        {
            (*callback)(file, NULL, error, callback_data);
        }
        g_error_free (error);

        return NULL;
    }

    return new_name;
}

void
nautilus_file_rename (NautilusFile                  *file,
                      const char                    *new_name,
                      NautilusFileOperationCallback  callback,
                      gpointer                       callback_data)
{
    NautilusFileOperation *op;
    const char *new_file_name;
    GFile *location;

    g_return_if_fail (NAUTILUS_IS_FILE (file));
    g_return_if_fail (new_name != NULL);
    g_return_if_fail (callback != NULL);

    new_file_name = nautilus_file_can_rename_file (file,
                                                   new_name,
                                                   callback,
                                                   callback_data);

    if (new_file_name == NULL)
    {
        return;
    }

    /* Set up a renaming operation. */
    op = nautilus_file_operation_new (file, callback, callback_data);
    op->is_rename = TRUE;
    location = nautilus_file_get_location (file);

    /* Tell the undo manager a rename is taking place */
    if (!nautilus_file_undo_manager_is_operating ())
    {
        const char *old_name;
        op->undo_info = nautilus_file_undo_info_rename_new ();

        old_name = nautilus_file_get_display_name (file);
        nautilus_file_undo_info_rename_set_data_pre (NAUTILUS_FILE_UNDO_INFO_RENAME (op->undo_info),
                                                     location, old_name, new_file_name);
    }

    /* Do the renaming. */
    g_file_set_display_name_async (location,
                                   new_file_name,
                                   G_PRIORITY_DEFAULT,
                                   op->cancellable,
                                   rename_callback,
                                   op);
    g_object_unref (location);
}

gboolean
nautilus_file_rename_handle_file_gone (NautilusFile                  *file,
                                       NautilusFileOperationCallback  callback,
                                       gpointer                       callback_data)
{
    GError *error;

    if (nautilus_file_is_gone (file))
    {
        /* Claim that something changed even if the rename
         * failed. This makes it easier for some clients who
         * see the "reverting" to the old name as "changing
         * back".
         */
        nautilus_file_changed (file);
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                             _("File not found"));
        if (callback)
        {
            (*callback)(file, NULL, error, callback_data);
        }
        g_error_free (error);
        return TRUE;
    }

    return FALSE;
}

typedef struct
{
    NautilusFileOperation *op;
    NautilusFile *file;
} BatchRenameData;

static void
batch_rename_get_info_callback (GObject      *source_object,
                                GAsyncResult *res,
                                gpointer      callback_data)
{
    NautilusFileOperation *op;
    NautilusDirectory *directory;
    NautilusFile *existing_file;
    char *old_uri;
    char *new_uri;
    const char *new_name;
    GFileInfo *new_info;
    GError *error;
    BatchRenameData *data;

    data = callback_data;

    op = data->op;
    op->file = data->file;

    error = NULL;
    new_info = g_file_query_info_finish (G_FILE (source_object), res, &error);
    if (new_info != NULL)
    {
        old_uri = nautilus_file_get_uri (op->file);

        new_name = g_file_info_get_name (new_info);

        directory = op->file->details->directory;

        /* If there was another file by the same name in this
         * directory and it is not the same file that we are
         * renaming, mark it gone.
         */
        existing_file = nautilus_directory_find_file_by_name (directory, new_name);
        if (existing_file != NULL && existing_file != op->file)
        {
            nautilus_file_mark_gone (existing_file);
            nautilus_file_changed (existing_file);
        }

        update_info_and_name (op->file, new_info);

        new_uri = nautilus_file_get_uri (op->file);
        nautilus_directory_moved (old_uri, new_uri);
        g_free (new_uri);
        g_free (old_uri);
        g_object_unref (new_info);
    }

    op->renamed_files++;

    if (op->files == NULL ||
        op->renamed_files + op->skipped_files == g_list_length (op->files))
    {
        nautilus_file_operation_complete (op, NULL, error);
    }

    g_free (data);

    if (error)
    {
        g_error_free (error);
    }
}

static void
real_batch_rename (GList                         *files,
                   GList                         *new_names,
                   NautilusFileOperationCallback  callback,
                   gpointer                       callback_data)
{
    GList *l1, *l2, *old_files, *new_files;
    NautilusFileOperation *op;
    GFile *location;
    GString *new_name;
    NautilusFile *file;
    GError *error;
    GFile *new_file;
    BatchRenameData *data;

    error = NULL;
    old_files = NULL;
    new_files = NULL;

    /* Set up a batch renaming operation. */
    op = nautilus_file_operation_new (files->data, callback, callback_data);
    op->files = nautilus_file_list_copy (files);
    op->renamed_files = 0;
    op->skipped_files = 0;

    for (l1 = files->next; l1 != NULL; l1 = l1->next)
    {
        file = NAUTILUS_FILE (l1->data);

        file->details->operations_in_progress = g_list_prepend (file->details->operations_in_progress,
                                                                op);
    }

    for (l1 = files, l2 = new_names; l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next)
    {
        const char *new_file_name;
        file = NAUTILUS_FILE (l1->data);
        new_name = l2->data;

        location = nautilus_file_get_location (file);

        new_file_name = nautilus_file_can_rename_file (file,
                                                       new_name->str,
                                                       callback,
                                                       callback_data);

        if (new_file_name == NULL)
        {
            op->skipped_files++;

            continue;
        }

        g_assert (G_IS_FILE (location));

        /* Do the renaming. */
        new_file = g_file_set_display_name (location,
                                            new_file_name,
                                            op->cancellable,
                                            &error);

        data = g_new0 (BatchRenameData, 1);
        data->op = op;
        data->file = file;

        g_file_query_info_async (new_file,
                                 NAUTILUS_FILE_DEFAULT_ATTRIBUTES,
                                 0,
                                 G_PRIORITY_DEFAULT,
                                 op->cancellable,
                                 batch_rename_get_info_callback,
                                 data);

        if (error != NULL)
        {
            g_warning ("Batch rename for file \"%s\" failed", nautilus_file_get_name (file));
            g_error_free (error);
            error = NULL;

            op->skipped_files++;
        }
        else
        {
            old_files = g_list_append (old_files, location);
            new_files = g_list_append (new_files, new_file);
        }
    }

    /* Tell the undo manager a batch rename is taking place if at least
     * a file has been renamed*/
    if (!nautilus_file_undo_manager_is_operating () && op->skipped_files != g_list_length (files))
    {
        op->undo_info = nautilus_file_undo_info_batch_rename_new (g_list_length (new_files));

        nautilus_file_undo_info_batch_rename_set_data_pre (NAUTILUS_FILE_UNDO_INFO_BATCH_RENAME (op->undo_info),
                                                           old_files);

        nautilus_file_undo_info_batch_rename_set_data_post (NAUTILUS_FILE_UNDO_INFO_BATCH_RENAME (op->undo_info),
                                                            new_files);

        nautilus_file_undo_manager_set_action (op->undo_info);
    }

    if (op->skipped_files == g_list_length (files))
    {
        nautilus_file_operation_complete (op, NULL, error);
    }
}

void
nautilus_file_batch_rename (GList                         *files,
                            GList                         *new_names,
                            NautilusFileOperationCallback  callback,
                            gpointer                       callback_data)
{
    real_batch_rename (files,
                       new_names,
                       callback,
                       callback_data);
}

gboolean
nautilus_file_rename_in_progress (NautilusFile *file)
{
    GList *node;
    NautilusFileOperation *op;

    for (node = file->details->operations_in_progress; node != NULL; node = node->next)
    {
        op = node->data;
        if (op->is_rename)
        {
            return TRUE;
        }
    }
    return FALSE;
}

void
nautilus_file_cancel (NautilusFile                  *file,
                      NautilusFileOperationCallback  callback,
                      gpointer                       callback_data)
{
    GList *node, *next;
    NautilusFileOperation *op;

    for (node = file->details->operations_in_progress; node != NULL; node = next)
    {
        next = node->next;
        op = node->data;

        g_assert (op->file == file);
        if (op->callback == callback && op->callback_data == callback_data)
        {
            nautilus_file_operation_cancel (op);
        }
    }
}

gboolean
nautilus_file_matches_uri (NautilusFile *file,
                           const char   *match_uri)
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

int
nautilus_file_compare_location (NautilusFile *file_1,
                                NautilusFile *file_2)
{
    GFile *loc_a, *loc_b;
    gboolean res;

    loc_a = nautilus_file_get_location (file_1);
    loc_b = nautilus_file_get_location (file_2);

    res = !g_file_equal (loc_a, loc_b);

    g_object_unref (loc_a);
    g_object_unref (loc_b);

    return (gint) res;
}

/**
 * nautilus_file_has_local_path:
 *
 * @file: a #NautilusFile
 *
 * Checks whether this file has an obtainable local paths. Usually, this means
 * the local path can be obtained by calling g_file_get_path(); this includes
 * native and FUSE files. As an exception, the local URI for files in recent://
 * can only be obtained from the G_FILE_ATTRIBUTE_STANDARD_TARGET_URI attribute.
 *
 * Returns: %TRUE if a local path is known to be obtainable for this file.
 */
gboolean
nautilus_file_has_local_path (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    return nautilus_directory_is_local_or_fuse (file->details->directory);
}

static void
update_link (NautilusFile *link_file,
             NautilusFile *target_file)
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

    if (symbolic_links == NULL)
    {
        link_files = NULL;
    }
    else
    {
        uri = nautilus_file_get_uri (target_file);
        link_files = g_hash_table_lookup (symbolic_links, uri);
        g_free (uri);
    }
    if (link_files)
    {
        return nautilus_file_list_copy (*link_files);
    }
    return NULL;
}

static void
update_links_if_target (NautilusFile *target_file)
{
    GList *link_files, *p;

    link_files = get_link_files (target_file);
    for (p = link_files; p != NULL; p = p->next)
    {
        update_link (NAUTILUS_FILE (p->data), target_file);
    }
    nautilus_file_list_free (link_files);
}

static gboolean
update_info_internal (NautilusFile *file,
                      GFileInfo    *info,
                      gboolean      update_name)
{
    GList *node;
    gboolean changed;
    gboolean is_symlink, is_hidden, is_mountpoint;
    gboolean has_permissions;
    guint32 permissions;
    gboolean can_read, can_write, can_execute, can_delete, can_trash, can_rename, can_mount, can_unmount, can_eject;
    gboolean can_start, can_start_degraded, can_stop, can_poll_for_media, is_media_check_automatic;
    GDriveStartStopType start_stop_type;
    gboolean thumbnailing_failed;
    int uid, gid;
    goffset size;
    int sort_order;
    time_t atime, mtime, btime;
    time_t trash_time;
    time_t recency;
    const char *time_string;
    const char *symlink_name, *mime_type, *selinux_context, *name, *thumbnail_path;
    GFileType file_type;
    GIcon *icon;
    char *old_activation_uri;
    const char *activation_uri;
    const char *description;
    const char *filesystem_id;
    const char *trash_orig_path;
    const char *group, *owner, *owner_real;
    gboolean free_owner, free_group;
    const char *edit_name;

    if (file->details->is_gone)
    {
        return FALSE;
    }

    if (info == NULL)
    {
        nautilus_file_mark_gone (file);
        return TRUE;
    }

    file->details->file_info_is_up_to_date = TRUE;

    /* FIXME bugzilla.gnome.org 42044: Need to let links that
     * point to the old name know that the file has been renamed.
     */

    remove_from_link_hash_table (file);

    changed = FALSE;

    if (!file->details->got_file_info)
    {
        changed = TRUE;
    }
    file->details->got_file_info = TRUE;

    edit_name = g_file_info_get_attribute_string (info,
                                                  G_FILE_ATTRIBUTE_STANDARD_EDIT_NAME);
    changed |= nautilus_file_set_display_name (file,
                                               g_file_info_get_display_name (info),
                                               edit_name,
                                               FALSE);

    file_type = g_file_info_get_file_type (info);
    if (file->details->type != file_type)
    {
        changed = TRUE;
    }
    file->details->type = file_type;

    if (g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_VIRTUAL) ||
        file_type == G_FILE_TYPE_SHORTCUT ||
        nautilus_file_is_in_recent (file))
    {
        activation_uri = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
        if (activation_uri == NULL)
        {
            if (file->details->activation_uri)
            {
                g_free (file->details->activation_uri);
                file->details->activation_uri = NULL;
                changed = TRUE;
            }
        }
        else
        {
            old_activation_uri = file->details->activation_uri;
            file->details->activation_uri = g_strdup (activation_uri);

            if (old_activation_uri)
            {
                if (strcmp (old_activation_uri,
                            file->details->activation_uri) != 0)
                {
                    changed = TRUE;
                }
                g_free (old_activation_uri);
            }
            else
            {
                changed = TRUE;
            }
        }
    }

    is_symlink = g_file_info_get_attribute_boolean (info,
                                                    G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK);
    if (file->details->is_symlink != is_symlink)
    {
        changed = TRUE;
    }
    file->details->is_symlink = is_symlink;

    is_hidden = g_file_info_get_attribute_boolean (info,
                                                   G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN) ||
                g_file_info_get_attribute_boolean (info,
                                                   G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP);
    if (file->details->is_hidden != is_hidden)
    {
        changed = TRUE;
    }
    file->details->is_hidden = is_hidden;

    is_mountpoint = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_UNIX_IS_MOUNTPOINT);
    if (file->details->is_mountpoint != is_mountpoint)
    {
        changed = TRUE;
    }
    file->details->is_mountpoint = is_mountpoint;

    has_permissions = g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_MODE);
    permissions = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);
    if (file->details->has_permissions != has_permissions ||
        file->details->permissions != permissions)
    {
        changed = TRUE;
    }
    file->details->has_permissions = has_permissions;
    file->details->permissions = permissions;

    /* We default to TRUE for this if we can't know */
    can_read = TRUE;
    can_write = TRUE;
    can_execute = TRUE;
    can_delete = TRUE;
    can_rename = TRUE;
    can_trash = FALSE;
    can_mount = FALSE;
    can_unmount = FALSE;
    can_eject = FALSE;
    can_start = FALSE;
    can_start_degraded = FALSE;
    can_stop = FALSE;
    can_poll_for_media = FALSE;
    is_media_check_automatic = FALSE;
    start_stop_type = G_DRIVE_START_STOP_TYPE_UNKNOWN;
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
    {
        can_read = g_file_info_get_attribute_boolean (info,
                                                      G_FILE_ATTRIBUTE_ACCESS_CAN_READ);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
    {
        can_write = g_file_info_get_attribute_boolean (info,
                                                       G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE))
    {
        can_execute = g_file_info_get_attribute_boolean (info,
                                                         G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE))
    {
        can_delete = g_file_info_get_attribute_boolean (info,
                                                        G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH))
    {
        can_trash = g_file_info_get_attribute_boolean (info,
                                                       G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME))
    {
        can_rename = g_file_info_get_attribute_boolean (info,
                                                        G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_MOUNT))
    {
        can_mount = g_file_info_get_attribute_boolean (info,
                                                       G_FILE_ATTRIBUTE_MOUNTABLE_CAN_MOUNT);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_UNMOUNT))
    {
        can_unmount = g_file_info_get_attribute_boolean (info,
                                                         G_FILE_ATTRIBUTE_MOUNTABLE_CAN_UNMOUNT);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_EJECT))
    {
        can_eject = g_file_info_get_attribute_boolean (info,
                                                       G_FILE_ATTRIBUTE_MOUNTABLE_CAN_EJECT);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_START))
    {
        can_start = g_file_info_get_attribute_boolean (info,
                                                       G_FILE_ATTRIBUTE_MOUNTABLE_CAN_START);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_START_DEGRADED))
    {
        can_start_degraded = g_file_info_get_attribute_boolean (info,
                                                                G_FILE_ATTRIBUTE_MOUNTABLE_CAN_START_DEGRADED);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_STOP))
    {
        can_stop = g_file_info_get_attribute_boolean (info,
                                                      G_FILE_ATTRIBUTE_MOUNTABLE_CAN_STOP);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_START_STOP_TYPE))
    {
        start_stop_type = g_file_info_get_attribute_uint32 (info,
                                                            G_FILE_ATTRIBUTE_MOUNTABLE_START_STOP_TYPE);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_CAN_POLL))
    {
        can_poll_for_media = g_file_info_get_attribute_boolean (info,
                                                                G_FILE_ATTRIBUTE_MOUNTABLE_CAN_POLL);
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_MOUNTABLE_IS_MEDIA_CHECK_AUTOMATIC))
    {
        is_media_check_automatic = g_file_info_get_attribute_boolean (info,
                                                                      G_FILE_ATTRIBUTE_MOUNTABLE_IS_MEDIA_CHECK_AUTOMATIC);
    }
    if (file->details->can_read != can_read ||
        file->details->can_write != can_write ||
        file->details->can_execute != can_execute ||
        file->details->can_delete != can_delete ||
        file->details->can_trash != can_trash ||
        file->details->can_rename != can_rename ||
        file->details->can_mount != can_mount ||
        file->details->can_unmount != can_unmount ||
        file->details->can_eject != can_eject ||
        file->details->can_start != can_start ||
        file->details->can_start_degraded != can_start_degraded ||
        file->details->can_stop != can_stop ||
        file->details->start_stop_type != start_stop_type ||
        file->details->can_poll_for_media != can_poll_for_media ||
        file->details->is_media_check_automatic != is_media_check_automatic)
    {
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
    file->details->can_start = can_start;
    file->details->can_start_degraded = can_start_degraded;
    file->details->can_stop = can_stop;
    file->details->start_stop_type = start_stop_type;
    file->details->can_poll_for_media = can_poll_for_media;
    file->details->is_media_check_automatic = is_media_check_automatic;

    free_owner = FALSE;
    owner = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_USER);
    owner_real = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_USER_REAL);
    free_group = FALSE;
    group = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_OWNER_GROUP);

    uid = -1;
    gid = -1;
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_UID))
    {
        uid = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_UID);
        if (owner == NULL)
        {
            free_owner = TRUE;
            owner = g_strdup_printf ("%d", uid);
        }
    }
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_GID))
    {
        gid = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_GID);
        if (group == NULL)
        {
            free_group = TRUE;
            group = g_strdup_printf ("%d", gid);
        }
    }
    if (file->details->uid != uid ||
        file->details->gid != gid)
    {
        changed = TRUE;
    }
    file->details->uid = uid;
    file->details->gid = gid;

    if (g_strcmp0 (file->details->owner, owner) != 0)
    {
        changed = TRUE;
        g_clear_pointer (&file->details->owner, g_ref_string_release);
        file->details->owner = g_ref_string_new_intern (owner);
    }

    if (g_strcmp0 (file->details->owner_real, owner_real) != 0)
    {
        changed = TRUE;
        g_clear_pointer (&file->details->owner_real, g_ref_string_release);
        file->details->owner_real = g_ref_string_new_intern (owner_real);
    }

    if (g_strcmp0 (file->details->group, group) != 0)
    {
        changed = TRUE;
        g_clear_pointer (&file->details->group, g_ref_string_release);
        file->details->group = g_ref_string_new_intern (group);
    }

    if (free_owner)
    {
        g_free ((char *) owner);
    }
    if (free_group)
    {
        g_free ((char *) group);
    }

    size = -1;
    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE))
    {
        size = g_file_info_get_size (info);
    }
    if (file->details->size != size)
    {
        changed = TRUE;
    }
    file->details->size = size;

    sort_order = g_file_info_get_attribute_int32 (info,
                                                  G_FILE_ATTRIBUTE_STANDARD_SORT_ORDER);
    if (file->details->sort_order != sort_order)
    {
        changed = TRUE;
    }
    file->details->sort_order = sort_order;

    atime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_ACCESS);
    mtime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
    btime = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_CREATED);
    if (file->details->atime != atime ||
        file->details->mtime != mtime)
    {
        if (file->details->thumbnail == NULL)
        {
            file->details->thumbnail_is_up_to_date = FALSE;
        }

        changed = TRUE;
    }
    file->details->atime = atime;
    file->details->mtime = mtime;
    file->details->btime = btime;

    if (file->details->thumbnail != NULL &&
        file->details->thumbnail_mtime != 0 &&
        file->details->thumbnail_mtime != mtime)
    {
        file->details->thumbnail_is_up_to_date = FALSE;
        changed = TRUE;
    }

    icon = g_file_info_get_icon (info);
    if (!g_icon_equal (icon, file->details->icon))
    {
        changed = TRUE;

        if (file->details->icon)
        {
            g_object_unref (file->details->icon);
        }
        file->details->icon = g_object_ref (icon);
    }

    thumbnail_path = g_file_info_get_attribute_byte_string (info, G_FILE_ATTRIBUTE_THUMBNAIL_PATH);
    if (g_strcmp0 (file->details->thumbnail_path, thumbnail_path) != 0)
    {
        changed = TRUE;
        g_free (file->details->thumbnail_path);
        file->details->thumbnail_path = g_strdup (thumbnail_path);
    }

    thumbnailing_failed = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_THUMBNAILING_FAILED);
    if (file->details->thumbnailing_failed != thumbnailing_failed)
    {
        changed = TRUE;
        file->details->thumbnailing_failed = thumbnailing_failed;
    }

    symlink_name = g_file_info_get_attribute_byte_string (info,
                                                          G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET);
    if (g_strcmp0 (file->details->symlink_name, symlink_name) != 0)
    {
        changed = TRUE;
        g_free (file->details->symlink_name);
        file->details->symlink_name = g_strdup (symlink_name);
    }

    mime_type = g_file_info_get_attribute_string (info,
                                                  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE);
    if (mime_type == NULL)
    {
        mime_type = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
    }
    if (g_strcmp0 (file->details->mime_type, mime_type) != 0)
    {
        changed = TRUE;
        g_clear_pointer (&file->details->mime_type, g_ref_string_release);
        file->details->mime_type = g_ref_string_new_intern (mime_type);
    }

    selinux_context = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_SELINUX_CONTEXT);
    if (g_strcmp0 (file->details->selinux_context, selinux_context) != 0)
    {
        changed = TRUE;
        g_free (file->details->selinux_context);
        file->details->selinux_context = g_strdup (selinux_context);
    }

    description = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_DESCRIPTION);
    if (g_strcmp0 (file->details->description, description) != 0)
    {
        changed = TRUE;
        g_free (file->details->description);
        file->details->description = g_strdup (description);
    }

    filesystem_id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
    if (g_strcmp0 (file->details->filesystem_id, filesystem_id) != 0)
    {
        changed = TRUE;
        g_clear_pointer (&file->details->filesystem_id, g_ref_string_release);
        file->details->filesystem_id = g_ref_string_new_intern (filesystem_id);
    }

    trash_time = 0;
    time_string = g_file_info_get_attribute_string (info, "trash::deletion-date");
    if (time_string != NULL)
    {
        g_autoptr (GTimeZone) tz = g_time_zone_new_local ();
        g_autoptr (GDateTime) date_time = g_date_time_new_from_iso8601 (time_string, tz);

        trash_time = date_time != NULL ? g_date_time_to_unix (date_time) : 0;
    }
    if (file->details->trash_time != trash_time)
    {
        changed = TRUE;
        file->details->trash_time = trash_time;
    }

    recency = g_file_info_get_attribute_int64 (info, G_FILE_ATTRIBUTE_RECENT_MODIFIED);
    if (file->details->recency != recency)
    {
        changed = TRUE;
        file->details->recency = recency;
    }

    trash_orig_path = g_file_info_get_attribute_byte_string (info, "trash::orig-path");
    if (g_strcmp0 (file->details->trash_orig_path, trash_orig_path) != 0)
    {
        changed = TRUE;
        g_free (file->details->trash_orig_path);
        file->details->trash_orig_path = g_strdup (trash_orig_path);
    }

    if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_PREVIEW_ICON))
    {
        file->details->has_preview_icon = TRUE;
    }

    changed |=
        nautilus_file_update_metadata_from_info (file, info);

    if (update_name)
    {
        name = g_file_info_get_name (info);
        if (file->details->name == NULL ||
            strcmp (file->details->name, name) != 0)
        {
            changed = TRUE;

            node = nautilus_directory_begin_file_name_change
                       (file->details->directory, file);

            g_clear_pointer (&file->details->name, g_ref_string_release);
            if (g_strcmp0 (file->details->display_name, name) == 0)
            {
                file->details->name = g_ref_string_acquire (file->details->display_name);
            }
            else
            {
                file->details->name = g_ref_string_new (name);
            }

            if (!file->details->got_custom_display_name &&
                g_file_info_get_display_name (info) == NULL)
            {
                /* If the file info's display name is NULL,
                 * nautilus_file_set_display_name() did
                 * not unset the display name.
                 */
                nautilus_file_clear_display_name (file);
            }

            nautilus_directory_end_file_name_change
                (file->details->directory, file, node);
        }
    }

    if (changed)
    {
        add_to_link_hash_table (file);

        update_links_if_target (file);
    }

    return changed;
}

static gboolean
update_info_and_name (NautilusFile *file,
                      GFileInfo    *info)
{
    return update_info_internal (file, info, TRUE);
}

gboolean
nautilus_file_update_info (NautilusFile *file,
                           GFileInfo    *info)
{
    return update_info_internal (file, info, FALSE);
}

static gboolean
update_name_internal (NautilusFile *file,
                      const char   *name,
                      gboolean      in_directory)
{
    GList *node;

    g_assert (name != NULL);

    if (file->details->is_gone)
    {
        return FALSE;
    }

    if (name_is (file, name))
    {
        return FALSE;
    }

    node = NULL;
    if (in_directory)
    {
        node = nautilus_directory_begin_file_name_change
                   (file->details->directory, file);
    }

    g_clear_pointer (&file->details->name, g_ref_string_release);
    file->details->name = g_ref_string_new (name);

    if (!file->details->got_custom_display_name)
    {
        nautilus_file_clear_display_name (file);
    }

    if (in_directory)
    {
        nautilus_directory_end_file_name_change
            (file->details->directory, file, node);
    }

    return TRUE;
}

gboolean
nautilus_file_update_name (NautilusFile *file,
                           const char   *name)
{
    gboolean ret;

    ret = update_name_internal (file, name, TRUE);

    if (ret)
    {
        update_links_if_target (file);
    }

    return ret;
}

gboolean
nautilus_file_update_name_and_directory (NautilusFile      *file,
                                         const char        *name,
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
    if (old_directory == new_directory)
    {
        if (name)
        {
            return update_name_internal (file, name, TRUE);
        }
        else
        {
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

    nautilus_file_set_directory (file, new_directory);

    if (name)
    {
        update_name_internal (file, name, FALSE);
    }

    nautilus_directory_add_file (new_directory, file);
    nautilus_directory_add_file_monitors (new_directory, file, monitors);

    add_to_link_hash_table (file);

    update_links_if_target (file);

    nautilus_file_unref (file);

    return TRUE;
}

static Knowledge
get_item_count (NautilusFile *file,
                guint        *count)
{
    gboolean known, unreadable;

    known = nautilus_file_get_directory_item_count
                (file, count, &unreadable);
    if (!known)
    {
        return UNKNOWN;
    }
    if (unreadable)
    {
        return UNKNOWABLE;
    }
    return KNOWN;
}

static Knowledge
get_size (NautilusFile *file,
          goffset      *size)
{
    /* If we tried and failed, then treat it like there is no size
     * to know.
     */
    if (file->details->get_info_failed)
    {
        return UNKNOWABLE;
    }

    /* If the info is NULL that means we haven't even tried yet,
     * so it's just unknown, not unknowable.
     */
    if (!file->details->got_file_info)
    {
        return UNKNOWN;
    }

    /* If we got info with no size in it, it means there is no
     * such thing as a size as far as gnome-vfs is concerned,
     * so "unknowable".
     */
    if (file->details->size == -1)
    {
        return UNKNOWABLE;
    }

    /* We have a size! */
    *size = file->details->size;
    return KNOWN;
}

static Knowledge
get_time (NautilusFile     *file,
          time_t           *time_out,
          NautilusDateType  type)
{
    time_t time;

    /* If we tried and failed, then treat it like there is no size
     * to know.
     */
    if (file->details->get_info_failed)
    {
        return UNKNOWABLE;
    }

    /* If the info is NULL that means we haven't even tried yet,
     * so it's just unknown, not unknowable.
     */
    if (!file->details->got_file_info)
    {
        return UNKNOWN;
    }

    switch (type)
    {
        case NAUTILUS_DATE_TYPE_MODIFIED:
        {
            time = file->details->mtime;
        }
        break;

        case NAUTILUS_DATE_TYPE_ACCESSED:
        {
            time = file->details->atime;
        }
        break;

        case NAUTILUS_DATE_TYPE_CREATED:
        {
            time = file->details->btime;
        }
        break;

        case NAUTILUS_DATE_TYPE_TRASHED:
        {
            time = file->details->trash_time;
        }
        break;

        case NAUTILUS_DATE_TYPE_RECENCY:
        {
            time = file->details->recency;
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
        break;
    }

    *time_out = time;

    /* If we got info with no modification time in it, it means
     * there is no such thing as a modification time as far as
     * gnome-vfs is concerned, so "unknowable".
     */
    if (time == 0)
    {
        return UNKNOWABLE;
    }
    return KNOWN;
}

static int
compare_directories_by_count (NautilusFile *file_1,
                              NautilusFile *file_2)
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

    if (count_known_1 > count_known_2)
    {
        return -1;
    }
    if (count_known_1 < count_known_2)
    {
        return +1;
    }

    /* count_known_1 and count_known_2 are equal now. Check if count
     * details are UNKNOWABLE or UNKNOWN.
     */
    if (count_known_1 == UNKNOWABLE || count_known_1 == UNKNOWN)
    {
        return 0;
    }

    if (count_1 < count_2)
    {
        return -1;
    }
    if (count_1 > count_2)
    {
        return +1;
    }

    return 0;
}

static int
compare_files_by_size (NautilusFile *file_1,
                       NautilusFile *file_2)
{
    /* Sort order:
     *   Files with unknown size.
     *   Files with "unknowable" size.
     *   Files with smaller sizes.
     *   Files with large sizes.
     */

    Knowledge size_known_1, size_known_2;
    goffset size_1 = 0, size_2 = 0;

    size_known_1 = get_size (file_1, &size_1);
    size_known_2 = get_size (file_2, &size_2);

    if (size_known_1 > size_known_2)
    {
        return -1;
    }
    if (size_known_1 < size_known_2)
    {
        return +1;
    }

    /* size_known_1 and size_known_2 are equal now. Check if size
     * details are UNKNOWABLE or UNKNOWN
     */
    if (size_known_1 == UNKNOWABLE || size_known_1 == UNKNOWN)
    {
        return 0;
    }

    if (size_1 < size_2)
    {
        return -1;
    }
    if (size_1 > size_2)
    {
        return +1;
    }

    return 0;
}

static int
compare_by_size (NautilusFile *file_1,
                 NautilusFile *file_2)
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

    if (is_directory_1 && !is_directory_2)
    {
        return -1;
    }
    if (is_directory_2 && !is_directory_1)
    {
        return +1;
    }

    if (is_directory_1)
    {
        return compare_directories_by_count (file_1, file_2);
    }
    else
    {
        return compare_files_by_size (file_1, file_2);
    }
}

static int
compare_by_display_name (NautilusFile *file_1,
                         NautilusFile *file_2)
{
    const char *name_1, *name_2;
    const char *key_1, *key_2;
    gboolean sort_last_1, sort_last_2;
    int compare;

    name_1 = nautilus_file_peek_display_name (file_1);
    name_2 = nautilus_file_peek_display_name (file_2);

    sort_last_1 = name_1[0] == SORT_LAST_CHAR1 || name_1[0] == SORT_LAST_CHAR2;
    sort_last_2 = name_2[0] == SORT_LAST_CHAR1 || name_2[0] == SORT_LAST_CHAR2;

    if (sort_last_1 && !sort_last_2)
    {
        compare = +1;
    }
    else if (!sort_last_1 && sort_last_2)
    {
        compare = -1;
    }
    else
    {
        key_1 = nautilus_file_peek_display_name_collation_key (file_1);
        key_2 = nautilus_file_peek_display_name_collation_key (file_2);
        compare = strcmp (key_1, key_2);
    }

    return compare;
}

static int
compare_by_directory_name (NautilusFile *file_1,
                           NautilusFile *file_2)
{
    return strcmp (file_1->details->directory_name_collation_key,
                   file_2->details->directory_name_collation_key);
}

static GList *
prepend_automatic_keywords (NautilusFile *file,
                            GList        *names)
{
    /* Prepend in reverse order. */
    NautilusFile *parent;

    parent = nautilus_file_get_parent (file);

    /* Trash files are assumed to be read-only,
     * so we want to ignore them here. */
    if (!nautilus_file_can_write (file) &&
        !nautilus_file_is_in_trash (file) &&
        (parent == NULL || nautilus_file_can_write (parent)))
    {
        names = g_list_prepend
                    (names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_CANT_WRITE));
    }
    if (!nautilus_file_can_read (file))
    {
        names = g_list_prepend
                    (names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_CANT_READ));
    }
    if (nautilus_file_is_symbolic_link (file))
    {
        names = g_list_prepend
                    (names, g_strdup (NAUTILUS_FILE_EMBLEM_NAME_SYMBOLIC_LINK));
    }

    if (parent)
    {
        nautilus_file_unref (parent);
    }


    return names;
}

static int
compare_by_type (NautilusFile *file_1,
                 NautilusFile *file_2)
{
    gboolean is_directory_1;
    gboolean is_directory_2;
    const char *type_string_1;
    const char *type_string_2;
    int result;

    /* Directories go first. Then, if mime types are identical,
     * don't bother getting strings (for speed). This assumes
     * that the string is dependent entirely on the mime type,
     * which is true now but might not be later.
     */
    is_directory_1 = nautilus_file_is_directory (file_1);
    is_directory_2 = nautilus_file_is_directory (file_2);

    if (is_directory_1 && is_directory_2)
    {
        return 0;
    }

    if (is_directory_1)
    {
        return -1;
    }

    if (is_directory_2)
    {
        return +1;
    }

    if (file_1->details->mime_type != NULL &&
        file_2->details->mime_type != NULL &&
        strcmp (file_1->details->mime_type,
                file_2->details->mime_type) == 0)
    {
        return 0;
    }

    type_string_1 = nautilus_file_get_type_as_string_no_extra_text (file_1);
    type_string_2 = nautilus_file_get_type_as_string_no_extra_text (file_2);

    if (type_string_1 == NULL || type_string_2 == NULL)
    {
        if (type_string_1 != NULL)
        {
            return -1;
        }

        if (type_string_2 != NULL)
        {
            return 1;
        }

        return 0;
    }

    result = g_utf8_collate (type_string_1, type_string_2);
    if (result == 0)
    {
        /* Among files of the same (generic) type, sort them by mime type. */
        result = g_utf8_collate (file_1->details->mime_type, file_2->details->mime_type);
    }

    return result;
}

static int
compare_by_starred (NautilusFile *file_1,
                    NautilusFile *file_2)
{
    NautilusTagManager *tag_manager = nautilus_tag_manager_get ();
    g_autofree gchar *uri_1 = NULL;
    g_autofree gchar *uri_2 = NULL;
    gboolean file_1_is_starred;
    gboolean file_2_is_starred;

    uri_1 = nautilus_file_get_uri (file_1);
    uri_2 = nautilus_file_get_uri (file_2);

    file_1_is_starred = nautilus_tag_manager_file_is_starred (tag_manager,
                                                              uri_1);
    file_2_is_starred = nautilus_tag_manager_file_is_starred (tag_manager,
                                                              uri_2);
    if (!!file_1_is_starred == !!file_2_is_starred)
    {
        return 0;
    }
    else if (file_1_is_starred && !file_2_is_starred)
    {
        return -1;
    }
    else
    {
        return 1;
    }
}

static Knowledge
get_search_relevance (NautilusFile *file,
                      gdouble      *relevance_out)
{
    /* we're only called in search directories, and in that
     * case, the relevance is always known (or zero).
     */
    *relevance_out = file->details->search_relevance;
    return KNOWN;
}

static int
compare_by_search_relevance (NautilusFile *file_1,
                             NautilusFile *file_2)
{
    gdouble r_1, r_2;

    get_search_relevance (file_1, &r_1);
    get_search_relevance (file_2, &r_2);

    if (r_1 < r_2)
    {
        return -1;
    }
    if (r_1 > r_2)
    {
        return +1;
    }

    return 0;
}

static int
compare_by_time (NautilusFile     *file_1,
                 NautilusFile     *file_2,
                 NautilusDateType  type)
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

    if (time_known_1 > time_known_2)
    {
        return -1;
    }
    if (time_known_1 < time_known_2)
    {
        return +1;
    }

    /* Now time_known_1 is equal to time_known_2. Check whether
     * we failed to get modification times for files
     */
    if (time_known_1 == UNKNOWABLE || time_known_1 == UNKNOWN)
    {
        return 0;
    }

    if (time_1 < time_2)
    {
        return -1;
    }
    if (time_1 > time_2)
    {
        return +1;
    }

    return 0;
}

static int
compare_by_full_path (NautilusFile *file_1,
                      NautilusFile *file_2)
{
    int compare;

    compare = compare_by_directory_name (file_1, file_2);
    if (compare != 0)
    {
        return compare;
    }
    return compare_by_display_name (file_1, file_2);
}

static int
nautilus_file_compare_for_sort_internal (NautilusFile *file_1,
                                         NautilusFile *file_2,
                                         gboolean      directories_first,
                                         gboolean      reversed)
{
    gboolean is_directory_1, is_directory_2;

    if (directories_first)
    {
        is_directory_1 = nautilus_file_is_directory (file_1);
        is_directory_2 = nautilus_file_is_directory (file_2);

        if (is_directory_1 && !is_directory_2)
        {
            return -1;
        }

        if (is_directory_2 && !is_directory_1)
        {
            return +1;
        }
    }

    if (file_1->details->sort_order < file_2->details->sort_order)
    {
        return reversed ? 1 : -1;
    }
    else if (file_1->details->sort_order > file_2->details->sort_order)
    {
        return reversed ? -1 : 1;
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
nautilus_file_compare_for_sort (NautilusFile         *file_1,
                                NautilusFile         *file_2,
                                NautilusFileSortType  sort_type,
                                gboolean              directories_first,
                                gboolean              reversed)
{
    int result;

    if (file_1 == file_2)
    {
        return 0;
    }

    result = nautilus_file_compare_for_sort_internal (file_1, file_2, directories_first, reversed);

    if (result == 0)
    {
        switch (sort_type)
        {
            case NAUTILUS_FILE_SORT_BY_DISPLAY_NAME:
            {
                result = compare_by_display_name (file_1, file_2);
                if (result == 0)
                {
                    result = compare_by_directory_name (file_1, file_2);
                }
            }
            break;

            case NAUTILUS_FILE_SORT_BY_SIZE:
            {
                /* Compare directory sizes ourselves, then if necessary
                 * use GnomeVFS to compare file sizes.
                 */
                result = compare_by_size (file_1, file_2);
                if (result == 0)
                {
                    result = compare_by_full_path (file_1, file_2);
                }
            }
            break;

            case NAUTILUS_FILE_SORT_BY_TYPE:
            {
                /* GnomeVFS doesn't know about our special text for certain
                 * mime types, so we handle the mime-type sorting ourselves.
                 */
                result = compare_by_type (file_1, file_2);
                if (result == 0)
                {
                    result = compare_by_full_path (file_1, file_2);
                }
            }
            break;

            case NAUTILUS_FILE_SORT_BY_STARRED:
            {
                result = compare_by_starred (file_1, file_2);
                if (result == 0)
                {
                    result = compare_by_full_path (file_1, file_2);
                }
            }
            break;

            case NAUTILUS_FILE_SORT_BY_MTIME:
            {
                result = compare_by_time (file_1, file_2, NAUTILUS_DATE_TYPE_MODIFIED);
                if (result == 0)
                {
                    result = compare_by_full_path (file_1, file_2);
                }
            }
            break;

            case NAUTILUS_FILE_SORT_BY_ATIME:
            {
                result = compare_by_time (file_1, file_2, NAUTILUS_DATE_TYPE_ACCESSED);
                if (result == 0)
                {
                    result = compare_by_full_path (file_1, file_2);
                }
            }
            break;

            case NAUTILUS_FILE_SORT_BY_BTIME:
            {
                result = compare_by_time (file_1, file_2, NAUTILUS_DATE_TYPE_CREATED);
                if (result == 0)
                {
                    result = compare_by_full_path (file_1, file_2);
                }
            }
            break;

            case NAUTILUS_FILE_SORT_BY_TRASHED_TIME:
            {
                result = compare_by_time (file_1, file_2, NAUTILUS_DATE_TYPE_TRASHED);
                if (result == 0)
                {
                    result = compare_by_full_path (file_1, file_2);
                }
            }
            break;

            case NAUTILUS_FILE_SORT_BY_SEARCH_RELEVANCE:
            {
                result = compare_by_search_relevance (file_1, file_2);
                if (result == 0)
                {
                    result = compare_by_full_path (file_1, file_2);

                    /* ensure alphabetical order for files of the same relevance
                     * grows in reverse (higher character = lower relevance) */
                    result = -result;
                }
            }
            break;

            case NAUTILUS_FILE_SORT_BY_RECENCY:
            {
                result = compare_by_time (file_1, file_2, NAUTILUS_DATE_TYPE_RECENCY);
                if (result == 0)
                {
                    result = compare_by_full_path (file_1, file_2);
                }
            }
            break;

            default:
            {
                g_return_val_if_reached (0);
            }
        }

        if (reversed)
        {
            result = -result;
        }
    }

    return result;
}

int
nautilus_file_compare_for_sort_by_attribute_q   (NautilusFile *file_1,
                                                 NautilusFile *file_2,
                                                 GQuark        attribute,
                                                 gboolean      directories_first,
                                                 gboolean      reversed)
{
    int result;

    if (file_1 == file_2)
    {
        return 0;
    }

    /* Convert certain attributes into NautilusFileSortTypes and use
     * nautilus_file_compare_for_sort()
     */
    if (attribute == 0 || attribute == attribute_name_q)
    {
        return nautilus_file_compare_for_sort (file_1, file_2,
                                               NAUTILUS_FILE_SORT_BY_DISPLAY_NAME,
                                               directories_first,
                                               reversed);
    }
    else if (attribute == attribute_size_q)
    {
        return nautilus_file_compare_for_sort (file_1, file_2,
                                               NAUTILUS_FILE_SORT_BY_SIZE,
                                               directories_first,
                                               reversed);
    }
    else if (attribute == attribute_type_q)
    {
        return nautilus_file_compare_for_sort (file_1, file_2,
                                               NAUTILUS_FILE_SORT_BY_TYPE,
                                               directories_first,
                                               reversed);
    }
    else if (attribute == attribute_starred_q)
    {
        return nautilus_file_compare_for_sort (file_1, file_2,
                                               NAUTILUS_FILE_SORT_BY_STARRED,
                                               directories_first,
                                               reversed);
    }
    else if (attribute == attribute_modification_date_q || attribute == attribute_date_modified_q || attribute == attribute_date_modified_with_time_q || attribute == attribute_date_modified_full_q)
    {
        return nautilus_file_compare_for_sort (file_1, file_2,
                                               NAUTILUS_FILE_SORT_BY_MTIME,
                                               directories_first,
                                               reversed);
    }
    else if (attribute == attribute_accessed_date_q || attribute == attribute_date_accessed_q || attribute == attribute_date_accessed_full_q)
    {
        return nautilus_file_compare_for_sort (file_1, file_2,
                                               NAUTILUS_FILE_SORT_BY_ATIME,
                                               directories_first,
                                               reversed);
    }
    else if (attribute == attribute_date_created_q || attribute == attribute_date_created_full_q)
    {
        return nautilus_file_compare_for_sort (file_1, file_2,
                                               NAUTILUS_FILE_SORT_BY_BTIME,
                                               directories_first,
                                               reversed);
    }
    else if (attribute == attribute_trashed_on_q || attribute == attribute_trashed_on_full_q)
    {
        return nautilus_file_compare_for_sort (file_1, file_2,
                                               NAUTILUS_FILE_SORT_BY_TRASHED_TIME,
                                               directories_first,
                                               reversed);
    }
    else if (attribute == attribute_search_relevance_q)
    {
        return nautilus_file_compare_for_sort (file_1, file_2,
                                               NAUTILUS_FILE_SORT_BY_SEARCH_RELEVANCE,
                                               directories_first,
                                               reversed);
    }
    else if (attribute == attribute_recency_q)
    {
        return nautilus_file_compare_for_sort (file_1, file_2,
                                               NAUTILUS_FILE_SORT_BY_RECENCY,
                                               directories_first,
                                               reversed);
    }

    /* it is a normal attribute, compare by strings */

    result = nautilus_file_compare_for_sort_internal (file_1, file_2, directories_first, reversed);

    if (result == 0)
    {
        char *value_1;
        char *value_2;

        value_1 = nautilus_file_get_string_attribute_q (file_1,
                                                        attribute);
        value_2 = nautilus_file_get_string_attribute_q (file_2,
                                                        attribute);

        if (value_1 != NULL && value_2 != NULL)
        {
            result = strcmp (value_1, value_2);
        }

        g_free (value_1);
        g_free (value_2);

        if (reversed)
        {
            result = -result;
        }
    }

    return result;
}

int
nautilus_file_compare_for_sort_by_attribute     (NautilusFile *file_1,
                                                 NautilusFile *file_2,
                                                 const char   *attribute,
                                                 gboolean      directories_first,
                                                 gboolean      reversed)
{
    return nautilus_file_compare_for_sort_by_attribute_q (file_1, file_2,
                                                          g_quark_from_string (attribute),
                                                          directories_first,
                                                          reversed);
}


/**
 * nautilus_file_compare_name:
 * @file: A file object
 * @string: A string we are comparing it with
 *
 * Return value: result of a comparison of the file name and the given string.
 **/
int
nautilus_file_compare_display_name (NautilusFile *file,
                                    const char   *string)
{
    const char *name;
    int result;

    g_return_val_if_fail (string != NULL, -1);

    name = nautilus_file_peek_display_name (file);
    result = g_strcmp0 (name, string);
    return result;
}


gboolean
nautilus_file_is_hidden_file (NautilusFile *file)
{
    return file->details->is_hidden;
}

/**
 * nautilus_file_should_show:
 * @file: the file to check
 * @show_hidden: whether we want to show hidden files or not
 *
 * Determines if a #NautilusFile should be shown. Note that when browsing
 * a trash directory, this function will always return %TRUE.
 *
 * Returns: %TRUE if the file should be shown, %FALSE if it shouldn't.
 */
gboolean
nautilus_file_should_show (NautilusFile *file,
                           gboolean      show_hidden)
{
    /* Never hide any files in trash. */
    if (nautilus_file_is_in_trash (file))
    {
        return TRUE;
    }

    if (!show_hidden && nautilus_file_is_hidden_file (file))
    {
        return FALSE;
    }

    return TRUE;
}

gboolean
nautilus_file_is_home (NautilusFile *file)
{
    g_autoptr (GFile) location = NULL;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    location = nautilus_directory_get_location (file->details->directory);
    if (location == NULL)
    {
        return FALSE;
    }

    return nautilus_is_home_directory_file (location, file->details->name);
}

gboolean
nautilus_file_is_in_search (NautilusFile *file)
{
    g_autoptr (GFile) location = nautilus_file_get_location (file);

    return g_file_has_uri_scheme (location, SCHEME_SEARCH);
}

static gboolean
filter_hidden_partition_callback (NautilusFile *file,
                                  gpointer      callback_data)
{
    FilterOptions options;

    options = GPOINTER_TO_INT (callback_data);

    return nautilus_file_should_show (file,
                                      options & SHOW_HIDDEN);
}

GList *
nautilus_file_list_filter_hidden (GList    *files,
                                  gboolean  show_hidden)
{
    GList *filtered_files;
    GList *removed_files;

    /* FIXME bugzilla.gnome.org 40653:
     * Eventually this should become a generic filtering thingy.
     */

    filtered_files = nautilus_file_list_filter (files,
                                                &removed_files,
                                                filter_hidden_partition_callback,
                                                GINT_TO_POINTER ((show_hidden ? SHOW_HIDDEN : 0)));
    nautilus_file_list_free (removed_files);

    return filtered_files;
}

/* This functions filters a file list when its items match a certain condition
 * in the filter function. This function preserves the ordering.
 */
GList *
nautilus_file_list_filter (GList                   *files,
                           GList                  **failed,
                           NautilusFileFilterFunc   filter_function,
                           gpointer                 user_data)
{
    GList *filtered = NULL;
    GList *l;
    GList *reversed;

    *failed = NULL;
    /* Avoid using g_list_append since it's O(n) */
    reversed = g_list_copy (files);
    reversed = g_list_reverse (reversed);
    for (l = reversed; l != NULL; l = l->next)
    {
        if (filter_function (l->data, user_data))
        {
            filtered = g_list_prepend (filtered, nautilus_file_ref (l->data));
        }
        else
        {
            *failed = g_list_prepend (*failed, nautilus_file_ref (l->data));
        }
    }

    g_list_free (reversed);

    return filtered;
}

gboolean
nautilus_file_list_are_all_folders (const GList *files)
{
    const GList *l;

    for (l = files; l != NULL; l = l->next)
    {
        if (!nautilus_file_is_directory (NAUTILUS_FILE (l->data)))
        {
            return FALSE;
        }
    }

    return TRUE;
}

char *
nautilus_file_get_metadata (NautilusFile *file,
                            const char   *key,
                            const char   *default_metadata)
{
    guint id;
    char *value;

    g_return_val_if_fail (key != NULL, g_strdup (default_metadata));
    g_return_val_if_fail (key[0] != '\0', g_strdup (default_metadata));

    if (file == NULL ||
        file->details->metadata == NULL)
    {
        return g_strdup (default_metadata);
    }

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), g_strdup (default_metadata));

    id = nautilus_metadata_get_id (key);
    value = g_hash_table_lookup (file->details->metadata, GUINT_TO_POINTER (id));

    if (value)
    {
        return g_strdup (value);
    }
    return g_strdup (default_metadata);
}

/**
 * nautilus_file_get_metadata_list:
 * @file: A #NautilusFile to get metadata from.
 * @key: A string representation of the metadata key (use macros when possible).
 *
 * Get the value of a metadata attribute which holds a list of strings.
 *
 * Returns: (transfer full): A zero-terminated array of newly allocated strings.
 */
gchar **
nautilus_file_get_metadata_list (NautilusFile *file,
                                 const char   *key)
{
    guint id;
    char **value;

    g_return_val_if_fail (key != NULL, NULL);
    g_return_val_if_fail (key[0] != '\0', NULL);

    if (file == NULL ||
        file->details->metadata == NULL)
    {
        return NULL;
    }

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    id = nautilus_metadata_get_id (key);
    id |= METADATA_ID_IS_LIST_MASK;

    value = g_hash_table_lookup (file->details->metadata, GUINT_TO_POINTER (id));

    return g_strdupv (value);
}

void
nautilus_file_set_metadata (NautilusFile *file,
                            const char   *key,
                            const char   *default_metadata,
                            const char   *metadata)
{
    const char *val;

    g_return_if_fail (NAUTILUS_IS_FILE (file));
    g_return_if_fail (key != NULL);
    g_return_if_fail (key[0] != '\0');

    val = metadata;
    if (val == NULL)
    {
        val = default_metadata;
    }

    NAUTILUS_FILE_CLASS (G_OBJECT_GET_CLASS (file))->set_metadata (file, key, val);
}

/**
 * nautilus_file_set_metadata_list:
 * @file: A #NautilusFile to set metadata into.
 * @key: A string representation of the metadata key (use macros when possible).
 * @list: (transfer none): A zero-terminated array of newly allocated strings.
 *
 * Set the value of a metadata attribute which takes a list of strings.
 */
void
nautilus_file_set_metadata_list (NautilusFile  *file,
                                 const char    *key,
                                 gchar        **list)
{
    g_return_if_fail (NAUTILUS_IS_FILE (file));
    g_return_if_fail (key != NULL);
    g_return_if_fail (key[0] != '\0');

    NAUTILUS_FILE_CLASS (G_OBJECT_GET_CLASS (file))->set_metadata_as_list (file, key, list);
}

gboolean
nautilus_file_get_boolean_metadata (NautilusFile *file,
                                    const char   *key,
                                    gboolean      default_metadata)
{
    char *result_as_string;
    gboolean result;

    g_return_val_if_fail (key != NULL, default_metadata);
    g_return_val_if_fail (key[0] != '\0', default_metadata);

    if (file == NULL)
    {
        return default_metadata;
    }

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), default_metadata);

    result_as_string = nautilus_file_get_metadata
                           (file, key, default_metadata ? "true" : "false");
    g_assert (result_as_string != NULL);

    if (g_ascii_strcasecmp (result_as_string, "true") == 0)
    {
        result = TRUE;
    }
    else if (g_ascii_strcasecmp (result_as_string, "false") == 0)
    {
        result = FALSE;
    }
    else
    {
        g_error ("boolean metadata with value other than true or false");
        result = default_metadata;
    }

    g_free (result_as_string);
    return result;
}

void
nautilus_file_set_boolean_metadata (NautilusFile *file,
                                    const char   *key,
                                    gboolean      metadata)
{
    g_return_if_fail (NAUTILUS_IS_FILE (file));
    g_return_if_fail (key != NULL);
    g_return_if_fail (key[0] != '\0');

    nautilus_file_set_metadata (file, key,
                                NULL, /* No default needed. Boolean string below is never NULL. */
                                metadata ? "true" : "false");
}

static const char *
nautilus_file_peek_display_name_collation_key (NautilusFile *file)
{
    const char *res;

    res = file->details->display_name_collation_key;
    if (res == NULL)
    {
        res = "";
    }

    return res;
}

static const char *
nautilus_file_peek_display_name (NautilusFile *file)
{
    const char *name;
    char *escaped_name;

    /* FIXME: for some reason we can get a NautilusFile instance which is
     *        no longer valid or could be freed somewhere else in the same time.
     *        There's race condition somewhere. See bug 602500.
     */
    if (file == NULL || nautilus_file_is_gone (file))
    {
        return "";
    }

    /* Default to display name based on filename if its not set yet */

    if (file->details->display_name == NULL)
    {
        name = file->details->name;
        if (g_utf8_validate (name, -1, NULL))
        {
            nautilus_file_set_display_name (file,
                                            name,
                                            NULL,
                                            FALSE);
        }
        else
        {
            escaped_name = g_uri_escape_string (name, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
            nautilus_file_set_display_name (file,
                                            escaped_name,
                                            NULL,
                                            FALSE);
            g_free (escaped_name);
        }
    }

    return file->details->display_name ?
           file->details->display_name : "";
}

const char *
nautilus_file_get_display_name (NautilusFile *file)
{
    if (nautilus_file_is_other_locations (file))
    {
        return _("Other Locations");
    }
    else if (nautilus_file_is_starred_location (file))
    {
        return _("Starred");
    }
    else
    {
        return nautilus_file_peek_display_name (file);
    }
}

char *
nautilus_file_get_edit_name (NautilusFile *file)
{
    const char *res;

    res = file->details->edit_name;
    if (res == NULL)
    {
        res = "";
    }

    return g_strdup (res);
}

char *
nautilus_file_get_name (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    return nautilus_file_info_get_name (NAUTILUS_FILE_INFO (file));
}

/**
 * nautilus_file_get_description:
 * @file: a #NautilusFile.
 *
 * Gets the standard::description key from @file, if
 * it has been cached.
 *
 * Returns: a string containing the value of the standard::description
 *  key, or %NULL.
 */
char *
nautilus_file_get_description (NautilusFile *file)
{
    return g_strdup (file->details->description);
}

void
nautilus_file_monitor_add (NautilusFile           *file,
                           gconstpointer           client,
                           NautilusFileAttributes  attributes)
{
    g_return_if_fail (NAUTILUS_IS_FILE (file));
    g_return_if_fail (client != NULL);

    NAUTILUS_FILE_CLASS (G_OBJECT_GET_CLASS (file))->monitor_add (file, client, attributes);
}

void
nautilus_file_monitor_remove (NautilusFile  *file,
                              gconstpointer  client)
{
    g_return_if_fail (NAUTILUS_IS_FILE (file));
    g_return_if_fail (client != NULL);

    NAUTILUS_FILE_CLASS (G_OBJECT_GET_CLASS (file))->monitor_remove (file, client);
}

gboolean
nautilus_file_has_activation_uri (NautilusFile *file)
{
    return file->details->activation_uri != NULL;
}


/* Return the uri associated with the passed-in file, which may not be
 * the actual uri if the file is an desktop file or a nautilus
 * xml link file.
 */
char *
nautilus_file_get_activation_uri (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    return nautilus_file_info_get_activation_uri (NAUTILUS_FILE_INFO (file));
}

GFile *
nautilus_file_get_activation_location (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    if (file->details->activation_uri != NULL)
    {
        return g_file_new_for_uri (file->details->activation_uri);
    }

    return nautilus_file_get_location (file);
}

static gboolean
is_uri_relative (const char *uri)
{
    char *scheme;
    gboolean ret;

    scheme = g_uri_parse_scheme (uri);
    ret = (scheme == NULL);
    g_free (scheme);
    return ret;
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
        is_uri_relative (uri))
    {
        dir_uri = nautilus_file_get_uri (file);
        custom_icon_uri = g_build_filename (dir_uri, uri, NULL);
        g_free (dir_uri);
        g_free (uri);
    }
    else
    {
        custom_icon_uri = uri;
    }
    return custom_icon_uri;
}

static char *
get_custom_icon_metadata_name (NautilusFile *file)
{
    char *icon_name;

    icon_name = nautilus_file_get_metadata (file,
                                            NAUTILUS_METADATA_KEY_CUSTOM_ICON_NAME, NULL);

    return icon_name;
}

static GIcon *
get_mount_icon (NautilusFile *file)
{
    GMount *mount;
    GIcon *mount_icon;

    mount = nautilus_file_get_mount (file);
    mount_icon = NULL;

    if (mount != NULL)
    {
        mount_icon = g_mount_get_icon (mount);
        g_object_unref (mount);
    }
    else
    {
        g_autoptr (GFile) location = nautilus_file_get_location (file);

        /* Root directory doesn't have a GMount, but for UI purposes we want
         * it to be treated the same way. */
        if (nautilus_is_root_directory (location))
        {
            mount_icon = g_themed_icon_new_with_default_fallbacks ("drive-harddisk");
        }
    }

    return mount_icon;
}

static GIcon *
get_custom_icon (NautilusFile *file)
{
    char *custom_icon_uri, *custom_icon_name;
    GFile *icon_file;
    GIcon *icon;

    if (file == NULL)
    {
        return NULL;
    }

    icon = NULL;

    /* Metadata takes precedence; first we look at the custom
     * icon URI, then at the custom icon name.
     */
    custom_icon_uri = get_custom_icon_metadata_uri (file);

    if (custom_icon_uri)
    {
        icon_file = g_file_new_for_uri (custom_icon_uri);
        icon = g_file_icon_new (icon_file);
        g_object_unref (icon_file);
        g_free (custom_icon_uri);
    }

    if (icon == NULL)
    {
        custom_icon_name = get_custom_icon_metadata_name (file);

        if (custom_icon_name != NULL)
        {
            icon = g_themed_icon_new_with_default_fallbacks (custom_icon_name);
            g_free (custom_icon_name);
        }
    }

    return icon;
}

static GIcon *
get_default_file_icon (void)
{
    static GIcon *fallback_icon = NULL;
    if (fallback_icon == NULL)
    {
        fallback_icon = g_themed_icon_new ("text-x-generic");
    }

    return fallback_icon;
}

GFilesystemPreviewType
nautilus_file_get_filesystem_use_preview (NautilusFile *file)
{
    GFilesystemPreviewType use_preview;
    NautilusFile *parent;

    parent = nautilus_file_get_parent (file);
    if (parent != NULL)
    {
        use_preview = parent->details->filesystem_use_preview;
        g_object_unref (parent);
    }
    else
    {
        use_preview = 0;
    }

    return use_preview;
}

char *
nautilus_file_get_filesystem_type (NautilusFile *file)
{
    char *filesystem_type = NULL;

    g_assert (NAUTILUS_IS_FILE (file));

    if (nautilus_file_is_directory (file))
    {
        filesystem_type = g_strdup (file->details->filesystem_type);
    }
    else
    {
        g_autoptr (NautilusFile) parent = NULL;

        parent = nautilus_file_get_parent (file);
        if (parent != NULL)
        {
            filesystem_type = g_strdup (parent->details->filesystem_type);
        }
    }

    return filesystem_type;
}

gboolean
nautilus_file_get_filesystem_remote (NautilusFile *file)
{
    g_assert (NAUTILUS_IS_FILE (file));

    if (nautilus_file_is_directory (file) && file->details->filesystem_info_is_up_to_date)
    {
        return file->details->filesystem_remote;
    }
    else
    {
        g_autoptr (NautilusFile) parent = NULL;

        parent = nautilus_file_get_parent (file);
        if (parent != NULL)
        {
            return parent->details->filesystem_remote;
        }
    }

    return FALSE;
}

static gboolean
get_speed_tradeoff_preference_for_file (NautilusFile               *file,
                                        NautilusSpeedTradeoffValue  value)
{
    GFilesystemPreviewType use_preview;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    use_preview = nautilus_file_get_filesystem_use_preview (file);

    if (value == NAUTILUS_SPEED_TRADEOFF_ALWAYS)
    {
        if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_NEVER)
        {
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }
    else if (value == NAUTILUS_SPEED_TRADEOFF_NEVER)
    {
        return FALSE;
    }
    else if (value == NAUTILUS_SPEED_TRADEOFF_LOCAL_ONLY)
    {
        if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_NEVER)
        {
            /* file system says to never preview anything */
            return FALSE;
        }
        else if (use_preview == G_FILESYSTEM_PREVIEW_TYPE_IF_LOCAL)
        {
            /* file system says we should treat file as if it's local */
            return TRUE;
        }
        else
        {
            /* only local files */
            return !nautilus_file_is_remote (file);
        }
    }

    return FALSE;
}

gboolean
nautilus_file_should_show_thumbnail (NautilusFile *file)
{
    const char *mime_type;

    mime_type = file->details->mime_type;
    if (mime_type == NULL)
    {
        mime_type = "application/octet-stream";
    }

    /* If the thumbnail has already been created, don't care about the size
     * of the original file.
     */
    if (nautilus_thumbnail_is_mimetype_limited_by_size (mime_type) &&
        file->details->thumbnail_path == NULL &&
        nautilus_file_get_size (file) > cached_thumbnail_limit)
    {
        return FALSE;
    }

    if (show_file_thumbs != NAUTILUS_SPEED_TRADEOFF_NEVER &&
        file->details->has_preview_icon)
    {
        /* The thumbnail should be generated if the preview icon is available
         * regardless of the filesystem type (i.e. for MTP/GPhoto2 backends).
         */
        return TRUE;
    }

    return get_speed_tradeoff_preference_for_file (file, show_file_thumbs);
}

static gboolean
nautilus_is_video_file (NautilusFile *file)
{
    const char *mime_type;
    guint i;

    mime_type = file->details->mime_type;
    if (mime_type == NULL)
    {
        return FALSE;
    }

    for (i = 0; video_mime_types[i] != NULL; i++)
    {
        if (g_content_type_equals (video_mime_types[i], mime_type))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static GList *
sort_keyword_list_and_remove_duplicates (GList *keywords)
{
    GList *p;
    GList *duplicate_link;

    if (keywords != NULL)
    {
        keywords = g_list_sort (keywords, (GCompareFunc) g_utf8_collate);

        p = keywords;
        while (p->next != NULL)
        {
            if (strcmp ((const char *) p->data, (const char *) p->next->data) == 0)
            {
                duplicate_link = p->next;
                keywords = g_list_remove_link (keywords, duplicate_link);
                g_list_free_full (duplicate_link, g_free);
            }
            else
            {
                p = p->next;
            }
        }
    }

    return keywords;
}

static void
clean_up_metadata_keywords (NautilusFile  *file,
                            GList        **metadata_keywords)
{
    NautilusFile *parent_file;
    GList *l, *res = NULL;
    char *exclude[4];
    char *keyword;
    gboolean found;
    gint i;

    i = 0;
    exclude[i++] = NAUTILUS_FILE_EMBLEM_NAME_TRASH;
    exclude[i++] = NAUTILUS_FILE_EMBLEM_NAME_NOTE;

    parent_file = nautilus_file_get_parent (file);
    if (parent_file)
    {
        if (!nautilus_file_can_write (parent_file))
        {
            exclude[i++] = NAUTILUS_FILE_EMBLEM_NAME_CANT_WRITE;
        }
        nautilus_file_unref (parent_file);
    }
    exclude[i++] = NULL;

    for (l = *metadata_keywords; l != NULL; l = l->next)
    {
        keyword = l->data;
        found = FALSE;

        for (i = 0; exclude[i] != NULL; i++)
        {
            if (strcmp (exclude[i], keyword) == 0)
            {
                found = TRUE;
                break;
            }
        }

        if (!found)
        {
            res = g_list_prepend (res, keyword);
        }
    }

    g_list_free (*metadata_keywords);
    *metadata_keywords = res;
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
static GList *
nautilus_file_get_keywords (NautilusFile *file)
{
    GList *keywords;
    gchar **metadata_strv;
    GList *metadata_keywords = NULL;

    if (file == NULL)
    {
        return NULL;
    }

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    keywords = g_list_copy_deep (file->details->extension_emblems, (GCopyFunc) g_strdup, NULL);
    keywords = g_list_concat (keywords, g_list_copy_deep (file->details->pending_extension_emblems, (GCopyFunc) g_strdup, NULL));

    metadata_strv = nautilus_file_get_metadata_list (file, NAUTILUS_METADATA_KEY_EMBLEMS);
    /* Convert array to list */
    for (gint i = 0; metadata_strv != NULL && metadata_strv[i] != NULL; i++)
    {
        metadata_keywords = g_list_prepend (metadata_keywords, metadata_strv[i]);
    }
    /* Free only the container array. The strings are owned by the list now. */
    g_free (metadata_strv);

    clean_up_metadata_keywords (file, &metadata_keywords);
    keywords = g_list_concat (keywords, metadata_keywords);

    return sort_keyword_list_and_remove_duplicates (keywords);
}

/**
 * nautilus_file_get_emblem_icons
 *
 * Return the list of names of emblems that this file should display,
 * in canonical order.
 * @file: NautilusFile representing the file in question.
 *
 * Returns: (transfer full) (element-type GIcon): A list of emblem names.
 *
 **/
GList *
nautilus_file_get_emblem_icons (NautilusFile *file)
{
    GList *keywords, *l;
    GList *icons;
    char *icon_names[2];
    char *keyword;
    GIcon *icon;

    if (file == NULL)
    {
        return NULL;
    }

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    keywords = nautilus_file_get_keywords (file);
    keywords = prepend_automatic_keywords (file, keywords);

    icons = NULL;
    for (l = keywords; l != NULL; l = l->next)
    {
        keyword = l->data;

        icon_names[0] = g_strconcat ("emblem-", keyword, NULL);
        icon_names[1] = keyword;
        icon = g_themed_icon_new_from_names (icon_names, 2);
        g_free (icon_names[0]);

        icons = g_list_prepend (icons, icon);
    }

    icon = get_mount_icon (file);
    if (icon != NULL)
    {
        icons = g_list_prepend (icons, icon);
    }

    g_list_free_full (keywords, g_free);

    return icons;
}

GIcon *
nautilus_file_get_gicon (NautilusFile          *file,
                         NautilusFileIconFlags  flags)
{
    GIcon *icon;

    if (file == NULL)
    {
        return NULL;
    }

    icon = get_custom_icon (file);
    if (icon != NULL)
    {
        return icon;
    }

    if (flags & NAUTILUS_FILE_ICON_FLAGS_USE_MOUNT_ICON)
    {
        icon = get_mount_icon (file);

        if (icon != NULL)
        {
            goto out;
        }
    }

    if (file->details->icon)
    {
        icon = g_object_ref (file->details->icon);
    }

out:
    if (icon == NULL)
    {
        icon = g_object_ref (get_default_file_icon ());
    }

    return icon;
}

char *
nautilus_file_get_thumbnail_path (NautilusFile *file)
{
    return g_strdup (file->details->thumbnail_path);
}

static NautilusIconInfo *
nautilus_file_get_thumbnail_icon (NautilusFile          *file,
                                  int                    size,
                                  int                    scale,
                                  NautilusFileIconFlags  flags)
{
    g_autoptr (GdkPaintable) paintable = NULL;
    NautilusIconInfo *icon;

    icon = NULL;

    if (file->details->thumbnail != NULL)
    {
        GdkPixbuf *pixbuf = file->details->thumbnail;
        double width = gdk_pixbuf_get_width (pixbuf) / scale;
        double height = gdk_pixbuf_get_height (pixbuf) / scale;
        g_autoptr (GdkTexture) texture = gdk_texture_new_for_pixbuf (pixbuf);
        g_autoptr (GtkSnapshot) snapshot = gtk_snapshot_new ();
        GskRoundedRect rounded_rect;

        if (MAX (width, height) > size)
        {
            float scale_down_factor = MAX (width, height) / size;

            width = width / scale_down_factor;
            height = height / scale_down_factor;
        }

        gsk_rounded_rect_init_from_rect (&rounded_rect,
                                         &GRAPHENE_RECT_INIT (0, 0, width, height),
                                         2 /* radius*/);
        gtk_snapshot_push_rounded_clip (snapshot, &rounded_rect);

        gdk_paintable_snapshot (GDK_PAINTABLE (texture),
                                GDK_SNAPSHOT (snapshot),
                                width, height);

        if (size >= NAUTILUS_GRID_ICON_SIZE_SMALL &&
            nautilus_is_video_file (file))
        {
            nautilus_ui_frame_video (snapshot, width, height);
        }

        gtk_snapshot_pop (snapshot); /* End rounded clip */

        DEBUG ("Returning thumbnailed image, at size %d %d",
               (int) (width), (int) (height));
        paintable = gtk_snapshot_to_paintable (snapshot, NULL);
    }
    else if (file->details->thumbnail_path == NULL &&
             file->details->can_read &&
             !file->details->is_thumbnailing &&
             !file->details->thumbnailing_failed &&
             nautilus_can_thumbnail (file))
    {
        nautilus_create_thumbnail (file);
    }

    if (paintable != NULL)
    {
        icon = nautilus_icon_info_new_for_paintable (paintable, scale);
    }
    else if (file->details->is_thumbnailing)
    {
        g_autoptr (GIcon) gicon = g_themed_icon_new (ICON_NAME_THUMBNAIL_LOADING);
        icon = nautilus_icon_info_lookup (gicon, size, scale);
    }

    return icon;
}

NautilusIconInfo *
nautilus_file_get_icon (NautilusFile          *file,
                        int                    size,
                        int                    scale,
                        NautilusFileIconFlags  flags)
{
    NautilusIconInfo *icon;
    GIcon *gicon;

    icon = NULL;

    if (file == NULL)
    {
        goto out;
    }

    gicon = get_custom_icon (file);
    if (gicon != NULL)
    {
        icon = nautilus_icon_info_lookup (gicon, size, scale);
        g_object_unref (gicon);

        goto out;
    }

    DEBUG ("Called file_get_icon(), at size %d", size);

    if (flags & NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS &&
        nautilus_file_should_show_thumbnail (file) &&
        size >= NAUTILUS_THUMBNAIL_MINIMUM_ICON_SIZE)
    {
        icon = nautilus_file_get_thumbnail_icon (file, size, scale, flags);
    }

    if (icon == NULL)
    {
        gicon = nautilus_file_get_gicon (file, flags);
        icon = nautilus_icon_info_lookup (gicon, size, scale);
        g_object_unref (gicon);

        if (nautilus_icon_info_is_fallback (icon))
        {
            g_object_unref (icon);
            icon = nautilus_icon_info_lookup (get_default_file_icon (), size, scale);
        }
    }

out:
    return icon;
}

GdkTexture *
nautilus_file_get_icon_texture (NautilusFile          *file,
                                int                    size,
                                int                    scale,
                                NautilusFileIconFlags  flags)
{
    g_autoptr (NautilusIconInfo) info = NULL;

    info = nautilus_file_get_icon (file, size, scale, flags);

    return nautilus_icon_info_get_texture (info);
}

GdkPaintable *
nautilus_file_get_icon_paintable (NautilusFile          *file,
                                  int                    size,
                                  int                    scale,
                                  NautilusFileIconFlags  flags)
{
    g_autoptr (NautilusIconInfo) info = NULL;

    info = nautilus_file_get_icon (file, size, scale, flags);

    return nautilus_icon_info_get_paintable (info);
}

gboolean
nautilus_file_get_date (NautilusFile     *file,
                        NautilusDateType  date_type,
                        time_t           *date)
{
    if (date != NULL)
    {
        *date = 0;
    }

    g_return_val_if_fail (date_type == NAUTILUS_DATE_TYPE_ACCESSED
                          || date_type == NAUTILUS_DATE_TYPE_MODIFIED
                          || date_type == NAUTILUS_DATE_TYPE_CREATED
                          || date_type == NAUTILUS_DATE_TYPE_TRASHED
                          || date_type == NAUTILUS_DATE_TYPE_RECENCY,
                          FALSE);

    if (file == NULL)
    {
        return FALSE;
    }

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    return NAUTILUS_FILE_CLASS (G_OBJECT_GET_CLASS (file))->get_date (file, date_type, date);
}

static char *
nautilus_file_get_where_string (NautilusFile *file)
{
    if (file == NULL)
    {
        return NULL;
    }

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    return NAUTILUS_FILE_CLASS (G_OBJECT_GET_CLASS (file))->get_where_string (file);
}

static char *
nautilus_file_get_trash_original_file_parent_as_string (NautilusFile *file)
{
    NautilusFile *orig_file;
    char *filename;

    filename = NULL;
    orig_file = nautilus_file_get_trash_original_file (file);
    if (orig_file != NULL)
    {
        filename = nautilus_file_get_parent_uri_for_display (orig_file);

        nautilus_file_unref (orig_file);
    }

    return filename;
}

G_DEFINE_AUTO_CLEANUP_FREE_FUNC (locale_t, freelocale, (locale_t) 0)

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
nautilus_file_get_date_as_string (NautilusFile       *file,
                                  NautilusDateType    date_type,
                                  NautilusDateFormat  date_format)
{
    const char *time_locale;
    locale_t current_locale;
    g_auto (locale_t) forced_locale = (locale_t) 0;
    time_t file_time_raw;
    GDateTime *file_date_time, *now;
    GDateTime *today_midnight;
    gint days_ago;
    const gchar *format;
    gchar *result;
    GString *time_label;

    if (!nautilus_file_get_date (file, date_type, &file_time_raw))
    {
        return NULL;
    }

    /* We are going to pick a translatable string which defines a time format,
     * which is then used to obtain a final string for the current time.
     *
     * The time locale might be different from the language we pick translations
     * from; so, in order to avoid chimeric results (with some particles in one
     * language and other particles in another language), we need to temporarily
     * force translations to be obtained from the language corresponding to the
     * time locale. The current locale settings are saved to be restored later.
     */
    time_locale = setlocale (LC_TIME, NULL);
    current_locale = uselocale ((locale_t) 0);
    forced_locale = newlocale (LC_MESSAGES_MASK, time_locale, duplocale (current_locale));
    uselocale (forced_locale);

    file_date_time = g_date_time_new_from_unix_local (file_time_raw);
    if (date_format != NAUTILUS_DATE_FORMAT_FULL)
    {
        GTimeZone *local_tz;
        GDateTime *file_date;

        /* Re-use local time zone, because every time a new local time zone is
         * created, glib needs to check if the time zone file has changed */
        local_tz = g_date_time_get_timezone (file_date_time);

        now = g_date_time_new_now (local_tz);
        today_midnight = g_date_time_new (local_tz,
                                          g_date_time_get_year (now),
                                          g_date_time_get_month (now),
                                          g_date_time_get_day_of_month (now),
                                          0, 0, 0);

        file_date = g_date_time_new (local_tz,
                                     g_date_time_get_year (file_date_time),
                                     g_date_time_get_month (file_date_time),
                                     g_date_time_get_day_of_month (file_date_time),
                                     0, 0, 0);

        days_ago = g_date_time_difference (today_midnight, file_date) / G_TIME_SPAN_DAY;

        /* Show the word "Today" and time if date is on today */
        if (days_ago == 0)
        {
            if (use_24_hour)
            {
                /* Translators: this is the word "Today" followed by
                 * a time in 24h format. i.e. "Today 23:04" */
                /* xgettext:no-c-format */
                format = _("Today %-H:%M");
            }
            else
            {
                /* Translators: this is the word Today followed by
                 * a time in 12h format. i.e. "Today 9:04 PM" */
                /* xgettext:no-c-format */
                format = _("Today %-I:%M %p");
            }
        }
        /* Show the word "Yesterday" and time if date is on yesterday */
        else if (days_ago == 1)
        {
            if (use_24_hour)
            {
                /* Translators: this is the word Yesterday followed by
                 * a time in 24h format. i.e. "Yesterday 23:04" */
                /* xgettext:no-c-format */
                format = _("Yesterday %-H:%M");
            }
            else
            {
                /* Translators: this is the word Yesterday followed by
                 * a time in 12h format. i.e. "Yesterday 9:04 PM" */
                /* xgettext:no-c-format */
                format = _("Yesterday %-I:%M %p");
            }
        }
        else
        {
            if (date_format == NAUTILUS_DATE_FORMAT_REGULAR)
            {
                /* Translators: this is the day of the month followed by the abbreviated
                 * month name followed by the year i.e. "3 Feb 2015" */
                /* xgettext:no-c-format */
                format = _("%-e %b %Y");
            }
            else
            {
                if (use_24_hour)
                {
                    /* Translators: this is the day number followed
                     * by the abbreviated month name followed by the year followed
                     * by a time in 24h format i.e. "3 Feb 2015 23:04" */
                    /* xgettext:no-c-format */
                    format = _("%-e %b %Y %-H:%M");
                }
                else
                {
                    /* Translators: this is the day number followed
                     * by the abbreviated month name followed by the year followed
                     * by a time in 12h format i.e. "3 Feb 2015 9:04 PM" */
                    /* xgettext:no-c-format */
                    format = _("%-e %b %Y %-I:%M %p");
                }
            }
        }

        g_date_time_unref (file_date);
        g_date_time_unref (now);
        g_date_time_unref (today_midnight);
    }
    else
    {
        if (use_24_hour)
        {
            /* Translators: this is the day number followed by the full month
             * name followed by the year followed by a time in 24h format
             * with seconds i.e. "3 February 2015 23:04:00" */
            /* xgettext:no-c-format */
            format = _("%-e %B %Y %H:%M:%S");
        }
        else
        {
            /* Translators: this is the day number followed by the full month
             * name followed by the year followed by a time in 12h format
             * with seconds i.e. "3 February 2015 09:04:00 PM" */
            /* xgettext:no-c-format */
            format = _("%-e %B %Y %I:%M:%S %p");
        }
    }

    /* Restore locale settings */
    uselocale (current_locale);

    result = g_date_time_format (file_date_time, format);
    g_date_time_unref (file_date_time);

    /* Replace ":" with ratio. Replacement is done afterward because g_date_time_format
     * may fail with utf8 chars in some locales */
    time_label = g_string_new_take (g_steal_pointer (&result));
    g_string_replace (time_label, ":", "∶", 0);

    return g_string_free_and_steal (time_label);
}

static void
clock_format_changed_callback (gpointer data)
{
    gint clock_format;

    clock_format = g_settings_get_enum (gnome_interface_preferences, "clock-format");
    use_24_hour = (clock_format == G_DESKTOP_CLOCK_FORMAT_24H);
}

static void
show_directory_item_count_changed_callback (gpointer callback_data)
{
    show_directory_item_count = g_settings_get_enum (nautilus_preferences, NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS);
}

gboolean
nautilus_file_should_show_directory_item_count (NautilusFile *file)
{
    static gboolean show_directory_item_count_callback_added = FALSE;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    if (file->details->mime_type &&
        strcmp (file->details->mime_type, "x-directory/smb-share") == 0)
    {
        return FALSE;
    }

    /* Add the callback once for the life of our process */
    if (!show_directory_item_count_callback_added)
    {
        g_signal_connect_swapped (nautilus_preferences,
                                  "changed::" NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
                                  G_CALLBACK (show_directory_item_count_changed_callback),
                                  NULL);
        show_directory_item_count_callback_added = TRUE;

        /* Peek for the first time */
        show_directory_item_count_changed_callback (NULL);
    }

    return get_speed_tradeoff_preference_for_file (file, show_directory_item_count);
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
                                        guint        *count,
                                        gboolean     *count_unreadable)
{
    if (count != NULL)
    {
        *count = 0;
    }
    if (count_unreadable != NULL)
    {
        *count_unreadable = FALSE;
    }

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    if (!nautilus_file_is_directory (file))
    {
        return FALSE;
    }

    if (!nautilus_file_should_show_directory_item_count (file))
    {
        return FALSE;
    }

    return NAUTILUS_FILE_CLASS (G_OBJECT_GET_CLASS (file))->get_item_count
               (file, count, count_unreadable);
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
                               guint        *directory_count,
                               guint        *file_count,
                               guint        *unreadable_directory_count,
                               goffset      *total_size,
                               gboolean      force)
{
    if (directory_count != NULL)
    {
        *directory_count = 0;
    }
    if (file_count != NULL)
    {
        *file_count = 0;
    }
    if (unreadable_directory_count != NULL)
    {
        *unreadable_directory_count = 0;
    }
    if (total_size != NULL)
    {
        *total_size = 0;
    }

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NAUTILUS_REQUEST_DONE);

    if (!force && !nautilus_file_should_show_directory_item_count (file))
    {
        /* Set field so an existing value isn't treated as up-to-date
         * when preference changes later.
         */
        file->details->deep_counts_status = NAUTILUS_REQUEST_NOT_STARTED;
        return file->details->deep_counts_status;
    }

    return NAUTILUS_FILE_CLASS (G_OBJECT_GET_CLASS (file))->get_deep_counts
               (file, directory_count, file_count,
               unreadable_directory_count, total_size);
}

void
nautilus_file_recompute_deep_counts (NautilusFile *file)
{
    if (file->details->deep_counts_status != NAUTILUS_REQUEST_IN_PROGRESS)
    {
        file->details->deep_counts_status = NAUTILUS_REQUEST_NOT_STARTED;
        if (file->details->directory != NULL)
        {
            nautilus_directory_add_file_to_work_queue (file->details->directory, file);
            nautilus_directory_async_state_changed (file->details->directory);
        }
    }
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
    {
        return 0;
    }
    return file->details->size;
}

time_t
nautilus_file_get_mtime (NautilusFile *file)
{
    return file->details->mtime;
}

time_t
nautilus_file_get_atime (NautilusFile *file)
{
    return file->details->atime;
}

time_t
nautilus_file_get_btime (NautilusFile *file)
{
    return file->details->btime;
}

time_t
nautilus_file_get_recency (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), 0);

    return file->details->recency;
}

time_t
nautilus_file_get_trash_time (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), 0);

    return file->details->trash_time;
}

static void
set_attributes_get_info_callback (GObject      *source_object,
                                  GAsyncResult *res,
                                  gpointer      callback_data)
{
    NautilusFileOperation *op;
    GFileInfo *new_info;
    GError *error;

    op = callback_data;

    error = NULL;
    new_info = g_file_query_info_finish (G_FILE (source_object), res, &error);
    if (new_info != NULL)
    {
        if (nautilus_file_update_info (op->file, new_info))
        {
            nautilus_file_changed (op->file);
        }
        g_object_unref (new_info);
    }
    nautilus_file_operation_complete (op, NULL, error);
    if (error)
    {
        g_error_free (error);
    }
}


static void
set_attributes_callback (GObject      *source_object,
                         GAsyncResult *result,
                         gpointer      callback_data)
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

    if (res)
    {
        g_file_query_info_async (G_FILE (source_object),
                                 NAUTILUS_FILE_DEFAULT_ATTRIBUTES,
                                 0,
                                 G_PRIORITY_DEFAULT,
                                 op->cancellable,
                                 set_attributes_get_info_callback, op);
    }
    else
    {
        nautilus_file_operation_complete (op, NULL, error);
        g_error_free (error);
    }
}

void
nautilus_file_set_attributes (NautilusFile                  *file,
                              GFileInfo                     *attributes,
                              NautilusFileOperationCallback  callback,
                              gpointer                       callback_data)
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

void
nautilus_file_set_search_relevance (NautilusFile *file,
                                    gdouble       relevance)
{
    file->details->search_relevance = relevance;
}

void
nautilus_file_set_search_fts_snippet (NautilusFile *file,
                                      const gchar  *fts_snippet)
{
    file->details->fts_snippet = g_strdup (fts_snippet);
}

const gchar *
nautilus_file_get_search_fts_snippet (NautilusFile *file)
{
    return file->details->fts_snippet;
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
    g_autoptr (GFile) location = NULL;
    uid_t user_id;

    location = nautilus_file_get_location (file);

    if (file->details->uid != -1 &&
        g_file_is_native (location))
    {
        /* Check the user. */
        user_id = geteuid ();

        /* Owner is allowed to set permissions. */
        if (user_id == (uid_t) file->details->uid)
        {
            return TRUE;
        }

        /* Root is also allowed to set permissions. */
        if (user_id == 0)
        {
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
nautilus_file_set_permissions (NautilusFile                  *file,
                               guint32                        new_permissions,
                               NautilusFileOperationCallback  callback,
                               gpointer                       callback_data)
{
    GFileInfo *info;
    GError *error;

    if (!nautilus_file_can_set_permissions (file))
    {
        /* Claim that something changed even if the permission change failed.
         * This makes it easier for some clients who see the "reverting"
         * to the old permissions as "changing back".
         */
        nautilus_file_changed (file);
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                             _("Not allowed to set permissions"));
        (*callback)(file, NULL, error, callback_data);
        g_error_free (error);
        return;
    }

    /* Test the permissions-haven't-changed case explicitly
     * because we don't want to send the file-changed signal if
     * nothing changed.
     */
    if (new_permissions == file->details->permissions)
    {
        (*callback)(file, NULL, NULL, callback_data);
        return;
    }

    if (!nautilus_file_undo_manager_is_operating ())
    {
        NautilusFileUndoInfo *undo_info;

        undo_info = nautilus_file_undo_info_permissions_new (nautilus_file_get_location (file),
                                                             file->details->permissions,
                                                             new_permissions);
        nautilus_file_undo_manager_set_action (undo_info);
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

    if (!nautilus_file_can_get_selinux_context (file))
    {
        return NULL;
    }

    raw = file->details->selinux_context;

#ifdef HAVE_SELINUX
    if (selinux_raw_to_trans_context (raw, &translated) == 0)
    {
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
get_real_name (const char *name,
               const char *gecos)
{
    char *part_before_comma, *capitalized_login_name, *real_name;
    const char *locale_string;
    g_auto (GStrv) geco_parts = NULL;

    if (gecos == NULL || gecos[0] == '\0')
    {
        return NULL;
    }

    geco_parts = g_strsplit (gecos, ",", 2);
    if (geco_parts == NULL)
    {
        return NULL;
    }

    locale_string = geco_parts[0];
    if (!g_utf8_validate (locale_string, -1, NULL))
    {
        part_before_comma = g_locale_to_utf8 (locale_string, -1, NULL, NULL, NULL);
    }
    else
    {
        part_before_comma = g_strdup (locale_string);
    }

    if (!g_utf8_validate (name, -1, NULL))
    {
        g_autofree gchar *login_name = g_locale_to_utf8 (name, -1, NULL, NULL, NULL);
        capitalized_login_name = nautilus_capitalize_str (login_name);
    }
    else
    {
        capitalized_login_name = nautilus_capitalize_str (name);
    }

    if (capitalized_login_name == NULL)
    {
        real_name = part_before_comma;
    }
    else
    {
        GString *real_name_str = g_string_new_take (g_steal_pointer (&part_before_comma));
        g_string_replace (real_name_str, "&", capitalized_login_name, 0);
        real_name = g_string_free_and_steal (real_name_str);
    }


    if (g_strcmp0 (real_name, NULL) == 0
        || g_strcmp0 (name, real_name) == 0
        || g_strcmp0 (capitalized_login_name, real_name) == 0)
    {
        g_free (real_name);
        real_name = NULL;
    }

    g_free (capitalized_login_name);

    return real_name;
}

static gboolean
get_group_id_from_group_name (const char *group_name,
                              uid_t      *gid)
{
    struct group *group;

    g_assert (gid != NULL);

    group = getgrnam (group_name);

    if (group == NULL)
    {
        return FALSE;
    }

    *gid = group->gr_gid;

    return TRUE;
}

static gboolean
get_ids_from_user_name (const char *user_name,
                        uid_t      *uid,
                        uid_t      *gid)
{
    struct passwd *password_info;

    g_assert (uid != NULL || gid != NULL);

    password_info = getpwnam (user_name);

    if (password_info == NULL)
    {
        return FALSE;
    }

    if (uid != NULL)
    {
        *uid = password_info->pw_uid;
    }

    if (gid != NULL)
    {
        *gid = password_info->pw_gid;
    }

    return TRUE;
}

static gboolean
get_user_id_from_user_name (const char *user_name,
                            uid_t      *id)
{
    return get_ids_from_user_name (user_name, id, NULL);
}

static gboolean
get_id_from_digit_string (const char *digit_string,
                          uid_t      *id)
{
    long scanned_id;
    char c;

    g_assert (id != NULL);

    /* Only accept string if it has one integer with nothing
     * afterwards.
     */
    if (sscanf (digit_string, "%ld%c", &scanned_id, &c) != 1)
    {
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
 * nautilus_file_get_uid:
 *
 * Get the user id of the file's owner.
 *
 * @file: The file in question.
 *
 * Return value: (transfer none): the user id.
 */
const uid_t
nautilus_file_get_uid (NautilusFile *file)
{
    return file->details->uid;
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
    if (!nautilus_file_can_get_owner (file))
    {
        return FALSE;
    }

    /* Owner can be changed only in admin backend or by root */
    return nautilus_file_is_in_admin (file) || geteuid () == 0;
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
nautilus_file_set_owner (NautilusFile                  *file,
                         const char                    *user_name_or_id,
                         NautilusFileOperationCallback  callback,
                         gpointer                       callback_data)
{
    GError *error;
    GFileInfo *info;
    uid_t new_id;

    if (!nautilus_file_can_set_owner (file))
    {
        /* Claim that something changed even if the permission
         * change failed. This makes it easier for some
         * clients who see the "reverting" to the old owner as
         * "changing back".
         */
        nautilus_file_changed (file);
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                             _("Not allowed to set owner"));
        (*callback)(file, NULL, error, callback_data);
        g_error_free (error);
        return;
    }

    /* If no match treating user_name_or_id as name, try treating
     * it as id.
     */
    if (!get_user_id_from_user_name (user_name_or_id, &new_id)
        && !get_id_from_digit_string (user_name_or_id, &new_id))
    {
        /* Claim that something changed even if the permission
         * change failed. This makes it easier for some
         * clients who see the "reverting" to the old owner as
         * "changing back".
         */
        nautilus_file_changed (file);
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                             _("Specified owner “%s” doesn’t exist"), user_name_or_id);
        (*callback)(file, NULL, error, callback_data);
        g_error_free (error);
        return;
    }

    /* Test the owner-hasn't-changed case explicitly because we
     * don't want to send the file-changed signal if nothing
     * changed.
     */
    if (new_id == (uid_t) file->details->uid)
    {
        (*callback)(file, NULL, NULL, callback_data);
        return;
    }

    if (!nautilus_file_undo_manager_is_operating ())
    {
        NautilusFileUndoInfo *undo_info;
        char *current_owner;

        current_owner = nautilus_file_get_owner_as_string (file, FALSE);

        undo_info = nautilus_file_undo_info_ownership_new (NAUTILUS_FILE_UNDO_OP_CHANGE_OWNER,
                                                           nautilus_file_get_location (file),
                                                           current_owner,
                                                           user_name_or_id);
        nautilus_file_undo_manager_set_action (undo_info);

        g_free (current_owner);
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
 * by a dash surrounded by spaces. The caller is responsible for freeing
 * this list and its contents.
 */
GList *
nautilus_get_user_names (void)
{
    GList *list;
    char *real_name, *name;
    struct passwd *user;

    list = NULL;

    setpwent ();

    while ((user = getpwent ()) != NULL)
    {
        real_name = get_real_name (user->pw_name, user->pw_gecos);
        if (real_name != NULL && !g_str_equal (real_name, ""))
        {
            name = g_strconcat (user->pw_name, " – ", real_name, NULL);
        }
        else
        {
            name = g_strdup (user->pw_name);
        }
        g_free (real_name);
        list = g_list_prepend (list, name);
    }

    endpwent ();

    return g_list_sort (list, (GCompareFunc) g_utf8_collate);
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
 * nautilus_file_get_gid:
 *
 * Get the group id of the file's group.
 *
 * @file: The file in question.
 *
 * Return value: (transfer none): the group id.
 */
const gid_t
nautilus_file_get_gid (NautilusFile *file)
{
    return file->details->gid;
}

/**
 * nautilus_file_get_group_name:
 *
 * Get the name of the file's group. If the group has no
 * name, returns the groupid as a string.
 *
 * @file: The file in question.
 *
 * Return value: A newly-allocated string.
 **/
const char *
nautilus_file_get_group_name (NautilusFile *file)
{
    return file->details->group;
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
    if (!nautilus_file_can_get_group (file))
    {
        return FALSE;
    }

    /* Check the user. */
    user_id = geteuid ();

    /* Owner is allowed to set group (with restrictions). */
    if (user_id == (uid_t) file->details->uid)
    {
        return TRUE;
    }

    /* Root is also allowed to set group. */
    if (user_id == 0)
    {
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
    for (i = 0; i < count; i++)
    {
        group = getgrgid (gid_list[i]);
        if (group == NULL)
        {
            break;
        }

        list = g_list_prepend (list, g_strdup (group->gr_name));
    }

    return g_list_sort (list, (GCompareFunc) g_utf8_collate);
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
    {
        list = g_list_prepend (list, g_strdup (group->gr_name));
    }

    endgrent ();

    return g_list_sort (list, (GCompareFunc) g_utf8_collate);
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

    if (!nautilus_file_can_set_group (file))
    {
        return NULL;
    }

    /* Check the user. */
    user_id = geteuid ();

    if (user_id == 0)
    {
        /* Root is allowed to set group to anything. */
        result = nautilus_get_all_group_names ();
    }
    else if (user_id == (uid_t) file->details->uid)
    {
        /* Owner is allowed to set group to any that owner is member of. */
        result = nautilus_get_group_names_for_user ();
    }
    else
    {
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
nautilus_file_set_group (NautilusFile                  *file,
                         const char                    *group_name_or_id,
                         NautilusFileOperationCallback  callback,
                         gpointer                       callback_data)
{
    GError *error;
    GFileInfo *info;
    uid_t new_id;

    if (!nautilus_file_can_set_group (file))
    {
        /* Claim that something changed even if the group
         * change failed. This makes it easier for some
         * clients who see the "reverting" to the old group as
         * "changing back".
         */
        nautilus_file_changed (file);
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
                             _("Not allowed to set group"));
        (*callback)(file, NULL, error, callback_data);
        g_error_free (error);
        return;
    }

    /* If no match treating group_name_or_id as name, try treating
     * it as id.
     */
    if (!get_group_id_from_group_name (group_name_or_id, &new_id)
        && !get_id_from_digit_string (group_name_or_id, &new_id))
    {
        /* Claim that something changed even if the group
         * change failed. This makes it easier for some
         * clients who see the "reverting" to the old group as
         * "changing back".
         */
        nautilus_file_changed (file);
        error = g_error_new (G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT,
                             _("Specified group “%s” doesn’t exist"), group_name_or_id);
        (*callback)(file, NULL, error, callback_data);
        g_error_free (error);
        return;
    }

    if (new_id == (gid_t) file->details->gid)
    {
        (*callback)(file, NULL, NULL, callback_data);
        return;
    }

    if (!nautilus_file_undo_manager_is_operating ())
    {
        NautilusFileUndoInfo *undo_info;
        const char *current_group = nautilus_file_get_group_name (file);

        undo_info = nautilus_file_undo_info_ownership_new (NAUTILUS_FILE_UNDO_OP_CHANGE_GROUP,
                                                           nautilus_file_get_location (file),
                                                           current_group,
                                                           group_name_or_id);
        nautilus_file_undo_manager_set_action (undo_info);
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

    if (!nautilus_file_can_get_permissions (file))
    {
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

    if (!nautilus_file_can_get_permissions (file))
    {
        return NULL;
    }

    g_assert (NAUTILUS_IS_FILE (file));

    permissions = file->details->permissions;
    is_directory = nautilus_file_is_directory (file);
    is_link = nautilus_file_is_symbolic_link (file);

    /* We use ls conventions for displaying these three obscure flags */
    suid = permissions & S_ISUID;
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
nautilus_file_get_owner_as_string (NautilusFile *file,
                                   gboolean      include_real_name)
{
    char *user_name;

    /* Before we have info on a file, the owner is unknown. */
    if (file->details->owner == NULL &&
        file->details->owner_real == NULL)
    {
        return NULL;
    }

    if (include_real_name &&
        file->details->uid == getuid ())
    {
        /* Translators: This is a username followed by "(You)" to indicate the file is owned by the current user */
        user_name = g_strdup_printf (_("%s (You)"), file->details->owner);
    }
    else if (file->details->owner_real == NULL)
    {
        user_name = g_strdup (file->details->owner);
    }
    else if (file->details->owner == NULL)
    {
        user_name = g_strdup (file->details->owner_real);
    }
    else if (include_real_name &&
             strcmp (file->details->owner, file->details->owner_real) != 0)
    {
        user_name = g_strdup (file->details->owner_real);
    }
    else
    {
        user_name = g_strdup (file->details->owner);
    }

    return user_name;
}

static char *
format_item_count_for_display (guint    item_count,
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

    if (file == NULL)
    {
        return NULL;
    }

    g_assert (NAUTILUS_IS_FILE (file));

    if (nautilus_file_is_directory (file))
    {
        if (!nautilus_file_get_directory_item_count (file, &item_count, &count_unreadable))
        {
            return NULL;
        }
        return format_item_count_for_display (item_count, TRUE, TRUE);
    }

    if (file->details->size == -1)
    {
        return NULL;
    }
    return g_format_size (file->details->size);
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
    g_autofree char *size_str = NULL;

    if (file == NULL)
    {
        return NULL;
    }

    g_assert (NAUTILUS_IS_FILE (file));

    if (nautilus_file_is_directory (file))
    {
        if (!nautilus_file_get_directory_item_count (file, &item_count, &count_unreadable))
        {
            return NULL;
        }
        return format_item_count_for_display (item_count, TRUE, TRUE);
    }

    if (file->details->size == -1)
    {
        return NULL;
    }

    size_str = g_strdup_printf ("%'" G_GOFFSET_FORMAT, file->details->size);
    return g_strdup_printf (ngettext ("%s byte",
                                      "%s bytes",
                                      file->details->size),
                            size_str);
}


static char *
nautilus_file_get_deep_count_as_string_internal (NautilusFile *file,
                                                 gboolean      report_size,
                                                 gboolean      report_directory_count,
                                                 gboolean      report_file_count)
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

    if (file == NULL)
    {
        return NULL;
    }

    g_assert (NAUTILUS_IS_FILE (file));
    g_assert (nautilus_file_is_directory (file));

    status = nautilus_file_get_deep_counts
                 (file, &directory_count, &file_count, &unreadable_count, &total_size, FALSE);

    /* Check whether any info is available. */
    if (status == NAUTILUS_REQUEST_NOT_STARTED)
    {
        return NULL;
    }

    total_count = file_count + directory_count;

    if (total_count == 0)
    {
        switch (status)
        {
            case NAUTILUS_REQUEST_IN_PROGRESS:
            {
                /* Don't return confident "zero" until we're finished looking,
                 * because of next case.
                 */
                return NULL;
            }

            case NAUTILUS_REQUEST_DONE:
            {
                /* Don't return "zero" if we there were contents but we couldn't read them. */
                if (unreadable_count != 0)
                {
                    return NULL;
                }
            }

            default:
            {}
            break;
        }
    }

    /* Note that we don't distinguish the "everything was readable" case
     * from the "some things but not everything was readable" case here.
     * Callers can distinguish them using nautilus_file_get_deep_counts
     * directly if desired.
     */
    if (report_size)
    {
        return g_format_size (total_size);
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
 * set includes "name", "type", "detailed_type", "mime_type", "size", "deep_size", "deep_directory_count",
 * "deep_file_count", "deep_total_count", "date_modified", "date_accessed", "date_created",
 * "date_modified_full", "date_accessed_full", "date_created_full",
 * "owner", "group", "permissions", "octal_permissions", "uri", "where",
 * "link_target", "volume", "free_space", "selinux_context", "trashed_on", "trashed_on_full", "trashed_orig_path",
 * "recency"
 *
 * Returns: Newly allocated string ready to display to the user, or NULL
 * if the value is unknown or @attribute_name is not supported.
 *
 **/
char *
nautilus_file_get_string_attribute_q (NautilusFile *file,
                                      GQuark        attribute_q)
{
    char *extension_attribute;

    if (attribute_q == attribute_name_q)
    {
        return g_strdup (nautilus_file_get_display_name (file));
    }
    if (attribute_q == attribute_type_q)
    {
        return nautilus_file_get_type_as_string (file);
    }
    if (attribute_q == attribute_detailed_type_q)
    {
        return nautilus_file_get_detailed_type_as_string (file);
    }
    if (attribute_q == attribute_mime_type_q)
    {
        return nautilus_file_get_mime_type (file);
    }
    if (attribute_q == attribute_size_q)
    {
        return nautilus_file_get_size_as_string (file);
    }
    if (attribute_q == attribute_size_detail_q)
    {
        return nautilus_file_get_size_as_string_with_real_size (file);
    }
    if (attribute_q == attribute_deep_size_q)
    {
        return nautilus_file_get_deep_size_as_string (file);
    }
    if (attribute_q == attribute_deep_file_count_q)
    {
        return nautilus_file_get_deep_file_count_as_string (file);
    }
    if (attribute_q == attribute_deep_directory_count_q)
    {
        return nautilus_file_get_deep_directory_count_as_string (file);
    }
    if (attribute_q == attribute_deep_total_count_q)
    {
        return nautilus_file_get_deep_total_count_as_string (file);
    }
    if (attribute_q == attribute_trash_orig_path_q)
    {
        return nautilus_file_get_trash_original_file_parent_as_string (file);
    }
    if (attribute_q == attribute_date_modified_q)
    {
        return nautilus_file_get_date_as_string (file,
                                                 NAUTILUS_DATE_TYPE_MODIFIED,
                                                 NAUTILUS_DATE_FORMAT_REGULAR);
    }
    if (attribute_q == attribute_date_modified_full_q)
    {
        return nautilus_file_get_date_as_string (file,
                                                 NAUTILUS_DATE_TYPE_MODIFIED,
                                                 NAUTILUS_DATE_FORMAT_FULL);
    }
    if (attribute_q == attribute_date_modified_with_time_q)
    {
        return nautilus_file_get_date_as_string (file,
                                                 NAUTILUS_DATE_TYPE_MODIFIED,
                                                 NAUTILUS_DATE_FORMAT_REGULAR_WITH_TIME);
    }
    if (attribute_q == attribute_date_accessed_q)
    {
        return nautilus_file_get_date_as_string (file,
                                                 NAUTILUS_DATE_TYPE_ACCESSED,
                                                 NAUTILUS_DATE_FORMAT_REGULAR);
    }
    if (attribute_q == attribute_date_accessed_full_q)
    {
        return nautilus_file_get_date_as_string (file,
                                                 NAUTILUS_DATE_TYPE_ACCESSED,
                                                 NAUTILUS_DATE_FORMAT_FULL);
    }
    if (attribute_q == attribute_date_created_q)
    {
        return nautilus_file_get_date_as_string (file,
                                                 NAUTILUS_DATE_TYPE_CREATED,
                                                 NAUTILUS_DATE_FORMAT_REGULAR);
    }
    if (attribute_q == attribute_date_created_full_q)
    {
        return nautilus_file_get_date_as_string (file,
                                                 NAUTILUS_DATE_TYPE_CREATED,
                                                 NAUTILUS_DATE_FORMAT_FULL);
    }
    if (attribute_q == attribute_trashed_on_q)
    {
        return nautilus_file_get_date_as_string (file,
                                                 NAUTILUS_DATE_TYPE_TRASHED,
                                                 NAUTILUS_DATE_FORMAT_REGULAR);
    }
    if (attribute_q == attribute_trashed_on_full_q)
    {
        return nautilus_file_get_date_as_string (file,
                                                 NAUTILUS_DATE_TYPE_TRASHED,
                                                 NAUTILUS_DATE_FORMAT_FULL);
    }
    if (attribute_q == attribute_recency_q)
    {
        return nautilus_file_get_date_as_string (file,
                                                 NAUTILUS_DATE_TYPE_RECENCY,
                                                 NAUTILUS_DATE_FORMAT_REGULAR);
    }
    if (attribute_q == attribute_permissions_q)
    {
        return nautilus_file_get_permissions_as_string (file);
    }
    if (attribute_q == attribute_selinux_context_q)
    {
        return nautilus_file_get_selinux_context (file);
    }
    if (attribute_q == attribute_octal_permissions_q)
    {
        return nautilus_file_get_octal_permissions_as_string (file);
    }
    if (attribute_q == attribute_owner_q)
    {
        return nautilus_file_get_owner_as_string (file, TRUE);
    }
    if (attribute_q == attribute_group_q)
    {
        return g_strdup (nautilus_file_get_group_name (file));
    }
    if (attribute_q == attribute_uri_q)
    {
        return nautilus_file_get_uri (file);
    }
    if (attribute_q == attribute_where_q)
    {
        return nautilus_file_get_where_string (file);
    }
    if (attribute_q == attribute_link_target_q)
    {
        return g_strdup (nautilus_file_get_symbolic_link_target_path (file));
    }
    if (attribute_q == attribute_volume_q)
    {
        return nautilus_file_get_volume_name (file);
    }
    if (attribute_q == attribute_free_space_q)
    {
        return nautilus_file_get_volume_free_space (file);
    }

    extension_attribute = NULL;

    if (file->details->pending_extension_attributes)
    {
        extension_attribute = g_hash_table_lookup (file->details->pending_extension_attributes,
                                                   GINT_TO_POINTER (attribute_q));
    }

    if (extension_attribute == NULL && file->details->extension_attributes)
    {
        extension_attribute = g_hash_table_lookup (file->details->extension_attributes,
                                                   GINT_TO_POINTER (attribute_q));
    }

    return g_strdup (extension_attribute);
}

char *
nautilus_file_get_string_attribute (NautilusFile *file,
                                    const char   *attribute_name)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    return nautilus_file_info_get_string_attribute (NAUTILUS_FILE_INFO (file), attribute_name);
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
nautilus_file_get_string_attribute_with_default_q (NautilusFile *file,
                                                   GQuark        attribute_q)
{
    char *result;
    guint item_count;
    gboolean count_unreadable;
    NautilusRequestStatus status;

    result = nautilus_file_get_string_attribute_q (file, attribute_q);
    if (result != NULL)
    {
        return result;
    }

    /* Supply default values for the ones we know about. */
    /* FIXME bugzilla.gnome.org 40646:
     * Use hash table and switch statement or function pointers for speed?
     */
    if (attribute_q == attribute_size_q)
    {
        if (!nautilus_file_should_show_directory_item_count (file))
        {
            return g_strdup ("—");
        }
        count_unreadable = FALSE;
        if (nautilus_file_is_directory (file))
        {
            nautilus_file_get_directory_item_count (file, &item_count, &count_unreadable);
        }
        return g_strdup (count_unreadable ? "—" : "…");
    }
    if (attribute_q == attribute_deep_size_q)
    {
        status = nautilus_file_get_deep_counts (file, NULL, NULL, NULL, NULL, FALSE);
        if (status == NAUTILUS_REQUEST_DONE)
        {
            /* This means no contents at all were readable */
            return g_strdup (_("? bytes"));
        }
        return g_strdup ("…");
    }
    if (attribute_q == attribute_deep_file_count_q
        || attribute_q == attribute_deep_directory_count_q
        || attribute_q == attribute_deep_total_count_q)
    {
        status = nautilus_file_get_deep_counts (file, NULL, NULL, NULL, NULL, FALSE);
        if (status == NAUTILUS_REQUEST_DONE)
        {
            /* This means no contents at all were readable */
            return g_strdup (_("? items"));
        }
        return g_strdup ("…");
    }
    if (attribute_q == attribute_type_q
        || attribute_q == attribute_detailed_type_q
        || attribute_q == attribute_mime_type_q)
    {
        /* Translators: This about a file type. */
        return g_strdup (_("Unknown type"));
    }
    if (attribute_q == attribute_trashed_on_q)
    {
        /* If n/a */
        return g_strdup ("");
    }
    if (attribute_q == attribute_trash_orig_path_q)
    {
        /* If n/a */
        return g_strdup ("");
    }
    if (attribute_q == attribute_recency_q)
    {
        /* If n/a */
        return g_strdup ("");
    }
    if (attribute_q == attribute_starred_q)
    {
        /* If n/a */
        return g_strdup ("");
    }
    if (attribute_q == attribute_date_created_full_q)
    {
        /* If n/a */
        return g_strdup ("—");
    }

    /* Fallback, use for both unknown attributes and attributes
     * for which we have no more appropriate default.
     */
    return g_strdup (_("Unknown"));
}

char *
nautilus_file_get_string_attribute_with_default (NautilusFile *file,
                                                 const char   *attribute_name)
{
    return nautilus_file_get_string_attribute_with_default_q (file, g_quark_from_string (attribute_name));
}

gboolean
nautilus_file_is_date_sort_attribute_q (GQuark attribute_q)
{
    if (attribute_q == attribute_modification_date_q ||
        attribute_q == attribute_date_modified_q ||
        attribute_q == attribute_date_modified_full_q ||
        attribute_q == attribute_date_modified_with_time_q ||
        attribute_q == attribute_accessed_date_q ||
        attribute_q == attribute_date_accessed_q ||
        attribute_q == attribute_date_accessed_full_q ||
        attribute_q == attribute_date_created_q ||
        attribute_q == attribute_date_created_full_q ||
        attribute_q == attribute_trashed_on_q ||
        attribute_q == attribute_trashed_on_full_q ||
        attribute_q == attribute_recency_q)
    {
        return TRUE;
    }

    return FALSE;
}

struct
{
    const char *icon_name;
    const char *display_name;
} mime_type_map[] =
{
    { "application-x-executable", N_("Program") },
    { "audio-x-generic", N_("Audio") },
    { "font-x-generic", N_("Font") },
    { "image-x-generic", N_("Image") },
    { "package-x-generic", N_("Archive") },
    { "text-html", N_("Markup") },
    { "text-x-generic", N_("Text") },
    { "text-x-generic-template", N_("Text") },
    { "text-x-script", N_("Program") },
    { "video-x-generic", N_("Video") },
    { "x-office-address-book", N_("Contacts") },
    { "x-office-calendar", N_("Calendar") },
    { "x-office-document", N_("Document") },
    { "x-office-presentation", N_("Presentation") },
    { "x-office-spreadsheet", N_("Spreadsheet") },
};

/** Only returns NULL, if mime_type was NULL. */
static const char *
get_basic_type_for_mime_type (const char *mime_type)
{
    g_autofree char *icon_name = NULL;

    if (mime_type == NULL)
    {
        return NULL;
    }

    icon_name = g_content_type_get_generic_icon_name (mime_type);
    if (icon_name != NULL)
    {
        int i;

        for (i = 0; i < G_N_ELEMENTS (mime_type_map); i++)
        {
            if (strcmp (mime_type_map[i].icon_name, icon_name) == 0)
            {
                const char *basic_type = gettext (mime_type_map[i].display_name);

                if (basic_type != NULL)
                {
                    return basic_type;
                }
                break;
            }
        }
    }

    /* Refers to a file type which is known but not one of the basic types */
    return _("Other");
}

static const char *
get_common_description (NautilusFile *file)
{
    const char *mime_type;

    g_assert (NAUTILUS_IS_FILE (file));

    mime_type = file->details->mime_type;
    if (mime_type == NULL)
    {
        return NULL;
    }

    if (g_content_type_is_unknown (mime_type))
    {
        if (nautilus_file_is_executable (file))
        {
            return _("Program");
        }
        return _("Binary");
    }

    if (strcmp (mime_type, "inode/directory") == 0)
    {
        return _("Folder");
    }

    /* No common type */
    return NULL;
}

static const char *
get_description (NautilusFile *file)
{
    const char *common_description = get_common_description (file);
    if (common_description != NULL)
    {
        return common_description;
    }
    else
    {
        const char *mime_type = file->details->mime_type;

        return get_basic_type_for_mime_type (mime_type);
    }
}

static char *
get_detailed_description (NautilusFile *file)
{
    char *description;
    const char *mime_type = file->details->mime_type;
    const char *common_description = get_common_description (file);

    if (common_description != NULL)
    {
        return g_strdup (common_description);
    }

    if (mime_type == NULL)
    {
        return NULL;
    }

    description = g_content_type_get_description (mime_type);
    if (description != NULL)
    {
        return description;
    }

    return g_strdup (mime_type);
}

static char *
update_description_for_link (NautilusFile *file,
                             const char   *string)
{
    if (nautilus_file_is_symbolic_link (file))
    {
        g_assert (!nautilus_file_is_broken_symbolic_link (file));
        if (string == NULL)
        {
            return g_strdup (_("Link"));
        }
        /* Note to localizers: convert file type string for file
         * (e.g. "folder", "plain text") to file type for symbolic link
         * to that kind of file (e.g. "link to folder").
         */
        return g_strdup_printf (_("Link to %s"), string);
    }

    return g_strdup (string);
}

static char *
nautilus_file_get_type_as_string (NautilusFile *file)
{
    if (file == NULL)
    {
        return NULL;
    }

    if (nautilus_file_is_broken_symbolic_link (file))
    {
        return g_strdup (_("Link (broken)"));
    }

    return update_description_for_link (file, get_description (file));
}

static const char *
nautilus_file_get_type_as_string_no_extra_text (NautilusFile *file)
{
    if (file == NULL)
    {
        return NULL;
    }

    if (nautilus_file_is_broken_symbolic_link (file))
    {
        return _("Link (broken)");
    }

    return get_description (file);
}

static char *
nautilus_file_get_detailed_type_as_string (NautilusFile *file)
{
    g_autofree char *detailed_description;

    if (file == NULL)
    {
        return NULL;
    }

    if (nautilus_file_is_broken_symbolic_link (file))
    {
        return g_strdup (_("Link (broken)"));
    }

    detailed_description = get_detailed_description (file);
    return update_description_for_link (file, detailed_description);
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
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), G_FILE_TYPE_UNKNOWN);

    return nautilus_file_info_get_file_type (NAUTILUS_FILE_INFO (file));
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
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    return nautilus_file_info_get_mime_type (NAUTILUS_FILE_INFO (file));
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
nautilus_file_is_mime_type (NautilusFile *file,
                            const char   *mime_type)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);
    g_return_val_if_fail (mime_type != NULL, FALSE);

    return nautilus_file_info_is_mime_type (NAUTILUS_FILE_INFO (file), mime_type);
}

char *
nautilus_file_get_extension (NautilusFile *file)
{
    char *name;
    char *extension = NULL;

    name = nautilus_file_get_name (file);
    if (name != NULL)
    {
        extension = g_strdup (eel_filename_get_extension_offset (name));
        g_free (name);
    }

    return extension;
}

gboolean
nautilus_file_is_launchable (NautilusFile *file)
{
    gboolean type_can_be_executable;

    type_can_be_executable = FALSE;
    if (file->details->mime_type != NULL)
    {
        type_can_be_executable =
            g_content_type_can_be_executable (file->details->mime_type);
    }

    return type_can_be_executable &&
           nautilus_file_can_get_permissions (file) &&
           nautilus_file_can_execute (file) &&
           nautilus_file_is_executable (file) &&
           nautilus_file_is_regular_file (file);
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

GMount *
nautilus_file_get_mount (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    return nautilus_file_info_get_mount (NAUTILUS_FILE_INFO (file));
}

static void
file_mount_unmounted (GMount   *mount,
                      gpointer  data)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (data);

    nautilus_file_invalidate_attributes (file, NAUTILUS_FILE_ATTRIBUTE_MOUNT);
}

void
nautilus_file_set_mount (NautilusFile *file,
                         GMount       *mount)
{
    if (file->details->mount)
    {
        g_signal_handlers_disconnect_by_func (file->details->mount, file_mount_unmounted, file);
        g_object_unref (file->details->mount);
        file->details->mount = NULL;
    }

    if (mount)
    {
        file->details->mount = g_object_ref (mount);
        g_signal_connect_object (mount, "unmounted",
                                 G_CALLBACK (file_mount_unmounted), file, 0);
    }
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
    if (file == NULL)
    {
        return FALSE;
    }

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    /* Non-broken symbolic links return the target's type for get_file_type. */
    return nautilus_file_get_file_type (file) == G_FILE_TYPE_SYMBOLIC_LINK;
}

static void
get_fs_free_cb (GObject      *source_object,
                GAsyncResult *res,
                gpointer      user_data)
{
    NautilusFile *file;
    guint64 free_space;
    GFileInfo *info;

    file = NAUTILUS_FILE (user_data);

    free_space = (guint64) - 1;
    info = g_file_query_filesystem_info_finish (G_FILE (source_object),
                                                res, NULL);
    if (info)
    {
        if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE))
        {
            free_space = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
        }
        g_object_unref (info);
    }

    if (file->details->free_space != free_space)
    {
        file->details->free_space = free_space;
        nautilus_file_emit_changed (file);
    }

    nautilus_file_unref (file);
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
    GFile *location;
    char *res;
    time_t now;

    now = time (NULL);
    /* Update first time and then every 2 seconds */
    if (file->details->free_space_read == 0 ||
        (now - file->details->free_space_read) > 2)
    {
        file->details->free_space_read = now;
        location = nautilus_file_get_location (file);
        g_file_query_filesystem_info_async (location,
                                            G_FILE_ATTRIBUTE_FILESYSTEM_FREE,
                                            0, NULL,
                                            get_fs_free_cb,
                                            nautilus_file_ref (file));
        g_object_unref (location);
    }

    res = NULL;
    if (file->details->free_space != (guint64) - 1)
    {
        g_autofree gchar *size_string = g_format_size (file->details->free_space);

        /* Translators: This refers to available space in a folder; e.g.: 100 MB Free */
        res = g_strdup_printf (_("%s Free"), size_string);
    }

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
    if (mount)
    {
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
 * Returns: the file path of the target of the symbolic link.
 */
const char *
nautilus_file_get_symbolic_link_target_path (NautilusFile *file)
{
    if (!nautilus_file_is_symbolic_link (file))
    {
        g_warning ("File has symlink target, but  is not marked as symlink");
    }

    return file->details->symlink_name;
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

    if (!nautilus_file_is_symbolic_link (file))
    {
        g_warning ("File has symlink target, but  is not marked as symlink");
    }

    if (file->details->symlink_name == NULL)
    {
        return NULL;
    }
    else
    {
        target = NULL;

        location = nautilus_file_get_location (file);
        parent = g_file_get_parent (location);
        g_object_unref (location);
        if (parent)
        {
            target = g_file_resolve_relative_path (parent, file->details->symlink_name);
            g_object_unref (parent);
        }

        target_uri = NULL;
        if (target)
        {
            target_uri = g_file_get_uri (target);
            g_object_unref (target);
        }
        return target_uri;
    }
}

/**
 * nautilus_file_is_regular_file
 *
 * Check if this file is a regular file.
 * @file: NautilusFile representing the file in question.
 *
 * Returns: TRUE if @file is a regular file.
 *
 **/
gboolean
nautilus_file_is_regular_file (NautilusFile *file)
{
    return nautilus_file_get_file_type (file) == G_FILE_TYPE_REGULAR;
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
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    return nautilus_file_info_is_directory (NAUTILUS_FILE_INFO (file));
}

gboolean
nautilus_file_is_public_share_folder (NautilusFile *file)
{
    if (nautilus_file_is_user_special_directory (file, G_USER_DIRECTORY_PUBLIC_SHARE))
    {
        return TRUE;
    }
    if (g_strcmp0 (g_get_home_dir (), g_get_user_special_dir (G_USER_DIRECTORY_PUBLIC_SHARE)))
    {
        /* In order to match the behavior of gnome-user-share the ~/Public folder
         * is considered to be the public sharing folder when XDG_PUBLICSHARE_DIR
         * is set to the home folder. */
        g_autoptr (GFile) public_folder = g_file_new_build_filename (g_get_home_dir (), "Public", NULL);
        g_autoptr (GFile) location = nautilus_file_get_location (file);

        return g_file_equal (public_folder, location);
    }
    return FALSE;
}

/**
 * nautilus_file_is_user_special_directory
 *
 * Check if this file is a special platform directory.
 * @file: NautilusFile representing the file in question.
 * @special_directory: GUserDirectory representing the type to test for
 *
 * Returns: TRUE if @file is a special directory of the given kind.
 */
gboolean
nautilus_file_is_user_special_directory (NautilusFile   *file,
                                         GUserDirectory  special_directory)
{
    gboolean is_special_dir;
    const gchar *special_dir;

    if (nautilus_file_is_home (file))
    {
        /* A xdg-user-dir is disabled by setting it to the home directory */
        return FALSE;
    }

    special_dir = g_get_user_special_dir (special_directory);
    is_special_dir = FALSE;

    if (special_dir)
    {
        GFile *loc;
        GFile *special_gfile;

        loc = nautilus_file_get_location (file);
        special_gfile = g_file_new_for_path (special_dir);
        is_special_dir = g_file_equal (loc, special_gfile);
        g_object_unref (special_gfile);
        g_object_unref (loc);
    }

    return is_special_dir;
}

gboolean
nautilus_file_is_archive (NautilusFile *file)
{
    g_autofree char *mime_type = NULL;

    mime_type = nautilus_file_get_mime_type (file);

    return autoar_check_mime_type_supported (mime_type);
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

/**
 * nautilus_file_is_in_recent
 *
 * Check if this file is a file in Recent.
 * @file: NautilusFile representing the file in question.
 *
 * Returns: TRUE if @file is in Recent.
 *
 **/
gboolean
nautilus_file_is_in_recent (NautilusFile *file)
{
    g_assert (NAUTILUS_IS_FILE (file));

    return nautilus_directory_is_in_recent (file->details->directory);
}

/**
 * nautilus_file_is_in_starred
 *
 * Check if this file is a file in Starred.
 * @file: NautilusFile representing the file in question.
 *
 * Returns: TRUE if @file is in Starred.
 *
 **/
gboolean
nautilus_file_is_in_starred (NautilusFile *file)
{
    g_assert (NAUTILUS_IS_FILE (file));

    return nautilus_directory_is_in_starred (file->details->directory);
}

/**
 * nautilus_file_is_remote
 *
 * Check if this file is a file in a remote filesystem.
 * @file: NautilusFile representing the file in question.
 *
 * Returns: TRUE if @file is in a remote filesystem.
 *
 **/
gboolean
nautilus_file_is_remote (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    return nautilus_file_get_filesystem_remote (file);
}

/**
 * nautilus_file_is_other_locations
 *
 * Check if this file is Other Locations.
 * @file: NautilusFile representing the file in question.
 *
 * Returns: TRUE if @file is Other Locations.
 *
 **/
gboolean
nautilus_file_is_other_locations (NautilusFile *file)
{
    g_autoptr (GFile) location = NULL;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    location = nautilus_file_get_location (file);

    return nautilus_is_root_for_scheme (location, SCHEME_OTHER_LOCATIONS);
}

/**
 * nautilus_file_is_starred_location
 *
 * Check if this file is the Favorite location.
 * @file: NautilusFile representing the file in question.
 *
 * Returns: TRUE if @file is the Favorite location.
 *
 **/
gboolean
nautilus_file_is_starred_location (NautilusFile *file)
{
    g_autoptr (GFile) location = NULL;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    location = nautilus_file_get_location (file);

    return g_file_has_uri_scheme (location, SCHEME_STARRED);
}

/**
 * nautilus_file_is_in_admin
 *
 * Check if this file is using admin backend.
 * @file: NautilusFile representing the file in question.
 *
 * Returns: TRUE if @file is using admin backend.
 *
 **/
gboolean
nautilus_file_is_in_admin (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    return nautilus_directory_is_in_admin (file->details->directory);
}

GError *
nautilus_file_get_file_info_error (NautilusFile *file)
{
    if (!file->details->get_info_failed)
    {
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
    if (file == NULL)
    {
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
    if (!file->details->has_permissions)
    {
        /* File's permissions field is not valid.
         * Can't access specific permissions, so return FALSE.
         */
        return FALSE;
    }

    return file->details->can_execute;
}

char *
nautilus_file_get_filesystem_id (NautilusFile *file)
{
    return g_strdup (file->details->filesystem_id);
}

NautilusFile *
nautilus_file_get_trash_original_file (NautilusFile *file)
{
    GFile *location;
    NautilusFile *original_file;

    original_file = NULL;

    if (file->details->trash_orig_path != NULL)
    {
        location = g_file_new_for_path (file->details->trash_orig_path);
        original_file = nautilus_file_get (location);
        g_object_unref (location);
    }

    return original_file;
}

void
nautilus_file_mark_gone (NautilusFile *file)
{
    NautilusDirectory *directory;

    if (file->details->is_gone)
    {
        return;
    }

    file->details->is_gone = TRUE;

    update_links_if_target (file);

    /* Drop it from the symlink hash ! */
    remove_from_link_hash_table (file);

    /* Removing the file from the directory can result in dropping the last
     * reference, and so clearing the info then will result in a crash.
     */
    nautilus_file_clear_info (file);

    /* Let the directory know it's gone. */
    directory = file->details->directory;
    if (!nautilus_file_is_self_owned (file))
    {
        nautilus_directory_remove_file (directory, file);
    }

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

    if (nautilus_file_is_self_owned (file))
    {
        nautilus_file_emit_changed (file);
    }
    else
    {
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
nautilus_file_updated_deep_count_in_progress (NautilusFile *file)
{
    GList *link_files, *node;

    g_assert (NAUTILUS_IS_FILE (file));
    g_assert (nautilus_file_is_directory (file));

    /* Send out a signal. */
    g_signal_emit (file, signals[UPDATED_DEEP_COUNT_IN_PROGRESS], 0, file);

    /* Tell link files pointing to this object about the change. */
    link_files = get_link_files (file);
    for (node = link_files; node != NULL; node = node->next)
    {
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

    /* Send out a signal. */
    g_signal_emit (file, signals[CHANGED], 0, file);

    /* Tell link files pointing to this object about the change. */
    link_files = get_link_files (file);
    for (p = link_files; p != NULL; p = p->next)
    {
        /* Looking for directly recursive links. */
        g_autolist (NautilusFile) link_targets = NULL;
        NautilusDirectory *directory;

        /* Files can be links to themselves. */
        if (p->data == file)
        {
            continue;
        }

        link_targets = get_link_files (p->data);
        directory = nautilus_file_get_directory (p->data);

        /* Reiterating (heh) that this will break with more complex cycles.
         * Users, stop trying to break things on purpose.
         */
        if (g_list_find (link_targets, file) != NULL &&
            directory == nautilus_file_get_directory (file))
        {
            g_signal_emit (p->data, signals[CHANGED], 0, p->data);
            continue;
        }

        nautilus_file_changed (NAUTILUS_FILE (p->data));
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

    return nautilus_file_info_is_gone (NAUTILUS_FILE_INFO (file));
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
nautilus_file_check_if_ready (NautilusFile           *file,
                              NautilusFileAttributes  file_attributes)
{
    /* To be parallel with call_when_ready, return
     * TRUE for NULL file.
     */
    if (file == NULL)
    {
        return TRUE;
    }

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    return NAUTILUS_FILE_CLASS (G_OBJECT_GET_CLASS (file))->check_if_ready (file, file_attributes);
}

void
nautilus_file_call_when_ready (NautilusFile           *file,
                               NautilusFileAttributes  file_attributes,
                               NautilusFileCallback    callback,
                               gpointer                callback_data)
{
    if (file == NULL)
    {
        (*callback)(file, callback_data);
        return;
    }

    g_return_if_fail (NAUTILUS_IS_FILE (file));

    NAUTILUS_FILE_CLASS (G_OBJECT_GET_CLASS (file))->call_when_ready
        (file, file_attributes, callback, callback_data);
}

void
nautilus_file_cancel_call_when_ready (NautilusFile         *file,
                                      NautilusFileCallback  callback,
                                      gpointer              callback_data)
{
    g_return_if_fail (callback != NULL);

    if (file == NULL)
    {
        return;
    }

    g_return_if_fail (NAUTILUS_IS_FILE (file));

    NAUTILUS_FILE_CLASS (G_OBJECT_GET_CLASS (file))->cancel_call_when_ready
        (file, callback, callback_data);
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
invalidate_file_info (NautilusFile *file)
{
    file->details->file_info_is_up_to_date = FALSE;
}

static void
invalidate_thumbnail (NautilusFile *file)
{
    file->details->thumbnail_is_up_to_date = FALSE;
}

static void
invalidate_mount (NautilusFile *file)
{
    file->details->mount_is_up_to_date = FALSE;
}

void
nautilus_file_invalidate_extension_info_internal (NautilusFile *file)
{
    if (file->details->pending_info_providers)
    {
        g_list_free_full (file->details->pending_info_providers, g_object_unref);
    }

    file->details->pending_info_providers =
        nautilus_module_get_extensions_for_type (NAUTILUS_TYPE_INFO_PROVIDER);
}

void
nautilus_file_invalidate_attributes_internal (NautilusFile           *file,
                                              NautilusFileAttributes  file_attributes)
{
    Request request;

    if (file == NULL)
    {
        return;
    }

    request = nautilus_directory_set_up_request (file_attributes);

    if (REQUEST_WANTS_TYPE (request, REQUEST_DIRECTORY_COUNT))
    {
        invalidate_directory_count (file);
    }
    if (REQUEST_WANTS_TYPE (request, REQUEST_DEEP_COUNT))
    {
        invalidate_deep_counts (file);
    }
    if (REQUEST_WANTS_TYPE (request, REQUEST_MIME_LIST))
    {
        invalidate_mime_list (file);
    }
    if (REQUEST_WANTS_TYPE (request, REQUEST_FILE_INFO))
    {
        invalidate_file_info (file);
    }
    if (REQUEST_WANTS_TYPE (request, REQUEST_EXTENSION_INFO))
    {
        nautilus_file_invalidate_extension_info_internal (file);
    }
    if (REQUEST_WANTS_TYPE (request, REQUEST_THUMBNAIL))
    {
        invalidate_thumbnail (file);
    }
    if (REQUEST_WANTS_TYPE (request, REQUEST_MOUNT))
    {
        invalidate_mount (file);
    }

    /* FIXME bugzilla.gnome.org 45075: implement invalidating metadata */
}

gboolean
nautilus_file_is_thumbnailing (NautilusFile *file)
{
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    return file->details->is_thumbnailing;
}

void
nautilus_file_set_is_thumbnailing (NautilusFile *file,
                                   gboolean      is_thumbnailing)
{
    g_return_if_fail (NAUTILUS_IS_FILE (file));

    file->details->is_thumbnailing = is_thumbnailing;
}

gboolean
nautilus_file_set_thumbnail (NautilusFile *file,
                             GdkPixbuf    *pixbuf)
{
    const char *thumb_mtime_str;
    time_t thumb_mtime = 0;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    file->details->thumbnail_is_up_to_date = TRUE;
    g_clear_object (&file->details->thumbnail);

    if (pixbuf != NULL)
    {
        thumb_mtime_str = gdk_pixbuf_get_option (pixbuf, "tEXt::Thumb::MTime");
        if (thumb_mtime_str)
        {
            thumb_mtime = atol (thumb_mtime_str);
        }

        if (thumb_mtime == 0 ||
            thumb_mtime == file->details->mtime)
        {
            file->details->thumbnail = g_object_ref (pixbuf);
            file->details->thumbnail_mtime = thumb_mtime;
        }
        else
        {
            return FALSE;
        }
    }

    return TRUE;
}


/**
 * nautilus_file_invalidate_attributes
 *
 * Invalidate the specified attributes and force a reload.
 * @file: NautilusFile representing the file in question.
 * @file_attributes: attributes to froget.
 **/

void
nautilus_file_invalidate_attributes (NautilusFile           *file,
                                     NautilusFileAttributes  file_attributes)
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
    return NAUTILUS_FILE_ATTRIBUTE_INFO |
           NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS |
           NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
           NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_MIME_TYPES |
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
    if (!file->details->got_file_info)
    {
        g_print ("no file info \n");
    }
    else if (file->details->get_info_failed)
    {
        g_print ("failed to get file info \n");
    }
    else
    {
        g_print ("size: %ld \n", size);
        switch (file->details->type)
        {
            case G_FILE_TYPE_REGULAR:
            {
                file_kind = "regular file";
            }
            break;

            case G_FILE_TYPE_DIRECTORY:
            {
                file_kind = "folder";
            }
            break;

            case G_FILE_TYPE_SPECIAL:
            {
                file_kind = "special";
            }
            break;

            case G_FILE_TYPE_SYMBOLIC_LINK:
            {
                file_kind = "symbolic link";
            }
            break;

            case G_FILE_TYPE_UNKNOWN:
            default:
            {
                file_kind = "unknown";
            }
            break;
        }
        g_print ("kind: %s \n", file_kind);
        if (file->details->type == G_FILE_TYPE_SYMBOLIC_LINK)
        {
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

/**
 * nautilus_file_get_default_sort_type:
 * @file: A #NautilusFile representing a location
 * @reversed: (out): Location to store whether the order is reversed by default.
 *
 * Gets which sort order applies by default for the provided locations.
 *
 * If @file is a location with special needs (e.g. Trash or Recent), the return
 * value and @reversed flag are dictated by design. Otherwise, they stem from
 * the "default-sort-order" and "default-sort-in-reverse-order" preference keys.
 *
 * Returns: The default #NautilusFileSortType for this @file.
 */
NautilusFileSortType
nautilus_file_get_default_sort_type (NautilusFile *file,
                                     gboolean     *reversed)
{
    g_assert (reversed != NULL);

    /* Special handling for certain directories */
    if (nautilus_file_is_user_special_directory (file, G_USER_DIRECTORY_DOWNLOAD))
    {
        *reversed = TRUE;
        return NAUTILUS_FILE_SORT_BY_MTIME;
    }
    else if (nautilus_file_is_in_trash (file))
    {
        *reversed = TRUE;
        return NAUTILUS_FILE_SORT_BY_TRASHED_TIME;
    }
    else if (nautilus_file_is_in_recent (file))
    {
        *reversed = TRUE;
        return NAUTILUS_FILE_SORT_BY_RECENCY;
    }
    else if (nautilus_file_is_in_search (file))
    {
        *reversed = TRUE;
        return NAUTILUS_FILE_SORT_BY_SEARCH_RELEVANCE;
    }

    /* Use defaults */
    *reversed = g_settings_get_boolean (nautilus_preferences,
                                        NAUTILUS_PREFERENCES_DEFAULT_SORT_IN_REVERSE_ORDER);

    return g_settings_get_enum (nautilus_preferences,
                                NAUTILUS_PREFERENCES_DEFAULT_SORT_ORDER);
}

const char *
nautilus_file_sort_type_get_attribute (NautilusFileSortType sort_type)
{
    GQuark sort_q = 0;

    switch (sort_type)
    {
        case NAUTILUS_FILE_SORT_BY_DISPLAY_NAME:
        {
            sort_q = attribute_name_q;
        }
        break;

        case NAUTILUS_FILE_SORT_BY_SIZE:
        {
            sort_q = attribute_size_q;
        }
        break;

        case NAUTILUS_FILE_SORT_BY_TYPE:
        {
            sort_q = attribute_type_q;
        }
        break;

        case NAUTILUS_FILE_SORT_BY_MTIME:
        {
            sort_q = attribute_date_modified_q;
        }
        break;

        case NAUTILUS_FILE_SORT_BY_ATIME:
        {
            sort_q = attribute_date_accessed_q;
        }
        break;

        case NAUTILUS_FILE_SORT_BY_BTIME:
        {
            sort_q = attribute_date_created_q;
        }
        break;

        case NAUTILUS_FILE_SORT_BY_TRASHED_TIME:
        {
            sort_q = attribute_trashed_on_q;
        }
        break;

        case NAUTILUS_FILE_SORT_BY_SEARCH_RELEVANCE:
        {
            sort_q = attribute_search_relevance_q;
        }
        break;

        case NAUTILUS_FILE_SORT_BY_RECENCY:
        {
            sort_q = attribute_recency_q;
        }
        break;

        case NAUTILUS_FILE_SORT_BY_STARRED:
        {
            sort_q = attribute_starred_q;
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
        break;
    }

    g_assert (sort_q != 0);

    return g_quark_to_string (sort_q);
}

static int
compare_by_display_name_cover (gconstpointer a,
                               gconstpointer b)
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
    if (l != NULL)
    {
        ready_data_list = g_list_delete_link (ready_data_list, l);

        nautilus_file_list_free (data->file_list);
        g_list_free (data->remaining_files);
        g_free (data);
    }
}

static FileListReadyData *
file_list_ready_data_new (GList                    *file_list,
                          NautilusFileListCallback  callback,
                          gpointer                  callback_data)
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
                               gpointer      user_data)
{
    FileListReadyData *data;

    data = user_data;
    data->remaining_files = g_list_remove (data->remaining_files, file);

    if (data->remaining_files == NULL)
    {
        if (data->callback)
        {
            (*data->callback)(data->file_list, data->callback_data);
        }

        file_list_ready_data_free (data);
    }
}

void
nautilus_file_list_call_when_ready (GList                     *file_list,
                                    NautilusFileAttributes     attributes,
                                    NautilusFileListHandle   **handle,
                                    NautilusFileListCallback   callback,
                                    gpointer                   callback_data)
{
    GList *l;
    FileListReadyData *data;
    NautilusFile *file;

    g_return_if_fail (file_list != NULL);

    data = file_list_ready_data_new
               (file_list, callback, callback_data);

    if (handle)
    {
        *handle = (NautilusFileListHandle *) data;
    }


    l = file_list;
    while (l != NULL)
    {
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
    if (l != NULL)
    {
        for (l = data->remaining_files; l != NULL; l = l->next)
        {
            file = NAUTILUS_FILE (l->data);

            NAUTILUS_FILE_CLASS (G_OBJECT_GET_CLASS (file))->cancel_call_when_ready
                (file, file_list_file_ready_callback, data);
        }

        file_list_ready_data_free (data);
    }
}

static void
update_thumbnail_limit (void)
{
    cached_thumbnail_limit = g_settings_get_uint64 (nautilus_preferences,
                                                    NAUTILUS_PREFERENCES_FILE_THUMBNAIL_LIMIT);

    /*Converts the obtained limit in MB to bytes */
    cached_thumbnail_limit *= MEGA_TO_BASE_RATE;
}

static void
thumbnail_limit_changed_callback (gpointer user_data)
{
    update_thumbnail_limit ();

    /* Tell the world that icons might have changed. We could invent a narrower-scope
     * signal to mean only "thumbnails might have changed" if this ends up being slow
     * for some reason.
     */
    emit_change_signals_for_all_files_in_all_directories ();
}

static void
update_show_thumbnails (void)
{
    show_file_thumbs = g_settings_get_enum (nautilus_preferences, NAUTILUS_PREFERENCES_SHOW_FILE_THUMBNAILS);
}

static void
show_thumbnails_changed_callback (gpointer user_data)
{
    update_show_thumbnails ();

    /* Tell the world that icons might have changed. We could invent a narrower-scope
     * signal to mean only "thumbnails might have changed" if this ends up being slow
     * for some reason.
     */
    emit_change_signals_for_all_files_in_all_directories ();
}

static void
mime_type_data_changed_callback (GObject  *signaller,
                                 gpointer  user_data)
{
    /* Tell the world that icons might have changed. We could invent a narrower-scope
     * signal to mean only "thumbnails might have changed" if this ends up being slow
     * for some reason.
     */
    emit_change_signals_for_all_files_in_all_directories ();
}

static gboolean
real_get_item_count (NautilusFile *file,
                     guint        *count,
                     gboolean     *count_unreadable)
{
    if (count_unreadable != NULL)
    {
        *count_unreadable = file->details->directory_count_failed;
    }

    if (!file->details->got_directory_count)
    {
        if (count != NULL)
        {
            *count = 0;
        }
        return FALSE;
    }

    if (count != NULL)
    {
        *count = file->details->directory_count;
    }

    return TRUE;
}

static NautilusRequestStatus
real_get_deep_counts (NautilusFile *file,
                      guint        *directory_count,
                      guint        *file_count,
                      guint        *unreadable_directory_count,
                      goffset      *total_size)
{
    GFileType type;

    type = nautilus_file_get_file_type (file);

    if (directory_count != NULL)
    {
        *directory_count = 0;
    }
    if (file_count != NULL)
    {
        *file_count = 0;
    }
    if (unreadable_directory_count != NULL)
    {
        *unreadable_directory_count = 0;
    }
    if (total_size != NULL)
    {
        *total_size = 0;
    }

    if (type != G_FILE_TYPE_DIRECTORY)
    {
        return NAUTILUS_REQUEST_DONE;
    }

    if (file->details->deep_counts_status != NAUTILUS_REQUEST_NOT_STARTED)
    {
        if (directory_count != NULL)
        {
            *directory_count = file->details->deep_directory_count;
        }
        if (file_count != NULL)
        {
            *file_count = file->details->deep_file_count;
        }
        if (unreadable_directory_count != NULL)
        {
            *unreadable_directory_count = file->details->deep_unreadable_count;
        }
        if (total_size != NULL)
        {
            *total_size = file->details->deep_size;
        }
        return file->details->deep_counts_status;
    }

    /* For directories, or before we know the type, we haven't started. */
    if (type == G_FILE_TYPE_UNKNOWN || type == G_FILE_TYPE_DIRECTORY)
    {
        return NAUTILUS_REQUEST_NOT_STARTED;
    }

    /* For other types, we are done, and the zeros are permanent. */
    return NAUTILUS_REQUEST_DONE;
}

static void
real_set_metadata (NautilusFile *file,
                   const char   *key,
                   const char   *value)
{
    /* Dummy default impl */
}

static void
real_set_metadata_as_list (NautilusFile  *file,
                           const char    *key,
                           char         **value)
{
    /* Dummy default impl */
}

static void
nautilus_file_class_init (NautilusFileClass *class)
{
    nautilus_file_info_getter = nautilus_file_get_internal;

    attribute_name_q = g_quark_from_static_string ("name");
    attribute_size_q = g_quark_from_static_string ("size");
    attribute_type_q = g_quark_from_static_string ("type");
    attribute_detailed_type_q = g_quark_from_static_string ("detailed_type");
    attribute_modification_date_q = g_quark_from_static_string ("modification_date");
    attribute_date_modified_q = g_quark_from_static_string ("date_modified");
    attribute_date_modified_full_q = g_quark_from_static_string ("date_modified_full");
    attribute_date_modified_with_time_q = g_quark_from_static_string ("date_modified_with_time");
    attribute_recency_q = g_quark_from_static_string ("recency");
    attribute_accessed_date_q = g_quark_from_static_string ("accessed_date");
    attribute_date_accessed_q = g_quark_from_static_string ("date_accessed");
    attribute_date_accessed_full_q = g_quark_from_static_string ("date_accessed_full");
    attribute_date_created_q = g_quark_from_static_string ("date_created");
    attribute_date_created_full_q = g_quark_from_static_string ("date_created_full");
    attribute_mime_type_q = g_quark_from_static_string ("mime_type");
    attribute_size_detail_q = g_quark_from_static_string ("size_detail");
    attribute_deep_size_q = g_quark_from_static_string ("deep_size");
    attribute_deep_file_count_q = g_quark_from_static_string ("deep_file_count");
    attribute_deep_directory_count_q = g_quark_from_static_string ("deep_directory_count");
    attribute_deep_total_count_q = g_quark_from_static_string ("deep_total_count");
    attribute_search_relevance_q = g_quark_from_static_string ("search_relevance");
    attribute_trashed_on_q = g_quark_from_static_string ("trashed_on");
    attribute_trashed_on_full_q = g_quark_from_static_string ("trashed_on_full");
    attribute_trash_orig_path_q = g_quark_from_static_string ("trash_orig_path");
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
    attribute_starred_q = g_quark_from_static_string ("starred");

    G_OBJECT_CLASS (class)->finalize = finalize;
    G_OBJECT_CLASS (class)->constructor = nautilus_file_constructor;

    class->get_item_count = real_get_item_count;
    class->get_deep_counts = real_get_deep_counts;
    class->set_metadata = real_set_metadata;
    class->set_metadata_as_list = real_set_metadata_as_list;

    signals[CHANGED] =
        g_signal_new ("changed",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusFileClass, changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    signals[UPDATED_DEEP_COUNT_IN_PROGRESS] =
        g_signal_new ("updated-deep-count-in-progress",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusFileClass, updated_deep_count_in_progress),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    update_thumbnail_limit ();
    g_signal_connect_swapped (nautilus_preferences,
                              "changed::" NAUTILUS_PREFERENCES_FILE_THUMBNAIL_LIMIT,
                              G_CALLBACK (thumbnail_limit_changed_callback),
                              NULL);
    update_show_thumbnails ();
    g_signal_connect_swapped (nautilus_preferences,
                              "changed::" NAUTILUS_PREFERENCES_SHOW_FILE_THUMBNAILS,
                              G_CALLBACK (show_thumbnails_changed_callback),
                              NULL);

    g_signal_connect (nautilus_signaller_get_current (),
                      "mime-data-changed",
                      G_CALLBACK (mime_type_data_changed_callback),
                      NULL);
    clock_format_changed_callback (NULL);
    g_signal_connect_swapped (gnome_interface_preferences,
                              "changed::clock-format",
                              G_CALLBACK (clock_format_changed_callback),
                              NULL);
}

void
nautilus_file_info_providers_done (NautilusFile *file)
{
    g_list_free_full (file->details->extension_emblems, g_free);
    file->details->extension_emblems = file->details->pending_extension_emblems;
    file->details->pending_extension_emblems = NULL;

    if (file->details->extension_attributes)
    {
        g_hash_table_destroy (file->details->extension_attributes);
    }

    file->details->extension_attributes = file->details->pending_extension_attributes;
    file->details->pending_extension_attributes = NULL;

    nautilus_file_changed (file);
}

static gboolean
is_gone (NautilusFileInfo *file_info)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (file_info);

    return file->details->is_gone;
}

static char *
get_name (NautilusFileInfo *file_info)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (file_info);

    return g_strdup (file->details->name);
}

static char *
get_uri (NautilusFileInfo *file_info)
{
    NautilusFile *file;
    g_autoptr (GFile) location = NULL;

    file = NAUTILUS_FILE (file_info);
    location = nautilus_file_get_location (file);

    return g_file_get_uri (location);
}

static char *
get_parent_uri (NautilusFileInfo *file_info)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (file_info);

    if (nautilus_file_is_self_owned (file))
    {
        /* Callers expect an empty string, not a NULL. */
        return g_strdup ("");
    }

    return nautilus_directory_get_uri (file->details->directory);
}

static char *
get_uri_scheme (NautilusFileInfo *file_info)
{
    NautilusFile *file;
    g_autoptr (GFile) location = NULL;

    file = NAUTILUS_FILE (file_info);

    if (file->details->directory == NULL)
    {
        return NULL;
    }

    location = nautilus_directory_get_location (file->details->directory);
    if (location == NULL)
    {
        return NULL;
    }

    return g_file_get_uri_scheme (location);
}

static char *
get_mime_type (NautilusFileInfo *file_info)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (file_info);

    if (file->details->mime_type != NULL)
    {
        return g_strdup (file->details->mime_type);
    }

    return g_strdup ("application/octet-stream");
}

static gboolean
is_mime_type (NautilusFileInfo *file_info,
              const char       *mime_type)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (file_info);

    if (file->details->mime_type == NULL)
    {
        return FALSE;
    }

    return g_content_type_is_a (file->details->mime_type, mime_type);
}

static gboolean
is_directory (NautilusFileInfo *file_info)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (file_info);

    return nautilus_file_get_file_type (file) == G_FILE_TYPE_DIRECTORY;
}

static void
add_emblem (NautilusFileInfo *file_info,
            const char       *emblem_name)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (file_info);

    if (file->details->pending_info_providers)
    {
        file->details->pending_extension_emblems = g_list_prepend (file->details->pending_extension_emblems,
                                                                   g_strdup (emblem_name));
    }
    else
    {
        file->details->extension_emblems = g_list_prepend (file->details->extension_emblems,
                                                           g_strdup (emblem_name));
    }

    nautilus_file_changed (file);
}

static char *
get_string_attribute (NautilusFileInfo *file_info,
                      const char       *attribute_name)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (file_info);

    return nautilus_file_get_string_attribute_q (file, g_quark_from_string (attribute_name));
}

static void
add_string_attribute (NautilusFileInfo *file_info,
                      const char       *attribute_name,
                      const char       *value)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (file_info);

    if (file->details->pending_info_providers != NULL)
    {
        /* Lazily create hashtable */
        if (file->details->pending_extension_attributes == NULL)
        {
            file->details->pending_extension_attributes =
                g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                       NULL,
                                       (GDestroyNotify) g_free);
        }
        g_hash_table_insert (file->details->pending_extension_attributes,
                             GINT_TO_POINTER (g_quark_from_string (attribute_name)),
                             g_strdup (value));
    }
    else
    {
        if (file->details->extension_attributes == NULL)
        {
            file->details->extension_attributes =
                g_hash_table_new_full (g_direct_hash, g_direct_equal,
                                       NULL,
                                       (GDestroyNotify) g_free);
        }
        g_hash_table_insert (file->details->extension_attributes,
                             GINT_TO_POINTER (g_quark_from_string (attribute_name)),
                             g_strdup (value));
    }

    nautilus_file_changed (file);
}

static void
invalidate_extension_info (NautilusFileInfo *file_info)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (file_info);

    nautilus_file_invalidate_attributes (file, NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO);
}

static char *
get_activation_uri (NautilusFileInfo *file_info)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (file_info);

    if (file->details->activation_uri != NULL)
    {
        return g_strdup (file->details->activation_uri);
    }

    return nautilus_file_get_uri (file);
}

static GFileType
get_file_type (NautilusFileInfo *file_info)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (file_info);

    return file->details->type;
}

static GFile *
get_location (NautilusFileInfo *file_info)
{
    NautilusFile *file;
    g_autoptr (GFile) location = NULL;

    file = NAUTILUS_FILE (file_info);
    location = nautilus_directory_get_location (file->details->directory);

    if (nautilus_file_is_self_owned (file))
    {
        return g_object_ref (location);
    }

    return g_file_get_child (location, file->details->name);
}

static GFile *
get_parent_location (NautilusFileInfo *file_info)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (file_info);

    if (nautilus_file_is_self_owned (file))
    {
        return NULL;
    }

    return nautilus_directory_get_location (file->details->directory);
}

static NautilusFileInfo *
get_parent_info (NautilusFileInfo *file_info)
{
    NautilusFile *file;
    NautilusFile *parent_file;

    file = NAUTILUS_FILE (file_info);

    if (nautilus_file_is_self_owned (file))
    {
        return NULL;
    }

    parent_file = nautilus_directory_get_corresponding_file (file->details->directory);

    return NAUTILUS_FILE_INFO (parent_file);
}

static GMount *
get_mount (NautilusFileInfo *file_info)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (file_info);

    if (file->details->mount)
    {
        return g_object_ref (file->details->mount);
    }

    return NULL;
}

static gboolean
can_write (NautilusFileInfo *file_info)
{
    NautilusFile *file;

    file = NAUTILUS_FILE (file_info);

    return file->details->can_write;
}

static void
nautilus_file_info_iface_init (NautilusFileInfoInterface *iface)
{
    iface->is_gone = is_gone;

    iface->get_name = get_name;
    iface->get_uri = get_uri;
    iface->get_parent_uri = get_parent_uri;
    iface->get_uri_scheme = get_uri_scheme;

    iface->get_mime_type = get_mime_type;
    iface->is_mime_type = is_mime_type;
    iface->is_directory = is_directory;

    iface->add_emblem = add_emblem;
    iface->get_string_attribute = get_string_attribute;
    iface->add_string_attribute = add_string_attribute;
    iface->invalidate_extension_info = invalidate_extension_info;

    iface->get_activation_uri = get_activation_uri;

    iface->get_file_type = get_file_type;
    iface->get_location = get_location;
    iface->get_parent_location = get_parent_location;
    iface->get_parent_info = get_parent_info;
    iface->get_mount = get_mount;
    iface->can_write = can_write;
}

#if !defined (NAUTILUS_OMIT_SELF_CHECK)

void
nautilus_self_check_file (void)
{
}

#endif /* !NAUTILUS_OMIT_SELF_CHECK */
