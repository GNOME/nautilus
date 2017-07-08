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
 * along with Nautilus.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nautilus-task.h"

#include <gobject/gvaluecollector.h>

typedef struct
{
    GCancellable *cancellable;
    GMainContext *context;
} NautilusTaskPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE (NautilusTask, nautilus_task,
                                     G_TYPE_OBJECT)

typedef struct
{
    GValue instance_and_params[4];
    guint signal_id;
    int n_values;
} EmissionData;

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
emission_data_free (EmissionData *data)
{
    for (int i = 0; i < data->n_values; i++)
    {
        g_value_unset (&data->instance_and_params[i]);
    }

    g_free (data);
}

static gboolean
emit_signal (gpointer data)
{
    EmissionData *emission_data;

    emission_data = data;

    g_signal_emitv (emission_data->instance_and_params,
                    emission_data->signal_id,
                    0, NULL);

    g_clear_pointer (&emission_data, emission_data_free);

    return FALSE;
}

static void
emit_signal_in_main_context (NautilusTask *instance,
                             guint         signal_id,
                             ...)
{
    va_list ap;
    EmissionData *emission_data;
    GSignalQuery query;
    g_autofree gchar *error = NULL;
    NautilusTaskPrivate *priv;

    emission_data = g_new0 (EmissionData, 1);
    priv = nautilus_task_get_instance_private (instance);

    va_start (ap, signal_id);

    g_value_init (&emission_data->instance_and_params[0],
                  G_TYPE_FROM_INSTANCE (instance));
    g_value_set_instance (&emission_data->instance_and_params[0], instance);

    emission_data->signal_id = signal_id;

    g_signal_query (signal_id, &query);

    if (query.signal_id == 0)
    {
        g_clear_pointer (&emission_data, emission_data_free);

        va_end (ap);

        return;
    }

    for (int i = 0; i < query.n_params; i++)
    {
        G_VALUE_COLLECT_INIT (&emission_data->instance_and_params[i + 1],
                              query.param_types[i],
                              ap, 0, &error);

        if (error != NULL)
        {
            break;
        }

        emission_data->n_values++;
    }

    if (error != NULL)
    {
        g_clear_pointer (&emission_data, emission_data_free);

        va_end (ap);

        return;
    }

    g_main_context_invoke (priv->context, emit_signal, emission_data);

    va_end (ap);
}

static void
nautilus_task_class_init (NautilusTaskClass *klass)
{
    GObjectClass *object_class;

    object_class = G_OBJECT_CLASS (klass);

    object_class->set_property = set_property;
    object_class->finalize = finalize;

    klass->emit_signal_in_main_context = emit_signal_in_main_context;

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
