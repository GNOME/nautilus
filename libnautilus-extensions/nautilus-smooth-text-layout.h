/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-smooth-text-layout.h - A GtkObject subclass for dealing with smooth text.

   Copyright (C) 2000 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef NAUTILUS_SMOOTH_TEXT_LAYOUT_H
#define NAUTILUS_SMOOTH_TEXT_LAYOUT_H

#include <gtk/gtkobject.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-defs.h>
#include <libnautilus-extensions/nautilus-art-extensions.h>
#include <libnautilus-extensions/nautilus-scalable-font.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_SMOOTH_TEXT_LAYOUT		(nautilus_smooth_text_layout_get_type ())
#define NAUTILUS_SMOOTH_TEXT_LAYOUT(obj)		(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_SMOOTH_TEXT_LAYOUT, NautilusSmoothTextLayout))
#define NAUTILUS_SMOOTH_TEXT_LAYOUT_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SMOOTH_TEXT_LAYOUT, NautilusSmoothTextLayoutClass))
#define NAUTILUS_IS_SMOOTH_TEXT_LAYOUT(obj)		(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_SMOOTH_TEXT_LAYOUT))
#define NAUTILUS_IS_SMOOTH_TEXT_LAYOUT_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SMOOTH_TEXT_LAYOUT))

typedef struct NautilusSmoothTextLayout        NautilusSmoothTextLayout;
typedef struct NautilusSmoothTextLayoutClass   NautilusSmoothTextLayoutClass;
typedef struct NautilusSmoothTextLayoutDetails NautilusSmoothTextLayoutDetails;

struct NautilusSmoothTextLayout
{
	/* Superclass */
	GtkObject object;

	/* Private things */
	NautilusSmoothTextLayoutDetails *details;
};

struct NautilusSmoothTextLayoutClass
{
	GtkObjectClass parent_class;
};

GtkType                           nautilus_smooth_text_layout_get_type                  (void);
NautilusSmoothTextLayout  *       nautilus_smooth_text_layout_new                       (const char                     *text,
											 int                             text_length,
											 NautilusScalableFont           *font,
											 int                             font_size,
											 gboolean                        wrap);
NautilusDimensions                nautilus_smooth_text_layout_get_dimensions            (const NautilusSmoothTextLayout *smooth_text_layout);
void                              nautilus_smooth_text_layout_set_line_wrap_width       (NautilusSmoothTextLayout       *smooth_text_layout,
											 int                             line_wrap_width);
int                               nautilus_smooth_text_layout_get_width                 (const NautilusSmoothTextLayout *smooth_text_layout);
int                               nautilus_smooth_text_layout_get_height                (const NautilusSmoothTextLayout *smooth_text_layout);
void                              nautilus_smooth_text_layout_set_wrap                  (NautilusSmoothTextLayout       *smooth_text_layout,
											 gboolean                        wrap);
gboolean                          nautilus_smooth_text_layout_get_wrap                  (const NautilusSmoothTextLayout *smooth_text_layout);
void                              nautilus_smooth_text_layout_set_line_break_characters (NautilusSmoothTextLayout       *smooth_text_layout,
											 const char                     *line_break_characters);
char *                            nautilus_smooth_text_layout_get_line_break_characters (const NautilusSmoothTextLayout *smooth_text_layout);
void                              nautilus_smooth_text_layout_set_font                  (NautilusSmoothTextLayout       *smooth_text_layout,
											 NautilusScalableFont           *font);
NautilusScalableFont             *nautilus_smooth_text_layout_get_font                  (const NautilusSmoothTextLayout *smooth_text_layout);
void                              nautilus_smooth_text_layout_set_font_size             (NautilusSmoothTextLayout       *smooth_text_layout,
											 int                             font_size);
int                               nautilus_smooth_text_layout_get_font_size             (const NautilusSmoothTextLayout *smooth_text_layout);
void                              nautilus_smooth_text_layout_set_line_spacing          (NautilusSmoothTextLayout       *smooth_text_layout,
											 int                             line_spacing);
int                               nautilus_smooth_text_layout_get_line_spacing          (const NautilusSmoothTextLayout *smooth_text_layout);
void                              nautilus_smooth_text_layout_set_empty_line_height     (NautilusSmoothTextLayout       *smooth_text_layout,
											 int                             empty_line_height);
int                               nautilus_smooth_text_layout_get_empty_line_height     (const NautilusSmoothTextLayout *smooth_text_layout);
void                              nautilus_smooth_text_layout_draw_to_pixbuf            (const NautilusSmoothTextLayout *smooth_text_layout,
											 GdkPixbuf                      *destination_pixbuf,
											 int                             source_x,
											 int                             source_y,
											 const ArtIRect                 *destination_area,
											 GtkJustification                justification,
											 gboolean                        underlined,
											 guint32                         color,
											 int                             opacity);
void                              nautilus_smooth_text_layout_draw_to_pixbuf_shadow     (const NautilusSmoothTextLayout *smooth_text_layout,
											 GdkPixbuf                      *destination_pixbuf,
											 int                             source_x,
											 int                             source_y,
											 const ArtIRect                 *destination_area,
											 int                             shadow_offset,
											 GtkJustification                justification,
											 gboolean                        underlined,
											 guint32                         color,
											 guint32                         shadow_color,
											 int                             opacity);

END_GNOME_DECLS

#endif /* NAUTILUS_SMOOTH_TEXT_LAYOUT_H */


