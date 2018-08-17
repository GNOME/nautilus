#include "test-utilities.h"

static void
test_copy_one_file (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    create_one_file ("copy");

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "copy_first_dir");
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);
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

    empty_directory (root, "copy");
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

    create_one_file ("copy");

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "copy_first_dir");
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);
    files = g_list_prepend (files, g_object_ref (file));

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_operation_undo ();

    result_file = g_file_get_child (second_dir, "copy_first_dir_child");
    g_assert_false (g_file_query_exists (result_file, NULL));
    g_assert_true (g_file_query_exists (file, NULL));

    empty_directory (root, "copy");
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

    create_one_empty_directory ("copy");

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);
    first_dir = g_file_get_child (root, "copy_first_dir");
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);
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

    empty_directory (root, "copy");
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

    create_one_empty_directory ("copy");

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);
    first_dir = g_file_get_child (root, "copy_first_dir");
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);
    files = g_list_prepend (files, g_object_ref (file));

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    test_operation_undo ();

    result_file = g_file_get_child (second_dir, "copy_first_dir_child");
    g_assert_false (g_file_query_exists (result_file, NULL));

    g_assert_true (g_file_query_exists (file, NULL));

    empty_directory (root, "copy");
}

static void
test_copy_files_small (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    create_multiple_files ("copy", 10);

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);

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
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory (root, "copy");
}

static void
test_copy_files_small_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    create_multiple_files ("copy", 10);

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);

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
    }

    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory (root, "copy");
}

static void
test_copy_files_medium (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    create_multiple_files ("copy", 1000);

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    for (int i = 0; i < 1000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);

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
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory (root, "copy");
}

static void
test_copy_files_medium_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    create_multiple_files ("copy", 1000);

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    for (int i = 0; i < 1000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);

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
    }

    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory (root, "copy");
}

static void
test_copy_files_large (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    create_multiple_files ("copy", 10000);

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    for (int i = 0; i < 10000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);

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
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory (root, "copy");
}

static void
test_copy_files_large_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    create_multiple_files ("copy", 10000);

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    for (int i = 0; i < 10000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);

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
    }

    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory (root, "copy");
}

static void
test_copy_directories_small (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    create_multiple_directories ("copy", 10);

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);

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
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory (root, "copy");
}

static void
test_copy_directories_small_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    create_multiple_directories ("copy", 10);

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    for (int i = 0; i < 10; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);

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
    }

    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory (root, "copy");
}

static void
test_copy_directories_medium (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    create_multiple_directories ("copy", 1000);

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    for (int i = 0; i < 1000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);

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
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory (root, "copy");
}

static void
test_copy_directories_medium_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    create_multiple_directories ("copy", 1000);

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    for (int i = 0; i < 1000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);

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
    }

    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory (root, "copy");
}

static void
test_copy_directories_large (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    create_multiple_directories ("copy", 10000);

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    for (int i = 0; i < 10000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);

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
        file = g_file_get_child (root, file_name);
        g_assert_true (g_file_query_exists (file, NULL));
    }

    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory (root, "copy");
}

static void
test_copy_directories_large_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
    g_autofree gchar *file_name = NULL;

    create_multiple_directories ("copy", 10000);

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    for (int i = 0; i < 10000; i++)
    {
        file_name = g_strdup_printf ("copy_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (dir != NULL);

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
    }

    g_assert_true (g_file_query_exists (dir, NULL));
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_child
 * /tmp/second_dir
 * We're copying first_dir to second_dir.
 */
static void
test_copy_full_directory (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    create_one_file ("copy");

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "copy_first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_query_exists (first_dir, NULL));

    empty_directory (root, "copy");
}

static void
test_copy_full_directory_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    create_one_file ("copy");

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);

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
    g_assert_true (g_file_query_exists (first_dir, NULL));

    empty_directory (root, "copy");
}

