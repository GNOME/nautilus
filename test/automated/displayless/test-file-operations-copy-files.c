#include "test-utilities.h"

#include <src/nautilus-file-operations.h>
#include <src/nautilus-file-undo-manager.h>
#include <src/nautilus-file-utilities.h>
#include <src/nautilus-tag-manager.h>

static void
test_copy_one_file (void)
{
    g_autoptr (GFile) file = g_file_new_build_filename (test_get_tmp_dir (),
                                                        "copy_first_dir",
                                                        "copy_first_dir_child",
                                                        NULL);
    g_autoptr (GFile) second_dir = g_file_new_build_filename (test_get_tmp_dir (),
                                                              "copy_second_dir",
                                                              NULL);
    g_autoptr (GFile) result_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                               "copy_second_dir",
                                                               "copy_first_dir_child",
                                                               NULL);

    create_one_file ("copy");

    nautilus_file_operations_copy_sync (&(GList){ .data = file },
                                        second_dir);

    g_assert_true (g_file_query_exists (result_file, NULL));
    g_assert_true (g_file_query_exists (file, NULL));

    test_operation_undo ();

    g_assert_false (g_file_query_exists (result_file, NULL));
    g_assert_true (g_file_query_exists (file, NULL));

    test_clear_tmp_dir ();
}

static void
test_copy_one_empty_directory (void)
{
    g_autoptr (GFile) file = g_file_new_build_filename (test_get_tmp_dir (),
                                                        "copy_first_dir",
                                                        "copy_first_dir_child",
                                                        NULL);
    g_autoptr (GFile) second_dir = g_file_new_build_filename (test_get_tmp_dir (),
                                                              "copy_second_dir",
                                                              NULL);
    g_autoptr (GFile) result_file = g_file_new_build_filename (test_get_tmp_dir (),
                                                               "copy_second_dir",
                                                               "copy_first_dir_child",
                                                               NULL);
    g_autolist (GFile) files = g_list_prepend (NULL, g_object_ref (file));

    create_one_empty_directory ("copy");

    nautilus_file_operations_copy_sync (&(GList){ .data = file },
                                        second_dir);

    g_assert_true (g_file_query_exists (result_file, NULL));
    g_assert_true (g_file_query_exists (file, NULL));

    test_operation_undo ();

    g_assert_false (g_file_query_exists (result_file, NULL));
    g_assert_true (g_file_query_exists (file, NULL));

    test_clear_tmp_dir ();
}

static void
copy_multiple_files (const gchar *prefix,
                     GFile       *src,
                     GFile       *dest,
                     guint        num)
{
    g_autolist (GFile) files = NULL;

    for (guint i = 0; i < num; i++)
    {
        g_autofree gchar *file_name = g_strdup_printf ("%s_%i", prefix, i);
        GFile *file = g_file_get_child (src, file_name);

        g_assert_true (g_file_query_exists (file, NULL));
        files = g_list_prepend (files, file);
    }

    nautilus_file_operations_copy_sync (files, dest);
}

static void
verify_multiple_copies_existance (const gchar *prefix,
                                  GFile       *src,
                                  GFile       *dest,
                                  guint        num,
                                  gboolean     copies_exist)
{
    for (guint i = 0; i < num; i++)
    {
        g_autofree gchar *file_name = g_strdup_printf ("%s_%i", prefix, i);
        g_autoptr (GFile) file_in_source = g_file_get_child (src, file_name);
        g_autoptr (GFile) file_in_destination = g_file_get_child (dest, file_name);

        g_assert_true (g_file_query_exists (file_in_source, NULL));
        if (copies_exist)
        {
            g_assert_true (g_file_query_exists (file_in_destination, NULL));
        }
        else
        {
            g_assert_false (g_file_query_exists (file_in_destination, NULL));
        }
    }
}

static void
test_copy_files_small (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) dir = NULL;

    create_multiple_files ("copy", 10);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (g_file_query_exists (dir, NULL));

    copy_multiple_files ("copy_file", root, dir, 10);

    verify_multiple_copies_existance ("copy_file", root, dir, 10, TRUE);
    g_assert_true (g_file_query_exists (dir, NULL));

    test_operation_undo ();

    verify_multiple_copies_existance ("copy_file", root, dir, 10, FALSE);
    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory_by_prefix (root, "copy");
}

