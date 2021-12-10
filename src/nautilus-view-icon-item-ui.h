#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-view-item-model.h"

G_BEGIN_DECLS

enum
{
    NAUTILUS_VIEW_ICON_FIRST_CAPTION,
    NAUTILUS_VIEW_ICON_SECOND_CAPTION,
    NAUTILUS_VIEW_ICON_THIRD_CAPTION,
    NAUTILUS_VIEW_ICON_N_CAPTIONS
};

#define NAUTILUS_TYPE_VIEW_ICON_ITEM_UI (nautilus_view_icon_item_ui_get_type())

G_DECLARE_FINAL_TYPE (NautilusViewIconItemUi, nautilus_view_icon_item_ui, NAUTILUS, VIEW_ICON_ITEM_UI, GtkFlowBoxChild)

NautilusViewIconItemUi * nautilus_view_icon_item_ui_new (void);
void nautilus_view_icon_item_ui_set_model (NautilusViewIconItemUi *self,
                                           NautilusViewItemModel  *model);
void nautilus_view_item_ui_set_caption_attributes (NautilusViewIconItemUi *self,
                                                   GQuark                 *attrs);

G_END_DECLS
