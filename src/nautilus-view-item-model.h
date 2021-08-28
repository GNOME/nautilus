#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-file.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW_ITEM_MODEL (nautilus_view_item_model_get_type())

G_DECLARE_FINAL_TYPE (NautilusViewItemModel, nautilus_view_item_model, NAUTILUS, VIEW_ITEM_MODEL, GObject)

NautilusViewItemModel * nautilus_view_item_model_new (NautilusFile *file,
                                                      guint         icon_size);

void nautilus_view_item_model_set_icon_size (NautilusViewItemModel *self,
                                             guint                 icon_size);

guint nautilus_view_item_model_get_icon_size (NautilusViewItemModel *self);

NautilusFile * nautilus_view_item_model_get_file (NautilusViewItemModel *self);

void nautilus_view_item_model_set_item_ui (NautilusViewItemModel *self,
                                           GtkWidget             *item_ui);

GtkWidget * nautilus_view_item_model_get_item_ui (NautilusViewItemModel *self);
void nautilus_view_item_model_file_changed (NautilusViewItemModel *self);

G_END_DECLS
