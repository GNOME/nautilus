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

struct _NautilusImagesPropertiesPageProvider
{
    GObject parent_instance;
};

static void property_page_provider_iface_init (NautilusPropertyPageModelProviderInterface *iface);

G_DEFINE_DYNAMIC_TYPE_EXTENDED (NautilusImagesPropertiesPageProvider,
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
get_model (NautilusPropertyPageModelProvider *provider,
           GList                             *files)
{
    NautilusFileInfo *file_info;
    g_autofree char *mime_type = NULL;
    NautilusImagesPropertiesPage *image_properties_page;
    NautilusPropertyPageModel *property_page;
    GList *sections = NULL;
    GList *items = NULL;
    NautilusPropertyPageModelSection *section1;
    NautilusPropertyPageModelItem *item1;
    NautilusPropertyPageModelItem *item2;

    if (files == NULL || files->next != NULL)
    {
        return NULL;
    }

    file_info = NAUTILUS_FILE_INFO (files->data);
    mime_type = nautilus_file_info_get_mime_type (file_info);
    if (!is_mime_type_supported (mime_type))
    {
        return NULL;
    }

    section1 = g_new (NautilusPropertyPageModelSection, 1);
    section1->title = g_strdup ("Test section");
    section1->id = 1;

    item1 = g_new (NautilusPropertyPageModelItem, 1);
    item1->section_id = 1;
    item1->field = g_strdup ("Test item field");
    item1->value = g_strdup ("Test item value");

    item2 = g_new (NautilusPropertyPageModelItem, 1);
    item2->section_id = 1;
    item2->field = g_strdup ("Test item field 2");
    item2->value = g_strdup ("Test item value 2");

    sections = g_list_append (NULL, section1);
    items = g_list_append (NULL, item1);
    items = g_list_append (items, item2);

    g_print ("HEEEREEEE\n");
    property_page = nautilus_property_page_model_new (NAUTILUS_IMAGE_PROPERTIES_PAGE_NAME,
                                                      sections,
                                                      items);

    /*
    image_properties_page = nautilus_image_properties_page_new ();
    nautilus_image_properties_page_load_from_file_info (image_properties_page, file_info);
     */

    return property_page;
}

static void
property_page_provider_iface_init (NautilusPropertyPageModelProviderInterface *iface)
{
    iface->get_model = get_model;
}

static void
nautilus_image_properties_page_provider_init (NautilusImagesPropertiesPageProvider *self)
{
    (void) self;
}

static void
nautilus_image_properties_page_provider_class_init (NautilusImagesPropertiesPageProviderClass *klass)
{
    (void) klass;
}

static void
nautilus_image_properties_page_provider_class_finalize (NautilusImagesPropertiesPageProviderClass *klass)
{
    (void) klass;
}

void
nautilus_image_properties_page_provider_load (GTypeModule *module)
{
    nautilus_image_properties_page_provider_register_type (module);
}
