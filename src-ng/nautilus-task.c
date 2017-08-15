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

#include "nautilus-task.h"

#include "nautilus-context-scheduler.h"

struct _NautilusTask
{
    GObject parent_instance;

    NautilusScheduler *scheduler;

    GClosure *closure;
    GCancellable *cancellable;
    GError *error;

    GValue result;

    GMainContext *context;
    GQueue *callbacks;
};

G_DEFINE_TYPE (NautilusTask, nautilus_task, G_TYPE_OBJECT)

static void
finalize (GObject *object)
{
    NautilusTask *self;

    self = NAUTILUS_TASK (object);

    if (self->closure != NULL)
    {
        g_clear_pointer (&self->closure, g_closure_unref);
    }
    if (self->cancellable != NULL)
    {
        g_clear_object (&self->cancellable);
    }
    if (self->error != NULL)
    {
        g_clear_pointer (&self->error, g_error_free);
    }

    g_queue_free (self->callbacks);

    G_OBJECT_CLASS (nautilus_task_parent_class)->finalize (object);
}

static void
nautilus_task_class_init (NautilusTaskClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = finalize;
}

static void
nautilus_task_init (NautilusTask *self)
{
    self->scheduler = nautilus_scheduler_get_default ();
    self->cancellable = NULL;
    self->error = NULL;
    self->context = NULL;
    self->callbacks = g_queue_new ();
}

GMainContext *
nautilus_task_get_main_context (NautilusTask *task)
{
    g_return_val_if_fail (NAUTILUS_IS_TASK (task), NULL);

    if (task->context == NULL)
    {
        return NULL;
    }

    return g_main_context_ref (task->context);
}

void
nautilus_task_set_main_context (NautilusTask *task,
                                GMainContext *context)
{
    g_return_if_fail (NAUTILUS_IS_TASK (task));
    g_return_if_fail (context != NULL);

    if (task->context != NULL)
    {
        g_main_context_unref (task->context);
    }

    task->context = g_main_context_ref (context);
}

static void
nautilus_task_add_callback_closure (NautilusTask *task,
                                    GClosure     *closure)
{
    g_queue_push_tail (task->callbacks, g_closure_ref (closure));
}

void
nautilus_task_add_callback (NautilusTask     *task,
                            NautilusTaskFunc  callback,
                            gpointer          user_data)
{
    g_autoptr (GClosure) closure = NULL;

    g_return_if_fail (NAUTILUS_IS_TASK (task));
    g_return_if_fail (callback != NULL);

    closure = g_cclosure_new (G_CALLBACK (callback), user_data, NULL);

    g_closure_set_marshal (closure, g_cclosure_marshal_VOID__VOID);

    nautilus_task_add_callback_closure (task, closure);
}

static void
invoke_callbacks_iteration (gpointer data)
{
    NautilusTask *task;
    g_autoptr (GClosure) closure = NULL;
    GValue params = { 0 };

    task = data;
    closure = g_queue_pop_head (task->callbacks);

    g_value_init (&params, G_TYPE_OBJECT);
    g_value_set_object (&params, task);
    g_closure_invoke (closure, NULL, 1, &params, NULL);
    g_closure_invalidate (closure);
    g_value_unset (&params);
}

void
nautilus_task_complete (NautilusTask *task)
{
    g_return_if_fail (NAUTILUS_IS_TASK (task));

    while (g_queue_peek_head (task->callbacks) != NULL)
    {
        if (task->context != NULL)
        {
            NautilusScheduler *context_scheduler;

            context_scheduler = nautilus_context_scheduler_get_for_context (task->context);

            nautilus_scheduler_queue (context_scheduler, invoke_callbacks_iteration, task);
        }
        else
        {
            invoke_callbacks_iteration (task);
        }
    }
}

GValue *
nautilus_task_get_result (NautilusTask *task)
{
    g_return_val_if_fail (NAUTILUS_IS_TASK (task), NULL);

    return &task->result;
}

