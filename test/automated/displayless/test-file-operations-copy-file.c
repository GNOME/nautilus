#include <glib.h>
#include <glib/gprintf.h>
#include "src/nautilus-directory.h"
#include "src/nautilus-file-utilities.h"
#include "src/nautilus-search-directory.h"
#include "src/nautilus-directory.h"
#include "src/nautilus-file-operations.c"
#include <unistd.h>
#include "eel/eel-string.h"

/* The hierarchy looks like this:
 * /tmp/first_dir/file_to_copy
 * /tmp/second_dir
 * We're copying file_to_copy to second_dir without changing its name.
 */
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
                                             NULL,
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

/* The hierarchy looks like this:
 * /tmp/first_dir/file_to_copy
 * /tmp/second_dir
 * We're copying file_to_copy to second_dir while changing its name to
 * "new_file_to_copy".
 */
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

/* The hierarchy looks like this:
 * /tmp/first_dir/dir_to_copy
 * /tmp/second_dir
 * We're copying dir_to_copy to second_dir without changing its name.
 */
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
                                             NULL,
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

/* The hierarchy looks like this:
 * /tmp/first_dir/file_to_copy
 * /tmp/second_dir
 * We're copying dir_to_copy to second_dir while changing its name
 * to "new_dir_to_copy".
 */
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

/* The hierarchy looks like this:
 * /tmp/first_dir/file_to_copy
 * /tmp/second_dir/file_to_copy
 * We're copying file_to_copy to second_dir without changing its name.
 */
static void
test_copy_simple_file_overwrite (void)
{
    g_autoptr (GFile) root = NULL;
    g_autoptr (GFile) first_dir = NULL;
    g_autoptr (GFile) second_dir = NULL;
    g_autoptr (GFile) file = NULL;
    g_autoptr (GFile) second_file = NULL;
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

    second_file = g_file_get_child (second_dir, "file_to_copy");
    g_assert_true (second_file != NULL);
    g_file_create (file, G_FILE_CREATE_NONE, NULL, NULL);

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

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_child
 * /tmp/second_dir
 * We're copying first_dir to second_dir.
 */
static void
test_copy_full_directory_one_child (void)
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

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_file_sync (first_dir,
                                             second_dir,
                                             "second_dir",
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL);

    result_file = g_file_get_child (second_dir, "first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_dir_child");
    g_assert_true (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (result_file, NULL, NULL));

    file = g_file_get_child (first_dir, "first_dir_child");

    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_child
 * /tmp/first_dir/second_child
 * /tmp/second_dir
 * We're copying first_dir to second_dir.
 */
static void
test_copy_full_directory_two_children (void)
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


    file = g_file_get_child (first_dir, "first_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file = g_file_get_child (first_dir, "second_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_file_sync (first_dir,
                                             second_dir,
                                             "second_dir",
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL);

    result_file = g_file_get_child (second_dir, "first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (result_file, "second_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));

    file = g_file_get_child (first_dir, "first_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (first_dir, "second_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));    

    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_child/second_child
 * /tmp/second_dir
 * We're copying first_dir to second_dir.
 */
static void
test_copy_full_directory_first_hierarchy (void)
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


    file = g_file_get_child (first_dir, "first_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);
    file = g_file_get_child (file, "second_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_file_sync (first_dir,
                                             second_dir,
                                             "second_dir",
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL);

    result_file = g_file_get_child (second_dir, "first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_child");
    g_assert_true (g_file_query_exists (file, NULL));

    file = g_file_get_child (file, "second_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (result_file, "first_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));

    file = g_file_get_child (first_dir, "first_child");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "second_child");
    g_assert_true (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (file, NULL, NULL));
    file = g_file_get_child (first_dir, "first_child");
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (first_dir, NULL, NULL));
}

/* The hierarchy looks like this:
 * /tmp/first_dir/first_dir_dir1/dir1_child
 * /tmp/first_dir/first_dir_dir2/dir2_child
 * /tmp/second_dir
 * We're copying first_dir to second_dir.
 */
