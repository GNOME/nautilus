/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*
 *  Nautilus
 *
 *  Copyright (C) 1999, 2000 Red Hat, Inc.
 *  Copyright (C) 1999, 2000 Eazel, Inc.
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
#include "ntl-index-panel.h"

#define NAUTILUS_TYPE_WINDOW (nautilus_window_get_type())
#define NAUTILUS_WINDOW(obj)	        (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_WINDOW, NautilusWindow))
#define NAUTILUS_WINDOW_CLASS(klass)      (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_WINDOW, NautilusWindowClass))
#define NAUTILUS_IS_WINDOW(obj)	        (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_WINDOW))
#define NAUTILUS_IS_WINDOW_CLASS(klass)   (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_WINDOW))

typedef struct _NautilusWindow NautilusWindow;

typedef struct {
  GnomeAppClass parent_spot;

  GnomeAppClass *parent_class;

  guint window_signals[0];
} NautilusWindowClass;

typedef struct _NautilusWindowStateInfo NautilusWindowStateInfo;

struct _NautilusWindow {
  GnomeApp parent_object;

  /** UI stuff **/
  NautilusIndexPanel *index_panel;
  GtkWidget *content_hbox, *btn_back, *btn_fwd;
  GtkWidget *option_cvtype, *ent_uri;

  guint statusbar_ctx, statusbar_clear_id;

  /** CORBA-related elements **/
  GnomeObject *ntl_viewwindow;
  GnomeUIHandler *uih;

  /** State information **/

  /* Information about current location/selection */
  Nautilus_NavigationInfo *ni;
  Nautilus_SelectionInfo *si;
  /* History stuff */
  GSList *uris_prev, *uris_next;

  /* Current views stuff */
  NautilusView *content_view;
  GSList *meta_views;

  /* Pending changes */
  NautilusNavigationInfo *pending_ni;
  NautilusView *new_content_view, *new_requesting_view;
  GSList *new_meta_views, *error_views;

  enum { NW_LOADING_INFO, NW_LOADING_VIEWS, NW_IDLE } state;

  guint cancel_tag;
  guint action_tag;
  guint16 made_changes, making_changes;

  gboolean changes_pending : 1;
  gboolean is_back : 1;
  gboolean views_shown : 1;
  gboolean view_bombed_out : 1;
  gboolean view_activation_complete : 1;
  gboolean cv_progress_initial : 1;
  gboolean cv_progress_done : 1;
  gboolean cv_progress_error : 1;
  gboolean reset_to_idle : 1;
};

GtkType nautilus_window_get_type(void);
GtkWidget *nautilus_window_new(const char *app_id);
void nautilus_window_set_content_view(NautilusWindow *window, NautilusView *content_view);
void nautilus_window_add_meta_view(NautilusWindow *window, NautilusView *meta_view);
void nautilus_window_remove_meta_view(NautilusWindow *window, NautilusView *meta_view);
void nautilus_window_goto_uri(NautilusWindow *window, const char *uri);
void nautilus_window_display_error(NautilusWindow *window, const char *error_msg);

const char *nautilus_window_get_requested_uri(NautilusWindow *window);
GnomeUIHandler *nautilus_window_get_uih(NautilusWindow *window);

void nautilus_window_allow_back (NautilusWindow *window, gboolean allow);
void nautilus_window_allow_forward (NautilusWindow *window, gboolean allow);
void nautilus_window_allow_up (NautilusWindow *window, gboolean allow);
void nautilus_window_allow_reload (NautilusWindow *window, gboolean allow);
void nautilus_window_allow_stop (NautilusWindow *window, gboolean allow);

#endif
