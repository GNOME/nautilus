#ifndef NAUTILUS_NEW_FOLDER_DIALOG_CONTROLLER_H
#define NAUTILUS_NEW_FOLDER_DIALOG_CONTROLLER_H

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-file-name-widget-controller.h"
#include "nautilus-directory.h"

#define NAUTILUS_TYPE_NEW_FOLDER_DIALOG_CONTROLLER nautilus_new_folder_dialog_controller_get_type ()
G_DECLARE_FINAL_TYPE (NautilusNewFolderDialogController, nautilus_new_folder_dialog_controller, NAUTILUS, NEW_FOLDER_DIALOG_CONTROLLER, NautilusFileNameWidgetController)

NautilusNewFolderDialogController * nautilus_new_folder_dialog_controller_new (GtkWindow         *parent_window,
                                                                               NautilusDirectory *destination_directory,
                                                                               gboolean           with_selection,
                                                                               gchar             *initial_name);

gboolean nautilus_new_folder_dialog_controller_get_with_selection (NautilusNewFolderDialogController *controller);

#endif /* NAUTILUS_NEW_FOLDER_DIALOG_CONTROLLER_H */
