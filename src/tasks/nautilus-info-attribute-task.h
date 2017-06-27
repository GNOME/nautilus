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

#ifndef NAUTILUS_INFO_ATTRIBUTE_TASK_H
#define NAUTILUS_INFO_ATTRIBUTE_TASK_H

#include "nautilus-attribute-task.h"

#include "nautilus-directory.h"
#include "nautilus-file.h"

#define NAUTILUS_TYPE_INFO_ATTRIBUTE_TASK (nautilus_info_attribute_task_get_type ())

G_DECLARE_FINAL_TYPE (NautilusInfoAttributeTask, nautilus_info_attribute_task,
                      NAUTILUS, INFO_ATTRIBUTE_TASK,
                      NautilusAttributeTask)

NautilusTask *nautilus_info_attribute_task_new_for_file (NautilusFile *file);
NautilusTask *nautilus_info_attribute_task_new_for_directory (NautilusDirectory *directory);

#endif