// /* The hierarchy looks like this:
//  * /tmp/first_dir/first_child
//  * /tmp/first_dir/second_child
//  * /tmp/second_dir
//  * We're copying first_dir to second_dir.
//  */
static void
test_copy_first_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    create_first_hierarchy ("copy");

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "copy_first_child");
    g_assert_true (file != NULL);
    file = g_file_get_child (first_dir, "copy_second_child");
    g_assert_true (file != NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);

    nautilus_file_operations_copy_sync (files,
                                        second_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (second_dir, "copy_first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_first_child");
    g_assert_true (g_file_query_exists (file, NULL));

    file = g_file_get_child (result_file, "copy_second_child");
    g_assert_true (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "copy_first_child");
    g_assert_true (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "copy_second_child");
    g_assert_true (g_file_query_exists (file, NULL));

    g_assert_true (g_file_query_exists (first_dir, NULL));

    empty_directory (root, "copy");
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

    create_first_hierarchy ("copy");

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "copy_first_child");
    g_assert_true (file != NULL);
    file = g_file_get_child (first_dir, "copy_second_child");
    g_assert_true (file != NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);

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

    file = g_file_get_child (first_dir, "copy_second_child");
    g_assert_true (g_file_query_exists (file, NULL));

    g_assert_true (g_file_query_exists (first_dir, NULL));

    empty_directory (root, "copy");
}

// /* The hierarchy looks like this:
//  * /tmp/first_dir/first_child/second_child
//  * /tmp/second_dir
//  * We're copying first_dir to second_dir.
//  */
static void
test_copy_second_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    create_second_hierarchy ("copy");

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "copy_first_child");
    g_assert_true (file != NULL);
    file = g_file_get_child (file, "copy_second_child");
    g_assert_true (file != NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);

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

    file = g_file_get_child (result_file, "copy_first_child");
    g_assert_true (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "copy_first_child");
    file = g_file_get_child (file, "copy_second_child");

    file = g_file_get_child (first_dir, "copy_first_child");

    empty_directory (root, "copy");
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

    create_second_hierarchy ("copy");

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "copy_first_child");
    g_assert_true (file != NULL);
    file = g_file_get_child (file, "copy_second_child");
    g_assert_true (file != NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);

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

    file = g_file_get_child (first_dir, "copy_first_child");

    empty_directory (root, "copy");
}

// /* The hierarchy looks like this:
//  * /tmp/first_dir/first_dir_dir1/dir1_child
//  * /tmp/first_dir/first_dir_dir2/dir2_child
//  * /tmp/second_dir
//  * We're copying first_dir to second_dir.
//  */
static void
test_copy_third_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    create_third_hierarchy ("copy");

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_dir1");
    g_assert_true (file != NULL);

    file = g_file_get_child (file, "copy_dir1_child");
    g_assert_true (file != NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_dir2");
    g_assert_true (file != NULL);

    file = g_file_get_child (file, "copy_dir2_child");
    g_assert_true (file != NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);

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
    file = g_file_get_child (result_file, "copy_first_dir_dir1");

    file = g_file_get_child (result_file, "copy_first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "copy_dir2_child");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (result_file, "copy_first_dir_dir2");

    file = g_file_get_child (first_dir, "copy_first_dir_dir1");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "copy_dir1_child");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (first_dir, "copy_first_dir_dir1");

    file = g_file_get_child (first_dir, "copy_first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "copy_dir2_child");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (first_dir, "copy_first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));

    g_assert_true (g_file_query_exists (first_dir, NULL));

    empty_directory (root, "copy");
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

    create_third_hierarchy ("copy");

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_dir1");
    g_assert_true (file != NULL);

    file = g_file_get_child (file, "copy_dir1_child");
    g_assert_true (file != NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_dir2");
    g_assert_true (file != NULL);

    file = g_file_get_child (file, "copy_dir2_child");
    g_assert_true (file != NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    g_assert_true (second_dir != NULL);

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
    file = g_file_get_child (first_dir, "copy_first_dir_dir1");

    file = g_file_get_child (first_dir, "copy_first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "copy_dir2_child");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (first_dir, "copy_first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));

    g_assert_true (g_file_query_exists (first_dir, NULL));

    empty_directory (root, "copy");
}

