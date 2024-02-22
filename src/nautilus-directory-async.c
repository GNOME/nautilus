/*
 *  nautilus-directory-async.c: Nautilus directory model state machine.
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
#define G_LOG_DOMAIN "nautilus-async-jobs"

#include <stdio.h>
#include <stdlib.h>

#include "nautilus-directory-notify.h"
#include "nautilus-directory-private.h"
#include "nautilus-enums.h"
#include "nautilus-file-private.h"
#include "nautilus-file-queue.h"
#include "nautilus-file-utilities.h"
#include "nautilus-global-preferences.h"
#include "nautilus-metadata.h"
#include "nautilus-signaller.h"

/* turn this on to check if async. job calls are balanced */
#if 0
#define DEBUG_ASYNC_JOBS
#endif

#define DIRECTORY_LOAD_ITEMS_PER_CALLBACK 100

/* Keep async. jobs down to this number for all directories. */
#define MAX_ASYNC_JOBS 10

struct ThumbnailState
{
    NautilusDirectory *directory;
    GCancellable *cancellable;
    NautilusFile *file;
};

struct MountState
{
    NautilusDirectory *directory;
    GCancellable *cancellable;
    NautilusFile *file;
};

struct FilesystemInfoState
{
    NautilusDirectory *directory;
    GCancellable *cancellable;
    NautilusFile *file;
};

struct DirectoryLoadState
{
    NautilusDirectory *directory;
    GCancellable *cancellable;
    GFileEnumerator *enumerator;
    NautilusFile *load_directory_file;
    int load_file_count;
};

struct GetInfoState
{
    NautilusDirectory *directory;
    GCancellable *cancellable;
};

struct NewFilesState
{
    NautilusDirectory *directory;
    GCancellable *cancellable;
    int count;
};

struct DirectoryCountState
{
    NautilusDirectory *directory;
    NautilusFile *count_file;
    GCancellable *cancellable;
    GFileEnumerator *enumerator;
    int file_count;
};

struct DeepCountState
{
    NautilusDirectory *directory;
    GCancellable *cancellable;
    GFileEnumerator *enumerator;
    GFile *deep_count_location;
    GList *deep_count_subdirectories;
    GArray *seen_deep_count_inodes;
    char *fs_id;
};



typedef struct
{
    NautilusFile *file;     /* Which file, NULL means all. */
    union
    {
        NautilusDirectoryCallback directory;
        NautilusFileCallback file;
    } callback;
    gpointer callback_data;
    Request request;
} ReadyCallback;

typedef struct
{
    NautilusFile *file;     /* Which file, NULL means all. */
    gboolean monitor_hidden_files;     /* defines whether "all" includes hidden files */
    gconstpointer client;
    Request request;
} Monitor;

typedef struct
{
    NautilusDirectory *directory;
    NautilusInfoProvider *provider;
    NautilusOperationHandle *handle;
    NautilusOperationResult result;
} InfoProviderResponse;

typedef gboolean (*RequestCheck) (Request);
typedef gboolean (*FileCheck) (NautilusFile *);

/* Current number of async. jobs. */
static int async_job_count;
static GHashTable *waiting_directories;
#ifdef DEBUG_ASYNC_JOBS
static GHashTable *async_jobs;
#endif

/* Forward declarations for functions that need them. */
static void     deep_count_load (DeepCountState *state,
                                 GFile          *location);
static gboolean request_is_satisfied (NautilusDirectory *directory,
                                      NautilusFile      *file,
                                      Request            request);
static void     cancel_loading_attributes (NautilusDirectory     *directory,
                                           NautilusFileAttributes file_attributes);
static void     add_all_files_to_work_queue (NautilusDirectory *directory);
static void     move_file_to_low_priority_queue (NautilusDirectory *directory,
                                                 NautilusFile      *file);
static void     move_file_to_extension_queue (NautilusDirectory *directory,
                                              NautilusFile      *file);
static void     nautilus_directory_invalidate_file_attributes (NautilusDirectory     *directory,
                                                               NautilusFileAttributes file_attributes);

static void
request_counter_add_request (RequestCounter counter,
                             Request        request)
{
    guint i;

    for (i = 0; i < REQUEST_TYPE_LAST; i++)
    {
        if (REQUEST_WANTS_TYPE (request, i))
        {
            counter[i]++;
        }
    }
}

static void
request_counter_remove_request (RequestCounter counter,
                                Request        request)
{
    guint i;

    for (i = 0; i < REQUEST_TYPE_LAST; i++)
    {
        if (REQUEST_WANTS_TYPE (request, i))
        {
            counter[i]--;
        }
    }
}

#if 0
static void
nautilus_directory_verify_request_counts (NautilusDirectory *directory)
{
    GList *l;
    RequestCounter counters;
    int i;
    gboolean fail;
    GHashTableIter monitor_iter;
    gpointer value;

    fail = FALSE;
    for (i = 0; i < REQUEST_TYPE_LAST; i++)
    {
        counters[i] = 0;
    }
    g_hash_table_iter_init (&monitor_iter, directory->details->monitor_table);
    while (g_hash_table_iter_next (&monitor_iter, NULL, &value))
    {
        for (l = value; l; l = l->next)
        {
            Monitor *monitor = l->data;
            request_counter_add_request (counters, monitor->request);
        }
    }
    for (i = 0; i < REQUEST_TYPE_LAST; i++)
    {
        if (counters[i] != directory->details->monitor_counters[i])
        {
            g_warning ("monitor counter for %i is wrong, expecting %d but found %d",
                       i, counters[i], directory->details->monitor_counters[i]);
            fail = TRUE;
        }
    }
    for (i = 0; i < REQUEST_TYPE_LAST; i++)
    {
        counters[i] = 0;
    }
    for (l = directory->details->call_when_ready_list; l != NULL; l = l->next)
    {
        ReadyCallback *callback = l->data;
        request_counter_add_request (counters, callback->request);
    }
    for (i = 0; i < REQUEST_TYPE_LAST; i++)
    {
        if (counters[i] != directory->details->call_when_ready_counters[i])
        {
            g_warning ("call when ready counter for %i is wrong, expecting %d but found %d",
                       i, counters[i], directory->details->call_when_ready_counters[i]);
            fail = TRUE;
        }
    }
    g_assert (!fail);
}
#endif

/* Start a job. This is really just a way of limiting the number of
 * async. requests that we issue at any given time. Without this, the
 * number of requests is unbounded.
 */
static gboolean
async_job_start (NautilusDirectory *directory,
                 const char        *job)
{
#ifdef DEBUG_ASYNC_JOBS
    char *key;
#endif

    g_debug ("starting %s in %p", job, directory->details->location);

    g_assert (async_job_count >= 0);
    g_assert (async_job_count <= MAX_ASYNC_JOBS);

    if (async_job_count >= MAX_ASYNC_JOBS)
    {
        if (waiting_directories == NULL)
        {
            waiting_directories = g_hash_table_new (NULL, NULL);
        }

        g_hash_table_insert (waiting_directories,
                             directory,
                             directory);

        return FALSE;
    }

#ifdef DEBUG_ASYNC_JOBS
    {
        char *uri;
        if (async_jobs == NULL)
        {
            async_jobs = g_hash_table_new (g_str_hash, g_str_equal);
        }
        uri = nautilus_directory_get_uri (directory);
        key = g_strconcat (uri, ": ", job, NULL);
        if (g_hash_table_lookup (async_jobs, key) != NULL)
        {
            g_warning ("same job twice: %s in %s",
                       job, uri);
        }
        g_free (uri);
        g_hash_table_insert (async_jobs, key, directory);
    }
#endif

    async_job_count += 1;
    return TRUE;
}

/* End a job. */
static void
async_job_end (NautilusDirectory *directory,
               const char        *job)
{
#ifdef DEBUG_ASYNC_JOBS
    char *key;
    gpointer table_key, value;
#endif

    g_debug ("stopping %s in %p", job, directory->details->location);

    g_assert (async_job_count > 0);

#ifdef DEBUG_ASYNC_JOBS
    {
        char *uri;
        uri = nautilus_directory_get_uri (directory);
        g_assert (async_jobs != NULL);
        key = g_strconcat (uri, ": ", job, NULL);
        if (!g_hash_table_lookup_extended (async_jobs, key, &table_key, &value))
        {
            g_warning ("ending job we didn't start: %s in %s",
                       job, uri);
        }
        else
        {
            g_hash_table_remove (async_jobs, key);
            g_free (table_key);
        }
        g_free (uri);
        g_free (key);
    }
#endif

    async_job_count -= 1;
}

/* Helper to get one value from a hash table. */
static void
get_one_value_callback (gpointer key,
                        gpointer value,
                        gpointer callback_data)
{
    gpointer *returned_value;

    returned_value = callback_data;
    *returned_value = value;
}

/* return a single value from a hash table. */
static gpointer
get_one_value (GHashTable *table)
{
    gpointer value;

    value = NULL;
    if (table != NULL)
    {
        g_hash_table_foreach (table, get_one_value_callback, &value);
    }
    return value;
}

/* Wake up directories that are "blocked" as long as there are job
 * slots available.
 */
static void
async_job_wake_up (void)
{
    static gboolean already_waking_up = FALSE;
    gpointer value;

    g_assert (async_job_count >= 0);
    g_assert (async_job_count <= MAX_ASYNC_JOBS);

    if (already_waking_up)
    {
        return;
    }

    already_waking_up = TRUE;
    while (async_job_count < MAX_ASYNC_JOBS)
    {
        value = get_one_value (waiting_directories);
        if (value == NULL)
        {
            break;
        }
        g_hash_table_remove (waiting_directories, value);
        nautilus_directory_async_state_changed
            (NAUTILUS_DIRECTORY (value));
    }
    already_waking_up = FALSE;
}

static void
directory_count_cancel (NautilusDirectory *directory)
{
    if (directory->details->count_in_progress != NULL)
    {
        g_cancellable_cancel (directory->details->count_in_progress->cancellable);
        directory->details->count_in_progress = NULL;
    }
}

static void
deep_count_cancel (NautilusDirectory *directory)
{
    if (directory->details->deep_count_in_progress != NULL)
    {
        g_assert (NAUTILUS_IS_FILE (directory->details->deep_count_file));

        g_cancellable_cancel (directory->details->deep_count_in_progress->cancellable);

        directory->details->deep_count_file->details->deep_counts_status = NAUTILUS_REQUEST_NOT_STARTED;

        directory->details->deep_count_in_progress->directory = NULL;
        directory->details->deep_count_in_progress = NULL;
        directory->details->deep_count_file = NULL;

        async_job_end (directory, "deep count");
    }
}

static void
thumbnail_cancel (NautilusDirectory *directory)
{
    if (directory->details->thumbnail_state != NULL)
    {
        g_cancellable_cancel (directory->details->thumbnail_state->cancellable);
        directory->details->thumbnail_state->directory = NULL;
        directory->details->thumbnail_state = NULL;
        async_job_end (directory, "thumbnail");
    }
}

static void
mount_cancel (NautilusDirectory *directory)
{
    if (directory->details->mount_state != NULL)
    {
        g_cancellable_cancel (directory->details->mount_state->cancellable);
        directory->details->mount_state->directory = NULL;
        directory->details->mount_state = NULL;
        async_job_end (directory, "mount");
    }
}

static void
file_info_cancel (NautilusDirectory *directory)
{
    if (directory->details->get_info_in_progress != NULL)
    {
        g_cancellable_cancel (directory->details->get_info_in_progress->cancellable);
        directory->details->get_info_in_progress->directory = NULL;
        directory->details->get_info_in_progress = NULL;
        directory->details->get_info_file = NULL;

        async_job_end (directory, "file info");
    }
}

static void
new_files_cancel (NautilusDirectory *directory)
{
    GList *l;
    NewFilesState *state;

    if (directory->details->new_files_in_progress != NULL)
    {
        for (l = directory->details->new_files_in_progress; l != NULL; l = l->next)
        {
            state = l->data;
            g_cancellable_cancel (state->cancellable);
            state->directory = NULL;
        }
        g_list_free (directory->details->new_files_in_progress);
        directory->details->new_files_in_progress = NULL;
    }
}

static int
monitor_key_compare (gconstpointer a,
                     gconstpointer data)
{
    const Monitor *monitor;
    const Monitor *compare_monitor;

    monitor = a;
    compare_monitor = data;

    if (monitor->client < compare_monitor->client)
    {
        return -1;
    }
    if (monitor->client > compare_monitor->client)
    {
        return +1;
    }

    if (monitor->file < compare_monitor->file)
    {
        return -1;
    }
    if (monitor->file > compare_monitor->file)
    {
        return +1;
    }

    return 0;
}

static Monitor *
find_monitor (NautilusDirectory *directory,
              NautilusFile      *file,
              gconstpointer      client)
{
    GList *l;

    l = g_hash_table_lookup (directory->details->monitor_table, file);

    if (l)
    {
        Monitor key = {};
        key.client = client;
        key.file = file;

        l = g_list_find_custom (l, &key, monitor_key_compare);
        return l ? l->data : NULL;
    }

    return NULL;
}

