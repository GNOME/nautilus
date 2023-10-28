#include <glib.h>

#include <nautilus-file.h>
#include <nautilus-file-utilities.h>
#include <nautilus-metadata.h>

static const char *TEST_FILE = "file:///etc/passwd";
static const char *KEY_BOOL = NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_REVERSED;
static const char *KEY_STR = NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY;

static void
test_file_metadata_bool_set_true (void)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (TEST_FILE);
    nautilus_file_set_boolean_metadata (file, KEY_BOOL, TRUE);
    g_assert_true (nautilus_file_get_boolean_metadata (file, KEY_BOOL, FALSE));
}

static void
test_file_metadata_bool_set_false (void)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (TEST_FILE);
    nautilus_file_set_boolean_metadata (file, KEY_BOOL, FALSE);
    g_assert_false (nautilus_file_get_boolean_metadata (file, KEY_BOOL, TRUE));
}

static void
test_file_metadata_bool_get_null (void)
{
    g_assert_true (nautilus_file_get_boolean_metadata (NULL, KEY_BOOL, TRUE));

    g_assert_false (nautilus_file_get_boolean_metadata (NULL, KEY_BOOL, FALSE));
}

static void
test_file_metadata_str_set (void)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (TEST_FILE);
    g_autofree char *metadata = NULL;

    nautilus_file_set_metadata (file, KEY_STR, "default", "value");
    metadata = nautilus_file_get_metadata (file, KEY_STR, "default");

    g_assert_true (strcmp (metadata, "value") == 0);
}

static void
test_file_metadata_str_get_null (void)
{
    g_autofree char *metadata = nautilus_file_get_metadata (NULL, KEY_STR, "default");
    g_assert_cmpstr (metadata, ==, "default");
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();

    g_test_add_func ("/file-metadata-set-bool/true",
                     test_file_metadata_bool_set_true);
    g_test_add_func ("/file-metadata-set-bool/false",
                     test_file_metadata_bool_set_false);
    g_test_add_func ("/file-metadata-set-bool/null",
                     test_file_metadata_bool_get_null);
    g_test_add_func ("/file-metadata-str-set/default",
                     test_file_metadata_str_set);
    g_test_add_func ("/file-metadata-str-set/null",
                     test_file_metadata_str_get_null);

    return g_test_run ();
}
