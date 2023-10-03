#include <glib.h>

#include <nautilus-filename-utilities.h>


static char *
format_helper (const char *str)
{
    return g_strdup_printf ("%s-123456789", str);
}

static const char *long_base = "great-text-but-sadly-too-long";
static const char *short_base = "great-text";

static void
test_file_name_shortening_with_base (void)
{
    g_autofree char *filename = format_helper (long_base);
    g_autofree char *desired = format_helper (short_base);
    size_t max_length = 20;
    gboolean shortened;

    g_assert_cmpuint (strlen (filename), >, max_length);

    shortened = nautilus_filename_shorten_base (&filename, long_base, max_length);

    g_assert_true (shortened);

    g_assert_cmpuint (strlen (filename), <=, max_length);

    g_assert_cmpstr (filename, ==, desired);
}

static void
test_file_name_shortening_with_base_not_needed (void)
{
    g_autofree char *filename = format_helper (short_base);
    g_autofree char *desired = format_helper (short_base);
    size_t max_length = 20;
    gboolean shortened;

    g_assert_cmpuint (strlen (filename), <=, max_length);

    shortened = nautilus_filename_shorten_base (&filename, short_base, max_length);

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
                     test_file_name_shortening_with_base);
    g_test_add_func ("/file-name-shortening-with-base/not-needed",
                     test_file_name_shortening_with_base_not_needed);

    return g_test_run ();
}
