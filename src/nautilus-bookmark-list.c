/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: John Sullivan <sullivan@eazel.com>
 */

/* nautilus-bookmark-list.c - implementation of centralized list of bookmarks.
 */

#include <config.h>
#include "nautilus-bookmark-list.h"

#include "nautilus-file-utilities.h"
#include "nautilus-file.h"
#include "nautilus-icon-names.h"
#include "nautilus-scheme.h"

#include <gio/gio.h>
#include <string.h>
#include <errno.h>

#define MAX_BOOKMARK_LENGTH 80
#define LOAD_JOB 1
#define SAVE_JOB 2

struct _NautilusBookmarkList
{
    GObject parent_instance;

    GList *list;
    GFileMonitor *monitor;
    GQueue *pending_ops;
};

enum
{
    CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* forward declarations */
#define NAUTILUS_BOOKMARK_LIST_ERROR (nautilus_bookmark_list_error_quark ())
static GQuark      nautilus_bookmark_list_error_quark (void);

static void        nautilus_bookmark_list_load_file (NautilusBookmarkList *bookmarks);
static void        nautilus_bookmark_list_save_file (NautilusBookmarkList *bookmarks);

G_DEFINE_TYPE (NautilusBookmarkList, nautilus_bookmark_list, G_TYPE_OBJECT)

static GQuark
nautilus_bookmark_list_error_quark (void)
{
    return g_quark_from_static_string ("nautilus-bookmark-list-error-quark");
}

static NautilusBookmark *
new_bookmark_from_uri (const char *uri,
                       const char *label)
{
    NautilusBookmark *new_bookmark = NULL;
    g_autoptr (GFile) location = NULL;

    if (uri)
    {
        location = g_file_new_for_uri (uri);
        new_bookmark = nautilus_bookmark_new (location, label);
    }

    return new_bookmark;
}

static GFile *
nautilus_bookmark_list_get_legacy_file (void)
{
    g_autofree char *filename = NULL;

    filename = g_build_filename (g_get_home_dir (),
                                 ".gtk-bookmarks",
                                 NULL);

    return g_file_new_for_path (filename);
}

static GFile *
nautilus_bookmark_list_get_file (void)
{
    g_autofree char *filename = NULL;

    filename = g_build_filename (g_get_user_config_dir (),
                                 "gtk-3.0",
                                 "bookmarks",
                                 NULL);

    return g_file_new_for_path (filename);
}

/* Initialization.  */

static void
bookmark_in_list_changed_callback (NautilusBookmark     *bookmark,
                                   NautilusBookmarkList *bookmarks)
{
    g_assert (NAUTILUS_IS_BOOKMARK (bookmark));
    g_assert (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));

    /* save changes to the list */
    nautilus_bookmark_list_save_file (bookmarks);
}

static void
bookmark_in_list_notify (GObject              *object,
                         GParamSpec           *pspec,
                         NautilusBookmarkList *bookmarks)
{
    /* emit the changed signal without saving, as only appearance properties changed */
    g_signal_emit (bookmarks, signals[CHANGED], 0);
}

static void
stop_monitoring_bookmark (NautilusBookmarkList *bookmarks,
                          NautilusBookmark     *bookmark)
{
    g_signal_handlers_disconnect_by_func (bookmark,
                                          bookmark_in_list_changed_callback,
                                          bookmarks);
}

static void
stop_monitoring_one (gpointer data,
                     gpointer user_data)
{
    g_assert (NAUTILUS_IS_BOOKMARK (data));
    g_assert (NAUTILUS_IS_BOOKMARK_LIST (user_data));

    stop_monitoring_bookmark (NAUTILUS_BOOKMARK_LIST (user_data),
                              NAUTILUS_BOOKMARK (data));
}

static void
clear (NautilusBookmarkList *bookmarks)
{
    g_list_foreach (bookmarks->list, stop_monitoring_one, bookmarks);
    g_list_free_full (bookmarks->list, g_object_unref);
    bookmarks->list = NULL;
}

