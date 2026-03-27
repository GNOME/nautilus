#include <glib.h>
#include <glib/gprintf.h>

#include "nautilus-file.h"
#include "nautilus-file-private.h"
#include "src/nautilus-file-utilities.h"

#include "test-utilities.h"

typedef struct
{
    const char *a;
    const char *b;
    const char *prefix;
    int min_length;
} TwoStringPrefixTest;

static void
test_common_prefix_two_strings (void)
{
    const TwoStringPrefixTest tests[] =
    {
        { "test", "tests", "test", 4 },
        { "Cpt-J_Yossarian r1", "Cpt-J_Yossarian a1", "Cpt-J_Yossarian", 4 },
        { "Cpt-J, Yossarian.xml", "Cpt-J, Yossarian.xsl", "Cpt-J, Yossarian", 4 },
        { "foo", "foob", NULL, 4 },
        { "this string really", "isn't the same as the other", NULL, 4 },
        { "foo", "roo", NULL, 4 },
        { "fools", "rools", NULL, 4 },
        { "tes.txt", "tes.tar", NULL, 4 },
        { "nau tilus.c", "nau tilus.cpp", "nau tilus", 4 },
        { "nautilus ", "nautilus two", "nautilus", 4 },
        { "nautilus !£ $\"    foo.tar.gz", "nautilus !£ $\"  .lzma", "nautilus !£ $\"", 4 },
        {
            "nautilus (2018!£$%^&* ()_+-={}[ ];':@#~<>?,./\".mp4",
            "nautilus (2018!£$%^&* ()_+-={}[ ];':@#~<>?,./\".srt",
            "nautilus (2018!£$%^&* ()_+-={}[ ];':@#~<>?,./\"",
            4
        },
        { "ӶtestӶ234", "ӶtestӶ1", "ӶtestӶ", 4 },
        { "QQӶtestӶabb234", "QQӶtestӶabb1", "QQӶtestӶabb", 4 },
        { "ǣȸʸͻͻΎΘΛ", "ǣȸʸͻͻΎΘ", "ǣȸʸͻͻΎΘ", 4 },
        { "test.ǣȸʸͻͻΎΘΛ", "test.ǣȸʸͻͻΎΘ", "test", 4 },
        { "ǣȸʸ- ͻͻΎΘ$%%^", "ǣȸʸ- ͻͻΎΘ$%%&", "ǣȸʸ- ͻͻΎΘ$%%", 4 },
        { "CA", "CB", "C", 1 },
        { "CA", "BB", NULL, 1 },
        { NULL, NULL, NULL, 0 }
    };

    for (guint i = 0; tests[i].a != NULL; i += 1)
    {
        const TwoStringPrefixTest *test = &tests[i];
        const char * const filenames[3] = { test->a, test->b, NULL };
        g_autofree char *actual =
            nautilus_get_common_filename_prefix_from_filenames (filenames, test->min_length);

        g_assert_cmpstr (actual, ==, test->prefix);
    }
}

static void
test_swapped_char (GStrv list,
                   guint position)
{
    char swapped_char = list[position][2];

    list[position][2] = 'X';

    g_autofree char *actual = nautilus_get_common_filename_prefix_from_filenames ((const char * const *) list, 4);
    g_assert_null (actual);

    list[position][2] = swapped_char;
}

static void
test_common_prefix_multiple_strings (void)
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

    /* make first string differ */
    test_swapped_char (list, 0);

    /* make last string differ */
    test_swapped_char (list, n_strings - 1);
}

static GList *
get_files_for_names (const GStrv files_names)
{
    gsize len = g_strv_length (files_names);
    GList *files = NULL;

    for (gsize i = 0; i < len; i++)
    {
        gchar *name = files_names[i];
        g_autofree gchar *uri = g_strconcat ("file:///", name, NULL);
        NautilusFile *file = nautilus_file_get_by_uri (uri);

        if (name[strlen (name) - 1] == G_DIR_SEPARATOR)
        {
            /* Info about the file type being a directory need to be set manually. */
            file->details->type = G_FILE_TYPE_DIRECTORY;
        }

        files = g_list_prepend (files, file);
    }

    return files;
}

static void
test_common_prefix_filename (void)
{
    const TwoStringPrefixTest tests[] =
    {
        { "AC", "AB", "A", 1 },
        { "AC", "AB/", "A", 1 },
        { "AC/", "AB/", "A", 1 },
        { NULL, NULL, NULL, 0 }
    };

    /* Test empty list */
    g_assert_null (nautilus_get_common_filename_prefix (NULL, 1));

    for (guint i = 0; tests[i].a != NULL; i += 1)
    {
        const TwoStringPrefixTest *test = &tests[i];
        const char * const filenames[3] = { test->a, test->b, NULL };
        g_autolist (NautilusFile) files = get_files_for_names ((GStrv) filenames);
        g_autofree gchar *actual = nautilus_get_common_filename_prefix (files, 1);

        g_assert_cmpstr (actual, ==, test->prefix);
    }
}

static int empty_tests = 0;

static void
empty_check_callback (gpointer callback_data,
                      gboolean is_empty)
{
    gboolean expected = GPOINTER_TO_INT (callback_data);

    g_assert_cmpint (expected, ==, is_empty);
    empty_tests -= 1;
}

static void
test_directory_empty_check (void)
{
    const GStrv hierarchy = (char *[])
    {
        "empty_check/",
        "empty_check/empty_dir/",
        "empty_check/non_empty_dir/",
        "empty_check/non_empty_dir/file1",
        "empty_check/nested_dir/",
        "empty_check/nested_dir/dir/",
        NULL
    };
    g_autoptr (GFile) location = g_file_new_build_filename (test_get_tmp_dir (),
                                                            "empty_check", NULL);
    g_autoptr (GFile) empty = g_file_get_child (location, "empty_dir");
    g_autoptr (GFile) non_empty = g_file_get_child (location, "non_empty_dir");
    g_autoptr (GFile) nested = g_file_get_child (location, "nested_dir");

    file_hierarchy_create (hierarchy, "");

    empty_tests = 3;
    nautilus_is_directory_empty (empty, empty_check_callback, GINT_TO_POINTER (TRUE));
    nautilus_is_directory_empty (non_empty, empty_check_callback, GINT_TO_POINTER (FALSE));
    nautilus_is_directory_empty (nested, empty_check_callback, GINT_TO_POINTER (FALSE));

    ITER_CONTEXT_WHILE (empty_tests > 0);

    test_clear_tmp_dir ();
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);
    g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=747907");
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();

    g_test_add_func ("/common-prefix/two-strings",
                     test_common_prefix_two_strings);
    g_test_add_func ("/common-prefix/multiple-strings",
                     test_common_prefix_multiple_strings);
    g_test_add_func ("/common-prefix/filename",
                     test_common_prefix_filename);

    g_test_add_func ("/directory-empty-check",
                     test_directory_empty_check);

    return g_test_run ();
}
