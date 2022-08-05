/*
 * Copyright (C) 2022 The GNOME project contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <config.h>
#include "nautilus-properties-model.h"
#include "nautilus-properties-item.h"

enum
{
    PROP_0,
    PROP_TITLE,
    PROP_MODEL,
    LAST_PROP
};

struct _NautilusPropertiesModel
{
    GObject parent_instance;

    char *title;
    GListModel *model;
};

G_DEFINE_TYPE (NautilusPropertiesModel, nautilus_properties_model, G_TYPE_OBJECT)

NautilusPropertiesModel *
nautilus_properties_model_new (const char *title,
                               GListModel *model)
{
    g_return_val_if_fail (title != NULL, NULL);
    g_return_val_if_fail (G_IS_LIST_MODEL (model), NULL);
    g_return_val_if_fail (g_list_model_get_item_type (model) == NAUTILUS_TYPE_PROPERTIES_ITEM, NULL);

    return g_object_new (NAUTILUS_TYPE_PROPERTIES_MODEL,
                         "title", title,
                         "model", model,
                         NULL);
}

static void
nautilus_properties_model_get_property (GObject    *object,
                                        guint       param_id,
                                        GValue     *value,
                                        GParamSpec *pspec)
{
    NautilusPropertiesModel *self = NAUTILUS_PROPERTIES_MODEL (object);

    switch (param_id)
    {
        case PROP_TITLE:
        {
            g_value_set_string (value, self->title);
        }
        break;

        case PROP_MODEL:
        {
            g_value_set_object (value, self->model);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        }
        break;
    }
}

static void
nautilus_properties_model_set_property (GObject      *object,
                                        guint         param_id,
                                        const GValue *value,
                                        GParamSpec   *pspec)
{
    NautilusPropertiesModel *self = NAUTILUS_PROPERTIES_MODEL (object);

    switch (param_id)
    {
        case PROP_TITLE:
        {
            g_free (self->title);
            self->title = g_value_dup_string (value);
        }
        break;

        case PROP_MODEL:
        {
            g_set_object (&self->model, g_value_get_object (value));
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        }
        break;
    }
}

static void
nautilus_properties_model_dispose (GObject *object)
{
    NautilusPropertiesModel *self = NAUTILUS_PROPERTIES_MODEL (object);

    g_clear_object (&self->model);

    G_OBJECT_CLASS (nautilus_properties_model_parent_class)->dispose (object);
}

static void
nautilus_properties_model_finalize (GObject *object)
{
    NautilusPropertiesModel *self = NAUTILUS_PROPERTIES_MODEL (object);

    g_free (self->title);

    G_OBJECT_CLASS (nautilus_properties_model_parent_class)->finalize (object);
}

static void
nautilus_properties_model_init (NautilusPropertiesModel *self)
{
}

static void
nautilus_properties_model_class_init (NautilusPropertiesModelClass *class)
{
    GParamSpec *pspec;

    G_OBJECT_CLASS (class)->finalize = nautilus_properties_model_finalize;
    G_OBJECT_CLASS (class)->dispose = nautilus_properties_model_dispose;
    G_OBJECT_CLASS (class)->get_property = nautilus_properties_model_get_property;
    G_OBJECT_CLASS (class)->set_property = nautilus_properties_model_set_property;

    pspec = g_param_spec_string ("title", "", "",
                                 NULL,
                                 G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (G_OBJECT_CLASS (class), PROP_TITLE, pspec);

    pspec = g_param_spec_object ("model", "", "",
                                 G_TYPE_LIST_MODEL,
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
    g_object_class_install_property (G_OBJECT_CLASS (class), PROP_MODEL, pspec);
}

const char *
nautilus_properties_model_get_title (NautilusPropertiesModel *self)
{
    return self->title;
}

void
nautilus_properties_model_set_title (NautilusPropertiesModel *self,
                                     const char              *title)
{
    g_object_set (self, "title", title, NULL);
}

GListModel *
nautilus_properties_model_get_model (NautilusPropertiesModel *self)
{
    return self->model;
}
