/*
 * Copyright © 2025 The Files contributors
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Authors: Khalid Abu Shawarib <kas@gnome.org>
 */

#include "test-utilities.h"
#include <src/nautilus-file-operations.h>
#include <src/nautilus-file-undo-manager.h>
#include <src/nautilus-file-utilities.h>
#include <src/nautilus-progress-info.h>
#include <src/nautilus-progress-info-manager.h>

#include <gnome-autoar/gnome-autoar.h>

static const gchar *
mime_type_for_format (AutoarFormat format,
                      AutoarFilter filter)
{
    if (format == AUTOAR_FORMAT_ZIP &&
        filter == AUTOAR_FILTER_NONE)
    {
        return "application/zip";
    }
    if (format == AUTOAR_FORMAT_TAR &&
        filter == AUTOAR_FILTER_XZ)
    {
        return "application/x-xz-compressed-tar";
    }

    g_return_val_if_reached ("");
}

typedef struct
{
    GList *files;
    gboolean success;
    GMainLoop *loop;
} ArchiveCallbackData;

#define ARCHIVE_CALLBACK_DATA_INIT(file) { \
            &(GList){ .data = file }, \
            FALSE, \
            NULL, \
};

static void
compression_callback (GFile    *new_file,
                      gboolean  success,
                      gpointer  callback_data)
{
    ArchiveCallbackData *data = callback_data;

    g_assert_cmpuint (g_list_length (data->files), ==, 1);

    g_assert (g_file_equal (new_file, data->files->data));

    data->success = success;

    if (data->loop != NULL)
    {
        g_main_loop_quit (data->loop);
    }
}

static gint
file_compare (GFile *a,
              GFile *b)
{
    return !g_file_equal (a, b);
}

static void
extraction_callback (GList    *outputs,
                     gpointer  callback_data)
{
    ArchiveCallbackData *data = callback_data;

    g_assert_cmpuint (g_list_length (data->files), ==, g_list_length (outputs));

    for (GList *expected = data->files; expected != NULL; expected = expected->next)
    {
        g_assert_true (g_list_find_custom (outputs, expected->data, (GCompareFunc) file_compare));
    }

    data->success = outputs != NULL;

    if (data->loop != NULL)
    {
        g_main_loop_quit (data->loop);
    }
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
    ArchiveCallbackData data = ARCHIVE_CALLBACK_DATA_INIT (output_file);
    g_autoptr (GError) error = NULL;
    const gchar *mimetype = mime_type_for_format (format, filter);

    file_hierarchy_create (hier, "");

    nautilus_file_operations_compress (compressed_files,
                                       output_file,
                                       format,
                                       filter,
                                       passphrase,
                                       NULL,
                                       NULL,
                                       compression_callback,
                                       &data);

    ITER_CONTEXT_WHILE (!data.success);

    g_assert_true (data.success);
    g_assert_true (g_file_query_exists (output_file, NULL));

    g_autoptr (GFileInfo) info = g_file_query_info (output_file,
                                                    G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                                    G_FILE_QUERY_INFO_NONE,
                                                    NULL,
                                                    &error);

    g_assert_no_error (error);

    g_assert_cmpstr (g_file_info_get_content_type (info), ==, mimetype);
}

