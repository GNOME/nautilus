/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-label.h - A widget to display a anti aliased text.

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

#ifndef NAUTILUS_LABEL_H
#define NAUTILUS_LABEL_H

#include <gtk/gtklabel.h>
#include <libnautilus-extensions/nautilus-scalable-font.h>
#include <libnautilus-extensions/nautilus-smooth-widget.h>

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_LABEL            (nautilus_label_get_type ())
#define NAUTILUS_LABEL(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_LABEL, NautilusLabel))
#define NAUTILUS_LABEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_LABEL, NautilusLabelClass))
#define NAUTILUS_IS_LABEL(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_LABEL))
#define NAUTILUS_IS_LABEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_LABEL))

typedef struct _NautilusLabel	       NautilusLabel;
typedef struct _NautilusLabelClass     NautilusLabelClass;
typedef struct _NautilusLabelDetails   NautilusLabelDetails;

struct _NautilusLabel
{
	/* Superclass */
	GtkLabel gtk_label;

	/* Private things */
	NautilusLabelDetails *details;
};

struct _NautilusLabelClass
{
	GtkLabelClass parent_class;

	NautilusSmoothWidgetDrawBackground draw_background;
	NautilusSmoothWidgetSetIsSmooth set_is_smooth;
};

GtkType                      nautilus_label_get_type                       (void);
GtkWidget *                  nautilus_label_new                            (const char                   *text);
void                         nautilus_label_set_is_smooth                  (NautilusLabel                *label,
									    gboolean                      is_smooth);
gboolean                     nautilus_label_get_is_smooth                  (const NautilusLabel          *label);
void                         nautilus_label_set_smooth_font                (NautilusLabel                *label,
									    NautilusScalableFont         *font);
NautilusScalableFont *       nautilus_label_get_smooth_font                (const NautilusLabel          *label);
void                         nautilus_label_set_smooth_font_size           (NautilusLabel                *label,
									    guint                         font_size);
guint                        nautilus_label_get_smooth_font_size           (const NautilusLabel          *label);
void                         nautilus_label_set_text_opacity               (NautilusLabel                *label,
									    int                           opacity);
int                          nautilus_label_get_text_opacity               (const NautilusLabel          *label);
void                         nautilus_label_set_background_mode            (NautilusLabel                *label,
									    NautilusSmoothBackgroundMode  background_mode);
NautilusSmoothBackgroundMode nautilus_label_get_background_mode            (const NautilusLabel          *label);
void                         nautilus_label_set_solid_background_color     (NautilusLabel                *label,
									    guint32                       solid_background_color);
guint32                      nautilus_label_get_solid_background_color     (const NautilusLabel          *label);
void                         nautilus_label_set_text_color                 (NautilusLabel                *label,
									    guint32                       color);
guint32                      nautilus_label_get_text_color                 (const NautilusLabel          *label);
void                         nautilus_label_set_smooth_drop_shadow_offset  (NautilusLabel                *label,
									    guint                         offset);
guint                        nautilus_label_get_smooth_drop_shadow_offset  (const NautilusLabel          *label);
void                         nautilus_label_set_smooth_drop_shadow_color   (NautilusLabel                *label,
									    guint32                       color);
guint32                      nautilus_label_get_smooth_drop_shadow_color   (const NautilusLabel          *label);
void                         nautilus_label_set_smooth_line_wrap_width     (NautilusLabel                *label,
									    guint                         line_wrap_width);
guint                        nautilus_label_get_smooth_line_wrap_width     (const NautilusLabel          *label);
void                         nautilus_label_set_text                       (NautilusLabel                *label,
									    const char                   *text);
char*                        nautilus_label_get_text                       (const NautilusLabel          *label);
void                         nautilus_label_set_justify                    (NautilusLabel                *label,
									    GtkJustification              justification);
GtkJustification             nautilus_label_get_text_justify               (const NautilusLabel          *label);
void                         nautilus_label_set_wrap                       (NautilusLabel                *label,
									    gboolean                      line_wrap);
gboolean                     nautilus_label_get_wrap                       (const NautilusLabel          *label);
GtkWidget *                  nautilus_label_new_solid                      (const char                   *text,
									    guint                         drop_shadow_offset,
									    guint32                       drop_shadow_color,
									    guint32                       text_color,
									    float                         xalign,
									    float                         yalign,
									    int                           xpadding,
									    int                           ypadding,
									    guint32                       background_color,
									    GdkPixbuf                    *tile_pixbuf);
void                         nautilus_label_make_bold                      (NautilusLabel                *label);
void                         nautilus_label_make_larger                    (NautilusLabel                *label,
									    guint                         num_sizes);
void                         nautilus_label_make_smaller                   (NautilusLabel                *label,
									    guint                         num_sizes);
void                         nautilus_label_set_tile_pixbuf                (NautilusLabel                *label,
									    GdkPixbuf                    *pixbuf);
void                         nautilus_label_set_tile_width                 (NautilusLabel                *label,
									    int                           tile_width);
int                          nautilus_label_get_tile_width                 (const NautilusLabel          *label);
void                         nautilus_label_set_tile_height                (NautilusLabel                *label,
									    int                           tile_height);
int                          nautilus_label_get_tile_height                (const NautilusLabel          *label);
void                         nautilus_label_set_tile_pixbuf_from_file_name (NautilusLabel                *label,
									    const char                   *tile_file_name);
GdkPixbuf*                   nautilus_label_get_tile_pixbuf                (const NautilusLabel          *label);
void                         nautilus_label_set_tile_opacity               (NautilusLabel                *label,
									    int                           tile_opacity);
int                          nautilus_label_get_tile_opacity               (const NautilusLabel          *label);
void                         nautilus_label_set_tile_mode_vertical         (NautilusLabel                *label,
									    NautilusSmoothTileMode        horizontal_tile_mode);
NautilusSmoothTileMode       nautilus_label_get_tile_mode_vertical         (const NautilusLabel          *label);
void                         nautilus_label_set_tile_mode_horizontal       (NautilusLabel                *label,
									    NautilusSmoothTileMode        horizontal_tile_mode);
NautilusSmoothTileMode       nautilus_label_get_tile_mode_horizontal       (const NautilusLabel          *label);
void                         nautilus_label_set_never_smooth               (NautilusLabel                *label,
									    gboolean                      never_smooth);
void                         nautilus_label_set_adjust_wrap_on_resize      (NautilusLabel                *label,
									    gboolean                      adjust_wrap_on_resize);
gboolean                     nautilus_label_get_adjust_wrap_on_resize      (const NautilusLabel          *label);

END_GNOME_DECLS

#endif /* NAUTILUS_LABEL_H */


