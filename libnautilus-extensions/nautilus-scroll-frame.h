/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_SCROLL_FRAME_H__
#define __GTK_SCROLL_FRAME_H__


#include <gdk/gdk.h>
#include <gtk/gtkbin.h>


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define GTK_TYPE_SCROLL_FRAME            (gtk_scroll_frame_get_type ())
#define GTK_SCROLL_FRAME(obj)            (GTK_CHECK_CAST ((obj),		\
	GTK_TYPE_SCROLL_FRAME, GtkScrollFrame))
#define GTK_SCROLL_FRAME_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass),	\
	GTK_TYPE_SCROLL_FRAME, GtkScrollFrameClass))
#define GTK_IS_SCROLL_FRAME(obj)         (GTK_CHECK_TYPE ((obj),		\
	GTK_TYPE_SCROLL_FRAME))
#define GTK_IS_SCROLL_FRAME_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass),	\
	GTK_TYPE_SCROLL_FRAME))


typedef struct _GtkScrollFrame       GtkScrollFrame;
typedef struct _GtkScrollFrameClass  GtkScrollFrameClass;

struct _GtkScrollFrame
{
	GtkBin bin;

	/* Private data */
	gpointer priv;
};

struct _GtkScrollFrameClass
{
	GtkBinClass parent_class;
};


GtkType gtk_scroll_frame_get_type (void);
GtkWidget *gtk_scroll_frame_new (GtkAdjustment *hadj, GtkAdjustment *vadj);

void gtk_scroll_frame_set_hadjustment (GtkScrollFrame *sf, GtkAdjustment *adj);
void gtk_scroll_frame_set_vadjustment (GtkScrollFrame *sf, GtkAdjustment *adj);

GtkAdjustment *gtk_scroll_frame_get_hadjustment (GtkScrollFrame *sf);
GtkAdjustment *gtk_scroll_frame_get_vadjustment (GtkScrollFrame *sf);

void gtk_scroll_frame_set_policy (GtkScrollFrame *sf,
				  GtkPolicyType hsb_policy,
				  GtkPolicyType vsb_policy);

void gtk_scroll_frame_set_placement (GtkScrollFrame *sf, GtkCornerType frame_placement);
void gtk_scroll_frame_set_shadow_type (GtkScrollFrame *sf, GtkShadowType shadow_type);
void gtk_scroll_frame_set_scrollbar_spacing (GtkScrollFrame *sf, guint spacing);

void gtk_scroll_frame_add_with_viewport (GtkScrollFrame *sf, GtkWidget *child);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __GTK_SCROLL_FRAME_H__ */
