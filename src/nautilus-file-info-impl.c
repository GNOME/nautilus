/*
 * Copyright (C) 2003 Novell, Inc.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "nautilus-file-info-impl.h"

#include "nautilus-file.h"

#include <libnautilus-extension/nautilus-file-info-interface.h>
#include <nautilus-extension.h>

static void
file_info_impl_init_interface (NautilusFileInfoInterface *iface);

struct _NautilusFileInfoImpl
{
    GObject parent;

    NautilusFile *file;
};

enum
{
    CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

#define NAUTILUS_TYPE_FILE_INFO_IMPL (nautilus_file_info_impl_get_type())
G_DECLARE_FINAL_TYPE (NautilusFileInfoImpl, nautilus_file_info_impl,
                      NAUTILUS, FILE_INFO_IMPL, GObject)
G_DEFINE_FINAL_TYPE_WITH_CODE (NautilusFileInfoImpl, nautilus_file_info_impl, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (NAUTILUS_TYPE_FILE_INFO,
                                                      file_info_impl_init_interface));

static gboolean
is_gone (NautilusFileInfo *file_info)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    return nautilus_file_is_gone (self->file);
}

static char *
get_name (NautilusFileInfo *file_info)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    return g_strdup (nautilus_file_get_name (self->file));
}

static char *
get_uri (NautilusFileInfo *file_info)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    return nautilus_file_get_uri (self->file);
}

static char *
get_parent_uri (NautilusFileInfo *file_info)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    return nautilus_file_get_parent_uri (self->file);
}

static char *
get_uri_scheme (NautilusFileInfo *file_info)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    return nautilus_file_get_uri_scheme (self->file);
}

static char *
get_mime_type (NautilusFileInfo *file_info)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    return g_strdup (nautilus_file_get_mime_type (self->file));
}

static gboolean
is_mime_type (NautilusFileInfo *file_info,
              const char       *mime_type)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    return nautilus_file_is_mime_type (self->file, mime_type);
}

static gboolean
is_directory (NautilusFileInfo *file_info)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    return nautilus_file_is_directory (self->file);
}

static void
add_emblem (NautilusFileInfo *file_info,
            const char       *emblem_name)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    nautilus_file_add_extension_emblem (self->file, emblem_name);
}

static char *
get_string_attribute (NautilusFileInfo *file_info,
                      const char       *attribute_name)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    return nautilus_file_get_string_attribute (self->file, attribute_name);
}

static void
add_string_attribute (NautilusFileInfo *file_info,
                      const char       *attribute_name,
                      const char       *value)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    nautilus_file_add_extension_attribute (self->file, attribute_name, value);
}

static void
invalidate_extension_info (NautilusFileInfo *file_info)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    nautilus_file_invalidate_attributes (self->file, NAUTILUS_FILE_ATTRIBUTE_EXTENSION_INFO);
}

static char *
get_activation_uri (NautilusFileInfo *file_info)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    return nautilus_file_get_activation_uri (self->file);
}

static GFileType
get_file_type (NautilusFileInfo *file_info)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    return nautilus_file_get_file_type (self->file);
}

static GFile *
get_location (NautilusFileInfo *file_info)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    return nautilus_file_get_location (self->file);
}

static GFile *
get_parent_location (NautilusFileInfo *file_info)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    return nautilus_file_get_parent_location (self->file);
}

static NautilusFileInfo *
get_parent_info (NautilusFileInfo *file_info)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;
    NautilusFile *parent = nautilus_file_get_parent (self->file);

    if (parent == NULL)
    {
        return NULL;
    }
    else
    {
        return nautilus_file_info_from_file (parent);
    }
}

static GMount *
get_mount (NautilusFileInfo *file_info)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    return nautilus_file_get_mount (self->file);
}

static gboolean
can_write (NautilusFileInfo *file_info)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) file_info;

    return nautilus_file_can_write (self->file);
}

static GHashTable *file_to_file_info;

static void
file_info_impl_init_interface (NautilusFileInfoInterface *iface)
{
    iface->is_gone = is_gone;

    iface->get_name = get_name;
    iface->get_uri = get_uri;
    iface->get_parent_uri = get_parent_uri;
    iface->get_uri_scheme = get_uri_scheme;

    iface->get_mime_type = get_mime_type;
    iface->is_mime_type = is_mime_type;
    iface->is_directory = is_directory;

    iface->add_emblem = add_emblem;
    iface->get_string_attribute = get_string_attribute;
    iface->add_string_attribute = add_string_attribute;
    iface->invalidate_extension_info = invalidate_extension_info;

    iface->get_activation_uri = get_activation_uri;

    iface->get_file_type = get_file_type;
    iface->get_location = get_location;
    iface->get_parent_location = get_parent_location;
    iface->get_parent_info = get_parent_info;
    iface->get_mount = get_mount;
    iface->can_write = can_write;
}

static void
file_info_impl_changed (NautilusFileInfoImpl *self)
{
    g_signal_emit (self, signals[CHANGED], 0);
}

static void
nautilus_file_info_impl_init (NautilusFileInfoImpl *self)
{
    g_message ("  init %p", self);
}

static void
file_info_impl_finalize (GObject *object)
{
    NautilusFileInfoImpl *self = (NautilusFileInfoImpl *) object;

    g_message ("  finalize %p", self);

    g_signal_handlers_disconnect_by_func (self->file,
                                          (GCallback) file_info_impl_changed,
                                          self);
    g_hash_table_remove (file_to_file_info, self->file);

    g_clear_object (&self->file);
}

NautilusFileInfo *
nautilus_file_info_from_file (NautilusFile *file)
{
    g_return_val_if_fail (file != NULL, NULL);

    if (G_UNLIKELY (file_to_file_info == NULL))
    {
        file_to_file_info = g_hash_table_new (NULL, NULL);
    }

    NautilusFileInfoImpl *info = g_hash_table_lookup (file_to_file_info, file);

    if (info != NULL)
    {
        return (NautilusFileInfo *) g_object_ref (info);
    }
    else
    {
        NautilusFileInfoImpl *new_info = g_object_new (NAUTILUS_TYPE_FILE_INFO_IMPL, NULL);

        g_hash_table_insert (file_to_file_info, file, new_info);
        new_info->file = g_object_ref (file);
        g_signal_connect_swapped (new_info->file,
                                  "changed",
                                  G_CALLBACK (file_info_impl_changed),
                                  new_info);

        return (NautilusFileInfo *) new_info;
    }
}

static NautilusFileInfo *
get_info (GFile    *location,
          gboolean  create)
{
    g_autoptr (NautilusFile) file = (create)
                                    ? nautilus_file_get (location)
                                    : nautilus_file_get_existing (location);

    if (file == NULL)
    {
        return NULL;
    }
    else
    {
        return nautilus_file_info_from_file (file);
    }
}

static void
nautilus_file_info_impl_class_init (NautilusFileInfoImplClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = file_info_impl_finalize;

    signals[CHANGED] = g_signal_new ("changed",
                                     NAUTILUS_TYPE_FILE_INFO_IMPL,
                                     G_SIGNAL_RUN_LAST,
                                     0,
                                     NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID,
                                     G_TYPE_NONE, 0);

    nautilus_file_info_getter = get_info;
}