static void
do_finalize (GObject *object)
{
    NautilusBookmarkList *self = NAUTILUS_BOOKMARK_LIST (object);

    if (self->monitor != NULL)
    {
        g_file_monitor_cancel (self->monitor);
        g_clear_object (&self->monitor);
    }

    g_queue_free (self->pending_ops);

    clear (self);

    G_OBJECT_CLASS (nautilus_bookmark_list_parent_class)->finalize (object);
}

static void
nautilus_bookmark_list_class_init (NautilusBookmarkListClass *class)
{
    GObjectClass *object_class = G_OBJECT_CLASS (class);

    object_class->finalize = do_finalize;

    signals[CHANGED] =
        g_signal_new ("changed",
                      G_TYPE_FROM_CLASS (object_class),
                      G_SIGNAL_RUN_LAST,
                      0,
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);
}

static void
bookmark_monitor_changed_cb (GFileMonitor      *monitor,
                             GFile             *child,
                             GFile             *other_file,
                             GFileMonitorEvent  eflags,
                             gpointer           user_data)
{
    if (eflags == G_FILE_MONITOR_EVENT_CHANGED ||
        eflags == G_FILE_MONITOR_EVENT_CREATED)
    {
        g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (NAUTILUS_BOOKMARK_LIST (user_data)));
        nautilus_bookmark_list_load_file (NAUTILUS_BOOKMARK_LIST (user_data));
    }
}

static void
nautilus_bookmark_list_init (NautilusBookmarkList *bookmarks)
{
    g_autoptr (GFile) file = NULL;

    bookmarks->pending_ops = g_queue_new ();

    nautilus_bookmark_list_load_file (bookmarks);

    file = nautilus_bookmark_list_get_file ();
    bookmarks->monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);
    g_file_monitor_set_rate_limit (bookmarks->monitor, 1000);

    g_signal_connect (bookmarks->monitor, "changed",
                      G_CALLBACK (bookmark_monitor_changed_cb), bookmarks);
}

static void
insert_bookmark_internal (NautilusBookmarkList *bookmarks,
                          NautilusBookmark     *bookmark,
                          int                   index)
{
    bookmarks->list = g_list_insert (bookmarks->list, bookmark, index);

    g_signal_connect_object (bookmark, "contents-changed",
                             G_CALLBACK (bookmark_in_list_changed_callback), bookmarks, 0);
    g_signal_connect_object (bookmark, "notify::icon",
                             G_CALLBACK (bookmark_in_list_notify), bookmarks, 0);
    g_signal_connect_object (bookmark, "notify::name",
                             G_CALLBACK (bookmark_in_list_notify), bookmarks, 0);
}

/**
 * nautilus_bookmark_list_item_with_location:
 *
 * Get the bookmark with the specified location, if any
 * @bookmarks: the list of bookmarks.
 * @location: a #GFile
 * @index: location where to store bookmark index, or %NULL
 *
 * Return value: the bookmark with location @location, or %NULL.
 **/
NautilusBookmark *
nautilus_bookmark_list_item_with_location (NautilusBookmarkList *bookmarks,
                                           GFile                *location,
                                           guint                *index)
{
    GList *node;
    NautilusBookmark *bookmark;
    guint idx;

    g_return_val_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks), NULL);
    g_return_val_if_fail (G_IS_FILE (location), NULL);

    idx = 0;

    for (node = bookmarks->list; node != NULL; node = node->next)
    {
        g_autoptr (GFile) bookmark_location = NULL;

        bookmark = node->data;
        bookmark_location = nautilus_bookmark_get_location (bookmark);

        if (g_file_equal (location, bookmark_location))
        {
            if (index)
            {
                *index = idx;
            }

            return bookmark;
        }

        idx++;
    }

    return NULL;
}

/**
 * nautilus_bookmark_list_append:
 *
 * Append a bookmark to a bookmark list.
 * @bookmarks: NautilusBookmarkList to append to.
 * @bookmark: Bookmark to append a copy of.
 **/