static void
test_copy_files_medium (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) dir = NULL;

    create_multiple_files ("copy", 1000);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (g_file_query_exists (dir, NULL));

    copy_multiple_files ("copy_file", root, dir, 1000);

    verify_multiple_copies_existance ("copy_file", root, dir, 1000, TRUE);
    g_assert_true (g_file_query_exists (dir, NULL));

    test_operation_undo ();

    verify_multiple_copies_existance ("copy_file", root, dir, 1000, FALSE);
    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory_by_prefix (root, "copy");
}

static void
test_copy_files_large (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) dir = NULL;

    create_multiple_files ("copy", 10000);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    dir = g_file_get_child (root, "copy_dir");
    g_assert_true (g_file_query_exists (dir, NULL));

    copy_multiple_files ("copy_file", root, dir, 10000);

    verify_multiple_copies_existance ("copy_file", root, dir, 10000, TRUE);
    g_assert_true (g_file_query_exists (dir, NULL));

    test_operation_undo ();

    verify_multiple_copies_existance ("copy_file", root, dir, 10000, FALSE);
    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory_by_prefix (root, "copy");
}

static void
test_copy_directories_small (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) dir = NULL;

    create_multiple_directories ("copy", 10);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    dir = g_file_get_child (root, "copy_destination_dir");
    g_assert_true (g_file_query_exists (dir, NULL));

    copy_multiple_files ("copy_dir", root, dir, 10);

    verify_multiple_copies_existance ("copy_dir", root, dir, 10, TRUE);
    g_assert_true (g_file_query_exists (dir, NULL));

    test_operation_undo ();

    verify_multiple_copies_existance ("copy_dir", root, dir, 10, FALSE);
    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory_by_prefix (root, "copy");
}

static void
test_copy_directories_medium (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) dir = NULL;

    create_multiple_directories ("copy", 1000);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    dir = g_file_get_child (root, "copy_destination_dir");
    g_assert_true (g_file_query_exists (dir, NULL));

    copy_multiple_files ("copy_dir", root, dir, 1000);

    verify_multiple_copies_existance ("copy_dir", root, dir, 1000, TRUE);
    g_assert_true (g_file_query_exists (dir, NULL));

    test_operation_undo ();

    verify_multiple_copies_existance ("copy_dir", root, dir, 1000, FALSE);
    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory_by_prefix (root, "copy");
}

static void
test_copy_directories_large (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) dir = NULL;

    create_multiple_directories ("copy", 10000);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    dir = g_file_get_child (root, "copy_destination_dir");
    g_assert_true (g_file_query_exists (dir, NULL));

    copy_multiple_files ("copy_dir", root, dir, 10000);

    verify_multiple_copies_existance ("copy_dir", root, dir, 10000, TRUE);
    g_assert_true (g_file_query_exists (dir, NULL));

    test_operation_undo ();

    verify_multiple_copies_existance ("copy_dir", root, dir, 10000, FALSE);
    g_assert_true (g_file_query_exists (dir, NULL));

    empty_directory_by_prefix (root, "copy");
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_child
 * /tmp/second_dir
 * We're copying first_dir to second_dir.
 */
