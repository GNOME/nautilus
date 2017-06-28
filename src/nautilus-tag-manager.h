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

#ifndef NAUTILUS_TAG_MANAGER_H
#define NAUTILUS_TAG_MANAGER_H

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_TAG_MANAGER (nautilus_tag_manager_get_type ())

G_DECLARE_FINAL_TYPE (NautilusTagManager, nautilus_tag_manager, NAUTILUS, TAG_MANAGER, GObject);

typedef struct
{
    gchar *id;
    gchar *name;
    gchar *url;
} TagData;

NautilusTagManager* nautilus_tag_manager_new (GCancellable *cancellable_tags,
                                              GCancellable *cancellable_notifier,
                                              GCancellable *cancellable_favorite);

GList* nautilus_tag_manager_get_favorite_files (NautilusTagManager *self);

GQueue* nautilus_tag_manager_get_all_tags (NautilusTagManager *self);

gpointer nautilus_tag_manager_gpointer_task_finish (GObject     *source_object,
                                                    GAsyncResult *res,
                                                    GError      **error);

void nautilus_tag_manager_get_selection_tags (GObject            *object,
                                              GList              *selection,
                                              GAsyncReadyCallback callback,
                                              GCancellable       *cancellable);

void nautilus_tag_manager_get_files_with_tag (NautilusTagManager  *self,
                                              GObject             *object,
                                              const gchar         *tag_name,
                                              GAsyncReadyCallback  callback,
                                              GCancellable        *cancellable);

void nautilus_tag_manager_remove_tag (NautilusTagManager  *self,
                                      GObject             *object,
                                      const gchar         *tag_name,
                                      GAsyncReadyCallback  callback,
                                      GCancellable        *cancellable);

gboolean nautilus_tag_manager_update_tags_finish (GObject      *source_object,
                                                  GAsyncResult *res,
                                                  GError       **error);

void nautilus_tag_manager_update_tags (NautilusTagManager *self,
                                       GObject            *object,
                                       GList              *selection,
                                       GQueue             *selection_tags,
                                       GQueue             *new_selection_tags,
                                       GAsyncReadyCallback callback,
                                       GCancellable       *cancellable);

void nautilus_tag_manager_star_files (NautilusTagManager  *self,
                                      GObject             *object,
                                      GList               *selection,
                                      GAsyncReadyCallback  callback,
                                      GCancellable        *cancellable);

void nautilus_tag_manager_unstar_files (NautilusTagManager  *self,
                                        GObject             *object,
                                        GList               *selection,
                                        GAsyncReadyCallback  callback,
                                        GCancellable        *cancellable);


gboolean nautilus_tag_manager_file_is_favorite (NautilusTagManager *self,
                                                const gchar        *file_name);

GQueue* nautilus_tag_copy_tag_queue (GQueue *queue);

gboolean nautilus_tag_queue_has_tag (GQueue      *selection_tags,
                                     const gchar *tag_name);

void nautilus_tag_data_free (gpointer data);

gchar* parse_color_from_tag_id (const gchar *tag_id);

TagData* nautilus_tag_data_new (const gchar *id,
                                const gchar *name,
                                const gchar *url);



G_END_DECLS

#endif