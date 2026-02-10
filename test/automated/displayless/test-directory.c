#include "test-utilities.h"

#include <glib.h>

#include <nautilus-directory.h>
#include <nautilus-directory-private.h>
#include <nautilus-file-utilities.h>


static int data_dummy;
static gboolean got_dirs_flag;

static void
directories_ready (NautilusDirectoryList *directories,
                    gpointer              callback_data)
{
    guint expected = GPOINTER_TO_UINT (callback_data);

    g_assert_cmpuint (g_list_length (directories), ==, expected);

    got_dirs_flag = TRUE;
}


/** Test empty directory check */
static void
test_directory_empty_check (void)
{
    const GStrv hierarchy = (char *[])
    {
        "empty_check/",
        "empty_check/empty_dir/",
        "empty_check/non_empty_dir/",
        "empty_check/non_empty_dir/file1",
        "empty_check/nested_dir/",
        "empty_check/nested_dir/dir/",
        NULL
    };
    got_dirs_flag = FALSE;
    guint dirs_to_check = 3;
    g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (),
                                                            "empty_check", NULL);
    g_autofree char *base_uri = g_file_get_uri (location);

    file_hierarchy_create (hierarchy, "");

    g_autofree char *empty_uri = g_strdup_printf ("%s/empty_dir", base_uri);
    g_autofree char *non_empty_uri = g_strdup_printf ("%s/non_empty_dir", base_uri);
    g_autofree char *nested_uri = g_strdup_printf ("%s/nested_dir", base_uri);
    g_autoptr (NautilusDirectory) empty = nautilus_directory_get_by_uri (empty_uri);
    g_autoptr (NautilusDirectory) non_empty = nautilus_directory_get_by_uri (non_empty_uri);
    g_autoptr (NautilusDirectory) nested = nautilus_directory_get_by_uri (nested_uri);

    NautilusDirectoryList *directories = &(GList){ .data = empty };
    directories = g_list_prepend (directories, non_empty);
    directories = g_list_prepend (directories, nested);

    nautilus_directory_list_call_when_ready (directories, NAUTILUS_FILE_ATTRIBUTE_INFO, NULL, TRUE,
                                             directories_ready, GUINT_TO_POINTER (dirs_to_check));

    ITER_CONTEXT_WHILE (!got_dirs_flag);

    g_assert_false (nautilus_directory_is_not_empty (empty));
    g_assert_true (nautilus_directory_is_not_empty (non_empty));
    g_assert_true (nautilus_directory_is_not_empty (nested));

    test_clear_tmp_dir ();
}

/** Check for same directory object for duplicates */
static void
test_directory_duplicate_pointers (void)
{
    g_autoptr (NautilusDirectory) directory = nautilus_directory_get_by_uri ("file:///etc");
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri ("file:///etc/passwd");

    /* Assert that the NautilusFile reuses the existing NautilusDirectory instance.. */
    g_assert_cmpuint (nautilus_directory_number_outstanding (), ==, 1);

    g_assert_true (nautilus_directory_get_by_uri ("file:///etc") == directory);
    nautilus_directory_unref (directory);

    g_assert_true (nautilus_directory_get_by_uri ("file:///etc/") == directory);
    nautilus_directory_unref (directory);

    g_assert_true (nautilus_directory_get_by_uri ("file:///etc////") == directory);
    nautilus_directory_unref (directory);
}

/** Check that directory hash table gets cleaned up */
static void
test_directory_hash_table_cleanup (void)
{
    NautilusDirectory *directory = nautilus_directory_get_by_uri ("file:///etc");

    g_assert_cmpuint (nautilus_directory_number_outstanding (), ==, 1);

    nautilus_directory_file_monitor_add (directory, &data_dummy, TRUE, 0, NULL, NULL);

    /* For normal usage there would be activity here, however, it is not needed for testing. */

    nautilus_directory_file_monitor_remove (directory, &data_dummy);
    nautilus_directory_unref (directory);

    ITER_CONTEXT_WHILE (nautilus_directory_number_outstanding () != 0);

    g_assert_cmpuint (nautilus_directory_number_outstanding (), ==, 0);
}

static gboolean got_files_flag;

static void
got_files_callback (NautilusDirectory *directory,
                    GList             *files,
                    gpointer           callback_data)
{
    g_assert_true (NAUTILUS_IS_DIRECTORY (directory));
    g_assert_cmpint (g_list_length (files), >, 10);
    g_assert_true (callback_data == &data_dummy);

    got_files_flag = TRUE;
}

/** Check that call-when-ready works */
static void
test_directory_call_when_ready (void)
{
    g_autoptr (NautilusDirectory) directory = nautilus_directory_get_by_uri ("file:///etc");
    g_assert_cmpuint (nautilus_directory_number_outstanding (), ==, 1);

    got_files_flag = FALSE;
    nautilus_directory_call_when_ready (directory,
                                        NAUTILUS_FILE_ATTRIBUTE_INFO |
                                        NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS,
                                        TRUE,
                                        got_files_callback, &data_dummy);

    ITER_CONTEXT_WHILE (!got_files_flag);

    g_assert_true (got_files_flag);
    /* Every NautilusFile created by call_when_ready must have been
     * unref'd and destroyed after the NautilusDirectoryCallback returns */
    g_assert_null (directory->details->file_list);
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();

    g_test_add_func ("/directory-duplicate-pointers/1.0",
                     test_directory_duplicate_pointers);
    g_test_add_func ("/directory-hash-table-cleanup/1.0",
                     test_directory_hash_table_cleanup);
    g_test_add_func ("/directory-call-when-ready/1.0",
                     test_directory_call_when_ready);
    g_test_add_func ("/directory-empty-check",
                     test_directory_empty_check);

    return g_test_run ();
}
