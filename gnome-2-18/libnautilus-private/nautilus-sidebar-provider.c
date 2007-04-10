/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-sidebar-provider.c: register and create NautilusSidebars
 
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

#include <string.h>
#include "nautilus-sidebar-provider.h"

static void
nautilus_sidebar_provider_base_init (gpointer g_class)
{
}

GType                   
nautilus_sidebar_provider_get_type (void)
{
	static GType type = 0;

	if (!type) {
		const GTypeInfo info = {
			sizeof (NautilusSidebarProviderIface),
			nautilus_sidebar_provider_base_init,
			NULL,
			NULL,
			NULL,
			NULL,
			0,
			0,
			NULL
		};
		
		type = g_type_register_static (G_TYPE_INTERFACE, 
					       "NautilusSidebarProvider",
					       &info, 0);
		g_type_interface_add_prerequisite (type, G_TYPE_OBJECT);
	}

	return type;
}

NautilusSidebar *
nautilus_sidebar_provider_create (NautilusSidebarProvider *provider,
				  NautilusWindowInfo  *window)
{
	return (* NAUTILUS_SIDEBAR_PROVIDER_GET_IFACE (provider)->create) (provider, window);
}
					       

GList *
nautilus_list_sidebar_providers (void)
{
	return NULL;
}