static gboolean
insert_new_monitor (NautilusDirectory *directory,
                    Monitor           *monitor)
{
    GList *list;

    if (find_monitor (directory, monitor->file, monitor->client) != NULL)
    {
        return FALSE;
    }

    list = g_hash_table_lookup (directory->details->monitor_table, monitor->file);
    if (list == NULL)
    {
        list = g_list_append (list, monitor);
        g_hash_table_insert (directory->details->monitor_table,
                             monitor->file,
                             list);
    }
    else
    {
        list = g_list_append (list, monitor);
    }

    request_counter_add_request (directory->details->monitor_counters,
                                 monitor->request);
    return TRUE;
}

static Monitor *
remove_monitor_from_table (NautilusDirectory *directory,
                           NautilusFile      *file,
                           gconstpointer      client)
{
    GList *list, *l, *new_list;
    Monitor *monitor = NULL;

    list = g_hash_table_lookup (directory->details->monitor_table, file);
    if (list)
    {
        Monitor key = {};
        key.client = client;
        key.file = file;

        l = g_list_find_custom (list, &key, monitor_key_compare);
        monitor = l ? l->data : NULL;
    }

    if (monitor != NULL)
    {
        new_list = g_list_delete_link (list, l);
        if (new_list == NULL)
        {
            g_hash_table_remove (directory->details->monitor_table, file);
        }
        else
        {
            g_hash_table_replace (directory->details->monitor_table, file, new_list);
        }
    }

    return monitor;
}

static void
remove_monitor (NautilusDirectory *directory,
                NautilusFile      *file,
                gconstpointer      client)
{
    Monitor *monitor;

    monitor = remove_monitor_from_table (directory, file, client);

    if (monitor != NULL)
    {
        request_counter_remove_request (directory->details->monitor_counters,
                                        monitor->request);
        g_free (monitor);
    }
}

Request
nautilus_directory_set_up_request (NautilusFileAttributes file_attributes)
{
    Request request;

    request = 0;

    if ((file_attributes & NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT) != 0)
    {
        REQUEST_SET_TYPE (request, REQUEST_DIRECTORY_COUNT);
    }

    if ((file_attributes & NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS) != 0)
    {
        REQUEST_SET_TYPE (request, REQUEST_DEEP_COUNT);
    }

    if ((file_attributes & NAUTILUS_FILE_ATTRIBUTE_INFO) != 0)
    {
        REQUEST_SET_TYPE (request, REQUEST_FILE_INFO);
    }

    if ((file_attributes & NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO) != 0)
    {
        REQUEST_SET_TYPE (request, REQUEST_EXTENSION_INFO);
    }

    if (file_attributes & NAUTILUS_FILE_ATTRIBUTE_THUMBNAIL)
    {
        REQUEST_SET_TYPE (request, REQUEST_THUMBNAIL);
        REQUEST_SET_TYPE (request, REQUEST_FILE_INFO);
    }

    if (file_attributes & NAUTILUS_FILE_ATTRIBUTE_MOUNT)
    {
        REQUEST_SET_TYPE (request, REQUEST_MOUNT);
        REQUEST_SET_TYPE (request, REQUEST_FILE_INFO);
    }

    if (file_attributes & NAUTILUS_FILE_ATTRIBUTE_FILESYSTEM_INFO)
    {
        REQUEST_SET_TYPE (request, REQUEST_FILESYSTEM_INFO);
    }

    return request;
}

static void
mime_db_changed_callback (GObject           *ignore,
                          NautilusDirectory *dir)
{
    g_assert (dir != NULL);
    g_assert (dir->details != NULL);

    nautilus_directory_force_reload_internal (dir, NAUTILUS_FILE_ATTRIBUTE_INFO);
}

void
nautilus_directory_monitor_add_internal (NautilusDirectory         *directory,
                                         NautilusFile              *file,
                                         gconstpointer              client,
                                         gboolean                   monitor_hidden_files,
                                         NautilusFileAttributes     file_attributes,
                                         NautilusDirectoryCallback  callback,
                                         gpointer                   callback_data)
{
    Monitor *monitor;
    GList *file_list;
    char *file_uri = NULL;
    char *dir_uri = NULL;

    g_assert (NAUTILUS_IS_DIRECTORY (directory));

    if (file != NULL)
    {
        file_uri = nautilus_file_get_uri (file);
    }
    if (directory != NULL)
    {
        dir_uri = nautilus_directory_get_uri (directory);
    }
    g_free (dir_uri);
    g_free (file_uri);

    /* Replace any current monitor for this client/file pair. */
    remove_monitor (directory, file, client);

    /* Add the new monitor. */
    monitor = g_new (Monitor, 1);
    monitor->file = file;
    monitor->monitor_hidden_files = monitor_hidden_files;
    monitor->client = client;
    monitor->request = nautilus_directory_set_up_request (file_attributes);

    if (file == NULL)
    {
        REQUEST_SET_TYPE (monitor->request, REQUEST_FILE_LIST);
    }

    insert_new_monitor (directory, monitor);

    if (callback != NULL)
    {
        file_list = nautilus_directory_get_file_list (directory);
        (*callback)(directory, file_list, callback_data);
        nautilus_file_list_free (file_list);
    }

    /* Start the "real" monitoring (FAM or whatever). */
    /* We always monitor the whole directory since in practice
     * nautilus almost always shows the whole directory anyway, and
     * it allows us to avoid one file monitor per file in a directory.
     */
    if (directory->details->monitor == NULL)
    {
        directory->details->monitor = nautilus_monitor_directory (directory->details->location);
    }


    if (REQUEST_WANTS_TYPE (monitor->request, REQUEST_FILE_INFO) &&
        directory->details->mime_db_monitor == 0)
    {
        directory->details->mime_db_monitor =
            g_signal_connect_object (nautilus_signaller_get_current (),
                                     "mime-data-changed",
                                     G_CALLBACK (mime_db_changed_callback), directory, 0);
    }

    /* Put the monitor file or all the files on the work queue. */
    if (file != NULL)
    {
        nautilus_directory_add_file_to_work_queue (directory, file);
    }
    else
    {
        add_all_files_to_work_queue (directory);
    }

    /* Kick off I/O. */
    nautilus_directory_async_state_changed (directory);
}

static void
set_file_unconfirmed (NautilusFile *file,
                      gboolean      unconfirmed)
{
    NautilusDirectory *directory;

    g_assert (NAUTILUS_IS_FILE (file));
    g_assert (unconfirmed == FALSE || unconfirmed == TRUE);

    if (file->details->unconfirmed == unconfirmed)
    {
        return;
    }
    file->details->unconfirmed = unconfirmed;

    directory = file->details->directory;
    if (unconfirmed)
    {
        directory->details->confirmed_file_count--;
    }
    else
    {
        directory->details->confirmed_file_count++;
    }
}

static gboolean show_hidden_files = TRUE;

static void
show_hidden_files_changed_callback (gpointer callback_data)
{
    show_hidden_files = g_settings_get_boolean (gtk_filechooser_preferences, NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES);
}

static gboolean
should_skip_file (GFileInfo *info)
{
    static gboolean show_hidden_files_changed_callback_installed = FALSE;
    gboolean is_hidden;

    /* Add the callback once for the life of our process */
    if (!show_hidden_files_changed_callback_installed)
    {
        g_signal_connect_swapped (gtk_filechooser_preferences,
                                  "changed::" NAUTILUS_PREFERENCES_SHOW_HIDDEN_FILES,
                                  G_CALLBACK (show_hidden_files_changed_callback),
                                  NULL);

        show_hidden_files_changed_callback_installed = TRUE;

        /* Peek for the first time */
        show_hidden_files_changed_callback (NULL);
    }

    is_hidden = g_file_info_get_attribute_boolean (info,
                                                   G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN) ||
                g_file_info_get_attribute_boolean (info,
                                                   G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP);
    if (!show_hidden_files && is_hidden)
    {
        return TRUE;
    }

    return FALSE;
}

static void
notify_files_changed_while_being_added (NautilusDirectory *directory)
{
    if (directory->details->files_changed_while_adding == NULL)
    {
        return;
    }

    directory->details->files_changed_while_adding =
        g_list_reverse (directory->details->files_changed_while_adding);

    nautilus_directory_notify_files_changed (directory->details->files_changed_while_adding);

    g_clear_list (&directory->details->files_changed_while_adding, g_object_unref);
}

static gboolean
dequeue_pending_idle_callback (gpointer callback_data)
{
    NautilusDirectory *directory;
    GList *pending_file_info;
    GList *node, *next;
    NautilusFile *file;
    GList *changed_files, *added_files;
    GFileInfo *file_info;
    const char *name;
    DirectoryLoadState *dir_load_state;

    directory = NAUTILUS_DIRECTORY (callback_data);

    nautilus_directory_ref (directory);

    directory->details->dequeue_pending_idle_id = 0;

    /* Handle the files in the order we saw them. */
    pending_file_info = g_list_reverse (directory->details->pending_file_info);
    directory->details->pending_file_info = NULL;

    /* If we are no longer monitoring, then throw away these. */
    if (!nautilus_directory_is_file_list_monitored (directory))
    {
        nautilus_directory_async_state_changed (directory);
        goto drain;
    }

    added_files = NULL;
    changed_files = NULL;

    dir_load_state = directory->details->directory_load_in_progress;

    /* Build a list of NautilusFile objects. */
    for (node = pending_file_info; node != NULL; node = node->next)
    {
        file_info = node->data;

        name = g_file_info_get_name (file_info);

        /* Update the file count. */
        /* FIXME bugzilla.gnome.org 45063: This could count a
         * file twice if we get it from both load_directory
         * and from new_files_callback. Not too hard to fix by
         * moving this into the actual callback instead of
         * waiting for the idle function.
         */
        if (dir_load_state && !should_skip_file (file_info))
        {
            dir_load_state->load_file_count += 1;
        }

        /* check if the file already exists */
        file = nautilus_directory_find_file_by_name (directory, name);
        if (file != NULL)
        {
            /* file already exists in dir, check if we still need to
             *  emit file_added or if it changed */
            set_file_unconfirmed (file, FALSE);
            if (!file->details->is_added)
            {
                /* We consider this newly added even if its in the list.
                 * This can happen if someone called nautilus_file_get_by_uri()
                 * on a file in the folder before the add signal was
                 * emitted */
                nautilus_file_ref (file);
                file->details->is_added = TRUE;
                added_files = g_list_prepend (added_files, file);
            }
            else if (nautilus_file_update_info (file, file_info))
            {
                /* File changed, notify about the change. */
                nautilus_file_ref (file);
                changed_files = g_list_prepend (changed_files, file);
            }
        }
        else
        {
            /* new file, create a nautilus file object and add it to the list */
            file = nautilus_file_new_from_info (directory, file_info);
            nautilus_directory_add_file (directory, file);
            file->details->is_added = TRUE;
            added_files = g_list_prepend (added_files, file);
        }
    }

    /* If we are done loading, then we assume that any unconfirmed
     * files are gone.
     */
    if (directory->details->directory_loaded)
    {
        for (node = directory->details->file_list;
             node != NULL; node = next)
        {
            file = NAUTILUS_FILE (node->data);
            next = node->next;

            if (file->details->unconfirmed)
            {
                nautilus_file_ref (file);
                changed_files = g_list_prepend (changed_files, file);

                nautilus_file_mark_gone (file);
            }
        }
    }

    /* Send the changed and added signals. */
    nautilus_directory_emit_change_signals (directory, changed_files);
    nautilus_file_list_free (changed_files);
    nautilus_directory_emit_files_added (directory, added_files);
    nautilus_file_list_free (added_files);

    if (directory->details->directory_loaded &&
        !directory->details->directory_loaded_sent_notification)
    {
        /* Send the done_loading signal. */
        nautilus_directory_emit_done_loading (directory);

        if (dir_load_state)
        {
            file = dir_load_state->load_directory_file;

            file->details->directory_count = dir_load_state->load_file_count;
            file->details->directory_count_is_up_to_date = TRUE;
            file->details->got_directory_count = TRUE;

            nautilus_file_changed (file);
        }

        nautilus_directory_async_state_changed (directory);

        directory->details->directory_loaded_sent_notification = TRUE;
    }

    /* Process changes received for files while they were still being added.
     * See Bug 703179 and issue #1576 for a situation this happens. */
    notify_files_changed_while_being_added (directory);

drain:
    g_list_free_full (pending_file_info, g_object_unref);

    /* Get the state machine running again. */
    nautilus_directory_async_state_changed (directory);

    nautilus_directory_unref (directory);
    return FALSE;
}

void
nautilus_directory_schedule_dequeue_pending (NautilusDirectory *directory)
{
    if (directory->details->dequeue_pending_idle_id == 0)
    {
        directory->details->dequeue_pending_idle_id
            = g_idle_add (dequeue_pending_idle_callback, directory);
    }
}

static void
directory_load_one (NautilusDirectory *directory,
                    GFileInfo         *info)
{
    if (info == NULL)
    {
        return;
    }

    if (g_file_info_get_name (info) == NULL)
    {
        char *uri;

        uri = nautilus_directory_get_uri (directory);
        g_warning ("Got GFileInfo with NULL name in %s, ignoring. This shouldn't happen unless the gvfs backend is broken.\n", uri);
        g_free (uri);

        return;
    }

    /* Arrange for the "loading" part of the work. */
    g_object_ref (info);
    directory->details->pending_file_info
        = g_list_prepend (directory->details->pending_file_info, info);
    nautilus_directory_schedule_dequeue_pending (directory);
}