static void
test_archive_file (void)
{
    const GStrv compressed_files_hier = (char *[])
    {
        "my_file",
        NULL
    };
    const GStrv extracted_files_hier = (char *[])
    {
        "archive/",
        "archive/my_file",
        NULL
    };
    g_autoptr (GFile) tmp_dir = g_file_new_for_path (test_get_tmp_dir ());
    g_autoptr (GFile) archive_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                                "archive.zip",
                                                                NULL);
    AutoarFormat format = AUTOAR_FORMAT_ZIP;
    AutoarFilter filter = AUTOAR_FILTER_NONE;

    /*
     * Compression
     */
    file_hierarchy_create_compress (compressed_files_hier,
                                    "",
                                    archive_file,
                                    format,
                                    filter,
                                    NULL);

    file_hierarchy_assert_exists (compressed_files_hier, "", TRUE);
    g_assert_true (g_file_query_exists (archive_file, NULL));

    test_operation_undo ();

    file_hierarchy_assert_exists (compressed_files_hier, "", TRUE);
    g_assert_false (g_file_query_exists (archive_file, NULL));

    test_operation_redo ();

    file_hierarchy_assert_exists (compressed_files_hier, "", TRUE);
    g_assert_true (g_file_query_exists (archive_file, NULL));

    /*
     * Extraction
     */
    g_autolist (GFile) extracted_files = file_hierarchy_get_files_list (extracted_files_hier,
                                                                        "",
                                                                        TRUE);
    ArchiveCallbackData data = { extracted_files, FALSE, NULL };

    /* Delete original files so that they can be replace with extracted ones. */
    file_hierarchy_delete (compressed_files_hier, "");
    file_hierarchy_assert_exists (compressed_files_hier, "", FALSE);

    nautilus_file_operations_extract_files (&(GList){ .data = archive_file },
                                            tmp_dir,
                                            NULL,
                                            NULL,
                                            extraction_callback,
                                            &data);
    ITER_CONTEXT_WHILE (!data.success);

    g_assert_true (data.success);
    file_hierarchy_assert_exists (compressed_files_hier, "", FALSE);
    g_assert_true (g_file_query_exists (archive_file, NULL));
    file_hierarchy_assert_exists (extracted_files_hier, "", TRUE);

    test_operation_undo ();

    file_hierarchy_assert_exists (compressed_files_hier, "", FALSE);
    g_assert_true (g_file_query_exists (archive_file, NULL));
    file_hierarchy_assert_exists (extracted_files_hier, "", FALSE);

    test_operation_redo ();

    file_hierarchy_assert_exists (compressed_files_hier, "", FALSE);
    g_assert_true (g_file_query_exists (archive_file, NULL));
    file_hierarchy_assert_exists (extracted_files_hier, "", TRUE);

    test_clear_tmp_dir ();
}

static void
test_archive_file_long (void)
{
    /* Create a and compress a 16 MiB file using XZ to take longer. */
    gsize file_size = 16 * 1024 * 1024;
    g_autoptr (GFile) tmp_dir = g_file_new_for_path (test_get_tmp_dir ());
    g_autoptr (GFile) big_file = g_file_get_child (tmp_dir, "my_big_file");
    g_autoptr (GFile) archive_file = g_file_get_child (tmp_dir, "my_big_file.tar.xz");
    AutoarFormat format = AUTOAR_FORMAT_TAR;
    AutoarFilter filter = AUTOAR_FILTER_XZ;
    ArchiveCallbackData data = ARCHIVE_CALLBACK_DATA_INIT (archive_file);
    g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
    g_autoptr (GError) error = NULL;

    create_random_file (big_file, file_size);
    data.loop = loop;

    /*
     * Compression
     */
    nautilus_file_operations_compress (&(GList){ .data = big_file },
                                       archive_file,
                                       format,
                                       filter,
                                       NULL,
                                       NULL,
                                       NULL,
                                       compression_callback,
                                       &data);
    g_main_loop_run (loop);

    g_assert_true (data.success);
    g_assert_true (g_file_query_exists (archive_file, NULL));

    g_autoptr (GFileInfo) info = g_file_query_info (archive_file,
                                                    G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                                    G_FILE_QUERY_INFO_NONE,
                                                    NULL,
                                                    &error);

    g_assert_no_error (error);
    g_assert_cmpstr (g_file_info_get_content_type (info), ==, mime_type_for_format (format, filter));

    g_assert_true (g_file_query_exists (archive_file, NULL));
    g_assert_true (g_file_query_exists (big_file, NULL));

    test_operation_undo ();

    g_assert_false (g_file_query_exists (archive_file, NULL));
    g_assert_true (g_file_query_exists (big_file, NULL));

    test_operation_redo ();

    g_assert_true (g_file_query_exists (archive_file, NULL));
    g_assert_true (g_file_query_exists (big_file, NULL));

    /*
     * Extraction
     */
    data.files = &(GList){
        .data = big_file
    };
    data.success = FALSE;
    data.loop = loop;
    g_file_delete (big_file, NULL, &error);
    g_assert_no_error (error);
    g_assert_false (g_file_query_exists (big_file, NULL));

    nautilus_file_operations_extract_files (&(GList){ .data = archive_file },
                                            tmp_dir,
                                            NULL,
                                            NULL,
                                            extraction_callback,
                                            &data);
    g_main_loop_run (loop);

    g_assert_true (data.success);
    g_assert_true (g_file_query_exists (big_file, NULL));
    g_assert_true (g_file_query_exists (archive_file, NULL));

    test_operation_undo ();

    g_assert_false (g_file_query_exists (big_file, NULL));
    g_assert_true (g_file_query_exists (archive_file, NULL));

    test_operation_redo ();

    g_assert_true (g_file_query_exists (big_file, NULL));
    g_assert_true (g_file_query_exists (archive_file, NULL));

    test_clear_tmp_dir ();
}

