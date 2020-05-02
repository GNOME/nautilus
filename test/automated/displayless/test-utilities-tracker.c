#include "test-utilities-tracker.h"

#include "tracker-miner-proxy.h"

#define AWAIT_FILE_PROCESSED_TIMEOUT 10

struct _TrackerFilesProcessedWatcher {
    TrackerMiner *proxy;
    GList *file_list;
};

static void
files_processed_cb (TrackerMiner *proxy,
                    GVariant    *files,
                    gpointer     user_data)
{
    TrackerFilesProcessedWatcher *data = user_data;
    GVariantIter iter;
    GVariant *file_info;

    /* Save all the status info in our structure. Don't do this in real apps,
     * but it's useful for tests to assert about what the miner did.
     */
    g_variant_iter_init (&iter, files);
    while ((file_info = g_variant_iter_next_value (&iter)))
    {
        data->file_list = g_list_prepend (data->file_list, file_info);
    }
}

TrackerFilesProcessedWatcher *
tracker_files_processed_watcher_new (const gchar *bus_name,
                                     const gchar *object_path)
{
    TrackerFilesProcessedWatcher *data;
    g_autoptr (GError) error = NULL;

    data = g_slice_new0 (TrackerFilesProcessedWatcher);
    data->proxy = tracker_miner_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                                        G_DBUS_PROXY_FLAGS_NONE,
                                                        bus_name,
                                                        object_path,
                                                        NULL,
                                                        &error);

    g_assert_no_error (error);

    g_signal_connect (data->proxy, "files-processed", G_CALLBACK (files_processed_cb), data);

    return data;
}

void
tracker_files_processed_watcher_free (TrackerFilesProcessedWatcher *data)
{
    g_clear_object (&data->proxy);
    g_list_free_full (data->file_list, (GDestroyNotify) g_variant_unref);

    g_slice_free (TrackerFilesProcessedWatcher, data);
}

static gboolean await_file_timeout_cb (gpointer user_data)
{
    GFile *file = user_data;
    g_autofree gchar *uri;

    uri = g_file_get_uri (file);

    g_error ("Timeout waiting for %s to be processed.", uri);

    return G_SOURCE_REMOVE;
}

static gint
check_file_status_cb (GVariant *status,
                      GFile    *target_file)
{
    gchar *uri, *message;
    gboolean success;
    g_autoptr (GFile) file = NULL;

    g_variant_get (status, "(&sb&s)", &uri, &success, &message);

    file = g_file_new_for_uri (uri);

    if (g_file_equal (file, target_file))
    {
        if (success)
        {
            g_info ("Matched successful processing of %s", uri);
            return 0;
        }
        else
        {
            g_error ("Error processing %s: %s", uri, message);
        }
    }

    return 1;
}


static void
files_processed_quit_main_loop_cb (TrackerMiner *proxy,
                                   GVariant    *files,
                                   gpointer     user_data)
{
    GMainLoop *loop = user_data;

    g_main_loop_quit (loop);
}

void
tracker_files_processed_watcher_await_file (TrackerFilesProcessedWatcher *watcher,
                                            GFile                        *file)
{
    g_autoptr (GMainLoop) loop = NULL;
    g_autoptr (GDBusProxy) proxy = NULL;
    guint timeout_id;
    gulong signal_id;

    if (g_list_find_custom (watcher->file_list, file, (GCompareFunc) check_file_status_cb))
        return;

    loop = g_main_loop_new (NULL, 0);

    signal_id = g_signal_connect_after (watcher->proxy, "files-processed", G_CALLBACK (files_processed_quit_main_loop_cb), loop);
    timeout_id = g_timeout_add_seconds (AWAIT_FILE_PROCESSED_TIMEOUT, await_file_timeout_cb, file);

    while (TRUE) {
        g_main_loop_run (loop);

        if (g_list_find_custom (watcher->file_list, file, (GCompareFunc) check_file_status_cb))
            break;
    }

    g_source_remove (timeout_id);
    g_signal_handler_disconnect (watcher->proxy, signal_id);
}
