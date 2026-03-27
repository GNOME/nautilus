/*
 * Copyright © 2026 Khalid Abu Shawarib
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Tests for nautilus_file_operations_rename ()
 */

#include <glib.h>

#include <nautilus-file.h>
#include <nautilus-file-operations.h>
#include <nautilus-file-undo-manager.h>
#include <nautilus-file-utilities.h>
#include <nautilus-tag-manager.h>

#include <test-utilities.h>

typedef struct
{
    gboolean called;
    gboolean success;
    GFile *renamed_file;
} RenameCallbackData;

static void
rename_cb_data_clear (RenameCallbackData *data)
{
    g_clear_object (&data->renamed_file);
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (RenameCallbackData, rename_cb_data_clear)

static void
rename_done_callback (GFile    *renamed_file,
                      gboolean  success,
                      gpointer  callback_data)
{
    RenameCallbackData *data = callback_data;

    data->called = TRUE;
    data->success = success;
    data->renamed_file = renamed_file != NULL ? g_object_ref (renamed_file) : NULL;
}

static void
test_rename_basic (void)
{
    g_autoptr (GFile) file = g_file_new_build_filename (test_get_tmp_dir (),
                                                        "original.txt",
                                                        NULL);
    g_autoptr (GFile) renamed_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                                "renamed.txt",
                                                                NULL);
    g_autoptr (GError) error = NULL;
    g_auto (RenameCallbackData) callback_data = { 0 };
    g_autoptr (GFileOutputStream) stream = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);

    g_assert_no_error (error);
    g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, &error);
    g_assert_no_error (error);

    nautilus_file_operations_rename (file, "renamed.txt", NULL, NULL,
                                     rename_done_callback, &callback_data);

    ITER_CONTEXT_WHILE (!callback_data.called);

    g_assert_true (callback_data.success);
    g_assert_nonnull (callback_data.renamed_file);
    g_assert_true (g_file_query_exists (renamed_file, NULL));
    g_assert_true (g_file_equal (callback_data.renamed_file, renamed_file));
    g_assert_false (g_file_query_exists (file, NULL));

    test_operation_undo ();

    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_false (g_file_query_exists (renamed_file, NULL));

    test_operation_redo ();

    g_assert_false (g_file_query_exists (file, NULL));
    g_assert_true (g_file_query_exists (renamed_file, NULL));

    test_clear_tmp_dir ();
}

static void
test_rename_to_self (void)
{
    g_autoptr (GFile) file = g_file_new_build_filename (test_get_tmp_dir (),
                                                        "original.txt",
                                                        NULL);
    g_autoptr (GError) error = NULL;
    g_auto (RenameCallbackData) callback_data = { 0 };
    g_autoptr (GFileOutputStream) stream = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);

    g_assert_no_error (error);
    g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, &error);
    g_assert_no_error (error);

    nautilus_file_operations_rename (file, "original.txt", NULL, NULL,
                                     rename_done_callback, &callback_data);

    ITER_CONTEXT_WHILE (!callback_data.called);

    g_assert_true (callback_data.success);
    g_assert_nonnull (callback_data.renamed_file);
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_equal (callback_data.renamed_file, file));

    test_clear_tmp_dir ();
}

static void
test_rename_with_separator (void)
{
    g_autoptr (GFile) file = g_file_new_build_filename (test_get_tmp_dir (),
                                                        "original.txt",
                                                        NULL);
    g_autoptr (GError) error = NULL;
    g_auto (RenameCallbackData) callback_data = { 0 };
    const char *invalid_name = "invalid" G_DIR_SEPARATOR_S "name.txt";
    g_autoptr (GFileOutputStream) stream = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);

    g_assert_no_error (error);
    g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, &error);
    g_assert_no_error (error);

    nautilus_file_operations_rename (file, invalid_name, NULL, NULL,
                                     rename_done_callback, &callback_data);

    ITER_CONTEXT_WHILE (!callback_data.called);

    g_assert_false (callback_data.success);
    g_assert_true (g_file_query_exists (file, NULL));

    test_clear_tmp_dir ();
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (NautilusTagManager) tag_manager = NULL;
    g_autoptr (NautilusFileUndoManager) undo_manager = NULL;

    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();

    undo_manager = nautilus_file_undo_manager_new ();
    tag_manager = nautilus_tag_manager_new_dummy ();

    g_test_add_func ("/single/basic",
                     test_rename_basic);
    g_test_add_func ("/single/rename-to-self",
                     test_rename_to_self);
    g_test_add_func ("/single/rename-with-separator",
                     test_rename_with_separator);

    return g_test_run ();
}
