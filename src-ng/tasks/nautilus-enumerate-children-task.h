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

#ifndef NAUTILUS_ENUMERATE_CHILDREN_TASK_H_INCLUDED
#define NAUTILUS_ENUMERATE_CHILDREN_TASK_H_INCLUDED

#include "nautilus-task.h"

#include <gio/gio.h>

#define NAUTILUS_TYPE_ENUMERATE_CHILDREN_TASK (nautilus_enumerate_children_task_get_type ())

G_DECLARE_FINAL_TYPE (NautilusEnumerateChildrenTask,
                      nautilus_enumerate_children_task,
                      NAUTILUS, ENUMERATE_CHILDREN_TASK,
                      NautilusTask)

NautilusTask *nautilus_enumerate_children_task_new (GFile               *file,
                                                    const char          *attributes,
                                                    GFileQueryInfoFlags  flags,
                                                    GCancellable        *cancellable);

#endif
