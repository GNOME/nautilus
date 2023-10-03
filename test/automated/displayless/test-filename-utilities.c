#include <glib.h>

#include <nautilus-filename-utilities.h>


static const char *long_base = "great-text-but-sadly-too-long";
static const char *short_base = "great-text";
static const char *suffix = "-123456789";

static void
test_filename_shortening_with_base (void)
{
    g_autofree char *filename = g_strconcat (long_base, suffix, NULL);
    g_autofree char *desired = g_strconcat (short_base, suffix, NULL);
    size_t max_length = strlen (short_base) + strlen (suffix);

    g_assert_cmpuint (strlen (filename), >, max_length);

    gboolean shortened = nautilus_filename_shorten_base (&filename, long_base, max_length);

    g_assert_true (shortened);

    g_assert_cmpuint (strlen (filename), <=, max_length);

    g_assert_cmpstr (filename, ==, desired);
}

static void
test_filename_shortening_with_base_not_needed (void)
{
    g_autofree char *filename = g_strconcat (short_base, suffix, NULL);
    g_autofree char *desired = g_strconcat (short_base, suffix, NULL);
    size_t max_length = strlen (short_base) + strlen (suffix);

    g_assert_cmpuint (strlen (filename), <=, max_length);

    gboolean shortened = nautilus_filename_shorten_base (&filename, short_base, max_length);

    g_assert_false (shortened);

    g_assert_cmpuint (strlen (filename), <=, max_length);

    g_assert_cmpstr (filename, ==, desired);
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();

    g_test_add_func ("/file-name-shortening-with-base/needed",
                     test_filename_shortening_with_base);
    g_test_add_func ("/file-name-shortening-with-base/not-needed",
                     test_filename_shortening_with_base_not_needed);

    return g_test_run ();
}
