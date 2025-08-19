#include "test-utilities.h"

#include <src/nautilus-directory.h>
#include <src/nautilus-file-utilities.h>
#include <src/nautilus-global-preferences.h>
#include <src/nautilus-query.h>
#include <src/nautilus-search-engine.h>
#include <src/nautilus-search-hit.h>
#include <src/nautilus-search-provider.h>

static guint total_hits = 0;

static void
hits_added_cb (NautilusSearchEngine *engine,
               GPtrArray            *transferred_hits)
{
    g_autoptr (GPtrArray) hits = transferred_hits;

    g_print ("Hits added for search engine simple!\n");
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

    g_print ("\nNautilus search engine simple finished!\n");

    delete_search_file_hierarchy ("simple");

    g_main_loop_quit (user_data);
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (GMainLoop) loop = NULL;
    g_autoptr (NautilusQuery) query = NULL;
    g_autoptr (GFile) location = NULL;

    loop = g_main_loop_new (NULL, FALSE);

    nautilus_ensure_extension_points ();
    /* Needed for nautilus-query.c.
     * FIXME: tests are not installed, so the system does not
     * have the gschema. Installed tests is a long term GNOME goal.
     */
    nautilus_global_preferences_init ();

    NautilusSearchEngine *engine = nautilus_search_engine_new (NAUTILUS_SEARCH_TYPE_SIMPLE);
    g_signal_connect (engine, "hits-added",
                      G_CALLBACK (hits_added_cb), NULL);
    g_signal_connect (engine, "finished",
                      G_CALLBACK (finished_cb), loop);

    query = nautilus_query_new ();
    nautilus_query_set_text (query, "engine_simple");

    location = g_file_new_for_path (test_get_tmp_dir ());
    nautilus_query_set_location (query, location);

    create_search_file_hierarchy ("simple");

    nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (engine), query);

    g_main_loop_run (loop);

    g_assert_cmpint (total_hits, ==, 3);

    test_clear_tmp_dir ();

    return 0;
}
