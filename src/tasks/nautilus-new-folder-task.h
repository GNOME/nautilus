#ifndef NAUTILUS_NEW_FOLDER_TASK_H
#define NAUTILUS_NEW_FOLDER_TASK_H

#include "nautilus-create-task.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#define NAUTILUS_TYPE_NEW_FOLDER_TASK (nautilus_new_folder_task_get_type ())

G_DECLARE_FINAL_TYPE (NautilusNewFolderTask, nautilus_new_folder_task,
                      NAUTILUS, NEW_FOLDER_TASK,
                      NautilusCreateTask)

struct _NautilusNewFolderTaskClass
{
    NautilusCreateTaskClass parent_class;
};

NautilusTask *nautilus_new_folder_task_new (GtkWidget  *parent_view,
                                            GdkPoint   *target_point,
                                            const char *parent_dir,
                                            const char *folder_name);

#endif
