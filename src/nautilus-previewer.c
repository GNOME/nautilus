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

#include "nautilus-file.h"
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
#define PREVIEWER_DBUS_NAME "org.gnome.NautilusPreviewer"
#define PREVIEWER_DBUS_PATH "/org/gnome/NautilusPreviewer"

static const char *previewer_dbus_name = PREVIEWER_DBUS_NAME PROFILE;
static const char *previewer_dbus_path = PREVIEWER_DBUS_PATH PROFILE;
static gboolean tried_alternative_previewer_dbus_name = FALSE;

static gboolean previewer_ready = FALSE;
static gboolean fetching_bus = FALSE;
static GDBusProxy *previewer_proxy = NULL;

enum
{
    DBUS_SUBSCRIPTION_NAVIGATE_TO,
    DBUS_SUBSCRIPTION_RENAME,
    DBUS_SUBSCRIPTION_SELECTION,
    DBUS_SUBSCRIPTION_TRASH,
    NUM_DBUS_SUBSCRIPTIONS
};

static guint dbus_subscriptions[NUM_DBUS_SUBSCRIPTIONS];

static GCancellable *cancellable = NULL;

static NautilusWindowSlot *current_slot = NULL; /* weak ref */
static GtkRoot *current_window = NULL; /* weak ref */
static gchar *exported_window_handle = NULL;

static void real_call_show_file (const gchar *uri,
                                 const gchar *window_handle,
                                 gboolean     close_if_already_visible,
                                 const char  *activation_token);
static void create_new_bus (void);

#ifdef GDK_WINDOWING_WAYLAND
typedef struct
{
    gchar *uri;
    gboolean close_if_already_visible;
    char *activation_token;
} PreviewExportData;

static void
preview_export_data_free (gpointer _data)
{
    PreviewExportData *data = _data;
    g_free (data->uri);
    g_free (data->activation_token);
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

    real_call_show_file (data->uri, wayland_handle, data->close_if_already_visible, data->activation_token);
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

static gboolean
is_uri_selected (NautilusFilesView *files_view,
                 const char        *uri)
{
    g_autolist (NautilusFile) selection = nautilus_files_view_get_selection (files_view);

    if (g_list_length (selection) != 1)
    {
        return FALSE;
    }

    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);

    return file == selection->data;
}

typedef void (*PreviewerEventCallback) (NautilusFilesView *files_view,
                                        GVariant          *parameters);

static void
previewer_navigate_to_event (NautilusFilesView *files_view,
                             GVariant          *parameters)
{
    const char *uri;

    g_variant_get (parameters, "(s)", &uri);

    g_autoptr (NautilusFile) file = nautilus_file_get_by_uri (uri);

    if (!nautilus_file_is_directory (file) || !is_uri_selected (files_view, uri))
    {
        g_warning ("Ignoring previewer NavigateTo request for uri %s", uri);
        return;
    }

    nautilus_files_view_activate_file (files_view, file, NAUTILUS_OPEN_FLAG_NORMAL);
}

static void
previewer_rename_event (NautilusFilesView *files_view,
                        GVariant          *parameters)
{
    const char *uri;

    g_variant_get (parameters, "(s)", &uri);

    if (!is_uri_selected (files_view, uri))
    {
        g_warning ("Ignoring previewer Rename request for uri %s", uri);
        return;
    }

    gtk_widget_activate_action (GTK_WIDGET (files_view), "view.rename", NULL);
}

static void
previewer_selection_event (NautilusFilesView *files_view,
                           GVariant          *parameters)
{
    GtkDirectionType direction;

    g_variant_get (parameters, "(u)", &direction);
    nautilus_files_view_preview_selection_event (files_view, direction);
}

static void
previewer_trash_event (NautilusFilesView *files_view,
                       GVariant          *parameters)
{
    const char *uri;

    g_variant_get (parameters, "(s)", &uri);

    if (!is_uri_selected (files_view, uri))
    {
        g_warning ("Ignoring previewer Trash request for uri %s", uri);
        return;
    }

    gtk_widget_activate_action (GTK_WIDGET (files_view), "view.move-to-trash", NULL);
}

static void
handle_previewer_event (GDBusConnection *connection,
                        const gchar     *sender_name,
                        const gchar     *object_path,
                        const gchar     *interface_name,
                        const gchar     *signal_name,
                        GVariant        *parameters,
                        gpointer         user_data)
{
    PreviewerEventCallback callback = user_data;
    NautilusFilesView *files_view = (current_slot != NULL) ?
                                    nautilus_window_slot_get_current_view (current_slot) : NULL;

    if (files_view == NULL)
    {
        return;
    }

    callback (files_view, parameters);
}

static guint
setup_dbus_connection (GDBusConnection        *connection,
                       const char             *event_name,
                       PreviewerEventCallback  callback)
{
    return g_dbus_connection_signal_subscribe (connection,
                                               previewer_dbus_name,
                                               PREVIEWER2_DBUS_IFACE,
                                               event_name,
                                               previewer_dbus_path,
                                               NULL,
                                               G_DBUS_SIGNAL_FLAGS_NONE,
                                               handle_previewer_event,
                                               callback,
                                               NULL);
}

