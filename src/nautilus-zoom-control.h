/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* Nautilus
 * Copyright (C) 2000 Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * This is the header file for the zoom control on the location bar
 *
 */

#ifndef __NAUTILUS_ZOOM_CONTROL_H__
#define __NAUTILUS_ZOOM_CONTROL_H__


#include <gdk/gdk.h>
#include <gtk/gtkpixmap.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define NAUTILUS_TYPE_ZOOM_CONTROL	 (nautilus_zoom_control_get_type ())
#define NAUTILUS_ZOOM_CONTROL(obj)	 (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_ZOOM_CONTROL, NautilusZoomControl))
#define NAUTILUS_ZOOM_CONTROL_CLASS(klass) (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_ZOOM_CONTROL, NautilusZoomControlClass))
#define NAUTILUS_IS_ZOOM_CONTROL(obj)	 (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_ZOOM_CONTROL))
#define NAUTILUS_IS_ZOOM_CONTROL_CLASS(klass)	 (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_ZOOM_CONTROL))

typedef struct _NautilusZoomControl	 NautilusZoomControl;
typedef struct _NautilusZoomControlClass NautilusZoomControlClass;

struct _NautilusZoomControl
{
  GtkPixmap pixmap;
  
  gint current_zoom;
  gint min_zoom;	 
  gint max_zoom;
  double zoom_factor;
};

struct _NautilusZoomControlClass
{
  GtkEventBoxClass parent_class;
};


GtkType	   	nautilus_zoom_control_get_type	 (void);
GtkWidget* 	nautilus_zoom_control_new	 (void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __NAUTILUS_ZOOM_CONTROL_H__ */
