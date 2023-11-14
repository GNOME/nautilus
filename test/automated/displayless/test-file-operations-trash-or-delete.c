#include "test-utilities.h"
#include <src/nautilus-tag-manager.h>

static void
test_trash_one_file (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;

    create_one_file ("trash_or_delete");

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "trash_or_delete_first_dir");
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "trash_or_delete_first_dir_child");
    g_assert_true (file != NULL);
    files = g_list_prepend (files, g_object_ref (file));

    nautilus_file_operations_trash_or_delete_sync (files);

    g_assert_false (g_file_query_exists (file, NULL));

    empty_directory_by_prefix (root, "trash_or_delete");
}

static void
trash_or_delete_multiple_files (const gchar *prefix,
                                GFile       *src,
                                guint        num)
{
    g_autolist (GFile) files = NULL;

    for (guint i = 0; i < num; i++)
    {
        g_autofree gchar *file_name = g_strdup_printf ("%s_%i", prefix, i);
        GFile *file = g_file_get_child (src, file_name);

        g_assert_true (file != NULL);
        files = g_list_prepend (files, file);
    }

    nautilus_file_operations_trash_or_delete_sync (files);
}

static void
delete_multiple_files (const gchar *prefix,
                       GFile       *src,
                       guint        num)
{
    g_autolist (GFile) files = NULL;

    for (guint i = 0; i < num; i++)
    {
        g_autofree gchar *file_name = g_strdup_printf ("%s_%i", prefix, i);
        GFile *file = g_file_get_child (src, file_name);

        g_assert_true (file != NULL);
        files = g_list_prepend (files, file);
    }

    nautilus_file_operations_delete_sync (files);
}

static void
test_trash_more_files_func (gint files_to_trash)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;

    create_multiple_files ("trash_or_delete", files_to_trash);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (root != NULL);

    trash_or_delete_multiple_files ("trash_or_delete_file", root, files_to_trash);

    for (int i = 0; i < files_to_trash; i++)
    {
        gchar *file_name;

        file_name = g_strdup_printf ("trash_or_delete_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_free (file_name);
        g_assert_false (g_file_query_exists (file, NULL));
    }

    empty_directory_by_prefix (root, "trash_or_delete");
}

static void
test_trash_more_files (void)
{
    test_trash_more_files_func (100);
}

static void
test_delete_one_file (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;

    create_one_file ("delete");

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "delete_first_dir");
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "delete_first_dir_child");
    g_assert_true (file != NULL);
    files = g_list_prepend (files, g_object_ref (file));

    nautilus_file_operations_delete_sync (files);

    g_assert_false (g_file_query_exists (file, NULL));

    empty_directory_by_prefix (root, "delete");
}

static void
test_delete_more_files_func (gint files_to_delete)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;

    create_multiple_files ("trash_or_delete", files_to_delete);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (root != NULL);

    delete_multiple_files ("trash_or_delete_file", root, files_to_delete);

    for (int i = 0; i < files_to_delete; i++)
    {
        gchar *file_name;

        file_name = g_strdup_printf ("trash_or_delete_file_%i", i);
        file = g_file_get_child (root, file_name);
        g_free (file_name);
        g_assert_false (g_file_query_exists (file, NULL));
    }

    empty_directory_by_prefix (root, "trash_or_delete");
}

static void
test_delete_more_files (void)
{
    test_delete_more_files_func (100);
}

static void
test_trash_one_empty_directory (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;

    create_one_empty_directory ("trash_or_delete");

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "trash_or_delete_first_dir");
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "trash_or_delete_first_dir_child");
    g_assert_true (file != NULL);

    files = g_list_prepend (files, g_object_ref (file));

    nautilus_file_operations_trash_or_delete_sync (files);

    g_assert_false (g_file_query_exists (file, NULL));

    empty_directory_by_prefix (root, "trash_or_delete");
}

