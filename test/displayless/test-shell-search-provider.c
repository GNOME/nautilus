#define G_LOG_DOMAIN "test-shell-search-provider"

#include <config.h>

#include <gio/gio.h>

#include <nautilus-shell-search-provider.h>

static const gchar *SEARCH_PROVIDER_OBJECT_PATH =
    "/org/gnome/Nautilus" PROFILE "/SearchProvider";
static const gchar *SEARCH_PROVIDER_INTERFACE =
    "org.gnome.Shell.SearchProvider2";

/* The GDBusServer must run in a separate thread with its own main context:
 * g_dbus_connection_new_for_address_sync() blocks without iterating the main
 * context, so a server running on the same thread would never accept the
 * incoming connection.
 *
 * Likewise, exported objects (e.g. the provider skeleton) must be registered
 * from the server thread: g_dbus_connection_call_sync() only iterates a
 * private main context while waiting, while method calls on registered
 * objects are dispatched as idles in the thread-default context of the thread
 * that registered them. Registering from the main thread would schedule the
 * dispatch on a context that is never iterated and the call would time out.
 */
typedef struct
{
    GMainContext *context;
    GMainLoop *loop;
    GDBusServer *server;
    GDBusConnection *server_connection;

    GMutex mutex;
    GCond cond;
    gboolean server_ready;
    gchar *client_address;

    /* Provider to export on the new connection (set before starting the
     * server thread, used from the new-connection callback).
     */
    NautilusShellSearchProvider *provider;
    gboolean provider_registered;
    GError *register_error;
} TestServerData;

typedef struct
{
    GThread *server_thread;
    TestServerData *server;
    GDBusConnection *client_connection;
} TestDbusEnvironment;

static gboolean
test_server_new_connection_cb (GDBusServer     *server,
                               GDBusConnection *connection,
                               gpointer         user_data)
{
    TestServerData *data = user_data;
    g_autoptr (GError) error = NULL;

    g_mutex_lock (&data->mutex);
    data->server_connection = g_object_ref (connection);
    if (data->provider != NULL)
    {
        data->provider_registered =
            nautilus_shell_search_provider_register (data->provider,
                                                     connection,
                                                     &error);
        data->register_error = g_steal_pointer (&error);
    }
    g_cond_broadcast (&data->cond);
    g_mutex_unlock (&data->mutex);

    /* Claiming the connection is required for the server to start
     * processing incoming messages on it.
     */
    return TRUE;
}

static gpointer
test_server_thread (gpointer user_data)
{
    TestServerData *data = user_data;
    g_autofree gchar *guid = g_dbus_generate_guid ();
    g_autoptr (GError) error = NULL;

    data->context = g_main_context_new ();
    g_main_context_push_thread_default (data->context);

    data->server = g_dbus_server_new_sync ("unix:tmpdir=/tmp",
                                           G_DBUS_SERVER_FLAGS_NONE,
                                           guid,
                                           NULL, /* observer */
                                           NULL, /* cancellable */
                                           &error);
    g_assert_no_error (error);
    g_assert_nonnull (data->server);

    g_signal_connect (data->server, "new-connection",
                      G_CALLBACK (test_server_new_connection_cb), data);
    g_dbus_server_start (data->server);

    data->loop = g_main_loop_new (data->context, FALSE);

    g_mutex_lock (&data->mutex);
    data->client_address = g_strdup (g_dbus_server_get_client_address (data->server));
    data->server_ready = TRUE;
    g_cond_broadcast (&data->cond);
    g_mutex_unlock (&data->mutex);

    g_main_loop_run (data->loop);

    g_main_context_pop_thread_default (data->context);
    return NULL;
}

