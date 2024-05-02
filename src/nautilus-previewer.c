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
#define G_LOG_DOMAIN "nautilus-previewer"

#include "config.h"

#include "nautilus-previewer.h"

#include "nautilus-files-view.h"
#include "nautilus-window.h"
#include "nautilus-window-slot.h"

#include <gio/gio.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#endif
#ifdef GDK_WINDOWING_X11
#include <gdk/x11/gdkx.h>
#endif

#define PREVIEWER2_DBUS_IFACE "org.gnome.NautilusPreviewer2"

static const char *previewer_dbus_name = "org.gnome.NautilusPreviewer" PROFILE;
static const char *previewer_dbus_path = "/org/gnome/NautilusPreviewer" PROFILE;

static gboolean previewer_ready = FALSE;
static gboolean fetching_bus = FALSE;
static GDBusProxy *previewer_proxy = NULL;
static guint subscription_id = 0;

static GCancellable *cancellable = NULL;

static GtkRoot *current_window = NULL; /* weak ref */
static gchar *exported_window_handle = NULL;

static void real_call_show_file (const gchar *uri,
                                 const gchar *window_handle,
                                 gboolean     close_if_already_visible);
static void create_new_bus (void);
static void previewer_selection_event (GDBusConnection *connection,
                                       const gchar     *sender_name,
                                       const gchar     *object_path,
                                       const gchar     *interface_name,
                                       const gchar     *signal_name,
                                       GVariant        *parameters,
                                       gpointer         user_data);

#ifdef GDK_WINDOWING_WAYLAND
typedef struct
{
    gchar *uri;
    gboolean close_if_already_visible;
} PreviewExportData;

static void
preview_export_data_free (gpointer _data)
{
    PreviewExportData *data = _data;
    g_free (data->uri);
    g_free (data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (PreviewExportData, preview_export_data_free)

static void
wayland_window_handle_exported (GdkToplevel *toplevel,
                                const char  *wayland_handle_str,
                                gpointer     user_data)
{
    PreviewExportData *data = user_data;
    g_autofree char *wayland_handle = g_strdup_printf ("wayland:%s", wayland_handle_str);

    real_call_show_file (data->uri, wayland_handle, data->close_if_already_visible);
}
#endif

static void
clear_exported_window_handle (void)
{
    if (exported_window_handle == NULL)
    {
        return;
    }

#ifdef GDK_WINDOWING_WAYLAND
    if (current_window != NULL &&
        GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (current_window))))
    {
        GdkSurface *gdk_surface = gtk_native_get_surface (GTK_NATIVE (current_window));
        if (GDK_IS_WAYLAND_TOPLEVEL (gdk_surface) &&
            g_str_has_prefix (exported_window_handle, "wayland:"))
        {
            gdk_wayland_toplevel_drop_exported_handle (GDK_WAYLAND_TOPLEVEL (gdk_surface),
                                                       exported_window_handle + strlen ("wayland:"));
        }
    }
#endif

    g_free (exported_window_handle);
    exported_window_handle = NULL;
}

static void
on_ping_finished (GObject      *object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
    g_autoptr (GVariant) variant = NULL;
    g_autoptr (GError) error = NULL;

    variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (object), res, &error);

    if (error == NULL)
    {
        GDBusConnection *connection = g_dbus_proxy_get_connection (previewer_proxy);

        previewer_ready = TRUE;
        fetching_bus = FALSE;
        subscription_id = g_dbus_connection_signal_subscribe (connection,
                                                              previewer_dbus_name,
                                                              PREVIEWER2_DBUS_IFACE,
                                                              "SelectionEvent",
                                                              previewer_dbus_path,
                                                              NULL,
                                                              G_DBUS_SIGNAL_FLAGS_NONE,
                                                              previewer_selection_event,
                                                              NULL,
                                                              NULL);
    }
    else if (g_strcmp0 (previewer_dbus_name, "org.gnome.NautilusPreviewerDevel") == 0)
    {
        previewer_dbus_name = "org.gnome.NautilusPreviewer";
        previewer_dbus_path = "/org/gnome/NautilusPreviewer";
        create_new_bus ();
    }
    else
    {
        fetching_bus = FALSE;
        g_debug ("Unable to create NautilusPreviewer2 proxy: %s", error->message);
    }
}

static void
on_bus_ready (GObject      *object,
              GAsyncResult *res,
              gpointer      user_data)
{
    g_autoptr (GError) error = NULL;

    g_clear_object (&previewer_proxy);
    previewer_proxy = g_dbus_proxy_new_for_bus_finish (res, &error);

    if (error == NULL)
    {
        g_dbus_proxy_call (previewer_proxy,
                           "org.freedesktop.DBus.Peer.Ping", NULL,
                           G_DBUS_CALL_FLAGS_NONE, G_MAXINT,
                           cancellable, on_ping_finished, NULL);
    }
    else if (g_strcmp0 (previewer_dbus_name, "org.gnome.NautilusPreviewerDevel") == 0)
    {
        previewer_dbus_name = "org.gnome.NautilusPreviewer";
        previewer_dbus_path = "/org/gnome/NautilusPreviewer";
        create_new_bus ();
    }
    else
    {
        fetching_bus = FALSE;
        g_debug ("Unable to create NautilusPreviewer2 proxy: %s", error->message);
    }
}

