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

#include "nautilus-image-properties-page-provider.h"
#include "nautilus-image-properties-page.h"
#include <glib/gi18n.h>
#include <nautilus-extension.h>

#define NAUTILUS_IMAGE_PROPERTIES_PAGE_NAME "NautilusImagePropertiesPage::property_page"

struct _NautilusImagePropertiesPageProvider
{
    GObject parent_instance;

    NautilusPropertyPageModel *page_model;
    GList *files;
};

enum
{
    PROP_0,
    PROP_PAGE_MODEL,
    PROP_FILES,
    N_PROPERTIES
};

static void property_page_provider_iface_init (NautilusPropertyPageModelProviderInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (NautilusImagePropertiesPageProvider,
                                nautilus_image_properties_page_provider,
                                G_TYPE_OBJECT,
                                0,
                                G_IMPLEMENT_INTERFACE_DYNAMIC (NAUTILUS_TYPE_PROPERTY_PAGE_MODEL_PROVIDER,
                                                               property_page_provider_iface_init))

static gboolean
is_mime_type_supported (const char *mime_type)
{
    g_autoptr (GSList) formats = NULL;

    if (mime_type == NULL)
    {
        return FALSE;
    }

    formats = gdk_pixbuf_get_formats ();

    for (GSList *l = formats; l != NULL; l = l->next)
    {
        g_auto (GStrv) mime_types = NULL;

        mime_types = gdk_pixbuf_format_get_mime_types (l->data);
        if (mime_types == NULL)
        {
            continue;
        }

        if (g_strv_contains ((const char *const *) mime_types, mime_type))
        {
            return TRUE;
        }
    }

    return FALSE;
}

static NautilusPropertyPageModel*
get_model (NautilusPropertyPageModelProvider *provider)
{
    NautilusImagePropertiesPageProvider *self;

    self = NAUTILUS_IMAGE_PROPERTIES_PAGE_PROVIDER (provider);

    return self->page_model;
}

static void
set_files (NautilusPropertyPageModelProvider *provider,
           GList                             *files)
{
    NautilusImagePropertiesPageProvider *self;
    NautilusFileInfo *file_info;
    g_autofree char *mime_type = NULL;

    self = NAUTILUS_IMAGE_PROPERTIES_PAGE_PROVIDER (provider);

    if (self->files != NULL)
    {
        g_list_free_full (self->files, g_object_unref);
        self->files = NULL;
    }

    self->files = g_list_copy_deep (files,
                                    (GCopyFunc) g_object_ref, NULL);

    if (self->files == NULL || self->files->next != NULL)
    {
        return;
    }

    file_info = NAUTILUS_FILE_INFO (self->files->data);
    mime_type = nautilus_file_info_get_mime_type (file_info);
    if (!is_mime_type_supported (mime_type))
    {
        return;
    }

    nautilus_image_properties_page_load_from_file_info (NAUTILUS_IMAGE_PROPERTIES_PAGE_MODEL (self->page_model),
                                                        file_info);
}

static gboolean
supports_files (NautilusPropertyPageModelProvider *provider)
{
    g_autofree char *mime_type = NULL;
    NautilusFileInfo *file_info;
    NautilusImagePropertiesPageProvider *self;

    self = NAUTILUS_IMAGE_PROPERTIES_PAGE_PROVIDER (provider);
    if (self->files == NULL || self->files->next != NULL)
    {
        return FALSE;
    }

    file_info = NAUTILUS_FILE_INFO (self->files->data);
    mime_type = nautilus_file_info_get_mime_type (file_info);
    if (!is_mime_type_supported (mime_type))
    {
        return FALSE;
    }

    return TRUE;
}

static void
nautilus_image_properties_page_provider_get_property (GObject    *object,
                                                      guint       prop_id,
                                                      GValue     *value,
                                                      GParamSpec *pspec)
{
    NautilusImagePropertiesPageProvider *self = NAUTILUS_IMAGE_PROPERTIES_PAGE_PROVIDER (object);
    switch (prop_id)
    {
        case PROP_PAGE_MODEL:
        {
            g_value_set_object (value, self->page_model);
        }
        break;

        case PROP_FILES:
        {
            g_value_set_pointer (value, self->files);
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
    }
}

static void
nautilus_image_properties_page_provider_set_property (GObject    *object,
                                                      guint       prop_id,
                                                      const GValue     *value,
                                                      GParamSpec *pspec)
{
    NautilusImagePropertiesPageProvider *self = NAUTILUS_IMAGE_PROPERTIES_PAGE_PROVIDER (object);
    switch (prop_id)
    {
        case PROP_PAGE_MODEL:
        {
            g_clear_object (&self->page_model);
            self->page_model = NAUTILUS_PROPERTY_PAGE_MODEL (g_value_get_object (value));
        }
        break;

        case PROP_FILES:
        {
            set_files (NAUTILUS_PROPERTY_PAGE_MODEL_PROVIDER (self),
                       g_value_get_pointer (value));
        }
        break;

        default:
        {
            g_assert_not_reached ();
        }
    }
}

static void
property_page_provider_iface_init (NautilusPropertyPageModelProviderInterface *iface)
{
    iface->get_model = get_model;
    iface->set_files = set_files;
    iface->supports_files = supports_files;

}

static void
nautilus_image_properties_page_provider_init (NautilusImagePropertiesPageProvider *self)
{
    self->page_model = NAUTILUS_PROPERTY_PAGE_MODEL (nautilus_image_properties_page_model_new ());
}

static void
nautilus_image_properties_page_provider_class_init (NautilusImagePropertiesPageProviderClass *klass)
{
    GObjectClass *oclass;

    oclass = G_OBJECT_CLASS (klass);
    oclass->get_property = nautilus_image_properties_page_provider_get_property;
    oclass->set_property = nautilus_image_properties_page_provider_set_property;

    g_object_class_override_property (oclass, PROP_PAGE_MODEL, "page-model");
    g_object_class_override_property (oclass, PROP_FILES, "files");
}

static void
nautilus_image_properties_page_provider_class_finalize (NautilusImagePropertiesPageProviderClass *klass)
{
    (void) klass;
}

void
nautilus_image_properties_page_provider_load (GTypeModule *module)
{
    nautilus_image_properties_page_provider_register_type (module);
}
