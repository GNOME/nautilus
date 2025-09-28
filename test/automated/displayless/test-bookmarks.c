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
test_bookmark_list_basic (void)
{
    g_autoptr (NautilusBookmarkList) list = nautilus_bookmark_list_new ();
    g_autoptr (GFile) bookmark1 = g_file_new_build_filename (test_get_tmp_dir (), "one", NULL);
    g_autoptr (GFile) bookmark2 = g_file_new_build_filename (test_get_tmp_dir (), "two", NULL);

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
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);

    test_init_config_dir ();
    nautilus_ensure_extension_points ();

    g_test_add_func ("/bookmarks/basic",
                     test_bookmark);

    g_test_add_func ("/bookmark-list/basic",
                     test_bookmark_list_basic);

    return g_test_run ();
}
