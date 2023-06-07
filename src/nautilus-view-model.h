#pragma once

#include <glib.h>
#include "nautilus-file.h"
#include "nautilus-view-item.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW_MODEL (nautilus_view_model_get_type())

G_DECLARE_FINAL_TYPE (NautilusViewModel, nautilus_view_model, NAUTILUS, VIEW_MODEL, GObject)

NautilusViewModel * nautilus_view_model_new (void);

GtkSorter *nautilus_view_model_get_sorter (NautilusViewModel *self);
void nautilus_view_model_set_sorter (NautilusViewModel *self,
                                     GtkSorter         *sorter);
void nautilus_view_model_sort (NautilusViewModel *self);
NautilusViewItem * nautilus_view_model_get_item_from_file (NautilusViewModel *self,
                                                           NautilusFile      *file);
GList * nautilus_view_model_get_items_from_files (NautilusViewModel *self,
                                                  GList             *files);
/* Don't use inside a loop, use nautilus_view_model_remove_all_items instead. */
void nautilus_view_model_remove_item (NautilusViewModel     *self,
                                      NautilusViewItem      *item,
                                      NautilusDirectory     *directory);
void nautilus_view_model_remove_all_items (NautilusViewModel *self);
/* Don't use inside a loop, use nautilus_view_model_add_items instead. */
void nautilus_view_model_add_item (NautilusViewModel     *self,
                                   NautilusViewItem *item);
void nautilus_view_model_add_items (NautilusViewModel *self,
                                    GList             *items);
guint nautilus_view_model_get_index (NautilusViewModel *self,
                                     NautilusViewItem  *item);
void nautilus_view_model_clear_subdirectory (NautilusViewModel *self,
                                             NautilusViewItem  *item);
void nautilus_view_model_expand_as_a_tree (NautilusViewModel *self,
                                           gboolean           expand_as_a_tree);

G_END_DECLS
