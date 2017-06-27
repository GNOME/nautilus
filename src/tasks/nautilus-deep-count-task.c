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

#include "nautilus-deep-count-task.h"

#include "nautilus-directory.h"
#include "nautilus-file-private.h"

struct _NautilusDeepCountTask
{
    GObject parent_instance;

    NautilusFile *file;
    GCancellable *cancellable;
    GFileEnumerator *enumerator;
    GFile *deep_count_location;
    GList *deep_count_subdirectories;
    GArray *seen_deep_count_inodes;
    char *fs_id;

    gsize deep_directory_count;
    gsize deep_file_count;
    gsize deep_unreadable_count;
    goffset deep_size;
};

G_DEFINE_TYPE (NautilusDeepCountTask, nautilus_deep_count_task,
               NAUTILUS_TYPE_TASK)

#define DIRECTORY_LOAD_ITEMS_PER_CALLBACK 100

enum
{
    PROP_FILE = 1,
    N_PROPERTIES
};

GParamSpec *properties[N_PROPERTIES] = { NULL };

static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    switch (property_id)
    {
        case PROP_FILE:
        {
            NautilusDeepCountTask *self;

            self = NAUTILUS_DEEP_COUNT_TASK (object);

            self->file = g_value_get_pointer (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static GCancellable *
get_cancellable (NautilusTask *task)
{
    return NULL;
}

static void
deep_count_next_dir (NautilusDeepCountTask *self)
{
    NautilusDirectory *directory;

    directory = self->file->details->directory;
}

static void
deep_count_load (NautilusDeepCountTask *self,
                 GFile                 *location)
{
    g_autoptr (GFileEnumerator) enumerator = NULL;

    self->deep_count_location = g_object_ref (location);

    enumerator =
        g_file_enumerate_children (location,
                                   G_FILE_ATTRIBUTE_STANDARD_NAME ","
                                   G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                   G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                   G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN ","
                                   G_FILE_ATTRIBUTE_STANDARD_IS_BACKUP ","
                                   G_FILE_ATTRIBUTE_ID_FILESYSTEM ","
                                   G_FILE_ATTRIBUTE_UNIX_INODE,
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                   self->cancellable,
                                   NULL);

    if (enumerator == NULL)
    {
        self->deep_unreadable_count++;

        deep_count_next_dir (self);
    }
    else
    {
        /*g_file_enumerator_next_files_async (enumerator,
                                            DIRECTORY_LOAD_ITEMS_PER_CALLBACK,
                                            G_PRIORITY_LOW,
                                            self->cancellable,
                                            deep_count_more_files_callback,
                                            self);*/
    }

    g_object_unref (location);
}

static void
execute (NautilusTask *task)
{
    NautilusDeepCountTask *self;
    g_autoptr (GFile) location = NULL;
    g_autoptr (GFileInfo) info = NULL;

    self = NAUTILUS_DEEP_COUNT_TASK (task);
    location = nautilus_file_get_location (self->file);

    info = g_file_query_info (location,
                              G_FILE_ATTRIBUTE_ID_FILESYSTEM,
                              G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                              self->cancellable,
                              NULL);

    if (info != NULL)
    {
        const char *fs_id;

        fs_id = g_file_info_get_attribute_string (info,
                                                  G_FILE_ATTRIBUTE_ID_FILESYSTEM);
        self->fs_id = g_strdup (fs_id);
    }

    deep_count_load (self, g_object_ref (location));
}

static void
nautilus_deep_count_task_class_init (NautilusDeepCountTaskClass *klass)
{
    GObjectClass *object_class;
    NautilusTaskClass *task_class;

    object_class = G_OBJECT_CLASS (klass);
    task_class = NAUTILUS_TASK_CLASS (klass);

    object_class->set_property = set_property;

    task_class->get_cancellable = get_cancellable;
    task_class->execute = execute;

    properties[PROP_FILE] =
        g_param_spec_pointer ("file", "File", "File",
                              G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
nautilus_deep_count_task_init (NautilusDeepCountTask *self)
{
    self->cancellable = g_cancellable_new ();
    self->enumerator = NULL;
    self->deep_count_location = NULL;
    self->deep_count_subdirectories = NULL;
    self->seen_deep_count_inodes = g_array_new (FALSE, TRUE, sizeof (guint64));;
    self->fs_id = NULL;

    self->deep_directory_count = 0;
    self->deep_file_count = 0;
    self->deep_unreadable_count = 0;
    self->deep_size = 0;
}

NautilusTask *
nautilus_deep_count_task_new (NautilusFile *file)
{
    return g_object_new (NAUTILUS_TYPE_DEEP_COUNT_TASK,
                         "file", file,
                         NULL);
}