static void
test_compress_file_password (void)
{
    gsize file_size = 4 * 1024;
    g_autoptr (GFile) tmp_dir = g_file_new_for_path (test_get_tmp_dir ());
    g_autoptr (GFile) compressed_file = g_file_get_child (tmp_dir, "my_file");
    g_autoptr (GFile) archive_file = g_file_get_child (tmp_dir, "my_file.zip");
    const gchar *password = "The Quick Brown Fox Jumps Over The Lazy Dog.";
    AutoarFormat format = AUTOAR_FORMAT_ZIP;
    AutoarFilter filter = AUTOAR_FILTER_NONE;
    ArchiveCallbackData data = ARCHIVE_CALLBACK_DATA_INIT (archive_file);
    g_autoptr (GError) error = NULL;

    create_random_file (compressed_file, file_size);

    /*
     * Compression
     */
    nautilus_file_operations_compress (&(GList){ .data = compressed_file },
                                       archive_file,
                                       format,
                                       filter,
                                       password,
                                       NULL,
                                       NULL,
                                       compression_callback,
                                       &data);
    ITER_CONTEXT_WHILE (!data.success);

    g_assert_true (data.success);
    g_assert_true (g_file_query_exists (archive_file, NULL));

    g_autoptr (GFileInfo) info = g_file_query_info (archive_file,
                                                    G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE,
                                                    G_FILE_QUERY_INFO_NONE,
                                                    NULL,
                                                    &error);

    g_assert_no_error (error);
    g_assert_cmpstr (g_file_info_get_content_type (info), ==, mime_type_for_format (format, filter));

    g_assert_true (g_file_query_exists (archive_file, NULL));
    g_assert_true (g_file_query_exists (compressed_file, NULL));

    test_operation_undo ();

    g_assert_false (g_file_query_exists (archive_file, NULL));
    g_assert_true (g_file_query_exists (compressed_file, NULL));

    test_operation_redo ();

    g_assert_true (g_file_query_exists (archive_file, NULL));
    g_assert_true (g_file_query_exists (compressed_file, NULL));

    /* TODO: Implement extraction */

    test_clear_tmp_dir ();
}

