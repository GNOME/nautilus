/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999 Red Hat, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this library; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Author: Elliot Lee <sopwith@redhat.com>
 *
 */
/* ntl-window.h: Interface of the main window object */

#ifndef NTL_WINDOW_H
#define NTL_WINDOW_H 1

#include <libgnomeui/gnome-app.h>
#include "ntl-types.h"
#include "ntl-view.h"

#define NAUTILUS_TYPE_WINDOW (nautilus_window_get_type())
#define NAUTILUS_WINDOW(obj)	        (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_WINDOW, NautilusWindow))
#define NAUTILUS_WINDOW_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_WINDOW, NautilusWindowClass))
#define NAUTILUS_IS_WINDOW(obj)	        (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_WINDOW))
#define NAUTILUS_IS_WINDOW_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_WINDOW))

typedef struct _NautilusWindow NautilusWindow;

typedef struct {
  GnomeAppClass parent_spot;

  GnomeAppClass *parent_class;

  void (* request_location_change)(NautilusWindow *window,
				   Nautilus_NavigationRequestInfo *loc,
				   GtkWidget *requesting_view);
  void (* request_selection_change)(NautilusWindow *window,
				    Nautilus_SelectionRequestInfo *loc,
				    GtkWidget *requesting_view);
  void (* request_status_change)   (NautilusWindow *window,
                                    Nautilus_StatusRequestInfo *loc,
                                    GtkWidget *requesting_view);

  guint window_signals[3];
} NautilusWindowClass;

struct _NautilusWindow {
  GnomeApp parent_object;

  GtkWidget *content_view;
  GSList *meta_views;

  GtkWidget *meta_notebook, *content_hbox, *btn_back, *btn_fwd;
  GtkWidget *option_cvtype, *menu_cvtype, *ent_url;

  NautilusLocationReference current_uri, actual_current_uri;
  char *current_content_type;

  GSList *uris_prev, *uris_next;

  guint statusbar_ctx, statusbar_clear_id;
};

GtkType nautilus_window_get_type(void);
GtkWidget *nautilus_window_new(const char *app_id);
void nautilus_window_request_location_change(NautilusWindow *window,
					     Nautilus_NavigationRequestInfo *loc,
					     GtkWidget *requesting_view);
void nautilus_window_request_selection_change(NautilusWindow *window,
					      Nautilus_SelectionRequestInfo *loc,
					      GtkWidget *requesting_view);
void nautilus_window_request_status_change(NautilusWindow *window,
                                           Nautilus_StatusRequestInfo *loc,
                                           GtkWidget *requesting_view);
void nautilus_window_save_state(NautilusWindow *window, const char *config_path);
void nautilus_window_load_state(NautilusWindow *window, const char *config_path);
void nautilus_window_set_initial_state(NautilusWindow *window);
void nautilus_window_set_content_view(NautilusWindow *window, NautilusView *content_view);
void nautilus_window_add_meta_view(NautilusWindow *window, NautilusView *meta_view);
void nautilus_window_remove_meta_view(NautilusWindow *window, NautilusView *meta_view);

#endif