static void
test_copy_full_directory_second_hierarchy (void)
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


    file = g_file_get_child (first_dir, "first_dir_dir1");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (file, "dir1_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (first_dir, "first_dir_dir2");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    file = g_file_get_child (file, "dir2_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_file_sync (first_dir,
                                             second_dir,
                                             "second_dir",
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL);

    result_file = g_file_get_child (second_dir, "first_dir");

    g_assert_true (g_file_query_exists (result_file, NULL));
    file = g_file_get_child (result_file, "first_dir_dir1");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir1_child");
    g_assert_true (g_file_delete (file, NULL, NULL));
    file = g_file_get_child (result_file, "first_dir_dir1");
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (result_file, "first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir2_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    file = g_file_get_child (result_file, "first_dir_dir2");
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (result_file, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));

    file = g_file_get_child (first_dir, "first_dir_dir1");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir1_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    file = g_file_get_child (first_dir, "first_dir_dir1");
    g_assert_true (g_file_delete (file, NULL, NULL));

    file = g_file_get_child (first_dir, "first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));
    file = g_file_get_child (file, "dir2_child");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));
    file = g_file_get_child (first_dir, "first_dir_dir2");
    g_assert_true (g_file_query_exists (file, NULL));
    g_assert_true (g_file_delete (file, NULL, NULL));

    g_assert_true (g_file_delete (first_dir, NULL, NULL));
}

/* The hierarchy is:
 * /tmp/first_dir/first_dir_child
 * /tmp/second_dir
 * We're copying first_dir to second_dir while passing "copied_first_dir"
 * to the "new_name" argument. The expected result would be:
 * /tmp/second_dir/copied_first_dir/first_dir_child
 * But the actual result is:
 * /tmp/second_dir/ccopied_first_dir/copied_first_dir
 * This should be decommented once fixed.

static void
test_copy_full_directory_new_name (void)
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

    file = g_file_get_child (first_dir, "first_dir_child");
    g_assert_true (file != NULL);
    g_file_make_directory (file, NULL, NULL);

    second_dir = g_file_get_child (root, "second_dir");
    g_assert_true (second_dir != NULL);
    g_file_make_directory (second_dir, NULL, NULL);

    nautilus_file_operations_copy_file_sync (first_dir,
                                             second_dir,
                                             "second_dir",
                                             "copied_first_dir",
                                             NULL,
                                             NULL,
                                             NULL);

    result_file = g_file_get_child (second_dir, "copied_first_dir");
    g_assert_true (g_file_query_exists (result_file, NULL));
    * Once fixed, "copied_first_dir" should be changed to "first_dir_child"
    * on this line:
    file = g_file_get_child (result_file, "copied_first_dir");
    g_assert_true (g_file_query_exists (file, NULL));

    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (result_file, NULL, NULL));

    file = g_file_get_child (first_dir, "first_dir_child");

    g_assert_true (g_file_delete (file, NULL, NULL));
    g_assert_true (g_file_delete (first_dir, NULL, NULL));
    g_assert_true (g_file_delete (second_dir, NULL, NULL));
}
*/
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
    g_test_add_func ("/copy-simple-file-overwrite/1.0",
                     test_copy_simple_file_overwrite);
    g_test_add_func ("/copy-full-dir/1.0",
                     test_copy_full_directory_one_child);
    g_test_add_func ("/copy-full-dir/1.1",
                     test_copy_full_directory_two_children);
    g_test_add_func ("/copy-full-dir/1.2",
                     test_copy_full_directory_first_hierarchy);
    g_test_add_func ("/copy-full-dir/1.3",
                     test_copy_full_directory_second_hierarchy);
    // g_test_add_func ("/copy-full-dir-new-name/1.0",
    //                  test_copy_full_directory_new_name);
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
