#ifndef NAUTILUS_VIEW_LIST_CONTROLLER_H
#define NAUTILUS_VIEW_LIST_CONTROLLER_H

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-files-view.h"
#include "nautilus-window-slot.h"
#include "nautilus-view-model.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW_LIST_CONTROLLER (nautilus_view_list_controller_get_type())

G_DECLARE_FINAL_TYPE (NautilusViewListController, nautilus_view_list_controller, NAUTILUS, VIEW_LIST_CONTROLLER, NautilusFilesView)

NautilusViewListController *nautilus_view_list_controller_new (NautilusWindowSlot *slot);

NautilusViewModel * nautilus_view_list_controller_get_model (NautilusViewListController *self);

G_END_DECLS

#endif /* NAUTILUS_VIEW_LIST_CONTROLLER_H */

