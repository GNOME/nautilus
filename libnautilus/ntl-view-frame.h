/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 2 -*- */

/*
 *  libnautilus: A library for nautilus clients.
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */
/* ntl-view-client.h: Interface for object that represents a nautilus client. */

#ifndef NTL_VIEW_CLIENT_H
#define NTL_VIEW_CLIENT_H

#include <gtk/gtk.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NAUTILUS_TYPE_VIEW_CLIENT			(nautilus_view_client_get_type ())
#define NAUTILUS_VIEW_CLIENT(obj)			(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_VIEW_CLIENT, NautilusViewClient))
#define NAUTILUS_VIEW_CLIENT_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_VIEW_CLIENT, NautilusViewClientClass))
#define NAUTILUS_IS_VIEW_CLIENT(obj)			(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_VIEW_CLIENT))
#define NAUTILUS_IS_VIEW_CLIENT_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_VIEW_CLIENT))

typedef struct _NautilusViewClient       NautilusViewClient;
typedef struct _NautilusViewClientClass  NautilusViewClientClass;

struct _NautilusViewClientClass
{
  GtkBinClass parent_spot;

  void (*notify_location_change)	(NautilusViewClient *view,
					 Nautilus_NavigationInfo *nav_context);
  void (*notify_selection_change)	(NautilusViewClient *view,
					 Nautilus_SelectionInfo *nav_context);
  void (*load_state)                    (NautilusViewClient *view, const char *config_path);
  void (*save_state)                    (NautilusViewClient *view, const char *config_path);
  void (*show_properties)               (NautilusViewClient *view);
  void (*stop_location_change)          (NautilusViewClient *view);

  GtkBinClass *parent_class;

  gpointer servant_init_func, servant_destroy_func, vepv;

  guint view_client_signals[5];
};

struct _NautilusViewClient
{
  GtkBin parent;

  GtkWidget *main_window;
 
  GnomeObject *control, *view_client;
  Nautilus_ViewFrame view_frame;
};

GtkType nautilus_view_client_get_type                (void);
void    nautilus_view_client_request_location_change (NautilusViewClient         *view,
						      Nautilus_NavigationRequestInfo *loc);
void    nautilus_view_client_request_selection_change (NautilusViewClient        *view,
						       Nautilus_SelectionRequestInfo *loc);
void    nautilus_view_client_request_status_change    (NautilusViewClient        *view,
						       Nautilus_StatusRequestInfo *loc);
void    nautilus_view_client_request_progress_change  (NautilusViewClient        *view,
						       Nautilus_ProgressRequestInfo *loc);
GnomeObject *nautilus_view_client_get_gnome_object    (NautilusViewClient        *view);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
