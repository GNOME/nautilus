#include "nautilus-localsearch-utilities.h"
#include "test-utilities.h"

/* Time in seconds we allow for localsearch Miners to index the file */
#define LOCALSEARCH_MINERS_AWAIT_TIMEOUT 1000

static guint total_hits = 0;

typedef struct
{
    GMainLoop *main_loop;
    gchar *uri;
    gboolean created;
} LocalsearchAwaitFileData;

static LocalsearchAwaitFileData *
localsearch_await_file_data_new (const char *uri,
                                 GMainLoop  *main_loop)
{
    LocalsearchAwaitFileData *data;

    data = g_slice_new0 (LocalsearchAwaitFileData);
    data->uri = g_strdup (uri);
    data->main_loop = g_main_loop_ref (main_loop);

    return data;
}

static void
localsearch_await_file_data_free (LocalsearchAwaitFileData *data)
{
    g_free (data->uri);
    g_main_loop_unref (data->main_loop);
    g_slice_free (LocalsearchAwaitFileData, data);
}

static gboolean timeout_cb (gpointer user_data)
{
    LocalsearchAwaitFileData *data = user_data;
    g_error ("Timeout waiting for %s to be indexed by Tracker.", data->uri);
    return G_SOURCE_REMOVE;
}

static void
localsearch_events_cb (TrackerNotifier *self,
                       gchar           *service,
                       gchar           *graph,
                       GPtrArray       *events,
                       gpointer         user_data)
{
    LocalsearchAwaitFileData *data = user_data;

    for (guint i = 0; i < events->len; i++)
    {
        TrackerNotifierEvent *event = g_ptr_array_index (events, i);

        if (tracker_notifier_event_get_event_type (event) == TRACKER_NOTIFIER_EVENT_CREATE)
        {
            const gchar *urn = tracker_notifier_event_get_urn (event);
            g_debug ("Got CREATED event for %s", urn);
            if (strcmp (urn, data->uri) == 0)
            {
                data->created = TRUE;
                g_main_loop_quit (data->main_loop);
            }
        }
    }
}

/* Create data that the localsearch indexer will find, and wait for the database to be updated. */
static void
create_test_data (TrackerSparqlConnection *connection,
                  const gchar             *indexed_tmpdir)
{
    g_autoptr (GFile) test_file = NULL;
    g_autoptr (GMainLoop) main_loop = NULL;
    g_autoptr (GError) error = NULL;
    g_autoptr (TrackerNotifier) notifier = NULL;
    LocalsearchAwaitFileData *await_data;
    gulong signal_id, timeout_id;

    test_file = g_file_new_build_filename (indexed_tmpdir, "target_file.txt", NULL);

    main_loop = g_main_loop_new (NULL, 0);
    await_data = localsearch_await_file_data_new (g_file_get_uri (test_file), main_loop);

    notifier = tracker_sparql_connection_create_notifier (connection);

    signal_id = g_signal_connect (notifier, "events", G_CALLBACK (localsearch_events_cb), await_data);
    timeout_id = g_timeout_add_seconds (LOCALSEARCH_MINERS_AWAIT_TIMEOUT, timeout_cb, await_data);

    g_file_set_contents (g_file_peek_path (test_file), "Please show me in the search results", -1, &error);
    g_assert_no_error (error);

    g_main_loop_run (main_loop);

    g_assert_true (await_data->created);
    g_source_remove (timeout_id);
    g_clear_signal_handler (&signal_id, notifier);

    localsearch_await_file_data_free (await_data);
}

static void
hits_added_cb (NautilusSearchEngine *engine,
               GList                *transferred_hits)
{
    g_autolist (NautilusSearchHit) hits = transferred_hits;

    g_print ("Hits added for search engine localsearch!\n");
    for (gint hit_number = 0; hits != NULL; hits = hits->next, hit_number++)
    {
        g_print ("Hit %i: %s\n", hit_number, nautilus_search_hit_get_uri (hits->data));
        total_hits += 1;
    }
}

static void
finished_cb (GMainLoop  *loop,
             const char *error_message)
{
    g_assert_null (error_message);

    g_print ("\nNautilus search engine localsearch finished!\n");
    g_main_loop_quit (loop);
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (GMainLoop) loop = NULL;
    g_autoptr (TrackerSparqlConnection) connection = NULL;
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autoptr (NautilusQuery) query = NULL;
    g_autoptr (GFile) location = NULL;
    g_autoptr (GError) error = NULL;
    const gchar *indexed_tmpdir;

    nautilus_localsearch_setup_host_miner_fs_connection_sync ();

    indexed_tmpdir = g_getenv ("TRACKER_INDEXED_TMPDIR");
    if (!indexed_tmpdir)
    {
        g_error ("This test must be inside the `tracker-sandbox` script "
                 "to ensure a private Localsearch indexer daemon is used.");
    }

    connection = tracker_sparql_connection_bus_new ("org.freedesktop.Tracker3.Miner.Files", NULL, NULL, &error);

    g_assert_no_error (error);

    loop = g_main_loop_new (NULL, FALSE);

    nautilus_ensure_extension_points ();
    /* Needed for nautilus-query.c.
     * FIXME: tests are not installed, so the system does not
     * have the gschema. Installed tests is a long term GNOME goal.
     */
    nautilus_global_preferences_init ();

    create_test_data (connection, indexed_tmpdir);

    NautilusSearchEngine *engine = nautilus_search_engine_new (NAUTILUS_SEARCH_TYPE_LOCALSEARCH);
    g_signal_connect (engine, "hits-added",
                      G_CALLBACK (hits_added_cb), NULL);
    g_signal_connect_swapped (engine, "search-finished",
                              G_CALLBACK (finished_cb), loop);

    query = nautilus_query_new ();
    nautilus_query_set_text (query, "target");

    location = g_file_new_for_path (indexed_tmpdir);
    directory = nautilus_directory_get (location);
    nautilus_query_set_location (query, location);

    nautilus_search_engine_start (engine, query);

    g_main_loop_run (loop);

    g_assert_cmpint (total_hits, ==, 1);

    return 0;
}