static void
directory_load_cancel (NautilusDirectory *directory)
{
    NautilusFile *file;
    DirectoryLoadState *state;

    state = directory->details->directory_load_in_progress;
    if (state != NULL)
    {
        file = state->load_directory_file;
        file->details->loading_directory = FALSE;
        if (file->details->directory != directory)
        {
            nautilus_directory_async_state_changed (file->details->directory);
        }

        g_cancellable_cancel (state->cancellable);
        state->directory = NULL;
        directory->details->directory_load_in_progress = NULL;
        async_job_end (directory, "file list");
    }
}

static void
file_list_cancel (NautilusDirectory *directory)
{
    directory_load_cancel (directory);

    if (directory->details->dequeue_pending_idle_id != 0)
    {
        g_source_remove (directory->details->dequeue_pending_idle_id);
        directory->details->dequeue_pending_idle_id = 0;
    }

    if (directory->details->pending_file_info != NULL)
    {
        g_list_free_full (directory->details->pending_file_info, g_object_unref);
        directory->details->pending_file_info = NULL;
    }
}

static void
directory_load_done (NautilusDirectory *directory,
                     GError            *error)
{
    GList *node;

    g_object_ref (directory);

    directory->details->directory_loaded = TRUE;
    directory->details->directory_loaded_sent_notification = FALSE;

    if (error != NULL)
    {
        /* The load did not complete successfully. This means
         * we don't know the status of the files in this directory.
         * We clear the unconfirmed bit on each file here so that
         * they won't be marked "gone" later -- we don't know enough
         * about them to know whether they are really gone.
         */
        for (node = directory->details->file_list;
             node != NULL; node = node->next)
        {
            set_file_unconfirmed (NAUTILUS_FILE (node->data), FALSE);
        }

        nautilus_directory_emit_load_error (directory, error);
    }

    /* Call the idle function right away. */
    if (directory->details->dequeue_pending_idle_id != 0)
    {
        g_source_remove (directory->details->dequeue_pending_idle_id);
    }
    dequeue_pending_idle_callback (directory);

    directory_load_cancel (directory);

    g_object_unref (directory);
}

void
nautilus_directory_monitor_remove_internal (NautilusDirectory *directory,
                                            NautilusFile      *file,
                                            gconstpointer      client)
{
    g_assert (NAUTILUS_IS_DIRECTORY (directory));
    g_assert (file == NULL || NAUTILUS_IS_FILE (file));
    g_assert (client != NULL);

    remove_monitor (directory, file, client);

    if (directory->details->monitor != NULL
        && g_hash_table_size (directory->details->monitor_table) == 0)
    {
        nautilus_monitor_cancel (directory->details->monitor);
        directory->details->monitor = NULL;
    }

    /* XXX - do we need to remove anything from the work queue? */

    nautilus_directory_async_state_changed (directory);
}

FileMonitors *
nautilus_directory_remove_file_monitors (NautilusDirectory *directory,
                                         NautilusFile      *file)
{
    GList *result, *node;
    Monitor *monitor;

    g_assert (NAUTILUS_IS_DIRECTORY (directory));
    g_assert (NAUTILUS_IS_FILE (file));
    g_assert (file->details->directory == directory);

    result = g_hash_table_lookup (directory->details->monitor_table, file);

    if (result != NULL)
    {
        g_hash_table_remove (directory->details->monitor_table, file);

        for (node = result; node; node = node->next)
        {
            monitor = node->data;
            request_counter_remove_request (directory->details->monitor_counters,
                                            monitor->request);
        }
        result = g_list_reverse (result);
    }

    /* XXX - do we need to remove anything from the work queue? */

    nautilus_directory_async_state_changed (directory);

    return (FileMonitors *) result;
}

void
nautilus_directory_add_file_monitors (NautilusDirectory *directory,
                                      NautilusFile      *file,
                                      FileMonitors      *monitors)
{
    GList *l;
    Monitor *monitor;

    g_assert (NAUTILUS_IS_DIRECTORY (directory));
    g_assert (NAUTILUS_IS_FILE (file));
    g_assert (file->details->directory == directory);

    if (monitors == NULL)
    {
        return;
    }

    for (l = (GList *) monitors; l != NULL; l = l->next)
    {
        monitor = l->data;

        remove_monitor (directory, monitor->file, monitor->client);
        insert_new_monitor (directory, monitor);
    }

    g_list_free ((GList *) monitors);

    nautilus_directory_add_file_to_work_queue (directory, file);

    nautilus_directory_async_state_changed (directory);
}

static int
ready_callback_key_compare (gconstpointer a,
                            gconstpointer b)
{
    const ReadyCallback *callback_a, *callback_b;

    callback_a = a;
    callback_b = b;

    return !(callback_a->file == callback_b->file &&
             (callback_a->file != NULL ?
              callback_a->callback.file == callback_b->callback.file :
              callback_a->callback.directory == callback_b->callback.directory) &&
             callback_a->callback_data == callback_b->callback_data);
}

static void
ready_callback_call (NautilusDirectory   *directory,
                     const ReadyCallback *callback)
{
    GList *file_list;

    /* Call the callback. */
    if (callback->file != NULL)
    {
        if (callback->callback.file)
        {
            (*callback->callback.file)(callback->file,
                                       callback->callback_data);
        }
    }
    else if (callback->callback.directory != NULL)
    {
        if (directory == NULL ||
            !REQUEST_WANTS_TYPE (callback->request, REQUEST_FILE_LIST))
        {
            file_list = NULL;
        }
        else
        {
            file_list = nautilus_directory_get_file_list (directory);
        }

        /* Pass back the file list if the user was waiting for it. */
        (*callback->callback.directory)(directory,
                                        file_list,
                                        callback->callback_data);

        nautilus_file_list_free (file_list);
    }
}

void
nautilus_directory_call_when_ready_internal (NautilusDirectory         *directory,
                                             NautilusFile              *file,
                                             NautilusFileAttributes     file_attributes,
                                             gboolean                   wait_for_file_list,
                                             NautilusDirectoryCallback  directory_callback,
                                             NautilusFileCallback       file_callback,
                                             gpointer                   callback_data)
{
    ReadyCallback callback;
    GList *node;

    g_assert (directory == NULL || NAUTILUS_IS_DIRECTORY (directory));
    g_assert (file == NULL || NAUTILUS_IS_FILE (file));
    g_assert (file != NULL || directory_callback != NULL);

    /* Construct a callback object. */
    callback.file = file;
    if (file == NULL)
    {
        callback.callback.directory = directory_callback;
    }
    else
    {
        callback.callback.file = file_callback;
    }
    callback.callback_data = callback_data;
    callback.request = nautilus_directory_set_up_request (file_attributes);
    if (wait_for_file_list)
    {
        REQUEST_SET_TYPE (callback.request, REQUEST_FILE_LIST);
    }

    /* Handle the NULL case. */
    if (directory == NULL)
    {
        ready_callback_call (NULL, &callback);
        return;
    }

    /* Check if the callback is already there. */
    node = g_hash_table_lookup (directory->details->call_when_ready_hash.unsatisfied, callback.file);

    if (g_list_find_custom (node, &callback, ready_callback_key_compare) != NULL)
    {
        if (file_callback != NULL && directory_callback != NULL)
        {
            g_warning ("tried to add a new callback while an old one was pending");
        }
        /* NULL callback means, just read it. Conflicts are ok. */
        return;
    }

    /* Add the new callback to the list. */
    node = g_list_prepend (node, g_memdup2 (&callback, sizeof (callback)));
    g_hash_table_replace (directory->details->call_when_ready_hash.unsatisfied, callback.file, node);
    request_counter_add_request (directory->details->call_when_ready_counters,
                                 callback.request);

    /* Put the callback file or all the files on the work queue. */
    if (file != NULL)
    {
        nautilus_directory_add_file_to_work_queue (directory, file);
    }
    else
    {
        add_all_files_to_work_queue (directory);
    }

    nautilus_directory_async_state_changed (directory);
}

gboolean
nautilus_directory_check_if_ready_internal (NautilusDirectory      *directory,
                                            NautilusFile           *file,
                                            NautilusFileAttributes  file_attributes)
{
    Request request;

    g_assert (NAUTILUS_IS_DIRECTORY (directory));

    request = nautilus_directory_set_up_request (file_attributes);
    return request_is_satisfied (directory, file, request);
}

static GList *
remove_callback_link_keep_data (NautilusDirectory *directory,
                                GList             *link,
                                gboolean           ready)
{
    ReadyCallback *callback = link->data;
    GList *list = g_list_first (link);
    gboolean is_first_link = (list == link);

    list = g_list_delete_link (list, link);

    if (is_first_link)
    {
        GHashTable *hash = ready ? directory->details->call_when_ready_hash.ready :
                                   directory->details->call_when_ready_hash.unsatisfied;

        if (list != NULL)
        {
            g_hash_table_replace (hash, callback->file, list);
        }
        else
        {
            g_hash_table_remove (hash, callback->file);
        }
    }

    request_counter_remove_request (directory->details->call_when_ready_counters,
                                    callback->request);

    return list;
}

static GList *
remove_callback_link (NautilusDirectory *directory,
                      GList             *link,
                      gboolean           ready)
{
    ReadyCallback *callback;

    callback = link->data;
    link = remove_callback_link_keep_data (directory, link, ready);
    g_free (callback);

    return link;
}

static void
remove_similar_callbacks (NautilusDirectory *directory,
                          ReadyCallback     *callback)
{
    GList *list, *node;

    /* Remove all queued ready callbacks */
    list = g_hash_table_lookup (directory->details->call_when_ready_hash.ready, callback->file);
    node = g_list_find_custom (list, callback, ready_callback_key_compare);
    while (node != NULL)
    {
        node = remove_callback_link (directory, node, TRUE);
        nautilus_directory_async_state_changed (directory);
    }

    /* Remove all queued unsatisfied callbacks */
    list = g_hash_table_lookup (directory->details->call_when_ready_hash.unsatisfied, callback->file);
    node = g_list_find_custom (list, callback, ready_callback_key_compare);
    while (node != NULL)
    {
        node = remove_callback_link (directory, node, FALSE);
        nautilus_directory_async_state_changed (directory);
    }
}

void
nautilus_directory_cancel_callback_internal (NautilusDirectory         *directory,
                                             NautilusFile              *file,
                                             NautilusDirectoryCallback  directory_callback,
                                             NautilusFileCallback       file_callback,
                                             gpointer                   callback_data)
{
    ReadyCallback callback;

    if (directory == NULL)
    {
        return;
    }

    g_assert (NAUTILUS_IS_DIRECTORY (directory));
    g_assert (file == NULL || NAUTILUS_IS_FILE (file));
    g_assert (file != NULL || directory_callback != NULL);
    g_assert (file == NULL || file_callback != NULL);

    /* Construct a callback object. */
    callback.file = file;
    if (file == NULL)
    {
        callback.callback.directory = directory_callback;
    }
    else
    {
        callback.callback.file = file_callback;
    }
    callback.callback_data = callback_data;

    remove_similar_callbacks (directory, &callback);
}

static void
new_files_state_unref (NewFilesState *state)
{
    state->count--;

    if (state->count == 0)
    {
        if (state->directory)
        {
            state->directory->details->new_files_in_progress =
                g_list_remove (state->directory->details->new_files_in_progress,
                               state);
        }

        g_object_unref (state->cancellable);
        g_free (state);
    }
}

static void
new_files_callback (GObject      *source_object,
                    GAsyncResult *res,
                    gpointer      user_data)
{
    NautilusDirectory *directory;
    GFileInfo *info;
    NewFilesState *state;

    state = user_data;

    if (state->directory == NULL)
    {
        /* Operation was cancelled. Bail out */
        new_files_state_unref (state);
        return;
    }

    directory = nautilus_directory_ref (state->directory);

    /* Queue up the new file. */
    info = g_file_query_info_finish (G_FILE (source_object), res, NULL);
    if (info != NULL)
    {
        directory_load_one (directory, info);
        g_object_unref (info);
    }

    new_files_state_unref (state);

    nautilus_directory_unref (directory);
}

void
nautilus_directory_get_info_for_new_files (NautilusDirectory *directory,
                                           GList             *location_list)
{
    NewFilesState *state;
    GFile *location;
    GList *l;

    if (location_list == NULL)
    {
        return;
    }

    state = g_new (NewFilesState, 1);
    state->directory = directory;
    state->cancellable = g_cancellable_new ();
    state->count = 0;

    for (l = location_list; l != NULL; l = l->next)
    {
        location = l->data;

        state->count++;

        g_file_query_info_async (location,
                                 NAUTILUS_FILE_DEFAULT_ATTRIBUTES,
                                 0,
                                 G_PRIORITY_DEFAULT,
                                 state->cancellable,
                                 new_files_callback, state);
    }

    directory->details->new_files_in_progress
        = g_list_prepend (directory->details->new_files_in_progress,
                          state);
}

