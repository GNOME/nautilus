#include <glib.h>
#include <glib/gprintf.h>
#include "src/nautilus-directory.h"
#include "src/nautilus-directory-private.h"
#include "src/nautilus-file.h"
#include "src/nautilus-file-private.h"
#include "src/nautilus-file-utilities.h"
#include "src/nautilus-search-directory.h"
#include <unistd.h>

#define ROOT_DIR "file:///tmp"

/* Tests the function for empty selections */
static void
test_both_null (void)
{
    g_assert_true (nautilus_file_selection_equal (NULL, NULL));
}

/* Tests the function for an empty and a non-empty selection */
static void
test_either_null (void)
{
    g_autoptr (NautilusFile) file = NULL;
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autolist (NautilusFile) selection = NULL;

    directory = nautilus_directory_get_by_uri (ROOT_DIR);
    g_assert_true (NAUTILUS_IS_DIRECTORY (directory));

    file = nautilus_file_new_from_filename (directory, "null_first", FALSE);
    nautilus_directory_add_file (directory, file);

    selection = g_list_append (selection, g_object_ref (file));
    g_assert_false (nautilus_file_selection_equal (NULL, selection));
    g_assert_false (nautilus_file_selection_equal (selection, NULL));
}

/* tests the function for 2 identical selections, each containing one file */
static void
test_one_file_equal (void)
{
    g_autoptr (NautilusFile) file = NULL;
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autolist (NautilusFile) selection = NULL;

    directory = nautilus_directory_get_by_uri (ROOT_DIR);
    g_assert_true (NAUTILUS_IS_DIRECTORY (directory));

    file = nautilus_file_new_from_filename (directory, "one_file_equal", FALSE);
    nautilus_directory_add_file (directory, file);

    selection = g_list_append (selection, g_object_ref (file));
    g_assert_true (nautilus_file_selection_equal (selection, selection));
}

/* Tests the function for 2 different selections, each containing one file */
static void
test_one_file_different (void)
{
    g_autoptr (NautilusFile) one_file_first = NULL;
    g_autoptr (NautilusFile) one_file_second = NULL;
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autolist (NautilusFile) first_selection = NULL;
    g_autolist (NautilusFile) second_selection = NULL;

    directory = nautilus_directory_get_by_uri (ROOT_DIR);
    g_assert_true (NAUTILUS_IS_DIRECTORY (directory));

    one_file_first = nautilus_file_new_from_filename (directory, "one_file_first", FALSE);
    nautilus_directory_add_file (directory, one_file_first);

    one_file_second = nautilus_file_new_from_filename (directory, "one_file_second", FALSE);
    nautilus_directory_add_file (directory, one_file_second);

    first_selection = g_list_append (first_selection, g_object_ref (one_file_first));
    second_selection = g_list_append (second_selection, g_object_ref (one_file_second));
    g_assert_false (nautilus_file_selection_equal (first_selection, second_selection));
}

/* Tests the function for 2 identical selections, each containing 50 files */
static void
test_multiple_files_equal_medium (void)
{
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autolist (NautilusFile) selection = NULL;

    directory = nautilus_directory_get_by_uri (ROOT_DIR);
    g_assert_true (NAUTILUS_IS_DIRECTORY (directory));
    for (gint index = 0; index < 50; index++)
    {
        g_autoptr (NautilusFile) file = NULL;
        g_autofree gchar *file_name = NULL;

        file_name = g_strdup_printf ("multiple_files_equal_medium_%i", index);
        file = nautilus_file_new_from_filename (directory, file_name, FALSE);
        nautilus_directory_add_file (directory, file);
        selection = g_list_prepend (selection, g_object_ref (file));
    }

    g_assert_true (nautilus_file_selection_equal (selection, selection));
}

/* Tests the function for 2 different selections, each containing 51 files,
 * the last file being the different one */
