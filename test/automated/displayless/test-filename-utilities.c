#include <glib.h>

#include <nautilus-filename-utilities.h>


static void
test_filename_extension (void)
{
    g_assert_cmpstr (nautilus_filename_get_extension ("abc"), ==, "");
    g_assert_cmpstr (nautilus_filename_get_extension ("abcdef"), ==, "");
    g_assert_cmpstr (nautilus_filename_get_extension ("abc.def"), ==, ".def");
    g_assert_cmpstr (nautilus_filename_get_extension ("abc.def ghi"), ==, "");
    g_assert_cmpstr (nautilus_filename_get_extension ("abc.def.ghi"), ==, ".ghi");
}

static void
test_filename_extension_end_position (void)
{
    const char *filename_with_ext = "abc.def.ghi";
    const char *extension = nautilus_filename_get_extension (filename_with_ext);
    g_assert_true (extension == filename_with_ext + strlen ("abc.def"));

    const char *filename_without_ext = "abcdefghi";
    const char *non_extension = nautilus_filename_get_extension (filename_without_ext);
    g_assert_true (non_extension == filename_without_ext + strlen (filename_without_ext));
}

static void
test_filename_extension_with_tar (void)
{
    g_assert_cmpstr (nautilus_filename_get_extension ("abc.gz"), ==, ".gz");
    g_assert_cmpstr (nautilus_filename_get_extension ("abc.def.gz"), ==, ".gz");
    g_assert_cmpstr (nautilus_filename_get_extension ("abc.tar.gz"), ==, ".tar.gz");
}

static void
test_filename_create_file_copy (void)
{
#define ASSERT_DUPLICATION_NAME(ORIGINAL, DUPLICATE) \
        { \
            g_autofree char *duplicated = nautilus_filename_for_copy (ORIGINAL, 1, -1, FALSE); \
            g_assert_cmpstr (duplicated, ==, DUPLICATE); \
        }

    /* test the next duplicate name generator */
    ASSERT_DUPLICATION_NAME (" (Copy)", " (Copy 2)");
    ASSERT_DUPLICATION_NAME ("foo", "foo (Copy)");
    ASSERT_DUPLICATION_NAME (".bashrc", ".bashrc (Copy)");
    ASSERT_DUPLICATION_NAME (".foo.txt", ".foo (Copy).txt");
    ASSERT_DUPLICATION_NAME ("foo foo", "foo foo (Copy)");
    ASSERT_DUPLICATION_NAME ("foo.txt", "foo (Copy).txt");
    ASSERT_DUPLICATION_NAME ("foo foo.txt", "foo foo (Copy).txt");
    ASSERT_DUPLICATION_NAME ("foo foo.txt txt", "foo foo.txt txt (Copy)");
    ASSERT_DUPLICATION_NAME ("foo...txt", "foo.. (Copy).txt");
    ASSERT_DUPLICATION_NAME ("foo...", "foo... (Copy)");
    ASSERT_DUPLICATION_NAME ("foo. (Copy)", "foo. (Copy 2)");
    ASSERT_DUPLICATION_NAME ("foo (Copy)", "foo (Copy 2)");
    ASSERT_DUPLICATION_NAME ("foo (Copy).txt", "foo (Copy 2).txt");
    ASSERT_DUPLICATION_NAME ("foo (Copy 2)", "foo (Copy 3)");
    ASSERT_DUPLICATION_NAME ("foo (Copy 2).txt", "foo (Copy 3).txt");
    ASSERT_DUPLICATION_NAME ("foo foo (Copy 2).txt", "foo foo (Copy 3).txt");
    ASSERT_DUPLICATION_NAME ("foo (Copy 13)", "foo (Copy 14)");
    ASSERT_DUPLICATION_NAME ("foo foo (Copy 100000000000000).txt", "foo foo (Copy 100000000000001).txt");

#undef ASSERT_DUPLICATION_NAME
}

static void
test_filename_create_dir_copy (void)
{
#define ASSERT_DUPLICATION_NAME(ORIGINAL, DUPLICATE) \
        { \
            g_autofree char *duplicated = nautilus_filename_for_copy (ORIGINAL, 1, -1, TRUE); \
            g_assert_cmpstr (duplicated, ==, DUPLICATE); \
        }

    ASSERT_DUPLICATION_NAME ("dir.with.dots", "dir.with.dots (Copy)");
    ASSERT_DUPLICATION_NAME ("dir (Copy).dir", "dir (Copy).dir (Copy)");

#undef ASSERT_DUPLICATION_NAME
}

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

    g_test_add_func ("/filename-extension/default",
                     test_filename_extension);
    g_test_add_func ("/filename-extension/end-position",
                     test_filename_extension_end_position);
    g_test_add_func ("/filename-extension/tar",
                     test_filename_extension_with_tar);
    g_test_add_func ("/filename-create-copy/file",
                     test_filename_create_file_copy);
    g_test_add_func ("/filename-create-copy/dir",
                     test_filename_create_dir_copy);
    g_test_add_func ("/file-name-shortening-with-base/needed",
                     test_filename_shortening_with_base);
    g_test_add_func ("/file-name-shortening-with-base/not-needed",
                     test_filename_shortening_with_base_not_needed);

    return g_test_run ();
}
