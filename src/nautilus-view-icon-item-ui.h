#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-view-item-model.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW_ICON_ITEM_UI (nautilus_view_icon_item_ui_get_type())

G_DECLARE_FINAL_TYPE (NautilusViewIconItemUi, nautilus_view_icon_item_ui, NAUTILUS, VIEW_ICON_ITEM_UI, GtkFlowBoxChild)

NautilusViewIconItemUi * nautilus_view_icon_item_ui_new (NautilusViewItemModel *item_model);

G_END_DECLS
