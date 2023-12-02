/*
 * Copyright (C) 2023 António Fernandes <antoniof@gnome.org>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_FD_HOLDER (nautilus_fd_holder_get_type())

G_DECLARE_FINAL_TYPE (NautilusFdHolder, nautilus_fd_holder, NAUTILUS, FD_HOLDER, GObject)

NautilusFdHolder *nautilus_fd_holder_new                (void);
void              nautilus_fd_holder_set_location       (NautilusFdHolder *self,
                                                         GFile            *location);
void              nautilus_fd_holders_release_for_mount (GMount           *mount);

G_END_DECLS
