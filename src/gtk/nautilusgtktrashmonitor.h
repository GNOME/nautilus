/* GTK - The GIMP Toolkit
 * nautilusgtktrashmonitor.h: Monitor the trash:/// folder to see if there is trash or not
 * Copyright (C) 2011 Suse
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Federico Mena Quintero <federico@gnome.org>
 */

#ifndef __NAUTILUS_GTK_TRASH_MONITOR_H__
#define __NAUTILUS_GTK_TRASH_MONITOR_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define NAUTILUS_TYPE_GTK_TRASH_MONITOR			(_nautilus_gtk_trash_monitor_get_type ())
#define NAUTILUS_GTK_TRASH_MONITOR(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), NAUTILUS_TYPE_GTK_TRASH_MONITOR, NautilusGtkTrashMonitor))
#define NAUTILUS_GTK_TRASH_MONITOR_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_GTK_TRASH_MONITOR, NautilusGtkTrashMonitorClass))
#define NAUTILUS_GTK_IS_TRASH_MONITOR(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NAUTILUS_TYPE_GTK_TRASH_MONITOR))
#define NAUTILUS_GTK_IS_TRASH_MONITOR_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_GTK_TRASH_MONITOR))
#define NAUTILUS_GTK_TRASH_MONITOR_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), NAUTILUS_TYPE_GTK_TRASH_MONITOR, NautilusGtkTrashMonitorClass))

typedef struct _NautilusGtkTrashMonitor NautilusGtkTrashMonitor;
typedef struct _NautilusGtkTrashMonitorClass NautilusGtkTrashMonitorClass;

GType _nautilus_gtk_trash_monitor_get_type (void);
NautilusGtkTrashMonitor *_nautilus_gtk_trash_monitor_get (void);

GIcon *_nautilus_gtk_trash_monitor_get_icon (NautilusGtkTrashMonitor *monitor);

gboolean _nautilus_gtk_trash_monitor_get_has_trash (NautilusGtkTrashMonitor *monitor);

G_END_DECLS

#endif /* __NAUTILUS_GTK_TRASH_MONITOR_H__ */
