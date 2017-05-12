#ifndef NAUTILUS_CREATE_TASK_H
#define NAUTILUS_CREATE_TASK_H

#include "nautilus-file-task.h"

#define NAUTILUS_TYPE_CREATE_TASK (nautilus_create_task_get_type ())

G_DECLARE_DERIVABLE_TYPE (NautilusCreateTask, nautilus_create_task,
                          NAUTILUS, CREATE_TASK,
                          NautilusFileTask)

struct _NautilusCreateTaskClass
{
    NautilusFileTaskClass parent_class;
};

#endif
