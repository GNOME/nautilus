/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-sidebar-provider.h: register and create NautilusSidebars
 
   Copyright (C) 2004 Red Hat Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Alexander Larsson <alexl@redhat.com>
*/

#ifndef NAUTILUS_SIDEBAR_PROVIDER_H
#define NAUTILUS_SIDEBAR_PROVIDER_H

#include <libnautilus-private/nautilus-sidebar.h>
#include <libnautilus-private/nautilus-window-info.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SIDEBAR_PROVIDER           (nautilus_sidebar_provider_get_type ())
#define NAUTILUS_SIDEBAR_PROVIDER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_SIDEBAR_PROVIDER, NautilusSidebarProvider))
#define NAUTILUS_IS_SIDEBAR_PROVIDER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_SIDEBAR_PROVIDER))
#define NAUTILUS_SIDEBAR_PROVIDER_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NAUTILUS_TYPE_SIDEBAR_PROVIDER, NautilusSidebarProviderIface))

typedef struct _NautilusSidebarProvider       NautilusSidebarProvider;
typedef struct _NautilusSidebarProviderIface  NautilusSidebarProviderIface;

struct _NautilusSidebarProviderIface {
	GTypeInterface g_iface;

	NautilusSidebar * (*create) (NautilusSidebarProvider *provider,
				     NautilusWindowInfo *window);
};

/* Interface Functions */
GType                   nautilus_sidebar_provider_get_type  (void);
NautilusSidebar *       nautilus_sidebar_provider_create (NautilusSidebarProvider *provider,
							  NautilusWindowInfo  *window);
GList *                 nautilus_list_sidebar_providers (void);

G_END_DECLS

#endif /* NAUTILUS_SIDEBAR_PROVIDER_H */
