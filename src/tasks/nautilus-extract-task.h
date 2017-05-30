#ifndef NAUTILUS_EXTRACT_TASK_H
#define NAUTILUS_EXTRACT_TASK_H

#include "nautilus-file-task.h"

#include <gtk/gtk.h>

#define NAUTILUS_TYPE_EXTRACT_TASK (nautilus_extract_task_get_type ())

G_DECLARE_FINAL_TYPE (NautilusExtractTask, nautilus_extract_task,
                      NAUTILUS, EXTRACT_TASK,
                      NautilusFileTask)

GList *nautilus_extract_task_get_output_files (NautilusExtractTask *self);

NautilusTask *nautilus_extract_task_new (GtkWindow *parent_window,
                                         GList     *source_files,
                                         GFile     *destination_directory);

#endif
