/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
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

#ifndef __NAUTILUS_PROGRESS_PERSISTENCE_HANDLER_H__
#define __NAUTILUS_PROGRESS_PERSISTENCE_HANDLER_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_PROGRESS_PERSISTENCE_HANDLER nautilus_progress_persistence_handler_get_type()
#define NAUTILUS_PROGRESS_PERSISTENCE_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_PROGRESS_PERSISTENCE_HANDLER, NautilusProgressPersistenceHandler))
#define NAUTILUS_PROGRESS_PERSISTENCE_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_PROGRESS_PERSISTENCE_HANDLER, NautilusProgressPersistenceHandlerClass))
#define NAUTILUS_IS_PROGRESS_PERSISTENCE_HANDLER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_PROGRESS_PERSISTENCE_HANDLER))
#define NAUTILUS_IS_PROGRESS_PERSISTENCE_HANDLER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_PROGRESS_PERSISTENCE_HANDLER))
#define NAUTILUS_PROGRESS_PERSISTENCE_HANDLER_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_PROGRESS_PERSISTENCE_HANDLER, NautilusProgressPersistenceHandlerClass))

typedef struct _NautilusProgressPersistenceHandlerPriv NautilusProgressPersistenceHandlerPriv;

typedef struct {
  GObject parent;

  /* private */
  NautilusProgressPersistenceHandlerPriv *priv;
} NautilusProgressPersistenceHandler;

typedef struct {
  GObjectClass parent_class;
} NautilusProgressPersistenceHandlerClass;

GType nautilus_progress_persistence_handler_get_type (void);

/* @app is actually a NautilusApplication, but we need to avoid circular dependencies */
NautilusProgressPersistenceHandler * nautilus_progress_persistence_handler_new (GObject *app);
void nautilus_progress_persistence_handler_make_persistent (NautilusProgressPersistenceHandler *self);

G_END_DECLS

#endif /* __NAUTILUS_PROGRESS_PERSISTENCE_HANDLER_H__ */
