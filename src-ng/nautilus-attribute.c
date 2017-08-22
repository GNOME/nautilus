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

#include "nautilus-attribute.h"

struct _NautilusAttribute
{
    GRecMutex mutex;

    gpointer value;
    NautilusAttributeState state;

    NautilusTaskFunc update_func;
    NautilusTask *update_task;

    NautilusCopyFunc copy_func;
    GDestroyNotify destroy_func;

    GCancellable *cancellable;
};

G_DEFINE_TYPE (NautilusAttribute, nautilus_attribute, G_TYPE_OBJECT)

static void
finalize (GObject *object)
{
    NautilusAttribute *self;

    self = NAUTILUS_ATTRIBUTE (object);

    g_rec_mutex_clear (&self->mutex);

    if (self->value != NULL)
    {
        g_clear_pointer (&self->value, self->destroy_func);
    }

    g_cancellable_cancel (self->cancellable);

    g_object_unref (self->cancellable);

    G_OBJECT_CLASS (nautilus_attribute_parent_class)->finalize (object);
}

static void
nautilus_attribute_class_init (NautilusAttributeClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = finalize;
}

static void
nautilus_attribute_init (NautilusAttribute *self)
{
    g_rec_mutex_init (&self->mutex);

    self->update_task = NULL;
    self->cancellable = g_cancellable_new ();
}

typedef struct
{
    NautilusAttribute *attribute;

    NautilusAttributeUpdateValueCallback callback;
    gpointer user_data;
} GetValueCallbackDetails;

static void
update_task_callback (NautilusTask *task,
                      gpointer      user_data)
{
    GetValueCallbackDetails *details;

    details = user_data;

    g_rec_mutex_lock (&attribute->mutex);

    if (attribute->update_task == task)
    {
        if (attribute->state == NAUTILUS_ATTRIBUTE_STATE_PENDING)
        {
            attribute->state = NAUTILUS_ATTRIBUTE_STATE_VALID;
        }

        

        details->callback (details->attribute, nautilus_task_get_result (task),
                           details->user_data);
    }

    g_rec_mutex_lock (&attribute->mutex);

    g_object_unref (details->attribute);
    g_free (details);
}

void
nautilus_attribute_get_value (NautilusAttribute                    *attribute,
                              NautilusAttributeUpdateValueCallback  callback,
                              gpointer                              user_data)
{
    gpointer value;

    g_return_if_fail (NAUTILUS_IS_ATTRIBUTE (attribute));

    g_rec_mutex_lock (&attribute->mutex);

    switch (attribute->state)
    {
        case NAUTILUS_ATTRIBUTE_STATE_PENDING:
        {
            GetValueCallbackDetails *details;

            details = g_new0 (GetValueCallbackDetails, 1);

            details->attribute = g_object_ref (attribute);
            details->callback = callback;
            details->user_data = user_data;

            nautilus_task_add_callback (attribute->update_task, update_task_callback, details);
        }
        break;

        case NAUTILUS_ATTRIBUTE_STATE_VALID:
        {
            callback (attribute, attribute->copy_func (attribute->value), user_data);
        };
        break;

        case NAUTILUS_ATTRIBUTE_STATE_INVALID:
        {
            if (attribute->update_task != NULL)
            {
                g_object_unref (attribute->update_task);
            }

            attribute->update_task = nautilus_task_new_with_func (attribute->update_func,
                                                                  g_object_ref (attribute),
                                                                  g_object_unref,
                                                                  attribute->cancellable);

            nautilus_task_add_callback (
        }
        break;
    }

    g_rec_mutex_unlock (&attribute->mutex);
}

void
nautilus_attribute_set_value (NautilusAttribute *attribute,
                              gpointer           value)
{
    g_return_if_fail (NAUTILUS_IS_ATTRIBUTE (attribute));

    g_rec_mutex_lock (&attribute->mutex);

    if (attribute->value != NULL)
    {
        attribute->destroy_func (attribute->value);
    }

    attribute->value = attribute->copy_func (value);

    /* If an update is pending,
     * the new value divined shall be discarded after the state check.
     */
    attribute->state = NAUTILUS_ATTRIBUTE_STATE_VALID;

    g_rec_mutex_unlock (&attribute->mutex);
}

static gpointer
dummy_copy_func (gpointer data)
{
    return data;
}

static void
dummy_destroy_func (gpointer data)
{
    (void) data;
}

NautilusAttribute *
nautilus_attribute_new (NautilusTaskFunc update_func,
                        NautilusCopyFunc copy_func,
                        GDestroyNotify   destroy_func)
{
    NautilusAttribute *instance;

    g_return_val_if_fail (update_func != NULL, NULL);

    instance = g_object_new (NAUTILUS_TYPE_ATTRIBUTE, NULL);

    instance->update_func = update_func;
    if (copy_func == NULL)
    {
        copy_func = dummy_copy_func;
    }
    instance->copy_func = copy_func;
    if (destroy_func == NULL)
    {
        destroy_func = dummy_destroy_func;
    }
    instance->destroy_func = destroy_func;

    return instance;
}