void
nautilus_async_destroying_file (NautilusFile *file)
{
    NautilusDirectory *directory = file->details->directory;
    gboolean changed = FALSE;
    GList *node;

    /* Check for callbacks. */
    node = g_hash_table_lookup (directory->details->call_when_ready_hash.unsatisfied, file);

    if (node != NULL)
    {
        /* Client should have cancelled callback. */
        g_warning ("destroyed file has call_when_ready pending");
        changed = TRUE;

        while (node != NULL)
        {
            node = remove_callback_link (directory, node, FALSE);
        }
    }

    node = g_hash_table_lookup (directory->details->call_when_ready_hash.ready, file);

    while (node != NULL)
    {
        node = remove_callback_link (directory, node, TRUE);
        changed = TRUE;
    }

    /* Check for monitors. */
    node = g_hash_table_lookup (directory->details->monitor_table, file);
    if (node != NULL)
    {
        /* Client should have removed monitor earlier. */
        g_warning ("destroyed file still being monitored");

        while (node != NULL)
        {
            Monitor *monitor = node->data;
            node = node->next;
            remove_monitor (directory, monitor->file, monitor->client);
        }
        changed = TRUE;
    }

    /* Check if it's a file that's currently being worked on.
     * If so, make that NULL so it gets canceled right away.
     */
    if (directory->details->count_in_progress != NULL &&
        directory->details->count_in_progress->count_file == file)
    {
        directory->details->count_in_progress->count_file = NULL;
        changed = TRUE;
    }
    if (directory->details->deep_count_file == file)
    {
        directory->details->deep_count_file = NULL;
        changed = TRUE;
    }
    if (directory->details->get_info_file == file)
    {
        directory->details->get_info_file = NULL;
        changed = TRUE;
    }
    if (directory->details->extension_info_file == file)
    {
        directory->details->extension_info_file = NULL;
        changed = TRUE;
    }

    if (directory->details->thumbnail_state != NULL &&
        directory->details->thumbnail_state->file == file)
    {
        directory->details->thumbnail_state->file = NULL;
        changed = TRUE;
    }

    if (directory->details->mount_state != NULL &&
        directory->details->mount_state->file == file)
    {
        directory->details->mount_state->file = NULL;
        changed = TRUE;
    }

    if (directory->details->filesystem_info_state != NULL &&
        directory->details->filesystem_info_state->file == file)
    {
        directory->details->filesystem_info_state->file = NULL;
        changed = TRUE;
    }

    /* Let the directory take care of the rest. */
    if (changed)
    {
        nautilus_directory_async_state_changed (directory);
    }
}

static gboolean
lacks_directory_count (NautilusFile *file)
{
    return !file->details->directory_count_is_up_to_date
           && nautilus_file_should_show_directory_item_count (file);
}

static gboolean
should_get_directory_count_now (NautilusFile *file)
{
    return lacks_directory_count (file)
           && !file->details->loading_directory;
}

static gboolean
lacks_info (NautilusFile *file)
{
    return !file->details->file_info_is_up_to_date
           && !file->details->is_gone;
}

static gboolean
lacks_filesystem_info (NautilusFile *file)
{
    return !file->details->filesystem_info_is_up_to_date;
}

static gboolean
lacks_deep_count (NautilusFile *file)
{
    return file->details->deep_counts_status != NAUTILUS_REQUEST_DONE;
}

static gboolean
lacks_extension_info (NautilusFile *file)
{
    return file->details->pending_info_providers != NULL;
}

static gboolean
lacks_thumbnail (NautilusFile *file)
{
    return nautilus_file_should_show_thumbnail (file) &&
           file->details->thumbnail_path != NULL &&
           !file->details->thumbnail_is_up_to_date;
}

static gboolean
lacks_mount (NautilusFile *file)
{
    return (!file->details->mount_is_up_to_date &&
            (
                /* Unix mountpoint, could be a GMount */
                file->details->is_mountpoint ||

                /* The toplevel directory of something */
                (file->details->type == G_FILE_TYPE_DIRECTORY &&
                 nautilus_file_is_self_owned (file)) ||

                /* Mountable, could be a mountpoint */
                (file->details->type == G_FILE_TYPE_MOUNTABLE)

            )
            );
}

