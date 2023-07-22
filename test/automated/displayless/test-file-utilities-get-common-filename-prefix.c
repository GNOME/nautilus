#include <glib.h>
#include <glib/gprintf.h>

#include "src/nautilus-file-utilities.h"


static void
test_has_large_enough_common_prefix (void)
{
    const char *list[] =
    {
        "test",
        "tests",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("test", ==, actual);
}

static void
test_has_large_enough_common_prefix_with_spaces_in_middle (void)
{
    const char *list[] =
    {
        "Cpt J Yossarian r1",
        "Cpt J Yossarian a1",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("Cpt J Yossarian", ==, actual);
}

static void
test_has_large_enough_common_prefix_with_punctuation_in_middle (void)
{
    const char *list[] =
    {
        "Cpt-J_Yossarian r1",
        "Cpt-J_Yossarian a1",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("Cpt-J_Yossarian", ==, actual);
}

static void
test_has_large_enough_common_prefix_with_punctuation_in_middle_and_extension (void)
{
    const char *list[] =
    {
        "Cpt-J, Yossarian.xml",
        "Cpt-J, Yossarian.xsl",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("Cpt-J, Yossarian", ==, actual);
}

static void
test_doesnt_have_large_enough_common_prefix (void)
{
    const char *list[] =
    {
        "foo",
        "foob",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_null (actual);
}

static void
test_doesnt_have_large_enough_common_prefix_completely_different_strings (void)
{
    const char *list[] =
    {
        "this string really",
        "isn't the same as the other",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_null (actual);
}

static void
test_doesnt_have_large_enough_common_prefix_first_character_differs (void)
{
    const char *list[] =
    {
        "foo",
        "roo",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_null (actual);
}

static void
test_doesnt_have_large_enough_common_prefix_first_character_differs_longer_string (void)
{
    const char *list[] =
    {
        "fools",
        "rools",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_null (actual);
}

static void
test_has_large_enough_common_prefix_until_extension_removed (void)
{
    const char *list[] =
    {
        "tes.txt",
        "tes.tar",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_null (actual);
}

static void
test_extension_is_removed (void)
{
    const char *list[] =
    {
        "nau tilus.c",
        "nau tilus.cpp",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("nau tilus", ==, actual);
}

static void
test_whitespace_is_removed (void)
{
    const char *list[] =
    {
        "nautilus ",
        "nautilus two",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("nautilus", ==, actual);
}

static void
test_whitespace_and_extension_are_removed (void)
{
    const char *list[] =
    {
        "nautilus !£ $\"    foo.tar.gz",
        "nautilus !£ $\"  .lzma",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("nautilus !£ $\"", ==, actual);
}

static void
test_punctuation_is_preserved (void)
{
    const char *list[] =
    {
        "nautilus (2018!£$%^&* ()_+-={}[ ];':@#~<>?,./\".mp4",
        "nautilus (2018!£$%^&* ()_+-={}[ ];':@#~<>?,./\".srt",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("nautilus (2018!£$%^&* ()_+-={}[ ];':@#~<>?,./\"", ==, actual);
}

static void
test_unicode_on_outside (void)
{
    const char *list[] =
    {
        "ӶtestӶ234",
        "ӶtestӶ1",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("ӶtestӶ", ==, actual);
}

static void
test_unicode_on_inside (void)
{
    const char *list[] =
    {
        "QQӶtestӶabb234",
        "QQӶtestӶabb1",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("QQӶtestӶabb", ==, actual);
}

static void
test_unicode_whole_string (void)
{
    const char *list[] =
    {
        "ǣȸʸͻͻΎΘΛ",
        "ǣȸʸͻͻΎΘ",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("ǣȸʸͻͻΎΘ", ==, actual);
}

static void
test_unicode_extension (void)
{
    const char *list[] =
    {
        "test.ǣȸʸͻͻΎΘΛ",
        "test.ǣȸʸͻͻΎΘ",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("test", ==, actual);
}

static void
test_unicode_with_punctuation (void)
{
    const char *list[] =
    {
        "ǣȸʸ- ͻͻΎΘ$%%^",
        "ǣȸʸ- ͻͻΎΘ$%%&",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 4);

    g_assert_cmpstr ("ǣȸʸ- ͻͻΎΘ$%%", ==, actual);
}

static void
test_many_strings (void)
{
    const int n_strings = 500;
    g_auto (GStrv) list = g_new (char *, n_strings + 1);
    g_autofree char *actual = NULL;

    for (int i = 0; i < n_strings; ++i)
    {
        list[i] = g_strdup_printf ("we are no longer the knights who say nii%d", i);
    }
    list[n_strings] = NULL;

    actual = nautilus_get_common_filename_prefix_from_filenames ((const char * const *) list, 4);
    g_assert_cmpstr ("we are no longer the knights who say nii", ==, actual);
}

static void
test_many_strings_last_differs (void)
{
    const int n_strings = 500;
    g_auto (GStrv) list = g_new (char *, n_strings + 1);
    g_autofree char *actual = NULL;
    char *filename;

    for (int i = 0; i < n_strings; ++i)
    {
        filename = g_strdup_printf ("we are no longer the knights who say nii%d", i);

        if (i == n_strings - 1)
        {
            filename[2] = 'X';
        }

        list[i] = filename;
    }
    list[n_strings] = NULL;

    actual = nautilus_get_common_filename_prefix_from_filenames ((const char * const *) list, 4);
    g_assert_null (actual);
}

static void
test_many_strings_first_differs (void)
{
    const int n_strings = 500;
    g_auto (GStrv) list = g_new (char *, n_strings + 1);
    g_autofree char *actual = NULL;
    char *filename;

    for (int i = 0; i < n_strings; ++i)
    {
        filename = g_strdup_printf ("we are no longer the knights who say nii%d", i);

        if (i == 0)
        {
            filename[2] = 'X';
        }

        list[i] = filename;
    }
    list[n_strings] = NULL;

    actual = nautilus_get_common_filename_prefix_from_filenames ((const char * const *) list, 4);
    g_assert_null (actual);
}

static void
test_smaller_min_length_and_does_have_common_prefix (void)
{
    const char *list[] =
    {
        "CA",
        "CB",
        NULL
    };
    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames (list, 1);

    g_assert_cmpstr ("C", ==, actual);
}

static void
test_smaller_min_length_and_doesnt_have_common_prefix (void)
{
    const char *list[] =
    {
        "CA",
        "BB",
        NULL
    };
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
