
/* 
   nautilus-trash-monitor.h: Nautilus trash state watcher.
 
   Copyright (C) 2000 Eazel, Inc.
  
   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
  
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
  
   You should have received a copy of the GNU General Public
   License along with this program; if not, see <http://www.gnu.org/licenses/>.
  
   Author: Pavel Cisler <pavel@eazel.com>
*/

#pragma once

#include <gio/gio.h>

#define NAUTILUS_TYPE_TRASH_MONITOR (nautilus_trash_monitor_get_type ())

G_DECLARE_FINAL_TYPE (NautilusTrashMonitor, nautilus_trash_monitor,
                      NAUTILUS, TRASH_MONITOR,
                      GObject)

NautilusTrashMonitor   *nautilus_trash_monitor_get      (void);
gboolean                nautilus_trash_monitor_is_empty (void);
GIcon                  *nautilus_trash_monitor_get_symbolic_icon (void);
void                    nautilus_trash_monitor_clear (void);
