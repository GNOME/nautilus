/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-scalable-font.h - A GtkObject subclass for access to scalable fonts.

   Copyright (C) 1999, 2000 Eazel, Inc.

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

/* NautilusScalableFont is a GtkObject that provdes a simple
 * interface to Raph Levien's librsvg FreeType2 based anti aliased
 * text rendering.
 *
 * Currently, only Type1 fonts are supported.
 *
 * Fonts are automatically queried and used if available.  Right
 * now this is fairly simple code which does not handle all the 
 * complexities of the hell that is unix fonts.
 *
 * In the Star Trek future, we will use gnome-print (gnome-font?).
 * However, we will keep the interface to scalable font usage simple
 * and hidden behind this interface.
 */

#ifndef NAUTILUS_SCALABLE_FONT_H
#define NAUTILUS_SCALABLE_FONT_H

#include <gtk/gtkobject.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-defs.h>
#include <libnautilus-extensions/nautilus-string-list.h>
#include <libnautilus-extensions/nautilus-art-extensions.h>
#include <libart_lgpl/art_rect.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_SCALABLE_FONT		(nautilus_scalable_font_get_type ())
#define NAUTILUS_SCALABLE_FONT(obj)		(GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_SCALABLE_FONT, NautilusScalableFont))
#define NAUTILUS_SCALABLE_FONT_CLASS(klass)	(GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_SCALABLE_FONT, NautilusScalableFontClass))
#define NAUTILUS_IS_SCALABLE_FONT(obj)		(GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_SCALABLE_FONT))
#define NAUTILUS_IS_SCALABLE_FONT_CLASS(klass)	(GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_SCALABLE_FONT))

typedef struct _NautilusScalableFont	       NautilusScalableFont;
typedef struct _NautilusScalableFontClass      NautilusScalableFontClass;
typedef struct _NautilusScalableFontDetail     NautilusScalableFontDetail;

struct _NautilusScalableFont
{
	/* Superclass */
	GtkObject		object;

	/* Private things */
	NautilusScalableFontDetail	*detail;
};

struct _NautilusScalableFontClass
{
	GtkObjectClass		parent_class;
};

GtkType                nautilus_scalable_font_get_type                        (void);
NautilusScalableFont  *nautilus_scalable_font_new                             (const char                  *family,
									       const char                  *weight,
									       const char                  *slant,
									       const char                  *set_width);
NautilusDimensions     nautilus_scalable_font_measure_text                    (const NautilusScalableFont  *font,
									       int                          font_size,
									       const char                  *text,
									       guint                        text_length);
int                    nautilus_scalable_font_text_width                      (const NautilusScalableFont  *font,
									       int                          font_size,
									       const char                  *text,
									       guint                        text_length);
void                   nautilus_scalable_font_draw_text                       (const NautilusScalableFont  *font,
									       GdkPixbuf                   *destination_pixbuf,
									       int                          x,
									       int                          y,
									       const ArtIRect              *clip_area,
									       int                          font_size,
									       const char                  *text,
									       guint                        text_length,
									       guint32                      color,
									       int                          opacity);
void                   nautilus_scalable_font_measure_text_lines              (const NautilusScalableFont  *font,
									       int                          font_size,
									       const char                  *text,
									       guint                        num_text_lines,
									       double                       empty_line_height,
									       NautilusDimensions           text_line_dimensions[],
									       int                         *max_width_out,
									       int                         *total_height_out);
void                   nautilus_scalable_font_draw_text_lines_with_dimensions (const NautilusScalableFont  *font,
									       GdkPixbuf                   *destination_pixbuf,
									       int                          x,
									       int                          y,
									       const ArtIRect              *clip_area,
									       int                          font_size,
									       const char                  *text,
									       guint                        num_text_lines,
									       const NautilusDimensions    *text_line_dimensions,
									       GtkJustification             justification,
									       int                          line_offset,
									       double                       empty_line_height,
									       guint32                      color,
									       int                          opacity);
void                   nautilus_scalable_font_draw_text_lines                 (const NautilusScalableFont  *font,
									       GdkPixbuf                   *destination_pixbuf,
									       int                          x,
									       int                          y,
									       const ArtIRect              *clip_area,
									       int                          font_size,
									       const char                  *text,
									       GtkJustification             justification,
									       int                          line_offset,
									       double                       empty_line_height,
									       guint32                      color,
									       int                          opacity);
int                    nautilus_scalable_font_largest_fitting_font_size       (const NautilusScalableFont  *font,
									       const char                  *text,
									       int                          available_width,
									       int                          minimum_acceptable_font_size,
									       int                          maximum_acceptable_font_size);
NautilusScalableFont  *nautilus_scalable_font_get_default_font                (void);
NautilusStringList *   nautilus_scalable_font_get_font_family_list            (void);
gboolean               nautilus_scalable_font_query_font                      (const char                  *family,
									       NautilusStringList         **weights,
									       NautilusStringList         **slants,
									       NautilusStringList         **set_widths);
NautilusScalableFont  *nautilus_scalable_font_make_bold                       (NautilusScalableFont        *font);

END_GNOME_DECLS

#endif /* NAUTILUS_SCALABLE_FONT_H */


