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

#include "nautilus-attribute-task.h"

struct _NautilusAttributeTask
{
    GObject parent_instance;

    GCancellable *cancellable;

    GFile *file;
    const char *attributes;

    GFileQueryInfoFlags query_flags;

    GFileAttributeMatcher *filesystem_attribute_matcher;
} NautilusAttributeTaskPrivate;

G_DEFINE_TYPE (NautilusAttributeTask, nautilus_attribute_task,
               NAUTILUS_TYPE_TASK)

enum
{
    PROP_FILE = 1,
    PROP_ATTRIBUTES,
    PROP_QUERY_FLAGS,
    N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL };

static void
set_property (GObject      *object,
              guint         property_id,
              const GValue *value,
              GParamSpec   *pspec)
{
    NautilusAttributeTask *self;

    self = NAUTILUS_ATTRIBUTE_TASK (object);

    switch (property_id)
    {
        case PROP_FILE:
        {
            if (G_UNLIKELY (self->file != NULL))
            {
                g_clear_object (&self->file);
            }

            self->file = g_value_dup_object (value);
        }
        break;

        case PROP_ATTRIBUTES:
        {
            g_free ((gpointer) self->attributes);

            self->attributes = g_value_dup_string (value);
        }
        break;

        case PROP_QUERY_FLAGS:
        {
            self->query_flags = g_value_get_int (value);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        }
    }
}

static void
finalize (GObject *object)
{
    NautilusAttributeTask *self;

    self = NAUTILUS_ATTRIBUTE_TASK (object);

    g_clear_pointer (&self->file, nautilus_file_unref);
    g_clear_pointer (&self->attributes, g_free);

    G_OBJECT_CLASS (nautilus_attribute_task_parent_class)->finalize (object);
}

static void
execute (NautilusTask *task)
{
    NautilusAttributeTask *self;
    g_autoptr (GCancellable) cancellable = NULL;
    GError *error = NULL;
    GFileInfo *info;

    self = NAUTILUS_ATTRIBUTE_TASK (task);

    cancellable = nautilus_task_get_cancellable (NAUTILUS_TASK (self));
    info = g_file_query_info (self->file, self->attributes,
                              self->query_flags,
                              cancellable,
                              &error);
}

static void
nautilus_attribute_task_class_init (NautilusAttributeTaskClass *klass)
{
    GObjectClass *object_class;
    NautilusTaskClass *task_class;

    object_class = G_OBJECT_CLASS (klass);
    task_class = NAUTILUS_TASK_CLASS (klass);

    object_class->set_property = set_property;
    object_class->finalize = finalize;

    task_class->execute = execute;

    properties[PROP_FILE] =
        g_param_spec_object ("file", "File", "File",
                             G_TYPE_FILE,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME);

    properties[PROP_ATTRIBUTES] =
        g_param_spec_string ("attributes", "Attributes", "Attributes",
                             NULL,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME);

    properties[PROP_QUERY_FLAGS] =
        g_param_spec_int ("query-flags", "Query flags", "Query flags",
                          G_FILE_QUERY_INFO_NONE, G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                          G_FILE_QUERY_INFO_NONE,
                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_STATIC_NAME);

    g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

static void
nautilus_attribute_task_init (NautilusAttributeTask *self)
{
    self->file = NULL;
    self->attributes = NULL;
}

NautilusTask *
nautilus_attribute_task_new (NautilusFile *file,
                             const char   *attributes,
                             gboolean      follow_symlinks,
                             GCancellable *cancellable)
{
    GFileQueryInfoFlags query_flags;

    query_flags = follow_symlinks? G_FILE_QUERY_INFO_NONE :
                                   G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS;

    return g_object_new (NAUTILUS_TYPE_ATTRIBUTE_TASK,
                         "file", file,
                         "attributes", attributes,
                         "query-flags", query_flags,
                         "cancellable", cancellable,
                         NULL);
}