void
nautilus_bookmark_list_append (NautilusBookmarkList *bookmarks,
                               NautilusBookmark     *bookmark)
{
    g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));
    g_return_if_fail (NAUTILUS_IS_BOOKMARK (bookmark));

    if (g_list_find_custom (bookmarks->list, bookmark,
                            nautilus_bookmark_compare_with) != NULL)
    {
        return;
    }

    insert_bookmark_internal (bookmarks, g_object_ref (bookmark), -1);
    nautilus_bookmark_list_save_file (bookmarks);
}

/**
 * nautilus_bookmark_list_contains:
 *
 * Check whether a bookmark with matching name and url is already in the list.
 * @bookmarks: NautilusBookmarkList to check contents of.
 * @bookmark: NautilusBookmark to match against.
 *
 * Return value: TRUE if matching bookmark is in list, FALSE otherwise
 **/
gboolean
nautilus_bookmark_list_contains (NautilusBookmarkList *bookmarks,
                                 NautilusBookmark     *bookmark)
{
    g_return_val_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks), FALSE);
    g_return_val_if_fail (NAUTILUS_IS_BOOKMARK (bookmark), FALSE);

    return g_list_find_custom (bookmarks->list,
                               (gpointer) bookmark,
                               nautilus_bookmark_compare_with)
           != NULL;
}

/**
 * nautilus_bookmark_list_delete_item_at:
 *
 * Delete the bookmark at the specified position.
 * @bookmarks: the list of bookmarks.
 * @index: index, must be less than length of list.
 **/
void
nautilus_bookmark_list_delete_item_at (NautilusBookmarkList *bookmarks,
                                       guint                 index)
{
    GList *doomed;

    g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));
    g_return_if_fail (index < g_list_length (bookmarks->list));

    doomed = g_list_nth (bookmarks->list, index);
    bookmarks->list = g_list_remove_link (bookmarks->list, doomed);

    g_assert (NAUTILUS_IS_BOOKMARK (doomed->data));
    stop_monitoring_bookmark (bookmarks, NAUTILUS_BOOKMARK (doomed->data));
    g_object_unref (doomed->data);

    g_list_free_1 (doomed);

    nautilus_bookmark_list_save_file (bookmarks);
}

/**
 * nautilus_bookmark_list_move_item:
 *
 * Move the item from the given position to the destination.
 * @index: the index of the first bookmark.
 * @destination: the index of the second bookmark.
 **/
void
nautilus_bookmark_list_move_item (NautilusBookmarkList *bookmarks,
                                  guint                 index,
                                  guint                 destination)
{
    GList *bookmark_item;

    if (index == destination)
    {
        return;
    }

    bookmark_item = g_list_nth (bookmarks->list, index);
    bookmarks->list = g_list_remove_link (bookmarks->list,
                                          bookmark_item);

    bookmarks->list = g_list_insert (bookmarks->list,
                                     bookmark_item->data,
                                     destination);

    g_list_free_1 (bookmark_item);

    nautilus_bookmark_list_save_file (bookmarks);
}

/**
 * nautilus_bookmark_list_delete_items_with_uri:
 *
 * Delete all bookmarks with the given uri.
 * @bookmarks: the list of bookmarks.
 * @uri: The uri to match.
 **/
void
nautilus_bookmark_list_delete_items_with_uri (NautilusBookmarkList *bookmarks,
                                              const char           *uri)
{
    GList *node, *next;
    gboolean list_changed;
    char *bookmark_uri;

    g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));
    g_return_if_fail (uri != NULL);

    list_changed = FALSE;
    for (node = bookmarks->list; node != NULL; node = next)
    {
        next = node->next;

        bookmark_uri = nautilus_bookmark_get_uri (NAUTILUS_BOOKMARK (node->data));
        if (g_strcmp0 (bookmark_uri, uri) == 0)
        {
            bookmarks->list = g_list_remove_link (bookmarks->list, node);
            stop_monitoring_bookmark (bookmarks, NAUTILUS_BOOKMARK (node->data));
            g_object_unref (node->data);
            g_list_free_1 (node);
            list_changed = TRUE;
        }
        g_free (bookmark_uri);
    }

    if (list_changed)
    {
        nautilus_bookmark_list_save_file (bookmarks);
    }
}

