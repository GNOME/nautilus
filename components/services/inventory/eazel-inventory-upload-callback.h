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

#ifndef EAZEL_INVENTORY_UPLOAD_CALLBACK_H
#define EAZEL_INVENTORY_UPLOAD_CALLBACK_H

#include <bonobo/bonobo-object.h>

#include "eazel-inventory.h"

#define EAZEL_TYPE_INVENTORY_UPLOAD_CALLBACK	     (eazel_inventory_upload_callback_get_type ())
#define EAZEL_INVENTORY_UPLOAD_CALLBACK(obj)	     (GTK_CHECK_CAST ((obj), EAZEL_TYPE_INVENTORY_UPLOAD_CALLBACK, EazelInventoryUploadCallback))
#define EAZEL_INVENTORY_UPLOAD_CALLBACK_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), EAZEL_TYPE_INVENTORY_UPLOAD_CALLBACK, EazelInventoryUploadCallbackClass))
#define EAZEL_IS_INVENTORY_UPLOAD_CALLBACK(obj)	     (GTK_CHECK_TYPE ((obj), EAZEL_TYPE_INVENTORY_UPLOAD_CALLBACK))
#define EAZEL_IS_INVENTORY_UPLOAD_CALLBACK_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), EAZEL_TYPE_INVENTORY_UPLOAD_CALLBACK))

typedef struct EazelInventoryUploadCallbackDetails EazelInventoryUploadCallbackDetails;

typedef struct {
	BonoboObject parent;
	EazelInventoryUploadCallbackDetails *details;
} EazelInventoryUploadCallback;

typedef struct {
	BonoboObjectClass parent;
} EazelInventoryUploadCallbackClass;

/* GtkObject support */
GtkType                  eazel_inventory_upload_callback_get_type (void);

EazelInventoryUploadCallback *eazel_inventory_upload_callback_new (EazelInventory *inventory,
								   EazelInventoryDoneCallback callback,
								   gpointer callback_data);

#endif /* EAZEL_INVENTORY_UPLOAD_CALLBACK_H */
