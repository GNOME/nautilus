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

/* nautilus-inventory-view.h -
 */

#ifndef NAUTILUS_INVENTORY_VIEW_H
#define NAUTILUS_INVENTORY_VIEW_H

#include <gtk/gtklabel.h>
#include <libnautilus/nautilus-view.h>

#define NAUTILUS_TYPE_INVENTORY_VIEW	     (nautilus_inventory_view_get_type ())
#define NAUTILUS_INVENTORY_VIEW(obj)	     (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_INVENTORY_VIEW, NautilusInventoryView))
#define NAUTILUS_INVENTORY_VIEW_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_INVENTORY_VIEW, NautilusInventoryViewClass))
#define NAUTILUS_IS_INVENTORY_VIEW(obj)	     (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_INVENTORY_VIEW))
#define NAUTILUS_IS_INVENTORY_VIEW_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_INVENTORY_VIEW))

typedef struct NautilusInventoryViewDetails NautilusInventoryViewDetails;

typedef struct {
	NautilusView parent;
	NautilusInventoryViewDetails *details;
} NautilusInventoryView;

typedef struct {
	NautilusViewClass parent;
} NautilusInventoryViewClass;

/* GtkObject support */
GtkType       nautilus_inventory_view_get_type          (void);

#endif /* NAUTILUS_INVENTORY_VIEW_H */
