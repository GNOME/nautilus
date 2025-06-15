#include "test-utilities.h"

#include "nautilus-directory.h"
#include "nautilus-query.h"
#include "nautilus-search-engine.h"
#include "nautilus-search-hit.h"

static guint total_hits = 0;

static void
hits_added_cb (GPtrArray *transferred_hits,
               gpointer   user_data)
{
    g_autoptr (GPtrArray) hits = transferred_hits;

    g_print ("Hits added for search engine model!\n");
    for (guint i = 0; i < hits->len; i++)
    {
        g_print ("Hit %i: %s\n", i, nautilus_search_hit_get_uri (hits->pdata[i]));

        total_hits += 1;
    }
}

static void
finished_cb (gpointer user_data)
{
    g_print ("\nNautilus search engine model finished!\n");

    delete_search_file_hierarchy ("model");

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

    g_autoptr (NautilusSearchEngine) engine =
        nautilus_search_engine_new (NAUTILUS_SEARCH_TYPE_LOCALSEARCH,
                                    hits_added_cb,
                                    finished_cb,
                                    loop);

    query = nautilus_query_new ();
    nautilus_query_set_text (query, "engine_model");

    location = g_file_new_for_path (test_get_tmp_dir ());

    nautilus_query_set_location (query, location);

    create_search_file_hierarchy ("model");

    nautilus_search_engine_start (engine, query);
    g_main_loop_run (loop);

    g_assert_cmpint (total_hits, ==, 3);

    test_clear_tmp_dir ();

    return 0;
}
