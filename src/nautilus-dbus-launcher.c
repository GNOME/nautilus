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
    gboolean ping_on_creation;
} NautilusDBusLauncherData;

struct _NautilusDBusLauncher
{
    GObject parent;
    GCancellable *cancellable;

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

static void
on_nautilus_dbus_launcher_ping_finished   (GObject      *source_object,
                                           GAsyncResult *res,
                                           gpointer      user_data)
{
    NautilusDBusLauncherData *data = user_data;
    g_autoptr (GError) error = NULL;

    g_dbus_proxy_call_finish (G_DBUS_PROXY (source_object), res, &error);

    if (error != NULL)
    {
        data->error = g_strdup (error->message);
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
                           NULL,
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
    else if (data->ping_on_creation)
    {
        g_dbus_proxy_call (data->proxy,
                           "org.freedesktop.DBus.Peer.Ping", NULL,
                           G_DBUS_CALL_FLAGS_NONE, G_MAXINT, launcher->cancellable,
                           on_nautilus_dbus_launcher_ping_finished, data);
    }
}

static void
nautilus_dbus_launcher_create_proxy (NautilusDBusLauncherData *data,
                                     const gchar              *name,
                                     const gchar              *object_path,
                                     const gchar              *interface)
{
    g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                              G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION,
                              NULL,
                              name,
                              object_path,
                              interface,
                              NULL,
                              on_nautilus_dbus_proxy_ready,
                              data);
}

gboolean nautilus_dbus_launcher_is_available (NautilusDBusLauncher   *self,
                                              NautilusDBusLauncherApp app)
{
    return self->data[app]->error == NULL && self->data[app]->proxy != NULL;
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
                                  NautilusDBusLauncherApp  app,
                                  gboolean                 ping_on_creation)
{
    NautilusDBusLauncherData *data;
    g_assert_true (app == self->last_app_initialized + 1);

    data = g_new0 (NautilusDBusLauncherData, 1);
    data->proxy = NULL;
    data->error = NULL;
    data->ping_on_creation = ping_on_creation;

    self->data[app] = data;
    self->last_app_initialized = app;
}

static void
nautilus_dbus_launcher_init (NautilusDBusLauncher *self)
{
    self->cancellable = g_cancellable_new ();

    nautilus_dbus_launcher_data_init (self, NAUTILUS_DBUS_LAUNCHER_SETTINGS, FALSE);
    nautilus_dbus_launcher_data_init (self, NAUTILUS_DBUS_LAUNCHER_DISKS, TRUE);
    nautilus_dbus_launcher_data_init (self, NAUTILUS_DBUS_LAUNCHER_CONSOLE, TRUE);

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
