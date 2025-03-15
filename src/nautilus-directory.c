/*
 *  nautilus-directory.c: Nautilus directory model.
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

#include "nautilus-directory-private.h"

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "nautilus-directory-notify.h"
#include "nautilus-directory-private.h"
#include "nautilus-enums.h"
#include "nautilus-file-private.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-hash-queue.h"
#include "nautilus-metadata.h"
#include "nautilus-scheme.h"
#include "nautilus-vfs-directory.h"
#include "nautilus-vfs-file.h"

enum
{
    FILES_ADDED,
    FILES_CHANGED,
    DONE_LOADING,
    LOAD_ERROR,
    LAST_SIGNAL
};

enum
{
    PROP_LOCATION = 1,
    NUM_PROPERTIES
};

static guint signals[LAST_SIGNAL];
static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

static GHashTable *directories;

static NautilusDirectory *nautilus_directory_new (GFile *location);
static void               set_directory_location (NautilusDirectory *directory,
                                                  GFile             *location);

G_DEFINE_TYPE_WITH_PRIVATE (NautilusDirectory, nautilus_directory, G_TYPE_OBJECT);

static gboolean
real_contains_file (NautilusDirectory *self,
                    NautilusFile      *file)
{
    NautilusDirectory *directory;

    directory = nautilus_file_get_directory (file);

    return directory == self;
}

static gboolean
real_are_all_files_seen (NautilusDirectory *directory)
{
    return directory->details->directory_loaded;
}

static gboolean
real_is_not_empty (NautilusDirectory *directory)
{
    return directory->details->file_list != NULL;
}

static gboolean
is_tentative (NautilusFile *file,
              gpointer      callback_data)
{
    g_assert (callback_data == NULL);

    /* Avoid returning files with !is_added, because these
     * will later be sent with the files_added signal, and a
     * user doing get_file_list + files_added monitoring will
     * then see the file twice */
    return !file->details->got_file_info || !file->details->is_added;
}

static GList *
real_get_file_list (NautilusDirectory *directory)
{
    GList *tentative_files, *non_tentative_files;

    tentative_files = nautilus_file_list_filter (directory->details->file_list,
                                                 &non_tentative_files, is_tentative, NULL);
    nautilus_file_list_free (tentative_files);

    return non_tentative_files;
}

static gboolean
real_is_editable (NautilusDirectory *directory)
{
    return TRUE;
}

static NautilusFile *
real_new_file_from_filename (NautilusDirectory *directory,
                             const char        *filename,
                             gboolean           self_owned)
{
    return NAUTILUS_FILE (g_object_new (NAUTILUS_TYPE_VFS_FILE,
                                        "directory", directory,
                                        NULL));
}

static gboolean
real_handles_location (GFile *location)
{
    /* This class is the fallback on handling any location */
    return TRUE;
}

static void
nautilus_directory_finalize (GObject *object)
{
    NautilusDirectory *directory;

    directory = NAUTILUS_DIRECTORY (object);

    g_hash_table_remove (directories, directory->details->location);

    nautilus_directory_cancel (directory);
    g_assert (directory->details->count_in_progress == NULL);

    if (g_hash_table_size (directory->details->monitor_table) != 0)
    {
        GHashTableIter iter;
        gpointer value;

        g_warning ("destroying a NautilusDirectory while it's being monitored");

        g_hash_table_iter_init (&iter, directory->details->monitor_table);
        while (g_hash_table_iter_next (&iter, NULL, &value))
        {
            GList *list = value;
            g_list_free_full (list, g_free);
        }
        g_hash_table_remove_all (directory->details->monitor_table);
    }
    g_hash_table_destroy (directory->details->monitor_table);

    if (directory->details->monitor != NULL)
    {
        nautilus_monitor_cancel (directory->details->monitor);
    }

    if (directory->details->dequeue_pending_idle_id != 0)
    {
        g_source_remove (directory->details->dequeue_pending_idle_id);
    }

    if (directory->details->call_ready_idle_id != 0)
    {
        g_source_remove (directory->details->call_ready_idle_id);
    }

    if (directory->details->location)
    {
        g_object_unref (directory->details->location);
    }

    g_assert (directory->details->file_list == NULL);
    g_hash_table_destroy (directory->details->file_hash);

    nautilus_hash_queue_destroy (directory->details->high_priority_queue);
    nautilus_hash_queue_destroy (directory->details->low_priority_queue);
    nautilus_hash_queue_destroy (directory->details->extension_queue);
    g_clear_pointer (&directory->details->call_when_ready_hash.unsatisfied, g_hash_table_unref);
    g_clear_pointer (&directory->details->call_when_ready_hash.ready, g_hash_table_unref);
    g_clear_list (&directory->details->files_changed_while_adding, g_object_unref);
    g_assert (directory->details->directory_load_in_progress == NULL);
    g_assert (directory->details->count_in_progress == NULL);
    g_assert (directory->details->dequeue_pending_idle_id == 0);
    g_list_free_full (directory->details->pending_file_info, g_object_unref);

    G_OBJECT_CLASS (nautilus_directory_parent_class)->finalize (object);
}