static gboolean
has_problem (NautilusDirectory *directory,
             NautilusFile      *file,
             FileCheck          problem)
{
    GList *node;

    if (file != NULL)
    {
        return (*problem)(file);
    }

    for (node = directory->details->file_list; node != NULL; node = node->next)
    {
        if ((*problem)(node->data))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean
request_is_satisfied (NautilusDirectory *directory,
                      NautilusFile      *file,
                      Request            request)
{
    if (REQUEST_WANTS_TYPE (request, REQUEST_FILE_LIST) &&
        !(directory->details->directory_loaded &&
          directory->details->directory_loaded_sent_notification))
    {
        return FALSE;
    }

    if (REQUEST_WANTS_TYPE (request, REQUEST_DIRECTORY_COUNT))
    {
        if (has_problem (directory, file, lacks_directory_count))
        {
            return FALSE;
        }
    }

    if (REQUEST_WANTS_TYPE (request, REQUEST_FILE_INFO))
    {
        if (has_problem (directory, file, lacks_info))
        {
            return FALSE;
        }
    }

    if (REQUEST_WANTS_TYPE (request, REQUEST_FILESYSTEM_INFO))
    {
        if (has_problem (directory, file, lacks_filesystem_info))
        {
            return FALSE;
        }
    }

    if (REQUEST_WANTS_TYPE (request, REQUEST_DEEP_COUNT))
    {
        if (has_problem (directory, file, lacks_deep_count))
        {
            return FALSE;
        }
    }

    if (REQUEST_WANTS_TYPE (request, REQUEST_THUMBNAIL))
    {
        if (has_problem (directory, file, lacks_thumbnail))
        {
            return FALSE;
        }
    }

    if (REQUEST_WANTS_TYPE (request, REQUEST_MOUNT))
    {
        if (has_problem (directory, file, lacks_mount))
        {
            return FALSE;
        }
    }

    return TRUE;
}

static void
call_ready_callbacks_at_idle (gpointer callback_data)
{
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autoptr (GPtrArray) values = NULL;
    g_autoptr (GHashTable) ready_callbacks = NULL;

    directory = nautilus_directory_ref (NAUTILUS_DIRECTORY (callback_data));
    directory->details->call_ready_idle_id = 0;

    /* Swap out hash table for empty new one */
    ready_callbacks = directory->details->call_when_ready_hash.ready;
    directory->details->call_when_ready_hash.ready = g_hash_table_new (NULL, NULL);

    values = g_hash_table_get_values_as_ptr_array (ready_callbacks);

    if (values->len == 0)
    {
        /* Return early in case all ready callbacks were cancelled, don't emit state change */
        return;
    }

    for (guint i = 0; i < values->len; i++)
    {
        for (GList *node = values->pdata[i]; node != NULL; node = node->next)
        {
            ReadyCallback *callback = node->data;

            request_counter_remove_request (directory->details->call_when_ready_counters,
                                            callback->request);

            ready_callback_call (directory, callback);
        }
    }

    /* Cleanup */
    for (guint i = 0; i < values->len; i++)
    {
        g_list_free_full (values->pdata[i], g_free);
    }

    nautilus_directory_async_state_changed (directory);
}

static void
schedule_call_ready_callbacks (NautilusDirectory *directory)
{
    if (directory->details->call_ready_idle_id == 0)
    {
        directory->details->call_ready_idle_id
            = g_idle_add_once (call_ready_callbacks_at_idle, directory);
    }
}

/* Moves all the callbacks that are ready from the unsatisfied hash table
 * to the ready hash table and schedules them to be called at idle, unless
 * they are removed before then */
static gboolean
call_ready_callbacks (NautilusDirectory *directory)
{
    GHashTable *ready_hash = directory->details->call_when_ready_hash.ready;
    GHashTable *unsatisfied_hash = directory->details->call_when_ready_hash.unsatisfied;
    g_autoptr (GPtrArray) unsatisfied_callbacks = g_hash_table_get_values_as_ptr_array (unsatisfied_hash);
    gboolean found_any = FALSE;

    /* Check if any callbacks are satisfied and mark them for call them if they are. */
    for (guint i = 0; i < unsatisfied_callbacks->len; i++)
    {
        GList *unsatisfied_list = unsatisfied_callbacks->pdata[i];
        GList *ready_list = NULL;
        GList *next;
        NautilusFile *callback_file = NULL;
        gboolean satisfied_callbacks = FALSE;

        for (GList *node = unsatisfied_list; node != NULL; node = next)
        {
            next = node->next;
            ReadyCallback *callback = node->data;

            if (!request_is_satisfied (directory, callback->file, callback->request))
            {
                continue;
            }

            if (!satisfied_callbacks)
            {
                ready_list = g_hash_table_lookup (ready_hash, callback->file);
                callback_file = callback->file;
                satisfied_callbacks = TRUE;
            }

            /* Move callback from unready list to ready list */
            unsatisfied_list = g_list_delete_link (unsatisfied_list, node);
            ready_list = g_list_prepend (ready_list, callback);
        }

        if (satisfied_callbacks)
        {
            g_hash_table_replace (ready_hash, callback_file, ready_list);

            if (unsatisfied_list != NULL)
            {
                g_hash_table_replace (unsatisfied_hash, callback_file, unsatisfied_list);
            }
            else
            {
                g_hash_table_remove (unsatisfied_hash, callback_file);
            }

            found_any = TRUE;
        }
    }

    if (found_any)
    {
        schedule_call_ready_callbacks (directory);
    }

    return found_any;
}

static GList *
lookup_monitors (GHashTable   *monitor_table,
                 NautilusFile *file)
{
    /* To find monitors monitoring all files, use lookup_all_files_monitors. */
    g_return_val_if_fail (file, NULL);

    return g_hash_table_lookup (monitor_table, file);
}

static GList *
lookup_all_files_monitors (GHashTable *monitor_table)
{
    /* monitor->file == NULL means monitor all files. */
    return g_hash_table_lookup (monitor_table, NULL);
}

gboolean
nautilus_directory_has_request_for_file (NautilusDirectory *directory,
                                         NautilusFile      *file)
{
    if (g_hash_table_lookup (directory->details->call_when_ready_hash.unsatisfied, NULL) != NULL ||
        g_hash_table_lookup (directory->details->call_when_ready_hash.unsatisfied, file) != NULL ||
        g_hash_table_lookup (directory->details->call_when_ready_hash.ready, NULL) != NULL ||
        g_hash_table_lookup (directory->details->call_when_ready_hash.ready, file) != NULL)
    {
        return TRUE;
    }

    if (lookup_monitors (directory->details->monitor_table, file) != NULL)
    {
        return TRUE;
    }
    if (lookup_all_files_monitors (directory->details->monitor_table) != NULL)
    {
        return TRUE;
    }

    return FALSE;
}


/* This checks if there's a request for monitoring the file list. */
gboolean
nautilus_directory_is_anyone_monitoring_file_list (NautilusDirectory *directory)
{
    if (directory->details->call_when_ready_counters[REQUEST_FILE_LIST] > 0)
    {
        return TRUE;
    }

    if (directory->details->monitor_counters[REQUEST_FILE_LIST] > 0)
    {
        return TRUE;
    }

    return FALSE;
}

/* This checks if the file list being monitored. */
gboolean
nautilus_directory_is_file_list_monitored (NautilusDirectory *directory)
{
    return directory->details->file_list_monitored;
}

static void
mark_all_files_unconfirmed (NautilusDirectory *directory)
{
    GList *node;
    NautilusFile *file;

    for (node = directory->details->file_list; node != NULL; node = node->next)
    {
        file = node->data;
        set_file_unconfirmed (file, TRUE);
    }
}

static void
directory_load_state_free (DirectoryLoadState *state)
{
    if (state->enumerator)
    {
        if (!g_file_enumerator_is_closed (state->enumerator))
        {
            g_file_enumerator_close_async (state->enumerator,
                                           0, NULL, NULL, NULL);
        }
        g_object_unref (state->enumerator);
    }

    nautilus_file_unref (state->load_directory_file);
    g_object_unref (state->cancellable);
    g_free (state);
}

static void
more_files_callback (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
    DirectoryLoadState *state;
    NautilusDirectory *directory;
    GError *error;
    GList *files, *l;
    GFileInfo *info;

    state = user_data;

    if (state->directory == NULL)
    {
        /* Operation was cancelled. Bail out */
        directory_load_state_free (state);
        return;
    }

    directory = nautilus_directory_ref (state->directory);

    g_assert (directory->details->directory_load_in_progress != NULL);
    g_assert (directory->details->directory_load_in_progress == state);

    error = NULL;
    files = g_file_enumerator_next_files_finish (state->enumerator,
                                                 res, &error);

    for (l = files; l != NULL; l = l->next)
    {
        info = l->data;
        directory_load_one (directory, info);
        g_object_unref (info);
    }

    if (files == NULL)
    {
        directory_load_done (directory, error);
        directory_load_state_free (state);
    }
    else
    {
        g_file_enumerator_next_files_async (state->enumerator,
                                            DIRECTORY_LOAD_ITEMS_PER_CALLBACK,
                                            G_PRIORITY_DEFAULT,
                                            state->cancellable,
                                            more_files_callback,
                                            state);
    }

    nautilus_directory_unref (directory);

    if (error)
    {
        g_error_free (error);
    }

    g_list_free (files);
}

static void
enumerate_children_callback (GObject      *source_object,
                             GAsyncResult *res,
                             gpointer      user_data)
{
    DirectoryLoadState *state;
    GFileEnumerator *enumerator;
    GError *error;

    state = user_data;

    if (state->directory == NULL)
    {
        /* Operation was cancelled. Bail out */
        directory_load_state_free (state);
        return;
    }

    error = NULL;
    enumerator = g_file_enumerate_children_finish (G_FILE (source_object),
                                                   res, &error);

    if (enumerator == NULL)
    {
        directory_load_done (state->directory, error);
        g_error_free (error);
        directory_load_state_free (state);
        return;
    }
    else
    {
        state->enumerator = enumerator;
        g_file_enumerator_next_files_async (state->enumerator,
                                            DIRECTORY_LOAD_ITEMS_PER_CALLBACK,
                                            G_PRIORITY_DEFAULT,
                                            state->cancellable,
                                            more_files_callback,
                                            state);
    }
}


/* Start monitoring the file list if it isn't already. */
static void
start_monitoring_file_list (NautilusDirectory *directory)
{
    DirectoryLoadState *state;

    if (!directory->details->file_list_monitored)
    {
        g_assert (!directory->details->directory_load_in_progress);
        directory->details->file_list_monitored = TRUE;
        g_list_foreach (directory->details->file_list, (GFunc) nautilus_file_ref, NULL);
    }

    if (directory->details->directory_loaded ||
        directory->details->directory_load_in_progress != NULL)
    {
        return;
    }

    if (!async_job_start (directory, "file list"))
    {
        return;
    }

    mark_all_files_unconfirmed (directory);

    state = g_new0 (DirectoryLoadState, 1);
    state->directory = directory;
    state->cancellable = g_cancellable_new ();
    state->load_file_count = 0;

    g_assert (directory->details->location != NULL);
    state->load_directory_file =
        nautilus_directory_get_corresponding_file (directory);
    state->load_directory_file->details->loading_directory = TRUE;


    g_debug ("load_directory called to monitor file list of %p", directory->details->location);

    directory->details->directory_load_in_progress = state;

    g_file_enumerate_children_async (directory->details->location,
                                     NAUTILUS_FILE_DEFAULT_ATTRIBUTES,
                                     0,     /* flags */
                                     G_PRIORITY_DEFAULT,     /* prio */
                                     state->cancellable,
                                     enumerate_children_callback,
                                     state);
}

/* Stop monitoring the file list if it is being monitored. */
void
nautilus_directory_stop_monitoring_file_list (NautilusDirectory *directory)
{
    if (!directory->details->file_list_monitored)
    {
        g_assert (directory->details->directory_load_in_progress == NULL);
        return;
    }

    directory->details->file_list_monitored = FALSE;
    file_list_cancel (directory);
    g_list_foreach (directory->details->file_list, (GFunc) nautilus_file_unref, NULL);
    directory->details->directory_loaded = FALSE;
}

static void
file_list_start_or_stop (NautilusDirectory *directory)
{
    if (nautilus_directory_is_anyone_monitoring_file_list (directory))
    {
        start_monitoring_file_list (directory);
    }
    else
    {
        nautilus_directory_stop_monitoring_file_list (directory);
    }
}

void
nautilus_file_invalidate_count (NautilusFile *file)
{
    nautilus_file_invalidate_attributes (file, NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT);
}


/* Reset count. Invalidating deep counts is handled by
 * itself elsewhere because it's a relatively heavyweight and
 * special-purpose operation (see bug 5863). Also, the shallow count
 * needs to be refreshed when filtering changes, but the deep count
 * deliberately does not take filtering into account.
 */
void
nautilus_directory_invalidate_count (NautilusDirectory *directory)
{
    NautilusFile *file;

    file = nautilus_directory_get_existing_corresponding_file (directory);
    if (file != NULL)
    {
        nautilus_file_invalidate_count (file);
    }

    nautilus_file_unref (file);
}

static void
nautilus_directory_invalidate_file_attributes (NautilusDirectory      *directory,
                                               NautilusFileAttributes  file_attributes)
{
    GList *node;

    cancel_loading_attributes (directory, file_attributes);

    for (node = directory->details->file_list; node != NULL; node = node->next)
    {
        nautilus_file_invalidate_attributes_internal (NAUTILUS_FILE (node->data),
                                                      file_attributes);
    }

    if (directory->details->as_file != NULL)
    {
        nautilus_file_invalidate_attributes_internal (directory->details->as_file,
                                                      file_attributes);
    }
}

void
nautilus_directory_force_reload_internal (NautilusDirectory      *directory,
                                          NautilusFileAttributes  file_attributes)
{
    /* invalidate attributes that are getting reloaded for all files */
    nautilus_directory_invalidate_file_attributes (directory, file_attributes);

    /* Start a new directory load. */
    file_list_cancel (directory);
    directory->details->directory_loaded = FALSE;

    /* Start a new directory count. */
    nautilus_directory_invalidate_count (directory);

    add_all_files_to_work_queue (directory);
    nautilus_directory_async_state_changed (directory);
}

static gboolean
monitor_includes_file (const Monitor *monitor,
                       NautilusFile  *file)
{
    if (monitor->file == file)
    {
        return TRUE;
    }
    /* monitor->file == NULL means monitor all files. */
    if (monitor->file != NULL)
    {
        return FALSE;
    }
    if (nautilus_file_is_self_owned (file))
    {
        return FALSE;
    }
    return nautilus_file_should_show (file,
                                      monitor->monitor_hidden_files);
}

static gboolean
is_wanted_by_monitor (NautilusFile *file,
                      GList        *monitors,
                      RequestType   request_type_wanted)
{
    GList *node;

    for (node = monitors; node; node = node->next)
    {
        Monitor *monitor = node->data;
        if (REQUEST_WANTS_TYPE (monitor->request, request_type_wanted))
        {
            if (monitor_includes_file (monitor, file))
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

static gboolean
is_needy (NautilusFile *file,
          FileCheck     check_missing,
          RequestType   request_type_wanted)
{
    NautilusDirectory *directory;
    GList *node;
    ReadyCallback *callback;

    if (!(*check_missing)(file))
    {
        return FALSE;
    }

    directory = file->details->directory;
    if (directory->details->call_when_ready_counters[request_type_wanted] > 0)
    {
        node = g_hash_table_lookup (directory->details->call_when_ready_hash.unsatisfied, file);
        for (; node != NULL; node = node->next)
        {
            callback = node->data;
            if (REQUEST_WANTS_TYPE (callback->request, request_type_wanted))
            {
                return TRUE;
            }
        }

        node = g_hash_table_lookup (directory->details->call_when_ready_hash.unsatisfied, NULL);
        if (node != NULL && !nautilus_file_is_self_owned (file))
        {
            callback = node->data;
            if (REQUEST_WANTS_TYPE (callback->request, request_type_wanted))
            {
                return TRUE;
            }
        }
    }

    if (directory->details->monitor_counters[request_type_wanted] > 0)
    {
        GList *monitors;

        monitors = lookup_monitors (directory->details->monitor_table, file);
        if (is_wanted_by_monitor (file, monitors, request_type_wanted))
        {
            return TRUE;
        }

        monitors = lookup_all_files_monitors (directory->details->monitor_table);
        if (is_wanted_by_monitor (file, monitors, request_type_wanted))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static void
directory_count_stop (NautilusDirectory *directory)
{
    NautilusFile *file;

    if (directory->details->count_in_progress != NULL)
    {
        file = directory->details->count_in_progress->count_file;
        if (file != NULL)
        {
            g_assert (NAUTILUS_IS_FILE (file));
            g_assert (file->details->directory == directory);
            if (is_needy (file,
                          should_get_directory_count_now,
                          REQUEST_DIRECTORY_COUNT))
            {
                return;
            }
        }

        /* The count is not wanted, so stop it. */
        directory_count_cancel (directory);
    }
}

static guint
count_non_skipped_files (GList *list)
{
    guint count;
    GList *node;
    GFileInfo *info;

    count = 0;
    for (node = list; node != NULL; node = node->next)
    {
        info = node->data;
        if (!should_skip_file (info))
        {
            count += 1;
        }
    }
    return count;
}

static void
count_children_done (NautilusDirectory *directory,
                     NautilusFile      *count_file,
                     gboolean           succeeded,
                     int                count)
{
    g_assert (NAUTILUS_IS_FILE (count_file));

    count_file->details->directory_count_is_up_to_date = TRUE;

    /* Record either a failure or success. */
    if (!succeeded)
    {
        count_file->details->directory_count_failed = TRUE;
        count_file->details->got_directory_count = FALSE;
        count_file->details->directory_count = 0;
    }
    else
    {
        count_file->details->directory_count_failed = FALSE;
        count_file->details->got_directory_count = TRUE;
        count_file->details->directory_count = count;
    }
    directory->details->count_in_progress = NULL;

    /* Send file-changed even if count failed, so interested parties can
     * distinguish between unknowable and not-yet-known cases.
     */
    nautilus_file_changed (count_file);

    /* Start up the next one. */
    async_job_end (directory, "directory count");
    nautilus_directory_async_state_changed (directory);
}

static void
directory_count_state_free (DirectoryCountState *state)
{
    if (state->enumerator)
    {
        if (!g_file_enumerator_is_closed (state->enumerator))
        {
            g_file_enumerator_close_async (state->enumerator,
                                           0, NULL, NULL, NULL);
        }
        g_object_unref (state->enumerator);
    }
    g_object_unref (state->cancellable);
    nautilus_directory_unref (state->directory);
    g_free (state);
}

static void
count_more_files_callback (GObject      *source_object,
                           GAsyncResult *res,
                           gpointer      user_data)
{
    DirectoryCountState *state;
    NautilusDirectory *directory;
    GError *error;
    GList *files;

    state = user_data;
    directory = state->directory;

    if (g_cancellable_is_cancelled (state->cancellable))
    {
        /* Operation was cancelled. Bail out */

        async_job_end (directory, "directory count");
        nautilus_directory_async_state_changed (directory);

        directory_count_state_free (state);

        return;
    }

    g_assert (directory->details->count_in_progress != NULL);
    g_assert (directory->details->count_in_progress == state);

    error = NULL;
    files = g_file_enumerator_next_files_finish (state->enumerator,
                                                 res, &error);

    state->file_count += count_non_skipped_files (files);

    if (files == NULL)
    {
        count_children_done (directory, state->count_file,
                             TRUE, state->file_count);
        directory_count_state_free (state);
    }
    else
    {
        g_file_enumerator_next_files_async (state->enumerator,
                                            DIRECTORY_LOAD_ITEMS_PER_CALLBACK,
                                            G_PRIORITY_DEFAULT,
                                            state->cancellable,
                                            count_more_files_callback,
                                            state);
    }

    g_list_free_full (files, g_object_unref);

    if (error)
    {
        g_error_free (error);
    }
}

static void
count_children_callback (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
    DirectoryCountState *state;
    GFileEnumerator *enumerator;
    NautilusDirectory *directory;
    GError *error;

    state = user_data;

    if (g_cancellable_is_cancelled (state->cancellable))
    {
        /* Operation was cancelled. Bail out */
        directory = state->directory;

        async_job_end (directory, "directory count");
        nautilus_directory_async_state_changed (directory);

        directory_count_state_free (state);

        return;
    }

    error = NULL;
    enumerator = g_file_enumerate_children_finish (G_FILE (source_object),
                                                   res, &error);

    if (enumerator == NULL)
    {
        count_children_done (state->directory,
                             state->count_file,
                             FALSE, 0);
        g_error_free (error);
        directory_count_state_free (state);
        return;
    }
    else
    {
        state->enumerator = enumerator;
        g_file_enumerator_next_files_async (state->enumerator,
                                            DIRECTORY_LOAD_ITEMS_PER_CALLBACK,
                                            G_PRIORITY_DEFAULT,
                                            state->cancellable,
                                            count_more_files_callback,
                                            state);
    }
}

static void
directory_count_start (NautilusDirectory *directory,
                       NautilusFile      *file,
                       gboolean          *doing_io)
{
    DirectoryCountState *state;
    GFile *location;

    if (directory->details->count_in_progress != NULL)
    {
        *doing_io = TRUE;
        return;
    }

    if (!is_needy (file,
                   should_get_directory_count_now,
                   REQUEST_DIRECTORY_COUNT))
    {
        return;
    }
    *doing_io = TRUE;

    if (!nautilus_file_is_directory (file))
    {
        file->details->directory_count_is_up_to_date = TRUE;
        file->details->directory_count_failed = FALSE;
        file->details->got_directory_count = FALSE;

        nautilus_directory_async_state_changed (directory);
        return;
    }

    if (!async_job_start (directory, "directory count"))
    {
        return;
    }

    /* Start counting. */
    state = g_new0 (DirectoryCountState, 1);
    state->count_file = file;
    state->directory = nautilus_directory_ref (directory);
    state->cancellable = g_cancellable_new ();

    directory->details->count_in_progress = state;

    location = nautilus_file_get_location (file);

    {
        g_autofree char *uri = NULL;
        uri = g_file_get_uri (location);
        g_debug ("load_directory called to get shallow file count for %s", uri);
    }

    g_file_enumerate_children_async (location,
                                     G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                     G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","
                                     G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP,
                                     G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,     /* flags */
                                     G_PRIORITY_DEFAULT,     /* prio */
                                     state->cancellable,
                                     count_children_callback,
                                     state);
    g_object_unref (location);
}

static inline gboolean
seen_inode (DeepCountState *state,
            GFileInfo      *info)
{
    guint64 inode, inode2;
    guint i;

    inode = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_UNIX_INODE);

    if (inode != 0)
    {
        for (i = 0; i < state->seen_deep_count_inodes->len; i++)
        {
            inode2 = g_array_index (state->seen_deep_count_inodes, guint64, i);
            if (inode == inode2)
            {
                return TRUE;
            }
        }
    }

    return FALSE;
}

static inline void
mark_inode_as_seen (DeepCountState *state,
                    GFileInfo      *info)
{
    guint64 inode;

    inode = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_UNIX_INODE);
    if (inode != 0)
    {
        g_array_append_val (state->seen_deep_count_inodes, inode);
    }
}

static void
deep_count_one (DeepCountState *state,
                GFileInfo      *info)
{
    NautilusFile *file;
    GFile *subdir;
    gboolean is_seen_inode;
    const char *fs_id;

    is_seen_inode = seen_inode (state, info);
    if (!is_seen_inode)
    {
        mark_inode_as_seen (state, info);
    }

    file = state->directory->details->deep_count_file;

    if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
    {
        /* Count the directory. */
        file->details->deep_directory_count += 1;

        /* Record the fact that we have to descend into this directory. */
        fs_id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
        if (g_strcmp0 (fs_id, state->fs_id) == 0)
        {
            /* only if it is on the same filesystem */
            subdir = g_file_get_child (state->deep_count_location, g_file_info_get_name (info));
            state->deep_count_subdirectories = g_list_prepend
                                                   (state->deep_count_subdirectories, subdir);
        }
    }
    else
    {
        /* Even non-regular files count as files. */
        file->details->deep_file_count += 1;
    }

    /* Count the size. */
    if (!is_seen_inode && g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_SIZE))
    {
        file->details->deep_size += g_file_info_get_size (info);
    }
}

static void
deep_count_state_free (DeepCountState *state)
{
    if (state->enumerator)
    {
        if (!g_file_enumerator_is_closed (state->enumerator))
        {
            g_file_enumerator_close_async (state->enumerator,
                                           0, NULL, NULL, NULL);
        }
        g_object_unref (state->enumerator);
    }
    g_object_unref (state->cancellable);
    if (state->deep_count_location)
    {
        g_object_unref (state->deep_count_location);
    }
    g_list_free_full (state->deep_count_subdirectories, g_object_unref);
    g_array_free (state->seen_deep_count_inodes, TRUE);
    g_free (state->fs_id);
    g_free (state);
}

static void
deep_count_next_dir (DeepCountState *state)
{
    GFile *location;
    NautilusFile *file;
    NautilusDirectory *directory;
    gboolean done;

    directory = state->directory;

    g_object_unref (state->deep_count_location);
    state->deep_count_location = NULL;

    done = FALSE;
    file = directory->details->deep_count_file;

    if (state->deep_count_subdirectories != NULL)
    {
        /* Work on a new directory. */
        location = state->deep_count_subdirectories->data;
        state->deep_count_subdirectories = g_list_remove
                                               (state->deep_count_subdirectories, location);
        deep_count_load (state, location);
        g_object_unref (location);
    }
    else
    {
        file->details->deep_counts_status = NAUTILUS_REQUEST_DONE;
        directory->details->deep_count_file = NULL;
        directory->details->deep_count_in_progress = NULL;
        deep_count_state_free (state);
        done = TRUE;
    }

    nautilus_file_updated_deep_count_in_progress (file);

    if (done)
    {
        nautilus_file_changed (file);
        async_job_end (directory, "deep count");
        nautilus_directory_async_state_changed (directory);
    }
}

static void
deep_count_more_files_callback (GObject      *source_object,
                                GAsyncResult *res,
                                gpointer      user_data)
{
    DeepCountState *state;
    NautilusDirectory *directory;
    GList *files, *l;
    GFileInfo *info;

    state = user_data;

    if (state->directory == NULL)
    {
        /* Operation was cancelled. Bail out */
        deep_count_state_free (state);
        return;
    }

    directory = nautilus_directory_ref (state->directory);

    g_assert (directory->details->deep_count_in_progress != NULL);
    g_assert (directory->details->deep_count_in_progress == state);

    files = g_file_enumerator_next_files_finish (state->enumerator,
                                                 res, NULL);

    for (l = files; l != NULL; l = l->next)
    {
        info = l->data;
        deep_count_one (state, info);
        g_object_unref (info);
    }

    if (files == NULL)
    {
        g_file_enumerator_close_async (state->enumerator, 0, NULL, NULL, NULL);
        g_object_unref (state->enumerator);
        state->enumerator = NULL;

        deep_count_next_dir (state);
    }
    else
    {
        g_file_enumerator_next_files_async (state->enumerator,
                                            DIRECTORY_LOAD_ITEMS_PER_CALLBACK,
                                            G_PRIORITY_LOW,
                                            state->cancellable,
                                            deep_count_more_files_callback,
                                            state);
    }

    g_list_free (files);

    nautilus_directory_unref (directory);
}

static void
deep_count_callback (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
    DeepCountState *state;
    GFileEnumerator *enumerator;
    NautilusFile *file;

    state = user_data;

    if (state->directory == NULL)
    {
        /* Operation was cancelled. Bail out */
        deep_count_state_free (state);
        return;
    }

    file = state->directory->details->deep_count_file;

    enumerator = g_file_enumerate_children_finish (G_FILE (source_object), res, NULL);

    if (enumerator == NULL)
    {
        file->details->deep_unreadable_count += 1;

        deep_count_next_dir (state);
    }
    else
    {
        state->enumerator = enumerator;
        g_file_enumerator_next_files_async (state->enumerator,
                                            DIRECTORY_LOAD_ITEMS_PER_CALLBACK,
                                            G_PRIORITY_LOW,
                                            state->cancellable,
                                            deep_count_more_files_callback,
                                            state);
    }
}


static void
deep_count_load (DeepCountState *state,
                 GFile          *location)
{
    state->deep_count_location = g_object_ref (location);

    g_debug ("load_directory called to get deep file count for %p", location);
    g_file_enumerate_children_async (state->deep_count_location,
                                     G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                     G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                     G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                     G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","
                                     G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP ","
                                     G_FILE_ATTRIBUTE_ID_FILESYSTEM ","
                                     G_FILE_ATTRIBUTE_UNIX_INODE,
                                     G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,     /* flags */
                                     G_PRIORITY_LOW,     /* prio */
                                     state->cancellable,
                                     deep_count_callback,
                                     state);
}

static void
deep_count_stop (NautilusDirectory *directory)
{
    NautilusFile *file;

    if (directory->details->deep_count_in_progress != NULL)
    {
        file = directory->details->deep_count_file;
        if (file != NULL)
        {
            g_assert (NAUTILUS_IS_FILE (file));
            g_assert (file->details->directory == directory);
            if (is_needy (file,
                          lacks_deep_count,
                          REQUEST_DEEP_COUNT))
            {
                return;
            }
        }

        /* The count is not wanted, so stop it. */
        deep_count_cancel (directory);
    }
}

static void
deep_count_got_info (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
    GFileInfo *info;
    const char *id;
    GFile *file = (GFile *) source_object;
    DeepCountState *state = (DeepCountState *) user_data;

    info = g_file_query_info_finish (file, res, NULL);
    if (info != NULL)
    {
        id = g_file_info_get_attribute_string (info, G_FILE_ATTRIBUTE_ID_FILESYSTEM);
        state->fs_id = g_strdup (id);
        g_object_unref (info);
    }
    deep_count_load (state, file);
}

static void
deep_count_start (NautilusDirectory *directory,
                  NautilusFile      *file,
                  gboolean          *doing_io)
{
    GFile *location;
    DeepCountState *state;

    if (directory->details->deep_count_in_progress != NULL)
    {
        *doing_io = TRUE;
        return;
    }

    if (!is_needy (file,
                   lacks_deep_count,
                   REQUEST_DEEP_COUNT))
    {
        return;
    }
    *doing_io = TRUE;

    if (!nautilus_file_is_directory (file))
    {
        file->details->deep_counts_status = NAUTILUS_REQUEST_DONE;

        nautilus_directory_async_state_changed (directory);
        return;
    }

    if (!async_job_start (directory, "deep count"))
    {
        return;
    }

    /* Start counting. */
    file->details->deep_counts_status = NAUTILUS_REQUEST_IN_PROGRESS;
    file->details->deep_directory_count = 0;
    file->details->deep_file_count = 0;
    file->details->deep_unreadable_count = 0;
    file->details->deep_size = 0;
    directory->details->deep_count_file = file;

    state = g_new0 (DeepCountState, 1);
    state->directory = directory;
    state->cancellable = g_cancellable_new ();
    state->seen_deep_count_inodes = g_array_new (FALSE, TRUE, sizeof (guint64));
    state->fs_id = NULL;

    directory->details->deep_count_in_progress = state;

    location = nautilus_file_get_location (file);
    g_file_query_info_async (location,
                             G_FILE_ATTRIBUTE_ID_FILESYSTEM,
                             G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                             G_PRIORITY_DEFAULT,
                             NULL,
                             deep_count_got_info,
                             state);
    g_object_unref (location);
}

static void
get_info_state_free (GetInfoState *state)
{
    g_object_unref (state->cancellable);
    g_free (state);
}

static void
query_info_callback (GObject      *source_object,
                     GAsyncResult *res,
                     gpointer      user_data)
{
    NautilusDirectory *directory;
    NautilusFile *get_info_file;
    GFileInfo *info;
    GetInfoState *state;
    GError *error;

    state = user_data;

    if (state->directory == NULL)
    {
        /* Operation was cancelled. Bail out */
        get_info_state_free (state);
        return;
    }

    directory = nautilus_directory_ref (state->directory);

    get_info_file = directory->details->get_info_file;
    g_assert (NAUTILUS_IS_FILE (get_info_file));

    directory->details->get_info_file = NULL;
    directory->details->get_info_in_progress = NULL;

    /* ref here because we might be removing the last ref when we
     * mark the file gone below, but we need to keep a ref at
     * least long enough to send the change notification.
     */
    nautilus_file_ref (get_info_file);

    error = NULL;
    info = g_file_query_info_finish (G_FILE (source_object), res, &error);

    if (info == NULL)
    {
        if (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_FOUND)
        {
            /* mark file as gone */
            nautilus_file_mark_gone (get_info_file);
        }
        get_info_file->details->file_info_is_up_to_date = TRUE;
        nautilus_file_clear_info (get_info_file);
        get_info_file->details->get_info_failed = TRUE;
        get_info_file->details->get_info_error = error;
    }
    else
    {
        nautilus_file_update_info (get_info_file, info);
        g_object_unref (info);
    }

    nautilus_file_changed (get_info_file);
    nautilus_file_unref (get_info_file);

    async_job_end (directory, "file info");
    nautilus_directory_async_state_changed (directory);

    nautilus_directory_unref (directory);

    get_info_state_free (state);
}

static void
file_info_stop (NautilusDirectory *directory)
{
    NautilusFile *file;

    if (directory->details->get_info_in_progress != NULL)
    {
        file = directory->details->get_info_file;
        if (file != NULL)
        {
            g_assert (NAUTILUS_IS_FILE (file));
            g_assert (file->details->directory == directory);
            if (is_needy (file, lacks_info, REQUEST_FILE_INFO))
            {
                return;
            }
        }

        /* The info is not wanted, so stop it. */
        file_info_cancel (directory);
    }
}

static void
file_info_start (NautilusDirectory *directory,
                 NautilusFile      *file,
                 gboolean          *doing_io)
{
    GFile *location;
    GetInfoState *state;

    file_info_stop (directory);

    if (directory->details->get_info_in_progress != NULL)
    {
        *doing_io = TRUE;
        return;
    }

    if (!is_needy (file, lacks_info, REQUEST_FILE_INFO))
    {
        return;
    }
    *doing_io = TRUE;

    if (!async_job_start (directory, "file info"))
    {
        return;
    }

    directory->details->get_info_file = file;
    file->details->get_info_failed = FALSE;
    if (file->details->get_info_error)
    {
        g_error_free (file->details->get_info_error);
        file->details->get_info_error = NULL;
    }

    state = g_new (GetInfoState, 1);
    state->directory = directory;
    state->cancellable = g_cancellable_new ();

    directory->details->get_info_in_progress = state;

    location = nautilus_file_get_location (file);
    g_file_query_info_async (location,
                             NAUTILUS_FILE_DEFAULT_ATTRIBUTES,
                             0,
                             G_PRIORITY_DEFAULT,
                             state->cancellable, query_info_callback, state);
    g_object_unref (location);
}

static void
thumbnail_done (NautilusDirectory *directory,
                NautilusFile      *file,
                GdkPixbuf         *pixbuf)
{
    if (!nautilus_file_set_thumbnail (file, pixbuf))
    {
        g_clear_pointer (&file->details->thumbnail_path, g_free);
    }

    nautilus_directory_async_state_changed (directory);
}

static void
thumbnail_stop (NautilusDirectory *directory)
{
    NautilusFile *file;

    if (directory->details->thumbnail_state != NULL)
    {
        file = directory->details->thumbnail_state->file;

        if (file != NULL)
        {
            g_assert (NAUTILUS_IS_FILE (file));
            g_assert (file->details->directory == directory);
            if (is_needy (file,
                          lacks_thumbnail,
                          REQUEST_THUMBNAIL))
            {
                return;
            }
        }

        /* The link info is not wanted, so stop it. */
        thumbnail_cancel (directory);
    }
}

static void
thumbnail_got_pixbuf (NautilusDirectory *directory,
                      NautilusFile      *file,
                      GdkPixbuf         *pixbuf)
{
    nautilus_directory_ref (directory);

    nautilus_file_ref (file);
    thumbnail_done (directory, file, pixbuf);
    nautilus_file_changed (file);
    nautilus_file_unref (file);

    if (pixbuf)
    {
        g_object_unref (pixbuf);
    }

    nautilus_directory_unref (directory);
}

static void
thumbnail_state_free (ThumbnailState *state)
{
    g_object_unref (state->cancellable);
    g_free (state);
}

/* scale very large images down to the max. size we need */
static void
thumbnail_loader_size_prepared (GdkPixbufLoader *loader,
                                int              width,
                                int              height,
                                gpointer         user_data)
{
    int max_thumbnail_size;
    double aspect_ratio;

    aspect_ratio = ((double) width) / height;

    /* cf. nautilus_file_get_icon() */
    max_thumbnail_size = NAUTILUS_GRID_ICON_SIZE_EXTRA_LARGE * NAUTILUS_GRID_ICON_SIZE_MEDIUM / NAUTILUS_GRID_ICON_SIZE_SMALL;
    if (MAX (width, height) > max_thumbnail_size)
    {
        if (width > height)
        {
            width = max_thumbnail_size;
            height = width / aspect_ratio;
        }
        else
        {
            height = max_thumbnail_size;
            width = height * aspect_ratio;
        }

        gdk_pixbuf_loader_set_size (loader, width, height);
    }
}

static GdkPixbuf *
get_pixbuf_for_content (goffset  file_len,
                        char    *file_contents)
{
    gboolean res;
    GdkPixbuf *pixbuf, *pixbuf2;
    GdkPixbufLoader *loader;
    pixbuf = NULL;

    loader = gdk_pixbuf_loader_new ();
    g_signal_connect (loader, "size-prepared",
                      G_CALLBACK (thumbnail_loader_size_prepared),
                      NULL);

    res = TRUE;
    if (file_len > 0)
    {
        res = gdk_pixbuf_loader_write (loader, (guchar *) file_contents, file_len, NULL);
    }
    if (res)
    {
        res = gdk_pixbuf_loader_close (loader, NULL);
    }
    if (res)
    {
        pixbuf = g_object_ref (gdk_pixbuf_loader_get_pixbuf (loader));
    }
    g_object_unref (G_OBJECT (loader));

    if (pixbuf)
    {
        pixbuf2 = gdk_pixbuf_apply_embedded_orientation (pixbuf);
        g_object_unref (pixbuf);
        pixbuf = pixbuf2;
    }
    return pixbuf;
}


static void
thumbnail_read_callback (GObject      *source_object,
                         GAsyncResult *res,
                         gpointer      user_data)
{
    ThumbnailState *state;
    gsize file_size;
    char *file_contents;
    gboolean result;
    NautilusDirectory *directory;
    GdkPixbuf *pixbuf;

    state = user_data;

    if (state->directory == NULL)
    {
        /* Operation was cancelled. Bail out */
        thumbnail_state_free (state);
        return;
    }

    directory = nautilus_directory_ref (state->directory);

    result = g_file_load_contents_finish (G_FILE (source_object),
                                          res,
                                          &file_contents, &file_size,
                                          NULL, NULL);

    pixbuf = NULL;
    if (result)
    {
        pixbuf = get_pixbuf_for_content (file_size, file_contents);
        g_free (file_contents);
    }

    state->directory->details->thumbnail_state = NULL;
    async_job_end (state->directory, "thumbnail");

    thumbnail_got_pixbuf (state->directory, state->file, pixbuf);

    thumbnail_state_free (state);

    nautilus_directory_unref (directory);
}

static void
thumbnail_start (NautilusDirectory *directory,
                 NautilusFile      *file,
                 gboolean          *doing_io)
{
    GFile *location;
    ThumbnailState *state;

    if (directory->details->thumbnail_state != NULL)
    {
        *doing_io = TRUE;
        return;
    }

    if (!is_needy (file,
                   lacks_thumbnail,
                   REQUEST_THUMBNAIL))
    {
        return;
    }
    *doing_io = TRUE;

    if (!async_job_start (directory, "thumbnail"))
    {
        return;
    }

    state = g_new0 (ThumbnailState, 1);
    state->directory = directory;
    state->file = file;
    state->cancellable = g_cancellable_new ();

    location = g_file_new_for_path (file->details->thumbnail_path);

    directory->details->thumbnail_state = state;

    g_file_load_contents_async (location,
                                state->cancellable,
                                thumbnail_read_callback,
                                state);
    g_object_unref (location);
}

static void
mount_stop (NautilusDirectory *directory)
{
    NautilusFile *file;

    if (directory->details->mount_state != NULL)
    {
        file = directory->details->mount_state->file;

        if (file != NULL)
        {
            g_assert (NAUTILUS_IS_FILE (file));
            g_assert (file->details->directory == directory);
            if (is_needy (file,
                          lacks_mount,
                          REQUEST_MOUNT))
            {
                return;
            }
        }

        /* The link info is not wanted, so stop it. */
        mount_cancel (directory);
    }
}

static void
mount_state_free (MountState *state)
{
    g_object_unref (state->cancellable);
    g_free (state);
}

static void
got_mount (MountState *state,
           GMount     *mount)
{
    NautilusDirectory *directory;
    NautilusFile *file;

    directory = nautilus_directory_ref (state->directory);

    state->directory->details->mount_state = NULL;
    async_job_end (state->directory, "mount");

    file = nautilus_file_ref (state->file);

    file->details->mount_is_up_to_date = TRUE;
    nautilus_file_set_mount (file, mount);

    nautilus_directory_async_state_changed (directory);
    nautilus_file_changed (file);

    nautilus_file_unref (file);

    nautilus_directory_unref (directory);

    mount_state_free (state);
}

static void
find_enclosing_mount_callback (GObject      *source_object,
                               GAsyncResult *res,
                               gpointer      user_data)
{
    GMount *mount;
    MountState *state;
    GFile *location, *root;

    state = user_data;
    if (state->directory == NULL)
    {
        /* Operation was cancelled. Bail out */
        mount_state_free (state);
        return;
    }

    mount = g_file_find_enclosing_mount_finish (G_FILE (source_object),
                                                res, NULL);

    if (mount)
    {
        root = g_mount_get_root (mount);
        location = nautilus_file_get_location (state->file);
        if (!g_file_equal (location, root))
        {
            g_object_unref (mount);
            mount = NULL;
        }
        g_object_unref (root);
        g_object_unref (location);
    }

    got_mount (state, mount);

    if (mount)
    {
        g_object_unref (mount);
    }
}

static void
mount_start (NautilusDirectory *directory,
             NautilusFile      *file,
             gboolean          *doing_io)
{
    GFile *location;
    MountState *state;

    if (directory->details->mount_state != NULL)
    {
        *doing_io = TRUE;
        return;
    }

    if (!is_needy (file,
                   lacks_mount,
                   REQUEST_MOUNT))
    {
        return;
    }
    *doing_io = TRUE;

    if (!async_job_start (directory, "mount"))
    {
        return;
    }

    state = g_new0 (MountState, 1);
    state->directory = directory;
    state->file = file;
    state->cancellable = g_cancellable_new ();

    location = nautilus_file_get_location (file);

    directory->details->mount_state = state;

    if (file->details->type == G_FILE_TYPE_MOUNTABLE)
    {
        GFile *target;
        GMount *mount;

        mount = NULL;
        target = nautilus_file_get_activation_location (file);
        if (target != NULL)
        {
            mount = nautilus_get_mounted_mount_for_root (target);
            g_object_unref (target);
        }

        got_mount (state, mount);

        if (mount)
        {
            g_object_unref (mount);
        }
    }
    else
    {
        g_file_find_enclosing_mount_async (location,
                                           G_PRIORITY_DEFAULT,
                                           state->cancellable,
                                           find_enclosing_mount_callback,
                                           state);
    }
    g_object_unref (location);
}

static void
filesystem_info_cancel (NautilusDirectory *directory)
{
    if (directory->details->filesystem_info_state != NULL)
    {
        g_cancellable_cancel (directory->details->filesystem_info_state->cancellable);
        directory->details->filesystem_info_state->directory = NULL;
        directory->details->filesystem_info_state = NULL;
        async_job_end (directory, "filesystem info");
    }
}

static void
filesystem_info_stop (NautilusDirectory *directory)
{
    NautilusFile *file;

    if (directory->details->filesystem_info_state != NULL)
    {
        file = directory->details->filesystem_info_state->file;

        if (file != NULL)
        {
            g_assert (NAUTILUS_IS_FILE (file));
            g_assert (file->details->directory == directory);
            if (is_needy (file,
                          lacks_filesystem_info,
                          REQUEST_FILESYSTEM_INFO))
            {
                return;
            }
        }

        /* The filesystem info is not wanted, so stop it. */
        filesystem_info_cancel (directory);
    }
}

static void
filesystem_info_state_free (FilesystemInfoState *state)
{
    g_object_unref (state->cancellable);
    g_free (state);
}

static void
got_filesystem_info (FilesystemInfoState *state,
                     GFileInfo           *info)
{
    NautilusDirectory *directory;
    NautilusFile *file;

    /* careful here, info may be NULL */

    directory = nautilus_directory_ref (state->directory);

    state->directory->details->filesystem_info_state = NULL;
    async_job_end (state->directory, "filesystem info");

    file = nautilus_file_ref (state->file);

    file->details->filesystem_info_is_up_to_date = TRUE;
    if (info != NULL)
    {
        file->details->filesystem_use_preview =
            g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_FILESYSTEM_USE_PREVIEW);
        file->details->filesystem_readonly =
            g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_FILESYSTEM_READONLY);
        file->details->filesystem_remote = g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE);
    }

    nautilus_directory_async_state_changed (directory);
    nautilus_file_changed (file);

    nautilus_file_unref (file);

    nautilus_directory_unref (directory);

    filesystem_info_state_free (state);
}

static void
query_filesystem_info_callback (GObject      *source_object,
                                GAsyncResult *res,
                                gpointer      user_data)
{
    GFileInfo *info;
    FilesystemInfoState *state;

    state = user_data;
    if (state->directory == NULL)
    {
        /* Operation was cancelled. Bail out */
        filesystem_info_state_free (state);
        return;
    }

    info = g_file_query_filesystem_info_finish (G_FILE (source_object), res, NULL);

    got_filesystem_info (state, info);

    if (info != NULL)
    {
        g_object_unref (info);
    }
}

static void
filesystem_info_start (NautilusDirectory *directory,
                       NautilusFile      *file,
                       gboolean          *doing_io)
{
    GFile *location;
    FilesystemInfoState *state;

    if (directory->details->filesystem_info_state != NULL)
    {
        *doing_io = TRUE;
        return;
    }

    if (!is_needy (file,
                   lacks_filesystem_info,
                   REQUEST_FILESYSTEM_INFO))
    {
        return;
    }
    *doing_io = TRUE;

    if (!async_job_start (directory, "filesystem info"))
    {
        return;
    }

    state = g_new0 (FilesystemInfoState, 1);
    state->directory = directory;
    state->file = file;
    state->cancellable = g_cancellable_new ();

    location = nautilus_file_get_location (file);

    directory->details->filesystem_info_state = state;

    g_file_query_filesystem_info_async (location,
                                        G_FILE_ATTRIBUTE_FILESYSTEM_READONLY ","
                                        G_FILE_ATTRIBUTE_FILESYSTEM_USE_PREVIEW ","
                                        G_FILE_ATTRIBUTE_FILESYSTEM_REMOTE,
                                        G_PRIORITY_DEFAULT,
                                        state->cancellable,
                                        query_filesystem_info_callback,
                                        state);
    g_object_unref (location);
}

static void
extension_info_cancel (NautilusDirectory *directory)
{
    if (directory->details->extension_info_in_progress != NULL)
    {
        if (directory->details->extension_info_idle)
        {
            g_source_remove (directory->details->extension_info_idle);
        }
        else
        {
            nautilus_info_provider_cancel_update
                (directory->details->extension_info_provider,
                directory->details->extension_info_in_progress);
        }

        directory->details->extension_info_in_progress = NULL;
        directory->details->extension_info_file = NULL;
        directory->details->extension_info_provider = NULL;
        directory->details->extension_info_idle = 0;

        async_job_end (directory, "extension info");
    }
}

static void
extension_info_stop (NautilusDirectory *directory)
{
    if (directory->details->extension_info_in_progress != NULL)
    {
        NautilusFile *file;

        file = directory->details->extension_info_file;
        if (file != NULL)
        {
            g_assert (NAUTILUS_IS_FILE (file));
            g_assert (file->details->directory == directory);
            if (is_needy (file, lacks_extension_info, REQUEST_EXTENSION_INFO))
            {
                return;
            }
        }

        /* The info is not wanted, so stop it. */
        extension_info_cancel (directory);
    }
}

static void
finish_info_provider (NautilusDirectory    *directory,
                      NautilusFile         *file,
                      NautilusInfoProvider *provider)
{
    file->details->pending_info_providers =
        g_list_remove (file->details->pending_info_providers,
                       provider);
    g_object_unref (provider);

    nautilus_directory_async_state_changed (directory);

    if (file->details->pending_info_providers == NULL)
    {
        nautilus_file_info_providers_done (file);
    }
}


static gboolean
info_provider_idle_callback (gpointer user_data)
{
    InfoProviderResponse *response;
    NautilusDirectory *directory;

    response = user_data;
    directory = response->directory;

    if (response->handle != directory->details->extension_info_in_progress
        || response->provider != directory->details->extension_info_provider)
    {
        g_warning ("Unexpected plugin response.  This probably indicates a bug in a Nautilus extension: handle=%p", response->handle);
    }
    else
    {
        NautilusFile *file;
        async_job_end (directory, "extension info");

        file = directory->details->extension_info_file;

        directory->details->extension_info_file = NULL;
        directory->details->extension_info_provider = NULL;
        directory->details->extension_info_in_progress = NULL;
        directory->details->extension_info_idle = 0;

        finish_info_provider (directory, file, response->provider);
    }

    return FALSE;
}

static void
info_provider_callback (NautilusInfoProvider    *provider,
                        NautilusOperationHandle *handle,
                        NautilusOperationResult  result,
                        gpointer                 user_data)
{
    InfoProviderResponse *response;

    response = g_new0 (InfoProviderResponse, 1);
    response->provider = provider;
    response->handle = handle;
    response->result = result;
    response->directory = NAUTILUS_DIRECTORY (user_data);

    response->directory->details->extension_info_idle =
        g_idle_add_full (G_PRIORITY_DEFAULT_IDLE,
                         info_provider_idle_callback, response,
                         g_free);
}

static void
extension_info_start (NautilusDirectory *directory,
                      NautilusFile      *file,
                      gboolean          *doing_io)
{
    NautilusInfoProvider *provider;
    NautilusOperationResult result;
    NautilusOperationHandle *handle;
    GClosure *update_complete;

    if (directory->details->extension_info_in_progress != NULL)
    {
        *doing_io = TRUE;
        return;
    }

    if (!is_needy (file, lacks_extension_info, REQUEST_EXTENSION_INFO))
    {
        return;
    }
    *doing_io = TRUE;

    if (!async_job_start (directory, "extension info"))
    {
        return;
    }

    provider = file->details->pending_info_providers->data;

    update_complete = g_cclosure_new (G_CALLBACK (info_provider_callback),
                                      directory,
                                      NULL);
    g_closure_set_marshal (update_complete,
                           g_cclosure_marshal_generic);

    result = nautilus_info_provider_update_file_info
                 (provider,
                 NAUTILUS_FILE_INFO (file),
                 update_complete,
                 &handle);

    g_closure_unref (update_complete);

    if (result == NAUTILUS_OPERATION_COMPLETE ||
        result == NAUTILUS_OPERATION_FAILED)
    {
        finish_info_provider (directory, file, provider);
        async_job_end (directory, "extension info");
    }
    else
    {
        directory->details->extension_info_in_progress = handle;
        directory->details->extension_info_provider = provider;
        directory->details->extension_info_file = file;
    }
}

static void
start_or_stop_io (NautilusDirectory *directory)
{
    NautilusFile *file;
    gboolean doing_io;

    /* Start or stop reading files. */
    file_list_start_or_stop (directory);

    /* Stop any no longer wanted attribute fetches. */
    file_info_stop (directory);
    directory_count_stop (directory);
    deep_count_stop (directory);
    extension_info_stop (directory);
    mount_stop (directory);
    thumbnail_stop (directory);
    filesystem_info_stop (directory);

    doing_io = FALSE;
    /* Take files that are all done off the queue. */
    while (!nautilus_file_queue_is_empty (directory->details->high_priority_queue))
    {
        file = nautilus_file_queue_head (directory->details->high_priority_queue);

        /* Start getting attributes if possible */
        file_info_start (directory, file, &doing_io);

        if (doing_io)
        {
            return;
        }

        move_file_to_low_priority_queue (directory, file);
    }

    /* High priority queue must be empty */
    while (!nautilus_file_queue_is_empty (directory->details->low_priority_queue))
    {
        file = nautilus_file_queue_head (directory->details->low_priority_queue);

        /* Start getting attributes if possible */
        mount_start (directory, file, &doing_io);
        directory_count_start (directory, file, &doing_io);
        deep_count_start (directory, file, &doing_io);
        thumbnail_start (directory, file, &doing_io);
        filesystem_info_start (directory, file, &doing_io);

        if (doing_io)
        {
            return;
        }

        move_file_to_extension_queue (directory, file);
    }

    /* Low priority queue must be empty */
    while (!nautilus_file_queue_is_empty (directory->details->extension_queue))
    {
        file = nautilus_file_queue_head (directory->details->extension_queue);

        /* Start getting attributes if possible */
        extension_info_start (directory, file, &doing_io);
        if (doing_io)
        {
            return;
        }

        nautilus_directory_remove_file_from_work_queue (directory, file);
    }
}

/* Call this when the monitor or call when ready list changes,
 * or when some I/O is completed.
 */
void
nautilus_directory_async_state_changed (NautilusDirectory *directory)
{
    /* Check if any callbacks are satisfied and call them if they
     * are. Do this last so that any changes done in start or stop
     * I/O functions immediately (not in callbacks) are taken into
     * consideration. If any callbacks are called, consider the
     * I/O state again so that we can release or cancel I/O that
     * is not longer needed once the callbacks are satisfied.
     */

    if (directory->details->in_async_service_loop)
    {
        directory->details->state_changed = TRUE;
        return;
    }
    directory->details->in_async_service_loop = TRUE;
    nautilus_directory_ref (directory);
    do
    {
        directory->details->state_changed = FALSE;
        start_or_stop_io (directory);
        if (call_ready_callbacks (directory))
        {
            directory->details->state_changed = TRUE;
        }
    }
    while (directory->details->state_changed);
    directory->details->in_async_service_loop = FALSE;
    nautilus_directory_unref (directory);

    /* Check if any directories should wake up. */
    async_job_wake_up ();
}

void
nautilus_directory_cancel (NautilusDirectory *directory)
{
    /* Arbitrary order (kept alphabetical). */
    deep_count_cancel (directory);
    directory_count_cancel (directory);
    file_info_cancel (directory);
    file_list_cancel (directory);
    new_files_cancel (directory);
    extension_info_cancel (directory);
    thumbnail_cancel (directory);
    mount_cancel (directory);
    filesystem_info_cancel (directory);

    /* We aren't waiting for anything any more. */
    if (waiting_directories != NULL)
    {
        g_hash_table_remove (waiting_directories, directory);
    }

    /* Check if any directories should wake up. */
    async_job_wake_up ();
}

static void
cancel_directory_count_for_file (NautilusDirectory *directory,
                                 NautilusFile      *file)
{
    if (directory->details->count_in_progress != NULL &&
        directory->details->count_in_progress->count_file == file)
    {
        directory_count_cancel (directory);
    }
}

static void
cancel_deep_counts_for_file (NautilusDirectory *directory,
                             NautilusFile      *file)
{
    if (directory->details->deep_count_file == file)
    {
        deep_count_cancel (directory);
    }
}

static void
cancel_file_info_for_file (NautilusDirectory *directory,
                           NautilusFile      *file)
{
    if (directory->details->get_info_file == file)
    {
        file_info_cancel (directory);
    }
}

static void
cancel_thumbnail_for_file (NautilusDirectory *directory,
                           NautilusFile      *file)
{
    if (directory->details->thumbnail_state != NULL &&
        directory->details->thumbnail_state->file == file)
    {
        thumbnail_cancel (directory);
    }
}

static void
cancel_mount_for_file (NautilusDirectory *directory,
                       NautilusFile      *file)
{
    if (directory->details->mount_state != NULL &&
        directory->details->mount_state->file == file)
    {
        mount_cancel (directory);
    }
}

static void
cancel_filesystem_info_for_file (NautilusDirectory *directory,
                                 NautilusFile      *file)
{
    if (directory->details->filesystem_info_state != NULL &&
        directory->details->filesystem_info_state->file == file)
    {
        filesystem_info_cancel (directory);
    }
}

static void
cancel_loading_attributes (NautilusDirectory      *directory,
                           NautilusFileAttributes  file_attributes)
{
    Request request;

    request = nautilus_directory_set_up_request (file_attributes);

    if (REQUEST_WANTS_TYPE (request, REQUEST_DIRECTORY_COUNT))
    {
        directory_count_cancel (directory);
    }
    if (REQUEST_WANTS_TYPE (request, REQUEST_DEEP_COUNT))
    {
        deep_count_cancel (directory);
    }
    if (REQUEST_WANTS_TYPE (request, REQUEST_FILE_INFO))
    {
        file_info_cancel (directory);
    }
    if (REQUEST_WANTS_TYPE (request, REQUEST_FILESYSTEM_INFO))
    {
        filesystem_info_cancel (directory);
    }
    if (REQUEST_WANTS_TYPE (request, REQUEST_EXTENSION_INFO))
    {
        extension_info_cancel (directory);
    }

    if (REQUEST_WANTS_TYPE (request, REQUEST_THUMBNAIL))
    {
        thumbnail_cancel (directory);
    }

    if (REQUEST_WANTS_TYPE (request, REQUEST_MOUNT))
    {
        mount_cancel (directory);
    }

    nautilus_directory_async_state_changed (directory);
}

void
nautilus_directory_cancel_loading_file_attributes (NautilusDirectory      *directory,
                                                   NautilusFile           *file,
                                                   NautilusFileAttributes  file_attributes)
{
    Request request;

    nautilus_directory_remove_file_from_work_queue (directory, file);

    request = nautilus_directory_set_up_request (file_attributes);

    if (REQUEST_WANTS_TYPE (request, REQUEST_DIRECTORY_COUNT))
    {
        cancel_directory_count_for_file (directory, file);
    }
    if (REQUEST_WANTS_TYPE (request, REQUEST_DEEP_COUNT))
    {
        cancel_deep_counts_for_file (directory, file);
    }
    if (REQUEST_WANTS_TYPE (request, REQUEST_FILE_INFO))
    {
        cancel_file_info_for_file (directory, file);
    }
    if (REQUEST_WANTS_TYPE (request, REQUEST_FILESYSTEM_INFO))
    {
        cancel_filesystem_info_for_file (directory, file);
    }
    if (REQUEST_WANTS_TYPE (request, REQUEST_THUMBNAIL))
    {
        cancel_thumbnail_for_file (directory, file);
    }
    if (REQUEST_WANTS_TYPE (request, REQUEST_MOUNT))
    {
        cancel_mount_for_file (directory, file);
    }

    nautilus_directory_async_state_changed (directory);
}

void
nautilus_directory_add_file_to_work_queue (NautilusDirectory *directory,
                                           NautilusFile      *file)
{
    g_return_if_fail (file->details->directory == directory);

    nautilus_file_queue_enqueue (directory->details->high_priority_queue,
                                 file);
}


static void
add_all_files_to_work_queue (NautilusDirectory *directory)
{
    GList *node;
    NautilusFile *file;

    for (node = directory->details->file_list; node != NULL; node = node->next)
    {
        file = NAUTILUS_FILE (node->data);

        nautilus_directory_add_file_to_work_queue (directory, file);
    }
}

void
nautilus_directory_remove_file_from_work_queue (NautilusDirectory *directory,
                                                NautilusFile      *file)
{
    nautilus_file_queue_remove (directory->details->high_priority_queue,
                                file);
    nautilus_file_queue_remove (directory->details->low_priority_queue,
                                file);
    nautilus_file_queue_remove (directory->details->extension_queue,
                                file);
}


static void
move_file_to_low_priority_queue (NautilusDirectory *directory,
                                 NautilusFile      *file)
{
    /* Must add before removing to avoid ref underflow */
    nautilus_file_queue_enqueue (directory->details->low_priority_queue,
                                 file);
    nautilus_file_queue_remove (directory->details->high_priority_queue,
                                file);
}

static void
move_file_to_extension_queue (NautilusDirectory *directory,
                              NautilusFile      *file)
{
    /* Must add before removing to avoid ref underflow */
    nautilus_file_queue_enqueue (directory->details->extension_queue,
                                 file);
    nautilus_file_queue_remove (directory->details->low_priority_queue,
                                file);
}