static TestDbusEnvironment *
test_dbus_environment_new (NautilusShellSearchProvider *provider)
{
    g_autoptr (GError) error = NULL;
    TestDbusEnvironment *env = g_new0 (TestDbusEnvironment, 1);

    env->server = g_new0 (TestServerData, 1);
    g_mutex_init (&env->server->mutex);
    g_cond_init (&env->server->cond);
    env->server->provider = provider;

    env->server_thread = g_thread_new ("test-dbus-server",
                                       test_server_thread,
                                       env->server);

    /* Wait for the server to be ready and grab the client address. */
    g_mutex_lock (&env->server->mutex);
    while (!env->server->server_ready)
    {
        g_cond_wait (&env->server->cond, &env->server->mutex);
    }
    g_autofree gchar *address = g_strdup (env->server->client_address);
    g_mutex_unlock (&env->server->mutex);

    env->client_connection = g_dbus_connection_new_for_address_sync (address,
                                                                     G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                                                     NULL, /* observer */
                                                                     NULL, /* cancellable */
                                                                     &error);
    g_assert_no_error (error);
    g_assert_nonnull (env->client_connection);

    /* Wait for the server to accept the connection and export the provider. */
    g_mutex_lock (&env->server->mutex);
    while (env->server->server_connection == NULL)
    {
        g_cond_wait (&env->server->cond, &env->server->mutex);
    }
    gboolean registered = env->server->provider_registered;
    g_autoptr (GError) register_error =
        env->server->register_error != NULL ? g_error_copy (env->server->register_error) : NULL;
    g_mutex_unlock (&env->server->mutex);

    g_assert_no_error (register_error);
    g_assert_true (registered);

    return env;
}

static void
test_dbus_environment_free (TestDbusEnvironment *env)
{
    g_main_loop_quit (env->server->loop);
    g_thread_join (env->server_thread);

    g_clear_object (&env->client_connection);
    g_clear_object (&env->server->server_connection);
    g_clear_object (&env->server->server);
    g_main_loop_unref (env->server->loop);
    g_main_context_unref (env->server->context);
    g_clear_error (&env->server->register_error);
    g_mutex_clear (&env->server->mutex);
    g_cond_clear (&env->server->cond);
    g_free (env->server->client_address);
    g_free (env->server);
    g_free (env);
}

typedef struct
{
    NautilusShellSearchProvider *provider;
    GDBusConnection *connection;
    GMutex *mutex;
    GCond *cond;
    gboolean done;
    gboolean result;
    GError *error;
} RegisterProviderData;

static gboolean
register_provider_in_server_thread (gpointer user_data)
{
    RegisterProviderData *data = user_data;
    g_autoptr (GError) error = NULL;

    data->result = nautilus_shell_search_provider_register (data->provider,
                                                            data->connection,
                                                            &error);

    g_mutex_lock (data->mutex);
    data->error = g_steal_pointer (&error);
    data->done = TRUE;
    g_cond_broadcast (data->cond);
    g_mutex_unlock (data->mutex);

    return G_SOURCE_REMOVE;
}

/* Re-register the provider from the server thread, so that method calls are
 * dispatched on the server's main context (see the comment above).
 */
static gboolean
test_dbus_register_provider (TestDbusEnvironment          *env,
                             NautilusShellSearchProvider  *provider,
                             GError                      **error)
{
    RegisterProviderData data =
    {
        .provider = provider,
        .connection = env->server->server_connection,
        .mutex = &env->server->mutex,
        .cond = &env->server->cond,
    };

    g_main_context_invoke (env->server->context,
                           register_provider_in_server_thread,
                           &data);

    g_mutex_lock (&env->server->mutex);
    while (!data.done)
    {
        g_cond_wait (&env->server->cond, &env->server->mutex);
    }
    gboolean result = data.result;
    g_autoptr (GError) local_error = data.error != NULL ? g_error_copy (data.error) : NULL;
    g_mutex_unlock (&env->server->mutex);

    if (error != NULL)
    {
        *error = g_steal_pointer (&local_error);
    }

    return result;
}

