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

#ifndef GD_DESKTOP_LAYOUT_H
#define GD_DESKTOP_LAYOUT_H

#include <libgnome/gnome-defs.h>
#include <glib.h>

BEGIN_GNOME_DECLS

/*
  A DesktopLayoutItem represents an object to be layed out.
  The Item has callbacks for size request and allocate; it works
  just like GtkWidget, pretty much, except that the layout item
  is separate from whatever actual thing is being displayed.
*/

typedef struct _DesktopLayoutItem DesktopLayoutItem;

/*
  A DesktopLayout is a container for DesktopLayoutItem that calculates
  the layout of the items.
*/

typedef struct _DesktopLayout DesktopLayout;

/* Unlike gtk_widget_size_request() we can also request a position, if the user
   has manually positioned the object. If the object should be auto-arranged,
   then we pass -1 for x and y (user functions don't need to set -1, it is
   automatically set as a default)
*/
typedef void (* DesktopLayoutSizeRequestFunc) (DesktopLayoutItem *item,
                                               gint *x, gint *y, gint *width, gint *height,
                                               gpointer user_data);
typedef void (* DesktopLayoutSizeAllocateFunc) (DesktopLayoutItem *item,
                                                gint x, gint y, gint width, gint height,
                                                gpointer user_data);

typedef enum {
        DesktopLayoutRightToLeft,
        DesktopLayoutLeftToRight
} DesktopHLayoutMode;

typedef enum {
        DesktopLayoutBottomToTop,
        DesktopLayoutTopToBottom
} DesktopVLayoutMode;

/* allocate func may be NULL but request func is required. destroy notify can be NULL */
DesktopLayoutItem *desktop_layout_item_new            (DesktopLayoutSizeRequestFunc   request_func,
                                                       DesktopLayoutSizeAllocateFunc  allocate_func,
                                                       GDestroyNotify                 destroy_notify_func,
                                                       gpointer user_data);
void               desktop_layout_item_ref            (DesktopLayoutItem             *item);
void               desktop_layout_item_unref          (DesktopLayoutItem             *item);
gpointer           desktop_layout_item_get_user_data  (DesktopLayoutItem             *item);


/* get the last allocation we were given; pass NULL for any dimensions
   you don't care about */
void               desktop_layout_item_get_allocation (DesktopLayoutItem             *item,
                                                       gint                          *x,
                                                       gint                          *y,
                                                       gint                          *w,
                                                       gint                          *h);


DesktopLayout*     desktop_layout_new                 (void);
void               desktop_layout_ref                 (DesktopLayout                 *layout);
void               desktop_layout_unref               (DesktopLayout                 *layout);


/* area within which to arrange the icons */
void               desktop_layout_set_size            (DesktopLayout                 *layout,
                                                       gint                           x,
                                                       gint                           y,
                                                       gint                           width,
                                                       gint                           height);
void               desktop_layout_set_mode            (DesktopLayout                 *layout,
                                                       /* horizontal rows, or columns */
                                                       gboolean                       horizontal,
                                                       DesktopHLayoutMode             hmode,
                                                       DesktopVLayoutMode             vmode);
void               desktop_layout_add_item            (DesktopLayout                 *layout,
                                                       DesktopLayoutItem             *item);
void               desktop_layout_remove_item         (DesktopLayout                 *layout,
                                                       DesktopLayoutItem             *item);


/* Redo the layout, ignoring all x/y positions from size requests
   (i.e.  arrange icons after the user has made a big mess). */
void               desktop_layout_arrange             (DesktopLayout                 *layout,
                                                       gboolean                       ignore_position_requests);


END_GNOME_DECLS

#endif


