#include <glib.h>
#include <glib/gprintf.h>

#include "nautilus-filename-utilities.h"


static void
free_list_and_result (GList *list,
                      char  *result)
{
    g_list_free (list);
    g_free (result);
}

static void
test_has_large_enough_common_prefix (void)
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "foo-1.txt");
    list = g_list_append (list, "foo-1.tar");

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_cmpstr ("foo-1.t", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_has_common_prefix_that_equals_the_min_required_length (void)
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "food");
    list = g_list_append (list, "foody");

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_cmpstr ("food", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_has_common_prefix_that_equals_the_min_required_length2 (void)
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "foody");
    list = g_list_append (list, "food");

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_cmpstr ("food", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_many_strings_with_common_prefix (void)
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "some text that matches abcde");
    list = g_list_append (list, "some text that matches abc22");
    list = g_list_append (list, "some text that 11");
    list = g_list_append (list, "some text that matches---");
    list = g_list_append (list, "some text that matches £$$");
    list = g_list_append (list, "some text that matches.txt");

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_cmpstr ("some text that ", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_strings_with_unicode_characters_that_have_common_prefix (void)
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "ƹƱƱƬ");
    list = g_list_append (list, "ƹƱƱƬƧƥƧ");

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_cmpstr ("ƹƱƱƬ", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_no_common_prefix (void)
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "fyod");
    list = g_list_append (list, "completely different string");

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_null (actual);

    free_list_and_result (list, actual);
}

static void
test_has_common_prefix_but_smaller_than_min_required_length (void)
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "fyod");
    list = g_list_append (list, "fyoa");

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_null (actual);

    free_list_and_result (list, actual);
}

static void
test_first_character_differs (void)
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "tyodaa");
    list = g_list_append (list, "fyodaa");

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_null (actual);

    free_list_and_result (list, actual);
}

static void
test_strings_with_unicode_characters_that_dont_have_common_prefix (void)
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "ƹƱƱƬ");
    list = g_list_append (list, "ƹƱƢƱƬƧƥƧ");

    actual = nautilus_filename_get_common_prefix (list, 4);
    g_assert_null (actual);

    free_list_and_result (list, actual);
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
