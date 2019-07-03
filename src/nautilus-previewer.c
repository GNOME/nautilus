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
#define PREVIEWER2_DBUS_IFACE "org.gnome.NautilusPreviewer2"
#define PREVIEWER_DBUS_PATH "/org/gnome/NautilusPreviewer"

static void
previewer2_method_ready_cb (GObject      *source,
                            GAsyncResult *res,
                            gpointer      user_data)
{
    GDBusConnection *connection = G_DBUS_CONNECTION (source);
    g_autoptr(GError) error = NULL;

    g_dbus_connection_call_finish (connection, res, &error);

    if (error != NULL)
    {
        DEBUG ("Unable to call method on NautilusPreviewer: %s", error->message);
    }
}

void
nautilus_previewer_call_show_file (const gchar *uri,
                                   const gchar *window_handle,
                                   guint        xid,
                                   gboolean     close_if_already_visible)
{
    GDBusConnection *connection = g_application_get_dbus_connection (g_application_get_default ());
    GVariant *variant;

    variant = g_variant_new ("(ssb)",
                             uri, window_handle, close_if_already_visible);

    g_dbus_connection_call (connection,
                            PREVIEWER_DBUS_NAME,
                            PREVIEWER_DBUS_PATH,
                            PREVIEWER2_DBUS_IFACE,
                            "ShowFile",
                            variant,
                            NULL,
                            G_DBUS_CALL_FLAGS_NONE,
                            -1,
                            NULL,
                            previewer2_method_ready_cb,
                            NULL);
}

void
nautilus_previewer_call_close (void)
{
    GDBusConnection *connection = g_application_get_dbus_connection (g_application_get_default ());

    /* don't autostart the previewer if it's not running */
    g_dbus_connection_call (connection,
                            PREVIEWER_DBUS_NAME,
                            PREVIEWER_DBUS_PATH,
                            PREVIEWER2_DBUS_IFACE,
                            "Close",
                            NULL,
                            NULL,
                            G_DBUS_CALL_FLAGS_NO_AUTO_START,
                            -1,
                            NULL,
                            previewer2_method_ready_cb,
                            NULL);
}
