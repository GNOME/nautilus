/*
 * SPDX-FileCopyrightText: 2026 Khalid Abu Shawarib <kas@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#define G_LOG_DOMAIN "test-window-slot"

#include <test-utilities.h>

#include <nautilus-application.h>
#include <nautilus-enums.h>
#include <nautilus-file.h>
#include <nautilus-files-view.h>
#include <nautilus-file-utilities.h>
#include <nautilus-global-preferences.h>
#include <nautilus-resources.h>
#include <nautilus-tag-manager.h>
#include <nautilus-view-model.h>
#include <nautilus-window-slot.h>

static void
test_open_regular_file_location (void)
{
    g_autoptr (NautilusWindowSlot) slot = nautilus_window_slot_new (NAUTILUS_MODE_BROWSE);
    g_autofree gchar *path = g_build_filename (test_get_tmp_dir (),
                                               "regular-file.txt",
                                               NULL);
    g_autoptr (GFile) file_location = g_file_new_for_path (path);
    g_autoptr (GFile) dir_location = g_file_new_for_path (test_get_tmp_dir ());
    g_autoptr (GError) error = NULL;

    g_assert_true (g_file_set_contents (path, "test", -1, &error));
    g_assert_no_error (error);

    /* Deliberately keep no NautilusFile reference for file_location */
    nautilus_window_slot_open_location_full (slot, file_location, NULL);

    /* Wait for the view to load. */
    ITER_CONTEXT_WHILE (nautilus_window_slot_get_current_view (slot) == NULL);

    NautilusFilesView *view = nautilus_window_slot_get_current_view (slot);

    ITER_CONTEXT_WHILE (nautilus_files_view_get_loading (view));

    GFile *view_location = nautilus_window_slot_get_location (slot);
    NautilusViewModel *model = nautilus_files_view_get_private_model (view);
    g_autoptr (NautilusFile) selected_file = nautilus_file_get_existing (file_location);
    NautilusViewItem *selected_item = nautilus_view_model_get_item_for_file (model, selected_file);
    g_autoptr (GtkBitset) selection = gtk_selection_model_get_selection (GTK_SELECTION_MODEL (model));

    /* Opening a file location is supposed to reveal the file in its parent
     * directory instead of opening the file itself. */
    g_assert_true (g_file_equal (view_location, dir_location));
    g_assert_nonnull (selected_item);
    g_assert_cmpint (g_list_model_get_n_items (G_LIST_MODEL (model)), ==, 1);
    g_assert_cmpint (gtk_bitset_get_size (selection), ==, 1);

    test_clear_tmp_dir ();
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (NautilusTagManager) tag_manager = NULL;
    g_autoptr (NautilusApplication) app = NULL;

    nautilus_test_gtk_init (&argc, &argv,
                            NAUTILUS_TEST_INIT_ISOLATE |
                            NAUTILUS_TEST_INIT_REGISTER_RESOURCE |
                            NAUTILUS_TEST_INIT_EXTENSIONS |
                            NAUTILUS_TEST_INIT_PREFS);

    if (nautilus_application_is_sandboxed ())
    {
        g_message ("window slot tests don't work inside a sandbox.");

        return 77;
    }

    tag_manager = nautilus_tag_manager_new_dummy ();

    app = nautilus_application_new ();

    g_test_add_func ("/window-slot/open-file-location",
                     test_open_regular_file_location);

    return g_test_run ();
}
