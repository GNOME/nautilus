/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 *  Copyright (C) 2000 Eazel, Inc.
 *                2000 SuSE GmbH.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors: Maciej Stachowiak <mjs@eazel.com>
 *           Martin Baulig <baulig@suse.de>
 *
 */

#ifndef NAUTILUS_ZOOMABLE_PROXY_H
#define NAUTILUS_ZOOMABLE_PROXY_H

#include <bonobo/bonobo-zoomable.h>

#define NAUTILUS_ZOOMABLE_PROXY_TYPE		(nautilus_zoomable_proxy_get_type ())
#define NAUTILUS_ZOOMABLE_PROXY(o)		(GTK_CHECK_CAST ((o), NAUTILUS_ZOOMABLE_PROXY_TYPE, NautilusZoomableProxy))
#define NAUTILUS_ZOOMABLE_PROXY_CLASS(k)	(GTK_CHECK_CLASS_CAST((k), NAUTILUS_ZOOMABLE_PROXY_TYPE, NautilusZoomableProxyClass))
#define NAUTILUS_IS_ZOOMABLE_PROXY(o)		(GTK_CHECK_TYPE ((o), NAUTILUS_ZOOMABLE_PROXY_TYPE))
#define NAUTILUS_S_ZOOMABLE_PROXY_CLASS(k)	(GTK_CHECK_CLASS_TYPE ((k), NAUTILUS_ZOOMABLE_PROXY_TYPE))

typedef struct _NautilusZoomableProxy		NautilusZoomableProxy;
typedef struct _NautilusZoomableProxyPrivate	NautilusZoomableProxyPrivate;
typedef struct _NautilusZoomableProxyClass	NautilusZoomableProxyClass;

struct _NautilusZoomableProxy {
        BonoboZoomable		zoomable;

	NautilusZoomableProxyPrivate *priv;
};

struct _NautilusZoomableProxyClass {
	BonoboZoomableClass	parent;
};

GtkType		 nautilus_zoomable_proxy_get_type	(void);

BonoboObject	*nautilus_zoomable_proxy_get		(Bonobo_Zoomable corba_zoomable);

#endif /* NAUTILUS_ZOOMABLE_PROXY_H */
