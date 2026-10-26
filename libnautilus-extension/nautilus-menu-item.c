/*
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
 */

#include <config.h>
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
    PROP_NAME = 1,
    PROP_LABEL,
    PROP_TIP,
    PROP_ICON,
    PROP_SENSITIVE,
    PROP_PRIORITY,
    PROP_MENU,
    NUM_PROPS
};

static GParamSpec *properties[NUM_PROPS];

static guint signals[LAST_SIGNAL];

/**
 * nautilus_menu_item_new:
 * @name: the identifier for the menu item
 * @label: the user-visible label of the menu item
 * @tip: (nullable): this has no effect
 * @icon: (nullable): this has no effect
 *
 * Creates a new menu item that can be added to the toolbar or to a contextual menu.
 *
 * Returns: (transfer full): a new #NautilusMenuItem
 */
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

/**
 * nautilus_menu_item_activate:
 * @item: pointer to a #NautilusMenuItem
 *
 * Emits #NautilusMenuItem::activate.
 */
void
nautilus_menu_item_activate (NautilusMenuItem *self)
{
    g_return_if_fail (NAUTILUS_IS_MENU_ITEM (self));

    g_signal_emit (self, signals[ACTIVATE], 0);
}

/**
 * nautilus_menu_item_set_submenu:
 * @item: pointer to a #NautilusMenuItem
 * @menu: (transfer full): pointer to a #NautilusMenu to attach to the button
 *
 * Attaches a menu to the given #NautilusMenuItem.
 */
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
            if (g_set_str (&priv->name, g_value_get_string (value)))
            {
                g_object_notify (object, "name");
            }
        }
        break;

        case PROP_LABEL:
        {
            if (g_set_str (&priv->label, g_value_get_string (value)))
            {
                g_object_notify (object, "label");
            }
        }
        break;

        case PROP_TIP:
        {
            if (g_set_str (&priv->tip, g_value_get_string (value)))
            {
                g_object_notify (object, "tip");
            }
        }
        break;

        case PROP_ICON:
        {
            if (g_set_str (&priv->icon, g_value_get_string (value)))
            {
                g_object_notify (object, "icon");
            }
        }
        break;

        case PROP_SENSITIVE:
        {
            gboolean sensitive = g_value_get_boolean (value);

            if (priv->sensitive != sensitive)
            {
                priv->sensitive = sensitive;
                g_object_notify (object, "sensitive");
            }
        }
        break;

        case PROP_PRIORITY:
        {
            gboolean priority = g_value_get_boolean (value);

            if (priv->priority != priority)
            {
                priv->priority = priority;
                g_object_notify (object, "priority");
            }
        }
        break;

        case PROP_MENU:
        {
            if (g_set_object (&priv->menu, g_value_get_object (value)))
            {
                g_object_notify (object, "menu");
            }
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
    GObjectClass *object_class = G_OBJECT_CLASS (class);

    object_class->finalize = nautilus_menu_item_finalize;
    object_class->get_property = nautilus_menu_item_get_property;
    object_class->set_property = nautilus_menu_item_set_property;

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

    /**
     * NautilusMenuItem:name:
     *
     * A unique identifier for the `NautilusMenuItem`.  This is not user visible.
     */
    properties[PROP_NAME] =
        g_param_spec_string ("name", NULL, NULL,
                             NULL,
                             G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE |
                             G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
    /**
     * NautilusMenuItem:label:
     *
     * A user visible string describing the `NautilusMenuItem`.
     */
    properties[PROP_LABEL] =
        g_param_spec_string ("label", NULL, NULL,
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
    /**
     * NautilusMenuItem:tip:
     *
     * This property has no effect.
     *
     * Deprecated: 4.1
     */
    properties[PROP_TIP] =
        g_param_spec_string ("tip", NULL, NULL,
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
    /**
     * NautilusMenuItem:icon:
     *
     * This property has no effect.
     *
     * Deprecated: 4.1
     */
    properties[PROP_ICON] =
        g_param_spec_string ("icon", NULL, NULL,
                             NULL,
                             G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
    /**
     * NautilusMenuItem:sensitive:
     *
     * Whether the `NautilusMenuItem` should be sensitive (i.e. clickable).
     *
     */
    properties[PROP_SENSITIVE] =
        g_param_spec_boolean ("sensitive", NULL, NULL,
                              TRUE,
                              G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
    /**
     * NautilusMenuItem:priority:
     *
     * This property has no effect.
     *
     * Deprecated: 4.1
     */
    properties[PROP_PRIORITY] =
        g_param_spec_boolean ("priority", NULL, NULL,
                              TRUE,
                              G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);
    /**
     * NautilusMenuItem:menu:
     *
     * A submenu for the current `NautilusMenuItem`.
     */
    properties[PROP_MENU] =
        g_param_spec_object ("menu", NULL, NULL,
                             NAUTILUS_TYPE_MENU,
                             G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (object_class, NUM_PROPS, properties);
}
