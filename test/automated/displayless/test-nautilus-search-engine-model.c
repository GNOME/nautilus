#include <src/nautilus-file-utilities.h>
#include <src/nautilus-global-preferences.h>
#include <src/nautilus-search-provider.h>
#include <src/nautilus-search-engine.c>
#include <gtk/gtk.h>
#include <locale.h>
#include "test-utilities.h"

static void
hits_added_cb (NautilusSearchEngine *engine,
               GSList               *hits)
{
    g_print ("Hits added for search engine model!\n");
    for (gint hit_number = 0; hits != NULL; hits = hits->next, hit_number++)
    {
        g_print ("Hit %i: %s\n", hit_number, nautilus_search_hit_get_uri (hits->data));
    }
}

static void
finished_cb (NautilusSearchEngine         *engine,
             NautilusSearchProviderStatus  status,
             gpointer                      user_data)
{
    nautilus_search_provider_stop (NAUTILUS_SEARCH_PROVIDER (engine));

    g_print ("\nNautilus search engine model finished!\n");

    delete_search_file_hierarchy ("model");

    g_main_loop_quit (user_data);
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (GMainLoop) loop = NULL;
    NautilusSearchEngine *engine;
    NautilusSearchEngineModel *model;
    NautilusSearchEnginePrivate *priv;
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autoptr (NautilusQuery) query = NULL;
    g_autoptr (GFile) location = NULL;

    loop = g_main_loop_new (NULL, FALSE);

    nautilus_ensure_extension_points ();
    /* Needed for nautilus-query.c. 
     * FIXME: this assumes the tests are instlled, as the system would
     * not have the gschema.
     */
    nautilus_global_preferences_init ();

    engine = nautilus_search_engine_new ();
    priv = nautilus_search_engine_get_instance_private (engine);
    g_signal_connect (engine, "hits-added",
                      G_CALLBACK (hits_added_cb), NULL);
    g_signal_connect (engine, "finished",
                      G_CALLBACK (finished_cb), loop);

    query = nautilus_query_new ();
    nautilus_query_set_text (query, "engine_model");
    nautilus_search_provider_set_query (NAUTILUS_SEARCH_PROVIDER (engine), query);

    location = g_file_new_for_path (g_get_tmp_dir ());
    directory = nautilus_directory_get (location);
    nautilus_query_set_location (query, location);

    model = nautilus_search_engine_get_model_provider (engine);
    nautilus_search_engine_model_set_model (model, directory);

    create_search_file_hierarchy ("model");

    priv->providers_running = 0;
    priv->providers_finished = 0;
    priv->providers_error = 0;

    priv->restart = FALSE;
    priv->running++;

    nautilus_search_provider_start (NAUTILUS_SEARCH_PROVIDER (priv->model));

    g_main_loop_run (loop);
    return 0;
}
