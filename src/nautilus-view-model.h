#pragma once

#include <glib.h>
#include "nautilus-file.h"
#include "nautilus-view-item-model.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW_MODEL (nautilus_view_model_get_type())

G_DECLARE_FINAL_TYPE (NautilusViewModel, nautilus_view_model, NAUTILUS, VIEW_MODEL, GObject)

NautilusViewModel * nautilus_view_model_new (void);

void nautilus_view_model_set_sorter (NautilusViewModel *self,
                                     GtkSorter         *sorter);
NautilusViewItemModel * nautilus_view_model_get_item_from_file (NautilusViewModel *self,
                                                                NautilusFile      *file);
GQueue * nautilus_view_model_get_items_from_files (NautilusViewModel *self,
                                                   GQueue            *files);
/* Don't use inside a loop, use nautilus_view_model_remove_all_items instead. */
void nautilus_view_model_remove_item (NautilusViewModel     *self,
                                      NautilusViewItemModel *item);
void nautilus_view_model_remove_all_items (NautilusViewModel *self);
/* Don't use inside a loop, use nautilus_view_model_add_items instead. */
void nautilus_view_model_add_item (NautilusViewModel     *self,
                                   NautilusViewItemModel *item);
void nautilus_view_model_add_items (NautilusViewModel *self,
                                    GQueue            *items);
guint nautilus_view_model_get_index (NautilusViewModel     *self,
                                     NautilusViewItemModel *item);

G_END_DECLS
