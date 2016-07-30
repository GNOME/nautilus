#ifndef NAUTILUS_RENAME_FILE_POPOVER_CONTROLLER_H
#define NAUTILUS_RENAME_FILE_POPOVER_CONTROLLER_H

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-file-name-widget-controller.h"
#include "nautilus-file.h"

#define NAUTILUS_TYPE_RENAME_FILE_POPOVER_CONTROLLER nautilus_rename_file_popover_controller_get_type ()
G_DECLARE_FINAL_TYPE (NautilusRenameFilePopoverController, nautilus_rename_file_popover_controller, NAUTILUS, RENAME_FILE_POPOVER_CONTROLLER, NautilusFileNameWidgetController)

NautilusRenameFilePopoverController * nautilus_rename_file_popover_controller_new (NautilusFile *target_file,
                                                                                   GdkRectangle *pointing_to,
                                                                                   GtkWidget    *relative_to);

NautilusFile * nautilus_rename_file_popover_controller_get_target_file (NautilusRenameFilePopoverController *controller);

#endif /* NAUTILUS_RENAME_FILE_POPOVER_CONTROLLER_H */
