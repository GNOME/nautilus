/*
 * Copyright (C) 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: António Fernandes <antoniof@gnome.org>
 */

#define G_LOG_DOMAIN "nautilus-dbus"

#include "nautilus-portal.h"

#include <config.h>
#include <xdp-gnome/request.h>
#include <xdp-gnome/xdg-desktop-portal-dbus.h>

#define DESKTOP_PORTAL_OBJECT_PATH "/org/freedesktop/portal/desktop"

struct _NautilusPortal
{
    GObject parent;

    XdpImplFileChooser *impl_file_chooser_skeleton;
};

G_DEFINE_TYPE (NautilusPortal, nautilus_portal, G_TYPE_OBJECT);

static void
nautilus_portal_dispose (GObject *object)
{
    NautilusPortal *self = (NautilusPortal *) object;

    if (self->impl_file_chooser_skeleton != NULL)
    {
        g_signal_handlers_disconnect_by_data (self->impl_file_chooser_skeleton, self);
        g_clear_object (&self->impl_file_chooser_skeleton);
    }

    G_OBJECT_CLASS (nautilus_portal_parent_class)->dispose (object);
}

static void
nautilus_portal_class_init (NautilusPortalClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->dispose = nautilus_portal_dispose;
}

static void
nautilus_portal_init (NautilusPortal *self)
{
    self->impl_file_chooser_skeleton = xdp_impl_file_chooser_skeleton_new ();
}

NautilusPortal *
nautilus_portal_new (void)
{
    return g_object_new (NAUTILUS_TYPE_PORTAL, NULL);
}

gboolean
nautilus_portal_register (NautilusPortal   *self,
                          GDBusConnection  *connection,
                          GError          **error)
{
    if (!g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (self->impl_file_chooser_skeleton),
                                           connection,
                                           DESKTOP_PORTAL_OBJECT_PATH,
                                           error))
    {
        return FALSE;
    }

    return TRUE;
}

void
nautilus_portal_unregister (NautilusPortal *self)
{
    g_dbus_interface_skeleton_unexport (G_DBUS_INTERFACE_SKELETON (self->impl_file_chooser_skeleton));
}
