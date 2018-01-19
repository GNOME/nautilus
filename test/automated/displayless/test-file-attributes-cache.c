#include <glib.h>
#include <glib/gprintf.h>
#include <gtk/gtk.h>

#include "src-ng/nautilus-file.h"

#define THUMBNAIL_EXPECT_WIDTH 101
#define THUMBNAIL_EXPECT_HEIGHT 34

static GMutex mutex;
static GCond cond;

typedef struct
{
    gboolean completed;
    gboolean success;
} TestResult;

static void
free_result (TestResult *result)
{
    g_free (result);
}

static void
on_query_info_finished (NautilusFile *file,
                        GFileInfo    *info,
                        GError       *error,
                        gpointer      user_data)
{
    TestResult *result;

    result = user_data;

    g_mutex_lock (&mutex);
    result->completed = TRUE;
    result->success = error == NULL;
    g_cond_signal (&cond);
    g_mutex_unlock (&mutex);
    g_print ("unlocking\n");
}

static void
test_query_info ()
{
    g_autoptr (NautilusFile) file = NULL;
    g_autoptr (GFile) location = NULL;
    TestResult *result;
    gint64 end_time;

    location = g_file_new_for_path ("test_file_01");
    file = nautilus_file_new (location);
    g_assert_nonnull (file);

    result = g_new (TestResult, 1);
    result->completed = FALSE;
    result->success = FALSE;

    g_mutex_lock (&mutex);
    end_time = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
    nautilus_file_query_info (file, NULL, on_query_info_finished, result);
    while (!result->completed)
    {
        if (!g_cond_wait_until (&cond, &mutex, end_time))
        {
            g_printerr ("Timeout has been reached");
            g_mutex_unlock (&mutex);
            g_assert_not_reached ();
        }
    }
    g_assert (result->success);

    free_result(result);
    g_mutex_unlock (&mutex);
}

static void
on_get_thumbnail_finished (NautilusFile *file,
                           GdkPixbuf    *thumbnail,
                           gpointer      user_data)
{
    TestResult *result;
    gint width;
    gint height;

    result = user_data;

    g_mutex_lock (&mutex);
    result->completed = TRUE;
    width = gdk_pixbuf_get_width (thumbnail);
    height = gdk_pixbuf_get_height (thumbnail);
    g_assert_cmpint (width, THUMBNAIL_EXPECT_WIDTH);
    g_assert_cmpint (height, THUMBNAIL_EXPECT_HEIGHT);
    result->success = TRUE;
    g_cond_signal (&cond);
    g_mutex_unlock (&mutex);
}

static void
test_get_thumbnail ()
{
    g_autoptr (NautilusFile) file = NULL;
    g_autoptr (GFile) location = NULL;
    TestResult *result;
    gint64 end_time;

    location = g_file_new_for_path ("test_file_02");
    file = nautilus_file_new (location);
    g_assert_nonnull (file);

    result = g_new (TestResult, 1);
    result->completed = FALSE;
    result->success = FALSE;

    g_mutex_lock (&mutex);
    end_time = g_get_monotonic_time () + 10 * G_TIME_SPAN_SECOND;
    nautilus_file_get_thumbnail (file, on_get_thumbnail_finished, result);
    while (!result->completed)
    {
        if (!g_cond_wait_until (&cond, &mutex, end_time))
        {
            g_printerr ("Timeout has been reached");
            g_mutex_unlock (&mutex);
            g_assert_not_reached ();
        }
    }

    g_assert (result->success);

    free_result(result);
    g_mutex_unlock (&mutex);
}

static void
setup_test_suite ()
{
    g_test_add_func ("/info/1.0",
                     test_query_info);

    g_test_add_func ("/thumbnail/1.1",
                     test_get_thumbnail);
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
