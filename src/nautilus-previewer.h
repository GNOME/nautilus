/*
 * nautilus-previewer: nautilus previewer DBus wrapper
 *
 * SPDX-FileCopyrightText: 2011, Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-types.h"

G_BEGIN_DECLS

void nautilus_previewer_call_show_file (const gchar        *uri,
                                        NautilusWindowSlot *window,
                                        gboolean            close_if_already_visible);

gboolean nautilus_previewer_is_visible (void);

void  nautilus_previewer_setup         (void);
void  nautilus_previewer_teardown      (GDBusConnection *connection);

G_END_DECLS
