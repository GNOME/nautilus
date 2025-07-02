/*
 * Copyright © 2024 The Files contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#include "test-utilities.h"

static void
compression_callback (GFile    *new_file,
                      gboolean  success,
                      gpointer  callback_data)
{
    gboolean *success_result = callback_data;

    *success_result = success;
}

static void
file_hierarchy_create_compress (const GStrv   hier,
                                const gchar  *substitution,
                                GFile        *output_file,
                                AutoarFormat  format,
                                AutoarFilter  filter,
                                const gchar  *passphrase)
{
    g_autolist (GFile) compressed_files = file_hierarchy_get_files_list (hier, "", TRUE);
    gboolean success = FALSE;
    g_autoptr (GError) error = NULL;

    file_hierarchy_create (hier, "");

    nautilus_file_operations_compress (compressed_files,
                                       output_file,
                                       format,
                                       filter,
                                       passphrase,
                                       NULL,
                                       NULL,
                                       compression_callback,
                                       &success);

    ITER_CONTEXT_WHILE (!success);

    g_assert_true (success);
    g_assert_true (g_file_query_exists (output_file, NULL));

    g_autoptr (GFileInfo) info = g_file_query_info (output_file,
                                                    G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                                    G_FILE_QUERY_INFO_NONE,
                                                    NULL,
                                                    &error);

    g_assert_no_error (error);
    g_assert_cmpstr (g_file_info_get_content_type (info), ==, "application/zip");
}

static void
test_compress_big_file (void)
{
    /* Create a and compress a 256 MiB file */
    gsize file_size = 256 * 1024 * 1024;
    g_autoptr (GFile) big_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                            "my_big_file",
                                                            NULL);
    g_autoptr (GFile) archive_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                                "archive.zip",
                                                                NULL);
    gboolean success = FALSE;
    g_autoptr (GError) error = NULL;

    create_random_file (big_file, file_size);
    nautilus_file_operations_compress (&(GList){ .data = big_file},
                                       archive_file,
                                       AUTOAR_FORMAT_ZIP,
                                       AUTOAR_FILTER_NONE,
                                       NULL,
                                       NULL,
                                       NULL,
                                       compression_callback,
                                       &success);

    ITER_CONTEXT_WHILE (!success);

    g_assert_true (success);
    g_assert_true (g_file_query_exists (archive_file, NULL));

    g_autoptr (GFileInfo) info = g_file_query_info (archive_file,
                                                    G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                                    G_FILE_QUERY_INFO_NONE,
                                                    NULL,
                                                    &error);

    g_assert_no_error (error);
    g_assert_cmpstr (g_file_info_get_content_type (info), ==, "application/zip");

    g_assert_true (g_file_query_exists (archive_file, NULL));
    g_assert_true (g_file_query_exists (big_file, NULL));

    test_operation_undo ();

    g_assert_false (g_file_query_exists (archive_file, NULL));
    g_assert_true (g_file_query_exists (big_file, NULL));

    test_operation_redo ();

    g_assert_true (g_file_query_exists (archive_file, NULL));
    g_assert_true (g_file_query_exists (big_file, NULL));

    test_clear_tmp_dir ();
}

static void
test_compress_file_and_dir (void)
{
    const GStrv files_hier = (char *[])
    {
        "my_file",
        "my_directory/",
        "my_directory/my_inner_file",
        NULL
    };
    g_autoptr (GFile) archive_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                                "archive.zip",
                                                                NULL);

    file_hierarchy_create_compress (files_hier,
                                    "",
                                    archive_file,
                                    AUTOAR_FORMAT_ZIP,
                                    AUTOAR_FILTER_NONE,
                                    NULL);

    g_assert_true (g_file_query_exists (archive_file, NULL));
    file_hierarchy_assert_exists (files_hier, "", TRUE);

    test_operation_undo ();

    g_assert_false (g_file_query_exists (archive_file, NULL));
    file_hierarchy_assert_exists (files_hier, "", TRUE);

    test_operation_redo ();

    g_assert_true (g_file_query_exists (archive_file, NULL));
    file_hierarchy_assert_exists (files_hier, "", TRUE);

    test_clear_tmp_dir ();
}

static void
extraction_callback (GList    *outputs,
                     gpointer  callback_data)
{
    gboolean *success_result = callback_data;

    g_assert_nonnull (outputs);

    *success_result = TRUE;
}

static void
test_extract_file (void)
{
    const GStrv compressed_files_hier = (char *[])
    {
        "my_file_1",
        "my_file_2",
        NULL
    };
    const GStrv extracted_files_hier = (char *[])
    {
        "archive/",
        "archive/my_file_1",
        "archive/my_file_2",
        NULL
    };
    g_autoptr (GFile) tmp_dir = g_file_new_for_path (test_get_tmp_dir ());
    g_autoptr (GFile) archive_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                                "archive.zip",
                                                                NULL);
    gboolean success = FALSE;

    /*
     * Compression
     */
    file_hierarchy_create_compress (compressed_files_hier,
                                    "",
                                    archive_file,
                                    AUTOAR_FORMAT_ZIP,
                                    AUTOAR_FILTER_NONE,
                                    NULL);

    /*
     * Extraction
     */
    /* Delete original files so that they can be replace with extracted ones. */
    file_hierarchy_delete (compressed_files_hier, "");
    file_hierarchy_assert_exists (compressed_files_hier, "", FALSE);

    nautilus_file_operations_extract_files (&(GList){ .data = archive_file},
                                            tmp_dir,
                                            NULL,
                                            NULL,
                                            extraction_callback,
                                            &success);
    ITER_CONTEXT_WHILE (!success);

    g_assert_true (success);
    file_hierarchy_assert_exists (compressed_files_hier, "", FALSE);
    file_hierarchy_assert_exists (extracted_files_hier, "", TRUE);

    test_operation_undo ();

    file_hierarchy_assert_exists (compressed_files_hier, "", FALSE);
    file_hierarchy_assert_exists (extracted_files_hier, "", FALSE);

    test_operation_redo ();

    file_hierarchy_assert_exists (compressed_files_hier, "", FALSE);
    file_hierarchy_assert_exists (extracted_files_hier, "", TRUE);

    test_clear_tmp_dir ();
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (NautilusFileUndoManager) undo_manager = NULL;

    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();

    undo_manager = nautilus_file_undo_manager_new ();

    g_test_add_func ("/compress/single/big_file",
                     test_compress_big_file);
    g_test_add_func ("/compress/multiple/file_dir",
                     test_compress_file_and_dir);
    g_test_add_func ("/extract/single",
                     test_extract_file);

    return g_test_run ();
}
