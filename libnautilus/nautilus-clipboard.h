/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* fm-directory-view.h
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Rebecca Schulman <rebecka@eazel.com>
 */


#ifndef NAUTILUS_CLIPBOARD_INFO_H
#define NAUTILUS_CLIPBOARD_INFO_H



#include <gtk/gtkscrolledwindow.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-view-frame.h> 



typedef struct NautilusClipboardInfo NautilusClipboardInfo;
typedef struct NautilusClipboardInfoClass NautilusClipboardInfoClass;


#define NAUTILUS_TYPE_CLIPBOARD_INFO	(nautilus_clipboard_info_get_type ())
#define NAUTILUS_CLIPBOARD_INFO(obj)	(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_CLIPBOARD_INFO, NautilusClipboardInfo))
#define NAUTILUS_CLIPBOARD_INFO_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_CLIPBOARD_INFO, NautilusClipboardInfoClass))
#define NAUTILUS_IS_CLIPBOARD_INFO(obj)	(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_CLIPBOARD_INFO))
#define NAUTILUS_IS_CLIPBOARD_INFO_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_CLIPBOARD_INFO))


typedef struct NautilusClipboardDetails NautilusClipboardDetails;



struct NautilusClipboardInfo {
	GtkScrolledWindow parent;
	NautilusClipboardDetails *details;
};

struct NautilusClipboardInfoClass {
	GtkScrolledWindowClass parent_class;
	void (* destroy) (NautilusClipboardInfo *info);
};

/* GtkObject support */
GtkType                nautilus_clipboard_info_get_type            (void);
void                   nautilus_clipboard_info_initialize          (NautilusClipboardInfo *info);
void                   nautilus_clipboard_info_destroy             (NautilusClipboardInfo *info);		
void                   nautilus_clipboard_info_destroy_cb          (GtkObject             *object,
								    gpointer               user_data);
NautilusClipboardInfo *nautilus_clipboard_info_new                 (void);
void                   nautilus_clipboard_info_set_component_name  (NautilusClipboardInfo *info,
								    char                  *component_name);
char *                 nautilus_clipboard_info_get_component_name  (NautilusClipboardInfo *info);
void                   nautilus_clipboard_info_set_clipboard_owner (NautilusClipboardInfo *info,
								    GtkWidget             *clipboard_owner);
GtkWidget *            nautilus_clipboard_info_get_clipboard_owner (NautilusClipboardInfo *info);
void                   nautilus_clipboard_info_set_view            (NautilusClipboardInfo *info,
								    NautilusView          *view);
NautilusView *         nautilus_clipboard_info_get_view            (NautilusClipboardInfo *info);
void                   nautilus_clipboard_info_free                (NautilusClipboardInfo *info);
void                   nautilus_component_merge_bonobo_items_cb    (GtkWidget             *widget,
								    GdkEventAny           *event,
								    gpointer               user_data);
void                   nautilus_component_unmerge_bonobo_items_cb  (GtkWidget             *widget,
								    GdkEventAny           *event,
								    gpointer               user_data);

#endif