static void
test_archive_full_dir (void)
{
    const GStrv compressed_files_hier = (char *[])
    {
        "my_file",
        "my_directory/",
        "my_directory/my_inner_file",
        NULL
    };
    const GStrv extracted_files_hier = (char *[])
    {
        "archive/",
        "archive/my_file",
        "archive/my_directory/",
        "archive/my_directory/my_inner_file",
        NULL
    };
    g_autoptr (GFile) tmp_dir = g_file_new_for_path (test_get_tmp_dir ());
    g_autoptr (GFile) archive_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                                "archive.zip",
                                                                NULL);
    AutoarFormat format = AUTOAR_FORMAT_ZIP;
    AutoarFilter filter = AUTOAR_FILTER_NONE;

    /*
     * Compression
     */
    file_hierarchy_create_compress (compressed_files_hier,
                                    "",
                                    archive_file,
                                    format,
                                    filter,
                                    NULL);

    g_assert_true (g_file_query_exists (archive_file, NULL));
    file_hierarchy_assert_exists (compressed_files_hier, "", TRUE);

    test_operation_undo ();

    g_assert_false (g_file_query_exists (archive_file, NULL));
    file_hierarchy_assert_exists (compressed_files_hier, "", TRUE);

    test_operation_redo ();

    g_assert_true (g_file_query_exists (archive_file, NULL));
    file_hierarchy_assert_exists (compressed_files_hier, "", TRUE);

    /*
     * Extraction
     */
    g_autolist (GFile) extracted_files = file_hierarchy_get_files_list (extracted_files_hier, "",
                                                                        TRUE);
    ArchiveCallbackData data = { extracted_files, FALSE, NULL };

    /* Delete original files so that they can be replace with extracted ones. */
    file_hierarchy_delete (compressed_files_hier, "");
    file_hierarchy_assert_exists (compressed_files_hier, "", FALSE);

    nautilus_file_operations_extract_files (&(GList){ .data = archive_file },
                                            tmp_dir,
                                            NULL,
                                            NULL,
                                            extraction_callback,
                                            &data);
    ITER_CONTEXT_WHILE (!data.success);

    g_assert_true (data.success);
    file_hierarchy_assert_exists (compressed_files_hier, "", FALSE);
    g_assert_true (g_file_query_exists (archive_file, NULL));
    file_hierarchy_assert_exists (extracted_files_hier, "", TRUE);

    test_operation_undo ();

    file_hierarchy_assert_exists (compressed_files_hier, "", FALSE);
    g_assert_true (g_file_query_exists (archive_file, NULL));
    file_hierarchy_assert_exists (extracted_files_hier, "", FALSE);

    test_operation_redo ();

    file_hierarchy_assert_exists (compressed_files_hier, "", FALSE);
    g_assert_true (g_file_query_exists (archive_file, NULL));
    file_hierarchy_assert_exists (extracted_files_hier, "", TRUE);

    test_clear_tmp_dir ();
}

