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

/* NautilusLabel is a widget that is capable of display anti aliased
 * text composited over a complex background.  The background can be
 * installed as NautilusBackground on a NautilusLabel widget or any 
 * of its ancestors.  The best background will automatically be found
 * and used by the widget.
 * 
 * Fonts can be specified using a NautilusScalableFont object.
 *
 * Text can contain embedded new lines.
 *
 */

#ifndef NAUTILUS_LABEL_H
#define NAUTILUS_LABEL_H

#include <libnautilus-extensions/nautilus-buffered-widget.h>
#include <libnautilus-extensions/nautilus-scalable-font.h>

/* NautilusLabel is GtkWidget that draws a string using the high quality
 * anti aliased librsvg/freetype.
 */

BEGIN_GNOME_DECLS

#define NAUTILUS_TYPE_LABEL            (nautilus_label_get_type ())
#define NAUTILUS_LABEL(obj)            (GTK_CHECK_CAST ((obj), NAUTILUS_TYPE_LABEL, NautilusLabel))
#define NAUTILUS_LABEL_CLASS(klass)    (GTK_CHECK_CLASS_CAST ((klass), NAUTILUS_TYPE_LABEL, NautilusLabelClass))
#define NAUTILUS_IS_LABEL(obj)         (GTK_CHECK_TYPE ((obj), NAUTILUS_TYPE_LABEL))
#define NAUTILUS_IS_LABEL_CLASS(klass) (GTK_CHECK_CLASS_TYPE ((klass), NAUTILUS_TYPE_LABEL))

typedef struct _NautilusLabel	       NautilusLabel;
typedef struct _NautilusLabelClass     NautilusLabelClass;
typedef struct _NautilusLabelDetail    NautilusLabelDetail;

struct _NautilusLabel
{
	/* Superclass */
	NautilusBufferedWidget		buffered_widget;

	/* Private things */
	NautilusLabelDetail		*detail;
};

struct _NautilusLabelClass
{
	NautilusBufferedWidgetClass	parent_class;
};

GtkType               nautilus_label_get_type               (void);
GtkWidget *           nautilus_label_new                    (void);
void                  nautilus_label_set_text               (NautilusLabel        *label,
							     const char           *text);
char*                 nautilus_label_get_text               (NautilusLabel        *label);
void                  nautilus_label_set_font               (NautilusLabel        *label,
							     NautilusScalableFont *font);
NautilusScalableFont *nautilus_label_get_font               (const NautilusLabel  *label);
void                  nautilus_label_set_font_size          (NautilusLabel        *label,
							     guint                 font_size);
guint                 nautilus_label_get_font_size          (const NautilusLabel  *label);
void                  nautilus_label_set_text_color         (NautilusLabel        *label,
							     guint32               color);
guint32               nautilus_label_get_text_color         (const NautilusLabel  *label);
void                  nautilus_label_set_text_alpha         (NautilusLabel        *label,
							     guchar                alpha);
guchar                nautilus_label_get_text_alpha         (const NautilusLabel  *label);
void                  nautilus_label_set_text_justification (NautilusLabel        *label,
							     GtkJustification      justification);
GtkJustification      nautilus_label_get_text_justification (const NautilusLabel  *label);
void                  nautilus_label_set_line_offset        (NautilusLabel        *label,
							     guint                 alpha);
guint                 nautilus_label_get_line_offset        (const NautilusLabel  *label);


END_GNOME_DECLS

#endif /* NAUTILUS_LABEL_H */


