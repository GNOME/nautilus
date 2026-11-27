#define G_LOG_DOMAIN "test-sidebar"

#include <nautilus-application.h>
#include <nautilus-file-utilities.h>
#include <nautilus-global-preferences.h>
#include <nautilus-resources.h>
#include <nautilus-sidebar.h>
#include <nautilus-tag-manager.h>

#include <test-utilities.h>

#include <glib.h>
#include <gtk/gtk.h>

static void
test_sidebar_set_location (void)
{
    g_autoptr (NautilusSidebar) sidebar = NAUTILUS_PLACES_SIDEBAR (nautilus_sidebar_new ());
    g_autoptr (GFile) home_location = g_file_new_for_path (g_get_home_dir ());

    /* Need to sink the object */
    g_object_ref_sink (sidebar);

    nautilus_sidebar_set_location (sidebar, home_location);

    g_autoptr (GFile) view_location = nautilus_sidebar_get_location (sidebar);

    /* Only locations on the sidebar will return non-null */
    g_assert_true (g_file_equal (view_location, home_location));
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (NautilusTagManager) tag_manager = NULL;

    nautilus_test_gtk_init (&argc, &argv,
                            NAUTILUS_TEST_INIT_ISOLATE |
                            NAUTILUS_TEST_INIT_REGISTER_RESOURCE |
                            NAUTILUS_TEST_INIT_EXTENSIONS |
                            NAUTILUS_TEST_INIT_PREFS);

    tag_manager = nautilus_tag_manager_new_dummy ();

    g_autoptr (NautilusApplication) app = nautilus_application_new ();

    g_test_add_func ("/sidebar/set_location",
                     test_sidebar_set_location);

    return g_test_run ();
}
