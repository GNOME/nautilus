/*
 * SPDX-FileCopyrightText: 2017 Alexandru Pandelea <alexandru.pandelea@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_TAG_MANAGER (nautilus_tag_manager_get_type ())

G_DECLARE_FINAL_TYPE (NautilusTagManager, nautilus_tag_manager, NAUTILUS, TAG_MANAGER, GObject);

NautilusTagManager* nautilus_tag_manager_new                (void);
NautilusTagManager* nautilus_tag_manager_new_dummy          (void);
NautilusTagManager* nautilus_tag_manager_get                (void);

GList*              nautilus_tag_manager_get_starred_files (NautilusTagManager *self);

void                nautilus_tag_manager_announce_unstarred_cb (GObject      *object,
                                                                GAsyncResult *result,
                                                                gpointer      user_data);
void                nautilus_tag_manager_announce_starred_cb   (GObject      *object,
                                                                GAsyncResult *result,
                                                                gpointer      user_data);

void                nautilus_tag_manager_star_files         (NautilusTagManager  *self,
                                                             GObject             *object,
                                                             GList               *selection,
                                                             GAsyncReadyCallback  callback,
                                                             gpointer             user_data,
                                                             GCancellable        *cancellable);

void                nautilus_tag_manager_unstar_files       (NautilusTagManager  *self,
                                                             GObject             *object,
                                                             GList               *selection,
                                                             GAsyncReadyCallback  callback,
                                                             gpointer             user_data,
                                                             GCancellable        *cancellable);


gboolean            nautilus_tag_manager_file_is_starred   (NautilusTagManager *self,
                                                            const gchar        *file_uri);

gboolean            nautilus_tag_manager_can_star_contents (NautilusTagManager *self,
                                                            GFile              *directory);
gboolean            nautilus_tag_manager_can_star_location (NautilusTagManager *self,
                                                            GFile              *directory);

void                nautilus_tag_manager_update_moved_uris  (NautilusTagManager *tag_manager,
                                                             GFile              *src,
                                                             GFile              *dest);

G_END_DECLS
