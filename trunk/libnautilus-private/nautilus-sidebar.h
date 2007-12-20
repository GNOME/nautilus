/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*-

   nautilus-sidebar.h: Interface for nautilus sidebar plugins
 
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

#ifndef NAUTILUS_SIDEBAR_H
#define NAUTILUS_SIDEBAR_H

#include <glib-object.h>
#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_SIDEBAR           (nautilus_sidebar_get_type ())
#define NAUTILUS_SIDEBAR(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_SIDEBAR, NautilusSidebar))
#define NAUTILUS_IS_SIDEBAR(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_SIDEBAR))
#define NAUTILUS_SIDEBAR_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), NAUTILUS_TYPE_SIDEBAR, NautilusSidebarIface))

typedef struct _NautilusSidebar NautilusSidebar; /* dummy typedef */
typedef struct _NautilusSidebarIface NautilusSidebarIface;

/* Must also be a GtkWidget */
struct _NautilusSidebarIface 
{
	GTypeInterface g_iface;

	/* Signals: */
	void           (* tab_icon_changed)       (NautilusSidebar *sidebar);
	
	/* VTable: */
	const char *   (* get_sidebar_id)         (NautilusSidebar *sidebar);
	char *         (* get_tab_label)          (NautilusSidebar *sidebar);
	char *         (* get_tab_tooltip)        (NautilusSidebar *sidebar);
        GdkPixbuf *    (* get_tab_icon)           (NautilusSidebar *sidebar);
        void           (* is_visible_changed)     (NautilusSidebar *sidebar,
						   gboolean         is_visible);

	
	/* Padding for future expansion */
	void (*_reserved1) (void);
	void (*_reserved2) (void);
	void (*_reserved3) (void);
	void (*_reserved4) (void);
	void (*_reserved5) (void);
	void (*_reserved6) (void);
	void (*_reserved7) (void);
	void (*_reserved8) (void);
};

GType             nautilus_sidebar_get_type             (void);

const char *nautilus_sidebar_get_sidebar_id     (NautilusSidebar *sidebar);
char *      nautilus_sidebar_get_tab_label      (NautilusSidebar *sidebar);
char *      nautilus_sidebar_get_tab_tooltip    (NautilusSidebar *sidebar);
GdkPixbuf * nautilus_sidebar_get_tab_icon       (NautilusSidebar *sidebar);
void        nautilus_sidebar_is_visible_changed (NautilusSidebar *sidebar,
						 gboolean         is_visible);

G_END_DECLS

#endif /* NAUTILUS_VIEW_H */
