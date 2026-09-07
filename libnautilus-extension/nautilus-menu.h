/*
 * nautilus-menu.h - Menus exported by NautilusMenuProvider objects.
 *
 * SPDX-FileCopyrightText: 2005 Raffaele Sandrini
 * SPDX-FileCopyrightText: 2003 Novell, Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Author: Dave Camp <dave@ximian.com>
 *         Raffaele Sandrini <rasa@gmx.ch>
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

struct _NautilusMenuItemClass
{
    GObjectClass parent;

    void (*activate) (NautilusMenuItem *item);
};

NautilusMenu     *nautilus_menu_new              (void);

void              nautilus_menu_append_item      (NautilusMenu     *menu,
                                                  NautilusMenuItem *item);

GList            *nautilus_menu_get_items        (NautilusMenu     *menu);

void              nautilus_menu_item_list_free   (GList            *item_list);

NautilusMenuItem *nautilus_menu_item_new         (const char       *name,
                                                  const char       *label,
                                                  const char       *tip,
                                                  const char       *icon);

void              nautilus_menu_item_activate    (NautilusMenuItem *item);

void              nautilus_menu_item_set_submenu (NautilusMenuItem *item,
                                                  NautilusMenu     *menu);
G_END_DECLS
