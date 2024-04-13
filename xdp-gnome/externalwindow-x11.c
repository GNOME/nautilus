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

#include <errno.h>
#include <gdk/x11/gdkx.h>
#include <X11/Xatom.h>
#include <gdk/gdk.h>
#include <stdlib.h>

#include "externalwindow-x11.h"


struct _ExternalWindowX11
{
  ExternalWindow parent;

  Window foreign_xid;
};

struct _ExternalWindowX11Class
{
  ExternalWindowClass parent_class;
};

G_DEFINE_TYPE (ExternalWindowX11, external_window_x11,
               EXTERNAL_TYPE_WINDOW)

ExternalWindowX11 *
external_window_x11_new (const char *handle_str)
{
  ExternalWindowX11 *external_window_x11;
  const char x11_prefix[] = "x11:";
  const char *x11_handle_str;
  int xid;

  if (!g_str_has_prefix (handle_str, x11_prefix))
    {
      g_warning ("Invalid external window handle string '%s'", handle_str);
      return NULL;
    }

  x11_handle_str = handle_str + strlen (x11_prefix);

  errno = 0;
  xid = strtol (x11_handle_str, NULL, 16);
  if (errno != 0)
    {
      g_warning ("Failed to reference external X11 window, invalid XID %s",
                 x11_handle_str);
      return NULL;
    }

  external_window_x11 = g_object_new (EXTERNAL_TYPE_WINDOW_X11,
                                      NULL);
  external_window_x11->foreign_xid = xid;

  return external_window_x11;
}

static void
external_window_x11_set_parent_of (ExternalWindow *external_window,
                                   GdkSurface     *surface)
{
  ExternalWindowX11 *external_window_x11 =
    EXTERNAL_WINDOW_X11 (external_window);
  GdkDisplay *display;
  Display *xdisplay;
  Atom atom;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);

  XSetTransientForHint (xdisplay,
                        GDK_SURFACE_XID (surface),
                        external_window_x11->foreign_xid);

  atom = gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE_DIALOG");
  XChangeProperty (xdisplay,
                   GDK_SURFACE_XID (surface),
                   gdk_x11_get_xatom_by_name_for_display (display, "_NET_WM_WINDOW_TYPE"),
                   XA_ATOM, 32, PropModeReplace,
                   (guchar *)&atom, 1);
}

static void
external_window_x11_init (ExternalWindowX11 *external_window_x11)
{
}

static void
external_window_x11_class_init (ExternalWindowX11Class *klass)
{
  ExternalWindowClass *external_window_class = EXTERNAL_WINDOW_CLASS (klass);

  external_window_class->set_parent_of = external_window_x11_set_parent_of;
}

GdkDisplay *
init_external_window_x11_display (GError **error)
{
  gdk_set_allowed_backends ("x11");
  return gdk_display_open (NULL);
}