/**
 * nautilus_bookmark_list_insert_item:
 *
 * Insert a bookmark at a specified position.
 * @bookmarks: the list of bookmarks.
 * @index: the position to insert the bookmark at.
 * @new_bookmark: the bookmark to insert a copy of.
 **/
void
nautilus_bookmark_list_insert_item (NautilusBookmarkList *bookmarks,
                                    NautilusBookmark     *new_bookmark,
                                    guint                 index)
{
    g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));
    g_return_if_fail (index <= g_list_length (bookmarks->list));

    insert_bookmark_internal (bookmarks, g_object_ref (new_bookmark), index);
    nautilus_bookmark_list_save_file (bookmarks);
}

/**
 * nautilus_bookmark_list_item_at:
 *
 * Get the bookmark at the specified position.
 * @bookmarks: the list of bookmarks.
 * @index: index, must be less than length of list.
 *
 * Return value: the bookmark at position @index in @bookmarks.
 **/
NautilusBookmark *
nautilus_bookmark_list_item_at (NautilusBookmarkList *bookmarks,
                                guint                 index)
{
    g_return_val_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks), NULL);
    g_return_val_if_fail (index < g_list_length (bookmarks->list), NULL);

    return NAUTILUS_BOOKMARK (g_list_nth_data (bookmarks->list, index));
}

/**
 * nautilus_bookmark_list_length:
 *
 * Get the number of bookmarks in the list.
 * @bookmarks: the list of bookmarks.
 *
 * Return value: the length of the bookmark list.
 **/
guint
nautilus_bookmark_list_length (NautilusBookmarkList *bookmarks)
{
    g_return_val_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks), 0);

    return g_list_length (bookmarks->list);
}

static void
process_next_op (NautilusBookmarkList *bookmarks);

static void
op_processed_cb (NautilusBookmarkList *self)
{
    g_queue_pop_tail (self->pending_ops);

    if (!g_queue_is_empty (self->pending_ops))
    {
        process_next_op (self);
    }
}

static void
load_callback (GObject      *source_object,
               GAsyncResult *res,
               gpointer      user_data)
{
    NautilusBookmarkList *self = NAUTILUS_BOOKMARK_LIST (source_object);
    g_autoptr (GError) error = NULL;
    g_autofree gchar *contents = NULL;
    char **lines;
    int i;

    contents = g_task_propagate_pointer (G_TASK (res), &error);
    if (error != NULL)
    {
        g_warning ("Unable to get contents of the bookmarks file: %s",
                   error->message);
        op_processed_cb (self);
        return;
    }

    lines = g_strsplit (contents, "\n", -1);
    for (i = 0; lines[i]; i++)
    {
        /* Ignore empty or invalid lines that cannot be parsed properly */
        if (lines[i][0] != '\0' && lines[i][0] != ' ')
        {
            /* gtk 2.7/2.8 might have labels appended to bookmarks which are separated by a space
             * we must seperate the bookmark uri and the potential label
             */
            char *space;
            g_autofree char *label = NULL;

            space = strchr (lines[i], ' ');
            if (space)
            {
                *space = '\0';
                label = g_strdup (space + 1);
            }

            insert_bookmark_internal (self, new_bookmark_from_uri (lines[i], label), -1);
        }
    }

    g_signal_emit (self, signals[CHANGED], 0);
    op_processed_cb (self);

    g_strfreev (lines);
}

