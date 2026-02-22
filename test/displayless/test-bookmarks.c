#define G_LOG_DOMAIN "test-bookmarks"

#include <nautilus-bookmark-list.h>
#include <nautilus-bookmark.h>
#include <nautilus-file-utilities.h>
#include <nautilus-file.h>

#include <test-utilities.h>

#include <glib.h>

static void
test_bookmark (void)
{
    g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (),
                                                            "my_bookmark",
                                                            NULL);
    g_autoptr (NautilusBookmark) bookmark = nautilus_bookmark_new (location, NULL);
    GFile *bookmark_location = nautilus_bookmark_get_location (bookmark);
    g_autoptr (NautilusFile) file = nautilus_file_get (location);
    const gchar *custom_name = "My Custom Bookmark";
    g_autoptr (GIcon) icon = nautilus_bookmark_get_icon (bookmark);
    g_autoptr (GIcon) symbolic_icon = nautilus_bookmark_get_symbolic_icon (bookmark);

    g_assert_true (g_file_equal (location, bookmark_location));
    g_assert_cmpstr (nautilus_file_get_display_name (file),
                     ==,
                     nautilus_bookmark_get_name (bookmark));

    nautilus_bookmark_set_name (bookmark, custom_name);
    g_assert_cmpstr (custom_name, ==, nautilus_bookmark_get_name (bookmark));

    g_assert_nonnull (icon);
    g_assert_nonnull (symbolic_icon);
}

static void
setup_tmp_bookmarks_file (const char *tmp_dir)
{
    g_autofree char *bookmark_list_folder = g_build_filename (tmp_dir,
                                                              "gtk-3.0",
                                                              NULL);
    g_autoptr (GFile) bookmarks_list_file = g_file_new_build_filename (bookmark_list_folder,
                                                                       "bookmarks",
                                                                       NULL);
    g_autoptr (GError) error = NULL;

    g_setenv ("XDG_CONFIG_HOME", tmp_dir, TRUE);
    g_mkdir_with_parents (bookmark_list_folder, 0700);
    g_file_replace_contents (bookmarks_list_file,
                             "", 0,
                             NULL,
                             FALSE,
                             G_FILE_CREATE_NONE,
                             NULL,
                             NULL, &error);

    g_assert_no_error (error);
}

static void
test_bookmark_list_basic (void)
{
    const char *tmp_dir = test_get_tmp_dir ();
    g_autoptr (GFile) bookmarks_list_file =
        bookmarks_list_file = g_file_new_build_filename (tmp_dir,
                                                         "gtk-3.0",
                                                         "bookmarks",
                                                         NULL);
    g_autoptr (NautilusBookmarkList) list = NULL;
    g_autoptr (GFile) bookmark1 = g_file_new_build_filename (tmp_dir, "one", NULL);
    g_autoptr (GFile) bookmark2 = g_file_new_build_filename (tmp_dir, "two", NULL);

    setup_tmp_bookmarks_file (tmp_dir);
    list = nautilus_bookmark_list_new ();
    g_assert_nonnull (list);
    g_assert_false (nautilus_bookmark_list_contains (list, bookmark1));
    g_assert_false (nautilus_bookmark_list_contains (list, bookmark2));

    /* Check insertion */
    nautilus_bookmark_list_add (list, bookmark1, -1);
    g_assert_true (nautilus_bookmark_list_contains (list, bookmark1));

    /* Check location */
    NautilusBookmark *bm = nautilus_bookmark_list_get_bookmark (list, bookmark1);
    g_assert_nonnull (bm);
    g_assert_true (g_file_equal (nautilus_bookmark_get_location (bm), bookmark1));

    /* Check insertion position and list length */
    nautilus_bookmark_list_add (list, bookmark2, 0);
    g_assert_true (nautilus_bookmark_list_contains (list, bookmark2));

    GList *bookmark_list = nautilus_bookmark_list_get_all (list);

    g_assert_nonnull (bookmark_list);
    g_assert_cmpint (g_list_length (bookmark_list), ==, 2);

    /* Check order: bookmark2 at 0, bookmark1 at 1 */
    NautilusBookmark *first = NAUTILUS_BOOKMARK (g_list_nth (bookmark_list, 0)->data);
    NautilusBookmark *second = NAUTILUS_BOOKMARK (g_list_nth (bookmark_list, 1)->data);

    g_assert_true (g_file_equal (nautilus_bookmark_get_location (first), bookmark2));
    g_assert_true (g_file_equal (nautilus_bookmark_get_location (second), bookmark1));

    /* Check moving index */
    nautilus_bookmark_list_move_item (list, bookmark1, 0);

    bookmark_list = nautilus_bookmark_list_get_all (list);
    g_assert_cmpint (g_list_length (bookmark_list), ==, 2);
    first = NAUTILUS_BOOKMARK (bookmark_list->data);
    g_assert_true (g_file_equal (nautilus_bookmark_get_location (first), bookmark1));

    /* Check Removal */
    nautilus_bookmark_list_remove (list, bookmark1);
    g_assert_false (nautilus_bookmark_list_contains (list, bookmark1));
    bookmark_list = nautilus_bookmark_list_get_all (list);
    g_assert_cmpint (g_list_length (bookmark_list), ==, 1);
    g_assert_true (g_file_equal (nautilus_bookmark_get_location (bookmark_list->data), bookmark2));

    test_clear_tmp_dir ();
}

