/* Copyright (C) 2017 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Nautilus.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef NAUTILUS_TASK_H_INCLUDED
#define NAUTILUS_TASK_H_INCLUDED

#include "nautilus-scheduler.h"

#include <gio/gio.h>
#include <glib-object.h>

#define NAUTILUS_TYPE_TASK (nautilus_task_get_type ())

G_DECLARE_FINAL_TYPE (NautilusTask, nautilus_task, NAUTILUS, TASK, GObject)

/**
 * NautilusTaskFunc:
 * @task: a #NautilusTask instance
 * @task_data: (nullable): task data
 */
typedef void (*NautilusTaskFunc) (NautilusTask *task,
                                  gpointer      task_data);

/**
 * nautilus_task_get_main_context:
 * @task: an initialized #NautilusTask
 *
 * Returns: (nullable) (transfer full): the main context set or %NULL
 */
GMainContext *nautilus_task_get_main_context (NautilusTask *task);
/**
 * nautilus_task_set_main_context:
 * @task: an initialized #NautilusTask
 * @context: (transfer full): the main context
 */
void          nautilus_task_set_main_context (NautilusTask *task,
                                              GMainContext *context);

/**
 * nautilus_task_add_callback:
 * @task: an initialized #NautilusTask
 * @callback: the function to call when @task completes
 * @user_data: (nullable): additional data to pass to @callback
 */
void nautilus_task_add_callback (NautilusTask     *task,
                                 NautilusTaskFunc  callback,
                                 gpointer          user_data);

/**
 * nautilus_task_complete:
 * @task: an initialized #NautilusTask
 */
void nautilus_task_complete (NautilusTask *task);

/**
 * nautilus_task_get_result:
 * @task: an initialized #NautilusTask
 *
 * Returns: (nullable) (transfer full): the set result or %NULL
 */
GValue *nautilus_task_get_result (NautilusTask *task);
/**
 * nautilus_task_set_result:
 * @task: an initialized #NautilusTask
 * @type: the #GType of @result
 * @result: the result
 */
void    nautilus_task_set_result (NautilusTask *task,
                                  GType         type,
                                  gpointer      result);

/**
 * nautilus_task_get_error:
 * @task: an initialized #NautilusTask
 *
 * Returns: (nullable) (transfer full): the set #GError or %NULL
 */
GError *nautilus_task_get_error (NautilusTask *task);
/**
 * nautilus_task_set_error:
 * @task: an initialized #NautilusTask
 * @error: (transfer full): a #GError
 */
void    nautilus_task_set_error (NautilusTask *task,
                                 GError       *error);

/**
 * nautilus_task_set_scheduler:
 * @task: an initialized #NautilusTask
 * @scheduler: (transfer full): the scheduler to use
 */
void nautilus_task_set_scheduler (NautilusTask      *task,
                                  NautilusScheduler *scheduler);

/**
 * nautilus_task_run:
 * @task: an initialized #NautilusTask
 *
 * Schedules the task to be run asynchronously.
 */
void nautilus_task_run (NautilusTask *task);

/**
 * nautilus_task_get_cancellable:
 * @task: an initialized #NautilusTask
 *
 * Returns: (nullable) (transfer full): a #GCancellable for @task
 */
GCancellable *nautilus_task_get_cancellable (NautilusTask *task);

/**
 * nautilus_task_new_with_closure:
 * @closure: (transfer full): the closure to invoke when executing
 * @cancellable: (nullable) (transfer full): an initialized #GCancellable or %NULL
 *
 * Returns: a new #NautilusTask instance
 */
NautilusTask *nautilus_task_new_with_closure (GClosure         *closure,
                                              GCancellable     *cancellable);
/**
 * nautilus_task_new_with_func:
 * @func: the function to call when executing
 * @func_data: (nullable): data to pass to @func
 * @cancellable: (nullable) (transfer full): an initialized #GCancellable or %NULL
 *
 * Returns: a new #NautilusTask instance
 */
NautilusTask *nautilus_task_new_with_func    (NautilusTaskFunc  func,
                                              gpointer          func_data,
                                              GCancellable     *cancellable);
/**
 * nautilus_task_new:
 * @cancellable: (nullable) (transfer full): an initialized #GCancellable or %NULL
 *
 * Returns: a new #NautilusTask instance
 */
NautilusTask *nautilus_task_new              (GCancellable     *cancellable);

#endif
