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
/* ntl-content-view-client.h: Interface for object that represents a nautilus content view implementation. */

#ifndef NTL_CONTENT_VIEW_CLIENT_H
#define NTL_CONTENT_VIEW_CLIENT_H

#include <libnautilus/ntl-view-client.h>

#define NAUTILUS_TYPE_CONTENT_VIEW_CLIENT			(nautilus_content_view_client_get_type ())
#define NAUTILUS_CONTENT_VIEW_CLIENT(obj)			(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_CONTENT_VIEW_CLIENT, NautilusContentViewClient))
#define NAUTILUS_CONTENT_VIEW_CLIENT_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CONTENT_VIEW_CLIENT, NautilusContentViewClientClass))
#define NAUTILUS_IS_CONTENT_VIEW_CLIENT(obj)			(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_CONTENT_VIEW_CLIENT))
#define NAUTILUS_IS_CONTENT_VIEW_CLIENT_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_CONTENT_VIEW_CLIENT))

typedef struct _NautilusContentViewClient NautilusContentViewClient;
typedef struct _NautilusContentViewClientClass NautilusContentViewClientClass;

struct _NautilusContentViewClientClass {
  NautilusViewClientClass parent_spot;
};

struct _NautilusContentViewClient {
  NautilusViewClient parent;
};

GtkType nautilus_content_view_client_get_type (void);

#endif
