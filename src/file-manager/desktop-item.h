/*  -*- Mode: C; c-set-style: linux; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * Desktop component of GNOME file manager
 * 
 * Copyright (C) 1999, 2000 Red Hat Inc., Free Software Foundation
 * (based on Midnight Commander code by Federico Mena Quintero and Miguel de Icaza)
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as 
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#ifndef GD_DESKTOP_ITEM_H
#define GD_DESKTOP_ITEM_H

#include <libgnome/gnome-defs.h>
#include <glib.h>
#include <libgnomeui/gnome-canvas.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

BEGIN_GNOME_DECLS

/* This is the interface used by DesktopCanvas to manage objects on the desktop */

typedef struct _DesktopItem DesktopItem;

DesktopItem*             desktop_item_new             (void);
void                     desktop_item_ref             (DesktopItem      *item);
void                     desktop_item_unref           (DesktopItem      *item);
void                     desktop_item_realize         (DesktopItem      *item,
                                                       GnomeCanvasGroup *group);
void                     desktop_item_unrealize       (DesktopItem      *item);
GnomeCanvasItem         *desktop_item_get_canvas_item (DesktopItem      *item);

void                     desktop_item_size_request    (DesktopItem      *item,
                                                       gint *x, gint *y,
                                                       gint *width, gint *height);
void                     desktop_item_size_allocate   (DesktopItem      *item,
                                                       gint x, gint y, gint width, gint height);

/* This is a specific DesktopItem type */
DesktopItem*             desktop_icon_new             (void);
void                     desktop_icon_set_icon        (DesktopItem      *item,
                                                       GdkPixbuf        *pixbuf);
void                     desktop_icon_set_name        (DesktopItem      *item,
                                                       const gchar      *filename);



                                                       

END_GNOME_DECLS

#endif


