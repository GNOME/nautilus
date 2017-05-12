#ifndef NAUTILUS_VIEW_LIST_ITEM_UI_H
#define NAUTILUS_VIEW_LIST_ITEM_UI_H

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-view-item-model.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW_LIST_ITEM_UI (nautilus_view_list_item_ui_get_type())

G_DECLARE_FINAL_TYPE (NautilusViewListItemUi, nautilus_view_list_item_ui, NAUTILUS, VIEW_LIST_ITEM_UI, GtkListBoxRow)

NautilusViewListItemUi * nautilus_view_list_item_ui_new (NautilusViewItemModel *item_model);

NautilusViewItemModel * nautilus_view_list_item_ui_get_model (NautilusViewListItemUi *self);

G_END_DECLS

#endif /* NAUTILUS_VIEW_LIST_ITEM_UI_H */

