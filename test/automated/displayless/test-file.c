#include <glib.h>

#include "test-utilities.h"

#include <nautilus-directory-private.h>
#include <nautilus-file.h>
#include <nautilus-file-private.h>
#include <nautilus-file-undo-manager.h>
#include <nautilus-file-utilities.h>
#include <nautilus-tag-manager.h>


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

static gboolean rename_test_done = FALSE;
static guint rename_test_iteration = 0;

static void
check_real_file (NautilusFile *file,
                 const char   *name)
{
    g_autoptr (GFile) location = nautilus_file_get_location (file);
    g_autoptr (GFileInfo) info = g_file_query_info (location, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, 0, NULL, NULL);
    g_assert_cmpstr (g_file_info_get_display_name (info), ==, name);
}

static void
rename_bad_root_done (NautilusFile *file,
                      GFile        *not_used,
                      GError       *error,
                      gpointer      user_data)
{
    g_assert_false (nautilus_file_is_gone (file));
    g_assert_nonnull (error);
    g_assert_cmpint (error->domain, ==, G_IO_ERROR);
    g_assert_cmpint (error->code, ==, G_IO_ERROR_NOT_SUPPORTED);

    rename_test_done = TRUE;
}

static void
rename_done (NautilusFile *file,
             GFile        *location,
             GError       *error,
             gpointer      user_data)
{
    g_assert_false (nautilus_file_is_gone (file));
    check_real_file (file, "new_name");
    g_assert_cmpstr ("new_name", ==, nautilus_file_get_name (file));
    g_assert_null (error);

    g_autoptr (NautilusFile) root = nautilus_file_get_by_uri ("file:///");
    nautilus_file_rename (root, "anything", rename_bad_root_done, NULL);
}

static void
rename_bad_done (NautilusFile *file,
                 GFile        *not_used,
                 GError       *error,
                 gpointer      user_data)
{
    rename_test_iteration++;

    g_assert_nonnull (error);
    g_assert_false (nautilus_file_is_gone (file));
    g_assert_cmpstr (nautilus_file_get_name (file), ==, "rename_first_dir_child");
    check_real_file (file, "rename_first_dir_child");

    if (rename_test_iteration == 1)
    {
        nautilus_file_rename (file, "foo/bar", rename_bad_done, NULL);
    }
    else if (rename_test_iteration == 2)
    {
        nautilus_file_rename (file, "new_name", rename_done, NULL);
    }
}

static void
directory_ready (NautilusDirectory *directory,
                 GList             *files,
                 gpointer           user_data)
{
    nautilus_file_rename (user_data, "/", rename_bad_done, NULL);
}

static void
test_file_rename (void)
{
    g_autofree char *path = g_build_filename (test_get_tmp_dir (),
                                              "rename_first_dir",
                                              "rename_first_dir_child", NULL);
    create_one_file ("rename");
    g_autoptr (GFile) location = g_file_new_for_path (path);
    g_autoptr (NautilusFile) file = nautilus_file_get (location);
    NautilusDirectory *directory = nautilus_file_get_directory (file);

    /* Adding the monitor tests specific issues where the monitor causes a
     * rename to mark a NautilusFile as gone */
    nautilus_directory_file_monitor_add (directory, &directory, TRUE,
                                         NAUTILUS_FILE_ATTRIBUTE_INFO, NULL, NULL);

    nautilus_directory_call_when_ready (directory, NAUTILUS_FILE_ATTRIBUTE_INFO, TRUE, directory_ready, file);

    while (!rename_test_done)
    {
        g_main_context_iteration (NULL, TRUE);
    }

    g_assert_false (nautilus_file_is_gone (file));

    nautilus_directory_file_monitor_remove (directory, &directory);
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

int
main (int   argc,
      char *argv[])
{
    G_GNUC_UNUSED NautilusFileUndoManager *manager = nautilus_file_undo_manager_new ();
    G_GNUC_UNUSED NautilusTagManager *tag_manager = nautilus_tag_manager_new_dummy ();
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();

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
    g_test_add_func ("/file/rename",
                     test_file_rename);

    return g_test_run ();
}
