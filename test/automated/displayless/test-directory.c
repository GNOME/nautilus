#include <glib.h>

#include <nautilus-directory.h>
#include <nautilus-directory-private.h>
#include <nautilus-file-utilities.h>


static int data_dummy;


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

    for (guint i = 0; nautilus_directory_number_outstanding () != 0 && i < 100000; i++)
    {
        g_main_context_iteration (NULL, TRUE);
    }
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
    g_assert (callback_data == &data_dummy);

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
    for (guint i = 0; !got_files_flag && i < 100000; i++)
    {
        g_main_context_iteration (NULL, TRUE);
    }

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

    return g_test_run ();
}