static void
load_io_thread (GTask        *task,
                gpointer      source_object,
                gpointer      task_data,
                GCancellable *cancellable)
{
    GFile *file;
    gchar *contents;
    GError *error = NULL;

    file = nautilus_bookmark_list_get_file ();
    if (!g_file_query_exists (file, NULL))
    {
        g_object_unref (file);
        file = nautilus_bookmark_list_get_legacy_file ();
    }

    g_file_load_contents (file, NULL, &contents, NULL, NULL, &error);
    g_object_unref (file);

    if (error != NULL)
    {
        g_task_return_error (task, error);
    }
    else
    {
        g_task_return_pointer (task, contents, g_free);
    }
}

static void
load_file_async (NautilusBookmarkList *self)
{
    g_autoptr (GTask) task = NULL;

    /* Wipe out old list. */
    clear (self);

    task = g_task_new (G_OBJECT (self),
                       NULL,
                       load_callback, NULL);
    g_task_run_in_thread (task, load_io_thread);
}

static void
save_callback (GObject      *source_object,
               GAsyncResult *res,
               gpointer      user_data)
{
    NautilusBookmarkList *self = NAUTILUS_BOOKMARK_LIST (source_object);
    g_autoptr (GError) error = NULL;
    gboolean success;
    g_autoptr (GFile) file = NULL;

    success = g_task_propagate_boolean (G_TASK (res), &error);

    if (error != NULL)
    {
        g_warning ("Unable to replace contents of the bookmarks file: %s",
                   error->message);
    }

    /* g_file_replace_contents() returned FALSE, but did not set an error. */
    if (!success)
    {
        g_warning ("Unable to replace contents of the bookmarks file.");
    }

    /* re-enable bookmark file monitoring */
    file = nautilus_bookmark_list_get_file ();
    self->monitor = g_file_monitor_file (file, 0, NULL, NULL);

    g_file_monitor_set_rate_limit (self->monitor, 1000);
    g_signal_connect (self->monitor, "changed",
                      G_CALLBACK (bookmark_monitor_changed_cb), self);

    op_processed_cb (self);
}

static void
save_io_thread (GTask        *task,
                gpointer      source_object,
                gpointer      task_data,
                GCancellable *cancellable)
{
    gchar *contents;
    g_autofree gchar *path = NULL;
    g_autoptr (GFile) parent = NULL;
    g_autoptr (GFile) file = NULL;
    gboolean success;
    GError *error = NULL;

    file = nautilus_bookmark_list_get_file ();
    parent = g_file_get_parent (file);
    path = g_file_get_path (parent);

    if (g_mkdir_with_parents (path, 0700) == -1)
    {
        int saved_errno = errno;

        g_set_error (&error, NAUTILUS_BOOKMARK_LIST_ERROR, 0,
                     "Failed to create bookmarks folder %s: %s",
                     path, g_strerror (saved_errno));
        g_task_return_error (task, error);
        return;
    }

    contents = (gchar *) g_task_get_task_data (task);

    success = g_file_replace_contents (file,
                                       contents, strlen (contents),
                                       NULL, FALSE, 0, NULL,
                                       NULL, &error);

    if (error != NULL)
    {
        g_task_return_error (task, error);
    }
    else
    {
        g_task_return_boolean (task, success);
    }
}

static void
save_file_async (NautilusBookmarkList *self)
{
    g_autoptr (GTask) task = NULL;
    GString *bookmark_string;
    gchar *contents;
    GList *l;

    bookmark_string = g_string_new (NULL);

    /* temporarily disable bookmark file monitoring when writing file */
    if (self->monitor != NULL)
    {
        g_file_monitor_cancel (self->monitor);
        g_clear_object (&self->monitor);
    }

    for (l = self->list; l; l = l->next)
    {
        NautilusBookmark *bookmark;

        bookmark = NAUTILUS_BOOKMARK (l->data);

        /* make sure we save label if it has one for compatibility with GTK 2.7 and 2.8 */
        if (nautilus_bookmark_get_has_custom_name (bookmark))
        {
            const char *label;
            g_autofree char *uri = NULL;

            label = nautilus_bookmark_get_name (bookmark);
            uri = nautilus_bookmark_get_uri (bookmark);

            g_string_append_printf (bookmark_string,
                                    "%s %s\n", uri, label);
        }
        else
        {
            g_autofree char *uri = NULL;

            uri = nautilus_bookmark_get_uri (bookmark);

            g_string_append_printf (bookmark_string, "%s\n", uri);
        }
    }

    task = g_task_new (G_OBJECT (self),
                       NULL,
                       save_callback, NULL);
    contents = g_string_free (bookmark_string, FALSE);
    g_task_set_task_data (task, contents, g_free);

    g_task_run_in_thread (task, save_io_thread);
}

