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

#include "desktop-item.h"

#include <gdk-pixbuf/gnome-canvas-pixbuf.h>


/* User data is used by the specific DesktopItem types */
static void     desktop_item_set_user_data (DesktopItem    *item,
                                            gpointer        user_data,
                                            GDestroyNotify  destroy_notify_func);
static gpointer desktop_item_get_user_data (DesktopItem    *item);



typedef enum {
        DESKTOP_ITEM_NONE,
        DESKTOP_ITEM_ICON

} DesktopItemType;

typedef void (* DesktopItemRealizeFunc) (DesktopItem *item, GnomeCanvasGroup *group);
typedef void (* DesktopItemUnrealizeFunc) (DesktopItem *item);
typedef void (* DesktopItemSizeRequestFunc) (DesktopItem *item,
                                             gint *x, gint *y,
                                             gint *width, gint *height);
typedef void (* DesktopItemSizeAllocateFunc) (DesktopItem *item,
                                              gint x, gint y, gint width, gint height);


struct _DesktopItem {
        guint refcount;

        DesktopItemType type;
        
        gpointer user_data;

        GDestroyNotify destroy_notify_func;
        
        GnomeCanvasItem *canvas_item;

        DesktopItemRealizeFunc realize_func;
        DesktopItemUnrealizeFunc unrealize_func;
        DesktopItemSizeRequestFunc request_func;
        DesktopItemSizeAllocateFunc allocate_func;
};


DesktopItem*
desktop_item_new (void)
{
        DesktopItem *item;

        item = g_new(DesktopItem, 1);

        item->refcount = 1;

        item->type = DESKTOP_ITEM_NONE;
        
        item->user_data = NULL;
        item->destroy_notify_func = NULL;

        item->canvas_item = NULL;

        item->realize_func = NULL;
        item->unrealize_func = NULL;
        item->request_func = NULL;
        item->allocate_func = NULL;
        
        return item;
}

void
desktop_item_ref (DesktopItem *item)
{
        g_return_if_fail(item != NULL);
        
        item->refcount += 1;
}

void
desktop_item_unref (DesktopItem *item)
{
        g_return_if_fail(item != NULL);
        g_return_if_fail(item->refcount > 0);

        item->refcount -= 1;

        if (item->refcount == 0) {
                if (item->canvas_item)
                        gtk_object_unref(GTK_OBJECT(item->canvas_item));

                if (item->destroy_notify_func != NULL) {
                        (* item->destroy_notify_func) (item->user_data);
                }
                
                g_free(item);
        }
}

void
desktop_item_set_user_data (DesktopItem *item,
                            gpointer user_data,
                            GDestroyNotify destroy_notify_func)
{
        g_return_if_fail(item != NULL);

        if (item->destroy_notify_func != NULL) {
                /* destroy old data */
                (* item->destroy_notify_func) (item->user_data);
        }
        
        item->user_data = user_data;

        item->destroy_notify_func = destroy_notify_func;
}

gpointer
desktop_item_get_user_data (DesktopItem *item)
{
        g_return_val_if_fail(item != NULL, NULL);

        return item->user_data;
}

static void
desktop_item_set_canvas_item (DesktopItem *item,
                              GnomeCanvasItem *canvas_item)
{
        g_return_if_fail(item != NULL);

        if (item->canvas_item == canvas_item)
                return;
        
        if (item->canvas_item)
                gtk_object_unref(GTK_OBJECT(item->canvas_item));

        item->canvas_item = canvas_item;

        if (item->canvas_item != NULL) {
                gtk_object_ref(GTK_OBJECT(item->canvas_item));
        }
}

GnomeCanvasItem*
desktop_item_get_canvas_item (DesktopItem *item)
{
        g_return_val_if_fail(item != NULL, NULL);

        return item->canvas_item;
}

void
desktop_item_realize         (DesktopItem     *item,
                              GnomeCanvasGroup     *group)
{
        g_return_if_fail(item != NULL);
        g_return_if_fail(item->realize_func != NULL);

        (* item->realize_func) (item, group);
}

void
desktop_item_unrealize       (DesktopItem     *item)
{
        g_return_if_fail(item != NULL);
        g_return_if_fail(item->unrealize_func != NULL);

        (* item->unrealize_func) (item);
}

void
desktop_item_size_request    (DesktopItem      *item,
                              gint *x, gint *y,
                              gint *width, gint *height)
{
        g_return_if_fail(item != NULL);
        g_return_if_fail(item->request_func != NULL);
        
        (* item->request_func) (item, x, y, width, height);
}

