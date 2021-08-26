#pragma once

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-files-view.h"
#include "nautilus-window-slot.h"
#include "nautilus-view-model.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_VIEW_ICON_CONTROLLER (nautilus_view_icon_controller_get_type())

G_DECLARE_FINAL_TYPE (NautilusViewIconController, nautilus_view_icon_controller, NAUTILUS, VIEW_ICON_CONTROLLER, NautilusFilesView)

NautilusViewIconController *nautilus_view_icon_controller_new (NautilusWindowSlot *slot);

G_END_DECLS
