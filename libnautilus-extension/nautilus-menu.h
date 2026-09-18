/*
 *  nautilus-menu.h - Menus exported by NautilusMenuProvider objects.
 *
 *  Copyright (C) 2005 Raffaele Sandrini
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
 *           Raffaele Sandrini <rasa@gmx.ch>
 *
 */

#pragma once

#if !defined (NAUTILUS_EXTENSION_H) && !defined (NAUTILUS_COMPILATION)
#warning "Only <nautilus-extension.h> should be included directly."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_MENU (nautilus_menu_get_type ())
#define NAUTILUS_TYPE_MENU_ITEM (nautilus_menu_item_get_type ())

G_DECLARE_FINAL_TYPE     (NautilusMenu, nautilus_menu,
                          NAUTILUS, MENU,
                          GObject)
G_DECLARE_DERIVABLE_TYPE (NautilusMenuItem, nautilus_menu_item,
                          NAUTILUS, MENU_ITEM,
                          GObject)

/**
 * NautilusMenu:
 *
 * An object that describes a submenu in a file manager menu.
 *
 * Extensions can provide #NautilusMenu objects by attaching them to
 * [class@Nautilus.MenuItem] objects, using [method@Nautilus.MenuItem.set_submenu].
 */

/**
 * NautilusMenuItem:
 *
 * An object that describes an item in a file manager menu.
 *
 * Extensions can provide #NautilusMenuItem objects by registering a
 * [iface@Nautilus.MenuProvider] and returning them from
 * [method@Nautilus.MenuProvider.get_file_items], or
 * [method@Nautilus.MenuProvider.get_background_items], which will be called by
 * the main application when creating menus.
 */

struct _NautilusMenuItemClass
{
    GObjectClass parent;

    void (*activate) (NautilusMenuItem *item);
};

/**
 * nautilus_menu_new:
 *
 * Creates a new [class@Nautilus.Menu] object.
 *
 * Returns: a new #NautilusMenu.
 */
NautilusMenu     *nautilus_menu_new              (void);

/**
 * nautilus_menu_append_item:
 * @menu: a #NautilusMenu
 * @item: (transfer full): a #NautilusMenuItem to append
 *
 * Appends a new menu item to this menu.
 */
void              nautilus_menu_append_item      (NautilusMenu     *menu,
                                                  NautilusMenuItem *item);
/**
 * nautilus_menu_get_items:
 * @menu: a #NautilusMenu
 *
 * Returns the list of menu items in this menu.
 *
 * Returns: (nullable) (element-type NautilusMenuItem) (transfer full): the provided #NautilusMenuItem list
 */
GList            *nautilus_menu_get_items        (NautilusMenu     *menu);
/**
 * nautilus_menu_item_list_free:
 * @item_list: (element-type NautilusMenuItem): a list of #NautilusMenuItem
 *
 */
void              nautilus_menu_item_list_free   (GList            *item_list);

/**
 * nautilus_menu_item_new:
 * @name: the identifier for the menu item
 * @label: the user-visible label of the menu item
 * @tip: (nullable): the tooltip of the menu item
 * @icon: (nullable): the name of the icon to display in the menu item
 *
 * Creates a new menu item that can be added to the toolbar or to a contextual menu.
 *
 * Returns: (transfer full): a new #NautilusMenuItem
 */
NautilusMenuItem *nautilus_menu_item_new         (const char       *name,
                                                  const char       *label,
                                                  const char       *tip,
                                                  const char       *icon);

/**
 * nautilus_menu_item_activate:
 * @item: pointer to a #NautilusMenuItem
 *
 * Emits [signal@Nautilus.MenuItem::activate].
 */
void              nautilus_menu_item_activate    (NautilusMenuItem *item);
/**
 * nautilus_menu_item_set_submenu:
 * @item: pointer to a #NautilusMenuItem
 * @menu: (transfer full): pointer to a #NautilusMenu to attach to the button
 *
 * Attaches a menu to the given [class@Nautilus.MenuItem].
 */
void              nautilus_menu_item_set_submenu (NautilusMenuItem *item,
                                                  NautilusMenu     *menu);

G_END_DECLS
