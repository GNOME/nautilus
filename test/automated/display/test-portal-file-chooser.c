/*
 * Copyright (C) 2026 Khalid Abu Shawarib.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Test suite for Portal File Chooser D-Bus implementation
 */

#define G_LOG_DOMAIN "test-portal-file-chooser"

#include <nautilus-application.h>
#include <nautilus-file-chooser.h>
#include <nautilus-file-utilities.h>
#include <nautilus-filename-validator.h>
#include <nautilus-resources.h>
#include <nautilus-tag-manager.h>
#include <nautilus-toolbar.h>

#include <test-utilities.h>

#define BUS_NAME APPLICATION_ID
#define PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"
#define PORTAL_INTERFACE "org.freedesktop.impl.portal.FileChooser"
#define DBUS_DELAY_MS 2500

typedef struct
{
    GVariant *results;
    GError *error;
    gboolean invocation_completed;
    GMainLoop *loop;
} PortalTestData;

static void
portal_test_data_free (PortalTestData *data)
{
    g_clear_pointer (&data->results, g_variant_unref);
    g_clear_error (&data->error);
    g_clear_pointer (&data->loop, g_main_loop_unref);
    g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (PortalTestData, portal_test_data_free)

static GVariant *
build_file_filters (const char *first_filter_name,
                    GStrv first_patterns,
                    ...)
{
    va_list args;
    g_autoptr (GVariantBuilder) filters_builder =
        g_variant_builder_new (G_VARIANT_TYPE ("a(sa(us))"));
    const char *filter_name;
    GStrv patterns;

    va_start (args, first_patterns);

    filter_name = first_filter_name;
    patterns = first_patterns;

    while (filter_name != NULL)
    {
        g_autoptr (GVariantBuilder) filter_patterns_builder =
            g_variant_builder_new (G_VARIANT_TYPE ("a(us)"));

        for (int i = 0; patterns != NULL && patterns[i] != NULL; i++)
        {
            g_variant_builder_add (filter_patterns_builder, "(us)", 0u, patterns[i]);
        }

        g_variant_builder_add (filters_builder, "(s@a(us))",
                               filter_name,
                               g_variant_builder_end (filter_patterns_builder));

        /* Next argument is the next filter name (or NULL to terminate) */
        filter_name = va_arg (args, const char *);
        if (filter_name != NULL)
        {
            patterns = va_arg (args, GStrv);
        }
    }

    va_end (args);

    return g_variant_builder_end (filters_builder);
}

static GVariant *
build_save_file_args (const char *parent_window,
                      const char *title,
                      const char *accept_label,
                      gboolean   *modal,
                      const char *current_folder_path,
                      const char *current_name,
                      GVariant   *filters)
{
    g_autoptr (GVariantBuilder) options_builder = NULL;
    GVariant *options = NULL;

    options_builder = g_variant_builder_new (G_VARIANT_TYPE ("a{sv}"));

    if (modal != NULL)
    {
        g_variant_builder_add (options_builder, "{sv}",
                               "modal",
                               g_variant_new_boolean (*modal));
    }

    if (accept_label != NULL)
    {
        g_variant_builder_add (options_builder, "{sv}",
                               "accept_label", g_variant_new_string (accept_label));
    }

    if (current_folder_path != NULL)
    {
        g_variant_builder_add (options_builder, "{sv}",
                               "current_folder",
                               g_variant_new_bytestring (current_folder_path));
    }

    if (current_name != NULL)
    {
        g_variant_builder_add (options_builder, "{sv}",
                               "current_name",
                               g_variant_new_string (current_name));
    }

    if (filters != NULL)
    {
        g_variant_builder_add (options_builder, "{sv}",
                               "filters",
                               filters);
    }

    options = g_variant_builder_end (options_builder);

    return g_variant_new ("(osss@a{sv})",
                          "/org/gnome/Nautilus/Devel",
                          "org.gnome.Nautilus.Devel",
                          parent_window,
                          title,
                          options);
}

static void
save_file_callback (GObject      *connection,
                    GAsyncResult *res,
                    gpointer      user_data)
{
    PortalTestData *data = user_data;
    g_autoptr (GError) error = NULL;
    g_autoptr (GVariant) result = NULL;

    result = g_dbus_connection_call_finish (G_DBUS_CONNECTION (connection), res, &data->error);

    data->results = g_steal_pointer (&result);
    data->invocation_completed = TRUE;
    g_main_loop_quit (data->loop);
}

static guint accept_source_id;

static void
trigger_accept_action (gpointer user_data)
{
    NautilusFileChooser *self = NAUTILUS_FILE_CHOOSER (user_data);

    accept_source_id = 0;

    gtk_test_widget_wait_for_draw (GTK_WIDGET (self));

    /* Simulate clicking the accept button to complete the portal request */
    gtk_widget_activate_action (GTK_WIDGET (self), "chooser.accept", NULL);
}

static void
file_chooser_window_shown_callback (GtkWidget *widget,
                                    gpointer   user_data)
{
    NautilusFileChooser *self = NAUTILUS_FILE_CHOOSER (widget);

    g_assert_true (gtk_widget_is_visible (GTK_WIDGET (self)));
    g_assert_true (gtk_widget_get_mapped (GTK_WIDGET (self)));

    g_clear_handle_id (&accept_source_id, g_source_remove);

    gtk_test_widget_wait_for_draw (GTK_WIDGET (self));

    accept_source_id = g_timeout_add_once (250, trigger_accept_action, self);
}

static void
window_added_callback (NautilusApplication *application,
                       NautilusWindow      *window,
                       gpointer             user_data)
{
    NautilusFileChooser *file_chooser = NAUTILUS_FILE_CHOOSER (window);

    g_signal_connect (file_chooser, "show", G_CALLBACK (file_chooser_window_shown_callback), NULL);
}

static void
test_portal_save_file (void)
{
    g_autoptr (GDBusConnection) connection = NULL;
    GError *error = NULL;

    /* Get the session D-Bus connection */
    connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
    if (error != NULL)
    {
        g_critical ("Could not get D-Bus connection: %s", error->message);

        return;
    }

    g_autoptr (GFile) tmp_dir = g_file_new_for_path (test_get_tmp_dir ());
    g_autofree char *current_folder_path = g_file_get_path (tmp_dir);
    g_autoptr (PortalTestData) test_data = g_new0 (PortalTestData, 1);
    GVariant *args_var = build_save_file_args ("",
                                               "Save Test File",
                                               "Save",
                                               NULL,
                                               current_folder_path,
                                               "test-file.txt",
                                               NULL);

    test_data->loop = g_main_loop_new (NULL, FALSE);
    g_signal_connect (g_application_get_default (),
                      "window-added", G_CALLBACK (window_added_callback), NULL);

    /* Invoke the SaveFile method on the portal */
    g_dbus_connection_call (connection,
                            BUS_NAME,
                            PORTAL_OBJECT_PATH,
                            PORTAL_INTERFACE,
                            "SaveFile",
                            args_var,
                            G_VARIANT_TYPE ("(ua{sv})"),
                            G_DBUS_CALL_FLAGS_NONE,
                            DBUS_DELAY_MS,
                            NULL,
                            save_file_callback,
                            test_data);

    g_main_loop_run (test_data->loop);

    g_assert_true (test_data->invocation_completed);
    g_assert_no_error (test_data->error);
    g_assert_nonnull (test_data->results);

    guint response = 0;
    g_autoptr (GVariant) result_dict = NULL;
    g_autofree gchar *result_printed = NULL;

    /* TODO: The test does not verify results for now */
    g_variant_get (test_data->results, "(u@a{sv})", &response, &result_dict);
    result_printed = g_variant_print (result_dict, FALSE);
    g_test_message ("SaveFile returned response code: %u", response);
    g_message ("Result: %s", result_printed);

    test_clear_tmp_dir ();
}

static void
test_portal_save_file_with_options (void)
{
    GError *error = NULL;
    g_autoptr (GDBusConnection) connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);

    g_assert_no_error (error);
    g_assert_nonnull (connection);

    g_autoptr (GFile) tmp_dir = g_file_new_for_path (test_get_tmp_dir ());
    g_autofree char *current_folder_path = g_file_get_path (tmp_dir);
    GStrv text_patterns = ((char *[]){ "*.txt", "*.text", NULL });
    GStrv all_patterns = ((char *[]){ "*", NULL });
    GVariant *filters = build_file_filters ("Text Files", text_patterns,
                                            "All Files", all_patterns,
                                            NULL);
    GVariant *args_var = build_save_file_args ("",
                                               "Save with Filters",
                                               "Save",
                                               FALSE,
                                               current_folder_path,
                                               "document.txt",
                                               filters);
    g_autoptr (PortalTestData) test_data = g_new0 (PortalTestData, 1);

    test_data->loop = g_main_loop_new (NULL, FALSE);
    g_signal_connect (g_application_get_default (),
                      "window-added", G_CALLBACK (window_added_callback), NULL);

    /* Invoke SaveFile with filters on the portal */
    g_dbus_connection_call (connection,
                            BUS_NAME,
                            PORTAL_OBJECT_PATH,
                            PORTAL_INTERFACE,
                            "SaveFile",
                            args_var,
                            G_VARIANT_TYPE ("(ua{sv})"),
                            G_DBUS_CALL_FLAGS_NONE,
                            DBUS_DELAY_MS,
                            NULL,
                            save_file_callback,
                            test_data);

    g_main_loop_run (test_data->loop);

    g_assert_true (test_data->invocation_completed);
    g_assert_no_error (test_data->error);
    g_assert_nonnull (test_data->results);

    /* TODO: The test does not verify results for now */
    guint response = 0;
    g_autoptr (GVariant) result_dict = NULL;
    g_autofree gchar *result_printed = NULL;

    g_variant_get (test_data->results, "(u@a{sv})", &response, &result_dict);
    result_printed = g_variant_print (result_dict, FALSE);
    g_test_message ("SaveFile with filters returned response code: %u", response);
    g_message ("Result: %s", result_printed);

    test_clear_tmp_dir ();
}

