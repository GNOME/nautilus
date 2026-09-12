#include "test-utilities.h"

#include <src/nautilus-file-operations.h>
#include <src/nautilus-file-undo-manager.h>
#include <src/nautilus-file-utilities.h>
#include <src/nautilus-progress-info-manager.h>
#include <src/nautilus-tag-manager.h>


typedef struct
{
    gboolean user_cancel;
    GMainLoop *loop;
} DeleteCallbackData;

static void
delete_callback (GHashTable *debuting_uris,
                 gboolean    user_cancel,
                 gpointer    callback_data)
{
    DeleteCallbackData *data = callback_data;

    data->user_cancel = user_cancel;

    g_main_loop_quit (data->loop);
}

static void
delete_callback_data_clear (DeleteCallbackData *data)
{
    g_clear_pointer (&data->loop, g_main_loop_unref);
}

static void
delete_callback_data_init (DeleteCallbackData *data)
{
    data->user_cancel = FALSE;
    data->loop = g_main_loop_new (NULL, FALSE);
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC (DeleteCallbackData, delete_callback_data_clear)

static void
test_trash_one_file (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autolist (GFile) files = NULL;

    create_one_file ("trash_or_delete");

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    first_dir = g_file_get_child (root, "trash_or_delete_first_dir");
    g_assert_true (g_file_query_exists (first_dir, NULL));

    file = g_file_get_child (first_dir, "trash_or_delete_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
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

        g_assert_true (g_file_query_exists (file, NULL));
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

        g_assert_true (g_file_query_exists (file, NULL));
        files = g_list_prepend (files, file);
    }

    nautilus_file_operations_delete_sync (files);
}

static void
verify_multiple_files_deleted (const gchar *prefix,
                               GFile       *src,
                               guint        num,
                               gboolean     deleted)
{
    for (guint i = 0; i < num; i++)
    {
        g_autofree gchar *file_name = g_strdup_printf ("%s_%i", prefix, i);
        g_autoptr (GFile) file = g_file_get_child (src, file_name);

        if (deleted)
        {
            g_assert_false (g_file_query_exists (file, NULL));
        }
        else
        {
            g_assert_true (g_file_query_exists (file, NULL));
        }
    }
}

static void
test_trash_more_files_func (gint files_to_trash)
{
    g_autoptr (GFile) root = NULL;

    create_multiple_files ("trash_or_delete", files_to_trash);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    trash_or_delete_multiple_files ("trash_or_delete_file", root, files_to_trash);

    verify_multiple_files_deleted ("trash_or_delete_file", root, files_to_trash, TRUE);

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
    g_assert_true (g_file_query_exists (root, NULL));

    first_dir = g_file_get_child (root, "delete_first_dir");
    g_assert_true (g_file_query_exists (first_dir, NULL));

    file = g_file_get_child (first_dir, "delete_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));
    files = g_list_prepend (files, g_object_ref (file));

    nautilus_file_operations_delete_sync (files);

    g_assert_false (g_file_query_exists (file, NULL));

    empty_directory_by_prefix (root, "delete");
}

static void
test_delete_more_files_func (gint files_to_delete)
{
    g_autoptr (GFile) root = NULL;

    create_multiple_files ("trash_or_delete", files_to_delete);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    delete_multiple_files ("trash_or_delete_file", root, files_to_delete);

    verify_multiple_files_deleted ("trash_or_delete_file", root, files_to_delete, TRUE);

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
    g_assert_true (g_file_query_exists (root, NULL));

    first_dir = g_file_get_child (root, "trash_or_delete_first_dir");
    g_assert_true (g_file_query_exists (first_dir, NULL));

    file = g_file_get_child (first_dir, "trash_or_delete_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));

    files = g_list_prepend (files, g_object_ref (file));

    nautilus_file_operations_trash_or_delete_sync (files);

    g_assert_false (g_file_query_exists (file, NULL));

    empty_directory_by_prefix (root, "trash_or_delete");
}

static void
test_trash_more_empty_directories_func (gint directories_to_trash)
{
    g_autoptr (GFile) root = NULL;

    create_multiple_directories ("trash_or_delete", directories_to_trash);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    trash_or_delete_multiple_files ("trash_or_delete_dir", root, directories_to_trash);

    verify_multiple_files_deleted ("trash_or_delete_dir", root, directories_to_trash, TRUE);

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
    g_assert_true (g_file_query_exists (root, NULL));

    first_dir = g_file_get_child (root, "delete_first_dir");
    g_assert_true (g_file_query_exists (first_dir, NULL));
    file = g_file_get_child (first_dir, "delete_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));

    files = g_list_prepend (files, g_object_ref (file));

    nautilus_file_operations_delete_sync (files);

    g_assert_false (g_file_query_exists (file, NULL));

    empty_directory_by_prefix (root, "delete");
}

static void
test_delete_more_empty_directories_func (gint directories_to_delete)
{
    g_autoptr (GFile) root = NULL;

    create_multiple_directories ("trash_or_delete", directories_to_delete);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    delete_multiple_files ("trash_or_delete_dir", root, directories_to_delete);

    verify_multiple_files_deleted ("trash_or_delete_dir", root, directories_to_delete, TRUE);

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
    g_assert_true (g_file_query_exists (root, NULL));

    first_dir = g_file_get_child (root, "trash_or_delete_first_dir");
    g_assert_true (g_file_query_exists (first_dir, NULL));

    file = g_file_get_child (first_dir, "trash_or_delete_first_dir_child");

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
    g_autolist (GFile) files = NULL;
    GFile *file;

    create_first_hierarchy ("trash_or_delete");

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    first_dir = g_file_get_child (root, "trash_or_delete_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (g_file_query_exists (first_dir, NULL));

    nautilus_file_operations_trash_or_delete_sync (files);

    file = g_file_get_child (first_dir, "trash_or_delete_first_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));
    g_clear_object (&file);

    file = g_file_get_child (first_dir, "trash_or_delete_second_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));
    g_clear_object (&file);

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

    create_multiple_full_directories ("trash_or_delete", 50);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    trash_or_delete_multiple_files ("trash_or_delete_directory", root, 50);

    verify_multiple_files_deleted ("trash_or_delete_directory", root, 50, TRUE);

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
    g_assert_true (g_file_query_exists (root, NULL));

    first_dir = g_file_get_child (root, "delete_first_dir");
    g_assert_true (g_file_query_exists (first_dir, NULL));

    file = g_file_get_child (first_dir, "delete_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));

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
    g_assert_true (g_file_query_exists (root, NULL));

    first_dir = g_file_get_child (root, "delete_first_dir");
    files = g_list_prepend (files, g_object_ref (first_dir));
    g_assert_true (g_file_query_exists (first_dir, NULL));

    nautilus_file_operations_delete_sync (files);

    file = g_file_get_child (first_dir, "delete_first_dir_child");
    g_assert_false (g_file_query_exists (file, NULL));
    g_clear_object (&file);

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

    create_multiple_full_directories ("trash_or_delete", 50);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    delete_multiple_files ("trash_or_delete_directory", root, 50);

    verify_multiple_files_deleted ("trash_or_delete_directory", root, 50, TRUE);

    empty_directory_by_prefix (root, "trash_or_delete");
}

/* Deleting an entry always requires write permission on its parent directory.
 * Removing that permission from the parent makes the deletion fail. */
#define DIRECTORY_PERMISSIONS 0755
#define DIRECTORY_PERMISSIONS_READ_ONLY 0555

static void
set_permissions (GFile   *file,
                 guint32  permissions)
{
    g_assert_true (g_file_set_attribute_uint32 (file,
                                                G_FILE_ATTRIBUTE_UNIX_MODE,
                                                permissions,
                                                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                NULL, NULL));
}

static void
test_delete_file_no_permission (void)
{
    g_autoptr (GFile) root = g_file_new_for_path (test_get_tmp_dir ());
    g_autoptr (GFile) first_dir = g_file_get_child (root, "delete_first_dir");
    g_autoptr (GFile) file = g_file_get_child (first_dir, "delete_first_dir_child");
    g_auto (DeleteCallbackData) data = { 0 };

    create_one_file ("delete");

    g_assert_true (g_file_query_exists (file, NULL));

    set_permissions (first_dir, DIRECTORY_PERMISSIONS_READ_ONLY);
    delete_callback_data_init (&data);

    nautilus_file_operations_delete_async (&(GList){ .data = file },
                                           NULL,
                                           NULL,
                                           delete_callback,
                                           &data);
    g_main_loop_run (data.loop);

    /* It is considered user cancellation due to test dialog auto response. */
    g_assert_true (data.user_cancel);
    g_assert_true (g_file_query_exists (file, NULL));

    set_permissions (first_dir, DIRECTORY_PERMISSIONS);
    empty_directory_by_prefix (root, "delete");
}

static void
test_delete_directory_no_permission (void)
{
    g_autoptr (GFile) root = g_file_new_for_path (test_get_tmp_dir ());
    g_autoptr (GFile) first_dir = g_file_get_child (root, "delete_first_dir");
    g_autoptr (GFile) enclosed_dir = g_file_get_child (first_dir, "delete_first_dir_child");
    g_auto (DeleteCallbackData) data = { 0 };

    create_one_empty_directory ("delete");

    g_assert_true (g_file_query_exists (enclosed_dir, NULL));

    set_permissions (first_dir, DIRECTORY_PERMISSIONS_READ_ONLY);
    delete_callback_data_init (&data);

    nautilus_file_operations_delete_async (&(GList){ .data = enclosed_dir },
                                           NULL,
                                           NULL,
                                           delete_callback,
                                           &data);
    g_main_loop_run (data.loop);

    /* It is considered user cancellation due to test dialog auto response. */
    g_assert_true (data.user_cancel);
    g_assert_true (g_file_query_exists (enclosed_dir, NULL));

    set_permissions (first_dir, DIRECTORY_PERMISSIONS);
    empty_directory_by_prefix (root, "delete");
}

static void
test_delete_enclosed_file_no_permission (void)
{
    g_autoptr (GFile) root = g_file_new_for_path (test_get_tmp_dir ());
    g_autoptr (GFile) first_dir = g_file_get_child (root, "delete_first_dir");
    g_autoptr (GFile) file = g_file_get_child (first_dir, "delete_first_dir_child");
    g_auto (DeleteCallbackData) data = { 0 };

    create_one_file ("delete");

    g_assert_true (g_file_query_exists (file, NULL));

    set_permissions (first_dir, DIRECTORY_PERMISSIONS_READ_ONLY);
    delete_callback_data_init (&data);

    nautilus_file_operations_delete_async (&(GList){ .data = first_dir },
                                           NULL,
                                           NULL,
                                           delete_callback,
                                           &data);
    g_main_loop_run (data.loop);

    /* It is considered user cancellation due to test dialog auto response. */
    g_assert_true (data.user_cancel);
    g_assert_true (g_file_query_exists (first_dir, NULL));
    g_assert_true (g_file_query_exists (file, NULL));

    set_permissions (first_dir, DIRECTORY_PERMISSIONS);
    empty_directory_by_prefix (root, "delete");
}

static void
test_delete_enclosed_directory_no_permission (void)
{
    g_autoptr (GFile) root = g_file_new_for_path (test_get_tmp_dir ());
    g_autoptr (GFile) first_dir = g_file_get_child (root, "delete_first_dir");
    g_autoptr (GFile) enclosed_dir = g_file_get_child (first_dir, "delete_first_dir_child");
    g_auto (DeleteCallbackData) data = { 0 };

    create_one_empty_directory ("delete");

    g_assert_true (g_file_query_exists (enclosed_dir, NULL));

    set_permissions (first_dir, DIRECTORY_PERMISSIONS_READ_ONLY);
    delete_callback_data_init (&data);

    nautilus_file_operations_delete_async (&(GList){ .data = first_dir },
                                           NULL,
                                           NULL,
                                           delete_callback,
                                           &data);
    g_main_loop_run (data.loop);

    /* It is considered user cancellation due to test dialog auto response. */
    g_assert_true (data.user_cancel);
    g_assert_true (g_file_query_exists (first_dir, NULL));
    g_assert_true (g_file_query_exists (enclosed_dir, NULL));

    set_permissions (first_dir, DIRECTORY_PERMISSIONS);
    empty_directory_by_prefix (root, "delete");
}

static void
test_delete_one_file_cancel (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_auto (DeleteCallbackData) data = { 0 };

    create_one_file ("delete");

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    first_dir = g_file_get_child (root, "delete_first_dir");
    g_assert_true (g_file_query_exists (first_dir, NULL));

    file = g_file_get_child (first_dir, "delete_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));

    delete_callback_data_init (&data);

    nautilus_file_operations_delete_async (&(GList){ .data = file },
                                           NULL,
                                           NULL,
                                           delete_callback,
                                           &data);

    test_operation_cancel ();

    g_main_loop_run (data.loop);

    /* Can't assert anything since deletion is racy */

    empty_directory_by_prefix (root, "delete");
}

static void
test_delete_more_files_cancel (void)
{
    const guint files_to_delete = 100;
    g_autoptr (GFile) root = NULL;
    g_autolist (GFile) files = NULL;
    g_auto (DeleteCallbackData) data = { 0 };

    create_multiple_files ("trash_or_delete", files_to_delete);

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    for (guint i = 0; i < files_to_delete; i++)
    {
        g_autofree gchar *file_name = g_strdup_printf ("trash_or_delete_file_%i", i);
        GFile *file = g_file_get_child (root, file_name);

        g_assert_true (g_file_query_exists (file, NULL));
        files = g_list_prepend (files, file);
    }

    delete_callback_data_init (&data);

    nautilus_file_operations_delete_async (files,
                                           NULL,
                                           NULL,
                                           delete_callback,
                                           &data);

    test_operation_cancel ();

    g_main_loop_run (data.loop);

    /* Can't assert anything since deletion is racy */

    empty_directory_by_prefix (root, "trash_or_delete");
}

static void
test_delete_full_directory_cancel (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_auto (DeleteCallbackData) data = { 0 };

    create_one_file ("delete");

    root = g_file_new_for_path (test_get_tmp_dir ());
    g_assert_true (g_file_query_exists (root, NULL));

    first_dir = g_file_get_child (root, "delete_first_dir");
    g_assert_true (g_file_query_exists (first_dir, NULL));

    file = g_file_get_child (first_dir, "delete_first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));

    delete_callback_data_init (&data);

    nautilus_file_operations_delete_async (&(GList){ .data = first_dir },
                                           NULL,
                                           NULL,
                                           delete_callback,
                                           &data);

    test_operation_cancel ();

    g_main_loop_run (data.loop);

    /* Can't assert anything since deletion is racy */

    empty_directory_by_prefix (root, "delete");
}

static void
setup_test_suite (void)
{
    g_test_add_func ("/trash/one-file/1.0",
                     test_trash_one_file);
    g_test_add_func ("/trash/more-files/1.0",
                     test_trash_more_files);
    g_test_add_func ("/trash/one-empty-directory/1.0",
                     test_trash_one_empty_directory);
    g_test_add_func ("/trash/more-empty-directories/1.0",
                     test_trash_more_empty_directories);
    g_test_add_func ("/trash/one-full-directory/1.0",
                     test_trash_full_directory);
    g_test_add_func ("/trash/one-full-directory/1.1",
                     test_trash_first_hierarchy);
    g_test_add_func ("/trash/more-full-directories/1.2",
                     test_trash_third_hierarchy);

    g_test_add_func ("/delete/one-file/1.0",
                     test_delete_one_file);
    g_test_add_func ("/delete/more-files/1.0",
                     test_delete_more_files);
    g_test_add_func ("/delete/one-empty-directory/1.0",
                     test_delete_one_empty_directory);
    g_test_add_func ("/delete/more-directories/1.0",
                     test_delete_more_empty_directories);
    g_test_add_func ("/delete/one-full-directory/1.0",
                     test_delete_full_directory);
    g_test_add_func ("/delete/one-full-directory/1.1",
                     test_delete_first_hierarchy);
    g_test_add_func ("/delete/more-full-directories/1.6",
                     test_delete_third_hierarchy);

    g_test_add_func ("/delete/one-file/error/no-permission",
                     test_delete_file_no_permission);
    g_test_add_func ("/delete/one-empty-directory/error/no-permission",
                     test_delete_directory_no_permission);
    g_test_add_func ("/delete/one-full-directory/error/no-permission/enclosed-file",
                     test_delete_enclosed_file_no_permission);
    g_test_add_func ("/delete/one-full-directory/error/no-permission/enclosed-directory",
                     test_delete_enclosed_directory_no_permission);

    g_test_add_func ("/delete/one-file/cancel",
                     test_delete_one_file_cancel);
    g_test_add_func ("/delete/more-files/cancel",
                     test_delete_more_files_cancel);
    g_test_add_func ("/delete/one-full-directory/cancel",
                     test_delete_full_directory_cancel);
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (NautilusProgressInfoManager) progress_manager = NULL;
    g_autoptr (NautilusFileUndoManager) undo_manager = NULL;
    g_autoptr (NautilusTagManager) tag_manager = NULL;
    int ret;

    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();
    progress_manager = nautilus_progress_info_manager_dup_singleton ();
    undo_manager = nautilus_file_undo_manager_new ();
    tag_manager = nautilus_tag_manager_new_dummy ();

    setup_test_suite ();

    ret = g_test_run ();

    test_clear_tmp_dir ();

    return ret;
}
