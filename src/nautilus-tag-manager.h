/* nautilus-tag-manager.h
 *
 * Copyright (C) 2017 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_TAG_MANAGER (nautilus_tag_manager_get_type ())

G_DECLARE_FINAL_TYPE (NautilusTagManager, nautilus_tag_manager, NAUTILUS, TAG_MANAGER, GObject);

NautilusTagManager* nautilus_tag_manager_get                (void);

void                nautilus_tag_manager_set_cancellable    (NautilusTagManager *tag_manager,
                                                             GCancellable *cancellable);

GList*              nautilus_tag_manager_get_starred_files (NautilusTagManager *self);

void                nautilus_tag_manager_star_files         (NautilusTagManager  *self,
                                                             GObject             *object,
                                                             GList               *selection,
                                                             GAsyncReadyCallback  callback,
                                                             GCancellable        *cancellable);

void                nautilus_tag_manager_unstar_files       (NautilusTagManager  *self,
                                                             GObject             *object,
                                                             GList               *selection,
                                                             GAsyncReadyCallback  callback,
                                                             GCancellable        *cancellable);


gboolean            nautilus_tag_manager_file_is_starred   (NautilusTagManager *self,
                                                            const gchar        *file_uri);

G_END_DECLS
