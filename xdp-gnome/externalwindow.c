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

#include <string.h>

#include "externalwindow.h"
#ifdef HAVE_GTK_X11
#include <gdk/x11/gdkx.h>
#include "externalwindow-x11.h"
#endif
#ifdef HAVE_GTK_WAYLAND
#include <gdk/wayland/gdkwayland.h>
#include "externalwindow-wayland.h"
#endif

G_DEFINE_TYPE (ExternalWindow, external_window, G_TYPE_OBJECT)

ExternalWindow *
create_external_window_from_handle (const char *handle_str)
{
  if (!handle_str)
    return NULL;

  if (strlen (handle_str) == 0)
    return NULL;

#ifdef HAVE_GTK_X11
  if (GDK_IS_X11_DISPLAY (gdk_display_get_default ()))
    {
      ExternalWindowX11 *external_window_x11;

      external_window_x11 = external_window_x11_new (handle_str);
      return EXTERNAL_WINDOW (external_window_x11);
    }
#endif
#ifdef HAVE_GTK_WAYLAND
  if (GDK_IS_WAYLAND_DISPLAY (gdk_display_get_default ()))
    {
      ExternalWindowWayland *external_window_wayland;

      external_window_wayland = external_window_wayland_new (handle_str);
      return EXTERNAL_WINDOW (external_window_wayland);
    }
#endif

  g_warning ("Unhandled parent window type %s", handle_str);
  return NULL;
}

void
external_window_set_parent_of (ExternalWindow *external_window,
                               GdkSurface     *surface)
{
  EXTERNAL_WINDOW_GET_CLASS (external_window)->set_parent_of (external_window,
                                                              surface);
}

static void
external_window_init (ExternalWindow *external_window)
{
}

static void
external_window_class_init (ExternalWindowClass *klass)
{
}

GdkDisplay *
init_external_window_display (GError **error)
{
  const char *session_type;

  session_type = getenv ("XDG_SESSION_TYPE");
#ifdef HAVE_GTK_WAYLAND
  if (g_strcmp0 (session_type, "wayland") == 0)
    return init_external_window_wayland_display (error);
#endif
#ifdef HAVE_GTK_X11
  if (g_strcmp0 (session_type, "x11") == 0)
    return init_external_window_x11_display (error);
#endif

  g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
               "Unsupported or missing session type '%s'",
               session_type ? session_type : "");
  return FALSE;
}