void
desktop_item_size_allocate   (DesktopItem      *item,
                              gint x, gint y, gint width, gint height)
{
        g_return_if_fail(item != NULL);
        g_return_if_fail(item->allocate_func != NULL);
        
        (* item->allocate_func) (item, x, y, width, height);
}


/*
 * DesktopIcon
 */

typedef struct _DesktopIcon DesktopIcon;

struct _DesktopIcon {
        gchar *name;
        GdkPixbuf *pixbuf;
};

#define IS_ICON(item) ((item)->type == DESKTOP_ITEM_ICON)

static void
icon_realize (DesktopItem *item, GnomeCanvasGroup *group)
{
        GnomeCanvasItem *canvas_item;
        DesktopIcon *icon;
        
        g_return_if_fail(item != NULL);
        g_return_if_fail(IS_ICON(item));

        icon = item->user_data;
        
        canvas_item = gnome_canvas_item_new(group,
                                            gnome_canvas_pixbuf_get_type(),
                                            "x", 0.0, "y", 0.0,
                                            "pixbuf", icon->pixbuf,
                                            "width", icon->pixbuf ?
                                            (double)gdk_pixbuf_get_width(icon->pixbuf) : 10.0,
                                            "height", icon->pixbuf ? 
                                            (double)gdk_pixbuf_get_height(icon->pixbuf) : 10.0,
                                            "x_set", TRUE, "y_set", TRUE,
                                            "width_set", TRUE, "height_set", TRUE,
                                            NULL);

        desktop_item_set_canvas_item(item, canvas_item);
}

static void
icon_unrealize (DesktopItem *item)
{
        DesktopIcon *icon;
        
        g_return_if_fail(item != NULL);
        g_return_if_fail(IS_ICON(item));

        icon = item->user_data;

        gtk_object_destroy(GTK_OBJECT(item->canvas_item));
        
        /* drop last refcount and set to NULL */
        desktop_item_set_canvas_item(item, NULL);
}

static void
icon_size_request    (DesktopItem      *item,
                      gint *x, gint *y,
                      gint *width, gint *height)
{
        DesktopIcon *icon;
        
        g_return_if_fail(item != NULL);
        g_return_if_fail(IS_ICON(item));

        icon = item->user_data;

        if (icon->pixbuf) {
                *width = gdk_pixbuf_get_width(icon->pixbuf);
                *height = gdk_pixbuf_get_height(icon->pixbuf);
        } else {
                *width = 1;
                *height = 1;
        }
}

static void
icon_size_allocate   (DesktopItem      *item,
                      gint x, gint y, gint width, gint height)
{
        DesktopIcon *icon;
        
        g_return_if_fail(item != NULL);
        g_return_if_fail(IS_ICON(item));

        icon = item->user_data;

        if (item->canvas_item) {
                printf("setting allocate\n");
                gnome_canvas_item_set (item->canvas_item,
                                       "x", (double)x,
                                       "y", (double)y,
                                       "width", (double)width,
                                       "height", (double)height,
                                       NULL);
        }
}

static void
icon_destroy (gpointer data)
{
        DesktopIcon *icon;

        icon = data;

        if (icon->name)
                g_free(icon->name);

        if (icon->pixbuf)
                gdk_pixbuf_unref(icon->pixbuf);
        
        g_free(icon);
}

DesktopItem*
desktop_icon_new (void)
{
        DesktopItem *item;
        DesktopIcon *icon;

        item = desktop_item_new();
        
        icon = g_new(DesktopIcon, 1);
        
        item->type = DESKTOP_ITEM_ICON;

        icon->name = NULL;
        icon->pixbuf = NULL;

        desktop_item_set_user_data(item, icon, icon_destroy);

        item->realize_func = icon_realize;
        item->unrealize_func = icon_unrealize;
        item->request_func = icon_size_request;
        item->allocate_func = icon_size_allocate;
        
        return item;
}

void
desktop_icon_set_icon (DesktopItem *item, GdkPixbuf *pixbuf)
{
        DesktopIcon *icon;
        
        g_return_if_fail(item != NULL);
        g_return_if_fail(IS_ICON(item));

        icon = item->user_data;
        
        if (icon->pixbuf)
                gdk_pixbuf_unref(icon->pixbuf);

        icon->pixbuf = pixbuf;

        if (icon->pixbuf)
                gdk_pixbuf_ref(icon->pixbuf);
}

void
desktop_icon_set_name        (DesktopItem     *item,
                              const gchar     *filename)
{
        DesktopIcon *icon;
        
        g_return_if_fail(item != NULL);
        g_return_if_fail(IS_ICON(item));

        icon = item->user_data;

        if (icon->name)
                g_free(icon->name);

        icon->name = filename ? g_strdup(filename) : NULL;
}




