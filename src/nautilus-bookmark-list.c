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

#include "nautilus-bookmark.h"
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
bookmark_in_list_icon_changed (NautilusBookmarkList *bookmarks)
{
    /* emit the changed signal without saving, as only appearance properties changed */
    g_signal_emit (bookmarks, signals[CHANGED], 0);
}

static void
bookmark_in_list_name_changed (NautilusBookmarkList *bookmarks)
{
    nautilus_bookmark_list_save_file (bookmarks);
    g_signal_emit (bookmarks, signals[CHANGED], 0);
}

static void
stop_monitoring_bookmark (NautilusBookmark *bookmark,
                          gpointer          user_data)
{
    g_signal_handlers_disconnect_by_func (bookmark,
                                          bookmark_in_list_changed_callback,
                                          user_data);
    g_signal_handlers_disconnect_by_func (bookmark,
                                          bookmark_in_list_icon_changed,
                                          user_data);
    g_signal_handlers_disconnect_by_func (bookmark,
                                          bookmark_in_list_name_changed,
                                          user_data);
}

static void
clear (NautilusBookmarkList *bookmarks)
{
    g_list_foreach (bookmarks->list, (GFunc) stop_monitoring_bookmark, bookmarks);
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

static GList *
bookmark_list_get_node (NautilusBookmarkList *bookmarks,
                        GFile                *location,
                        guint                *index_ptr)
{
    guint index = 0;

    for (GList *node = bookmarks->list; node != NULL; node = node->next, index += 1)
    {
        NautilusBookmark *bookmark = node->data;
        GFile *bookmark_location = nautilus_bookmark_get_location (bookmark);

        if (g_file_equal (location, bookmark_location))
        {
            if (index_ptr)
            {
                *index_ptr = index;
            }

            return node;
        }
    }

    return NULL;
}

/**
 * insert_bookmark_internal:
 * @bookmarks: pointer to a #NautilusBookmarkList
 * @bookmark: (transfer full): pointer to a #NautilusBookmark to insert
 * @index: Position to store bookmark index in the list
 *
 * Adds a bookmark to the given #NautilusBookmarkList if it doesn't exist.
 *
 * Returns: %TRUE when the bookmark was inserted and %FALSE if the bookmark
 *      already exists.
 */
static gboolean
insert_bookmark_internal (NautilusBookmarkList *bookmarks,
                          NautilusBookmark     *bookmark,
                          int                   index)
{
    GFile *location = nautilus_bookmark_get_location (bookmark);

    if (bookmark_list_get_node (bookmarks, location, NULL) != NULL)
    {
        g_object_unref (bookmark);
        return FALSE;
    }

    bookmarks->list = g_list_insert (bookmarks->list, bookmark, index);

    g_signal_connect_object (bookmark, "contents-changed",
                             G_CALLBACK (bookmark_in_list_changed_callback), bookmarks, 0);
    g_signal_connect_object (bookmark, "notify::icon",
                             G_CALLBACK (bookmark_in_list_icon_changed), bookmarks, G_CONNECT_SWAPPED);
    g_signal_connect_object (bookmark, "notify::name",
                             G_CALLBACK (bookmark_in_list_name_changed), bookmarks, G_CONNECT_SWAPPED);

    return TRUE;
}

/**
 * nautilus_bookmark_list_get_bookmark:
 *
 * Get the bookmark with the specified location, if any
 * @bookmarks: the list of bookmarks.
 * @location: a #GFile
 *
 * Returns: (transfer none): the bookmark with location @location, or %NULL.
 **/
NautilusBookmark *
nautilus_bookmark_list_get_bookmark (NautilusBookmarkList *bookmarks,
                                     GFile                *location)
{
    g_return_val_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks), NULL);
    g_return_val_if_fail (G_IS_FILE (location), NULL);

    GList *node = bookmark_list_get_node (bookmarks, location, NULL);

    return node != NULL ? node->data : NULL;
}

/**
 * nautilus_bookmark_list_add:
 * @bookmarks: NautilusBookmarkList to append to.
 * @location: (transfer none): location to insert a bookmark for
 * @position: position at which to add the bookmark, see g_list_insert
 *
 * Adds a bookmark for the given @location at at a specified @position.
 *
 * Returns: (transfer none): A new bookmark if one was appended or %NULL otherwise
 **/
void
nautilus_bookmark_list_add (NautilusBookmarkList *bookmarks,
                            GFile                *location,
                            int                   position)
{
    g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));
    g_return_if_fail (G_IS_FILE (location));

    NautilusBookmark *bookmark = nautilus_bookmark_new (location, NULL);

    if (insert_bookmark_internal (bookmarks, bookmark, position))
    {
        nautilus_bookmark_list_save_file (bookmarks);
    }
}

/**
 * nautilus_bookmark_list_contains:
 *
 * Check whether a bookmark for the given @location exists
 * @bookmarks: NautilusBookmarkList to check contents of.
 * @location: a #GFile to check for.
 *
 * Return value: TRUE if matching bookmark is in list, FALSE otherwise
 **/
