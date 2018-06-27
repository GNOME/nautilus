#include <glib.h>
#include "src/nautilus-directory.h"
#include "src/nautilus-file-utilities.h"
#include "src/nautilus-search-directory.h"
#include "src/nautilus-directory.h"
#include "src/nautilus-file-operations.c"
#include <unistd.h>
#include "eel/eel-string.h"

static void
test_trash_one_file (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;
    GFileOutputStream *out = NULL;
    g_autoptr (GError) error = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }
    files = g_list_prepend (files, g_object_ref (file));

    trash_or_delete_internal_sync (files,
                                   NULL,
                                   TRUE,
                                   NULL,
                                   NULL);

    g_assert_false (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
}

static void
test_trash_more_files_func (gint files_to_trash)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;
    GFileOutputStream *out = NULL;
 
    root = g_file_new_for_path (g_get_tmp_dir ());
 
    for (int i = 0; i < files_to_trash; i++)
    {
        g_autofree gchar *file_name = NULL;
        g_autoptr (GError) error = NULL;
 
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }
 
    trash_or_delete_internal_sync (files,
                                   NULL,
                                   TRUE,
                                   NULL,
                                   NULL);
 
    for (int i = 0; i < files_to_trash; i++)
    {
        g_autofree gchar *file_name = NULL;

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
    }
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
    GFileOutputStream *out = NULL;
    g_autoptr (GError) error = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }
    files = g_list_prepend (files, g_object_ref (file));

    trash_or_delete_internal_sync (files,
                                   NULL,
                                   TRUE,
                                   NULL,
                                   NULL);

    g_assert_false (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
}

static void
test_delete_more_files_func (gint files_to_delete)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;
    GFileOutputStream *out = NULL;
 
    root = g_file_new_for_path (g_get_tmp_dir ());
 
    for (int i = 0; i < files_to_delete; i++)
    {
        g_autofree gchar *file_name = NULL;
        g_autoptr (GError) error = NULL;
 
        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
        if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        {
            g_object_unref (out);
        }
        files = g_list_prepend (files, g_object_ref (file));
    }
 
    trash_or_delete_internal_sync (files,
                                   NULL,
                                   FALSE,
                                   NULL,
                                   NULL);
 
    for (int i = 0; i < files_to_delete; i++)
    {
        g_autofree gchar *file_name = NULL;

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
    }
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
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    files = g_list_prepend (files, g_object_ref (first_dir));

    trash_or_delete_internal_sync (files,
                                   NULL,
                                   TRUE,
                                   NULL,
                                   NULL);

    g_assert_false (g_file_query_exists (first_dir, NULL));
}

static void
test_trash_more_empty_directories_func (gint directories_to_trash)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
 
    root = g_file_new_for_path (g_get_tmp_dir ());
 
    for (int i = 0; i < directories_to_trash; i++)
    {
        g_autofree gchar *file_name = NULL;

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }
 
    trash_or_delete_internal_sync (files,
                                   NULL,
                                   TRUE,
                                   NULL,
                                   NULL);

    for (int i = 0; i < directories_to_trash; i++)
    {
        g_autofree gchar *file_name = NULL;

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_assert_false (g_file_query_exists (file, NULL));
    }
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
    g_autolist (GFile) files = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    files = g_list_prepend (files, g_object_ref (first_dir));

    trash_or_delete_internal_sync (files,
                                   NULL,
                                   FALSE,
                                   NULL,
                                   NULL);

    g_assert_false (g_file_query_exists (first_dir, NULL));
}

static void
test_delete_more_empty_directories_func (gint directories_to_delete)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) dir = NULL;
    g_autolist (GFile) files = NULL;
 
    root = g_file_new_for_path (g_get_tmp_dir ());
 
    for (int i = 0; i < directories_to_delete; i++)
    {
        g_autofree gchar *file_name = NULL;

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_file_make_directory (file, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (file));
    }
 
    trash_or_delete_internal_sync (files,
                                   NULL,
                                   FALSE,
                                   NULL,
                                   NULL);

    for (int i = 0; i < directories_to_delete; i++)
    {
        g_autofree gchar *file_name = NULL;

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (root, file_name);
        g_assert_true (file != NULL);
        g_assert_false (g_file_query_exists (file, NULL));
    }
}

static void
test_delete_more_empty_directories (void)
{
    test_delete_more_empty_directories_func (100);
}

static void
test_trash_one_full_directory (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;
    GFileOutputStream *out = NULL;
    g_autoptr (GError) error = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }

    files = g_list_prepend (files, g_object_ref (first_dir));

    trash_or_delete_internal_sync (files,
                                   NULL,
                                   TRUE,
                                   NULL,
                                   NULL);

    g_assert_false (g_file_query_exists (first_dir, NULL));
    g_assert_false (g_file_query_exists (file, NULL));
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_child
 * We're trashing first_dir.
 */
static void
test_trash_first_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;
 
    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);
 
    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
 
    trash_or_delete_internal_sync (files,
                                   NULL,
                                   TRUE,
                                   NULL,
                                   NULL);

    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_false (g_file_query_exists (first_dir, NULL)); 
}
 
