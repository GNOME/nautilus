/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */

/*
   nautilus-directory-background.h: Helper for the background of a widget
                                    that is viewing a particular directory.
 
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
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
  
   Author: Darin Adler <darin@bentspoon.com>
*/

#include <eel/eel-background.h>
#include <libnautilus-private/nautilus-file.h>
#include <libnautilus-private/nautilus-icon-container.h>

void     nautilus_connect_background_to_file_metadata         (GtkWidget             *widget,
                                                               NautilusFile          *file,
                                                               GdkDragAction          default_drag_action);
void     nautilus_connect_desktop_background_to_file_metadata (NautilusIconContainer *icon_container,
                                                               NautilusFile          *file);
gboolean nautilus_file_background_is_set                      (EelBackground         *background);
