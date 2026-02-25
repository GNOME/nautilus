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
test_empty_file_list (void)
{
    g_autofree char *actual = nautilus_get_common_filename_prefix (NULL, 1);

    g_assert_null (actual);
}

static void
test_files (void)
{
    const GStrv names = (char *[])
    {
        "AB",
        "AC",
        NULL
    };
    g_autolist (NautilusFile) files = get_files_for_names (names);
    g_autofree gchar *prefix = nautilus_get_common_filename_prefix (files, 1);

    g_assert_nonnull (prefix);
    g_assert_cmpstr (prefix, ==, "A");
}

static void
test_directories (void)
{
    const GStrv names = (char *[])
    {
        "AB/",
        "AC/",
        NULL
    };
    g_autolist (NautilusFile) files = get_files_for_names (names);
    g_autofree gchar *prefix = nautilus_get_common_filename_prefix (files, 1);

    g_assert_nonnull (prefix);
    g_assert_cmpstr (prefix, ==, "A");
}

static void
test_files_and_directories (void)
{
    const GStrv names = (char *[])
    {
        "AB",
        "AC/",
        NULL
    };
    g_autolist (NautilusFile) files = get_files_for_names (names);
    g_autofree gchar *prefix = nautilus_get_common_filename_prefix (files, 1);

    g_assert_nonnull (prefix);
    g_assert_cmpstr (prefix, ==, "A");
}

static void
setup_test_suite (void)
{
    /* nautilus_get_common_filename_prefix_from_filenames () */
    g_test_add_func ("/get-common-filename-prefix/string-list/6.0",
                     test_many_strings);
    g_test_add_func ("/get-common-filename-prefix/string-list/6.1",
                     test_many_strings_last_differs);
    g_test_add_func ("/get-common-filename-prefix/string-list/6.2",
                     test_many_strings_first_differs);

    /* nautilus_get_common_filename_prefix () */
    g_test_add_func ("/get-common-filename-prefix/file-list/1.0",
                     test_empty_file_list);

    g_test_add_func ("/get-common-filename-prefix/file-list/2.0",
                     test_files);

    g_test_add_func ("/get-common-filename-prefix/file-list/3.0",
                     test_directories);

    g_test_add_func ("/get-common-filename-prefix/file-list/4.0",
                     test_files_and_directories);
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
    setup_test_suite ();

    return g_test_run ();
}
