/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-sidebar-factory.c: register and create NautilusSidebars
 
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
#include "nautilus-sidebar-factory.h"

static GList *registered_sidebars;

void
nautilus_sidebar_factory_register (NautilusSidebarInfo *sidebar_info)
{
	g_return_if_fail (sidebar_info != NULL);
	g_return_if_fail (sidebar_info->id != NULL);
	g_return_if_fail (nautilus_sidebar_factory_lookup (sidebar_info->id) == NULL);
	
	registered_sidebars = g_list_append (registered_sidebars, sidebar_info);
  
}

const NautilusSidebarInfo *
nautilus_sidebar_factory_lookup (char *id)
{
	GList *l;
	NautilusSidebarInfo *sidebar_info;

	g_return_val_if_fail (id != NULL, NULL);
	
	for (l = registered_sidebars; l != NULL; l = l->next) {
		sidebar_info = l->data;
		
		if (strcmp (sidebar_info->id, id) == 0) {
			return sidebar_info;
		}
	}
	return NULL;
  
}

NautilusSidebar *
nautilus_sidebar_factory_create (char *id,
				 NautilusWindowInfo *window)
{
	const NautilusSidebarInfo *sidebar_info;

	sidebar_info = nautilus_sidebar_factory_lookup (id);
	if (sidebar_info == NULL) {
		return NULL;
	}

	return sidebar_info->create (window);
}

GList *
nautilus_sidebar_factory_enumerate_sidebars (void)
{
	GList *l, *res;
	const NautilusSidebarInfo *sidebar_info;

	res = NULL;
	
	for (l = registered_sidebars; l != NULL; l = l->next) {
		sidebar_info = l->data;

		res = g_list_prepend (res, g_strdup (sidebar_info->id));
	}
	
	return g_list_reverse (res);
}


