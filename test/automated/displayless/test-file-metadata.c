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
    for (guint i = 0;
         i < 10 && nautilus_file_get_boolean_metadata (file, KEY_BOOL, FALSE) != TRUE;
         i += 1)
    {
        g_main_context_iteration (NULL, TRUE);
    }
    g_assert_true (nautilus_file_get_boolean_metadata (file, KEY_BOOL, FALSE));
}

static void
test_file_metadata_bool_set_false (void)
{
    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (TEST_FILE);
    nautilus_file_set_boolean_metadata (file, KEY_BOOL, FALSE);
    for (guint i = 0;
         i < 10 && nautilus_file_get_boolean_metadata (file, KEY_BOOL, TRUE) != FALSE;
         i++)
    {
        g_main_context_iteration (NULL, TRUE);
    }
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
    gboolean metadata_was_set = FALSE;
    nautilus_file_set_metadata (file, KEY_STR, "default", "value");
    for (guint i = 0; i < 10; i += 1)
    {
        g_autofree char *metadata = nautilus_file_get_metadata (file, KEY_STR, "default");
        if (strcmp (metadata, "value") == 0)
        {
            metadata_was_set = TRUE;
            break;
        }
        g_main_context_iteration (NULL, TRUE);
    }
    g_assert_true (metadata_was_set);
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
    g_autoptr (GDBusProxy) proxy = NULL;

    g_test_init (&argc, &argv, NULL);
    g_test_set_nonfatal_assertions ();
    nautilus_ensure_extension_points ();

    proxy = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
                                           G_DBUS_PROXY_FLAGS_NONE,
                                           NULL,
                                           "org.gtk.vfs.Metadata",
                                           "/org/gtk/vfs/metadata",
                                           "org.gtk.vfs.Metadata",
                                           NULL,
                                           NULL);

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
