/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */

/*
 *  libnautilus: A library for nautilus clients.
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
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
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */
/* ntl-meta-view-client.h: Interface for object that represents a nautilus meta view implementation. */

#ifndef NTL_META_VIEW_CLIENT_H
#define NTL_META_VIEW_CLIENT_H

#include <libnautilus/ntl-view-client.h>

#define NAUTILUS_TYPE_META_VIEW_CLIENT			(nautilus_meta_view_client_get_type ())
#define NAUTILUS_META_VIEW_CLIENT(obj)			(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_META_VIEW_CLIENT, NautilusMetaViewClient))
#define NAUTILUS_META_VIEW_CLIENT_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_META_VIEW_CLIENT, NautilusMetaViewClientClass))
#define NAUTILUS_IS_META_VIEW_CLIENT(obj)			(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_META_VIEW_CLIENT))
#define NAUTILUS_IS_META_VIEW_CLIENT_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_META_VIEW_CLIENT))

typedef struct _NautilusMetaViewClient NautilusMetaViewClient;
typedef struct _NautilusMetaViewClientClass NautilusMetaViewClientClass;

struct _NautilusMetaViewClientClass {
  NautilusViewClientClass parent_spot;
};

struct _NautilusMetaViewClient {
  NautilusViewClient parent;
};

GtkType nautilus_meta_view_client_get_type (void);
void nautilus_meta_view_client_set_label(NautilusMetaViewClient *mvc,
					 const char *label);

#endif
