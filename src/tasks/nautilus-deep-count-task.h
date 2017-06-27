/* Copyright (C) 2017 Ernestas Kulik <ernestask@gnome.org>
 *
 * This file is part of Nautilus.
 *
 * Nautilus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#ifndef NAUTILUS_DEEP_COUNT_TASK_H
#define NAUTILUS_DEEP_COUNT_TASK_H

#include "nautilus-task.h"

#include "nautilus-file.h"

#define NAUTILUS_TYPE_DEEP_COUNT_TASK (nautilus_deep_count_task_get_type ())

G_DECLARE_FINAL_TYPE (NautilusDeepCountTask, nautilus_deep_count_task,
                      NAUTILUS, DEEP_COUNT_TASK,
                      NautilusTask)

NautilusTask *nautilus_deep_count_task_new (NautilusFile *file);

#endif
