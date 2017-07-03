#ifndef NAUTILUS_TASK_PRIVATE_H
#define NAUTILUS_TASK_PRIVATE_H

#include <glib.h>

void nautilus_emit_signal_in_main_context (gpointer instance,
                                           guint    signal_id,
                                           ...);

#endif
