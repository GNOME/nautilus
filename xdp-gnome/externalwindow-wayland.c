/*
 * Copyright © 2016 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Jonas Ådahl <jadahl@redhat.com>
 */

#include "config.h"

#include "externalwindow-wayland.h"

#include <gdk/gdk.h>
#include <gdk/wayland/gdkwayland.h>
#include <wayland-client.h>

#include "mutter-x11-interop-client-protocol.h"
#include "shell-dbus.h"

#pragma GCC diagnostic ignored "-Wshadow"

#define WAYLAND_HANDLE_PREFIX "wayland:"
#define X11_HANDLE_PREFIX "x11:"

typedef enum _ServiceClientType
{
  SERVICE_CLIENT_TYPE_NONE,
  SERVICE_CLIENT_TYPE_PORTAL_BACKEND,
  SERVICE_CLIENT_TYPE_FILECHOOSER_PORTAL_BACKEND,
} ServiceClientType;

typedef enum _ParentWindowType
{
  PARENT_WINDOW_TYPE_WAYLAND,
  PARENT_WINDOW_TYPE_X11,
} ParentWindowType;

struct _ExternalWindowWayland
{
  ExternalWindow parent;

  ParentWindowType parent_type;
  struct {
    char *xdg_foreign_handle;
  } wayland;
  struct {
    int xid;
    gulong mapped_handler_id;
  } x11;

  GdkSurface *parent_surface;
};

struct _ExternalWindowWaylandClass
{
  ExternalWindowClass parent_class;
};

G_DEFINE_TYPE (ExternalWindowWayland, external_window_wayland,
               EXTERNAL_TYPE_WINDOW)

static struct mutter_x11_interop *x11_interop = NULL;

ExternalWindowWayland *
external_window_wayland_new (const char *handle_str)
{
  g_autoptr(ExternalWindowWayland) external_window_wayland = NULL;

  external_window_wayland = g_object_new (EXTERNAL_TYPE_WINDOW_WAYLAND,
                                          NULL);

  if (g_str_has_prefix (handle_str, WAYLAND_HANDLE_PREFIX))
    {
      external_window_wayland->parent_type = PARENT_WINDOW_TYPE_WAYLAND;
      external_window_wayland->wayland.xdg_foreign_handle =
        g_strdup (handle_str + strlen (WAYLAND_HANDLE_PREFIX));
    }
  else if (g_str_has_prefix (handle_str, X11_HANDLE_PREFIX))
    {
      const char *x11_handle_str = handle_str + strlen (X11_HANDLE_PREFIX);
      int xid;

      errno = 0;
      xid = strtol (x11_handle_str, NULL, 16);
      if (errno != 0)
        {
          g_warning ("Failed to reference external X11 window, invalid XID %s",
                     x11_handle_str);
          return NULL;
        }

      external_window_wayland->parent_type = PARENT_WINDOW_TYPE_X11;
      external_window_wayland->x11.xid = xid;
    }
  else
    {
      g_warning ("Invalid external window handle string '%s'", handle_str);
      return NULL;
    }

  return g_steal_pointer (&external_window_wayland);
}

static void
set_x11_parent (ExternalWindow *external_window)
{
  ExternalWindowWayland *external_window_wayland =
    EXTERNAL_WINDOW_WAYLAND (external_window);
  GdkSurface *parent_surface = external_window_wayland->parent_surface;
  struct wl_surface *wl_surface;

  g_return_if_fail (parent_surface);

  if (!x11_interop)
    return;

  wl_surface = gdk_wayland_surface_get_wl_surface (parent_surface);
  mutter_x11_interop_set_x11_parent (x11_interop, wl_surface,
                                     external_window_wayland->x11.xid);
}

static void
on_mapped (GdkSurface     *surface,
           GParamSpec     *pspec,
           ExternalWindow *external_window)
{
  if (!gdk_surface_get_mapped (surface))
    return;

  set_x11_parent (external_window);
}

