#include <glib.h>
#include "src/nautilus-directory.h"
#include "src/nautilus-file-utilities.h"
#include "src/nautilus-search-directory.h"
#include "src/nautilus-directory.h"
#include "src/nautilus-file-operations.c"
#include <unistd.h>
#include "eel/eel-string.h"
#include "test-utilities.h"

static void
test_copy_one_file (void)
{
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;
    GFileOutputStream *out = NULL;
    g_autoptr (GError) error = NULL;

    create_one_file ("copy");

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
    files = g_list_prepend (files, g_object_ref (file));

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "copy_first_dir_child");
    g_assert_true (g_file_query_exists (result_file, NULL));
    g_assert_true (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_copy_one_file_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;
    GFileOutputStream *out = NULL;
    g_autoptr (GError) error = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "copy_first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }
    files = g_list_prepend (files, g_object_ref (file));

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_operation_undo ();

    result_file = g_file_get_child (second_dir, "copy_first_dir_child");
    g_assert_false (g_file_query_exists (result_file, NULL));
    g_assert_true (g_file_query_exists (file, NULL));

    g_file_delete (file, NULL, NULL);
    g_file_delete (first_dir, NULL, NULL);
    g_file_delete (second_dir, NULL, NULL);
}

static void
test_copy_one_empty_directory (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "copy_first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    files = g_list_prepend (files, g_object_ref (file));

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "copy_first_dir_child");
    g_assert_true (g_file_query_exists (result_file, NULL));
    g_assert_true (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_copy_one_empty_directory_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "copy_first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    files = g_list_prepend (files, g_object_ref (file));

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_operation_undo ();

    result_file = g_file_get_child (second_dir, "copy_first_dir_child");
    g_assert_false (g_file_query_exists (result_file, NULL));

    g_assert_true (g_file_query_exists (file, NULL));

    g_file_delete (file, NULL, NULL);
    g_file_delete (first_dir, NULL, NULL);
    g_file_delete (second_dir, NULL, NULL);
}

