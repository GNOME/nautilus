#include "test-utilities.h"

static guint total_hits = 0;

static void
hits_added_cb (NautilusSearchEngine *engine,
               GPtrArray            *transferred_hits)
{
    g_autoptr (GPtrArray) hits = transferred_hits;

    g_print ("Hits added for search engine!\n");
    for (guint i = 0; i < hits->len; i++)
    {
        g_print ("Hit %i: %s\n", i, nautilus_search_hit_get_uri (hits->pdata[i]));
        total_hits += 1;
    }
}

static void
finished_cb (NautilusSearchEngine         *engine,
             NautilusSearchProviderStatus  status,
             gpointer                      user_data)
{
    nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (engine));

    g_print ("\nNautilus search engine finished!\n");

    delete_search_file_hierarchy ("all_engines");

    g_main_loop_quit (user_data);
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (GMainLoop) loop = NULL;
    NautilusSearchEngine *engine;
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autoptr (NautilusQuery) query = NULL;
    g_autoptr (GFile) location = NULL;
    g_autoptr (GFile) file = NULL;

    loop = g_main_loop_new (NULL, FALSE);

    nautilus_ensure_extension_points ();
    /* Needed for nautilus-query.c.
     * FIXME: tests are not installed, so the system does not
     * have the gschema. Installed tests is a long term GNOME goal.
     */
    nautilus_global_preferences_init ();

    engine = nautilus_search_engine_new (NAUTILUS_SEARCH_TYPE_ALL);
    g_signal_connect (engine, "hits-added",
                      G_CALLBACK (hits_added_cb), NULL);
    g_signal_connect (engine, "finished",
                      G_CALLBACK (finished_cb), loop);

    query = nautilus_query_new ();
    nautilus_query_set_text (query, "engine_all_engines");

    location = g_file_new_for_path (test_get_tmp_dir ());
    directory = nautilus_directory_get (location);
    nautilus_query_set_location (query, location);

    create_search_file_hierarchy ("all_engines");

    nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (engine), query);

    g_main_loop_run (loop);

    g_assert_cmpint (total_hits, ==, 3);

    test_clear_tmp_dir ();

    return 0;
}
