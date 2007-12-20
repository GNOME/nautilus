/*
 *  nautilus-menu.h - Menus exported by NautilusMenuProvider objects.
 *
 *  Copyright (C) 2005 Raffaele Sandrini
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
 *  Author:  Raffaele Sandrini <rasa@gmx.ch>
 *
 */

#include <config.h>
#include "nautilus-menu.h"
#include "nautilus-extension-i18n.h"

#include <glib/glist.h>

#define NAUTILUS_MENU_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NAUTILUS_TYPE_MENU, NautilusMenuPrivate))

struct _NautilusMenuPrivate {
	GList *item_list;
};

void
nautilus_menu_append_item (NautilusMenu *this, NautilusMenuItem *item)
{
	g_return_if_fail (this != NULL);
	g_return_if_fail (item != NULL);
	
	this->private->item_list = g_list_append (this->private->item_list, g_object_ref (item));
}

GList *
nautilus_menu_get_items (NautilusMenu *this)
{
	GList *item_list;

	g_return_val_if_fail (this != NULL, NULL);
	
	item_list = g_list_copy (this->private->item_list);
	g_list_foreach (item_list, (GFunc)g_object_ref, NULL);
	
	return item_list;
}

void
nautilus_menu_item_list_free (GList *item_list)
{
	g_return_if_fail (item_list != NULL);
	
	g_list_foreach (item_list, (GFunc)g_object_unref, NULL);
	g_list_free (item_list);
}

/* Type initialization */

static void
nautilus_menu_finalize (GObject *object)
{
	NautilusMenu *this = NAUTILUS_MENU (object);
	GObjectClass *parent_class = g_type_class_peek_parent (NAUTILUS_MENU_GET_CLASS (object));
	
	if (this->private->item_list) {
		g_list_free (this->private->item_list);
	}

	parent_class->finalize (object);
}

static void
nautilus_menu_init (NautilusMenu *this)
{
	this->private = NAUTILUS_MENU_GET_PRIVATE (this);
	
	this->private->item_list = NULL;
}

static void
nautilus_menu_class_init (NautilusMenuClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (NautilusMenuPrivate));
	
	object_class->finalize = nautilus_menu_finalize;
}

GType
nautilus_menu_get_type (void)
{
	static GType type = 0;

	if(type == 0) {
		const GTypeInfo info = {
			sizeof (NautilusMenuClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) nautilus_menu_class_init,
			(GClassFinalizeFunc) NULL,
			NULL,
			sizeof (NautilusMenu),
			0,
			(GInstanceInitFunc) nautilus_menu_init,
		};

		type = g_type_register_static (G_TYPE_OBJECT, "NautilusMenu", &info, 0);
	}

	return type;
}

/* public constructors */

NautilusMenu *
nautilus_menu_new (void)
{
	NautilusMenu *obj;
	
	obj = NAUTILUS_MENU (g_object_new (NAUTILUS_TYPE_MENU, NULL));
	
	return obj;
}
