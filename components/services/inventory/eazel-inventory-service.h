/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* 
 * Copyright (C) 2000 Eazel, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Eskil Heyn Olsen <eskil@eazel.com>
 *
 */

#ifndef EAZEL_INVENTORY_SERVICE_H
#define EAZEL_INVENTORY_SERVICE_H

#include <bonobo/bonobo-object.h>

#include "eazel-inventory-service-interface.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define EAZEL_TYPE_INVENTORY_SERVICE             (eazel_inventory_service_get_type ())
#define EAZEL_INVENTORY_SERVICE(obj)             (GTK_CHECK_CAST ((obj), EAZEL_TYPE_INVENTORY_SERVICE, EazelInventoryService))
#define EAZEL_INVENTORY_SERVICE_CLASS(klass)     (GTK_CHECK_CLASS_CAST ((klass), EAZEL_TYPE_INVENTORY_SERVICE, EazelInventoryServiceClass))
#define EAZEL_IS_INVENTORY_SERVICE(obj)          (GTK_CHECK_TYPE ((obj), EAZEL_TYPE_INVENTORY_SERVICE))
#define EAZEL_IS_INVENTORY_SERVICE_CLASS(klass)  (GTK_CHECK_CLASS_TYPE ((klass), EAZEL_TYPE_INVENTORY_SERVICE)

typedef struct EazelInventoryServiceDetails EazelInventoryServiceDetails;
typedef struct EazelInventoryService        EazelInventoryService;
typedef struct EazelInventoryServiceClass   EazelInventoryServiceClass;

struct EazelInventoryService
{
	BonoboObject base;
	EazelInventoryServiceDetails *details;
};

struct EazelInventoryServiceClass 
{
	BonoboObjectClass base;

	gpointer servant_vepv;
};



GtkType                             eazel_inventory_service_get_type (void);
EazelInventoryService*              eazel_inventory_service_new      (void);
POA_Trilobite_Eazel_Inventory__epv* eazel_inventory_service_get_epv  (void);
void                                eazel_inventory_service_unref    (GtkObject        *object);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* EAZEL_INVENTORY_SERVICE_H */


