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

typedef struct
{
    gpointer value;
    NautilusAttributeState state;

    NautilusCopyFunc copy_func;
    GDestroyNotify destroy_func;

    GMutex mutex;
} NautilusAttributePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NautilusAttribute, nautilus_attribute, G_TYPE_OBJECT)

static void
finalize (GObject *object)
{
    NautilusAttribute *self;
    NautilusAttributePrivate *priv;

    self = NAUTILUS_ATTRIBUTE (object);
    priv = nautilus_attribute_get_instance_private (self);

    if (priv->destroy_func != NULL && priv->value != NULL)
    {
        g_clear_pointer (&priv->value, priv->destroy_func);
    }

    g_mutex_clear (&priv->mutex);

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
    NautilusAttributePrivate *priv;

    priv = nautilus_attribute_get_instance_private (self);

    g_mutex_init (&priv->mutex);
}

void
nautilus_attribute_get_value (NautilusAttribute                    *attribute,
                              gboolean                              update,
                              NautilusAttributeUpdateValueCallback  callback,
                              gpointer                              user_data)
{
    NautilusAttributePrivate *priv;
    gpointer value;

    g_return_if_fail (NAUTILUS_IS_ATTRIBUTE (attribute));

    if (callback == NULL)
    {
        return;
    }

    priv = nautilus_attribute_get_instance_private (attribute);

    if (!update)
    {
        if (priv->copy_func != NULL && priv->value != NULL)
        {
            value = priv->copy_func (priv->value);
        }
        else
        {
            value = priv->value;
        }
    }
}

void
nautilus_attribute_set_value (NautilusAttribute *attribute,
                              gpointer           value)
{
    NautilusAttributePrivate *priv;

    g_return_if_fail (NAUTILUS_IS_ATTRIBUTE (attribute));

    priv = nautilus_attribute_get_instance_private (attribute);

    g_mutex_lock (&priv->mutex);

    if (priv->destroy_func != NULL && priv->value != NULL)
    {
        priv->destroy_func (priv->value);
    }

    if (priv->copy_func != NULL)
    {
        priv->value = priv->copy_func (value);
    }
    else
    {
        priv->value = value;
    }

    /* If an update is pending,
     * the new value divined shall be discarded after the state check.
     */
    priv->state = NAUTILUS_ATTRIBUTE_STATE_VALID;

    g_mutex_unlock (&priv->mutex);
}

NautilusAttribute *
nautilus_attribute_new (NautilusCopyFunc copy_func,
                        GDestroyNotify   destroy_func)
{
    NautilusAttribute *instance;
    NautilusAttributePrivate *priv;

    instance = g_object_new (NAUTILUS_TYPE_ATTRIBUTE, NULL);
    priv = nautilus_attribute_get_instance_private (instance);

    priv->copy_func = copy_func;
    priv->destroy_func = destroy_func;

    return instance;
}
