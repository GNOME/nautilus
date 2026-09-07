/*
 * nautilus-progress-persistence-handler.h: file operation progress systray icon or notification handler.
 *
 * SPDX-FileCopyrightText: 2007, 2011 Red Hat, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Authors: Alexander Larsson <alexl@redhat.com>
 *          Cosimo Cecchi <cosimoc@redhat.com>
 */

#pragma once

#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PROGRESS_PERSISTENCE_HANDLER nautilus_progress_persistence_handler_get_type()
G_DECLARE_FINAL_TYPE (NautilusProgressPersistenceHandler, nautilus_progress_persistence_handler, NAUTILUS, PROGRESS_PERSISTENCE_HANDLER, GObject)

/* @app is actually a NautilusApplication, but we need to avoid circular dependencies */
NautilusProgressPersistenceHandler * nautilus_progress_persistence_handler_new (GObject *app);

G_END_DECLS