void
nautilus_task_set_result (NautilusTask *task,
                          GType         type,
                          gpointer      result)
{
    g_return_if_fail (NAUTILUS_IS_TASK (task));
    g_return_if_fail (type != G_TYPE_INVALID);
    g_return_if_fail (result != NULL);

    if (G_VALUE_TYPE (&task->result) != G_TYPE_INVALID)
    {
        g_value_unset (&task->result);
    }

    g_value_init (&task->result, type);
    g_value_set_instance (&task->result, result);
}

GError *
nautilus_task_get_error (NautilusTask *task)
{
    GError *error;

    g_return_val_if_fail (NAUTILUS_IS_TASK (task), NULL);

    error = task->error;
    task->error = NULL;

    return error;
}

void
nautilus_task_set_error (NautilusTask *task,
                         GError       *error)
{
    g_return_if_fail (NAUTILUS_IS_TASK (task));
    g_return_if_fail (error != NULL);

    task->error = error;
}

void
nautilus_task_set_scheduler (NautilusTask          *task,
                             NautilusScheduler *scheduler)
{
    g_return_if_fail (NAUTILUS_IS_TASK (task));
    g_return_if_fail (NAUTILUS_IS_SCHEDULER (scheduler));

    g_object_unref (task->scheduler);

    task->scheduler = g_object_ref (scheduler);
}

static void
nautilus_task_execute (gpointer user_data)
{
    NautilusTask *task;
    GValue params = { 0 };

    task = user_data;

    g_value_init (&params, G_TYPE_OBJECT);
    g_value_set_object (&params, task);
    g_closure_invoke (task->closure, NULL, 1, &params, NULL);
    g_closure_invalidate (task->closure);
    g_clear_pointer (&task->closure, g_closure_unref);
    g_value_unset (&params);
}

void
nautilus_task_run (NautilusTask *task)
{
    g_return_if_fail (NAUTILUS_IS_TASK (task));

    nautilus_scheduler_queue (task->scheduler,
                              NAUTILUS_CALLBACK (nautilus_task_execute),
                              g_object_ref (task));
}

GCancellable *
nautilus_task_get_cancellable (NautilusTask *task)
{
    g_return_val_if_fail (NAUTILUS_IS_TASK (task), NULL);

    return g_object_ref (task->cancellable);
}

NautilusTask *
nautilus_task_new_with_closure (GClosure     *closure,
                                GCancellable *cancellable)
{
    NautilusTask *instance;

    g_return_val_if_fail (closure != NULL, NULL);
    if (cancellable != NULL)
    {
        g_return_val_if_fail (G_IS_CANCELLABLE (cancellable), NULL);
    }

    instance = g_object_new (NAUTILUS_TYPE_TASK, NULL);

    instance->closure = g_closure_ref (closure);
    if (cancellable != NULL)
    {
        instance->cancellable = g_object_ref (cancellable);
    }

    return instance;
}

/* Would be nice to accept a #GDestroyNotify for @user_data,
 * but it’s not a #GClosureNotify, ergo UB.
 */
NautilusTask *
nautilus_task_new_with_func (NautilusTaskFunc  func,
                             gpointer          func_data,
                             GCancellable     *cancellable)
{
    g_autoptr (GClosure) closure = NULL;

    g_return_val_if_fail (func != NULL, NULL);

    closure = g_cclosure_new (G_CALLBACK (func), func_data, NULL);

    g_closure_set_marshal (closure, g_cclosure_marshal_VOID__VOID);

    return nautilus_task_new_with_closure (closure, cancellable);
}

static void
dummy_task_func (NautilusTask *task,
                 gpointer      user_data)
{
    (void) task;
    (void) user_data;
}

NautilusTask *
nautilus_task_new (GCancellable *cancellable)
{
    return nautilus_task_new_with_func (dummy_task_func, NULL, cancellable);
}
