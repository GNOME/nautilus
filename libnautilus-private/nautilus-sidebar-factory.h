/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-sidebar-factory.h: register and create NautilusSidebars
 
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

#ifndef NAUTILUS_SIDEBAR_FACTORY_H
#define NAUTILUS_SIDEBAR_FACTORY_H

#include <libnautilus-private/nautilus-sidebar.h>
#include <libnautilus-private/nautilus-window-info.h>

G_BEGIN_DECLS

typedef struct _NautilusSidebarInfo NautilusSidebarInfo;

struct _NautilusSidebarInfo {
	char *id;
	NautilusSidebar * (*create) (NautilusWindowInfo *window);
};


void                       nautilus_sidebar_factory_register           (NautilusSidebarInfo *sidebar_info);
const NautilusSidebarInfo *nautilus_sidebar_factory_lookup             (char                *id);
NautilusSidebar *          nautilus_sidebar_factory_create             (char                *id,
									NautilusWindowInfo  *window);
GList *                    nautilus_sidebar_factory_enumerate_sidebars (void);

G_END_DECLS

#endif /* NAUTILUS_SIDEBAR_FACTORY_H */
