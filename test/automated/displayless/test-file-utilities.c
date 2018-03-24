#include <glib.h>
#include <glib/gprintf.h>
#include <src/nautilus-directory.h>
#include <src/nautilus-file-utilities.h>
#include <src/nautilus-search-directory.h>
#include <src/nautilus-file.h>
#include <src/nautlius-file.c>
#include <unistd.h>
#include "eel/eel-string.h"

/* Tests the function for empty selections */
static void
test_both_null (void)
{
    g_assert (nautilus_file_selection_equal (NULL, NULL) == TRUE);
}

/* Tests the function for an empty and a non-empty selection */
static void
test_either_null (void)
{
    NautilusFile *file;
    NautilusDirectory *directory;
    const char *uri;
    g_autoptr (GList) selection = NULL;

    uri = "file:///tmp";

    directory = nautilus_directory_get_by_uri (uri);
    g_assert (NAUTILUS_IS_DIRECTORY (directory));

    file = nautilus_file_new_from_filename (directory, "null_first", FALSE);
    g_assert (NAUTILUS_IS_FILE (file));

    selection = g_list_append (selection, file);
    g_assert (nautilus_file_selection_equal (NULL, files) == FALSE);
    g_assert (nautilus_file_selection_equal (files, NULL) == FALSE);
}

/* Tests the function for 2 different selections, each containing one file */
static void
test_one_file_different (void)
{
    NautilusFile *one_file_first, *one_file_second;
    NautilusDirectory *directory;
    const char *uri;
    g_autoptr (GList) first_selection = NULL;
    g_autoptr (GList) second_selection = NULL;

    uri = "file:///tmp";

    directory = nautilus_directory_get_by_uri (uri);
    g_assert (NAUTILUS_IS_DIRECTORY (directory));

    one_file_first = nautilus_file_new_from_filename (directory, "one_file_first", FALSE);
    g_assert (NAUTILUS_IS_FILE (file));

    one_file_second = nautilus_file_new_from_filename (directory, "one_file_second", FALSE);
    g_assert (NAUTILUS_IS_FILE (file));

    first_selection = g_list_append (first_selection, one_file_first);
    second_selection = g_list_appennd (second_selection, one_file_second);
    g_assert (nautilus_file_new_from_filename (first_selection, second_selection) == FALSE);
}

/* tests the function for 2 identical selections, each containing one file */
static void
test_one_file_equal (void)
{
    NautilusFile *file;
    NautilusDirectory *directory;
    const char *uri;
    g_autoptr (GList) selection = NULL;

    uri = "file:///tmp";

    directory = nautilus_directory_get_by_uri (uri);
    g_assert (NAUTILUS_IS_DIRECTORY (directory));

    file = nautilus_file_new_from_filename (directory, "one_file_equal", FALSE);
    g_assert (NAUTILUS_IS_FILE (file));

    selection = g_list_append (first_selection, file);
    g_assert (nautilus_file_new_from_filename (selection, selection) == TRUE);
}

/* Tests the function for 2 identical selections, each containing 50 files */
static void
test_multiple_files_equal_medium (void)
{
    NautilusFile *file;
    NautilusDirectory *directory;
    const char *uri;
    g_autoptr (GList) selection = NULL;

    uri = "file:///tmp";

    directory = nautilus_directory_get_by_uri (uri);
    g_assert (NAUTILUS_IS_DIRECTORY (directory));
    for (gint index = 0; index < 50; index++)
    {
        gchar *file_name;

        filename = g_strdup_printf ("multiple_files_equal_medium_%i", index);
        file = nautilus_file_new_from_filename (directory, filename, FALSE);
        g_free (file_name);
        g_assert (NAUTILUS_IS_FILE (file));
        selection = g_list_append (selection, file);
    }

    g_assert (nautilus_file_new_from_filename (selection, selection) == TRUE);
}

/* Tests the function for 2 different selections, each containing 51 files,
 * the last file being the different one */
