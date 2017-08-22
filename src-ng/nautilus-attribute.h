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

#ifndef NAUTILUS_ATTRIBUTE_H_INCLUDED
#define NAUTILUS_ATTRIBUTE_H_INCLUDED

#include "nautilus-task.h"

#include <glib-object.h>

#define NAUTILUS_TYPE_ATTRIBUTE (nautilus_attribute_get_type ())

G_DECLARE_FINAL_TYPE (NautilusAttribute, nautilus_attribute, NAUTILUS, ATTRIBUTE, GObject)

/* GCopyFunc has too many parameters for our taste. */
typedef gpointer (*NautilusCopyFunc) (gpointer data);
#define NAUTILUS_COPY_FUNC(x) ((NautilusCopyFunc) x)

typedef void (*NautilusAttributeUpdateValueCallback) (NautilusAttribute *attribute,
                                                      gpointer           value,
                                                      gpointer           user_data);

typedef enum
{
    NAUTILUS_ATTRIBUTE_STATE_INVALID,
    NAUTILUS_ATTRIBUTE_STATE_PENDING,
    NAUTILUS_ATTRIBUTE_STATE_VALID
} NautilusAttributeState;

/**
 * nautilus_attribute_get_state:
 * @attribute: an initialized #NautilusAttribute
 *
 * Returns: the current state of @attribute
 */
NautilusAttributeState nautilus_attribute_get_state  (NautilusAttribute *attribute);
/**
 * nautilus_attribute_invalidate:
 * @attribute: an initialized #NautilusAttribute
 *
 * Mark the value of @attribute as no longer valid.
 */
void                   nautilus_attribute_invalidate (NautilusAttribute *attribute);

/**
 * nautilus_attribute_get_value:
 * @attribute: an initialized #NautilusAttribute
 * @callback: (nullable): the function to call with the value of @attribute
 * @user_data: (nullable): additional data to pass to @callback
 */
void nautilus_attribute_get_value (NautilusAttribute                    *attribute,
                                   NautilusAttributeUpdateValueCallback  callback,
                                   gpointer                              user_data);
/**
 * nautilus_attribute_set_value:
 * @attribute: an initialized #NautilusAttribute
 * @value: (nullable) (transfer full): the new value of @attribute
 */
void nautilus_attribute_set_value (NautilusAttribute *attribute,
                                   gpointer           value);

/**
 * nautilus_attribute_new:
 * @update_func: the function to call to update invalid values
 * @copy_func: (nullable): the function to call when copying the value
 * @destroy_func: (nullable): the function to call when destroying the value
 *
 * Returns: a new #NautilusAttribute
 */
NautilusAttribute *nautilus_attribute_new (NautilusTaskFunc update_func,
                                           NautilusCopyFunc copy_func,
                                           GDestroyNotify   destroy_func);

#endif