static void
nautilus_directory_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    NautilusDirectory *directory = NAUTILUS_DIRECTORY (object);

    switch (property_id)
    {
        case PROP_LOCATION:
        {
            set_directory_location (directory, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
nautilus_directory_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    NautilusDirectory *directory = NAUTILUS_DIRECTORY (object);

    switch (property_id)
    {
        case PROP_LOCATION:
        {
            g_value_set_object (value, directory->details->location);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
        break;
    }
}

static void
nautilus_directory_class_init (NautilusDirectoryClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    klass->contains_file = real_contains_file;
    klass->are_all_files_seen = real_are_all_files_seen;
    klass->is_not_empty = real_is_not_empty;
    klass->get_file_list = real_get_file_list;
    klass->is_editable = real_is_editable;
    klass->new_file_from_filename = real_new_file_from_filename;
    klass->handles_location = real_handles_location;

    object_class->finalize = nautilus_directory_finalize;
    object_class->set_property = nautilus_directory_set_property;
    object_class->get_property = nautilus_directory_get_property;

    signals[FILES_ADDED] =
        g_signal_new ("files-added",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusDirectoryClass, files_added),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);
    signals[FILES_CHANGED] =
        g_signal_new ("files-changed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusDirectoryClass, files_changed),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);
    signals[DONE_LOADING] =
        g_signal_new ("done-loading",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusDirectoryClass, done_loading),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
    signals[LOAD_ERROR] =
        g_signal_new ("load-error",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusDirectoryClass, load_error),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__POINTER,
                      G_TYPE_NONE, 1, G_TYPE_POINTER);

    properties[PROP_LOCATION] =
        g_param_spec_object ("location",
                             "The location",
                             "The location of this directory",
                             G_TYPE_FILE,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

    g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

static void
nautilus_directory_init (NautilusDirectory *directory)
{
    directory->details = nautilus_directory_get_instance_private (directory);
    directory->details->file_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
                                                           g_free, NULL);
    directory->details->high_priority_queue = nautilus_hash_queue_new (g_direct_hash, g_direct_equal, g_object_ref, g_object_unref);
    directory->details->low_priority_queue = nautilus_hash_queue_new (g_direct_hash, g_direct_equal, g_object_ref, g_object_unref);
    directory->details->extension_queue = nautilus_hash_queue_new (g_direct_hash, g_direct_equal, g_object_ref, g_object_unref);
    directory->details->call_when_ready_hash.unsatisfied = g_hash_table_new (NULL, NULL);
    directory->details->call_when_ready_hash.ready = g_hash_table_new (NULL, NULL);
    directory->details->monitor_table = g_hash_table_new (NULL, NULL);
}

NautilusDirectory *
nautilus_directory_ref (NautilusDirectory *directory)
{
    if (directory == NULL)
    {
        return directory;
    }

    g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

    g_object_ref (directory);
    return directory;
}

void
nautilus_directory_unref (NautilusDirectory *directory)
{
    if (directory == NULL)
    {
        return;
    }

    g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

    g_object_unref (directory);
}

static void
collect_all_directories (gpointer key,
                         gpointer value,
                         gpointer callback_data)
{
    NautilusDirectory *directory;
    GList **dirs;

    directory = NAUTILUS_DIRECTORY (value);
    dirs = callback_data;

    *dirs = g_list_prepend (*dirs, nautilus_directory_ref (directory));
}

static void
filtering_changed_callback (gpointer callback_data)
{
    g_autolist (NautilusDirectory) dirs = NULL;

    g_assert (callback_data == NULL);

    dirs = NULL;
    g_hash_table_foreach (directories, collect_all_directories, &dirs);

    /* Preference about which items to show has changed, so we
     * can't trust any of our precomputed directory counts.
     */
    for (GList *l = dirs; l != NULL; l = l->next)
    {
        NautilusDirectory *directory;

        directory = NAUTILUS_DIRECTORY (l->data);

        nautilus_directory_invalidate_count (directory);
    }
}

void
emit_change_signals_for_all_files (NautilusDirectory *directory)
{
    g_autolist (NautilusFile) files = NULL;

    files = nautilus_file_list_copy (directory->details->file_list);
    if (directory->details->as_file != NULL)
    {
        files = g_list_prepend (files, g_object_ref (directory->details->as_file));
    }

    nautilus_directory_emit_change_signals (directory, files);
}

void
emit_change_signals_for_all_files_in_all_directories (void)
{
    GList *dirs, *l;
    NautilusDirectory *directory;

    if (directories == NULL)
    {
        return;
    }

    dirs = NULL;
    g_hash_table_foreach (directories,
                          collect_all_directories,
                          &dirs);

    for (l = dirs; l != NULL; l = l->next)
    {
        directory = NAUTILUS_DIRECTORY (l->data);
        emit_change_signals_for_all_files (directory);
        nautilus_directory_unref (directory);
    }

    g_list_free (dirs);
}

static void
async_state_changed_one (gpointer key,
                         gpointer value,
                         gpointer user_data)
{
    NautilusDirectory *directory;

    g_assert (key != NULL);
    g_assert (NAUTILUS_IS_DIRECTORY (value));
    g_assert (user_data == NULL);

    directory = NAUTILUS_DIRECTORY (value);

    nautilus_directory_async_state_changed (directory);
    emit_change_signals_for_all_files (directory);
}

static void
async_data_preference_changed_callback (gpointer callback_data)
{
    g_assert (callback_data == NULL);

    /* Preference involving fetched async data has changed, so
     * we have to kick off refetching all async data, and tell
     * each file that it (might have) changed.
     */
    g_hash_table_foreach (directories, async_state_changed_one, NULL);
}

static void
add_preferences_callbacks (void)
{
    nautilus_global_preferences_init ();

    g_signal_connect_swapped (gtk_filechooser_preferences,
                              "changed::" NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
                              G_CALLBACK (filtering_changed_callback),
                              NULL);
    g_signal_connect_swapped (nautilus_preferences,
                              "changed::" NAUTILUS_PREFERENCES_SHOW_DIRECTORY_ITEM_COUNTS,
                              G_CALLBACK (async_data_preference_changed_callback),
                              NULL);
}

/**
 * nautilus_directory_get_by_uri:
 * @uri: URI of directory to get.
 *
 * Get a directory given a uri.
 * Creates the appropriate subclass given the uri mappings.
 * Returns a referenced object, not a floating one. Unref when finished.
 * If two windows are viewing the same uri, the directory object is shared.
 */
NautilusDirectory *
nautilus_directory_get_internal (GFile    *location,
                                 gboolean  create)
{
    NautilusDirectory *directory;

    /* Create the hash table first time through. */
    if (directories == NULL)
    {
        directories = g_hash_table_new (g_file_hash, (GCompareFunc) g_file_equal);
        add_preferences_callbacks ();
    }

    /* If the object is already in the hash table, look it up. */

    directory = g_hash_table_lookup (directories,
                                     location);
    if (directory != NULL)
    {
        nautilus_directory_ref (directory);
    }
    else if (create)
    {
        /* Create a new directory object instead. */
        directory = nautilus_directory_new (location);
        if (directory == NULL)
        {
            return NULL;
        }

        /* Put it in the hash table. */
        g_hash_table_insert (directories,
                             directory->details->location,
                             directory);
    }

    return directory;
}

NautilusDirectory *
nautilus_directory_get (GFile *location)
{
    if (location == NULL)
    {
        return NULL;
    }

    return nautilus_directory_get_internal (location, TRUE);
}

NautilusDirectory *
nautilus_directory_get_existing (GFile *location)
{
    if (location == NULL)
    {
        return NULL;
    }

    return nautilus_directory_get_internal (location, FALSE);
}


NautilusDirectory *
nautilus_directory_get_by_uri (const char *uri)
{
    NautilusDirectory *directory;
    GFile *location;

    if (uri == NULL)
    {
        return NULL;
    }

    location = g_file_new_for_uri (uri);

    directory = nautilus_directory_get_internal (location, TRUE);
    g_object_unref (location);
    return directory;
}

NautilusDirectory *
nautilus_directory_get_for_file (NautilusFile *file)
{
    char *uri;
    NautilusDirectory *directory;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    uri = nautilus_file_get_uri (file);
    directory = nautilus_directory_get_by_uri (uri);
    g_free (uri);
    return directory;
}

/* Returns a reffed NautilusFile object for this directory.
 */
NautilusFile *
nautilus_directory_get_corresponding_file (NautilusDirectory *directory)
{
    NautilusFile *file;

    file = nautilus_directory_get_existing_corresponding_file (directory);
    if (file == NULL)
    {
        g_autoptr (GFile) location = nautilus_directory_get_location (directory);
        file = nautilus_file_get (location);
    }

    return file;
}

/* Returns a reffed NautilusFile object for this directory, but only if the
 * NautilusFile object has already been created.
 */
NautilusFile *
nautilus_directory_get_existing_corresponding_file (NautilusDirectory *directory)
{
    NautilusFile *file;

    file = directory->details->as_file;
    if (file != NULL)
    {
        nautilus_file_ref (file);
        return file;
    }

    g_autoptr (GFile) location = nautilus_directory_get_location (directory);
    file = nautilus_file_get_existing (location);

    return file;
}

/* nautilus_directory_get_name_for_self_as_new_file:
 *
 * Get a name to display for the file representing this
 * directory. This is called only when there's no VFS
 * directory for this NautilusDirectory.
 */
char *
nautilus_directory_get_name_for_self_as_new_file (NautilusDirectory *directory)
{
    g_autofree char *directory_uri = NULL;
    g_autofree char *scheme = NULL;
    g_autofree char *hostname = NULL;

    directory_uri = nautilus_directory_get_uri (directory);

    g_uri_split (directory_uri, 0, &scheme, NULL, &hostname, NULL, NULL, NULL, NULL, NULL);
    if (hostname == NULL)
    {
        return g_steal_pointer (&directory_uri);
    }
    else if (scheme == NULL)
    {
        return g_steal_pointer (&hostname);
    }

    /* Translators: this is of the format "hostname (uri-scheme)" */
    return g_strdup_printf (_("%s (%s)"), hostname, scheme);
}

char *
nautilus_directory_get_uri (NautilusDirectory *directory)
{
    g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

    return g_file_get_uri (directory->details->location);
}

GFile *
nautilus_directory_get_location (NautilusDirectory *directory)
{
    g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

    return g_object_ref (directory->details->location);
}

NautilusFile *
nautilus_directory_new_file_from_filename (NautilusDirectory *directory,
                                           const char        *filename,
                                           gboolean           self_owned)
{
    g_assert (NAUTILUS_IS_DIRECTORY (directory));
    g_assert (filename != NULL);
    g_assert (filename[0] != '\0');

    return NAUTILUS_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->new_file_from_filename (directory,
                                                                                              filename,
                                                                                              self_owned);
}

static GList *
nautilus_directory_provider_get_all (void)
{
    GIOExtensionPoint *extension_point;
    GList *extensions;

    extension_point = g_io_extension_point_lookup (NAUTILUS_DIRECTORY_PROVIDER_EXTENSION_POINT_NAME);
    if (extension_point == NULL)
    {
        g_warning ("Directory provider extension point not registered. Did you call nautilus_ensure_extension_points()?");
    }
    extensions = g_io_extension_point_get_extensions (extension_point);

    return extensions;
}

static NautilusDirectory *
nautilus_directory_new (GFile *location)
{
    GList *extensions;
    GList *l;
    GIOExtension *gio_extension;
    GType handling_provider_type;
    gboolean handled = FALSE;
    NautilusDirectoryClass *current_provider_class;
    NautilusDirectory *handling_instance;

    extensions = nautilus_directory_provider_get_all ();

    for (l = extensions; l != NULL; l = l->next)
    {
        gio_extension = l->data;
        current_provider_class = NAUTILUS_DIRECTORY_CLASS (g_io_extension_ref_class (gio_extension));
        if (current_provider_class->handles_location (location))
        {
            handling_provider_type = g_io_extension_get_type (gio_extension);
            handled = TRUE;
            break;
        }
    }

    if (!handled)
    {
        /* This class is the fallback for any location */
        handling_provider_type = NAUTILUS_TYPE_VFS_DIRECTORY;
    }

    handling_instance = g_object_new (handling_provider_type,
                                      "location", location,
                                      NULL);


    return handling_instance;
}

/**
 * nautilus_directory_is_local_or_fuse:
 *
 * @directory: a #NautilusDirectory
 *
 * Checks whether this directory contains files with local paths. Usually, this
 * means the local path can be obtained by calling g_file_get_path(). As an
 * exception, the local URI for files in recent:// can only be obtained from the
 * G_FILE_ATTRIBUTE_STANDARD_TARGET_URI attribute.
 *
 * Returns: %TRUE if a local path is known to be obtainable for all files in
 *          this directory. Otherwise, %FALSE.
 */
gboolean
nautilus_directory_is_local_or_fuse (NautilusDirectory *directory)
{
    g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
    g_return_val_if_fail (directory->details->location, FALSE);


    if (nautilus_directory_is_in_recent (directory)
        || g_file_is_native (directory->details->location))
    {
        /* Native files have a local path by definition. The files in recent:/
         * have a local URI stored in the standard::target-uri attribute. */
        return TRUE;
    }
    else
    {
        g_autofree char *path = NULL;

        /* Non-native files may have local paths in FUSE mounts. The only way to
         * know if that's the case is to test if GIO reports a path.
         */
        path = g_file_get_path (directory->details->location);

        return (path != NULL);
    }
}

gboolean
nautilus_directory_is_in_trash (NautilusDirectory *directory)
{
    g_assert (NAUTILUS_IS_DIRECTORY (directory));

    if (directory->details->location == NULL)
    {
        return FALSE;
    }

    return g_file_has_uri_scheme (directory->details->location, SCHEME_TRASH);
}

gboolean
nautilus_directory_is_in_recent (NautilusDirectory *directory)
{
    g_assert (NAUTILUS_IS_DIRECTORY (directory));

    if (directory->details->location == NULL)
    {
        return FALSE;
    }

    return g_file_has_uri_scheme (directory->details->location, SCHEME_RECENT);
}

gboolean
nautilus_directory_is_in_starred (NautilusDirectory *directory)
{
    g_assert (NAUTILUS_IS_DIRECTORY (directory));

    if (directory->details->location == NULL)
    {
        return FALSE;
    }

    return g_file_has_uri_scheme (directory->details->location, SCHEME_STARRED);
}

gboolean
nautilus_directory_is_in_admin (NautilusDirectory *directory)
{
    g_assert (NAUTILUS_IS_DIRECTORY (directory));

    if (directory->details->location == NULL)
    {
        return FALSE;
    }

    return g_file_has_uri_scheme (directory->details->location, SCHEME_ADMIN);
}

gboolean
nautilus_directory_are_all_files_seen (NautilusDirectory *directory)
{
    g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);

    return NAUTILUS_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->are_all_files_seen (directory);
}

static void
add_to_hash_table (NautilusDirectory *directory,
                   NautilusFile      *file,
                   GList             *node)
{
    const char *name = nautilus_file_get_name (file);

    g_assert (name != NULL);
    g_assert (node != NULL);
    g_assert (g_hash_table_lookup (directory->details->file_hash,
                                   name) == NULL);
    g_hash_table_insert (directory->details->file_hash, g_strdup (name), node);
}

static GList *
extract_from_hash_table (NautilusDirectory *directory,
                         NautilusFile      *file)
{
    const char *name = nautilus_file_get_name (file);
    GList *node;

    if (name == NULL)
    {
        return NULL;
    }

    /* Find the list node in the hash table. */
    node = g_hash_table_lookup (directory->details->file_hash, name);
    g_hash_table_remove (directory->details->file_hash, name);

    return node;
}

void
nautilus_directory_add_file (NautilusDirectory *directory,
                             NautilusFile      *file)
{
    GList *node;
    gboolean add_to_work_queue;

    g_assert (NAUTILUS_IS_DIRECTORY (directory));
    g_assert (NAUTILUS_IS_FILE (file));

    /* Add to list. */
    node = g_list_prepend (directory->details->file_list, file);
    directory->details->file_list = node;

    /* Add to hash table. */
    add_to_hash_table (directory, file, node);

    directory->details->confirmed_file_count++;

    add_to_work_queue = FALSE;
    if (nautilus_directory_is_file_list_monitored (directory))
    {
        /* Ref if we are monitoring, since monitoring owns the file list. */
        nautilus_file_ref (file);
        add_to_work_queue = TRUE;
    }
    else if (nautilus_directory_has_request_for_file (directory, file))
    {
        /* We're waiting for the file in a call_when_ready. Make sure
         *  we add the file to the work queue so that said waiter won't
         *  wait forever for e.g. all files in the directory to be done */
        add_to_work_queue = TRUE;
    }

    if (add_to_work_queue)
    {
        nautilus_directory_add_file_to_work_queue (directory, file);
    }
}

void
nautilus_directory_remove_file (NautilusDirectory *directory,
                                NautilusFile      *file)
{
    GList *node;

    g_assert (NAUTILUS_IS_DIRECTORY (directory));
    g_assert (NAUTILUS_IS_FILE (file));

    /* Find the list node in the hash table. */
    node = extract_from_hash_table (directory, file);
    g_assert (node != NULL);
    g_assert (node->data == file);

    /* Remove the item from the list. */
    directory->details->file_list = g_list_remove_link
                                        (directory->details->file_list, node);
    g_list_free_1 (node);

    nautilus_directory_remove_file_from_work_queue (directory, file);

    if (!file->details->unconfirmed)
    {
        directory->details->confirmed_file_count--;
    }

    /* Unref if we are monitoring. */
    if (nautilus_directory_is_file_list_monitored (directory))
    {
        nautilus_file_unref (file);
    }
}

GList *
nautilus_directory_begin_file_name_change (NautilusDirectory *directory,
                                           NautilusFile      *file)
{
    /* Find the list node in the hash table. */
    return extract_from_hash_table (directory, file);
}

void
nautilus_directory_end_file_name_change (NautilusDirectory *directory,
                                         NautilusFile      *file,
                                         GList             *node)
{
    /* Add the list node to the hash table. */
    if (node != NULL)
    {
        add_to_hash_table (directory, file, node);
    }
}

NautilusFile *
nautilus_directory_find_file_by_name (NautilusDirectory *directory,
                                      const char        *name)
{
    GList *node;

    g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);
    g_return_val_if_fail (name != NULL, NULL);

    node = g_hash_table_lookup (directory->details->file_hash,
                                name);
    return node == NULL ? NULL : NAUTILUS_FILE (node->data);
}

void
nautilus_directory_emit_files_added (NautilusDirectory *directory,
                                     GList             *added_files)
{
    if (added_files != NULL)
    {
        g_signal_emit (directory,
                       signals[FILES_ADDED], 0,
                       added_files);
    }
}

void
nautilus_directory_emit_files_changed (NautilusDirectory *directory,
                                       GList             *changed_files)
{
    if (changed_files != NULL)
    {
        g_signal_emit (directory,
                       signals[FILES_CHANGED], 0,
                       changed_files);
    }
}

void
nautilus_directory_emit_change_signals (NautilusDirectory *directory,
                                        GList             *changed_files)
{
    for (GList *p = changed_files; p != NULL; p = p->next)
    {
        nautilus_file_emit_changed (p->data);
    }
    nautilus_directory_emit_files_changed (directory, changed_files);
}

void
nautilus_directory_emit_done_loading (NautilusDirectory *directory)
{
    g_signal_emit (directory,
                   signals[DONE_LOADING], 0);
}

void
nautilus_directory_emit_load_error (NautilusDirectory *directory,
                                    GError            *error)
{
    g_signal_emit (directory,
                   signals[LOAD_ERROR], 0,
                   error);
}

/* Return a directory object for this one's parent. */
static NautilusDirectory *
get_parent_directory (GFile *location)
{
    NautilusDirectory *directory;
    GFile *parent;

    parent = g_file_get_parent (location);
    if (parent)
    {
        directory = nautilus_directory_get_internal (parent, TRUE);
        g_object_unref (parent);
        return directory;
    }
    return NULL;
}

/* If a directory object exists for this one's parent, then
 * return it, otherwise return NULL.
 */
static NautilusDirectory *
get_parent_directory_if_exists (GFile *location)
{
    NautilusDirectory *directory;
    GFile *parent;

    parent = g_file_get_parent (location);
    if (parent)
    {
        directory = nautilus_directory_get_internal (parent, FALSE);
        g_object_unref (parent);
        return directory;
    }
    return NULL;
}

static void
hash_table_list_prepend (GHashTable    *table,
                         gconstpointer  key,
                         gpointer       data)
{
    GList *list;

    list = g_hash_table_lookup (table, key);
    list = g_list_prepend (list, data);
    g_hash_table_insert (table, (gpointer) key, list);
}

static void
call_files_added_free_list (gpointer key,
                            gpointer value,
                            gpointer user_data)
{
    g_assert (NAUTILUS_IS_DIRECTORY (key));
    g_assert (value != NULL);
    g_assert (user_data == NULL);

    g_signal_emit (key,
                   signals[FILES_ADDED], 0,
                   value);
    g_list_free (value);
}

static void
call_files_changed_common (NautilusDirectory *self,
                           GList             *file_list)
{
    GList *node;
    NautilusFile *file;

    for (node = file_list; node != NULL; node = node->next)
    {
        NautilusDirectory *directory;

        file = node->data;
        directory = nautilus_file_get_directory (file);

        if (directory == self)
        {
            nautilus_directory_add_file_to_work_queue (self, file);
        }
    }
    nautilus_directory_async_state_changed (self);
    nautilus_directory_emit_change_signals (self, file_list);
}

static void
call_files_changed_free_list (gpointer key,
                              gpointer value,
                              gpointer user_data)
{
    g_assert (value != NULL);
    g_assert (user_data == NULL);

    call_files_changed_common (NAUTILUS_DIRECTORY (key), value);
    g_list_free (value);
}

static void
call_files_changed_unref_free_list (gpointer key,
                                    gpointer value,
                                    gpointer user_data)
{
    g_assert (value != NULL);
    g_assert (user_data == NULL);

    call_files_changed_common (NAUTILUS_DIRECTORY (key), value);
    nautilus_file_list_free (value);
}

static void
call_get_file_info_free_list (gpointer key,
                              gpointer value,
                              gpointer user_data)
{
    NautilusDirectory *directory;
    GList *files;

    g_assert (NAUTILUS_IS_DIRECTORY (key));
    g_assert (value != NULL);
    g_assert (user_data == NULL);

    directory = key;
    files = value;

    nautilus_directory_get_info_for_new_files (directory, files);
    g_list_foreach (files, (GFunc) g_object_unref, NULL);
    g_list_free (files);
}

static void
invalidate_count_and_unref (gpointer key,
                            gpointer value,
                            gpointer user_data)
{
    g_assert (NAUTILUS_IS_DIRECTORY (key));
    g_assert (value == key);
    g_assert (user_data == NULL);

    nautilus_directory_invalidate_count (key);
    nautilus_directory_unref (key);
}

static void
collect_parent_directories (GHashTable        *hash_table,
                            NautilusDirectory *directory)
{
    g_assert (hash_table != NULL);
    g_assert (NAUTILUS_IS_DIRECTORY (directory));

    if (g_hash_table_lookup (hash_table, directory) == NULL)
    {
        nautilus_directory_ref (directory);
        g_hash_table_insert (hash_table, directory, directory);
    }
}

void
nautilus_directory_notify_files_added (GList *files)
{
    GHashTable *added_lists;
    GList *p;
    NautilusDirectory *directory;
    GHashTable *parent_directories;
    NautilusFile *file;
    GFile *location, *parent;

    /* Make a list of added files in each directory. */
    added_lists = g_hash_table_new (NULL, NULL);

    /* Make a list of parent directories that will need their counts updated. */
    parent_directories = g_hash_table_new (NULL, NULL);

    for (p = files; p != NULL; p = p->next)
    {
        location = p->data;

        /* See if the directory is already known. */
        directory = get_parent_directory_if_exists (location);
        if (directory == NULL)
        {
            /* In case the directory is not being
             * monitored, but the corresponding file is,
             * we must invalidate it's item count.
             */


            file = NULL;
            parent = g_file_get_parent (location);
            if (parent)
            {
                file = nautilus_file_get_existing (parent);
                g_object_unref (parent);
            }

            if (file != NULL)
            {
                nautilus_file_invalidate_count (file);
                nautilus_file_unref (file);
            }

            continue;
        }

        collect_parent_directories (parent_directories, directory);

        /* If no one is monitoring files in the directory, nothing to do. */
        if (!nautilus_directory_is_file_list_monitored (directory))
        {
            nautilus_directory_unref (directory);
            continue;
        }

        file = nautilus_file_get_existing (location);
        /* We check is_added here, because the file could have been added
         * to the directory by a nautilus_file_get() but not gotten
         * files_added emitted
         */
        if (file && file->details->is_added)
        {
            /* A file already exists, it was probably renamed.
             * If it was renamed this could be ignored, but
             * queue a change just in case */
            nautilus_file_changed (file);
        }
        else
        {
            hash_table_list_prepend (added_lists,
                                     directory,
                                     g_object_ref (location));
        }
        nautilus_file_unref (file);
        nautilus_directory_unref (directory);
    }

    /* Now get file info for the new files. This creates NautilusFile
     * objects for the new files, and sends out a files_added signal.
     */
    g_hash_table_foreach (added_lists, call_get_file_info_free_list, NULL);
    g_hash_table_destroy (added_lists);

    /* Invalidate count for each parent directory. */
    g_hash_table_foreach (parent_directories, invalidate_count_and_unref, NULL);
    g_hash_table_destroy (parent_directories);
}

void
nautilus_directory_notify_files_changed (GList *files)
{
    GHashTable *changed_lists;
    GList *node;
    GFile *location;
    g_autoptr (NautilusDirectory) dir = NULL;
    NautilusFile *file;

    /* Make a list of changed files in each directory. */
    changed_lists = g_hash_table_new (NULL, NULL);

    /* Go through all the notifications. */
    for (node = files; node != NULL; node = node->next)
    {
        location = node->data;

        /* Find the file. */
        file = nautilus_file_get_existing (location);
        if (file != NULL)
        {
            NautilusDirectory *directory;

            directory = nautilus_file_get_directory (file);

            /* Tell it to re-get info now, and later emit
             * a changed signal.
             */
            file->details->file_info_is_up_to_date = FALSE;
            nautilus_file_invalidate_extension_info_internal (file);

            hash_table_list_prepend (changed_lists, directory, file);
        }
        else
        {
            g_autoptr (GFile) parent = g_file_get_parent (location);

            dir = nautilus_directory_get_existing (parent);
            if (dir != NULL && dir->details->new_files_in_progress != NULL &&
                files != dir->details->files_changed_while_adding)
            {
                dir->details->files_changed_while_adding =
                    g_list_prepend (dir->details->files_changed_while_adding,
                                    g_object_ref (location));
            }
        }
    }

    /* Now send out the changed signals. */
    g_hash_table_foreach (changed_lists, call_files_changed_unref_free_list, NULL);
    g_hash_table_destroy (changed_lists);
}

void
nautilus_directory_mark_files_unmounted (GList *files)
{
    for (GList *l = files; l != NULL; l = l->next)
    {
        GFile *location = l->data;
        g_autoptr (NautilusFile) file = nautilus_file_get_existing (location);
        if (file != NULL)
        {
            nautilus_file_mark_unmounted (file);
        }
    }
}

void
nautilus_directory_notify_files_removed (GList *files)
{
    GHashTable *changed_lists;
    GList *p;
    GHashTable *parent_directories;
    NautilusFile *file;
    GFile *location;

    /* Make a list of changed files in each directory. */
    changed_lists = g_hash_table_new (NULL, NULL);

    /* Make a list of parent directories that will need their counts updated. */
    parent_directories = g_hash_table_new (NULL, NULL);

    /* Go through all the notifications. */
    for (p = files; p != NULL; p = p->next)
    {
        NautilusDirectory *directory;

        location = p->data;

        /* Update file count for parent directory if anyone might care. */
        directory = get_parent_directory_if_exists (location);
        if (directory != NULL)
        {
            collect_parent_directories (parent_directories, directory);
            nautilus_directory_unref (directory);
        }

        /* Find the file. */
        file = nautilus_file_get_existing (location);
        if (file != NULL && !nautilus_file_rename_in_progress (file))
        {
            directory = nautilus_file_get_directory (file);

            /* Mark it gone and prepare to send the changed signal. */
            nautilus_file_mark_gone (file);
            hash_table_list_prepend (changed_lists,
                                     directory, nautilus_file_ref (file));
        }
        nautilus_file_unref (file);
    }

    /* Now send out the changed signals. */
    g_hash_table_foreach (changed_lists, call_files_changed_unref_free_list, NULL);
    g_hash_table_destroy (changed_lists);

    /* Invalidate count for each parent directory. */
    g_hash_table_foreach (parent_directories, invalidate_count_and_unref, NULL);
    g_hash_table_destroy (parent_directories);
}

static void
set_directory_location (NautilusDirectory *directory,
                        GFile             *location)
{
    if (directory->details->location)
    {
        g_object_unref (directory->details->location);
    }
    directory->details->location = g_object_ref (location);

    g_object_notify_by_pspec (G_OBJECT (directory), properties[PROP_LOCATION]);
}

static void
change_directory_location (NautilusDirectory *directory,
                           GFile             *new_location)
{
    /* I believe it's impossible for a self-owned file/directory
     * to be moved. But if that did somehow happen, this function
     * wouldn't do enough to handle it.
     */
    g_assert (directory->details->as_file == NULL);

    g_hash_table_remove (directories,
                         directory->details->location);

    set_directory_location (directory, new_location);

    g_hash_table_insert (directories,
                         directory->details->location,
                         directory);
}

typedef struct
{
    GFile *container;
    GList *directories;
} CollectData;

static void
collect_directories_by_container (gpointer key,
                                  gpointer value,
                                  gpointer callback_data)
{
    NautilusDirectory *directory;
    CollectData *collect_data;
    GFile *location;

    location = (GFile *) key;
    directory = NAUTILUS_DIRECTORY (value);
    collect_data = (CollectData *) callback_data;

    if (g_file_has_prefix (location, collect_data->container) ||
        g_file_equal (collect_data->container, location))
    {
        nautilus_directory_ref (directory);
        collect_data->directories =
            g_list_prepend (collect_data->directories,
                            directory);
    }
}

static GList *
nautilus_directory_moved_internal (GFile *old_location,
                                   GFile *new_location)
{
    CollectData collection;
    NautilusDirectory *directory;
    GList *node, *affected_files;
    GFile *new_directory_location;
    char *relative_path;

    collection.container = old_location;
    collection.directories = NULL;

    g_hash_table_foreach (directories,
                          collect_directories_by_container,
                          &collection);

    affected_files = NULL;

    for (node = collection.directories; node != NULL; node = node->next)
    {
        directory = NAUTILUS_DIRECTORY (node->data);
        new_directory_location = NULL;

        if (g_file_equal (directory->details->location, old_location))
        {
            new_directory_location = g_object_ref (new_location);
        }
        else
        {
            relative_path = g_file_get_relative_path (old_location,
                                                      directory->details->location);
            if (relative_path != NULL)
            {
                new_directory_location = g_file_resolve_relative_path (new_location, relative_path);
                g_free (relative_path);
            }
        }

        if (new_directory_location)
        {
            change_directory_location (directory, new_directory_location);
            g_object_unref (new_directory_location);

            /* Collect affected files. */
            if (directory->details->as_file != NULL)
            {
                affected_files = g_list_prepend
                                     (affected_files,
                                     nautilus_file_ref (directory->details->as_file));
            }
            affected_files = g_list_concat
                                 (affected_files,
                                 nautilus_file_list_copy (directory->details->file_list));
        }

        nautilus_directory_unref (directory);
    }

    g_list_free (collection.directories);

    return affected_files;
}

void
nautilus_directory_moved (const char *old_uri,
                          const char *new_uri)
{
    GList *list, *node;
    GHashTable *hash;
    NautilusFile *file;
    GFile *old_location;
    GFile *new_location;

    hash = g_hash_table_new (NULL, NULL);

    old_location = g_file_new_for_uri (old_uri);
    new_location = g_file_new_for_uri (new_uri);

    list = nautilus_directory_moved_internal (old_location, new_location);
    for (node = list; node != NULL; node = node->next)
    {
        NautilusDirectory *directory;

        file = NAUTILUS_FILE (node->data);
        directory = nautilus_file_get_directory (file);

        hash_table_list_prepend (hash, directory, nautilus_file_ref (file));
    }
    nautilus_file_list_free (list);

    g_object_unref (old_location);
    g_object_unref (new_location);

    g_hash_table_foreach (hash, call_files_changed_unref_free_list, NULL);
    g_hash_table_destroy (hash);
}

void
nautilus_directory_notify_files_moved (GList *file_pairs)
{
    GList *p, *affected_files, *node;
    GFilePair *pair;
    NautilusFile *file;
    NautilusDirectory *old_directory, *new_directory;
    GHashTable *parent_directories;
    GList *new_files_list, *unref_list;
    GHashTable *added_lists, *changed_lists;
    char *name;
    NautilusFileAttributes cancel_attributes;
    GFile *to_location, *from_location;

    /* Make a list of added and changed files in each directory. */
    new_files_list = NULL;
    added_lists = g_hash_table_new (NULL, NULL);
    changed_lists = g_hash_table_new (NULL, NULL);
    unref_list = NULL;

    /* Make a list of parent directories that will need their counts updated. */
    parent_directories = g_hash_table_new (NULL, NULL);

    cancel_attributes = nautilus_file_get_all_attributes ();

    for (p = file_pairs; p != NULL; p = p->next)
    {
        pair = p->data;
        from_location = pair->from;
        to_location = pair->to;

        /* Handle overwriting a file. */
        file = nautilus_file_get_existing (to_location);
        if (file != NULL)
        {
            NautilusDirectory *directory;

            directory = nautilus_file_get_directory (file);

            /* Mark it gone and prepare to send the changed signal. */
            nautilus_file_mark_gone (file);
            hash_table_list_prepend (changed_lists, directory, file);
            collect_parent_directories (parent_directories, directory);
        }

        /* Update any directory objects that are affected. */
        affected_files = nautilus_directory_moved_internal (from_location,
                                                            to_location);
        for (node = affected_files; node != NULL; node = node->next)
        {
            NautilusDirectory *directory;

            file = NAUTILUS_FILE (node->data);
            directory = nautilus_file_get_directory (file);
            hash_table_list_prepend (changed_lists, directory, file);
        }
        unref_list = g_list_concat (unref_list, affected_files);

        /* Move an existing file. */
        file = nautilus_file_get_existing (from_location);
        if (file == NULL)
        {
            /* Handle this as if it was a new file. */
            new_files_list = g_list_prepend (new_files_list,
                                             to_location);
        }
        else
        {
            NautilusDirectory *directory;

            directory = nautilus_file_get_directory (file);

            /* Handle notification in the old directory. */
            old_directory = directory;
            collect_parent_directories (parent_directories, old_directory);

            /* Cancel loading of attributes in the old directory */
            nautilus_directory_cancel_loading_file_attributes
                (old_directory, file, cancel_attributes);

            /* Locate the new directory. */
            new_directory = get_parent_directory (to_location);
            collect_parent_directories (parent_directories, new_directory);
            /* We can unref now -- new_directory is in the
             * parent directories list so it will be
             * around until the end of this function
             * anyway.
             */
            nautilus_directory_unref (new_directory);

            /* Update the file's name and directory. */
            name = g_file_get_basename (to_location);
            nautilus_file_update_name_and_directory
                (file, name, new_directory);
            g_free (name);

            /* Update file attributes */
            nautilus_file_invalidate_attributes (file, NAUTILUS_FILE_ATTRIBUTE_INFO);

            hash_table_list_prepend (changed_lists,
                                     old_directory,
                                     file);
            if (old_directory != new_directory)
            {
                hash_table_list_prepend (added_lists,
                                         new_directory,
                                         file);
            }

            /* Unref each file once to balance out nautilus_file_get_by_uri. */
            unref_list = g_list_prepend (unref_list, file);
        }
    }

    /* Now send out the changed and added signals for existing file objects. */
    g_hash_table_foreach (changed_lists, call_files_changed_free_list, NULL);
    g_hash_table_destroy (changed_lists);
    g_hash_table_foreach (added_lists, call_files_added_free_list, NULL);
    g_hash_table_destroy (added_lists);

    /* Let the file objects go. */
    nautilus_file_list_free (unref_list);

    /* Invalidate count for each parent directory. */
    g_hash_table_foreach (parent_directories, invalidate_count_and_unref, NULL);
    g_hash_table_destroy (parent_directories);

    /* Separate handling for brand new file objects. */
    nautilus_directory_notify_files_added (new_files_list);
    g_list_free (new_files_list);
}

gboolean
nautilus_directory_contains_file (NautilusDirectory *directory,
                                  NautilusFile      *file)
{
    g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);
    g_return_val_if_fail (NAUTILUS_IS_FILE (file), FALSE);

    if (nautilus_file_is_gone (file))
    {
        return FALSE;
    }

    return NAUTILUS_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->contains_file (directory, file);
}

