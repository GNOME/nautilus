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
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 *  Author:  Dave Camp <dave@ximian.com>
 *
 */

#include <config.h>
#include "nautilus-menu-item.h"
#include "nautilus-extension-i18n.h"

enum {
	ACTIVATE,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_NAME,
	PROP_LABEL,
	PROP_TIP,
	PROP_ICON,
	PROP_SENSITIVE,
	LAST_PROP
};

struct _NautilusMenuItemDetails {
	char *name;
	char *label;
	char *tip;
	char *icon;
	gboolean sensitive;
};

static guint signals [LAST_SIGNAL];

NautilusMenuItem *
nautilus_menu_item_new (const char *name,
			const char *label,
			const char *tip,
			const char *icon)
{
	NautilusMenuItem *item;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (label != NULL, NULL);
	g_return_val_if_fail (tip != NULL, NULL);

	item = g_object_new (NAUTILUS_TYPE_MENU_ITEM, 
			     "name", name,
			     "label", label,
			     "tip", tip,
			     "icon", icon,
			     NULL);

	return item;
}

const char *
nautilus_menu_item_get_name (NautilusMenuItem *item)
{
	return item->details->name;
}

const char *
nautilus_menu_item_get_label (NautilusMenuItem *item)
{
	return item->details->label;
}

void
nautilus_menu_item_set_label (NautilusMenuItem *item,
			      const char *label)
{
	g_object_set (item, "label", label, NULL);
}


const char *
nautilus_menu_item_get_tip (NautilusMenuItem *item)
{
	return item->details->tip;
}

void
nautilus_menu_item_set_tip (NautilusMenuItem *item,
			      const char *tip)
{
	g_object_set (item, "tip", tip, NULL);
}

const char *
nautilus_menu_item_get_icon (NautilusMenuItem *item)
{
	return item->details->icon;
}

void
nautilus_menu_item_set_icon  (NautilusMenuItem *item,
			      const char *icon)
{
	g_object_set (item, "icon", icon, NULL);
}

gboolean
nautilus_menu_item_get_sensitive (NautilusMenuItem *item)
{
	return item->details->sensitive;
}

void
nautilus_menu_item_set_sensitive (NautilusMenuItem *item,
				  gboolean sensitive)
{
	g_object_set (item, "sensitve", sensitive, NULL);
}

void
nautilus_menu_item_activate (NautilusMenuItem *item)
{
	g_signal_emit (item, signals[ACTIVATE], 0);
}

static void
nautilus_menu_item_get_property (GObject *object,
				 guint param_id,
				 GValue *value,
				 GParamSpec *pspec)
{
	NautilusMenuItem *item;
	
	item = NAUTILUS_MENU_ITEM (object);
	
	switch (param_id) {
	case PROP_NAME :
		g_value_set_string (value, item->details->name);
		break;
	case PROP_LABEL :
		g_value_set_string (value, item->details->label);
		break;
	case PROP_TIP :
		g_value_set_string (value, item->details->tip);
		break;
	case PROP_ICON :
		g_value_set_string (value, item->details->icon);
		break;
	case PROP_SENSITIVE :
		g_value_set_boolean (value, item->details->sensitive);
		break;
	default :
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
nautilus_menu_item_set_property (GObject *object,
				 guint param_id,
				 const GValue *value,
				 GParamSpec *pspec)
{
	NautilusMenuItem *item;
	
	item = NAUTILUS_MENU_ITEM (object);

	switch (param_id) {
	case PROP_NAME :
		g_free (item->details->name);
		item->details->name = g_strdup (g_value_get_string (value));
		g_object_notify (object, "name");
		break;
	case PROP_LABEL :
		g_free (item->details->label);
		item->details->label = g_strdup (g_value_get_string (value));
		g_object_notify (object, "label");
		break;
	case PROP_TIP :
		g_free (item->details->tip);
		item->details->tip = g_strdup (g_value_get_string (value));
		g_object_notify (object, "tip");
		break;
	case PROP_ICON :
		g_free (item->details->icon);
		item->details->icon = g_strdup (g_value_get_string (value));
		g_object_notify (object, "icon");
		break;
	case PROP_SENSITIVE :
		item->details->sensitive = g_value_get_boolean (value);
		g_object_notify (object, "sensitive");
		break;
	default :
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
		break;
	}
}

static void
nautilus_menu_item_finalize (GObject *object)
{
	NautilusMenuItem *item;
	
	item = NAUTILUS_MENU_ITEM (object);

	g_free (item->details->name);
	g_free (item->details->label);
	g_free (item->details->tip);
	g_free (item->details->icon);

	g_free (item->details);
}

static void
nautilus_menu_item_instance_init (NautilusMenuItem *item)
{
	item->details = g_new0 (NautilusMenuItemDetails, 1);
	item->details->sensitive = TRUE;
}

static void
nautilus_menu_item_class_init (NautilusMenuItemClass *class)
{
	G_OBJECT_CLASS (class)->finalize = nautilus_menu_item_finalize;
	G_OBJECT_CLASS (class)->get_property = nautilus_menu_item_get_property;
	G_OBJECT_CLASS (class)->set_property = nautilus_menu_item_set_property;

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
							      _("Name"),
							      _("Name of the item"),
							      NULL,
							      G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_READABLE));
	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_LABEL,
					 g_param_spec_string ("label",
							      _("Label"),
							      _("Label to display to the user"),
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_TIP,
					 g_param_spec_string ("tip",
							      _("Tip"),
							      _("Tooltip for the menu item"),
							      NULL,
							      G_PARAM_READWRITE));
	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_ICON,
					 g_param_spec_string ("icon",
							      _("Icon"),
							      _("Name of the icon to display in the menu item"),
							      NULL,
							      G_PARAM_READWRITE));

	g_object_class_install_property (G_OBJECT_CLASS (class),
					 PROP_SENSITIVE,
					 g_param_spec_boolean ("sensitive",
							       _("Sensitive"),
							       _("Whether the menu item is sensitive"),
							       TRUE,
							       G_PARAM_READWRITE));
}

GType 
nautilus_menu_item_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (NautilusMenuItemClass),
			NULL,
			NULL,
			(GClassInitFunc)nautilus_menu_item_class_init,
			NULL,
			NULL,
			sizeof (NautilusMenuItem),
			0,
			(GInstanceInitFunc)nautilus_menu_item_instance_init
		};
		
		type = g_type_register_static 
			(G_TYPE_OBJECT, 
			 "NautilusMenuItem",
			 &info, 0);
	}

	return type;
}

