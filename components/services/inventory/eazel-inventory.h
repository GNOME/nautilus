/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2001 Eazel, Inc
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

/* eazel-inventory.h - Inventory component. 
 */

#ifndef EAZEL_INVENTORY_H
#define EAZEL_INVENTORY_H

#include <gtk/gtkobject.h>

#define EAZEL_TYPE_INVENTORY	          (eazel_inventory_get_type ())
#define EAZEL_INVENTORY(obj)	          (GTK_CHECK_CAST ((obj), EAZEL_TYPE_INVENTORY, EazelInventory))
#define EAZEL_INVENTORY_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), EAZEL_TYPE_INVENTORY, EazelInventoryClass))
#define EAZEL_IS_INVENTORY(obj)	  (GTK_CHECK_TYPE ((obj), EAZEL_TYPE_INVENTORY))
#define EAZEL_IS_INVENTORY_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), EAZEL_TYPE_INVENTORY))

typedef struct EazelInventoryDetails EazelInventoryDetails;
typedef struct EazelInventory EazelInventory;
typedef struct EazelInventoryClass EazelInventoryClass;
 

typedef void (*EazelInventoryDoneCallback) (EazelInventory *inventory,
					    gboolean        succeeded,
					    gpointer        callback_data);

struct EazelInventory {
	GtkObject parent;
	EazelInventoryDetails *details;
};

struct EazelInventoryClass {
	GtkObjectClass parent;
};

/* GtkObject support */
GtkType          eazel_inventory_get_type       (void);
EazelInventory * eazel_inventory_get            (void);
gboolean         eazel_inventory_get_enabled    (EazelInventory *           inventory);
void             eazel_inventory_set_enabled    (EazelInventory *           inventory,
						 gboolean                   enabled);
char *           eazel_inventory_get_machine_id (EazelInventory *           inventory);
void             eazel_inventory_upload         (EazelInventory *           inventory,
						 EazelInventoryDoneCallback done_callback,
						 gpointer                   callback_data);

#endif /* EAZEL_INVENTORY_H */
