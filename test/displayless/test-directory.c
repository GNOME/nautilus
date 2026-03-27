#include "test-utilities.h"

#include <glib.h>

#include <nautilus-directory.h>
#include <nautilus-directory-private.h>
#include <nautilus-file-utilities.h>


static int data_dummy;

typedef struct
{
    guint expected_number_callbacks;
    GMainLoop *loop;
} CallbackCounter;

static CallbackCounter *
callback_counter_new (guint expected_number_callbacks)
{
    CallbackCounter *data = g_new0 (CallbackCounter, 1);

    data->expected_number_callbacks = expected_number_callbacks;
    data->loop = g_main_loop_new (NULL, FALSE);

    return data;
}

static void
callback_counter_free (CallbackCounter *data)
{
    g_clear_pointer (&data->loop, g_main_loop_unref);

    g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (CallbackCounter, callback_counter_free)

static void
callback_counter_callback (CallbackCounter *data)
{
    data->expected_number_callbacks -= 1;

    if (data->expected_number_callbacks == 0)
    {
        g_assert_nonnull (data->loop);

        g_main_loop_quit (data->loop);
        g_clear_pointer (&data->loop, g_main_loop_unref);
    }
}

static void
callback_counter_await (CallbackCounter *data)
{
    if (data->loop != NULL)
    {
        g_main_loop_run (data->loop);
    }
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

    ITER_CONTEXT_WHILE (nautilus_directory_number_outstanding () > 0);

    g_assert_cmpuint (nautilus_directory_number_outstanding (), ==, 0);
}

static void
callback_counter_directory_callback (NautilusDirectory *directory,
                                     NautilusFileList  *files,
                                     gpointer           user_data)
{
    callback_counter_callback (user_data);
}

/** Check that call-when-ready works */
static void
test_directory_call_when_ready (void)
{
    g_autoptr (NautilusDirectory) directory = nautilus_directory_get_by_uri ("file:///etc");
    g_autoptr (CallbackCounter) data = callback_counter_new (1);

    g_assert_cmpuint (nautilus_directory_number_outstanding (), ==, 1);

    nautilus_directory_call_when_ready (directory,
                                        NAUTILUS_FILE_ATTRIBUTE_INFO |
                                        NAUTILUS_FILE_ATTRIBUTE_DEEP_COUNTS,
                                        TRUE,
                                        callback_counter_directory_callback, data);

    callback_counter_await (data);

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
