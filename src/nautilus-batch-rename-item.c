/* nautilus-batch-rename-dialog.c
 *
 * Copyright (C) 2024–2025 Markus Göllnitz <camelcasenick@bewares.it>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "nautilus-batch-rename-item.h"

#include <config.h>

#include "nautilus-batch-rename-dialog.h"

#include <glib.h>

struct _NautilusBatchRenameItem
{
    GObject parent_instance;

    gchar *name_before;
    gchar *name_after;
    gboolean has_conflict;

    NautilusBatchRenameDialog *dialog;
};

enum
{
    PROP_0,

    PROP_NAME_BEFORE,
    PROP_NAME_AFTER,
    PROP_HAS_CONFLICT,
    PROP_DIALOG,

    NUM_PROPERTIES
};

G_DEFINE_FINAL_TYPE (NautilusBatchRenameItem, nautilus_batch_rename_item, G_TYPE_OBJECT)

static GParamSpec *props[NUM_PROPERTIES] = { NULL, };

static void
nautilus_batch_rename_item_finalize (GObject *object)
{
    NautilusBatchRenameItem *item = NAUTILUS_BATCH_RENAME_ITEM (object);

    g_free (item->name_before);
    g_free (item->name_after);
    g_clear_weak_pointer (&item->dialog);

    G_OBJECT_CLASS (nautilus_batch_rename_item_parent_class)->finalize (object);
}

static void
nautilus_batch_rename_item_get_property (GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
    NautilusBatchRenameItem *item = NAUTILUS_BATCH_RENAME_ITEM (object);

    switch (prop_id)
    {
        case PROP_NAME_BEFORE:
        {
            g_value_set_string (value, item->name_before);
            break;
        }

        case PROP_NAME_AFTER:
        {
            g_value_set_string (value, item->name_after);
            break;
        }

        case PROP_HAS_CONFLICT:
        {
            g_value_set_boolean (value, item->has_conflict);
            break;
        }

        case PROP_DIALOG:
        {
            g_value_set_object (value, item->dialog);
            break;
        }

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_batch_rename_item_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
    NautilusBatchRenameItem *item = NAUTILUS_BATCH_RENAME_ITEM (object);

    switch (prop_id)
    {
        case PROP_NAME_BEFORE:
        {
            nautilus_batch_rename_item_set_name_before (item, g_value_get_string (value));
            break;
        }

        case PROP_NAME_AFTER:
        {
            nautilus_batch_rename_item_set_name_after (item, g_value_get_string (value));
            break;
        }

        case PROP_HAS_CONFLICT:
        {
            nautilus_batch_rename_item_set_has_conflict (item, g_value_get_boolean (value));
            break;
        }

        case PROP_DIALOG:
        {
            if (g_set_weak_pointer (&item->dialog, NAUTILUS_BATCH_RENAME_DIALOG (g_value_get_object (value))))
            {
                g_object_notify_by_pspec (G_OBJECT (item), props[PROP_DIALOG]);
            }
            break;
        }

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        }
    }
}

static void
nautilus_batch_rename_item_class_init (NautilusBatchRenameItemClass *klass)
{
    GObjectClass *oclass = G_OBJECT_CLASS (klass);

    oclass->finalize = nautilus_batch_rename_item_finalize;

    oclass->get_property = nautilus_batch_rename_item_get_property;
    oclass->set_property = nautilus_batch_rename_item_set_property;

    props[PROP_NAME_BEFORE] =
        g_param_spec_string ("name-before",
                             NULL,
                             "",
                             "",
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    props[PROP_NAME_AFTER] =
        g_param_spec_string ("name-after",
                             NULL,
                             "",
                             "",
                             G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    props[PROP_HAS_CONFLICT] =
        g_param_spec_boolean ("has-conflict",
                              NULL,
                              "",
                              FALSE,
                              G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    props[PROP_DIALOG] =
        g_param_spec_object ("dialog",
                             NULL,
                             "",
                             NAUTILUS_TYPE_BATCH_RENAME_DIALOG,
                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

    g_object_class_install_properties (oclass, NUM_PROPERTIES, props);
}

static void
nautilus_batch_rename_item_init (NautilusBatchRenameItem *item)
{
}

NautilusBatchRenameItem *
nautilus_batch_rename_item_new (const gchar               *name_before,
                                const gchar               *name_after,
                                NautilusBatchRenameDialog *dialog)
{
    return g_object_new (NAUTILUS_TYPE_BATCH_RENAME_ITEM,
                         "name-before", name_before,
                         "name-after", name_after,
                         "dialog", dialog,
                         NULL);
}

void
nautilus_batch_rename_item_set_name_before (NautilusBatchRenameItem *item,
                                            const gchar             *name_before)
{
    if (g_set_str (&item->name_before, name_before))
    {
        g_object_notify_by_pspec (G_OBJECT (item), props[PROP_NAME_BEFORE]);
    }
}

void
nautilus_batch_rename_item_set_name_after (NautilusBatchRenameItem *item,
                                           const gchar             *name_after)
{
    if (g_set_str (&item->name_after, name_after))
    {
        g_object_notify_by_pspec (G_OBJECT (item), props[PROP_NAME_AFTER]);
    }
}

void
nautilus_batch_rename_item_set_has_conflict (NautilusBatchRenameItem *item,
                                             gboolean                 has_conflict)
{
    if (item->has_conflict != has_conflict)
    {
        item->has_conflict = has_conflict;

        g_object_notify_by_pspec (G_OBJECT (item), props[PROP_HAS_CONFLICT]);
    }
}