gboolean
nautilus_bookmark_list_contains (NautilusBookmarkList *bookmarks,
                                 GFile                *location)
{
    g_return_val_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks), FALSE);
    g_return_val_if_fail (G_IS_FILE (location), FALSE);

    return bookmark_list_get_node (bookmarks, location, NULL) != NULL;
}

/**
 * nautilus_bookmark_list_move_item:
 *
 * Move the given item to the destination.
 * @location: the location of the bookmark to move.
 * @destination: index to which to move the bookmark.
 **/
void
nautilus_bookmark_list_move_item (NautilusBookmarkList *bookmarks,
                                  GFile                *location,
                                  guint                 destination)
{
    guint index;
    GList *link_to_move = bookmark_list_get_node (bookmarks, location, &index);

    if (index == destination)
    {
        return;
    }

    if (link_to_move == NULL)
    {
        g_autofree char *uri = g_file_get_uri (location);
        g_warning ("Attempted moving unknown bookmark of %s", uri);
        return;
    }

    bookmarks->list = g_list_remove_link (bookmarks->list,
                                          link_to_move);

    GList *link_at_destination = g_list_nth (bookmarks->list, destination);
    /* NULL link at destination means end of the list */
    bookmarks->list = g_list_insert_before_link (bookmarks->list,
                                                 link_at_destination,
                                                 link_to_move);

    nautilus_bookmark_list_save_file (bookmarks);
}

/**
 * nautilus_bookmark_list_remove:
 *
 * Removes any bookmark for @location
 * @bookmarks: the list of bookmarks.
 * @location: The location to remove a bookmark for.
 **/
void
nautilus_bookmark_list_remove (NautilusBookmarkList *bookmarks,
                               GFile                *location)
{
    g_return_if_fail (NAUTILUS_IS_BOOKMARK_LIST (bookmarks));
    g_return_if_fail (location != NULL);

    GList *link_to_remove = bookmark_list_get_node (bookmarks, location, NULL);

    if (link_to_remove == NULL)
    {
        g_autofree char *uri = g_file_get_uri (location);
        g_warning ("Attempted removing unknown bookmark of %s", uri);
        return;
    }

    bookmarks->list = g_list_remove_link (bookmarks->list,
                                          link_to_remove);

    NautilusBookmark *bookmark = link_to_remove->data;
    stop_monitoring_bookmark (bookmark, bookmarks);
    g_list_free_full (link_to_remove, g_object_unref);

    nautilus_bookmark_list_save_file (bookmarks);
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
    g_autofree gchar *contents = g_task_propagate_pointer (G_TASK (res), &error);

    if (error != NULL)
    {
        g_warning ("Unable to get contents of the bookmarks file: %s",
                   error->message);
        op_processed_cb (self);
        return;
    }

    char **lines = g_strsplit (contents, "\n", -1);
    for (guint i = 0; lines[i]; i++)
    {
        char *uri = lines[i];

        /* Ignore empty or invalid lines that cannot be parsed properly */
        if (uri[0] == '\0' || g_unichar_isspace (uri[0]))
        {
            continue;
        }

        /* Split bookmarks file entries into URI and label, which are separated by
         * a space. This behavior is used since GTK 2.7's file chooser.
         */
        char *label = NULL;
        char *space = strchr (uri, ' ');
        if (space != NULL)
        {
            *space = '\0';
            label = space + 1;
        }

        g_autoptr (GFile) location = g_file_new_for_uri (uri);
        NautilusBookmark *new_bookmark = nautilus_bookmark_new (location, label);

        insert_bookmark_internal (self, new_bookmark, -1);
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
    GString *bookmark_string = g_string_new (NULL);

    /* temporarily disable bookmark file monitoring when writing file */
    if (self->monitor != NULL)
    {
        g_file_monitor_cancel (self->monitor);
        g_clear_object (&self->monitor);
    }

    for (GList *l = self->list; l != NULL; l = l->next)
    {
        NautilusBookmark *bookmark = NAUTILUS_BOOKMARK (l->data);

        /* Store bookmark URI followed by label */
        g_autofree char *uri = nautilus_bookmark_get_uri (bookmark);
        const char *label = nautilus_bookmark_get_name (bookmark);

        g_string_append_printf (bookmark_string, "%s %s\n", uri, label);
    }

    task = g_task_new (G_OBJECT (self),
                       NULL,
                       save_callback, NULL);
    gchar *contents = g_string_free_and_steal (bookmark_string);
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
nautilus_bookmark_list_can_bookmark (NautilusBookmarkList *list,
                                     GFile                *location)
{
    if (bookmark_list_get_node (list, location, NULL) != NULL)
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
        g_file_has_uri_scheme (location, SCHEME_NETWORK_VIEW) ||
        nautilus_is_home_directory (location) ||
        g_file_has_uri_scheme (location, SCHEME_TRASH))
    {
        /* Already in the sidebar */
        return FALSE;
    }

    return TRUE;
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
    return g_object_new (NAUTILUS_TYPE_BOOKMARK_LIST, NULL);
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
