/* Copyright (C) 2004 Red Hat, Inc
 * Copyright (c) 2007 Novell, Inc.
 * Copyright (c) 2017 Thomas Bechtold <thomasbechtold@jpberlin.de>
 * Copyright (c) 2018 Ernestas Kulik <ernestask@gnome.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 * XMP support by Hubert Figuiere <hfiguiere@novell.com>
 */

#include "nautilus-image-properties-model-provider.h"

#include "nautilus-image-properties-model.h"

#include <nautilus-extension.h>

struct _NautilusImagesPropertiesModelProvider
{
    GObject parent_instance;
};

static void properties_group_provider_iface_init (NautilusPropertiesModelProviderInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (NautilusImagesPropertiesModelProvider,
                                nautilus_image_properties_model_provider,
                                G_TYPE_OBJECT,
                                0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (NAUTILUS_TYPE_PROPERTIES_MODEL_PROVIDER,
                                                               properties_group_provider_iface_init))

static GList *
get_models (NautilusPropertiesModelProvider *provider,
            GList                           *files)
{
    if (files == NULL || files->next != NULL)
    {
        return NULL;
    }

    NautilusFileInfo *file_info = NAUTILUS_FILE_INFO (files->data);
    g_autofree char *mime_type = nautilus_file_info_get_mime_type (file_info);
    g_autofree char *generic_icon = g_content_type_get_generic_icon_name (mime_type);

    if (g_str_equal (generic_icon, "image-x-generic"))
    {
        return g_list_prepend (NULL, nautilus_image_properties_model_new (file_info));
    }

    return NULL;
}

static void
properties_group_provider_iface_init (NautilusPropertiesModelProviderInterface *iface)
{
    iface->get_models = get_models;
}

static void
nautilus_image_properties_model_provider_init (NautilusImagesPropertiesModelProvider *self)
{
    (void) self;
}

static void
nautilus_image_properties_model_provider_class_init (NautilusImagesPropertiesModelProviderClass *klass)
{
    (void) klass;
}

static void
nautilus_image_properties_model_provider_class_finalize (NautilusImagesPropertiesModelProviderClass *klass)
{
    (void) klass;
}

void
nautilus_image_properties_model_provider_load (GTypeModule *module)
{
    nautilus_image_properties_model_provider_register_type (module);
}