static void
external_window_wayland_set_parent_of (ExternalWindow *external_window,
                                       GdkSurface     *surface)
{
  ExternalWindowWayland *external_window_wayland =
    EXTERNAL_WINDOW_WAYLAND (external_window);

  g_clear_signal_handler (&external_window_wayland->x11.mapped_handler_id,
                          external_window_wayland->parent_surface);
  g_clear_object (&external_window_wayland->parent_surface);

  switch (external_window_wayland->parent_type)
    {
    case PARENT_WINDOW_TYPE_WAYLAND:
      {
        GdkToplevel *toplevel = GDK_TOPLEVEL (surface);
        const char *handle = external_window_wayland->wayland.xdg_foreign_handle;

        if (!gdk_wayland_toplevel_set_transient_for_exported (toplevel, handle))
          g_warning ("Failed to set portal window transient for external parent");
        break;
      }
    case PARENT_WINDOW_TYPE_X11:
      if (gdk_surface_get_mapped (surface))
        {
          set_x11_parent (external_window);
        }
      else
        {
          external_window_wayland->x11.mapped_handler_id =
            g_signal_connect (surface, "notify::mapped",
                              G_CALLBACK (on_mapped), external_window);
        }
      break;
    }

  g_set_object (&external_window_wayland->parent_surface, surface);
}

static void
external_window_wayland_dispose (GObject *object)
{
  ExternalWindowWayland *external_window_wayland =
    EXTERNAL_WINDOW_WAYLAND (object);

  g_clear_pointer (&external_window_wayland->wayland.xdg_foreign_handle,
                   g_free);
  g_clear_signal_handler (&external_window_wayland->x11.mapped_handler_id,
                          external_window_wayland->parent_surface);
  g_clear_object (&external_window_wayland->parent_surface);

  G_OBJECT_CLASS (external_window_wayland_parent_class)->dispose (object);
}

static void
external_window_wayland_init (ExternalWindowWayland *external_window_wayland)
{
}

static void
external_window_wayland_class_init (ExternalWindowWaylandClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ExternalWindowClass *external_window_class = EXTERNAL_WINDOW_CLASS (klass);

  object_class->dispose = external_window_wayland_dispose;

  external_window_class->set_parent_of = external_window_wayland_set_parent_of;
}

static void
handle_registry_global (void               *user_data,
                        struct wl_registry *registry,
                        uint32_t            id,
                        const char         *interface,
                        uint32_t            version)
{
  struct mutter_x11_interop **x11_interop = user_data;

  if (strcmp (interface, mutter_x11_interop_interface.name) == 0)
    {
      *x11_interop = wl_registry_bind (registry, id,
                                       &mutter_x11_interop_interface, 1);
    }
}

static void
handle_registry_global_remove (void               *user_data,
                               struct wl_registry *registry,
                               uint32_t            name)
{
}

static const struct wl_registry_listener registry_listener = {
  handle_registry_global,
  handle_registry_global_remove
};

static void
init_x11_interop (GdkDisplay *display)
{
  struct wl_display *wl_display;
  struct wl_registry *wl_registry;

  wl_display = gdk_wayland_display_get_wl_display (display);
  wl_registry = wl_display_get_registry (wl_display);
  wl_registry_add_listener (wl_registry, &registry_listener, &x11_interop);
  wl_display_roundtrip (wl_display);

  if (!x11_interop)
    g_debug ("Missing X11 interop protocol support");
}

GdkDisplay *
init_external_window_wayland_display (GError **error)
{
  g_autoptr(OrgGnomeMutterServiceChannel) proxy = NULL;
  g_autoptr(GVariant) fd_variant = NULL;
  g_autoptr(GUnixFDList) fd_list = NULL;
  int fd;
  g_autofree char *fd_str = NULL;
  GdkDisplay *display;

  proxy = org_gnome_mutter_service_channel_proxy_new_for_bus_sync (
    G_BUS_TYPE_SESSION,
    (G_DBUS_PROXY_FLAGS_DO_NOT_AUTO_START |
     G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES |
     G_DBUS_PROXY_FLAGS_DO_NOT_CONNECT_SIGNALS),
    "org.gnome.Mutter.ServiceChannel",
    "/org/gnome/Mutter/ServiceChannel",
    NULL, error);
  if (!proxy)
    return NULL;

  if (!org_gnome_mutter_service_channel_call_open_wayland_service_connection_sync (
        proxy,
        SERVICE_CLIENT_TYPE_FILECHOOSER_PORTAL_BACKEND,
        NULL,
        &fd_variant,
        &fd_list,
        NULL, error))
    return NULL;

  fd = g_unix_fd_list_get (fd_list, g_variant_get_handle (fd_variant), error);
  if (fd < 0)
    return NULL;

  fd_str = g_strdup_printf ("%d", fd);

  g_setenv ("WAYLAND_SOCKET", fd_str, TRUE);
  gdk_set_allowed_backends ("wayland");
  display = gdk_display_open (NULL);
  g_assert (display);

  init_x11_interop (display);

  return display;
}
