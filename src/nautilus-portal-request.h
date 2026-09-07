/*
 * SPDX-FileCopyrightText: 2016 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Matthias Clasen <mclasen@redhat.com>
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
