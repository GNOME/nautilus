/*
 *  nautilus-menu-item.h - Menu items exported by NautilusMenuProvider
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

#ifndef NAUTILUS_MENU_ITEM_H
#define NAUTILUS_MENU_ITEM_H

#include <glib-object.h>
#include "nautilus-extension-types.h"

G_BEGIN_DECLS

#define NAUTILUS_TYPE_MENU_ITEM            (nautilus_menu_item_get_type())
#define NAUTILUS_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_MENU_ITEM, NautilusMenuItem))
#define NAUTILUS_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_MENU_ITEM, NautilusMenuItemClass))
#define NAUTILUS_MENU_IS_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_MENU_ITEM))
#define NAUTILUS_MENU_IS_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_MENU_ITEM))
#define NAUTILUS_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), NAUTILUS_TYPE_MENU_ITEM, NautilusMenuItemClass))

typedef struct _NautilusMenuItem        NautilusMenuItem;
typedef struct _NautilusMenuItemDetails NautilusMenuItemDetails;
typedef struct _NautilusMenuItemClass   NautilusMenuItemClass;

struct _NautilusMenuItem {
	GObject parent;

	NautilusMenuItemDetails *details;
};

struct _NautilusMenuItemClass {
	GObjectClass parent;

	void (*activate) (NautilusMenuItem *item);
};

GType             nautilus_menu_item_get_type      (void);
NautilusMenuItem *nautilus_menu_item_new           (const char       *name,
						    const char       *label,
						    const char       *tip,
						    const char       *icon);

void              nautilus_menu_item_activate      (NautilusMenuItem *item);

/* NautilusMenuItem has the following properties:
 *   name (string)        - the identifier for the menu item
 *   label (string)       - the user-visible label of the menu item
 *   tip (string)         - the tooltip of the menu item 
 *   icon (string)        - the name of the icon to display in the menu item
 *   sensitive (boolean)  - whether the menu item is sensitive or not
 *   priority (boolean)   - used for toolbar items, whether to show priority
 *                          text.
 */


G_END_DECLS

#endif
