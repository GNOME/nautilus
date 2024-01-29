/*
 * nautilus-progress-persistence-handler.h: file operation progress systray icon or notification handler.
 *
 * Copyright (C) 2007, 2011 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PROGRESS_PERSISTENCE_HANDLER nautilus_progress_persistence_handler_get_type()
G_DECLARE_FINAL_TYPE (NautilusProgressPersistenceHandler, nautilus_progress_persistence_handler, NAUTILUS, PROGRESS_PERSISTENCE_HANDLER, GObject)

/* @app is actually a NautilusApplication, but we need to avoid circular dependencies */
NautilusProgressPersistenceHandler * nautilus_progress_persistence_handler_new (GObject *app);
void nautilus_progress_persistence_handler_make_persistent (NautilusProgressPersistenceHandler *self);

G_END_DECLS
