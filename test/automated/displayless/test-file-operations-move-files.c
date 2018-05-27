#include <glib.h>
#include <glib/gprintf.h>
#include "src/nautilus-directory.h"
#include "src/nautilus-file-utilities.h"
#include "src/nautilus-search-directory.h"
#include "src/nautilus-directory.h"
#include "src/nautilus-file-operations.c"
#include <unistd.h>
#include "eel/eel-string.h"

static void
test_move_one_file (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    g_setenv ("TESTVAR", "TRUE", TRUE);
    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "file_to_copy");
    g_assert_true (file != NULL);
    g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
    files = g_list_prepend (files, file);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "file_to_copy");
    g_assert_true (g_file_query_exists (result_file, NULL));
    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_move_one_empty_directory (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    g_setenv ("TESTVAR", "TRUE", TRUE);
    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "dir_to_copy");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    files = g_list_prepend (files, file);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "dir_to_copy");
    g_assert_true (g_file_query_exists (result_file, NULL));
    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_move_files_small (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    g_setenv ("TESTVAR", "TRUE", TRUE);
    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_files_medium (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    g_setenv ("TESTVAR", "TRUE", TRUE);
    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 1000; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    nautilus_file_operations_move_sync (files,
                                        dir,
                                        NULL,
                                        NULL,
                                        NULL);

    for (int i = 0; i < 1000; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (dir, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
        g_assert_true (g_file_delete (file, NULL, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));
    g_assert_true (g_file_delete (dir, NULL, NULL));
}

static void
test_move_files_large (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    g_setenv ("TESTVAR", "TRUE", TRUE);
    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 1000; i++)
    {
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "dir");
    g_assert_true (dir != NULL);
    g_file_make_directory (dir, NULL, NULL);

    // nautilus_file_operations_move_sync (files,
    //                                     dir,
    //                                     NULL,
    //                                     NULL,
    //                                     NULL);

//     for (int i = 0; i < 1000; i++)
//     {
//         file_name = g_strdup_printf ("file_%i", i);
//         file = g_file_get_child (dir, file_name);
//         g_assert_true (g_file_query_exists (file, NULL));
//         g_assert_true (g_file_delete (file, NULL, NULL));
//     }

//     g_assert_true (g_file_query_exists (dir, NULL));
//     g_assert_true (g_file_delete (dir, NULL, NULL));
}


static void
setup_test_suite (void)
{
    g_test_add_func ("/test-move-one-file/1.0",
                     test_move_one_file);
    g_test_add_func ("/test-move-one-empty-directory/1.0",
                     test_move_one_empty_directory);
    g_test_add_func ("/test-move-files/1.0",
                     test_move_files_small);
    g_test_add_func ("/test-move-files/1.1",
                     test_move_files_medium);
    g_test_add_func ("/test-move-files/1.2",
                     test_move_files_large);
}

int
main (int argc, char *argv[])
{
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points();

    setup_test_suite ();

    return g_test_run ();
}
