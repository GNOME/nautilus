#include <glib.h>

#include <nautilus-ui-utilities.h>

#include <test-utilities.h>


static void
test_string_capitalization (void)
{
    char *capitalized;

    g_assert_null (nautilus_capitalize_str (NULL));

    capitalized = nautilus_capitalize_str ("");
    g_assert_cmpstr (capitalized, ==, "");
    g_free (capitalized);

    capitalized = nautilus_capitalize_str ("foo");
    g_assert_cmpstr (capitalized, ==, "Foo");
    g_free (capitalized);

    capitalized = nautilus_capitalize_str ("Foo");
    g_assert_cmpstr (capitalized, ==, "Foo");
    g_free (capitalized);
}

int
main (int   argc,
      char *argv[])
{
    nautilus_test_init (&argc, &argv, NAUTILUS_TEST_INIT_NONE);

    g_test_add_func ("/string-capitalization",
                     test_string_capitalization);

    return g_test_run ();
}