NautilusFile *
nautilus_directory_get_file_by_name (NautilusDirectory *directory,
                                     const gchar       *name)
{
    GList *files;
    GList *l;
    NautilusFile *result = NULL;

    files = nautilus_directory_get_file_list (directory);

    for (l = files; l != NULL; l = l->next)
    {
        if (nautilus_file_compare_display_name (l->data, name) == 0)
        {
            result = nautilus_file_ref (l->data);
            break;
        }
    }

    nautilus_file_list_free (files);

    return result;
}

void
nautilus_directory_call_when_ready (NautilusDirectory         *directory,
                                    NautilusFileAttributes     file_attributes,
                                    gboolean                   wait_for_all_files,
                                    NautilusDirectoryCallback  callback,
                                    gpointer                   callback_data)
{
    g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
    g_return_if_fail (callback != NULL);

    NAUTILUS_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->call_when_ready
        (directory, file_attributes, wait_for_all_files,
        callback, callback_data);
}

void
nautilus_directory_cancel_callback (NautilusDirectory         *directory,
                                    NautilusDirectoryCallback  callback,
                                    gpointer                   callback_data)
{
    g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
    g_return_if_fail (callback != NULL);

    NAUTILUS_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->cancel_callback
        (directory, callback, callback_data);
}

void
nautilus_directory_file_monitor_add (NautilusDirectory         *directory,
                                     gconstpointer              client,
                                     gboolean                   monitor_hidden_files,
                                     NautilusFileAttributes     file_attributes,
                                     NautilusDirectoryCallback  callback,
                                     gpointer                   callback_data)
{
    g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
    g_return_if_fail (client != NULL);

    NAUTILUS_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->file_monitor_add
        (directory, client,
        monitor_hidden_files,
        file_attributes,
        callback, callback_data);
}

