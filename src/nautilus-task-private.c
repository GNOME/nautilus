#include "nautilus-task-private.h"

#include <glib-object.h>
#include <gobject/gvaluecollector.h>

typedef struct
{
    GValue instance_and_params[3];
    guint signal_id;
    int n_values;
} EmissionData;

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

void
nautilus_emit_signal_in_main_context (gpointer instance,
                                      guint    signal_id,
                                      ...)
{
    va_list ap;
    EmissionData *emission_data;
    GSignalQuery query;
    g_autofree gchar *error = NULL;

    emission_data = g_new0 (EmissionData, 1);

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

    g_main_context_invoke (NULL, emit_signal, emission_data);

    va_end (ap);
}
