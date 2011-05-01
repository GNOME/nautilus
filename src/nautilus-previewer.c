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

#define DEBUG_FLAG NAUTILUS_DEBUG_PREVIEWER
#include <libnautilus-private/nautilus-debug.h>

#include <gio/gio.h>

G_DEFINE_TYPE (NautilusPreviewer, nautilus_previewer, G_TYPE_OBJECT);

#define PREVIEWER_DBUS_NAME "org.gnome.NautilusPreviewer"
#define PREVIEWER_DBUS_IFACE "org.gnome.NautilusPreviewer"
#define PREVIEWER_DBUS_PATH "/org/gnome/NautilusPreviewer"

static NautilusPreviewer *singleton = NULL;

struct _NautilusPreviewerPriv {
  GDBusProxy *proxy;

  GVariant *pending_variant;
};

static void
nautilus_previewer_dispose (GObject *object)
{
  NautilusPreviewer *self = NAUTILUS_PREVIEWER (object);

  DEBUG ("%p", self);

  g_clear_object (&self->priv->proxy);

  G_OBJECT_CLASS (nautilus_previewer_parent_class)->dispose (object);
}

static GObject *
nautilus_previewer_constructor (GType type,
                                guint n_construct_params,
                                GObjectConstructParam *construct_params)
{
  GObject *retval;

  if (singleton != NULL)
    return G_OBJECT (singleton);

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
  oclass->dispose = nautilus_previewer_dispose;

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

  g_object_unref (self);
}

static void
real_call_show_file (NautilusPreviewer *self,
                     GVariant *args)
{
  gchar *variant_str;

  variant_str = g_variant_print (args, TRUE);
  DEBUG ("Calling ShowFile with params %s", variant_str);

  g_dbus_proxy_call (self->priv->proxy,
                     "ShowFile",
                     args,
                     G_DBUS_CALL_FLAGS_NONE,
                     -1,
                     NULL,
                     previewer_show_file_ready_cb,
                     self);

  g_free (variant_str);
}

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
    g_object_unref (self);

    return;
  }

  DEBUG ("Got previewer DBus proxy");

  self->priv->proxy = proxy;

  if (self->priv->pending_variant != NULL) {
    real_call_show_file (self, self->priv->pending_variant);

    g_variant_unref (self->priv->pending_variant);
    self->priv->pending_variant = NULL;
  } else {
    g_object_unref (self);
  }
}

NautilusPreviewer *
nautilus_previewer_get_singleton (void)
{
  return g_object_new (NAUTILUS_TYPE_PREVIEWER, NULL);
}

void
nautilus_previewer_call_show_file (NautilusPreviewer *self,
                                   const gchar *uri,
                                   guint xid,
				   gboolean close_if_already_visible)
{
  GVariant *variant;

  variant = g_variant_new ("(sib)",
                           uri, xid, close_if_already_visible);
  g_object_ref (self);

  if (self->priv->proxy == NULL) {
    if (self->priv->pending_variant != NULL)
      g_variant_unref (self->priv->pending_variant);

    self->priv->pending_variant = g_variant_ref_sink (variant);
    g_dbus_proxy_new_for_bus (G_BUS_TYPE_SESSION,
                              G_DBUS_PROXY_FLAGS_NONE,
                              NULL,
                              PREVIEWER_DBUS_NAME,
                              PREVIEWER_DBUS_PATH,
                              PREVIEWER_DBUS_IFACE,
                              NULL,
                              previewer_proxy_async_ready_cb,
                              self);
  } else {
    real_call_show_file (self, variant);
  }
}
