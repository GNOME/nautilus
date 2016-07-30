#ifndef NAUTILUS_FILE_NAME_WIDGET_CONTROLLER_H
#define NAUTILUS_FILE_NAME_WIDGET_CONTROLLER_H

#include <glib.h>
#include <gtk/gtk.h>

#include "nautilus-file.h"
#include "nautilus-directory.h"

#define NAUTILUS_TYPE_FILE_NAME_WIDGET_CONTROLLER nautilus_file_name_widget_controller_get_type ()
G_DECLARE_DERIVABLE_TYPE (NautilusFileNameWidgetController, nautilus_file_name_widget_controller, NAUTILUS, FILE_NAME_WIDGET_CONTROLLER, GObject)

struct _NautilusFileNameWidgetControllerClass
{
        GObjectClass parent_class;

        gchar *  (*get_new_name)         (NautilusFileNameWidgetController *controller);

        gboolean (*name_is_valid)        (NautilusFileNameWidgetController  *controller,
                                          gchar                             *name,
                                          gchar                            **error_message);

        gboolean (*ignore_existing_file) (NautilusFileNameWidgetController *controller,
                                          NautilusFile                     *existing_file);

        void     (*name_accepted)        (NautilusFileNameWidgetController *controller);
};

gchar * nautilus_file_name_widget_controller_get_new_name (NautilusFileNameWidgetController *controller);


#endif /* NAUTILUS_FILE_NAME_WIDGET_CONTROLLER_H */
