/*
 * nautilus-dbus-manager: nautilus DBus interface
 *
 * Copyright (C) 2010, Red Hat, Inc.
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
