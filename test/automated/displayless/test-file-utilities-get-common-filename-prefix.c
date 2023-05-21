#include <glib.h>
#include <glib/gprintf.h>

#include "src/nautilus-file-utilities.h"


/* Helper function to abstract away the GList creation and make tests more readable. */
static GList *
make_list (char *str1,
           char *str2)
{
    GList *list = NULL;
    list = g_list_prepend (list, str1);
    list = g_list_prepend (list, str2);
    return list;
}


static void
test_has_large_enough_common_prefix (void)
{
    g_autoptr (GList) list = make_list ("test",
                                        "tests");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("test", ==, actual);
}

static void
test_has_large_enough_common_prefix_with_spaces_in_middle (void)
{
    g_autoptr (GList) list = make_list ("Cpt J Yossarian r1",
                                        "Cpt J Yossarian a1");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("Cpt J Yossarian", ==, actual);
}

static void
test_has_large_enough_common_prefix_with_punctuation_in_middle (void)
{
    g_autoptr (GList) list = make_list ("Cpt-J_Yossarian r1",
                                        "Cpt-J_Yossarian a1");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("Cpt-J_Yossarian", ==, actual);
}

static void
test_has_large_enough_common_prefix_with_punctuation_in_middle_and_extension (void)
{
    g_autoptr (GList) list = make_list ("Cpt-J, Yossarian.xml",
                                        "Cpt-J, Yossarian.xsl");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("Cpt-J, Yossarian", ==, actual);
}

static void
test_doesnt_have_large_enough_common_prefix (void)
{
    g_autoptr (GList) list = make_list ("foo",
                                        "foob");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_null (actual);
}

static void
test_doesnt_have_large_enough_common_prefix_completely_different_strings (void)
{
    g_autoptr (GList) list = make_list ("this string really",
                                        "isn't the same as the other");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_null (actual);
}

static void
test_doesnt_have_large_enough_common_prefix_first_character_differs (void)
{
    g_autoptr (GList) list = make_list ("foo",
                                        "roo");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_null (actual);
}

static void
test_doesnt_have_large_enough_common_prefix_first_character_differs_longer_string (void)
{
    g_autoptr (GList) list = make_list ("fools",
                                        "rools");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_null (actual);
}

static void
test_has_large_enough_common_prefix_until_extension_removed (void)
{
    g_autoptr (GList) list = make_list ("tes.txt",
                                        "tes.tar");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_null (actual);
}

static void
test_extension_is_removed (void)
{
    g_autoptr (GList) list = make_list ("nau tilus.c",
                                        "nau tilus.cpp");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("nau tilus", ==, actual);
}

static void
test_whitespace_is_removed (void)
{
    g_autoptr (GList) list = make_list ("nautilus ",
                                        "nautilus two");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("nautilus", ==, actual);
}

static void
test_whitespace_and_extension_are_removed (void)
{
    g_autoptr (GList) list = make_list ("nautilus !£ $\"    foo.tar.gz",
                                        "nautilus !£ $\"  .lzma");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("nautilus !£ $\"", ==, actual);
}

static void
test_punctuation_is_preserved (void)
{
    g_autoptr (GList) list = make_list (
        "nautilus (2018!£$%^&* ()_+-={}[ ];':@#~<>?,./\".mp4",
        "nautilus (2018!£$%^&* ()_+-={}[ ];':@#~<>?,./\".srt");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("nautilus (2018!£$%^&* ()_+-={}[ ];':@#~<>?,./\"", ==, actual);
}

static void
test_unicode_on_outside (void)
{
    g_autoptr (GList) list = make_list ("ӶtestӶ234",
                                        "ӶtestӶ1");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("ӶtestӶ", ==, actual);
}

static void
test_unicode_on_inside (void)
{
    g_autoptr (GList) list = make_list ("QQӶtestӶabb234",
                                        "QQӶtestӶabb1");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("QQӶtestӶabb", ==, actual);
}

static void
test_unicode_whole_string (void)
{
    g_autoptr (GList) list = make_list ("ǣȸʸͻͻΎΘΛ",
                                        "ǣȸʸͻͻΎΘ");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("ǣȸʸͻͻΎΘ", ==, actual);
}

static void
test_unicode_extension (void)
{
    g_autoptr (GList) list = make_list ("test.ǣȸʸͻͻΎΘΛ",
                                        "test.ǣȸʸͻͻΎΘ");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("test", ==, actual);
}

static void
test_unicode_with_punctuation (void)
{
    g_autoptr (GList) list = make_list ("ǣȸʸ- ͻͻΎΘ$%%^",
                                        "ǣȸʸ- ͻͻΎΘ$%%&");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("ǣȸʸ- ͻͻΎΘ$%%", ==, actual);
}

static void
test_many_strings (void)
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
test_many_strings_last_differs (void)
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
test_many_strings_first_differs (void)
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
test_smaller_min_length_and_does_have_common_prefix (void)
{
    g_autoptr (GList) list = make_list ("CA",
                                        "CB");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 1);

    g_assert_cmpstr ("C", ==, actual);
}

static void
test_smaller_min_length_and_doesnt_have_common_prefix (void)
{
    g_autoptr (GList) list = make_list ("CA",
                                        "BB");
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 1);

    g_assert_null (actual);
}


static void
setup_test_suite (void)
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
