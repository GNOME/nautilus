#include <glib.h>
#include <glib/gprintf.h>

#include "src/nautilus-file-utilities.h"


static void
free_list_and_result (GList *list,
                      char  *result)
{
    g_list_free (list);
    g_free (result);
}

static void
test_has_large_enough_common_prefix ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "test");
    list = g_list_append (list, "tests");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_cmpstr ("test", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_has_large_enough_common_prefix_with_spaces_in_middle ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "Cpt J Yossarian r1");
    list = g_list_append (list, "Cpt J Yossarian a1");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_cmpstr ("Cpt J Yossarian", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_has_large_enough_common_prefix_with_punctuation_in_middle ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "Cpt-J_Yossarian r1");
    list = g_list_append (list, "Cpt-J_Yossarian a1");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_cmpstr ("Cpt-J_Yossarian", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_has_large_enough_common_prefix_with_punctuation_in_middle_and_extension ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "Cpt-J, Yossarian.xml");
    list = g_list_append (list, "Cpt-J, Yossarian.xsl");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_cmpstr ("Cpt-J, Yossarian", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_doesnt_have_large_enough_common_prefix ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "foo");
    list = g_list_append (list, "foob");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_null (actual);

    free_list_and_result (list, actual);
}

static void
test_doesnt_have_large_enough_common_prefix_completely_different_strings ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "this string really");
    list = g_list_append (list, "isn't the same as the other");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_null (actual);

    free_list_and_result (list, actual);
}

static void
test_doesnt_have_large_enough_common_prefix_first_character_differs ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "foo");
    list = g_list_append (list, "roo");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_null (actual);

    free_list_and_result (list, actual);
}

static void
test_doesnt_have_large_enough_common_prefix_first_character_differs_longer_string ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "fools");
    list = g_list_append (list, "rools");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_null (actual);

    free_list_and_result (list, actual);
}

static void
test_has_large_enough_common_prefix_until_extension_removed ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "tes.txt");
    list = g_list_append (list, "tes.tar");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_null (actual);

    free_list_and_result (list, actual);
}

static void
test_extension_is_removed ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "nau tilus.c");
    list = g_list_append (list, "nau tilus.cpp");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_cmpstr ("nau tilus", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_whitespace_is_removed ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "nautilus ");
    list = g_list_append (list, "nautilus two");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_cmpstr ("nautilus", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_whitespace_and_extension_are_removed ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "nautilus !£ $\"    foo.tar.gz");
    list = g_list_append (list, "nautilus !£ $\"  .lzma");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_cmpstr ("nautilus !£ $\"", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_punctuation_is_preserved (void)
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "nautilus (2018!£$%^&* ()_+-={}[ ];':@#~<>?,./\".mp4");
    list = g_list_append (list, "nautilus (2018!£$%^&* ()_+-={}[ ];':@#~<>?,./\".srt");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_cmpstr ("nautilus (2018!£$%^&* ()_+-={}[ ];':@#~<>?,./\"", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_unicode_on_outside ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "ӶtestӶ234");
    list = g_list_append (list, "ӶtestӶ1");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_cmpstr ("ӶtestӶ", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_unicode_on_inside ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "QQӶtestӶabb234");
    list = g_list_append (list, "QQӶtestӶabb1");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_cmpstr ("QQӶtestӶabb", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_unicode_whole_string ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "ǣȸʸͻͻΎΘΛ");
    list = g_list_append (list, "ǣȸʸͻͻΎΘ");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_cmpstr ("ǣȸʸͻͻΎΘ", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_unicode_extension ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "test.ǣȸʸͻͻΎΘΛ");
    list = g_list_append (list, "test.ǣȸʸͻͻΎΘ");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_cmpstr ("test", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_unicode_with_punctuation ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "ǣȸʸ- ͻͻΎΘ$%%^");
    list = g_list_append (list, "ǣȸʸ- ͻͻΎΘ$%%&");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_cmpstr ("ǣȸʸ- ͻͻΎΘ$%%", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_many_strings ()
{
    GList *list = NULL;
    char *actual;
    char *filename;
    int i;

    for (i = 0; i < 500; ++i)
    {
        filename = g_strdup_printf ("we are no longer the knights who say nii%d", i);
        list = g_list_append (list, filename);
    }

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_cmpstr ("we are no longer the knights who say nii", ==, actual);

    g_free (actual);
    g_list_free_full (list, g_free);
}

static void
test_many_strings_last_differs ()
{
    GList *list = NULL;
    char *actual;
    char *filename;
    int i;

    for (i = 0; i < 500; ++i)
    {
        filename = g_strdup_printf ("we are no longer the knights who say nii%d", i);

        if (i == 499)
        {
            filename[2] = 'X';
        }

        list = g_list_append (list, filename);
    }

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_null (actual);

    g_free (actual);
    g_list_free_full (list, g_free);
}

static void
test_many_strings_first_differs ()
{
    GList *list = NULL;
    char *actual;
    char *filename;
    int i;

    for (i = 0; i < 500; ++i)
    {
        filename = g_strdup_printf ("we are no longer the knights who say nii%d", i);

        if (i == 0)
        {
            filename[2] = 'X';
        }

        list = g_list_append (list, filename);
    }

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);
    g_assert_null (actual);

    g_free (actual);
    g_list_free_full (list, g_free);
}

