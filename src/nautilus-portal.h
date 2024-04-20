/*
 * Copyright (C) 2024 GNOME Foundation Inc.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * Original Author: António Fernandes <antoniof@gnome.org>
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PORTAL (nautilus_portal_get_type())

G_DECLARE_FINAL_TYPE (NautilusPortal, nautilus_portal, NAUTILUS, PORTAL, GObject)

NautilusPortal * nautilus_portal_new (void);

void nautilus_portal_unregister (NautilusPortal *self);
gboolean nautilus_portal_register (NautilusPortal   *self,
                                   GDBusConnection  *connection,
                                   GError          **error);

G_END_DECLS
