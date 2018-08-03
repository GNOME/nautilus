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
 *  License along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 *  Author:  Raffaele Sandrini <rasa@gmx.ch>
 *
 */

#include <config.h>
#include "nautilus-menu.h"

#include <glib.h>
#include <glib/gi18n-lib.h>

struct _NautilusMenu
{
    GObject parent_instance;

    GList *item_list;
};

G_DEFINE_TYPE (NautilusMenu, nautilus_menu, G_TYPE_OBJECT);

void
nautilus_menu_append_item (NautilusMenu     *self,
                           NautilusMenuItem *menu_item)
{
    g_return_if_fail (NAUTILUS_IS_MENU (self));
    g_return_if_fail (NAUTILUS_IS_MENU_ITEM (menu_item));

    self->item_list = g_list_append (self->item_list, g_object_ref (menu_item));
}

GList *
nautilus_menu_get_items (NautilusMenu *self)
{
    GList *item_list;

    g_return_val_if_fail (NAUTILUS_IS_MENU (self), NULL);

    item_list = g_list_copy (self->item_list);
    g_list_foreach (item_list, (GFunc) g_object_ref, NULL);

    return item_list;
}

void
nautilus_menu_item_list_free (GList *item_list)
{
    g_return_if_fail (item_list != NULL);

    g_list_foreach (item_list, (GFunc) g_object_unref, NULL);
    g_list_free (item_list);
}

static void
nautilus_menu_finalize (GObject *object)
{
    NautilusMenu *menu = NAUTILUS_MENU (object);

    g_clear_pointer (&menu->item_list, g_list_free);

    G_OBJECT_CLASS (nautilus_menu_parent_class)->finalize (object);
}

static void
nautilus_menu_init (NautilusMenu *self)
{
    self->item_list = NULL;
}

static void
nautilus_menu_class_init (NautilusMenuClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = nautilus_menu_finalize;
}

NautilusMenu *
nautilus_menu_new (void)
{
    return g_object_new (NAUTILUS_TYPE_MENU, NULL);
}
