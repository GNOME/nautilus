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

/* nautilus-adapter-factory-server.c - Server object for a factory to
 * create NautilusAdapter objects.
 */

#ifndef NAUTILUS_ADAPTER_FACTORY_SERVER_H
#define NAUTILUS_ADAPTER_FACTORY_SERVER_H

#include <gtk/gtklabel.h>
#include <libnautilus/nautilus-view.h>

#define NAUTILUS_TYPE_ADAPTER_FACTORY_SERVER	     (nautilus_adapter_factory_server_get_type ())
#define NAUTILUS_ADAPTER_FACTORY_SERVER(obj)	     (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_ADAPTER_FACTORY_SERVER, NautilusAdapterFactoryServer))
#define NAUTILUS_ADAPTER_FACTORY_SERVER_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ADAPTER_FACTORY_SERVER, NautilusAdapterFactoryServerClass))
#define NAUTILUS_IS_ADAPTER_FACTORY_SERVER(obj)	     (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_ADAPTER_FACTORY_SERVER))
#define NAUTILUS_IS_ADAPTER_FACTORY_SERVER_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ADAPTER_FACTORY_SERVER))

typedef struct {
	BonoboObject parent;
} NautilusAdapterFactoryServer;

typedef struct {
	BonoboObjectClass parent;
} NautilusAdapterFactoryServerClass;

/* GtkObject support */
GtkType       nautilus_adapter_factory_server_get_type          (void);


#endif /* NAUTILUS_ADAPTER_FACTORY_SERVER_H */