/* The hierarchy looks like this:
 * /tmp/first_dir/first_child
 * /tmp/first_dir/second_child
 * We're trashing first_dir.
 */
static void
test_trash_second_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;
 
    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);
 
    file = g_file_get_child (first_dir, "first_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file = g_file_get_child (first_dir, "second_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    trash_or_delete_internal_sync (files,
                                   NULL,
                                   TRUE,
                                   NULL,
                                   NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "second_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_false (g_file_query_exists (first_dir, NULL));
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
    g_autolist (GFile) files = NULL;
 
    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 50; i++)
    {
        g_autofree gchar *file_name = NULL;

        file_name = g_strdup_printf ("directory_%i", i);

        directory = g_file_get_child (root, file_name);
        g_file_make_directory (directory, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (directory));

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (directory, file_name);
        g_file_make_directory (file, NULL, NULL);
    }
 
    trash_or_delete_internal_sync (files,
                                   NULL,
                                   TRUE,
                                   NULL,
                                   NULL);

    for (int i = 0; i < 50; i++)
    {
        g_autofree gchar *file_name = NULL;

        file_name = g_strdup_printf ("directory_%i", i);

        directory = g_file_get_child (root, file_name);
        g_assert_false (g_file_query_exists (directory, NULL));

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (directory, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
    }
}

static void
test_delete_one_full_directory (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;
    GFileOutputStream *out = NULL;
    g_autoptr (GError) error = NULL;

    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    out = g_file_create (file, G_FILE_CREATE_NONE, NULL, &error);
    if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
    {
        g_object_unref (out);
    }

    files = g_list_prepend (files, g_object_ref (first_dir));

    trash_or_delete_internal_sync (files,
                                   NULL,
                                   FALSE,
                                   NULL,
                                   NULL);

    g_assert_false (g_file_query_exists (first_dir, NULL));
    g_assert_false (g_file_query_exists (file, NULL));
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_child
 * We're deleting first_dir.
 */
static void
test_delete_first_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;
 
    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);
 
    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
 
    trash_or_delete_internal_sync (files,
                                   NULL,
                                   FALSE,
                                   NULL,
                                   NULL);

    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_false (g_file_query_exists (first_dir, NULL)); 
}
 
/* The hierarchy looks like this:
 * /tmp/first_dir/first_child
 * /tmp/first_dir/second_child
 * We're deleting first_dir.
 */
static void
test_delete_second_hierarchy (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;
 
    root = g_file_new_for_path (g_get_tmp_dir ());
    first_dir = g_file_get_child (root, "first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (first_dir != NULL);
    g_file_make_directory (first_dir, NULL, NULL);
 
    file = g_file_get_child (first_dir, "first_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file = g_file_get_child (first_dir, "second_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    trash_or_delete_internal_sync (files,
                                   NULL,
                                   FALSE,
                                   NULL,
                                   NULL);

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));

    file = g_file_get_child (first_dir, "second_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));

    g_assert_false (g_file_query_exists (first_dir, NULL));
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
    g_autolist (GFile) files = NULL;
 
    root = g_file_new_for_path (g_get_tmp_dir ());

    for (int i = 0; i < 50; i++)
    {
        g_autofree gchar *file_name = NULL;

        file_name = g_strdup_printf ("directory_%i", i);

        directory = g_file_get_child (root, file_name);
        g_file_make_directory (directory, NULL, NULL);
        files = g_list_prepend (files, g_object_ref (directory));

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (directory, file_name);
        g_file_make_directory (file, NULL, NULL);
    }
 
    trash_or_delete_internal_sync (files,
                                   NULL,
                                   FALSE,
                                   NULL,
                                   NULL);

    for (int i = 0; i < 50; i++)
    {
        g_autofree gchar *file_name = NULL;

        file_name = g_strdup_printf ("directory_%i", i);

        directory = g_file_get_child (root, file_name);
        g_assert_false (g_file_query_exists (directory, NULL));

        file_name = g_strdup_printf ("file_%i", i);
        file = g_file_get_child (directory, file_name);
        g_assert_false (g_file_query_exists (file, NULL));
    }
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
                     test_trash_one_full_directory);
    g_test_add_func ("/test-trash-one-full-directory/1.1",
                     test_trash_first_hierarchy);
    g_test_add_func ("/test-trash-one-full-directory/1.2",
                     test_trash_second_hierarchy);
    g_test_add_func ("/test-trash-more-full-directories/1.6",
                     test_trash_third_hierarchy);
    g_test_add_func ("/test-delete-one-full-directory/1.0",
                     test_delete_one_full_directory);
    g_test_add_func ("/test-delete-one-full-directory/1.1",
                     test_delete_first_hierarchy);
    g_test_add_func ("/test-delete-one-full-directory/1.2",
                     test_delete_second_hierarchy);
    g_test_add_func ("/test-delete-more-full-directories/1.6",
                     test_delete_third_hierarchy);

}

int
main (int argc, char *argv[])
{
    g_autoptr (NautilusFileUndoManager) undo_manager = NULL;

    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points();
    undo_manager = nautilus_file_undo_manager_new ();

    setup_test_suite ();

    return g_test_run ();
}