void
nautilus_directory_file_monitor_remove (NautilusDirectory *directory,
                                        gconstpointer      client)
{
    g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));
    g_return_if_fail (client != NULL);

    NAUTILUS_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->file_monitor_remove
        (directory, client);
}

void
nautilus_directory_force_reload (NautilusDirectory *directory)
{
    g_return_if_fail (NAUTILUS_IS_DIRECTORY (directory));

    NAUTILUS_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->force_reload (directory);
}

gboolean
nautilus_directory_is_not_empty (NautilusDirectory *directory)
{
    g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), FALSE);

    return NAUTILUS_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->is_not_empty (directory);
}

GList *
nautilus_directory_get_file_list (NautilusDirectory *directory)
{
    return NAUTILUS_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->get_file_list (directory);
}

gboolean
nautilus_directory_is_editable (NautilusDirectory *directory)
{
    return NAUTILUS_DIRECTORY_CLASS (G_OBJECT_GET_CLASS (directory))->is_editable (directory);
}

GList *
nautilus_directory_match_pattern (NautilusDirectory *directory,
                                  const char        *pattern)
{
    GList *files, *l, *ret;
    GPatternSpec *spec;


    ret = NULL;
    spec = g_pattern_spec_new (pattern);

    files = nautilus_directory_get_file_list (directory);
    for (l = files; l; l = l->next)
    {
        NautilusFile *file = NAUTILUS_FILE (l->data);
        const char *name = nautilus_file_get_display_name (file);

        if (g_pattern_spec_match_string (spec, name))
        {
            ret = g_list_prepend (ret, nautilus_file_ref (file));
        }
    }

    g_pattern_spec_free (spec);
    nautilus_file_list_free (files);

    return ret;
}