static void
test_archive_full_dir_early_cancel (void)
{
    const GStrv compressed_files_hier = (char *[])
    {
        "my_file",
        "my_directory/",
        "my_directory/my_inner_file",
        NULL
    };
    const GStrv extracted_files_hier = (char *[])
    {
        "archive/my_file",
        "archive/my_directory/",
        "archive/my_directory/my_inner_file",
        NULL
    };
    g_autoptr (GFile) tmp_dir = g_file_new_for_path (test_get_tmp_dir ());
    g_autoptr (GFile) archive_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                                "archive.zip",
                                                                NULL);
    g_autolist (GFile) compressed_files = file_hierarchy_get_files_list (compressed_files_hier,
                                                                         "",
                                                                         TRUE);
    AutoarFormat format = AUTOAR_FORMAT_ZIP;
    AutoarFilter filter = AUTOAR_FILTER_NONE;
    g_autoptr (NautilusProgressInfoManager) progress_manager = NULL;
    GList *progress_infos;
    NautilusProgressInfo *info;
    ArchiveCallbackData data = { NULL, FALSE, NULL };

    /*
     * Compression
     */

    progress_manager = nautilus_progress_info_manager_dup_singleton ();
    file_hierarchy_create (compressed_files_hier, "");

    nautilus_file_operations_compress (compressed_files,
                                       archive_file,
                                       format,
                                       filter,
                                       NULL,
                                       NULL,
                                       NULL,
                                       compression_callback,
                                       &data);

    /* TODO: Move progress manager management to test-utilities */
    progress_infos = nautilus_progress_info_manager_get_all_infos (progress_manager);
    g_assert_nonnull (progress_infos);

    info = progress_infos->data;
    nautilus_progress_info_cancel (info);

    ITER_CONTEXT_WHILE (data.success);

    g_assert_false (data.success);
    g_assert_false (g_file_query_exists (archive_file, NULL));

    data.files = &(GList){
        .data = archive_file
    };
    data.success = FALSE;
    nautilus_file_operations_compress (compressed_files,
                                       archive_file,
                                       format,
                                       filter,
                                       NULL,
                                       NULL,
                                       NULL,
                                       compression_callback,
                                       &data);

    ITER_CONTEXT_WHILE (!data.success);

    g_assert_true (data.success);
    g_assert_true (g_file_query_exists (archive_file, NULL));

    /*
     * Extraction
     */
    data.files = NULL;
    data.success = TRUE;

    /* Delete original files so that they can be replace with extracted ones. */
    file_hierarchy_delete (compressed_files_hier, "");
    file_hierarchy_assert_exists (compressed_files_hier, "", FALSE);

    nautilus_file_operations_extract_files (&(GList){ .data = archive_file },
                                            tmp_dir,
                                            NULL,
                                            NULL,
                                            extraction_callback,
                                            &data);
    progress_infos = nautilus_progress_info_manager_get_all_infos (progress_manager);
    g_assert_nonnull (progress_infos);

    info = progress_infos->data;
    nautilus_progress_info_cancel (info);

    ITER_CONTEXT_WHILE (data.success);

    g_assert_false (data.success);
    file_hierarchy_assert_exists (compressed_files_hier, "", FALSE);
    g_assert_true (g_file_query_exists (archive_file, NULL));
    file_hierarchy_assert_exists (extracted_files_hier, "", FALSE);

    test_clear_tmp_dir ();
}

static void
test_archive_files (void)
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
    AutoarFormat format = AUTOAR_FORMAT_ZIP;
    AutoarFilter filter = AUTOAR_FILTER_NONE;

    /*
     * Compression
     */
    file_hierarchy_create_compress (compressed_files_hier,
                                    "",
                                    archive_file,
                                    format,
                                    filter,
                                    NULL);

    file_hierarchy_assert_exists (compressed_files_hier, "", TRUE);
    g_assert_true (g_file_query_exists (archive_file, NULL));

    test_operation_undo ();

    file_hierarchy_assert_exists (compressed_files_hier, "", TRUE);
    g_assert_false (g_file_query_exists (archive_file, NULL));

    test_operation_redo ();

    file_hierarchy_assert_exists (compressed_files_hier, "", TRUE);
    g_assert_true (g_file_query_exists (archive_file, NULL));

    /*
     * Extraction
     */
    g_autolist (GFile) extracted_files = file_hierarchy_get_files_list (extracted_files_hier, "",
                                                                        TRUE);
    ArchiveCallbackData data = { extracted_files, FALSE, NULL };

    /* Delete original files so that they can be replace with extracted ones. */
    file_hierarchy_delete (compressed_files_hier, "");
    file_hierarchy_assert_exists (compressed_files_hier, "", FALSE);

    nautilus_file_operations_extract_files (&(GList){ .data = archive_file },
                                            tmp_dir,
                                            NULL,
                                            NULL,
                                            extraction_callback,
                                            &data);
    ITER_CONTEXT_WHILE (!data.success);

    g_assert_true (data.success);
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

