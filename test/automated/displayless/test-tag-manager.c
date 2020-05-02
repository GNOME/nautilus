#include "test-utilities.h"
#include "test-utilities-tracker.h"

#include <src/nautilus-file.h>
#include <src/nautilus-tag-manager.h>

typedef struct {
    NautilusTagManager *tag_manager;
    GFile *file_one;
} TagManagerFixture;

static void
tag_manager_fixture_set_up (TagManagerFixture *fixture,
                            gconstpointer      user_data)
{
    TrackerFilesProcessedWatcher *watcher;
    g_autofree gchar *path = NULL;

    fixture->tag_manager = nautilus_tag_manager_get ();
    nautilus_tag_manager_set_cancellable (fixture->tag_manager, NULL);

    watcher = tracker_files_processed_watcher_new ();

    create_one_file ("stars");

    path = g_build_filename (g_get_tmp_dir (), "stars_first_dir", "stars_first_dir_child", NULL);
    fixture->file_one = g_file_new_for_path (path);

    tracker_files_processed_watcher_await_file (watcher, fixture->file_one);
    tracker_files_processed_watcher_free (watcher);
}

static void
tag_manager_fixture_tear_down (TagManagerFixture *fixture,
                               gconstpointer      user_data)
{
    g_clear_object (&fixture->tag_manager);
    g_clear_object (&fixture->file_one);
};

static void
starred_changed_cb (NautilusTagManager *tag_manager,
                    GList              *selection,
                    gpointer            user_data)
{
    GMainLoop *loop = user_data;

    g_main_loop_quit (loop);
}

static void
test_star_unstar (TagManagerFixture *fixture,
             gconstpointer      user_data)
{
    GList *selection;
    gulong signal_id;
    g_autoptr (GMainLoop) loop = NULL;

    loop = g_main_loop_new (NULL, 0);

    signal_id = g_signal_connect (fixture->tag_manager, "starred-changed", G_CALLBACK (starred_changed_cb), loop);

    selection = g_list_prepend (NULL, nautilus_file_get (fixture->file_one));

    /* Nothing is starred to begin with */
    g_assert (nautilus_tag_manager_get_starred_files (fixture->tag_manager) == NULL);

    /* Star one file and check. */
    nautilus_tag_manager_star_files (fixture->tag_manager, NULL, selection, NULL, NULL);
    g_main_loop_run (loop);

    g_assert_cmpint (g_list_length (nautilus_tag_manager_get_starred_files (fixture->tag_manager)), ==, 1);

    /* Unstar the file and check again. */
    nautilus_tag_manager_unstar_files (fixture->tag_manager, NULL, selection, NULL, NULL);
    g_main_loop_run (loop);

    g_assert_cmpint (g_list_length (nautilus_tag_manager_get_starred_files (fixture->tag_manager)), ==, 0);

    g_signal_handler_disconnect (fixture->tag_manager, signal_id);
}

static void
setup_test_suite (void)
{
    g_test_add ("/test-tag-manager/star_unstar", TagManagerFixture, NULL,
                tag_manager_fixture_set_up, test_star_unstar,
                tag_manager_fixture_tear_down);
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (TrackerSparqlConnection) connection = NULL;
    g_autoptr (GError) error = NULL;
    g_autoptr (NautilusFileUndoManager) undo_manager = NULL;

    g_test_init (&argc, &argv, NULL);

    nautilus_ensure_extension_points ();

    undo_manager = nautilus_file_undo_manager_new ();

    /* Make sure to run this test using the 'tracker-sandbox' script
     * so it doesn't make changes to your real Tracker index.
     */
    connection = tracker_sparql_connection_bus_new ("org.freedesktop.Tracker3.Miner.Files", NULL, NULL, &error);

    g_assert_no_error (error);

    if (!g_getenv ("TRACKER_INDEXED_TMPDIR")) {
        g_error ("This test expects to be run inside `tracker sandbox --index-recursive-tmpdir`.");
    }

    g_setenv ("TMPDIR", g_getenv ("TRACKER_INDEXED_TMPDIR"), TRUE);

    setup_test_suite ();

    return g_test_run ();
}