static void
create_new_bus (void)
{
    g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                              G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START_AT_CONSTRUCTION,
                              NULL,
                              previewer_dbus_name,
                              previewer_dbus_path,
                              PREVIEWER2_DBUS_IFACE,
                              cancellable,
                              on_bus_ready,
                              NULL);
}

static gboolean
ensure_previewer_proxy (void)
{
    if (previewer_ready)
    {
        return TRUE;
    }
    if (fetching_bus)
    {
        return FALSE;
    }

    fetching_bus = TRUE;

    create_new_bus ();
    return FALSE;
}

static void
previewer2_method_ready_cb (GObject      *source,
                            GAsyncResult *res,
                            gpointer      user_data)
{
    GDBusProxy *proxy = G_DBUS_PROXY (source);
    g_autoptr (GError) error = NULL;

    g_dbus_proxy_call_finish (proxy, res, &error);

    if (error != NULL)
    {
        g_debug ("Unable to call method on NautilusPreviewer: %s", error->message);
    }
}

void
nautilus_previewer_call_show_file (const gchar *uri,
                                   GtkRoot     *window,
                                   gboolean     close_if_already_visible)
{
    /* Reuse existing handle if called again for the same window. */
    if (current_window == window &&
        exported_window_handle != NULL)
    {
        real_call_show_file (uri, exported_window_handle, close_if_already_visible);
        return;
    }

    /* Otherwise, obtain a new window handle. */
    clear_exported_window_handle ();
    g_set_weak_pointer (&current_window, window);

    GdkSurface *gdk_surface = gtk_native_get_surface (GTK_NATIVE (window));
#ifdef GDK_WINDOWING_X11
    if (GDK_IS_X11_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
        guint xid = (guint) gdk_x11_surface_get_xid (gdk_surface);
        g_autofree char *window_handle = g_strdup_printf ("x11:%x", xid);

        real_call_show_file (uri, window_handle, close_if_already_visible);
        return;
    }
#endif
#ifdef GDK_WINDOWING_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
        g_autoptr (PreviewExportData) data = g_new0 (PreviewExportData, 1);

        data->uri = g_strdup (uri);
        data->close_if_already_visible = close_if_already_visible;

        if (gdk_wayland_toplevel_export_handle (GDK_WAYLAND_TOPLEVEL (gdk_surface),
                                                wayland_window_handle_exported,
                                                data,
                                                preview_export_data_free))
        {
            /* Don't let autoptr free data successfully taken by the call. */
            (void) g_steal_pointer (&data);
            return;
        }
    }
#endif

    g_warning ("Couldn't export handle, unsupported windowing system");

    /* Let's use a fallback, so at least a preview will be displayed */
    real_call_show_file (uri, "x11:0", close_if_already_visible);
}

static void
real_call_show_file (const gchar *uri,
                     const gchar *window_handle,
                     gboolean     close_if_already_visible)
{
    g_set_str (&exported_window_handle, window_handle);

    if (!ensure_previewer_proxy ())
    {
        return;
    }

    g_dbus_proxy_call (previewer_proxy,
                       "ShowFile",
                       g_variant_new ("(ssb)",
                                      uri, window_handle, close_if_already_visible),
                       G_DBUS_CALL_FLAGS_NONE,
                       -1,
                       cancellable,
                       previewer2_method_ready_cb,
                       NULL);
}

void
nautilus_previewer_call_close (void)
{
    if (!ensure_previewer_proxy ())
    {
        return;
    }

    /* don't autostart the previewer if it's not running */
    g_dbus_proxy_call (previewer_proxy,
                       "Close",
                       NULL,
                       G_DBUS_CALL_FLAGS_NO_AUTO_START,
                       -1,
                       cancellable,
                       previewer2_method_ready_cb,
                       NULL);
}

static void
previewer_selection_event (GDBusConnection *connection,
                           const gchar     *sender_name,
                           const gchar     *object_path,
                           const gchar     *interface_name,
                           const gchar     *signal_name,
                           GVariant        *parameters,
                           gpointer         user_data)
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

void
nautilus_previewer_setup (void)
{
    ensure_previewer_proxy ();
}

void
nautilus_previewer_teardown (GDBusConnection *connection)
{
    if (subscription_id != 0)
    {
        g_dbus_connection_signal_unsubscribe (connection, subscription_id);
    }

    g_cancellable_cancel (cancellable);
    g_clear_object (&cancellable);
    g_clear_object (&previewer_proxy);
    clear_exported_window_handle ();
    g_clear_weak_pointer (&current_window);
}

gboolean
nautilus_previewer_is_visible (void)
{
    g_autoptr (GVariant) variant = NULL;

    if (!ensure_previewer_proxy ())
    {
        return FALSE;
    }

    variant = g_dbus_proxy_get_cached_property (previewer_proxy, "Visible");
    if (variant)
    {
        return g_variant_get_boolean (variant);
    }

    return FALSE;
}
