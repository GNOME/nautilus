/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gtk-container.h - Functions to simplify the implementations of 
  			 GtkContainer widgets.

   Copyright (C) 2001 Ramiro Estrugo.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef EEL_GTK_CONTAINER_H
#define EEL_GTK_CONTAINER_H

#include <gtk/gtk.h>
#include <eel/eel-art-extensions.h>

void eel_gtk_container_child_expose_event (GtkContainer   *container,
					   GtkWidget      *child,
					   GdkEventExpose *event);
void eel_gtk_container_child_map          (GtkContainer   *container,
					   GtkWidget      *child);
void eel_gtk_container_child_unmap        (GtkContainer   *container,
					   GtkWidget      *child);
void eel_gtk_container_child_add          (GtkContainer   *container,
					   GtkWidget      *child);
void eel_gtk_container_child_remove       (GtkContainer   *container,
					   GtkWidget      *child);
void eel_gtk_container_child_size_allocate (GtkContainer *container,
					    GtkWidget *child,
					    EelIRect child_geometry);

#endif /* EEL_GTK_CONTAINER_H */