static void
test_archives_files (void)
{
    const GStrv first_compressed_hier = (char *[])
    {
        "my_first_file_1",
        "my_first_file_2",
        NULL
    };
    const GStrv second_compressed_hier = (char *[])
    {
        "my_second_file_1",
        "my_second_file_2",
        NULL
    };
    const GStrv extracted_files_hier = (char *[])
    {
        "first_archive/",
        "first_archive/my_first_file_1",
        "first_archive/my_first_file_2",
        "second_archive/",
        "second_archive/my_second_file_1",
        "second_archive/my_second_file_2",
        NULL
    };
    g_autoptr (GFile) tmp_dir = g_file_new_for_path (test_get_tmp_dir ());
    g_autoptr (GFile) first_archive = g_file_new_build_filename (test_get_tmp_dir (),
                                                                 "first_archive.zip",
                                                                 NULL);
    g_autoptr (GFile) second_archive = g_file_new_build_filename (test_get_tmp_dir (),
                                                                  "second_archive.zip",
                                                                  NULL);
    AutoarFormat format = AUTOAR_FORMAT_ZIP;
    AutoarFilter filter = AUTOAR_FILTER_NONE;
    g_autoptr (GList) archives_to_extract = NULL;

    archives_to_extract = g_list_prepend (archives_to_extract, first_archive);
    archives_to_extract = g_list_prepend (archives_to_extract, second_archive);

    /*
     * Compression
     */
    file_hierarchy_create_compress (first_compressed_hier,
                                    "",
                                    first_archive,
                                    format,
                                    filter,
                                    NULL);

    file_hierarchy_assert_exists (first_compressed_hier, "", TRUE);
    g_assert_true (g_file_query_exists (first_archive, NULL));

    file_hierarchy_create_compress (second_compressed_hier,
                                    "",
                                    second_archive,
                                    format,
                                    filter,
                                    NULL);

    file_hierarchy_assert_exists (second_compressed_hier, "", TRUE);
    g_assert_true (g_file_query_exists (second_archive, NULL));

    /*
     * Extraction
     */
    g_autolist (GFile) extracted_files = file_hierarchy_get_files_list (extracted_files_hier, "",
                                                                        TRUE);
    ArchiveCallbackData data = { extracted_files, FALSE, NULL };

    /* Delete original files so that they can be replace with extracted ones. */
    file_hierarchy_delete (first_compressed_hier, "");
    file_hierarchy_assert_exists (first_compressed_hier, "", FALSE);
    file_hierarchy_delete (second_compressed_hier, "");
    file_hierarchy_assert_exists (second_compressed_hier, "", FALSE);

    nautilus_file_operations_extract_files (archives_to_extract,
                                            tmp_dir,
                                            NULL,
                                            NULL,
                                            extraction_callback,
                                            &data);
    ITER_CONTEXT_WHILE (!data.success);

    g_assert_true (data.success);
    file_hierarchy_assert_exists (first_compressed_hier, "", FALSE);
    file_hierarchy_assert_exists (second_compressed_hier, "", FALSE);
    file_hierarchy_assert_exists (extracted_files_hier, "", TRUE);

    test_operation_undo ();

    file_hierarchy_assert_exists (first_compressed_hier, "", FALSE);
    file_hierarchy_assert_exists (second_compressed_hier, "", FALSE);
    file_hierarchy_assert_exists (extracted_files_hier, "", FALSE);

    test_operation_redo ();

    file_hierarchy_assert_exists (first_compressed_hier, "", FALSE);
    file_hierarchy_assert_exists (second_compressed_hier, "", FALSE);
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

    g_test_add_func ("/single_file/short",
                     test_archive_file);
    g_test_add_func ("/single_file/long",
                     test_archive_file_long);
    g_test_add_func ("/single_file/password",
                     test_compress_file_password);
    g_test_add_func ("/single_folder/short",
                     test_archive_full_dir);
    g_test_add_func ("/single_folder/short/early_cancel",
                     test_archive_full_dir_early_cancel);
    g_test_add_func ("/multi_in/single_out/short",
                     test_archive_files);
    g_test_add_func ("/multi_in/multi_out/short",
                     test_archives_files);

    return g_test_run ();
}
