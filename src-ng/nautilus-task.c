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

#include "nautilus-signal-utilities.h"

typedef struct
{
    GCancellable *cancellable;
    GMainContext *context;
} NautilusTaskPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (NautilusTask, nautilus_task,
                                     G_TYPE_OBJECT)

enum
{
    PROP_CANCELLABLE = 1,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL };

static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    switch (property_id)
    {
        case PROP_CANCELLABLE:
        {
            NautilusTask *self;
            NautilusTaskPrivate *priv;

            self = NAUTILUS_TASK (object);
            priv = nautilus_task_get_instance_private (self);

            if (G_UNLIKELY (priv->cancellable) != NULL)
            {
                g_clear_object (&priv->cancellable);
            }

            priv->cancellable = g_value_dup_object (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
finalize (GObject *object)
{
    NautilusTask *self;
    NautilusTaskPrivate *priv;

    self = NAUTILUS_TASK (object);
    priv = nautilus_task_get_instance_private (self);

    g_clear_object (&priv->cancellable);
    g_clear_pointer (&priv->context, g_main_context_unref);

    G_OBJECT_CLASS (nautilus_task_parent_class)->finalize (object);
}

static void
nautilus_task_class_init (NautilusTaskClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = set_property;
    object_class->finalize = finalize;

    properties[PROP_CANCELLABLE] =
        g_param_spec_object ("cancellable", "Cancellable", "Cancellable",
                             G_TYPE_CANCELLABLE,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
nautilus_task_init (NautilusTask *self)
{
    NautilusTaskPrivate *priv;

    priv = nautilus_task_get_instance_private (self);

    priv->context = g_main_context_ref_thread_default ();
}

GCancellable *
nautilus_task_get_cancellable (NautilusTask *task)
{
    NautilusTaskPrivate *priv;

    g_return_val_if_fail (NAUTILUS_TASK (task), NULL);

    priv = nautilus_task_get_instance_private (task);

    if (priv->cancellable == NULL)
    {
        return NULL;
    }

    return g_object_ref (priv->cancellable);
}

void
nautilus_task_execute (NautilusTask *task)
{
    NautilusTaskClass *klass;

    g_return_if_fail (NAUTILUS_IS_TASK (task));

    klass = NAUTILUS_TASK_GET_CLASS (task);

    g_return_if_fail (klass->execute != NULL);

    klass->execute (task);
}

void
nautilus_task_emit_signal_in_main_context (NautilusTask *task,
                                           guint         signal_id,
                                           GQuark        detail,
                                           ...)
{
    NautilusTaskPrivate *priv;
    va_list ap;

    g_return_if_fail (NAUTILUS_IS_TASK (task));

    priv = nautilus_task_get_instance_private (task);
    va_start (ap, detail);

    nautilus_emit_signal_in_main_context_va_list (task,
                                                  priv->context,
                                                  signal_id,
                                                  detail,
                                                  ap);

    va_end (ap);
}
