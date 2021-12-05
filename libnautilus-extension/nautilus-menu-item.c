/*
 *  nautilus-menu-item.c - Menu items exported by NautilusMenuProvider
 *                         objects.
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
#include <glib/gi18n-lib.h>
#include "nautilus-menu.h"

typedef struct
{
    char *name;
    char *label;
    char *tip;
    char *icon;
    NautilusMenu *menu;
    gboolean sensitive;
    gboolean priority;
} NautilusMenuItemPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (NautilusMenuItem, nautilus_menu_item, G_TYPE_OBJECT)

enum
{
    ACTIVATE,
    LAST_SIGNAL
};

enum
{
    PROP_0,
    PROP_NAME,
    PROP_LABEL,
    PROP_TIP,
    PROP_ICON,
    PROP_SENSITIVE,
    PROP_PRIORITY,
    PROP_MENU,
    LAST_PROP
};

static guint signals[LAST_SIGNAL];

NautilusMenuItem *
nautilus_menu_item_new (const char *name,
                        const char *label,
                        const char *tip,
                        const char *icon)
{
    NautilusMenuItem *item;

    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);

    item = g_object_new (NAUTILUS_TYPE_MENU_ITEM,
                         "name", name,
                         "label", label,
                         "tip", tip,
                         "icon", icon,
                         NULL);

    return item;
}

void
nautilus_menu_item_activate (NautilusMenuItem *self)
{
    g_return_if_fail (NAUTILUS_IS_MENU_ITEM (self));

    g_signal_emit (self, signals[ACTIVATE], 0);
}

void
nautilus_menu_item_set_submenu (NautilusMenuItem *self,
                                NautilusMenu     *menu)
{
    g_return_if_fail (NAUTILUS_IS_MENU_ITEM (self));

    g_object_set (self, "menu", menu, NULL);
}

static void
nautilus_menu_item_get_property (GObject    *object,
                                 guint       param_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    NautilusMenuItem *item;
    NautilusMenuItemPrivate *priv;

    item = NAUTILUS_MENU_ITEM (object);
    priv = nautilus_menu_item_get_instance_private (item);

    switch (param_id)
    {
        case PROP_NAME:
        {
            g_value_set_string (value, priv->name);
        }
        break;

        case PROP_LABEL:
        {
            g_value_set_string (value, priv->label);
        }
        break;

        case PROP_TIP:
        {
            g_value_set_string (value, priv->tip);
        }
        break;

        case PROP_ICON:
        {
            g_value_set_string (value, priv->icon);
        }
        break;

        case PROP_SENSITIVE:
        {
            g_value_set_boolean (value, priv->sensitive);
        }
        break;

        case PROP_PRIORITY:
        {
            g_value_set_boolean (value, priv->priority);
        }
        break;

        case PROP_MENU:
        {
            g_value_set_object (value, priv->menu);
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
nautilus_menu_item_set_property (GObject      *object,
                                 guint         param_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    NautilusMenuItem *item;
    NautilusMenuItemPrivate *priv;

    item = NAUTILUS_MENU_ITEM (object);
    priv = nautilus_menu_item_get_instance_private (item);

    switch (param_id)
    {
        case PROP_NAME:
        {
            g_free (priv->name);
            priv->name = g_strdup (g_value_get_string (value));
            g_object_notify (object, "name");
        }
        break;

        case PROP_LABEL:
        {
            g_free (priv->label);
            priv->label = g_strdup (g_value_get_string (value));
            g_object_notify (object, "label");
        }
        break;

        case PROP_TIP:
        {
            g_free (priv->tip);
            priv->tip = g_strdup (g_value_get_string (value));
            g_object_notify (object, "tip");
        }
        break;

        case PROP_ICON:
        {
            g_free (priv->icon);
            priv->icon = g_strdup (g_value_get_string (value));
            g_object_notify (object, "icon");
        }
        break;

        case PROP_SENSITIVE:
        {
            priv->sensitive = g_value_get_boolean (value);
            g_object_notify (object, "sensitive");
        }
        break;

        case PROP_PRIORITY:
        {
            priv->priority = g_value_get_boolean (value);
            g_object_notify (object, "priority");
        }
        break;

        case PROP_MENU:
        {
            if (priv->menu)
            {
                g_object_unref (priv->menu);
            }
            priv->menu = g_object_ref (g_value_get_object (value));
            g_object_notify (object, "menu");
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
nautilus_menu_item_finalize (GObject *object)
{
    NautilusMenuItem *item;
    NautilusMenuItemPrivate *priv;

    item = NAUTILUS_MENU_ITEM (object);
    priv = nautilus_menu_item_get_instance_private (item);

    g_free (priv->name);
    g_free (priv->label);
    g_free (priv->tip);
    g_free (priv->icon);
    if (priv->menu)
    {
        g_object_unref (priv->menu);
    }

    G_OBJECT_CLASS (nautilus_menu_item_parent_class)->finalize (object);
}

static void
nautilus_menu_item_init (NautilusMenuItem *self)
{
    NautilusMenuItemPrivate *priv;

    priv = nautilus_menu_item_get_instance_private (self);

    priv->sensitive = TRUE;
    priv->menu = NULL;
}

static void
nautilus_menu_item_class_init (NautilusMenuItemClass *class)
{
    G_OBJECT_CLASS (class)->finalize = nautilus_menu_item_finalize;
    G_OBJECT_CLASS (class)->get_property = nautilus_menu_item_get_property;
    G_OBJECT_CLASS (class)->set_property = nautilus_menu_item_set_property;

    /**
     * NautilusMenuItem::activate:
     * @item: the object which received the signal
     *
     * Signals that the user has activated this menu item.
     */
    signals[ACTIVATE] =
        g_signal_new ("activate",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (NautilusMenuItemClass,
                                       activate),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_NAME,
                                     g_param_spec_string ("name",
                                                          "Name",
                                                          "Name of the item",
                                                          NULL,
                                                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_READABLE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_LABEL,
                                     g_param_spec_string ("label",
                                                          "Label",
                                                          "Label to display to the user",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_TIP,
                                     g_param_spec_string ("tip",
                                                          "Tip",
                                                          "Tooltip for the menu item",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_ICON,
                                     g_param_spec_string ("icon",
                                                          "Icon",
                                                          "Name of the icon to display in the menu item",
                                                          NULL,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_SENSITIVE,
                                     g_param_spec_boolean ("sensitive",
                                                           "Sensitive",
                                                           "Whether the menu item is sensitive",
                                                           TRUE,
                                                           G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_PRIORITY,
                                     g_param_spec_boolean ("priority",
                                                           "Priority",
                                                           "Show priority text in toolbars",
                                                           TRUE,
                                                           G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_MENU,
                                     g_param_spec_object ("menu",
                                                          "Menu",
                                                          "The menu belonging to this item. May be null.",
                                                          NAUTILUS_TYPE_MENU,
                                                          G_PARAM_READWRITE));
}
