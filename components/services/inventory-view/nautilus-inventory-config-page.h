/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* nautilus-inventory-config-page.h - 
 */

#ifndef NAUTILUS_INVENTORY_CONFIG_PAGE_H
#define NAUTILUS_INVENTORY_CONFIG_PAGE_H

#include <gtk/gtkvbox.h>
#include <libnautilus/nautilus-view.h>

#define NAUTILUS_TYPE_INVENTORY_CONFIG_PAGE            (nautilus_inventory_config_page_get_type ())
#define NAUTILUS_INVENTORY_CONFIG_PAGE(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_INVENTORY_CONFIG_PAGE, NautilusInventoryConfigPage))
#define NAUTILUS_INVENTORY_CONFIG_PAGE_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_INVENTORY_CONFIG_PAGE, NautilusInventoryConfigPageClass))
#define NAUTILUS_IS_INVENTORY_CONFIG_PAGE(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_INVENTORY_CONFIG_PAGE))
#define NAUTILUS_IS_INVENTORY_CONFIG_PAGE_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_INVENTORY_CONFIG_PAGE))

typedef struct NautilusInventoryConfigPage        NautilusInventoryConfigPage;
typedef struct NautilusInventoryConfigPageClass   NautilusInventoryConfigPageClass;
typedef struct NautilusInventoryConfigPageDetails NautilusInventoryConfigPageDetails;

struct NautilusInventoryConfigPage {
	GtkVBox base;
	NautilusInventoryConfigPageDetails *details;
};

struct NautilusInventoryConfigPageClass {
	GtkVBoxClass base;
};

GtkType nautilus_inventory_config_page_get_type (void);

GtkWidget *nautilus_inventory_config_page_new (NautilusView *view);

#endif /* NAUTILUS_INVENTORY_CONFIG_PAGE_H */

