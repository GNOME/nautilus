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
/* ntl-view.h: Interface of the object representing a data
   view. NautilusContentView and NautilusMetaView derive from this
   class. */

#ifndef __NAUTILUS_VIEW_H__
#define __NAUTILUS_VIEW_H__

#include <gtk/gtkwidget.h>
#include <gtk/gtkbin.h>
#include "ntl-types.h"
#include <bonobo/gnome-bonobo.h>

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

  void (*notify_location_change)	(NautilusView *view,
					 Nautilus_NavigationInfo *nav_context);
  void (*notify_selection_change)	(NautilusView *view,
					 Nautilus_SelectionInfo *nav_context);
  void (*load_state) (NautilusView *view, const char *config_path);
  void (*save_state) (NautilusView *view, const char *config_path);
  void (*show_properties) (NautilusView *view);

  void (*view_constructed) (NautilusView *view); /* Not a signal. Work-around for Gtk+'s lack of a 'constructed' operation */

  GtkBinClass *parent_class;
  guint view_signals[6];
  guint num_construct_args;

  gpointer servant_init_func, servant_destroy_func, vepv;
};

struct _NautilusView
{
  GtkBin parent;

  GtkWidget *main_window;

  char *iid;

  enum { NV_NONE, NV_NAUTILUS_VIEW, NV_BONOBO_SUBDOC, NV_BONOBO_CONTROL } type;

  union {
    struct {
      CORBA_Object view_client;
      GnomeObject *control_frame;

    } nautilus_view_info;
    struct {
      GnomeObject *container, *client_site, *view_frame;

    } bonobo_subdoc_info;
    struct {
      GnomeObject *control_frame;

    } bonobo_control_info;
  } u; 

  GnomeObjectClient *client_object;
  GtkWidget *client_widget;

  GnomeObject *view_frame;

  guint construct_arg_count;
};

GtkType nautilus_view_get_type                (void);
gboolean nautilus_view_load_client              (NautilusView              *view,
                                                 const char *               iid);
const char *nautilus_view_get_iid(NautilusView *view);
CORBA_Object nautilus_view_get_client_objref(NautilusView *view);
GnomeObject *nautilus_view_get_control_frame(NautilusView *view);
CORBA_Object nautilus_view_get_objref(NautilusView *view);

/* This is a "protected" operation */
void    nautilus_view_construct_arg_set(NautilusView *view);

#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __NAUTILUS_VIEW_H__ */