static void
switch_to_alternative_previewer_dbus_name (void)
{
    tried_alternative_previewer_dbus_name = TRUE;

    if (g_str_has_suffix (previewer_dbus_name, "Devel"))
    {
        previewer_dbus_name = PREVIEWER_DBUS_NAME;
        previewer_dbus_path = PREVIEWER_DBUS_PATH;
    }
    else
    {
        previewer_dbus_name = PREVIEWER_DBUS_NAME "Devel";
        previewer_dbus_path = PREVIEWER_DBUS_PATH "Devel";
    }
}

static void
on_ping_finished (GObject      *object,
                  GAsyncResult *res,
                  gpointer      user_data)
{
    g_autoptr (GError) error = NULL;
    g_autoptr (GVariant) variant = g_dbus_proxy_call_finish (G_DBUS_PROXY (object), res, &error);

    if (error == NULL)
    {
        GDBusConnection *connection = g_dbus_proxy_get_connection (previewer_proxy);

        previewer_ready = TRUE;
        fetching_bus = FALSE;

        dbus_subscriptions[DBUS_SUBSCRIPTION_NAVIGATE_TO] =
            setup_dbus_connection (connection, "NavigateTo", previewer_navigate_to_event);
        dbus_subscriptions[DBUS_SUBSCRIPTION_RENAME] =
            setup_dbus_connection (connection, "Rename", previewer_rename_event);
        dbus_subscriptions[DBUS_SUBSCRIPTION_SELECTION] =
            setup_dbus_connection (connection, "SelectionEvent", previewer_selection_event);
        dbus_subscriptions[DBUS_SUBSCRIPTION_TRASH] =
            setup_dbus_connection (connection, "Trash", previewer_trash_event);
    }
    else if (!tried_alternative_previewer_dbus_name)
    {
        switch_to_alternative_previewer_dbus_name ();
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
    else if (!tried_alternative_previewer_dbus_name)
    {
        switch_to_alternative_previewer_dbus_name ();
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
nautilus_previewer_call_show_file (const gchar        *uri,
                                   NautilusWindowSlot *slot,
                                   gboolean            close_if_already_visible)
{
    g_set_weak_pointer (&current_slot, slot);

    GtkRoot *window = gtk_widget_get_root (GTK_WIDGET (slot));

    g_autoptr (GdkAppLaunchContext) launch_context = gdk_display_get_app_launch_context (
        gtk_root_get_display (window));
    g_autofree char *activation_token = g_app_launch_context_get_startup_notify_id (
        G_APP_LAUNCH_CONTEXT (launch_context),
        NULL,
        NULL);

    /* Reuse existing handle if called again for the same window. */
    if (current_window == window &&
        exported_window_handle != NULL)
    {
        real_call_show_file (uri, exported_window_handle, close_if_already_visible, activation_token);
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

        real_call_show_file (uri, window_handle, close_if_already_visible, activation_token);
        return;
    }
#endif
#ifdef GDK_WINDOWING_WAYLAND
    if (GDK_IS_WAYLAND_DISPLAY (gtk_widget_get_display (GTK_WIDGET (window))))
    {
        g_autoptr (PreviewExportData) data = g_new0 (PreviewExportData, 1);

        data->uri = g_strdup (uri);
        data->close_if_already_visible = close_if_already_visible;
        data->activation_token = g_strdup (activation_token);

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
    real_call_show_file (uri, "x11:0", close_if_already_visible, activation_token);
}

static void
real_call_show_file (const gchar *uri,
                     const gchar *window_handle,
                     gboolean     close_if_already_visible,
                     const char  *activation_token)
{
    g_set_str (&exported_window_handle, window_handle);

    if (!ensure_previewer_proxy ())
    {
        return;
    }

    if (activation_token == NULL)
    {
        activation_token = "";
    }

    GVariant *parameters = g_variant_new (
        "(ssbs)",
        uri, window_handle, close_if_already_visible, activation_token);
    g_dbus_proxy_call (previewer_proxy,
                       "ShowFile",
                       parameters,
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

void
nautilus_previewer_setup (void)
{
    ensure_previewer_proxy ();
}

void
nautilus_previewer_teardown (GDBusConnection *connection)
{
    for (guint i = 0; i < NUM_DBUS_SUBSCRIPTIONS; i += 1)
    {
        if (dbus_subscriptions[i] != 0)
        {
            g_dbus_connection_signal_unsubscribe (connection, dbus_subscriptions[i]);
        }
    }

    g_cancellable_cancel (cancellable);
    g_clear_object (&cancellable);
    g_clear_object (&previewer_proxy);
    clear_exported_window_handle ();
    g_clear_weak_pointer (&current_window);
    g_clear_weak_pointer (&current_slot);
}

gboolean
nautilus_previewer_is_visible (void)
{
    if (!ensure_previewer_proxy ())
    {
        return FALSE;
    }

    g_autoptr (GVariant) variant = g_dbus_proxy_get_cached_property (previewer_proxy, "Visible");

    if (variant)
    {
        return g_variant_get_boolean (variant);
    }

    return FALSE;
}
