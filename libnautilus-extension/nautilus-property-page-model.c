/*
 *  nautilus-property-page.h - Property pages exported by
 *                             NautilusPropertyProvider objects.
 *
 *  Copyright (C) 2003 Novell, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

#include <config.h>
#include "nautilus-property-page_model.h"

#include <glib-object.h>

enum
{
    PROP_0,
    PROP_TITLE,
    PROP_SECTIONS,
    PROP_ITEMS,
    LAST_PROP
};

struct _NautilusPropertyPageModelModel
{
    GObject parent_instance;

    char *title;
};

G_DEFINE_TYPE (NautilusPropertyPageModel, nautilus_property_page_model, G_TYPE_OBJECT)

/**
 * SECTION:nautilus-property-page
 * @title: NautilusPropertyPageModel
 * @short_description: Property page descriptor object
 *
 * #NautilusPropertyPageModel is an object that describes a page in the file
 * properties dialog. Extensions can provide #NautilusPropertyPageModel objects
 * by registering a #NautilusPropertyPageModelProvider and returning them from
 * nautilus_property_page_model_provider_get_pages(), which will be called by the
 * main application when creating file properties dialogs.
 */

/**
 * nautilus_property_page_model_new:
 * @name: the identifier for the property page
 * @label: the user-visible label of the property page
 * @page: the property page to display
 *
 * Creates a new #NautilusPropertyPageModel from page_widget.
 *
 * Returns: a newly created #NautilusPropertyPageModel
 */
NautilusPropertyPageModel *
nautilus_property_page_model_new (const char *title,
                                  GList      *sections,
                                  GList      *items)
{
    NautilusPropertyPageModel *page;

    g_return_val_if_fail (title != NULL, NULL);
    g_return_val_if_fail (sections != NULL, NULL);
    g_return_val_if_fail (items != NULL, NULL);

    page = g_object_new (NAUTILUS_TYPE_PROPERTY_PAGE_MODEL,
                         "title", title,
                         "sections", sections,
                         "items", items,
                         NULL);

    return page;
}

static void
nautilus_property_page_model_get_property (GObject    *object,
                                           guint       param_id,
                                           GValue     *value,
                                           GParamSpec *pspec)
{
    NautilusPropertyPageModel *page;

    page = NAUTILUS_PROPERTY_PAGE_MODEL (object);

    switch (param_id)
    {
        case PROP_TITLE:
        {
            g_value_set_string (value, page->title);
        }
        break;

        case PROP_SECTIONS:
        {
            g_value_set_pointer (value, page->sections);
        }
        break;

        case PROP_ITEMS:
        {
            g_value_set_pointer (value, page->items);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        }
        break;
    }
}

static void
nautilus_property_page_model_set_property (GObject      *object,
                                           guint         param_id,
                                           const GValue *value,
                                           GParamSpec   *pspec)
{
    NautilusPropertyPageModel *page;

    page = NAUTILUS_PROPERTY_PAGE_MODEL (object);

    switch (param_id)
    {
        case PROP_TITLE:
        {
            g_free (page->title);
            page->title = g_strdup (g_value_get_string (value));
            g_object_notify (object, "title");
        }
        break;

        case PROP_SECTIONS:
        {
            if (page->sections)
            {
                g_list_free_full (page->sections, free_section);
            }

            page->label = g_list_copy_deep (g_value_get_pointer (value), copy_section, NULL);
            g_object_notify (object, "sections");
        }
        break;

        case PROP_ITEMS:
        {
            if (page->items)
            {
                g_list_free_full (page->sections, free_items);
            }

            page->items = g_list_copy_deep (g_value_get_pointer (value), copy_items, NULL);
            g_object_notify (object, "items");
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        }
        break;
    }
}

static void
nautilus_property_page_model_finalize (GObject *object)
{
    NautilusPropertyPageModel *page;

    page = NAUTILUS_PROPERTY_PAGE_MODEL (object);

    if (page->title != NULL)
    {
        g_free (page->title);
        page->title = NULL;
    }
    if (page->sections != NULL)
    {
        g_list_free_full (page->sections, free_section);
        page->sections = NULL;
    }
    if (page->items != NULL)
    {
        g_list_free_full (page->items, free_items);
        page->items = NULL;
    }

    G_OBJECT_CLASS (nautilus_property_page_model_parent_class)->finalize (object);
}

static void
nautilus_property_page_model_init (NautilusPropertyPageModel *page)
{
}

static void
nautilus_property_page_model_class_init (NautilusPropertyPageModelClass *class)
{
    G_OBJECT_CLASS (class)->finalize = nautilus_property_page_model_finalize;
    G_OBJECT_CLASS (class)->get_property = nautilus_property_page_model_get_property;
    G_OBJECT_CLASS (class)->set_property = nautilus_property_page_model_set_property;

    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_TITLE,
                                     g_param_spec_string ("title",
                                                          "Title",
                                                          "Title of the page",
                                                          NULL,
                                                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_READABLE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_SECTIONS,
                                     g_param_spec_object ("sections",
                                                          "Sections",
                                                          "Sections of the page",
                                                          G_TYPE_POINTER,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_ITEMS,
                                     g_param_spec_object ("items",
                                                          "Items",
                                                          "Items for the property page",
                                                          G_TYPE_POINTER,
                                                          G_PARAM_READWRITE));
}