static void
process_next_op (NautilusBookmarkList *bookmarks)
{
    gint op;

    op = GPOINTER_TO_INT (g_queue_peek_tail (bookmarks->pending_ops));

    if (op == LOAD_JOB)
    {
        load_file_async (bookmarks);
    }
    else
    {
        save_file_async (bookmarks);
    }
}

/**
 * nautilus_bookmark_list_load_file:
 *
 * Reads bookmarks from file, clobbering contents in memory.
 * @bookmarks: the list of bookmarks to fill with file contents.
 **/
static void
nautilus_bookmark_list_load_file (NautilusBookmarkList *bookmarks)
{
    g_queue_push_head (bookmarks->pending_ops, GINT_TO_POINTER (LOAD_JOB));

    if (g_queue_get_length (bookmarks->pending_ops) == 1)
    {
        process_next_op (bookmarks);
    }
}

/**
 * nautilus_bookmark_list_save_file:
 *
 * Save bookmarks to disk.
 * @bookmarks: the list of bookmarks to save.
 **/
static void
nautilus_bookmark_list_save_file (NautilusBookmarkList *bookmarks)
{
    g_signal_emit (bookmarks, signals[CHANGED], 0);

    g_queue_push_head (bookmarks->pending_ops, GINT_TO_POINTER (SAVE_JOB));

    if (g_queue_get_length (bookmarks->pending_ops) == 1)
    {
        process_next_op (bookmarks);
    }
}

gboolean
nautilus_bookmark_list_can_bookmark_location (NautilusBookmarkList *list,
                                              GFile                *location)
{
    g_autoptr (NautilusBookmark) bookmark = NULL;

    if (nautilus_bookmark_list_item_with_location (list, location, NULL))
    {
        /* Already bookmarked */
        return FALSE;
    }

    if (g_file_has_uri_scheme (location, SCHEME_SEARCH))
    {
        return FALSE;
    }

    if (g_file_has_uri_scheme (location, SCHEME_RECENT) ||
        g_file_has_uri_scheme (location, SCHEME_STARRED) ||
        nautilus_is_home_directory (location) ||
        g_file_has_uri_scheme (location, SCHEME_TRASH) ||
        g_file_has_uri_scheme (location, SCHEME_OTHER_LOCATIONS))
    {
        /* Already in the sidebar */
        return FALSE;
    }

    bookmark = nautilus_bookmark_new (location, NULL);
    return !nautilus_bookmark_get_is_builtin (bookmark);
}

/**
 * nautilus_bookmark_list_new:
 *
 * Create a new bookmark_list, with contents read from disk.
 *
 * Return value: A pointer to the new widget.
 **/
NautilusBookmarkList *
nautilus_bookmark_list_new (void)
{
    NautilusBookmarkList *list;

    list = NAUTILUS_BOOKMARK_LIST (g_object_new (NAUTILUS_TYPE_BOOKMARK_LIST, NULL));

    return list;
}

/**
 * nautilus_bookmark_list_get_all:
 *
 * Get a GList of all NautilusBookmark.
 * @bookmarks: NautilusBookmarkList from where to get the bookmarks.
 **/
GList *
nautilus_bookmark_list_get_all (NautilusBookmarkList *bookmarks)
{
    g_return_val_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks), NULL);

    return bookmarks->list;
}
