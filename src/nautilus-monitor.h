/*
   nautilus-monitor.h: file and directory change monitoring for nautilus
 
   Copyright (C) 2000, 2001 Eazel, Inc.
   Copyright (C) 2016 Red Hat, Inc.
  
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
  
   Authors: Seth Nickell <seth@eazel.com>
            Darin Adler <darin@bentspoon.com>
            Carlos Soriano <csoriano@gnome.org>
*/

#pragma once

#include <glib.h>
#include <gio/gio.h>

typedef struct NautilusMonitor NautilusMonitor;

NautilusMonitor *nautilus_monitor_directory (GFile *location);
void             nautilus_monitor_cancel    (NautilusMonitor *monitor);