static GVariant *
test_call_method (GDBusConnection    *connection,
                  const gchar        *method_name,
                  GVariant           *parameters,
                  const GVariantType *reply_type)
{
    g_autoptr (GError) error = NULL;
    g_autoptr (GVariant) result = NULL;

    /* bus_name is NULL because this is a peer-to-peer connection. */
    result = g_dbus_connection_call_sync (connection,
                                          NULL,
                                          SEARCH_PROVIDER_OBJECT_PATH,
                                          SEARCH_PROVIDER_INTERFACE,
                                          method_name,
                                          parameters,
                                          reply_type,
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);
    g_assert_no_error (error);
    g_assert_nonnull (result);

    return g_steal_pointer (&result);
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
    g_autoptr (NautilusShellSearchProvider) provider = NULL;
    g_autoptr (GError) error = NULL;
    g_autoptr (GVariant) result = NULL;
    TestDbusEnvironment *env;

    provider = nautilus_shell_search_provider_new ();
    env = test_dbus_environment_new (provider);
    /* The provider is exported automatically on the new connection. */

    nautilus_shell_search_provider_unregister (provider);

    /* The provider should be able to register again after unregistering. */
    g_assert_true (test_dbus_register_provider (env, provider, &error));
    g_assert_no_error (error);

    nautilus_shell_search_provider_unregister (provider);

    /* Calls to the provider should fail once it is no longer exported. */
    result = g_dbus_connection_call_sync (env->client_connection,
                                          NULL,
                                          SEARCH_PROVIDER_OBJECT_PATH,
                                          SEARCH_PROVIDER_INTERFACE,
                                          "GetInitialResultSet",
                                          g_variant_new ("(as)", NULL),
                                          G_VARIANT_TYPE ("(as)"),
                                          G_DBUS_CALL_FLAGS_NONE,
                                          -1,
                                          NULL,
                                          &error);
    g_assert_null (result);
    g_assert_nonnull (error);
    g_clear_error (&error);

    test_dbus_environment_free (env);
}

static void
test_get_initial_result_set_single_char (void)
{
    g_autoptr (NautilusShellSearchProvider) provider = NULL;
    g_autoptr (GVariant) result = NULL;
    g_autoptr (GVariant) results = NULL;
    TestDbusEnvironment *env;
    const gchar *terms[] = { "d", NULL };

    provider = nautilus_shell_search_provider_new ();
    env = test_dbus_environment_new (provider);

    /* A single character is too short to search for, so the provider
     * should return an empty result set right away.
     */
    result = test_call_method (env->client_connection,
                               "GetInitialResultSet",
                               g_variant_new ("(^as)", terms),
                               G_VARIANT_TYPE ("(as)"));
    results = g_variant_get_child_value (result, 0);
    g_assert_cmpint (g_variant_n_children (results), ==, 0);

    test_dbus_environment_free (env);
}

static void
test_get_subsearch_result_set_single_char (void)
{
    g_autoptr (NautilusShellSearchProvider) provider = NULL;
    g_autoptr (GVariant) result = NULL;
    g_autoptr (GVariant) results = NULL;
    TestDbusEnvironment *env;
    const gchar *previous_results[] = { "file:///tmp/foo", NULL };
    const gchar *terms[] = { "d", NULL };

    provider = nautilus_shell_search_provider_new ();
    env = test_dbus_environment_new (provider);

    result = test_call_method (env->client_connection,
                               "GetSubsearchResultSet",
                               g_variant_new ("(^as^as)", previous_results, terms),
                               G_VARIANT_TYPE ("(as)"));
    results = g_variant_get_child_value (result, 0);
    g_assert_cmpint (g_variant_n_children (results), ==, 0);

    test_dbus_environment_free (env);
}

static void
test_get_result_metas_empty (void)
{
    g_autoptr (NautilusShellSearchProvider) provider = NULL;
    g_autoptr (GVariant) result = NULL;
    g_autoptr (GVariant) metas = NULL;
    TestDbusEnvironment *env;
    const gchar *results[] = { NULL };

    provider = nautilus_shell_search_provider_new ();
    env = test_dbus_environment_new (provider);

    /* Requesting metas for no results should return an empty array. */
    result = test_call_method (env->client_connection,
                               "GetResultMetas",
                               g_variant_new ("(^as)", results),
                               G_VARIANT_TYPE ("(aa{sv})"));
    metas = g_variant_get_child_value (result, 0);
    g_assert_cmpint (g_variant_n_children (metas), ==, 0);

    test_dbus_environment_free (env);
}

int
main (int   argc,
      char *argv[])
{
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

    return g_test_run ();
}