int
main (int   argc,
      char *argv[])
{
    g_autoptr (GDBusConnection) connection = NULL;
    g_autoptr (GError) error = NULL;

    if (nautilus_application_is_sandboxed ())
    {
        /* TODO: Fix tracker-sandbox getting stuck when running this test,
         * which is required for flatpak. */
        return 77;
    }

    /* Register the portal on the D-Bus session bus */
    connection = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, &error);
    if (error != NULL)
    {
        g_critical ("Could not get D-Bus connection: %s", error->message);

        return 1;
    }

    /* Check if an application with the same APP_ID is already running.
     * query the bus via org.freedesktop.DBus NameHasOwner. */
    g_autoptr (GVariant) name_has_owner_result = NULL;
    gboolean name_has_owner = FALSE;

    name_has_owner_result = g_dbus_connection_call_sync (connection,
                                                         "org.freedesktop.DBus",
                                                         "/org/freedesktop/DBus",
                                                         "org.freedesktop.DBus",
                                                         "NameHasOwner",
                                                         g_variant_new ("(s)", BUS_NAME),
                                                         G_VARIANT_TYPE ("(b)"),
                                                         G_DBUS_CALL_FLAGS_NONE,
                                                         500,
                                                         NULL,
                                                         &error);

    if (error != NULL)
    {
        g_critical ("Could not get check owner of Bus name: %s", error->message);

        return 1;
    }

    g_variant_get (name_has_owner_result, "(b)", &name_has_owner);

    if (name_has_owner)
    {
        g_critical ("The application '" BUS_NAME "' is already running on D-Bus. "
                    "This test suite needs an isolated application instance.");

        return 1;
    }

    g_test_init (&argc, &argv, NULL);

    nautilus_register_resource ();

    g_autoptr (NautilusTagManager) tag_manager = nautilus_tag_manager_new_dummy ();
    g_autoptr (GApplication) app = G_APPLICATION (nautilus_application_new ());

    g_application_hold (app);
    g_assert_true (g_application_register (app, NULL, NULL));
    g_application_activate (app);

    g_test_add_func ("/portal/file-chooser/save-file/basic",
                     test_portal_save_file);
    g_test_add_func ("/portal/file-chooser/save-file/with-options",
                     test_portal_save_file_with_options);

    int result = g_test_run ();

    g_application_quit (app);

    return result;
}
