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
test_copy_simple_file_same_name (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;

    g_setenv ("TESTVAR", "TRUE", TRUE);
    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "file_to_copy");
    g_assert_true (file != NULL);
    g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_file_sync (file,
                                             second_dir,
                                             "file_to_copy",
                                             "file_to_copy",
                                             NULL,
                                             NULL,
                                             NULL);

    result_file = g_file_get_child (second_dir, "file_to_copy");
    g_assert_true (g_file_query_exists (result_file, NULL));

    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_copy_simple_file_new_name (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;

    g_setenv ("TESTVAR", "TRUE", TRUE);
    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "file_to_copy");
    g_assert_true (file != NULL);
    g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_file_sync (file,
                                             second_dir,
                                             "file_to_copy",
                                             "new_file_to_copy",
                                             NULL,
                                             NULL,
                                             NULL);

    result_file = g_file_get_child (second_dir, "new_file_to_copy");
    g_assert_true (g_file_query_exists (result_file, NULL));

    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_copy_empty_directory_same_name (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;

    g_setenv ("TESTVAR", "TRUE", TRUE);
    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "dir_to_copy");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_file_sync (file,
                                             second_dir,
                                             "dir_to_copy",
                                             "dir_to_copy",
                                             NULL,
                                             NULL,
                                             NULL);

    result_file = g_file_get_child (second_dir, "dir_to_copy");
    g_assert_true (g_file_query_exists (result_file, NULL));

    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
test_copy_empty_directory_new_name (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;

    g_setenv ("TESTVAR", "TRUE", TRUE);
    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "dir_to_copy");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_file_sync (file,
                                             second_dir,
                                             "dir_to_copy",
                                             "new_dir_to_copy",
                                             NULL,
                                             NULL,
                                             NULL);

    result_file = g_file_get_child (second_dir, "new_dir_to_copy");
    g_assert_true (g_file_query_exists (result_file, NULL));

    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

static void
setup_test_suite (void)
{
    g_test_add_func ("/copy-simple-file/1.0",
                     test_copy_simple_file_same_name);
    g_test_add_func ("/copy-simple-file/1.1",
                     test_copy_simple_file_new_name);
    g_test_add_func ("/copy-empty-dir/1.0",
                     test_copy_empty_directory_same_name);
    g_test_add_func ("/copy-empty-dir/1.1",
                     test_copy_empty_directory_new_name);
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
