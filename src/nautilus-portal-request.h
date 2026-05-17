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
 *       Alexander Larsson <alexl@redhat.com>
 *       Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#include "xdg-desktop-portal-dbus.h"

#define NAUTILUS_TYPE_PORTAL_REQUEST (nautilus_portal_request_get_type ())
G_DECLARE_FINAL_TYPE (NautilusPortalRequest,
                      nautilus_portal_request,
                      NAUTILUS, PORTAL_REQUEST,
                      XdpImplRequestSkeleton)

NautilusPortalRequest *
nautilus_portal_request_new (const char *sender,
                             const char *app_id,
                             const char *id);

void
nautilus_portal_request_export (NautilusPortalRequest *request,
                                GDBusConnection       *connection);
void
nautilus_portal_request_unexport (NautilusPortalRequest *request);