static void
test_copy_full_directory (void)
{
    const GStrv before_copy_hierarchy = (char *[])
    {
        "%s_first_dir/",
        "%s_first_dir/%s_first_dir_child",
        NULL
    };
    const GStrv after_copy_hierarchy = (char *[])
    {
        "%s_second_dir/%s_first_dir/",
        "%s_second_dir/%s_first_dir/%s_first_dir_child",
        NULL
    };
    g_autoptr (GFile) first_dir = g_file_new_build_filename (test_get_tmp_dir (),
                                                             "copy_first_dir",
                                                             NULL);
    g_autoptr (GFile) second_dir = g_file_new_build_filename (test_get_tmp_dir (),
                                                              "copy_second_dir",
                                                              NULL);

    create_one_file ("copy");

    nautilus_file_operations_copy_sync (&(GList){ .data = first_dir },
                                        second_dir);

    file_hierarchy_assert_exists (before_copy_hierarchy, "copy", TRUE);
    file_hierarchy_assert_exists (after_copy_hierarchy, "copy", TRUE);

    test_operation_undo ();

    file_hierarchy_assert_exists (before_copy_hierarchy, "copy", TRUE);
    file_hierarchy_assert_exists (after_copy_hierarchy, "copy", FALSE);

    test_clear_tmp_dir ();
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_child
 * /tmp/first_dir/second_child
 * /tmp/second_dir
 * We're copying first_dir to second_dir.
 */
static void
test_copy_first_hierarchy (void)
{
    const GStrv before_copy_hierarchy = (char *[])
    {
        "%s_first_dir/",
        "%s_first_dir/%s_first_child/",
        "%s_first_dir/%s_second_child/",
        NULL
    };
    const GStrv after_copy_hierarchy = (char *[])
    {
        "%s_second_dir/%s_first_dir/",
        "%s_second_dir/%s_first_dir/%s_first_child/",
        "%s_second_dir/%s_first_dir/%s_second_child/",
        NULL
    };
    g_autoptr (GFile) first_dir = g_file_new_build_filename (test_get_tmp_dir (),
                                                             "copy_first_dir",
                                                             NULL);
    g_autoptr (GFile) second_dir = g_file_new_build_filename (test_get_tmp_dir (),
                                                              "copy_second_dir",
                                                              NULL);

    create_first_hierarchy ("copy");

    nautilus_file_operations_copy_sync (&(GList){ .data = first_dir },
                                        second_dir);

    file_hierarchy_assert_exists (before_copy_hierarchy, "copy", TRUE);
    file_hierarchy_assert_exists (after_copy_hierarchy, "copy", TRUE);

    test_operation_undo ();

    file_hierarchy_assert_exists (before_copy_hierarchy, "copy", TRUE);
    file_hierarchy_assert_exists (after_copy_hierarchy, "copy", FALSE);

    test_clear_tmp_dir ();
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_child/second_child
 * /tmp/second_dir
 * We're copying first_dir to second_dir.
 */
static void
test_copy_second_hierarchy (void)
{
    const GStrv before_copy_hierarchy = (char *[])
    {
        "%s_first_dir/",
        "%s_first_dir/%s_first_child/",
        "%s_first_dir/%s_first_child/%s_second_child/",
        NULL
    };
    const GStrv after_copy_hierarchy = (char *[])
    {
        "%s_second_dir/%s_first_dir/",
        "%s_second_dir/%s_first_dir/%s_first_child/",
        "%s_second_dir/%s_first_dir/%s_first_child/%s_second_child/",
        NULL
    };
    g_autoptr (GFile) first_dir = g_file_new_build_filename (test_get_tmp_dir (),
                                                             "copy_first_dir",
                                                             NULL);
    g_autoptr (GFile) second_dir = g_file_new_build_filename (test_get_tmp_dir (),
                                                              "copy_second_dir",
                                                              NULL);

    create_second_hierarchy ("copy");

    nautilus_file_operations_copy_sync (&(GList){ .data = first_dir },
                                        second_dir);

    file_hierarchy_assert_exists (before_copy_hierarchy, "copy", TRUE);
    file_hierarchy_assert_exists (after_copy_hierarchy, "copy", TRUE);

    test_operation_undo ();

    file_hierarchy_assert_exists (before_copy_hierarchy, "copy", TRUE);
    file_hierarchy_assert_exists (after_copy_hierarchy, "copy", FALSE);

    test_clear_tmp_dir ();
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_dir1/dir1_child
 * /tmp/first_dir/first_dir_dir2/dir2_child
 * /tmp/second_dir
 * We're copying first_dir to second_dir.
 */
static void
test_copy_third_hierarchy (void)
{
    const GStrv before_copy_hierarchy = (char *[])
    {
        "%s_first_dir/",
        "%s_first_dir/%s_first_dir_dir1/",
        "%s_first_dir/%s_first_dir_dir1/%s_dir1_child/",

        "%s_first_dir/%s_first_dir_dir2/",
        "%s_first_dir/%s_first_dir_dir2/%s_dir2_child",
        NULL
    };
    const GStrv after_copy_hierarchy = (char *[])
    {
        "%s_second_dir/%s_first_dir/",
        "%s_second_dir/%s_first_dir/%s_first_dir_dir1/",
        "%s_second_dir/%s_first_dir/%s_first_dir_dir1/%s_dir1_child/",

        "%s_second_dir/%s_first_dir/%s_first_dir_dir2/",
        "%s_second_dir/%s_first_dir/%s_first_dir_dir2/%s_dir2_child",
        NULL
    };
    g_autoptr (GFile) first_dir = g_file_new_build_filename (test_get_tmp_dir (),
                                                             "copy_first_dir",
                                                             NULL);
    g_autoptr (GFile) second_dir = g_file_new_build_filename (test_get_tmp_dir (),
                                                              "copy_second_dir",
                                                              NULL);

    create_third_hierarchy ("copy");

    nautilus_file_operations_copy_sync (&(GList){ .data = first_dir },
                                        second_dir);

    file_hierarchy_assert_exists (before_copy_hierarchy, "copy", TRUE);
    file_hierarchy_assert_exists (after_copy_hierarchy, "copy", TRUE);

    test_operation_undo ();

    file_hierarchy_assert_exists (before_copy_hierarchy, "copy", TRUE);
    file_hierarchy_assert_exists (after_copy_hierarchy, "copy", FALSE);

    test_clear_tmp_dir ();
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_child
 * /tmp/second_dir/second_dir_child
 * /tmp/third_dir
 * We're copying first_dir and second_dir to third_dir.
 */
static void
test_copy_fourth_hierarchy (void)
{
    const GStrv before_copy_hierarchy = (char *[])
    {
        "%s_first_dir/",
        "%s_first_dir/%s_first_dir_child/",

        "%s_second_dir/",
        "%s_second_dir/%s_second_dir_child/",
        NULL
    };
    const GStrv after_copy_hierarchy = (char *[])
    {
        "%s_third_dir/%s_first_dir/",
        "%s_third_dir/%s_first_dir/%s_first_dir_child/",

        "%s_third_dir/%s_second_dir/",
        "%s_third_dir/%s_second_dir/%s_second_dir_child/",

        NULL
    };
    g_autoptr (GFile) first_dir = g_file_new_build_filename (test_get_tmp_dir (),
                                                             "copy_first_dir",
                                                             NULL);
    g_autoptr (GFile) second_dir = g_file_new_build_filename (test_get_tmp_dir (),
                                                              "copy_second_dir",
                                                              NULL);
    g_autoptr (GFile) third_dir = g_file_new_build_filename (test_get_tmp_dir (),
                                                             "copy_third_dir",
                                                             NULL);
    g_autolist (GFile) files = NULL;

    create_fourth_hierarchy ("copy");

    files = g_list_prepend (files, g_object_ref (first_dir));
    files = g_list_prepend (files, g_object_ref (second_dir));

    nautilus_file_operations_copy_sync (files, third_dir);

    file_hierarchy_assert_exists (before_copy_hierarchy, "copy", TRUE);
    file_hierarchy_assert_exists (after_copy_hierarchy, "copy", TRUE);

    test_operation_undo ();

    file_hierarchy_assert_exists (before_copy_hierarchy, "copy", TRUE);
    file_hierarchy_assert_exists (after_copy_hierarchy, "copy", FALSE);

    test_clear_tmp_dir ();
}

static void
setup_test_suite (void)
{
    g_test_add_func ("/test-copy-one-file/1.0",
                     test_copy_one_file);
    g_test_add_func ("/test-copy-one-empty-directory/1.0",
                     test_copy_one_empty_directory);
    g_test_add_func ("/test-copy-files/1.0",
                     test_copy_files_small);
    g_test_add_func ("/test-copy-files/1.1",
                     test_copy_files_medium);
    g_test_add_func ("/test-copy-files/1.2",
                     test_copy_files_large);
    g_test_add_func ("/test-copy-directories/1.0",
                     test_copy_directories_small);
    g_test_add_func ("/test-copy-directories/1.1",
                     test_copy_directories_medium);
    g_test_add_func ("/test-copy-directories/1.2",
                     test_copy_directories_large);
    g_test_add_func ("/test-copy-hierarchy/1.0",
                     test_copy_full_directory);
    g_test_add_func ("/test-copy-hierarchy/1.1",
                     test_copy_first_hierarchy);
    g_test_add_func ("/test-copy-hierarchy/1.2",
                     test_copy_second_hierarchy);
    g_test_add_func ("/test-copy-hierarchy/1.3",
                     test_copy_third_hierarchy);
    g_test_add_func ("/test-copy-hierarchy/1.4",
                     test_copy_fourth_hierarchy);
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (NautilusFileUndoManager) undo_manager = NULL;
    g_autoptr (NautilusTagManager) tag_manager = NULL;
    int ret;

    undo_manager = nautilus_file_undo_manager_new ();
    tag_manager = nautilus_tag_manager_new_dummy ();
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();

    setup_test_suite ();

    ret = g_test_run ();

    test_clear_tmp_dir ();

    return ret;
}