static void
test_copy_directories_small (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_copy_directories_small_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_operation_undo ();

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_copy_directories_medium (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 1000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    for (int i = 0; i < 1000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_copy_directories_medium_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 1000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_operation_undo ();

    for (int i = 0; i < 1000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_copy_directories_large (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 10000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    for (int i = 0; i < 10000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_copy_directories_large_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 10000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_operation_undo ();

    for (int i = 0; i < 10000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_copy_files_small (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;
    GFileOutputStream *out = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 10; i++)
    {
        g_autoptr (GError) error = NULL;

        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_copy_files_small_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;
    GFileOutputStream *out = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 10; i++)
    {
        g_autoptr (GError) error = NULL;

        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_operation_undo ();

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_copy_files_medium (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;
    GFileOutputStream *out = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 1000; i++)
    {
        g_autoptr (GError) error = NULL;

        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    for (int i = 0; i < 1000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_copy_files_medium_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;
    GFileOutputStream *out = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 1000; i++)
    {
        g_autoptr (GError) error = NULL;

        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_operation_undo ();

    for (int i = 0; i < 1000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_copy_files_large (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;
    GFileOutputStream *out = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 10000; i++)
    {
        g_autoptr (GError) error = NULL;

        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    for (int i = 0; i < 10000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_copy_files_large_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;
    GFileOutputStream *out = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 10000; i++)
    {
        g_autoptr (GError) error = NULL;

        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_operation_undo ();

    for (int i = 0; i < 10000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_child
 * /tmp/second_dir
 * We're copying first_dir to second_dir.
 */
static void
test_copy_first_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "copy_first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_query_exists (first_dir, NULL));
}

static void
test_copy_first_hierarchy_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_operation_undo ();

    result_file = g_file_get_child (second_dir, "copy_first_dir");
    g_assert_false (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_first_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_file_delete (file, NULL, NULL);
    g_assert_true (g_file_query_exists (first_dir, NULL));
    g_file_delete (first_dir, NULL, NULL);
    g_file_delete (second_dir, NULL, NULL);
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_child
 * /tmp/first_dir/second_child
 * /tmp/second_dir
 * We're copying first_dir to second_dir.
 */
static void
test_copy_second_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_first_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file = g_file_get_child (first_dir, "copy_second_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "copy_first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_first_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (result_file, "copy_second_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));

    file = g_file_get_child (first_dir, "copy_first_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_file_delete (file, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_second_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_file_delete (file, NULL, NULL);    

    g_assert_true (g_file_query_exists (first_dir, NULL));
    g_file_delete (first_dir, NULL, NULL);
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_copy_second_hierarchy_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_first_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file = g_file_get_child (first_dir, "copy_second_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_operation_undo ();

    result_file = g_file_get_child (second_dir, "copy_first_dir");
    g_assert_false (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_first_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (result_file, "copy_second_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "copy_first_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_file_delete (file, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_second_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_file_delete (file, NULL, NULL);    

    g_assert_true (g_file_query_exists (first_dir, NULL));
    g_file_delete (first_dir, NULL, NULL);
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_child/second_child
 * /tmp/second_dir
 * We're copying first_dir to second_dir.
 */
static void
test_copy_third_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_first_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file = g_file_get_child (file, "copy_second_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "copy_first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_first_child");
    g_assert_true (g_file_query_exists (file, NULL));

    file = g_file_get_child (file, "copy_second_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (result_file, "copy_first_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));

    file = g_file_get_child (first_dir, "copy_first_child");
    file = g_file_get_child (file, "copy_second_child");
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (first_dir, "copy_first_child");
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (first_dir, NULL, NULL));

    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_copy_third_hierarchy_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_first_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file = g_file_get_child (file, "copy_second_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_operation_undo ();

    result_file = g_file_get_child (second_dir, "copy_first_dir");
    g_assert_false (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_first_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (file, "copy_second_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (result_file, "copy_first_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "copy_first_child");
    file = g_file_get_child (file, "copy_second_child");
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (first_dir, "copy_first_child");
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (first_dir, NULL, NULL));

    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_dir1/dir1_child
 * /tmp/first_dir/first_dir_dir2/dir2_child
 * /tmp/second_dir
 * We're copying first_dir to second_dir.
 */
static void
test_copy_fourth_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_dir1");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (file, "copy_dir1_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_dir2");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (file, "copy_dir2_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "copy_first_dir");

    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_first_dir_dir1");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "copy_dir1_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    file = g_file_get_child (result_file, "copy_first_dir_dir1");
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (result_file, "copy_first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "copy_dir2_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    file = g_file_get_child (result_file, "copy_first_dir_dir2");
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));

    file = g_file_get_child (first_dir, "copy_first_dir_dir1");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "copy_dir1_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    file = g_file_get_child (first_dir, "copy_first_dir_dir1");
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (first_dir, "copy_first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "copy_dir2_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true  (g_file_delete (file, NULL, NULL));
    file = g_file_get_child (first_dir, "copy_first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_query_exists (first_dir, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));

    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_copy_fourth_hierarchy_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_dir1");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (file, "copy_dir1_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_dir2");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (file, "copy_dir2_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_operation_undo ();

    result_file = g_file_get_child (second_dir, "copy_first_dir");

    g_assert_false (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_first_dir_dir1");
    g_assert_false (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "copy_dir1_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (result_file, "copy_first_dir_dir2");
    g_assert_false (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "copy_dir2_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "copy_first_dir_dir1");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "copy_dir1_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    file = g_file_get_child (first_dir, "copy_first_dir_dir1");
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (first_dir, "copy_first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "copy_dir2_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true  (g_file_delete (file, NULL, NULL));
    file = g_file_get_child (first_dir, "copy_first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_query_exists (first_dir, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));

    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_child
 * /tmp/second_dir/second_dir_child
 * /tmp/third_dir
 * We're copying first_dir and second_dir to third_dir.
 */
static void
test_copy_fifth_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) third_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    files = g_list_prepend (files, g_object_ref (second_dir));
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    file = g_file_get_child (second_dir, "copy_second_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    third_dir = g_file_get_child (root, "copy_third_dir");
    g_assert_true (third_dir != NULL);
    g_file_make_directory (third_dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        third_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (third_dir, "copy_first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (result_file, NULL, NULL));

    result_file = g_file_get_child (third_dir, "copy_second_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_second_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (result_file, NULL, NULL));

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_query_exists (first_dir, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));

    file = g_file_get_child (second_dir, "copy_second_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_query_exists (second_dir, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));

    g_assert_true (g_file_delete (third_dir, NULL, NULL));
}

static void
test_copy_fifth_hierarchy_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) third_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    files = g_list_prepend (files, g_object_ref (second_dir));
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    file = g_file_get_child (second_dir, "copy_second_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    third_dir = g_file_get_child (root, "copy_third_dir");
    g_assert_true (third_dir != NULL);
    g_file_make_directory (third_dir, NULL, NULL);

    nautilus_file_operations_copy_sync (files,
                                        third_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_operation_undo ();

    result_file = g_file_get_child (third_dir, "copy_first_dir");
    g_assert_false (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_first_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));

    result_file = g_file_get_child (third_dir, "copy_second_dir");
    g_assert_false (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_second_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_query_exists (first_dir, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));

    file = g_file_get_child (second_dir, "copy_second_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_query_exists (second_dir, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));

    g_assert_true (g_file_delete (third_dir, NULL, NULL));
}

static void
setup_test_suite (void)
{
    g_test_add_func ("/test-copy-one-file/1.0",
                     test_copy_one_file);
    // g_test_add_func ("/test-copy-one-file-undo/1.0",
    //                  test_copy_one_file_undo);
    // g_test_add_func ("/test-copy-one-empty-directory/1.0",
    //                  test_copy_one_empty_directory);
    // g_test_add_func ("/test-copy-one-empty-directory-undo/1.0",
    //                  test_copy_one_empty_directory_undo);
    // g_test_add_func ("/test-copy-files/1.0",
    //                  test_copy_files_small);
    // g_test_add_func ("/test-copy-files-undo/1.0",
    //                  test_copy_files_small_undo);
    // g_test_add_func ("/test-copy-files/1.1",
    //                  test_copy_files_medium);
    // g_test_add_func ("/test-copy-files-undo/1.1",
    //                  test_copy_files_medium_undo);
    // g_test_add_func ("/test-copy-files/1.2",
    //                  test_copy_files_large);
    // g_test_add_func ("/test-copy-files-undo/1.2",
    //                  test_copy_files_large_undo);
    // g_test_add_func ("/test-copy-directories/1.0",
    //                  test_copy_directories_small);
    // g_test_add_func ("/test-copy-directories-undo/1.0",
    //                  test_copy_directories_small_undo);
    // g_test_add_func ("/test-copy-directories/1.1",
    //                  test_copy_directories_medium);
    // g_test_add_func ("/test-copy-directories-undo/1.1",
    //                  test_copy_directories_medium_undo);
    // g_test_add_func ("/test-copy-directories/1.2",
    //                  test_copy_directories_large);
    // g_test_add_func ("/test-copy-directories-undo/1.2",
    //                  test_copy_directories_large_undo);
    // g_test_add_func ("/test-copy-hierarchy/1.0",
    //                  test_copy_first_hierarchy);
    // g_test_add_func ("/test-copy-hierarchy-undo/1.0",
    //                  test_copy_first_hierarchy_undo);
    // g_test_add_func ("/test-copy-hierarchy/1.1",
    //                  test_copy_second_hierarchy);
    // g_test_add_func ("/test-copy-hierarchy-undo/1.1",
    //                  test_copy_second_hierarchy_undo);
    // g_test_add_func ("/test-copy-hierarchy/1.2",
    //                  test_copy_third_hierarchy);
    // g_test_add_func ("/test-copy-hierarchy-undo/1.2",
    //                  test_copy_third_hierarchy_undo);
    // g_test_add_func ("/test-copy-hierarchy/1.3",
    //                  test_copy_fourth_hierarchy);
    // g_test_add_func ("/test-copy-hierarchy-undo/1.3",
    //                  test_copy_fourth_hierarchy_undo);
    // g_test_add_func ("/test-copy-hierarchy/1.4",
    //                  test_copy_fifth_hierarchy);
    // g_test_add_func ("/test-copy-hierarchy-undo/1.4",
    //                  test_copy_fifth_hierarchy_undo);
}

int
main (int argc, char *argv[])
{
    g_autoptr (NautilusFileUndoManager) undo_manager = NULL;

    undo_manager = nautilus_file_undo_manager_new ();
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points();

    setup_test_suite ();

    return g_test_run ();
}