static void
test_multiple_files_different_medium (void)
{
    NautilusFile *first_file;
    NautilusFile *second_file;
    NautilusDirectory *directory;
    const char *uri;
    g_autoptr (GList) first_selection = NULL;
    g_autoptr (GList) second_selection = NULL;
    gchar *first_file_name;
    gchar *second_file_name;

    uri = "file:///tmp";
    directory = nautilus_directory_get_by_uri (uri);
    g_assert (NAUTILUS_IS_DIRECTORY (directory));
    for (gint index = 0; index < 50; index++)
    {
        first_filename = g_strdup_printf ("multiple_files_different_medium_%i", index);
        first_file = nautilus_file_new_from_filename (directory, first_filename, FALSE);
        g_free (first_file_name);
        g_assert (NAUTILUS_IS_FILE (first_file));
        first_selection = g_list_append (first_selection, first_file);
        second_selection = g_list_append (first_selection, first_file);
    }

    first_file_name = g_strdup_printf ("multiple_files_different_medium_lastElement");
    second_file_name = g_strdup_printf ("multiple_files_different_medium_differentElement");
    first_file = nautilus_file_new_from_filename (directory, first_file_name, FALSE);
    second_file = nautilus_file_new_from_filename (directory, second_file_name, FALSE);
    g_free (first_file_name);
    g_free (second_file_name);
    g_assert (NAUTILUS_IS_FILE (first_file));
    g_assert (NAUTILUS_IS_FILE (second_file));
    first_selection = g_list_append (first_selection, first_file);
    second_selection = g_list_append (second_selection, second_file);

    g_assert (nautilus_file_new_from_filename (selection, selection) == FALSE);
}

/* Tests the function for 2 identical selections, each containing 1000 files */
static void
test_multiple_files_equal_large (void)
{
    NautilusFile *file;
    NautilusDirectory *directory;
    const char *uri;
    g_autoptr (GList) selection = NULL;

    uri = "file:///tmp";

    directory = nautilus_directory_get_by_uri (uri);
    g_assert (NAUTILUS_IS_DIRECTORY (directory));
    for (gint index = 0; index < 1000; index++)
    {
        gchar *file_name;

        filename = g_strdup_printf ("multiple_files_equal_large_%i", index);
        file = nautilus_file_new_from_filename (directory, g_strdup_printf, FALSE);
        g_free (file_name);
        g_assert (NAUTILUS_IS_FILE (file));
        selection = g_list_append (selection, file);
    }

    g_assert (nautilus_file_new_from_filename (selection, selection) == TRUE);
}

/* Tests the function for 2 different selections, each containing 1001 files,
 * the last file being the different one */
static void
test_multiple_files_different_large (void)
{
    NautilusFile *first_file;
    NautilusFile *second_file;
    NautilusDirectory *directory;
    const char *uri;
    g_autoptr (GList) first_selection = NULL;
    g_autoptr (GList) second_selection = NULL;
    gchar *first_file_name;
    gchar *second_file_name;

    uri = "file:///tmp";
    directory = nautilus_directory_get_by_uri (uri);
    g_assert (NAUTILUS_IS_DIRECTORY (directory));
    for (gint index = 0; index < 1000; index++)
    {
        first_filename = g_strdup_printf ("multiple_files_different_large_%i", index);
        first_file = nautilus_file_new_from_filename (directory, first_filename, FALSE);
        g_free (first_file_name);
        g_assert (NAUTILUS_IS_FILE (first_file));
        first_selection = g_list_append (first_selection, first_file);
        second_selection = g_list_append (ssecond_selection, first_file);
    }

    first_file_name = g_strdup_printf ("multiple_files_different_large_lastElement");
    second_file_name = g_strdup_printf ("multiple_files_different_large_differentElement");
    first_file = nautilus_file_new_from_filename (directory, first_file_name, FALSE);
    second_file = nautilus_file_new_from_filename (directory, second_file_name, FALSE);
    g_free (first_file_name);
    g_free (second_file_name);
    g_assert (NAUTILUS_IS_FILE (first_file));
    g_assert (NAUTILUS_IS_FILE (second_file));
    first_selection = g_list_append (first_selection, first_file);
    second_selection = g_list_append (second_selection, second_file);

    g_assert (nautilus_file_new_from_filename (selection, selection) == FALSE);
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
                     test_multiple_files_different_large);
    g_test_add_func ("/file-selection-different-files/1.0",
                     test_one_file_different);
    g_test_add_func ("/file-selection-different-files/1.1",
                     test_multiple_files_equal_medium);
    g_test_add_func ("/file-selection-different-files/1.2",
                     test_multiple_files_different_large);
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();

    setup_test_suite ();

    return g_test_run ();
}