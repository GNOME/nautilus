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
 * Modified by the GTK+ Team and others 1997-1999, 2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef GTK_SCROLL_FRAME_H
#define GTK_SCROLL_FRAME_H

#include <gtk/gtkbin.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define NAUTILUS_TYPE_SCROLL_FRAME            (nautilus_scroll_frame_get_type ())
#define NAUTILUS_SCROLL_FRAME(obj)            (GTK_CHECK_CAST ((obj),		\
	NAUTILUS_TYPE_SCROLL_FRAME, NautilusScrollFrame))
#define NAUTILUS_SCROLL_FRAME_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass),	\
	NAUTILUS_TYPE_SCROLL_FRAME, NautilusScrollFrameClass))
#define NAUTILUS_IS_SCROLL_FRAME(obj)         (GTK_CHECK_TYPE ((obj),		\
	NAUTILUS_TYPE_SCROLL_FRAME))
#define NAUTILUS_IS_SCROLL_FRAME_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass),	\
	NAUTILUS_TYPE_SCROLL_FRAME))

typedef struct NautilusScrollFrame NautilusScrollFrame;
typedef struct NautilusScrollFrameClass NautilusScrollFrameClass;
typedef struct NautilusScrollFrameDetails NautilusScrollFrameDetails;

struct NautilusScrollFrame
{
	GtkBin bin;
	NautilusScrollFrameDetails *details;
};
struct NautilusScrollFrameClass
{
	GtkBinClass parent_class;
};

GtkType        nautilus_scroll_frame_get_type              (void);
GtkWidget *    nautilus_scroll_frame_new                   (GtkAdjustment       *h_adjustment,
							    GtkAdjustment       *v_adjustment);
void           nautilus_scroll_frame_set_hadjustment       (NautilusScrollFrame *frame,
							    GtkAdjustment       *adj);
void           nautilus_scroll_frame_set_vadjustment       (NautilusScrollFrame *frame,
							    GtkAdjustment       *adj);
GtkAdjustment *nautilus_scroll_frame_get_hadjustment       (NautilusScrollFrame *frame);
GtkAdjustment *nautilus_scroll_frame_get_vadjustment       (NautilusScrollFrame *frame);
void           nautilus_scroll_frame_set_policy            (NautilusScrollFrame *frame,
							    GtkPolicyType        h_scroll_policy,
							    GtkPolicyType        v_scroll_policy);
void           nautilus_scroll_frame_set_placement         (NautilusScrollFrame *frame,
							    GtkCornerType        frame_placement);
void           nautilus_scroll_frame_set_shadow_type       (NautilusScrollFrame *frame,
							    GtkShadowType        shadow_type);
void           nautilus_scroll_frame_set_scrollbar_spacing (NautilusScrollFrame *frame,
							    guint                spacing);
void           nautilus_scroll_frame_add_with_viewport     (NautilusScrollFrame *frame,
							    GtkWidget           *child);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* GTK_SCROLL_FRAME_H */