static void
test_trash_more_empty_directories_func (gint directories_to_trash)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;

    create_multiple_directories ("trash_or_delete", directories_to_trash);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (root != NULL);

    trash_or_delete_multiple_files ("trash_or_delete_dir", root, directories_to_trash);

    for (int i = 0; i < directories_to_trash; i++)
    {
        gchar *file_name;

        file_name = g_strdup_printf ("trash_or_delete_dir_%i", i);
        file = g_file_get_child (root, file_name);
        g_free (file_name);
        g_assert_true (file != NULL);
        g_assert_false (g_file_query_exists (file, NULL));
    }

    empty_directory_by_prefix (root, "trash_or_delete");
}

static void
test_trash_more_empty_directories (void)
{
    test_trash_more_empty_directories_func (100);
}

static void
test_delete_one_empty_directory (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;

    create_one_empty_directory ("delete");

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "delete_first_dir");
    g_assert_true (first_dir != NULL);
    file = g_file_get_child (first_dir, "delete_first_dir_child");
    g_assert_true (file != NULL);

    files = g_list_prepend (files, g_object_ref (file));

    nautilus_file_operations_delete_sync (files);

    g_assert_false (g_file_query_exists (file, NULL));

    empty_directory_by_prefix (root, "delete");
}

static void
test_delete_more_empty_directories_func (gint directories_to_delete)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;

    create_multiple_directories ("trash_or_delete", directories_to_delete);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (root != NULL);

    delete_multiple_files ("trash_or_delete_dir", root, directories_to_delete);


    for (int i = 0; i < directories_to_delete; i++)
    {
        gchar *file_name;

        file_name = g_strdup_printf ("trash_or_delete_dir_%i", i);
        file = g_file_get_child (root, file_name);
        g_free (file_name);
        g_assert_true (file != NULL);
        g_assert_false (g_file_query_exists (file, NULL));
    }

    empty_directory_by_prefix (root, "trash_or_delete");
}

static void
test_delete_more_empty_directories (void)
{
    test_delete_more_empty_directories_func (100);
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_child
 * We're trashing first_dir.
 */
static void
test_trash_full_directory (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;

    create_one_file ("trash_or_delete");

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "trash_or_delete_first_dir");
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "trash_or_delete_first_dir_child");
    g_assert_true (file != NULL);

    files = g_list_prepend (files, g_object_ref (first_dir));

    nautilus_file_operations_trash_or_delete_sync (files);

    g_assert_false (g_file_query_exists (first_dir, NULL));
    g_assert_false (g_file_query_exists (file, NULL));

    empty_directory_by_prefix (root, "trash_or_delete");
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_child
 * /tmp/first_dir/second_child
 * We're trashing first_dir.
 */
static void
test_trash_first_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;

    create_first_hierarchy ("trash_or_delete");

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "trash_or_delete_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "trash_or_delete_first_child");
    g_assert_true (file != NULL);
    file = g_file_get_child (first_dir, "trash_or_delete_second_child");
    g_assert_true (file != NULL);

    nautilus_file_operations_trash_or_delete_sync (files);

    file = g_file_get_child (first_dir, "trash_or_delete_first_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "trash_or_delete_second_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_false (g_file_query_exists (first_dir, NULL));

    empty_directory_by_prefix (root, "trash_or_delete");
}

/* We're creating 50 directories each containing one file
 * and trashing all of the directories.
 */
static void
test_trash_third_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) directory = NULL;
    g_autoptr (GFile) file = NULL;

    create_multiple_full_directories ("trash_or_delete", 50);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (root != NULL);

    trash_or_delete_multiple_files ("trash_or_delete_directory", root, 50);

    for (int i = 0; i < 50; i++)
    {
        gchar *file_name;

        file_name = g_strdup_printf ("trash_or_delete_directory_%i", i);

        directory = g_file_get_child (root, file_name);
        g_free (file_name);
        g_assert_false (g_file_query_exists (directory, NULL));
    }

    empty_directory_by_prefix (root, "trash_or_delete");
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_child
 * We're deleting first_dir.
 */
