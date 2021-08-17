#pragma once

#include <glib.h>
#include "nautilus-file.h"
#include "nautilus-view-item-model.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW_MODEL (nautilus_view_model_get_type())

G_DECLARE_FINAL_TYPE (NautilusViewModel, nautilus_view_model, NAUTILUS, VIEW_MODEL, GObject)

typedef struct
{
    NautilusFileSortType sort_type;
    gboolean reversed;
    gboolean directories_first;
} NautilusViewModelSortData;

NautilusViewModel * nautilus_view_model_new (void);

void nautilus_view_model_set_sort_type (NautilusViewModel         *self,
                                        NautilusViewModelSortData *sort_data);
NautilusViewModelSortData * nautilus_view_model_get_sort_type (NautilusViewModel *self);
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

G_END_DECLS
