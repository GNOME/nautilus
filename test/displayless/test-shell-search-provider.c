#define G_LOG_DOMAIN "test-shell-search-provider"

#include <config.h>

#include <gio/gio.h>

#include <nautilus-shell-search-provider.h>

static const gchar *SEARCH_PROVIDER_OBJECT_PATH = "/org/gnome/Nautilus" PROFILE "/SearchProvider";
static const gchar *SEARCH_PROVIDER_INTERFACE = "org.gnome.Shell.SearchProvider2";

/* Private session bus shared by all tests, started in main(). */
static GTestDBus *test_bus;

typedef struct
{
    GMainLoop *loop;
    GVariant *result;
    GError *error;
} TestCallData;

static void
test_call_done (GObject      *source_object,
                GAsyncResult *result,
                gpointer      user_data)
{
    TestCallData *data = user_data;

    data->result = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source_object),
                                                  result,
                                                  &data->error);
    g_main_loop_quit (data->loop);
}

/* Send a method call and iterate the main context until the reply arrives.
 *
 * The call must be asynchronous: g_dbus_connection_call_sync() only iterates a
 * private main context while waiting, so method calls dispatched to the
 * provider (which lives on this main context) would never be processed.
 */
static GVariant *
test_call_method (GDBusConnection     *connection,
                  const gchar         *method_name,
                  GVariant            *parameters,
                  const GVariantType  *reply_type,
                  GError             **error)
{
    TestCallData data = { 0 };

    data.loop = g_main_loop_new (NULL, FALSE);

    g_dbus_connection_call (connection,
                            g_dbus_connection_get_unique_name (connection),
                            SEARCH_PROVIDER_OBJECT_PATH,
                            SEARCH_PROVIDER_INTERFACE,
                            method_name,
                            parameters,
                            reply_type,
                            G_DBUS_CALL_FLAGS_NONE,
                            -1,
                            NULL,
                            test_call_done,
                            &data);

    g_main_loop_run (data.loop);
    g_main_loop_unref (data.loop);

    if (error != NULL)
    {
        *error = g_steal_pointer (&data.error);
    }
    else
    {
        g_assert_no_error (data.error);
        g_clear_error (&data.error);
    }

    return g_steal_pointer (&data.result);
}

/* Connect to the private session bus started by GTestDBus in main() and export
 * the provider on it. The provider and the test share the same connection, so
 * calls are addressed to its own unique name.
 */
static GDBusConnection *
test_setup_provider (NautilusShellSearchProvider *provider)
{
    g_autoptr (GError) error = NULL;
    GDBusConnection *connection;

    connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
    g_assert_no_error (error);
    g_assert_nonnull (connection);

    g_assert_true (nautilus_shell_search_provider_register (provider, connection, &error));
    g_assert_no_error (error);

    return connection;
}

static void
test_search_provider_new (void)
{
    g_autoptr (NautilusShellSearchProvider) provider = nautilus_shell_search_provider_new ();

    g_assert_nonnull (provider);
    g_assert_true (NAUTILUS_IS_SHELL_SEARCH_PROVIDER (provider));
}

static void
test_search_provider_register (void)
{
    g_autoptr (NautilusShellSearchProvider) provider = nautilus_shell_search_provider_new ();
    g_autoptr (GDBusConnection) connection = test_setup_provider (provider);
    g_autoptr (GError) error = NULL;
    g_autoptr (GVariant) result = NULL;
    const gchar *terms[] = { "d", NULL };

    nautilus_shell_search_provider_unregister (provider);

    /* The provider should be able to register again after unregistering. */
    g_assert_true (nautilus_shell_search_provider_register (provider, connection, &error));
    g_assert_no_error (error);

    nautilus_shell_search_provider_unregister (provider);

    /* Calls to the provider should fail once it is no longer exported. */
    result = test_call_method (connection,
                               "GetInitialResultSet",
                               g_variant_new ("(^as)", terms),
                               G_VARIANT_TYPE ("(as)"),
                               &error);
    g_assert_null (result);
    g_assert_nonnull (error);
}

static void
test_get_initial_result_set_single_char (void)
{
    g_autoptr (NautilusShellSearchProvider) provider = nautilus_shell_search_provider_new ();
    g_autoptr (GDBusConnection) connection = test_setup_provider (provider);
    g_autoptr (GVariant) result = NULL;
    g_autoptr (GVariant) results = NULL;
    const gchar *terms[] = { "d", NULL };

    /* A single character is too short to search for, so the provider
     * should return an empty result set right away.
     */
    result = test_call_method (connection,
                               "GetInitialResultSet",
                               g_variant_new ("(^as)", terms),
                               G_VARIANT_TYPE ("(as)"),
                               NULL);
    results = g_variant_get_child_value (result, 0);
    g_assert_cmpint (g_variant_n_children (results), ==, 0);
}

static void
test_get_subsearch_result_set_single_char (void)
{
    g_autoptr (NautilusShellSearchProvider) provider = nautilus_shell_search_provider_new ();
    g_autoptr (GDBusConnection) connection = test_setup_provider (provider);
    g_autoptr (GVariant) result = NULL;
    g_autoptr (GVariant) results = NULL;
    const gchar *previous_results[] = { "file:///tmp/foo", NULL };
    const gchar *terms[] = { "d", NULL };

    result = test_call_method (connection,
                               "GetSubsearchResultSet",
                               g_variant_new ("(^as^as)", previous_results, terms),
                               G_VARIANT_TYPE ("(as)"),
                               NULL);
    results = g_variant_get_child_value (result, 0);
    g_assert_cmpint (g_variant_n_children (results), ==, 0);
}

static void
test_get_result_metas_empty (void)
{
    g_autoptr (NautilusShellSearchProvider) provider = nautilus_shell_search_provider_new ();
    g_autoptr (GDBusConnection) connection = test_setup_provider (provider);
    g_autoptr (GVariant) result = NULL;
    g_autoptr (GVariant) metas = NULL;
    const gchar *results[] = { NULL };

    /* Requesting metas for no results should return an empty array. */
    result = test_call_method (connection,
                               "GetResultMetas",
                               g_variant_new ("(^as)", results),
                               G_VARIANT_TYPE ("(aa{sv})"),
                               NULL);
    metas = g_variant_get_child_value (result, 0);
    g_assert_cmpint (g_variant_n_children (metas), ==, 0);
}

int
main (int   argc,
      char *argv[])
{
    int ret;

    g_test_init (&argc, &argv, NULL);

    g_test_add_func ("/search-provider/new",
                     test_search_provider_new);
    g_test_add_func ("/search-provider/register",
                     test_search_provider_register);
    g_test_add_func ("/search-provider/get-initial-result-set/single-char",
                     test_get_initial_result_set_single_char);
    g_test_add_func ("/search-provider/get-subsearch-result-set/single-char",
                     test_get_subsearch_result_set_single_char);
    g_test_add_func ("/search-provider/get-result-metas/empty",
                     test_get_result_metas_empty);

    /* Run the tests against a private session bus, so they never touch the
     * user's session and can freely export and call the search provider.
     */
    test_bus = g_test_dbus_new (G_TEST_DBUS_NONE);
    g_test_dbus_up (test_bus);

    ret = g_test_run ();

    g_test_dbus_down (test_bus);
    g_object_unref (test_bus);

    return ret;
}