// /* The hierarchy looks like this:
//  * /tmp/first_dir/first_dir_child
//  * /tmp/second_dir/second_dir_child
//  * /tmp/third_dir
//  * We're copying first_dir and second_dir to third_dir.
//  */
static void
test_copy_fourth_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) third_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    create_fourth_hierarchy ("copy");

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    files = g_list_prepend (files, g_object_ref (second_dir));
    g_assert_true (second_dir != NULL);

    file = g_file_get_child (second_dir, "copy_second_dir_child");
    g_assert_true (file != NULL);

    third_dir = g_file_get_child (root, "copy_third_dir");
    g_assert_true (third_dir != NULL);

    nautilus_file_operations_copy_sync (files,
                                        third_dir,
                                        NULL,
                                        NULL,
                                        NULL);

    result_file = g_file_get_child (third_dir, "copy_first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));

    result_file = g_file_get_child (third_dir, "copy_second_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "copy_second_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_query_exists (first_dir, NULL));

    file = g_file_get_child (second_dir, "copy_second_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_query_exists (second_dir, NULL));

    empty_directory (root, "copy");
}

static void
test_copy_fourth_hierarchy_undo (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) third_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) result_file = NULL;
    g_autolist (GFile) files = NULL;

    create_fourth_hierarchy ("copy");

    root = g_file_new_for_path (g_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "copy_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "copy_first_dir_child");
    g_assert_true (file != NULL);

    second_dir = g_file_get_child (root, "copy_second_dir");
    files = g_list_prepend (files, g_object_ref (second_dir));
    g_assert_true (second_dir != NULL);

    file = g_file_get_child (second_dir, "copy_second_dir_child");
    g_assert_true (file != NULL);

    third_dir = g_file_get_child (root, "copy_third_dir");
    g_assert_true (third_dir != NULL);

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
    g_assert_true (g_file_query_exists (first_dir, NULL));

    file = g_file_get_child (second_dir, "copy_second_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_query_exists (second_dir, NULL));

    empty_directory (root, "copy");
}

static void
setup_test_suite (void)
{
    g_test_add_func ("/test-copy-one-file/1.0",
                     test_copy_one_file);
    g_test_add_func ("/test-copy-one-file-undo/1.0",
                     test_copy_one_file_undo);
    g_test_add_func ("/test-copy-one-empty-directory/1.0",
                     test_copy_one_empty_directory);
    g_test_add_func ("/test-copy-one-empty-directory-undo/1.0",
                     test_copy_one_empty_directory_undo);
    g_test_add_func ("/test-copy-files/1.0",
                     test_copy_files_small);
    g_test_add_func ("/test-copy-files-undo/1.0",
                     test_copy_files_small_undo);
    g_test_add_func ("/test-copy-files/1.1",
                     test_copy_files_medium);
    g_test_add_func ("/test-copy-files-undo/1.1",
                     test_copy_files_medium_undo);
    g_test_add_func ("/test-copy-files/1.2",
                     test_copy_files_large);
    g_test_add_func ("/test-copy-files-undo/1.2",
                     test_copy_files_large_undo);
    g_test_add_func ("/test-copy-directories/1.0",
                     test_copy_directories_small);
    g_test_add_func ("/test-copy-directories-undo/1.0",
                     test_copy_directories_small_undo);
    g_test_add_func ("/test-copy-directories/1.1",
                     test_copy_directories_medium);
    g_test_add_func ("/test-copy-directories-undo/1.1",
                     test_copy_directories_medium_undo);
    g_test_add_func ("/test-copy-directories/1.2",
                     test_copy_directories_large);
    g_test_add_func ("/test-copy-directories-undo/1.2",
                     test_copy_directories_large_undo);
    g_test_add_func ("/test-copy-hierarchy/1.0",
                     test_copy_full_directory);
    g_test_add_func ("/test-copy-hierarchy-undo/1.0",
                     test_copy_full_directory_undo);
    g_test_add_func ("/test-copy-hierarchy/1.1",
                     test_copy_first_hierarchy);
    g_test_add_func ("/test-copy-hierarchy-undo/1.1",
                     test_copy_first_hierarchy_undo);
    g_test_add_func ("/test-copy-hierarchy/1.2",
                     test_copy_second_hierarchy);
    g_test_add_func ("/test-copy-hierarchy-undo/1.2",
                     test_copy_second_hierarchy_undo);
    g_test_add_func ("/test-copy-hierarchy/1.3",
                     test_copy_third_hierarchy);
    g_test_add_func ("/test-copy-hierarchy-undo/1.3",
                     test_copy_third_hierarchy_undo);
    g_test_add_func ("/test-copy-hierarchy/1.4",
                     test_copy_fourth_hierarchy);
    g_test_add_func ("/test-copy-hierarchy-undo/1.4",
                     test_copy_fourth_hierarchy_undo);
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