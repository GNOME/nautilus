#ifndef NAUTILUS_TASK_MANAGER_H
#define NAUTILUS_TASK_MANAGER_H

#include "nautilus-task.h"

#include <glib-object.h>

#define NAUTILUS_TYPE_TASK_MANAGER (nautilus_task_manager_get_type ())

G_DECLARE_FINAL_TYPE (NautilusTaskManager, nautilus_task_manager,
                      NAUTILUS, TASK_MANAGER,
                      GObject)

void nautilus_task_manager_queue_task (NautilusTaskManager  *self,
                                       NautilusTask         *task);

NautilusTaskManager *nautilus_task_manager_dup_singleton (void);

#endif
