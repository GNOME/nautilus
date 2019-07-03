/*
 * nautilus-previewer: nautilus previewer DBus wrapper
 *
 * Copyright (C) 2011, Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "config.h"

#include "nautilus-previewer.h"

#define DEBUG_FLAG NAUTILUS_DEBUG_PREVIEWER
#include "nautilus-debug.h"

#include <gio/gio.h>

#define PREVIEWER_DBUS_NAME "org.gnome.NautilusPreviewer"
#define PREVIEWER_DBUS_IFACE "org.gnome.NautilusPreviewer"
#define PREVIEWER2_DBUS_IFACE "org.gnome.NautilusPreviewer2"
#define PREVIEWER_DBUS_PATH "/org/gnome/NautilusPreviewer"

typedef struct {
    GDBusCallFlags flags;
    gchar *method_name;
    GVariant *v1_variant;
} MethodCallData;

static void
method_call_data_free (MethodCallData *data)
{
    g_free (data->method_name);
    if (data->v1_variant != NULL)
    {
        g_variant_unref (data->v1_variant);
    }
    g_free (data);
}

static void
previewer_method_ready_cb (GObject      *source,
                           GAsyncResult *res,
                           gpointer      user_data)
{
    MethodCallData *data = user_data;
    GError *error = NULL;

    g_dbus_connection_call_finish (G_DBUS_CONNECTION (source),
                                   res, &error);

    if (error != NULL)
    {
        DEBUG ("Unable to call %s on NautilusPreviewer: %s",
               data->method_name, error->message);
        g_error_free (error);
    }

    method_call_data_free (data);
}

static void
previewer2_method_ready_cb (GObject      *source,
                            GAsyncResult *res,
                            gpointer      user_data)
{
    MethodCallData *data = user_data;
    GDBusConnection *connection = G_DBUS_CONNECTION (source);
    GError *error = NULL;

    g_dbus_connection_call_finish (connection, res, &error);

    if (error == NULL)
    {
        method_call_data_free (data);
        return;
    }

    DEBUG ("Unable to call %s on NautilusPreviewer: %s",
           data->method_name, error->message);
    g_clear_error (&error);

    g_dbus_connection_call (connection,
                            PREVIEWER_DBUS_NAME,
                            PREVIEWER_DBUS_PATH,
                            PREVIEWER_DBUS_IFACE,
                            data->method_name,
                            data->v1_variant,
                            NULL,
                            data->flags,
                            -1,
                            NULL,
                            previewer_method_ready_cb,
                            data);
}

static void
call_method_on_interfaces (const gchar *method_name,
                           GVariant *v2_variant,
                           GVariant *v1_variant,
                           GDBusCallFlags flags)
{
    GDBusConnection *connection = g_application_get_dbus_connection (g_application_get_default ());
    MethodCallData *data = g_new0 (MethodCallData, 1);

    data->flags = flags;
    data->method_name = g_strdup (method_name);
    if (v1_variant != NULL)
    {
        data->v1_variant = g_variant_ref_sink (v1_variant);
    }

    /* Try the v2 interface first */
    g_dbus_connection_call (connection,
                            PREVIEWER_DBUS_NAME,
                            PREVIEWER_DBUS_PATH,
                            PREVIEWER2_DBUS_IFACE,
                            method_name,
                            v2_variant,
                            NULL,
                            flags,
                            -1,
                            NULL,
                            previewer2_method_ready_cb,
                            data);
}

void
nautilus_previewer_call_show_file (const gchar *uri,
                                   const gchar *window_handle,
                                   guint        xid,
                                   gboolean     close_if_already_visible)
{
    call_method_on_interfaces ("ShowFile",
                               g_variant_new ("(ssb)",
                                              uri, window_handle, close_if_already_visible),
                               g_variant_new ("(sib)",
                                              uri, xid, close_if_already_visible),
                               G_DBUS_CALL_FLAGS_NONE);
}

void
nautilus_previewer_call_close (void)
{
    /* don't autostart the previewer if it's not running */
    call_method_on_interfaces ("Close",
                               NULL,
                               NULL,
                               G_DBUS_CALL_FLAGS_NO_AUTO_START);
}
