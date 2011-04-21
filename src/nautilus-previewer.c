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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "nautilus-previewer.h"

#include <gio/gio.h>

G_DEFINE_TYPE (NautilusPreviewer, nautilus_previewer, G_TYPE_OBJECT);

#define PREVIEWER_DBUS_NAME "org.gnome.NautilusPreviewer"
#define PREVIEWER_DBUS_IFACE "org.gnome.NautilusPreviewer"
#define PREVIEWER_DBUS_PATH "/org/gnome/NautilusPreviewer"

static NautilusPreviewer *singleton = NULL;

struct _NautilusPreviewerPriv {
  guint watch_id;
  GDBusProxy *proxy;
};

static void
previewer_proxy_async_ready_cb (GObject *source,
                                GAsyncResult *res,
                                gpointer user_data)
{
  GDBusProxy *proxy;
  NautilusPreviewer *self = user_data;
  GError *error = NULL;

  proxy = g_dbus_proxy_new_finish (res, &error);

  if (error != NULL) {
    g_warning ("Unable to create a dbus proxy for NautilusPreviewer: %s",
               error->message);
    g_error_free (error);

    return;
  }

  self->priv->proxy = proxy;
}

static void
previewer_name_appeared_cb (GDBusConnection *conn,
                            const gchar *name,
                            const gchar *name_owner,
                            gpointer user_data)
{
  NautilusPreviewer *self = user_data;

  g_dbus_proxy_new (conn,
                    G_DBUS_PROXY_FLAGS_NONE,
                    NULL,
                    name,
                    PREVIEWER_DBUS_PATH,
                    PREVIEWER_DBUS_IFACE,
                    NULL,
                    previewer_proxy_async_ready_cb,
                    self);
}

static void
previewer_name_vanished_cb (GDBusConnection *conn,
                            const gchar *name,
                            gpointer user_data)
{
  NautilusPreviewer *self = user_data;

  g_print ("vanished %p %s\n", self, name);
}

static void
nautilus_previewer_constructed (GObject *object)
{
  NautilusPreviewer *self = NAUTILUS_PREVIEWER (object);

  self->priv->watch_id =
    g_bus_watch_name (G_BUS_TYPE_SESSION,
                      PREVIEWER_DBUS_NAME,
                      G_BUS_NAME_WATCHER_FLAGS_AUTO_START,
                      previewer_name_appeared_cb,
                      previewer_name_vanished_cb,
                      self,
                      NULL);
}

static GObject *
nautilus_previewer_constructor (GType type,
                                guint n_construct_params,
                                GObjectConstructParam *construct_params)
{
  GObject *retval;

  if (singleton != NULL)
    return g_object_ref (singleton);

  retval = G_OBJECT_CLASS (nautilus_previewer_parent_class)->constructor
    (type, n_construct_params, construct_params);

  singleton = NAUTILUS_PREVIEWER (retval);
  g_object_add_weak_pointer (retval, (gpointer) &singleton);

  return retval;
}

static void
nautilus_previewer_init (NautilusPreviewer *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, NAUTILUS_TYPE_PREVIEWER,
                                            NautilusPreviewerPriv);
}

static void
nautilus_previewer_class_init (NautilusPreviewerClass *klass)
{
  GObjectClass *oclass;

  oclass = G_OBJECT_CLASS (klass);
  oclass->constructor = nautilus_previewer_constructor;
  oclass->constructed = nautilus_previewer_constructed;

  g_type_class_add_private (klass, sizeof (NautilusPreviewerPriv));
}

static void
previewer_show_file_ready_cb (GObject *source,
                              GAsyncResult *res,
                              gpointer user_data)
{
  NautilusPreviewer *self = user_data;
  GError *error = NULL;

  g_dbus_proxy_call_finish (self->priv->proxy,
                            res, &error);

  if (error != NULL) {
    g_warning ("Unable to call ShowFile on NautilusPreviewer: %s",
               error->message);
    g_error_free (error);
  }
}

NautilusPreviewer *
nautilus_previewer_dup_singleton (void)
{
  return g_object_new (NAUTILUS_TYPE_PREVIEWER, NULL);
}

void
nautilus_previewer_call_show_file (NautilusPreviewer *self,
                                   const gchar *uri,
                                   guint xid,
                                   guint x,
                                   guint y)
{
  if (!self->priv->proxy)
    return;

  g_dbus_proxy_call (self->priv->proxy,
                     "ShowFile",
                     g_variant_new ("(siii)",
                                    uri, xid, x, y),
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     previewer_show_file_ready_cb,
                     self);
}