static void
test_multiple_files_different_medium (void)
{
    g_autoptr (NautilusFile) first_file = NULL;
    g_autoptr (NautilusFile) second_file = NULL;
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autolist (NautilusFile) first_selection = NULL;
    g_autolist (NautilusFile) second_selection = NULL;
    g_autofree gchar *first_file_name = NULL;
    g_autofree gchar *second_file_name = NULL;

    directory = nautilus_directory_get_by_uri (ROOT_DIR);
    g_assert_true (NAUTILUS_IS_DIRECTORY (directory));
    for (gint index = 0; index < 50; index++)
    {
        first_file_name = g_strdup_printf ("multiple_files_different_medium_%i", index);
        first_file = nautilus_file_new_from_filename (directory, first_file_name, FALSE);
        nautilus_directory_add_file (directory, first_file);
        g_assert_true (NAUTILUS_IS_FILE (first_file));
        first_selection = g_list_prepend (first_selection, g_object_ref (first_file));
        second_selection = g_list_prepend (second_selection, g_object_ref (first_file));
    }

    first_file_name = g_strdup_printf ("multiple_files_different_medium_lastElement");
    second_file_name = g_strdup_printf ("multiple_files_different_medium_differentElement");
    first_file = nautilus_file_new_from_filename (directory, first_file_name, FALSE);
    nautilus_directory_add_file (directory, first_file);
    second_file = nautilus_file_new_from_filename (directory, second_file_name, FALSE);
    nautilus_directory_add_file (directory, second_file);
    g_assert_true (NAUTILUS_IS_FILE (first_file));
    g_assert_true (NAUTILUS_IS_FILE (second_file));
    first_selection = g_list_append (first_selection, g_object_ref (first_file));
    second_selection = g_list_append (second_selection, g_object_ref (second_file));

    g_assert_false (nautilus_file_selection_equal (first_selection, second_selection));
}

/* Tests the function for 2 identical selections, each containing 1000 files */
static void
test_multiple_files_equal_large (void)
{
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autolist (NautilusFile) selection = NULL;

    directory = nautilus_directory_get_by_uri (ROOT_DIR);
    g_assert_true (NAUTILUS_IS_DIRECTORY (directory));
    for (gint index = 0; index < 1000; index++)
    {
        g_autoptr (NautilusFile) file = NULL;
        g_autofree gchar *file_name = NULL;

        file_name = g_strdup_printf ("multiple_files_equal_large_%i", index);
        file = nautilus_file_new_from_filename (directory, file_name, FALSE);
        nautilus_directory_add_file (directory, file);
        g_assert_true (NAUTILUS_IS_FILE (file));
        selection = g_list_prepend (selection, g_object_ref (file));
    }

    g_assert_true (nautilus_file_selection_equal (selection, selection));
}

/* Tests the function for 2 different selections, each containing 1001 files,
 * the last file being the different one */
static void
test_multiple_files_different_large (void)
{
    g_autoptr (NautilusFile) first_file = NULL;
    g_autoptr (NautilusFile) second_file = NULL;
    g_autoptr (NautilusDirectory) directory = NULL;
    g_autolist (NautilusFile) first_selection = NULL;
    g_autolist (NautilusFile) second_selection = NULL;
    g_autofree gchar *first_file_name = NULL;
    g_autofree gchar *second_file_name = NULL;

    directory = nautilus_directory_get_by_uri (ROOT_DIR);
    g_assert_true (NAUTILUS_IS_DIRECTORY (directory));
    for (gint index = 0; index < 1000; index++)
    {
        first_file_name = g_strdup_printf ("multiple_files_different_large_%i", index);
        first_file = nautilus_file_new_from_filename (directory, first_file_name, FALSE);
        nautilus_directory_add_file (directory, first_file);
        g_assert_true (NAUTILUS_IS_FILE (first_file));
        first_selection = g_list_prepend (first_selection, g_object_ref (first_file));
        second_selection = g_list_prepend (second_selection, g_object_ref (first_file));
    }

    first_file_name = g_strdup_printf ("multiple_files_different_large_lastElement");
    second_file_name = g_strdup_printf ("multiple_files_different_large_differentElement");
    first_file = nautilus_file_new_from_filename (directory, first_file_name, FALSE);
    nautilus_directory_add_file (directory, first_file);
    second_file = nautilus_file_new_from_filename (directory, second_file_name, FALSE);
    nautilus_directory_add_file (directory, second_file);
    g_assert_true (NAUTILUS_IS_FILE (first_file));
    g_assert_true (NAUTILUS_IS_FILE (second_file));

    first_selection = g_list_append (first_selection, g_object_ref (first_file));
    second_selection = g_list_append (second_selection, g_object_ref (second_file));

    g_assert_false (nautilus_file_selection_equal (first_selection, second_selection));
}

static void
setup_test_suite (void)
{
    g_test_add_func ("/file-selection-equal-null/1.0",
                     test_both_null);
    g_test_add_func ("/file-selection-equal-null/1.1",
                     test_either_null);
    g_test_add_func ("/file-selection-equal-files/1.0",
                     test_one_file_equal);
    g_test_add_func ("/file-selection-equal-files/1.1",
                     test_multiple_files_equal_medium);
    g_test_add_func ("/file-selection-equal-files/1.2",
                     test_multiple_files_equal_large);
    g_test_add_func ("/file-selection-different-files/1.0",
                     test_one_file_different);
    g_test_add_func ("/file-selection-different-files/1.1",
                     test_multiple_files_different_medium);
    g_test_add_func ("/file-selection-different-files/1.2",
                     test_multiple_files_different_large);
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();

    setup_test_suite ();

    return g_test_run ();
}