static void
test_smaller_min_length_and_does_have_common_prefix ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "CA");
    list = g_list_append (list, "CB");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 1);
    g_assert_cmpstr ("C", ==, actual);

    free_list_and_result (list, actual);
}

static void
test_smaller_min_length_and_doesnt_have_common_prefix ()
{
    GList *list = NULL;
    char *actual;

    list = g_list_append (list, "CA");
    list = g_list_append (list, "BB");

    actual = nautilus_get_common_filename_prefix_from_filenames (list, 1);
    g_assert_null (actual);

    free_list_and_result (list, actual);
}


static void
setup_test_suite ()
{
    g_test_add_func ("/get-common-filename-prefix/1.0",
                     test_has_large_enough_common_prefix);
    g_test_add_func ("/get-common-filename-prefix/1.1",
                     test_has_large_enough_common_prefix_with_spaces_in_middle);
    g_test_add_func ("/get-common-filename-prefix/1.2",
                     test_has_large_enough_common_prefix_with_punctuation_in_middle);
    g_test_add_func ("/get-common-filename-prefix/1.3",
                     test_has_large_enough_common_prefix_with_punctuation_in_middle_and_extension);

    g_test_add_func ("/get-common-filename-prefix/2.0",
                     test_doesnt_have_large_enough_common_prefix);
    g_test_add_func ("/get-common-filename-prefix/2.1",
                     test_doesnt_have_large_enough_common_prefix_completely_different_strings);
    g_test_add_func ("/get-common-filename-prefix/2.2",
                     test_doesnt_have_large_enough_common_prefix_first_character_differs);
    g_test_add_func ("/get-common-filename-prefix/2.3",
                     test_doesnt_have_large_enough_common_prefix_first_character_differs_longer_string);

    g_test_add_func ("/get-common-filename-prefix/3.0",
                     test_has_large_enough_common_prefix_until_extension_removed);

    g_test_add_func ("/get-common-filename-prefix/4.0",
                     test_extension_is_removed);
    g_test_add_func ("/get-common-filename-prefix/4.1",
                     test_whitespace_is_removed);
    g_test_add_func ("/get-common-filename-prefix/4.2",
                     test_whitespace_and_extension_are_removed);
    g_test_add_func ("/get-common-filename-prefix/4.3",
                     test_punctuation_is_preserved);

    g_test_add_func ("/get-common-filename-prefix/5.0",
                     test_unicode_on_inside);
    g_test_add_func ("/get-common-filename-prefix/5.1",
                     test_unicode_on_outside);
    g_test_add_func ("/get-common-filename-prefix/5.2",
                     test_unicode_whole_string);
    g_test_add_func ("/get-common-filename-prefix/5.3",
                     test_unicode_extension);
    g_test_add_func ("/get-common-filename-prefix/5.4",
                     test_unicode_with_punctuation);

    g_test_add_func ("/get-common-filename-prefix/6.0",
                     test_many_strings);
    g_test_add_func ("/get-common-filename-prefix/6.1",
                     test_many_strings_last_differs);
    g_test_add_func ("/get-common-filename-prefix/6.2",
                     test_many_strings_first_differs);

    g_test_add_func ("/get-common-filename-prefix/7.0",
                     test_smaller_min_length_and_does_have_common_prefix);
    g_test_add_func ("/get-common-filename-prefix/7.1",
                     test_smaller_min_length_and_doesnt_have_common_prefix);
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