/**
 * nautilus_directory_list_ref
 *
 * Ref all the directories in a list.
 * @list: GList of directories.
 **/
GList *
nautilus_directory_list_ref (GList *list)
{
    g_list_foreach (list, (GFunc) nautilus_directory_ref, NULL);
    return list;
}

/**
 * nautilus_directory_list_unref
 *
 * Unref all the directories in a list.
 * @list: GList of directories.
 **/
void
nautilus_directory_list_unref (GList *list)
{
    g_list_foreach (list, (GFunc) nautilus_directory_unref, NULL);
}

/**
 * nautilus_directory_list_free
 *
 * Free a list of directories after unrefing them.
 * @list: GList of directories.
 **/
void
nautilus_directory_list_free (GList *list)
{
    nautilus_directory_list_unref (list);
    g_list_free (list);
}

/**
 * nautilus_directory_list_copy
 *
 * Copy the list of directories, making a new ref of each,
 * @list: GList of directories.
 **/
GList *
nautilus_directory_list_copy (GList *list)
{
    return g_list_copy (nautilus_directory_list_ref (list));
}

static int
compare_by_uri (NautilusDirectory *a,
                NautilusDirectory *b)
{
    char *uri_a, *uri_b;
    int res;

    uri_a = g_file_get_uri (a->details->location);
    uri_b = g_file_get_uri (b->details->location);

    res = strcmp (uri_a, uri_b);

    g_free (uri_a);
    g_free (uri_b);

    return res;
}

static int
compare_by_uri_cover (gconstpointer a,
                      gconstpointer b)
{
    return compare_by_uri (NAUTILUS_DIRECTORY (a), NAUTILUS_DIRECTORY (b));
}

/**
 * nautilus_directory_list_sort_by_uri
 *
 * Sort the list of directories by directory uri.
 * @list: GList of directories.
 **/
GList *
nautilus_directory_list_sort_by_uri (GList *list)
{
    return g_list_sort (list, compare_by_uri_cover);
}

/* Return the number of extant NautilusDirectories */
int
nautilus_directory_number_outstanding (void)
{
    return directories ? g_hash_table_size (directories) : 0;
}

void
nautilus_directory_dump (NautilusDirectory *directory)
{
    g_autofree gchar *uri = NULL;

    uri = g_file_get_uri (directory->details->location);
    g_print ("uri: %s\n", uri);
    g_print ("ref count: %d\n", G_OBJECT (directory)->ref_count);
}
