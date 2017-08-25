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
 * Returns: the current state of the attribute
 */
NautilusAttributeState nautilus_attribute_get_state  (NautilusAttribute *self);
/**
 * nautilus_attribute_invalidate:
 * @self: a #NautilusAttribute instance
 *
 * Mark the value of @attribute as no longer valid.
 */
void                   nautilus_attribute_invalidate (NautilusAttribute *self);

/**
 * nautilus_attribute_get_value:
 * @self: a #NautilusAttribute instance
 *
 * Returns: (transfer full): the value of the attribute
 */
gpointer nautilus_attribute_get_value           (NautilusAttribute *self)
/**
 * nautilus_attribute_set_value:
 * @self: a #NautilusAttribute instance
 * @value: (nullable) (transfer full): the new value of the attribute
 */
void     nautilus_attribute_set_value           (NautilusAttribute *self,
                                                 gpointer           value);
/**
 * nautilus_attribute_set_value_from_task:
 * @self: a #NautilusAttribute instance
 * @task: an idle #NautilusTask
 */
void     nautilus_attribute_set_value_from_task (NautilusAttribute *self,
                                                 NautilusTask      *task

/**
 * nautilus_attribute_new:
 * @destroy_func: (nullable): the function to call when destroying the value
 *
 * Returns: (transfer full): a #NautilusAttribute instance
 */
NautilusAttribute *nautilus_attribute_new (GDestroyNotify destroy_func);

#endif
