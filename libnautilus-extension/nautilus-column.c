/*
 *  nautilus-column.c - Info columns exported by NautilusColumnProvider
 *                      objects.
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

#include "nautilus-column.h"

enum
{
    PROP_0,
    PROP_NAME,
    PROP_ATTRIBUTE,
    PROP_ATTRIBUTE_Q,
    PROP_LABEL,
    PROP_DESCRIPTION,
    PROP_XALIGN,
    PROP_DEFAULT_SORT_ORDER,
    LAST_PROP
};

struct _NautilusColumn
{
    GObject parent_instance;

    char *name;
    GQuark attribute;
    char *label;
    char *description;
    float xalign;
    int default_sort_order; /* Actually, meant to store GtkSortType */
};

G_DEFINE_TYPE (NautilusColumn, nautilus_column, G_TYPE_OBJECT);

NautilusColumn *
nautilus_column_new (const char *name,
                     const char *attribute,
                     const char *label,
                     const char *description)
{
    NautilusColumn *column;

    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (attribute != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);
    g_return_val_if_fail (description != NULL, NULL);

    column = g_object_new (NAUTILUS_TYPE_COLUMN,
                           "name", name,
                           "attribute", attribute,
                           "label", label,
                           "description", description,
                           NULL);

    return column;
}

static void
nautilus_column_get_property (GObject    *object,
                              guint       param_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
    NautilusColumn *column;

    column = NAUTILUS_COLUMN (object);

    switch (param_id)
    {
        case PROP_NAME:
        {
            g_value_set_string (value, column->name);
        }
        break;

        case PROP_ATTRIBUTE:
        {
            g_value_set_string (value, g_quark_to_string (column->attribute));
        }
        break;

        case PROP_ATTRIBUTE_Q:
        {
            g_value_set_uint (value, column->attribute);
        }
        break;

        case PROP_LABEL:
        {
            g_value_set_string (value, column->label);
        }
        break;

        case PROP_DESCRIPTION:
        {
            g_value_set_string (value, column->description);
        }
        break;

        case PROP_XALIGN:
        {
            g_value_set_float (value, column->xalign);
        }
        break;

        case PROP_DEFAULT_SORT_ORDER:
        {
            g_value_set_enum (value, column->default_sort_order);
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
nautilus_column_set_property (GObject      *object,
                              guint         param_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
    NautilusColumn *column;

    column = NAUTILUS_COLUMN (object);

    switch (param_id)
    {
        case PROP_NAME:
        {
            g_free (column->name);
            column->name = g_strdup (g_value_get_string (value));
            g_object_notify (object, "name");
        }
        break;

        case PROP_ATTRIBUTE:
        {
            column->attribute = g_quark_from_string (g_value_get_string (value));
            g_object_notify (object, "attribute");
            g_object_notify (object, "attribute_q");
        }
        break;

        case PROP_LABEL:
        {
            g_free (column->label);
            column->label = g_strdup (g_value_get_string (value));
            g_object_notify (object, "label");
        }
        break;

        case PROP_DESCRIPTION:
        {
            g_free (column->description);
            column->description = g_strdup (g_value_get_string (value));
            g_object_notify (object, "description");
        }
        break;

        case PROP_XALIGN:
        {
            column->xalign = g_value_get_float (value);
            g_object_notify (object, "xalign");
        }
        break;

        case PROP_DEFAULT_SORT_ORDER:
        {
            column->default_sort_order = g_value_get_enum (value);
            g_object_notify (object, "default-sort-order");
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
nautilus_column_finalize (GObject *object)
{
    NautilusColumn *column;

    column = NAUTILUS_COLUMN (object);

    g_free (column->name);
    g_free (column->label);
    g_free (column->description);

    G_OBJECT_CLASS (nautilus_column_parent_class)->finalize (object);
}

static void
nautilus_column_init (NautilusColumn *column)
{
    column->xalign = 0.0;
}

static void
nautilus_column_class_init (NautilusColumnClass *class)
{
    G_OBJECT_CLASS (class)->finalize = nautilus_column_finalize;
    G_OBJECT_CLASS (class)->get_property = nautilus_column_get_property;
    G_OBJECT_CLASS (class)->set_property = nautilus_column_set_property;

    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_NAME,
                                     g_param_spec_string ("name",
                                                          "Name",
                                                          "Name of the column",
                                                          NULL,
                                                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_READABLE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_ATTRIBUTE,
                                     g_param_spec_string ("attribute",
                                                          "Attribute",
                                                          "The attribute name to display",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_ATTRIBUTE_Q,
                                     g_param_spec_uint ("attribute_q",
                                                        "Attribute quark",
                                                        "The attribute name to display, in quark form",
                                                        0, G_MAXUINT, 0,
                                                        G_PARAM_READABLE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_LABEL,
                                     g_param_spec_string ("label",
                                                          "Label",
                                                          "Label to display in the column",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_DESCRIPTION,
                                     g_param_spec_string ("description",
                                                          "Description",
                                                          "A user-visible description of the column",
                                                          NULL,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_XALIGN,
                                     g_param_spec_float ("xalign",
                                                         "xalign",
                                                         "The x-alignment of the column",
                                                         0.0,
                                                         1.0,
                                                         0.0,
                                                         G_PARAM_READWRITE));
    /**
     * NautilusColumn:default-sort-order: (type gboolean)
     *
     * Actually meant to store the enum values of GtkSortType, but we don't want
     * extensions to depend on GTK. Also, this is for internal consumption only.
     *
     * Stability: Private: Internal to the application.
     */
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_DEFAULT_SORT_ORDER,
                                     g_param_spec_int ("default-sort-order",
                                                       "Default sort order",
                                                       "Default sort order",
                                                       G_MININT, G_MAXINT, 0,
                                                       G_PARAM_READWRITE));
}
