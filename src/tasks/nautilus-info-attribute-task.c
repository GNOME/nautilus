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

#include "nautilus-info-attribute-task.h"

#include "nautilus-directory-private.h"
#include "nautilus-file-private.h"

struct _NautilusInfoAttributeTask
{
    NautilusAttributeTask parent_instance;
};

G_DEFINE_TYPE (NautilusInfoAttributeTask, nautilus_info_attribute_task,
               NAUTILUS_TYPE_ATTRIBUTE_TASK)

static gboolean
is_cache_invalid (NautilusAttributeTask *attribute_task,
                  NautilusFile          *file)
{
    return !file->details->file_info_is_up_to_date
           && !file->details->is_gone;
}

static void
update_cache (NautilusAttributeTask *task,
              NautilusFile          *file,
              GFileInfo             *info,
              GError                *error)
{
    if (info == NULL)
    {
        if (error->domain == G_IO_ERROR && error->code == G_IO_ERROR_NOT_FOUND)
        {
            /* mark file as gone */
            nautilus_file_mark_gone (file);
        }
        file->details->file_info_is_up_to_date = TRUE;
        nautilus_file_clear_info (file);
        file->details->get_info_failed = TRUE;
        file->details->get_info_error = error;
    }
    else
    {
        nautilus_file_update_info (file, info);
        g_object_unref (info);
    }
}

static void
nautilus_info_attribute_task_class_init (NautilusInfoAttributeTaskClass *klass)
{
    NautilusAttributeTaskClass *attribute_class;

    attribute_class = NAUTILUS_ATTRIBUTE_TASK_CLASS (klass);

    attribute_class->is_cache_invalid = is_cache_invalid;
    attribute_class->update_cache = update_cache;
}

static void
nautilus_info_attribute_task_init (NautilusInfoAttributeTask *self)
{
}

NautilusTask *
nautilus_info_attribute_task_new_for_file (NautilusFile *file)
{
    g_autoptr (GList) list = NULL;

    g_return_val_if_fail (NAUTILUS_IS_FILE (file), NULL);

    list = g_list_append (list, file);

    return g_object_new (NAUTILUS_TYPE_INFO_ATTRIBUTE_TASK,
                         "files", list,
                         "attributes", NAUTILUS_FILE_DEFAULT_ATTRIBUTES,
                         NULL);
}

NautilusTask *
nautilus_info_attribute_task_new_for_directory (NautilusDirectory *directory)
{
    g_autoptr (GList) list = NULL;

    g_return_val_if_fail (NAUTILUS_IS_DIRECTORY (directory), NULL);

    list = g_list_copy (directory->details->file_list);

    return g_object_new (NAUTILUS_TYPE_INFO_ATTRIBUTE_TASK,
                         "files", list,
                         "attributes", NAUTILUS_FILE_DEFAULT_ATTRIBUTES,
                         NULL);
}
