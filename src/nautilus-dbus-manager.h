/*
 * nautilus-dbus-manager: nautilus DBus interface
 *
 * SPDX-FileCopyrightText: 2010, Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

#define NAUTILUS_TYPE_DBUS_MANAGER (nautilus_dbus_manager_get_type())
G_DECLARE_FINAL_TYPE (NautilusDBusManager, nautilus_dbus_manager, NAUTILUS, DBUS_MANAGER, GObject)

NautilusDBusManager * nautilus_dbus_manager_new (void);

gboolean nautilus_dbus_manager_register   (NautilusDBusManager *self,
                                           GDBusConnection     *connection,
                                           GError             **error);
void     nautilus_dbus_manager_unregister (NautilusDBusManager *self);
