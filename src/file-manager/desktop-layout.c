/*  -*- Mode: C; c-set-style: linux; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 * Desktop component of GNOME file manager
 * 
 * Copyright (C) 1999 Red Hat Inc., Free Software Foundation
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

#include "desktop-layout.h"

struct _DesktopLayoutItem {
        guint refcount;
        
        /* Store the allocation here */
        gint x;
        gint y;
        gint width;
        gint height;

        /* Callbacks */
        DesktopLayoutSizeRequestFunc request_func;
        DesktopLayoutSizeAllocateFunc allocate_func;

        /* data */
        GDestroyNotify destroy_notify_func;
        gpointer user_data;
};

static void
item_size_request (DesktopLayoutItem* item,
                   gint *x,
                   gint *y,
                   gint *width,
                   gint *height)
{
        g_return_if_fail(item != NULL);
        g_return_if_fail(item->request_func != NULL);

        /* set these so user functions can be lazy and only set w/h */
        *x = -1;
        *y = -1;

        /* set these in order to check up on user functions */

        *width = 0;
        *height = 0;
        
        (* item->request_func) (item, x, y, width, height, item->user_data);

        /* check up */
        g_return_if_fail(*width > 0);
        g_return_if_fail(*height > 0);
        /* can't set one and not the other */
        g_return_if_fail((*x >= 0 && *y >= 0) || (*x < 0 && *y < 0));
}

static void
item_size_allocate (DesktopLayoutItem* item,
                    gint x, gint y, gint width, gint height)
{
        item->x = x;
        item->y = y;
        item->width = width;
        item->height = height;

        /* the allocate func is optional since users can request
           the allocation after-the-fact */
        if (item->allocate_func)
                (* item->allocate_func) (item, x, y, width, height, item->user_data);
}

DesktopLayoutItem*
desktop_layout_item_new (DesktopLayoutSizeRequestFunc   request_func,
                         DesktopLayoutSizeAllocateFunc  allocate_func,
                         GDestroyNotify                 destroy_notify_func,
                         gpointer user_data)
{
        DesktopLayoutItem *item;
        
        g_return_val_if_fail(request_func != NULL, NULL);
        

        item = g_new(DesktopLayoutItem, 1);

        item->refcount = 1;
        
        item->x = item->y = 0;
        item->width = item->height = 1;

        item->request_func = request_func;
        item->allocate_func = allocate_func;

        item->destroy_notify_func = destroy_notify_func;
        
        item->user_data = user_data;

        return item;
}

void
desktop_layout_item_ref (DesktopLayoutItem *item)
{
        g_return_if_fail(item != NULL);

        item->refcount += 1;
}

void
desktop_layout_item_unref (DesktopLayoutItem *item)
{
        g_return_if_fail(item != NULL);
        g_return_if_fail(item->refcount > 0);

        item->refcount -= 1;

        if (item->refcount == 0) {
                if (item->destroy_notify_func) {
                        (* item->destroy_notify_func) (item->user_data);
                }
                
                g_free(item);
        }
}

gpointer
desktop_item_get_user_data (DesktopLayoutItem *item)
{
        return item->user_data;
}

void
desktop_layout_item_get_allocation (DesktopLayoutItem *item,
                                    gint *x,
                                    gint *y,
                                    gint *w,
                                    gint *h)
{
        g_return_if_fail(item != NULL);
        
        if (x)
                *x = item->x;
        if (y)
                *y = item->y;

        if (w)
                *w = item->width;

        if (h)
                *h = item->height;
}

/*
 * DesktopLayout
 */

struct _DesktopLayout {
        guint refcount;
        
        GList* items;

        gboolean rows_not_columns;
        DesktopHLayoutMode hmode;
        DesktopVLayoutMode vmode;

        gint hpadding;
        gint vpadding;
        
        /* Our rectangle to put the items in */
        gint x;
        gint y;
        gint width;
        gint height;

};

DesktopLayout*
desktop_layout_new (void)
{
        DesktopLayout *layout;

        layout = g_new(DesktopLayout, 1);

        layout->refcount = 1;
        layout->items = NULL;

        layout->x = layout->y = 0;

        layout->width = layout->height = 1;

        layout->rows_not_columns = FALSE;
        layout->hmode = DesktopLayoutRightToLeft;
        layout->vmode = DesktopLayoutTopToBottom;

        layout->hpadding = 8;
        layout->vpadding = 8;

	return layout;
}

