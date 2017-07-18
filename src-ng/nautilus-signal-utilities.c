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

#include "nautilus-signal-utilities.h"

#include <gobject/gvaluecollector.h>

typedef struct
{
    GValue *instance_and_params;
    guint signal_id;
    GQuark detail;
    gint n_values;
} EmissionData;

static void
emission_data_free (EmissionData *data)
{
    for (int i = 0; i < data->n_values; i++)
    {
        g_value_unset (&data->instance_and_params[i]);
    }

    g_free (data->instance_and_params);
    g_free (data);
}

static gboolean
emit_signal (gpointer data)
{
    EmissionData *emission_data;

    emission_data = data;

    g_signal_emitv (emission_data->instance_and_params,
                    emission_data->signal_id,
                    emission_data->detail,
                    NULL);

    emission_data_free (emission_data);

    return FALSE;
}

void nautilus_emit_signal_in_main_context_va_list (gpointer      instance,
                                                   GMainContext *main_context,
                                                   guint         signal_id,
                                                   GQuark        detail,
                                                   va_list       ap)
{
    GSignalQuery query;
    EmissionData *emission_data;
    g_autofree gchar *error = NULL;

    g_signal_query (signal_id, &query);

    if (query.signal_id == 0)
    {
        return;
    }

    emission_data = g_new0 (EmissionData, 1);

    emission_data->instance_and_params = g_new0 (GValue, query.n_params + 1);
    emission_data->signal_id = signal_id;
    emission_data->detail = detail;

    g_value_init (&emission_data->instance_and_params[0],
                  G_TYPE_FROM_INSTANCE (instance));
    g_value_set_instance (&emission_data->instance_and_params[0], instance);

    for (int i = 0; i < query.n_params; i++)
    {
        G_VALUE_COLLECT_INIT (&emission_data->instance_and_params[i + 1],
                              query.param_types[i],
                              ap, 0, &error);

        if (error != NULL)
        {
            emission_data_free (emission_data);

            return;
        }

        emission_data->n_values++;
    }

    g_main_context_invoke (main_context, emit_signal, emission_data);
}

void
nautilus_emit_signal_in_main_context_by_name (gpointer      instance,
                                              GMainContext *main_context,
                                              const gchar  *detailed_signal,
                                              ...)
{
    guint signal_id;
    GQuark detail;
    gboolean signal_is_valid;
    va_list ap;

    g_return_if_fail (G_IS_OBJECT (instance));

    signal_is_valid = g_signal_parse_name (detailed_signal,
                                           G_TYPE_FROM_INSTANCE (instance),
                                           &signal_id,
                                           &detail,
                                           TRUE);

    g_return_if_fail (signal_is_valid);

    va_start (ap, detailed_signal);

    nautilus_emit_signal_in_main_context_va_list (instance, main_context,
                                                  signal_id, detail, ap);

    va_end (ap);
}

void
nautilus_emit_signal_in_main_context (gpointer      instance,
                                      GMainContext *main_context,
                                      guint         signal_id,
                                      GQuark        detail,
                                      ...)
{
    va_list ap;

    va_start (ap, detail);

    nautilus_emit_signal_in_main_context_va_list (instance,
                                                  main_context,
                                                  signal_id,
                                                  detail,
                                                  ap);

    va_end (ap);
}
