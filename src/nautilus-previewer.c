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

#include "nautilus-files-view.h"
#include "nautilus-window.h"
#include "nautilus-window-slot.h"

#define DEBUG_FLAG NAUTILUS_DEBUG_PREVIEWER
#include "nautilus-debug.h"

#include <gio/gio.h>

#define PREVIEWER_DBUS_NAME "org.gnome.NautilusPreviewer"
#define PREVIEWER_DBUS_IFACE "org.gnome.NautilusPreviewer"
#define PREVIEWER2_DBUS_IFACE "org.gnome.NautilusPreviewer2"
#define PREVIEWER_DBUS_PATH "/org/gnome/NautilusPreviewer"

static GDBusProxy *previewer_v1_proxy = NULL;
static GDBusProxy *previewer_v2_proxy = NULL;

static void
ensure_previewer_v1_proxy (void)
{
    if (previewer_v1_proxy == NULL)
    {
        GDBusConnection *connection = g_application_get_dbus_connection (g_application_get_default ());
        previewer_v1_proxy = g_dbus_proxy_new_sync (connection,
                                                    G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                    NULL,
                                                    PREVIEWER_DBUS_NAME,
                                                    PREVIEWER_DBUS_PATH,
                                                    PREVIEWER_DBUS_IFACE,
                                                    NULL,
                                                    NULL);
    }
}

static void
ensure_previewer_v2_proxy (void)
{
    if (previewer_v2_proxy == NULL)
    {
        GDBusConnection *connection = g_application_get_dbus_connection (g_application_get_default ());
        previewer_v2_proxy = g_dbus_proxy_new_sync (connection,
                                                    G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START,
                                                    NULL,
                                                    PREVIEWER_DBUS_NAME,
                                                    PREVIEWER_DBUS_PATH,
                                                    PREVIEWER2_DBUS_IFACE,
                                                    NULL,
                                                    NULL);
    }
}

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

    g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);

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
    GError *error = NULL;

    g_dbus_proxy_call_finish (G_DBUS_PROXY (source), res, &error);

    if (error == NULL)
    {
        method_call_data_free (data);
        return;
    }

    DEBUG ("Unable to call %s on NautilusPreviewer: %s",
           data->method_name, error->message);
    g_clear_error (&error);

    ensure_previewer_v1_proxy ();
    g_dbus_proxy_call (previewer_v1_proxy,
                       data->method_name,
                       data->v1_variant,
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
    MethodCallData *data = g_new0 (MethodCallData, 1);

    data->flags = flags;
    data->method_name = g_strdup (method_name);
    if (v1_variant != NULL)
    {
        data->v1_variant = g_variant_ref_sink (v1_variant);
    }

    /* Try the v2 interface first */
    ensure_previewer_v2_proxy ();
    g_dbus_proxy_call (previewer_v2_proxy,
                       method_name,
                       v2_variant,
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

static void
previewer_selection_event (GDBusConnection *connection,
                           const gchar *sender_name,
                           const gchar *object_path,
                           const gchar *interface_name,
                           const gchar *signal_name,
                           GVariant *parameters,
                           gpointer user_data)
{
    GApplication *application = g_application_get_default ();
    GList *l, *windows = gtk_application_get_windows (GTK_APPLICATION (application));
    NautilusWindow *window = NULL;
    NautilusWindowSlot *slot;
    NautilusView *view;
    GtkDirectionType direction;

    for (l = windows; l != NULL; l = l->next)
    {
        if (NAUTILUS_IS_WINDOW (l->data))
        {
            window = l->data;
            break;
        }
    }

    if (window == NULL)
    {
        return;
    }

    slot = nautilus_window_get_active_slot (window);
    view = nautilus_window_slot_get_current_view (slot);

    if (!NAUTILUS_IS_FILES_VIEW (view))
    {
        return;
    }

    g_variant_get (parameters, "(u)", &direction);
    nautilus_files_view_preview_selection_event (NAUTILUS_FILES_VIEW (view), direction);
}

guint
nautilus_previewer_connect_selection_event (GDBusConnection *connection)
{
    return g_dbus_connection_signal_subscribe (connection,
                                               PREVIEWER_DBUS_NAME,
                                               PREVIEWER2_DBUS_IFACE,
                                               "SelectionEvent",
                                               PREVIEWER_DBUS_PATH,
                                               NULL,
                                               G_DBUS_SIGNAL_FLAGS_NONE,
                                               previewer_selection_event,
                                               NULL,
                                               NULL);
}

void
nautilus_previewer_disconnect_selection_event (GDBusConnection *connection,
                                               guint            event_id)
{
    g_dbus_connection_signal_unsubscribe (connection, event_id);
}

gboolean
nautilus_previewer_is_visible (void)
{
    g_autoptr(GVariant) variant = NULL;

    ensure_previewer_v2_proxy ();
    variant = g_dbus_proxy_get_cached_property (previewer_v2_proxy, "Visible");
    if (variant)
    {
        return g_variant_get_boolean (variant);
    }

    return FALSE;
}
