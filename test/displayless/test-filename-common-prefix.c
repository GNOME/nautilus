#include <glib.h>
#include <glib/gprintf.h>

#include "nautilus-filename-utilities.h"


static void
test_has_large_enough_common_prefix (void)
{
    g_autofree char *actual = NULL;
    const char *list[] =
    {
        "foo-1.txt",
        "foo-1.tar",
        NULL
    };

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_cmpstr ("foo-1.t", ==, actual);
}

static void
test_has_common_prefix_that_equals_the_min_required_length (void)
{
    g_autofree char *actual = NULL;
    const char *list[] =
    {
        "food",
        "foody",
        NULL
    };

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_cmpstr ("food", ==, actual);
}

static void
test_has_common_prefix_that_equals_the_min_required_length2 (void)
{
    g_autofree char *actual = NULL;
    const char *list[] =
    {
        "foody",
        "food",
        NULL
    };

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_cmpstr ("food", ==, actual);
}

static void
test_many_strings_with_common_prefix (void)
{
    g_autofree char *actual = NULL;
    const char *list[] =
    {
        "some text that matches abcde",
        "some text that matches abc22",
        "some text that 11",
        "some text that matches---",
        "some text that matches £$$",
        "some text that matches.txt",
        NULL
    };

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_cmpstr ("some text that ", ==, actual);
}

static void
test_strings_with_unicode_characters_that_have_common_prefix (void)
{
    g_autofree char *actual = NULL;
    const char *list[] =
    {
        "ƹƱƱƬ",
        "ƹƱƱƬƧƥƧ",
        NULL
    };

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_cmpstr ("ƹƱƱƬ", ==, actual);
}

static void
test_no_common_prefix (void)
{
    g_autofree char *actual = NULL;
    const char *list[] =
    {
        "fyod",
        "completely different string",
        NULL
    };

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_null (actual);
}

static void
test_has_common_prefix_but_smaller_than_min_required_length (void)
{
    g_autofree char *actual = NULL;
    const char *list[] =
    {
        "fyod",
        "fyoa",
        NULL
    };

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_null (actual);
}

static void
test_first_character_differs (void)
{
    g_autofree char *actual = NULL;
    const char *list[] =
    {
        "tyodaa",
        "fyodaa",
        NULL
    };

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_null (actual);
}

static void
test_strings_with_unicode_characters_that_dont_have_common_prefix (void)
{
    g_autofree char *actual = NULL;
    const char *list[] =
    {
        "ƹƱƱƬ",
        "ƹƱƢƱƬƧƥƧ",
        NULL
    };

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_null (actual);
}


static void
setup_test_suite (void)
{
    g_test_add_func ("/get-common-prefix/1.0",
                     test_has_large_enough_common_prefix);
    g_test_add_func ("/get-common-prefix/1.1",
                     test_has_common_prefix_that_equals_the_min_required_length);
    g_test_add_func ("/get-common-prefix/1.2",
                     test_has_common_prefix_that_equals_the_min_required_length2);
    g_test_add_func ("/get-common-prefix/1.3",
                     test_many_strings_with_common_prefix);
    g_test_add_func ("/get-common-prefix/1.4",
                     test_strings_with_unicode_characters_that_have_common_prefix);

    g_test_add_func ("/get-common-prefix/2.0",
                     test_no_common_prefix);
    g_test_add_func ("/get-common-prefix/2.1",
                     test_has_common_prefix_but_smaller_than_min_required_length);
    g_test_add_func ("/get-common-prefix/2.2",
                     test_first_character_differs);
    g_test_add_func ("/get-common-prefix/2.3",
                     test_strings_with_unicode_characters_that_dont_have_common_prefix);
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);
    g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=747907");
    g_test_set_nonfatal_assertions ();

    setup_test_suite ();

    return g_test_run ();
}
