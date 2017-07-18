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

#ifndef NAUTILUS_RENAME_TASK_H_INCLUDED
#define NAUTILUS_RENAME_TASK_H_INCLUDED

#include "nautilus-task.h"

#define NAUTILUS_TYPE_RENAME_TASK (nautilus_rename_task_get_type ())

G_DECLARE_FINAL_TYPE (NautilusRenameTask, nautilus_rename_task,
                      NAUTILUS, RENAME_TASK, NautilusTask)

void nautilus_rename_task_add_target (NautilusRenameTask *task,
                                      GFile              *file,
                                      const gchar        *name);

NautilusTask *nautilus_rename_task_new (void);

#endif
