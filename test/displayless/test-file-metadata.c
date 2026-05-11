#include <glib.h>

#include <nautilus-file.h>
#include <nautilus-file-utilities.h>
#include <nautilus-metadata.h>

static const char *TEST_FILE = "file:///etc/passwd";
static const char *KEY_STR = NAUTILUS_METADATA_KEY_ICON_VIEW_SORT_BY;

static void
test_file_metadata_str_set (void)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (TEST_FILE);
    nautilus_file_set_metadata (file, KEY_STR, "default", "value");
    const char *metadata = nautilus_file_get_metadata (file, KEY_STR, "default");
    g_assert_cmpstr (metadata, ==, "value");
}

int
main (int   argc,
      char *argv[])
{
    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();

    g_test_add_func ("/file-metadata-str-set/default",
                     test_file_metadata_str_set);

    return g_test_run ();
}
