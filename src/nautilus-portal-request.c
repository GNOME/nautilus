/*
 * SPDX-FileCopyrightText: 2016 Red Hat, Inc
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors:
 *       Alexander Larsson <alexl@redhat.com>
 *       Matthias Clasen <mclasen@redhat.com>
 */

#include "nautilus-portal-request.h"

#include <string.h>

static void request_skeleton_iface_init (XdpImplRequestIface *iface);

struct _NautilusPortalRequest
{
    XdpImplRequestSkeleton parent_instance;

    gboolean exported;
    char *sender;
    char *app_id;
    char *id;
};

G_DEFINE_TYPE_WITH_CODE (NautilusPortalRequest, nautilus_portal_request, XDP_IMPL_TYPE_REQUEST_SKELETON,
                         G_IMPLEMENT_INTERFACE (XDP_IMPL_TYPE_REQUEST, request_skeleton_iface_init))

static gboolean
handle_close (XdpImplRequest        *object,
              GDBusMethodInvocation *invocation)
{
    NautilusPortalRequest *self = (NautilusPortalRequest *) object;
    g_autoptr (GError) error = NULL;

    if (self->exported)
    {
        nautilus_portal_request_unexport (self);
    }

    xdp_impl_request_complete_close (XDP_IMPL_REQUEST (self), invocation);

    return TRUE;
}

static void
request_skeleton_iface_init (XdpImplRequestIface *iface)
{
    iface->handle_close = handle_close;
}

static void
nautilus_portal_request_init (NautilusPortalRequest *self)
{
}

static void
portal_request_finalize (GObject *object)
{
    NautilusPortalRequest *self = (NautilusPortalRequest *) object;

    g_free (self->sender);
    g_free (self->app_id);
    g_free (self->id);

    G_OBJECT_CLASS (nautilus_portal_request_parent_class)->finalize (object);
}

static void
nautilus_portal_request_class_init (NautilusPortalRequestClass *klass)
{
    GObjectClass *gobject_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = portal_request_finalize;
}

NautilusPortalRequest *
nautilus_portal_request_new (const char *sender,
                             const char *app_id,
                             const char *id)
{
    NautilusPortalRequest *self = g_object_new (nautilus_portal_request_get_type (), NULL);

    self->sender = g_strdup (sender);
    self->app_id = g_strdup (app_id);
    self->id = g_strdup (id);

    return self;
}

void
nautilus_portal_request_export (NautilusPortalRequest *self,
                                GDBusConnection       *connection)
{
    g_autoptr (GError) error = NULL;

    if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self),
                                           connection,
                                           self->id,
                                           &error))
    {
        g_warning ("error exporting request: %s\n", error->message);
        g_clear_error (&error);
    }

    g_object_ref (self);
    self->exported = TRUE;
}

void
nautilus_portal_request_unexport (NautilusPortalRequest *self)
{
    self->exported = FALSE;
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self));
    g_object_unref (self);
}
