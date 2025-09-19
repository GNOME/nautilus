#include <glib.h>

#include <nautilus-directory-private.h>
#include <nautilus-file.h>
#include <nautilus-file-private.h>
#include <nautilus-file-undo-manager.h>
#include <nautilus-file-utilities.h>

#include <test-utilities.h>

static void
test_file_refcount_single_file (void)
{
    g_assert_cmpint (nautilus_directory_number_outstanding (), ==, 0);

    NautilusFile *file = nautilus_file_get_by_uri ("file:///home/");

    g_assert_cmpint (G_OBJECT (file)->ref_count, ==, 1);
    g_assert_cmpint (G_OBJECT (file->details->directory)->ref_count, ==, 1);
    g_assert_cmpint (nautilus_directory_number_outstanding (), ==, 1);

    nautilus_file_unref (file);

    g_assert_cmpint (nautilus_directory_number_outstanding (), ==, 0);
}

static void
test_file_check_name_bland (void)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri ("file:///home");
    const char *name = nautilus_file_get_name (file);
    g_assert_cmpstr (name, ==, "home");
}

static void
test_file_check_name_trailing_slash (void)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri ("file:///home/");
    const char *name = nautilus_file_get_name (file);
    g_assert_cmpstr (name, ==, "home");
}

static void
test_file_duplicate_pointers (void)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri ("file:///home/");

    g_assert_true (nautilus_file_get_by_uri ("file:///home/") == file);
    nautilus_file_unref (file);

    g_assert_true (nautilus_file_get_by_uri ("file:///home") == file);
    nautilus_file_unref (file);
}

static void
test_file_sort_order (void)
{
    g_autoptr (NautilusFile) file_1 = nautilus_file_get_by_uri ("file:///etc");
    g_autoptr (NautilusFile) file_2 = nautilus_file_get_by_uri ("file:///usr");
    NautilusFileSortType sort_type = NAUTILUS_FILE_SORT_BY_DISPLAY_NAME;

    g_assert_cmpint (G_OBJECT (file_1)->ref_count, ==, 1);
    g_assert_cmpint (G_OBJECT (file_2)->ref_count, ==, 1);

    int order = nautilus_file_compare_for_sort (file_1, file_2, sort_type, FALSE, FALSE);
    g_assert_cmpint (order, <, 0);

    int order_reversed = nautilus_file_compare_for_sort (file_1, file_2, sort_type, FALSE, TRUE);
    g_assert_cmpint (order_reversed, >, 0);
}

static void
test_file_sort_with_self (void)
{
    g_autoptr (NautilusFile) file_1 = nautilus_file_get_by_uri ("file:///etc");
    NautilusFileSortType sort_type = NAUTILUS_FILE_SORT_BY_DISPLAY_NAME;
    int order;

    order = nautilus_file_compare_for_sort (file_1, file_1, sort_type, TRUE, TRUE);
    g_assert_cmpint (order, ==, 0);

    order = nautilus_file_compare_for_sort (file_1, file_1, sort_type, TRUE, FALSE);
    g_assert_cmpint (order, ==, 0);

    order = nautilus_file_compare_for_sort (file_1, file_1, sort_type, FALSE, TRUE);
    g_assert_cmpint (order, ==, 0);

    order = nautilus_file_compare_for_sort (file_1, file_1, sort_type, FALSE, FALSE);
    g_assert_cmpint (order, ==, 0);
}

static void
test_file_permissions_can_set (void)
{
    g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (),
                                                            "perm_test_file",
                                                            NULL);
    g_autoptr (GFileOutputStream) stream = g_file_create (location, G_FILE_CREATE_NONE, NULL, NULL);
    gint mode = 0600;

    g_assert_nonnull (stream);
    g_assert_true (g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, NULL));
    g_assert_true (g_file_set_attribute_uint32 (location,
                                                G_FILE_ATTRIBUTE_UNIX_MODE, mode,
                                                G_FILE_QUERY_INFO_NONE, NULL, NULL));


    g_autoptr (NautilusFile) file = nautilus_file_get (location);

    file_load_attributes (file, NAUTILUS_FILE_ATTRIBUTE_INFO);
    g_assert_true (nautilus_file_can_set_permissions (file));

    test_clear_tmp_dir ();
}

typedef struct
{
    NautilusFile *file;
    gboolean done;
} PermissionTestData;

static void
file_changed_set_cb (NautilusFile *file,
                     gboolean     *done)
{
    *done = TRUE;
}

static void
permissions_success_cb (NautilusFile *file,
                        GFile        *result_location,
                        GError       *error,
                        gpointer      user_data)
{
    PermissionTestData *perm_data = user_data;

    g_assert_no_error (error);
    g_assert_true (file == perm_data->file);

    perm_data->done = TRUE;
}

