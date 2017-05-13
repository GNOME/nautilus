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

#ifndef NAUTILUS_FILE_TASK_H
#define NAUTILUS_FILE_TASK_H

#include "nautilus-task.h"

#define NAUTILUS_TYPE_FILE_TASK (nautilus_file_task_get_type ())

G_DECLARE_DERIVABLE_TYPE (NautilusFileTask, nautilus_file_task,
                          NAUTILUS, FILE_TASK,
                          NautilusTask)

struct _NautilusFileTaskClass
{
    NautilusTaskClass parent_class;

    void (*execute) (NautilusTask *task);
};

#endif