typedef struct
{
    uint signal_count;
    uint expected_signal_count;
} ChangedSignalTestData;

static void
bookmark_list_changed_cb (NautilusBookmarkList  *list,
                          ChangedSignalTestData *data)
{
    data->signal_count++;
    g_assert_cmpint (data->signal_count, ==, data->expected_signal_count);
}

static gboolean
file_has_string (GFile *file,
                 char  *string)
{
    g_autoptr (GError) error = NULL;
    g_autofree char *content = NULL;
    gsize length;

    if (!g_file_load_contents (file, NULL, &content, &length, NULL, &error))
    {
        g_warning ("Failed to load file contents: %s", error->message);

        return FALSE;
    }

    return g_strstr_len (content, length, string) != NULL;
}

static void
test_bookmark_list_changed_signal (void)
{
    const char *tmp_dir = test_get_tmp_dir ();
    g_autoptr (GFile) bookmarks_list_file = g_file_new_build_filename (tmp_dir,
                                                                       "gtk-3.0",
                                                                       "bookmarks",
                                                                       NULL);
    g_autoptr (NautilusBookmarkList) list = NULL;
    g_autoptr (GFile) bookmark1 = g_file_new_build_filename (test_get_tmp_dir (),
                                                             "one",
                                                             NULL);
    g_autoptr (GFile) bookmark2 = g_file_new_build_filename (test_get_tmp_dir (),
                                                             "two",
                                                             NULL);
    ChangedSignalTestData test_data = { 0, 0 };
    NautilusBookmark *existing_bookmark;

    setup_tmp_bookmarks_file (tmp_dir);
    list = nautilus_bookmark_list_new ();
    g_signal_connect (list, "changed", G_CALLBACK (bookmark_list_changed_cb), &test_data);

    /* Wait for the list to be loaded */
    test_data.expected_signal_count++;
    ITER_CONTEXT_WHILE (test_data.signal_count < test_data.expected_signal_count);
    g_assert_cmpint (test_data.signal_count, ==, test_data.expected_signal_count);

    /* Test direct changes by modifying bookmarks */
    test_data.expected_signal_count++;
    nautilus_bookmark_list_add (list, bookmark1, -1);
    g_assert_cmpint (test_data.signal_count, ==, test_data.expected_signal_count);

    test_data.expected_signal_count++;
    nautilus_bookmark_list_add (list, bookmark2, -1);
    g_assert_cmpint (test_data.signal_count, ==, test_data.expected_signal_count);

    test_data.expected_signal_count++;
    nautilus_bookmark_list_remove (list, bookmark2);
    g_assert_cmpint (test_data.signal_count, ==, test_data.expected_signal_count);

    existing_bookmark = nautilus_bookmark_list_get_bookmark (list, bookmark1);
    g_assert_nonnull (existing_bookmark);
    test_data.expected_signal_count++;
    nautilus_bookmark_set_name (existing_bookmark, "New Name");
    g_assert_cmpint (test_data.signal_count, ==, test_data.expected_signal_count);

    /* Wait for the list to be written to the file. */
    ITER_CONTEXT_WHILE (!file_has_string (bookmarks_list_file, "New Name"));
    /* Allow bookmark list to handle the save callback and start monitoring the
     * file again. */
    g_main_context_iteration (NULL, FALSE);

    /* Test external changes by modifying the bookmarks file */
    g_autoptr (GFile) external_bookmark = g_file_new_for_path ("/tmp/external");
    const char *content = "file:///tmp/external External\n";
    g_autoptr (GError) error = NULL;

    test_data.expected_signal_count++;
    g_file_replace_contents (bookmarks_list_file,
                             content, strlen (content),
                             NULL,
                             FALSE,
                             G_FILE_CREATE_NONE,
                             NULL,
                             NULL,
                             &error);
    g_assert_no_error (error);

    ITER_CONTEXT_WHILE (nautilus_bookmark_list_get_bookmark (list, external_bookmark) == NULL);
    ITER_CONTEXT_WHILE (test_data.signal_count < test_data.expected_signal_count);
    g_assert_cmpint (test_data.signal_count, ==, test_data.expected_signal_count);

    test_clear_tmp_dir ();
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    nautilus_ensure_extension_points ();

    g_test_add_func ("/bookmarks/basic",
                     test_bookmark);

    g_test_add_func ("/bookmark-list/basic",
                     test_bookmark_list_basic);
    g_test_add_func ("/bookmark-list/changed-signal",
                     test_bookmark_list_changed_signal);

    return g_test_run ();
}
