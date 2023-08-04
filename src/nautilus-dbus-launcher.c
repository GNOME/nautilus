/*
 * Copyright (C) 2022 Corey Berla <corey@berla.me>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "nautilus-dbus-launcher.h"

#include <glib/gi18n.h>

#include "nautilus-file.h"
#include "nautilus-ui-utilities.h"


typedef struct
{
    GDBusProxy *proxy;
    gchar *error;
    GCancellable *cancellable;
    const char *name;
    gboolean available;
} NautilusDBusLauncherData;

struct _NautilusDBusLauncher
{
    GObject parent;

    GCancellable *cancellable;
    GDBusProxy *proxy;
    NautilusDBusLauncherApp last_app_initialized;
    NautilusDBusLauncherData *data[NAUTILUS_DBUS_LAUNCHER_N_APPS];
};

G_DEFINE_TYPE (NautilusDBusLauncher, nautilus_dbus_launcher, G_TYPE_OBJECT)

static NautilusDBusLauncher *launcher = NULL;

static void
on_nautilus_dbus_launcher_call_finished   (GObject      *source_object,
                                           GAsyncResult *res,
                                           gpointer      user_data)
{
    GtkWindow *window = user_data;
    g_autoptr (GError) error = NULL;
    g_autofree char *message = NULL;

    g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);
    if (error != NULL)
    {
        g_warning ("Error calling proxy %s", error->message);
        message = g_strdup_printf (_("Details: %s"), error->message);
        show_dialog (_("There was an error launching the app."),
                     message,
                     window,
                     GTK_MESSAGE_ERROR);
    }
}

void
nautilus_dbus_launcher_call (NautilusDBusLauncher    *self,
                             NautilusDBusLauncherApp  app,
                             const gchar             *method_name,
                             GVariant                *parameters,
                             GtkWindow               *window)
{
    if (self->data[app]->proxy != NULL)
    {
        g_dbus_proxy_call (self->data[app]->proxy,
                           method_name,
                           parameters,
                           G_DBUS_CALL_FLAGS_NONE,
                           G_MAXINT,
                           self->cancellable,
                           on_nautilus_dbus_launcher_call_finished,
                           window);
    }
    else if (window != NULL)
    {
        show_dialog (_("There was an error launching the app."),
                     _("Details: The proxy has not been created."),
                     window,
                     GTK_MESSAGE_ERROR);
    }
}

static void
on_nautilus_dbus_proxy_ready (GObject      *source_object,
                              GAsyncResult *res,
                              gpointer      user_data)
{
    NautilusDBusLauncherData *data = user_data;
    g_autoptr (GError) error = NULL;

    data->proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

    if (error != NULL)
    {
        g_warning ("Error creating proxy %s", error->message);
        data->error = g_strdup (error->message);
    }
}

static void
nautilus_dbus_launcher_create_proxy (NautilusDBusLauncherData *data,
                                     const gchar              *name,
                                     const gchar              *object_path,
                                     const gchar              *interface)
{
    data->name = name;

    g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                              G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION,
                              NULL,
                              name,
                              object_path,
                              interface,
                              data->cancellable,
                              on_nautilus_dbus_proxy_ready,
                              data);
}

gboolean
nautilus_dbus_launcher_is_available (NautilusDBusLauncher    *self,
                                     NautilusDBusLauncherApp  app)
{
    return (self->data[app]->available && self->data[app]->proxy != NULL);
}

static void
activatable_names_received (GObject      *object,
                            GAsyncResult *res,
                            gpointer      user_data)
{
    NautilusDBusLauncher *self = user_data;
    g_auto (GStrv) activatable_names = NULL;
    g_autoptr (GError) error = NULL;
    g_autoptr (GVariant) names = NULL;

    names = g_dbus_proxy_call_finish (G_DBUS_PROXY (object), res, &error);
    if (error == NULL)
    {
        g_variant_get (names, "(^as)", &activatable_names);
    }
    else
    {
        g_warning ("Error receiving activatable names: %s", error->message);
    }

    for (guint i = 1; i < NAUTILUS_DBUS_LAUNCHER_N_APPS; i++)
    {
        if (activatable_names != NULL)
        {
            self->data[i]->available = g_strv_contains ((const char * const *) activatable_names,
                                                        self->data[i]->name);
        }
        else
        {
            self->data[i]->available = FALSE;
        }
    }
}

static void
get_activatable_names (NautilusDBusLauncher *self)
{
    g_dbus_proxy_call (self->proxy, "ListActivatableNames", NULL,
                       G_DBUS_CALL_FLAGS_NONE, G_MAXINT,
                       self->cancellable, activatable_names_received, self);
}

static void
activatable_services_changed (GDBusConnection *connection,
                              const gchar     *sender_name,
                              const gchar     *object_path,
                              const gchar     *interface_name,
                              const gchar     *signal_name,
                              GVariant        *parameters,
                              gpointer         user_data)
{
    NautilusDBusLauncher *self = user_data;

    get_activatable_names (self);
}

static void
proxy_ready (GObject      *object,
             GAsyncResult *res,
             gpointer      user_data)
{
    NautilusDBusLauncher *self = user_data;
    GDBusConnection *connection;

    self->proxy = g_dbus_proxy_new_for_bus_finish (res, NULL);
    get_activatable_names (self);
    connection = g_dbus_proxy_get_connection (self->proxy);
    g_dbus_connection_signal_subscribe (connection,
                                        "org.freedesktop.DBus",
                                        "org.freedesktop.DBus",
                                        "ActivatableServicesChanged",
                                        "/org/freedesktop/DBus",
                                        NULL, G_DBUS_SIGNAL_FLAGS_NONE,
                                        activatable_services_changed, self, NULL);
}

NautilusDBusLauncher *
nautilus_dbus_launcher_get (void)
{
    return launcher;
}

NautilusDBusLauncher *
nautilus_dbus_launcher_new (void)
{
    if (launcher != NULL)
    {
        return g_object_ref (launcher);
    }
    launcher = g_object_new (NAUTILUS_TYPE_DBUS_LAUNCHER, NULL);
    g_object_add_weak_pointer (G_OBJECT (launcher), (gpointer) & launcher);

    return launcher;
}

static void
nautilus_dbus_launcher_finalize (GObject *object)
{
    NautilusDBusLauncher *self = NAUTILUS_DBUS_LAUNCHER (object);

    g_cancellable_cancel (self->cancellable);
    g_clear_object (&self->cancellable);
    g_clear_object (&self->proxy);

    for (gint i = 1; i <= self->last_app_initialized; i++)
    {
        g_clear_object (&self->data[i]->proxy);
        g_free (self->data[i]->error);
        g_free (self->data[i]);
    }

    G_OBJECT_CLASS (nautilus_dbus_launcher_parent_class)->finalize (object);
}

static void
nautilus_dbus_launcher_class_init (NautilusDBusLauncherClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nautilus_dbus_launcher_finalize;
}

static void
nautilus_dbus_launcher_data_init (NautilusDBusLauncher    *self,
                                  NautilusDBusLauncherApp  app)
{
    NautilusDBusLauncherData *data;
    g_assert_true (app == self->last_app_initialized + 1);

    data = g_new0 (NautilusDBusLauncherData, 1);
    data->proxy = NULL;
    data->error = NULL;

    data->cancellable = self->cancellable;
    self->data[app] = data;
    self->last_app_initialized = app;
}

static void
nautilus_dbus_launcher_init (NautilusDBusLauncher *self)
{
    self->cancellable = g_cancellable_new ();

    nautilus_dbus_launcher_data_init (self, NAUTILUS_DBUS_LAUNCHER_SETTINGS);
    nautilus_dbus_launcher_data_init (self, NAUTILUS_DBUS_LAUNCHER_DISKS);
    nautilus_dbus_launcher_data_init (self, NAUTILUS_DBUS_LAUNCHER_CONSOLE);

    g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, NULL,
                              "org.freedesktop.DBus",
                              "/org/freedesktop/DBus",
                              "org.freedesktop.DBus",
                              self->cancellable, proxy_ready, self);

    nautilus_dbus_launcher_create_proxy (self->data[NAUTILUS_DBUS_LAUNCHER_SETTINGS],
                                         "org.gnome.Settings", "/org/gnome/Settings",
                                         "org.gtk.Actions");

    nautilus_dbus_launcher_create_proxy (self->data[NAUTILUS_DBUS_LAUNCHER_DISKS],
                                         "org.gnome.DiskUtility", "/org/gnome/DiskUtility",
                                         "org.gtk.Application");

    nautilus_dbus_launcher_create_proxy (self->data[NAUTILUS_DBUS_LAUNCHER_CONSOLE],
                                         "org.gnome.Console", "/org/gnome/Console",
                                         "org.freedesktop.Application");
}