void
desktop_layout_ref (DesktopLayout *layout)
{
        g_return_if_fail(layout != NULL);
        
        layout->refcount += 1;
}

void
desktop_layout_unref (DesktopLayout *layout)
{
        g_return_if_fail(layout != NULL);
        g_return_if_fail(layout->refcount > 0);
        
        layout->refcount -= 1;

        if (layout->refcount == 0) {
                GList *iter;

                iter = layout->items;
                while (iter != NULL) {
                        desktop_layout_item_unref(iter->data);
                        iter = g_list_next(iter);
                }

                g_list_free(layout->items);
                
                g_free(layout);
        }
}

/* FIXME changing any of this stuff requires a layout recomputation */

void
desktop_layout_set_size (DesktopLayout *layout,
                         gint x,
                         gint y,
                         gint width,
                         gint height)
{
        layout->x = x;
        layout->y = y;
        layout->width = width;
        layout->height = height;
}

void
desktop_layout_set_mode (DesktopLayout *layout,
                         gboolean rows_not_columns,
                         DesktopHLayoutMode hmode,
                         DesktopVLayoutMode vmode)
{
        layout->rows_not_columns = rows_not_columns;
        layout->hmode = hmode;
        layout->vmode = vmode;
}

void
desktop_layout_add_item (DesktopLayout *layout,
                         DesktopLayoutItem *item)
{
        desktop_layout_item_ref(item);
        layout->items = g_list_prepend(layout->items, item);
}

void
desktop_layout_remove_item (DesktopLayout *layout,
                            DesktopLayoutItem *item)
{
        layout->items = g_list_remove(layout->items, item);
        desktop_layout_item_unref(item);
}


static void
layout_top_to_bottom_right_to_left(DesktopLayout *layout,
                                   gboolean ignore_position_requests)
{
        

}

static void
layout_top_to_bottom_left_to_right(DesktopLayout *layout,
                                   gboolean ignore_position_requests)
{
        

}                     

static void
layout_rows_left_to_right(DesktopLayout *layout,
                          gboolean ignore_position_requests)
{
        gint next_x = layout->x + layout->hpadding;
        gint next_y = layout->y + layout->vpadding;
        gint right_edge = layout->x + layout->width - layout->hpadding;
        gint bottom_edge = layout->y + layout->height - layout->vpadding;
        gint row_height = 0;
        GList *iter;

        iter = layout->items;

        while (iter != NULL) {
                DesktopLayoutItem *item = iter->data;
                gint req_x, req_y, req_w, req_h;
                
                item_size_request(item, &req_x, &req_y, &req_w, &req_h);

                if (!ignore_position_requests && req_x >= 0) {
                        g_assert(req_y >= 0); /* req_x and req_y must be on the same side of 0 */
                        
                        /* Give it its request - eventually we may take w/h into account though,
                           and flow the other icons around this one? */
                        item_size_allocate(item, req_x, req_y, req_w, req_h);

                        iter = g_list_next(iter);
                        continue;
                } else {
                        /* We're either ignoring req_x or the item did not specify
                           a req_x */
                        g_assert(ignore_position_requests || req_x < 0);
                        
                        item_size_allocate(item, next_x, next_y, req_w, req_h);

                        next_x += req_w + layout->hpadding;

                        row_height = MAX(row_height, req_h);
                        
                        /* It is important not to check for reset until we place the item;
                           if there's no room for even a single item, we need to go ahead and
                           place one per row anyway */
                        if (next_x > right_edge) {
                                /* Reset */
                                next_x = layout->x + layout->hpadding;
                                next_y += row_height + layout->vpadding;
                                row_height = 0;
                        }

                        if (next_y > bottom_edge) {
                                /* return to the top */
                                next_y = layout->y + layout->vpadding;
                        }
                        
                        iter = g_list_next(iter);
                }
        }
}

void
desktop_layout_arrange (DesktopLayout *layout, gboolean ignore_position_requests)
{
        /* FIXME */
}

