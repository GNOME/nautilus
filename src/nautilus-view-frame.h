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
/* ntl-view.h: Interface of the object representing a data
   view. NautilusContentView and NautilusMetaView derive from this
   class. */

#ifndef __NAUTILUS_VIEW_H__
#define __NAUTILUS_VIEW_H__

#include <gtk/gtkwidget.h>
#include <gtk/gtkbin.h>
#include "ntl-types.h"
#include <bonobo.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NAUTILUS_TYPE_VIEW			(nautilus_view_get_type ())
#define NAUTILUS_VIEW(obj)			(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_VIEW, NautilusView))
#define NAUTILUS_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_VIEW, NautilusViewClass))
#define NAUTILUS_IS_VIEW(obj)			(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_VIEW))
#define NAUTILUS_IS_VIEW_CLASS(klass)		(GTK_CHECK_CLASS_TYPE ((obj), NAUTILUS_TYPE_VIEW))

typedef struct _NautilusView       NautilusView;
typedef struct _NautilusViewClass  NautilusViewClass;

struct _NautilusViewClass
{
  GtkBinClass parent_spot;

  /* These signals correspond to the Natuilus:ViewFrame CORBA interface. They
     are requests that the underlying view may make of the framework. */

  void (*request_location_change)	(NautilusView *view,
					 Nautilus_NavigationRequestInfo *navinfo);
  void (*request_selection_change)      (NautilusView *view,
				         Nautilus_SelectionRequestInfo *selinfo);
  void (*request_status_change)         (NautilusView *view,
                                         Nautilus_StatusRequestInfo *loc);
  void (*request_progress_change)       (NautilusView *view,
                                         Nautilus_ProgressRequestInfo *loc);
  void (*notify_zoom_level)             (NautilusView *view,
                                         gdouble       zoom_level);

  /* Not a signal. Work-around for Gtk+'s lack of a 'constructed' operation */
  void (*view_constructed) (NautilusView *view);

  GtkBinClass *parent_class;
  guint num_construct_args;

  gpointer servant_init_func, servant_destroy_func, vepv;
  gpointer zoomable_servant_init_func, zoomable_servant_destroy_func, zoomable_vepv;
};

typedef struct _NautilusViewComponentType NautilusViewComponentType;

struct _NautilusView
{
  GtkBin parent;

  GtkWidget *main_window;

  char *iid;

  BonoboObjectClient *client_object;
  GtkWidget *client_widget;

  BonoboObject *view_frame;
  BonoboObject *zoomable_frame;

  Nautilus_Zoomable zoomable;
  NautilusViewComponentType *component_class;
  gpointer component_data;

  guint construct_arg_count;

  guint timer_id;
  guint checking;
};

GtkType nautilus_view_get_type                  (void);
gboolean nautilus_view_load_client              (NautilusView              *view,
                                                 const char *               iid);
const char *nautilus_view_get_iid               (NautilusView *view);
CORBA_Object nautilus_view_get_client_objref    (NautilusView *view);
BonoboObject *nautilus_view_get_control_frame    (NautilusView *view);
CORBA_Object nautilus_view_get_objref           (NautilusView *view);

/* These functions correspond to methods of the Nautilus:View CORBAinterface */

void nautilus_view_notify_location_change       (NautilusView *view,
					         Nautilus_NavigationInfo *nav_context,
					         const char *initial_title);
void nautilus_view_notify_selection_change      (NautilusView *view,
					         Nautilus_SelectionInfo *sel_context);
void nautilus_view_notify_title_change		(NautilusView *view,
						 const char *new_title);
void nautilus_view_load_state                   (NautilusView *view, 
                                                 const char *config_path);
void nautilus_view_save_state                   (NautilusView *view, 
                                                 const char *config_path);
void nautilus_view_show_properties              (NautilusView *view);
void nautilus_view_stop_location_change         (NautilusView *view);
void nautilus_view_set_active_errors            (NautilusView *view, gboolean enabled);


gboolean  nautilus_view_is_zoomable                (NautilusView *view);
gdouble   nautilus_view_get_zoom_level             (NautilusView *view);
void      nautilus_view_set_zoom_level             (NautilusView *view,
                                                    gdouble       zoom_level);
gdouble   nautilus_view_get_min_zoom_level         (NautilusView *view);
gdouble   nautilus_view_get_max_zoom_level         (NautilusView *view);
gboolean  nautilus_view_get_is_continuous          (NautilusView *view);
void      nautilus_view_zoom_in                    (NautilusView *view);
void      nautilus_view_zoom_out	           (NautilusView *view);
void      nautilus_view_zoom_to_fit	           (NautilusView *view);


/* This is a "protected" operation */
void    nautilus_view_construct_arg_set(NautilusView *view);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __NAUTILUS_VIEW_H__ */