static void
test_file_permissions_set_basic (void)
{
    g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (),
                                                            "perm_test_file",
                                                            NULL);
    g_autoptr (GFileOutputStream) stream = g_file_create (location, G_FILE_CREATE_NONE, NULL, NULL);

    g_assert_nonnull (stream);
    g_assert_true (g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, NULL));

    g_autoptr (NautilusFile) file = nautilus_file_get (location);
    PermissionTestData perm_data = { file, FALSE };
    gboolean file_changed = FALSE;
    guint32 perms = 0600;

    file_load_attributes (file, NAUTILUS_FILE_ATTRIBUTE_INFO);
    g_signal_connect (file, "changed", G_CALLBACK (file_changed_set_cb), &file_changed);

    nautilus_file_set_permissions (file, perms, permissions_success_cb, &perm_data);
    ITER_CONTEXT_WHILE (!perm_data.done);

    g_autoptr (GFileInfo) info = g_file_query_info (location, G_FILE_ATTRIBUTE_UNIX_MODE,
                                                    G_FILE_QUERY_INFO_NONE, NULL, NULL);

    g_assert_nonnull (info);
    g_assert_true (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_MODE));

    guint32 mode = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);

    g_assert_cmphex (mode & 0777, ==, perms);
    g_assert_true (file_changed);

    test_clear_tmp_dir ();
}

static void
test_file_permissions_set_same (void)
{
    g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (),
                                                            "perm_test_file",
                                                            NULL);
    g_autoptr (GFileOutputStream) stream = g_file_create (location, G_FILE_CREATE_NONE, NULL, NULL);

    g_assert_nonnull (stream);
    g_assert_true (g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, NULL));

    g_autoptr (NautilusFile) file = nautilus_file_get (location);
    gboolean file_changed = FALSE;
    g_autoptr (GFileInfo) info_before = g_file_query_info (location, G_FILE_ATTRIBUTE_UNIX_MODE,
                                                           G_FILE_QUERY_INFO_NONE, NULL, NULL);
    guint32 mode_before = g_file_info_get_attribute_uint32 (info_before, G_FILE_ATTRIBUTE_UNIX_MODE);
    PermissionTestData perm_data = { file, FALSE };

    file_load_attributes (file, NAUTILUS_FILE_ATTRIBUTE_INFO);
    g_signal_connect (file, "changed", G_CALLBACK (file_changed_set_cb), &file_changed);

    nautilus_file_set_permissions (file, mode_before, permissions_success_cb, &perm_data);
    ITER_CONTEXT_WHILE (!perm_data.done);

    g_autoptr (GFileInfo) info_after = g_file_query_info (location, G_FILE_ATTRIBUTE_UNIX_MODE,
                                                          G_FILE_QUERY_INFO_NONE, NULL, NULL);

    g_assert_nonnull (info_after);
    g_assert_true (g_file_info_has_attribute (info_after, G_FILE_ATTRIBUTE_UNIX_MODE));

    guint32 mode_after = g_file_info_get_attribute_uint32 (info_after, G_FILE_ATTRIBUTE_UNIX_MODE);

    g_assert_cmphex (mode_after, ==, mode_before);
    g_assert_false (file_changed);

    test_clear_tmp_dir ();
}

static void
test_directory_counts (void)
{
    const GStrv deep_hierarchy = (char *[])
    {
        "deepcount/",
        "deepcount/file1",
        "deepcount/dir1/",
        "deepcount/dir1/file2",
        "deepcount/dir1/dir1a/",
        "deepcount/dir1/dir1a/file3",
        "deepcount/dir2/",
        "deepcount/dir2/file4",
        NULL
    };
    g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (), "deepcount", NULL);
    g_autoptr (NautilusFile) file = nautilus_file_get (location);
    guint dir_count = 0, file_count = 0, unreadable = 0, item_count = 0;
    NautilusRequestStatus status;
    gboolean count_unreadable = TRUE;

    file_hierarchy_create (deep_hierarchy, "");
    file_load_attributes (file,
                          NAUTILUS_FILE_ATTRIBUTE_INFO |
                          NAUTILUS_FILE_ATTRIBUTE_DIRECTORY_ITEM_COUNT |
                          NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS);

    g_assert_true (nautilus_file_get_directory_item_count (file, &item_count, &count_unreadable));
    g_assert_cmpint (item_count, ==, 3);
    g_assert_false (count_unreadable);

    status = nautilus_file_get_deep_counts (file, &dir_count, &file_count, &unreadable, NULL, TRUE);
    g_assert_cmpint (status, ==, NAUTILUS_REQUEST_DONE);
    g_assert_cmpint (dir_count, ==, 3);
    g_assert_cmpint (file_count, ==, 4);
    g_assert_cmpint (unreadable, ==, 0);

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

    g_test_add_func ("/file-refcount/single-file",
                     test_file_refcount_single_file);
    g_test_add_func ("/file-check-name/bland",
                     test_file_check_name_bland);
    g_test_add_func ("/file-check-name/trailing-slash",
                     test_file_check_name_trailing_slash);
    g_test_add_func ("/file-duplicate-pointers/1.0",
                     test_file_duplicate_pointers);
    g_test_add_func ("/file-sort/order",
                     test_file_sort_order);
    g_test_add_func ("/file-sort/with-self",
                     test_file_sort_with_self);
    g_test_add_func ("/file/permissions/can_set",
                     test_file_permissions_can_set);
    g_test_add_func ("/file/permissions/basic",
                     test_file_permissions_set_basic);
    g_test_add_func ("/file/permissions/same",
                     test_file_permissions_set_same);
    g_test_add_func ("/file/deep-counts/basic",
                     test_directory_counts);

    return g_test_run ();
}
