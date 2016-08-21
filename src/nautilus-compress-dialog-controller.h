#ifndef NAUTILUS_COMPRESS_DIALOG_CONTROLLER_H
#define NAUTILUS_COMPRESS_DIALOG_CONTROLLER_H

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-file-name-widget-controller.h"
#include "nautilus-directory.h"

#define NAUTILUS_TYPE_COMPRESS_DIALOG_CONTROLLER nautilus_compress_dialog_controller_get_type ()
G_DECLARE_FINAL_TYPE (NautilusCompressDialogController, nautilus_compress_dialog_controller, NAUTILUS, COMPRESS_DIALOG_CONTROLLER, NautilusFileNameWidgetController)

NautilusCompressDialogController * nautilus_compress_dialog_controller_new (GtkWindow         *parent_window,
                                                                            NautilusDirectory *destination_directory,
                                                                            gchar             *initial_name);

#endif /* NAUTILUS_COMPRESS_DIALOG_CONTROLLER_H */
