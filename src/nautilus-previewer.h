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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#pragma once

#include <gio/gio.h>
#include <glib.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

void nautilus_previewer_call_show_file (const gchar *uri,
                                        GtkRoot     *window,
                                        gboolean     close_if_already_visible);
void nautilus_previewer_call_close     (void);

gboolean nautilus_previewer_is_visible (void);

void  nautilus_previewer_setup         (void);
void  nautilus_previewer_teardown      (GDBusConnection *connection);

G_END_DECLS