static void
test_delete_full_directory (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;

    create_one_file ("delete");

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "delete_first_dir");
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "delete_first_dir_child");
    g_assert_true (file != NULL);

    files = g_list_prepend (files, g_object_ref (first_dir));

    nautilus_file_operations_delete_sync (files);

    g_assert_false (g_file_query_exists (first_dir, NULL));
    g_assert_false (g_file_query_exists (file, NULL));

    empty_directory_by_prefix (root, "delete");
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_child
 * /tmp/first_dir/second_child
 * We're deleting first_dir.
 */
static void
test_delete_first_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;

    create_first_hierarchy ("delete");

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (root != NULL);

    first_dir = g_file_get_child (root, "delete_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);

    file = g_file_get_child (first_dir, "delete_first_child");
    g_assert_true (file != NULL);
    file = g_file_get_child (first_dir, "delete_second_child");
    g_assert_true (file != NULL);

    nautilus_file_operations_delete_sync (files);

    file = g_file_get_child (first_dir, "delete_first_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "delete_second_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_false (g_file_query_exists (first_dir, NULL));

    empty_directory_by_prefix (root, "delete");
}

/* We're creating 50 directories each containing one file
 * and deleting all of the directories.
 */
static void
test_delete_third_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) directory = NULL;
    g_autoptr (GFile) file = NULL;

    create_multiple_full_directories ("trash_or_delete", 50);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (root != NULL);

    delete_multiple_files ("trash_or_delete_directory", root, 50);

    for (int i = 0; i < 50; i++)
    {
        gchar *file_name;

        file_name = g_strdup_printf ("trash_or_delete_directory_%i", i);

        directory = g_file_get_child (root, file_name);
        g_free (file_name);
        g_assert_false (g_file_query_exists (directory, NULL));
    }

    empty_directory_by_prefix (root, "trash_or_delete");
}

static void
setup_test_suite (void)
{
    g_test_add_func ("/test-trash-one-file/1.0",
                     test_trash_one_file);
    g_test_add_func ("/test-trash-more-files/1.0",
                     test_trash_more_files);
    g_test_add_func ("/test-delete-one-file/1.0",
                     test_delete_one_file);
    g_test_add_func ("/test-delete-more-files/1.0",
                     test_delete_more_files);
    g_test_add_func ("/test-trash-one-empty-directory/1.0",
                     test_trash_one_empty_directory);
    g_test_add_func ("/test-trash-more-empty-directories/1.0",
                     test_trash_more_empty_directories);
    g_test_add_func ("/test-delete-one-empty-directory/1.0",
                     test_delete_one_empty_directory);
    g_test_add_func ("/test-delete-more-directories/1.0",
                     test_delete_more_empty_directories);
    g_test_add_func ("/test-trash-one-full-directory/1.0",
                     test_trash_full_directory);
    g_test_add_func ("/test-trash-one-full-directory/1.1",
                     test_trash_first_hierarchy);
    g_test_add_func ("/test-trash-more-full-directories/1.2",
                     test_trash_third_hierarchy);
    g_test_add_func ("/test-delete-one-full-directory/1.0",
                     test_delete_full_directory);
    g_test_add_func ("/test-delete-one-full-directory/1.1",
                     test_delete_first_hierarchy);
    g_test_add_func ("/test-delete-more-full-directories/1.6",
                     test_delete_third_hierarchy);
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (NautilusFileUndoManager) undo_manager = NULL;
    g_autoptr (NautilusTagManager) tag_manager = NULL;
    int ret;

    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();
    undo_manager = nautilus_file_undo_manager_new ();
    tag_manager = nautilus_tag_manager_new_dummy ();

    setup_test_suite ();

    ret = g_test_run ();

    test_clear_tmp_dir ();

    return ret;
}
