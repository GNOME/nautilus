/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-text-layout.h - Functions to layout text.

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

#ifndef NAUTILUS_TEXT_LAYOUT_H
#define NAUTILUS_TEXT_LAYOUT_H

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libgnome/gnome-defs.h>
#include <libnautilus-extensions/nautilus-scalable-font.h>
#include <libart_lgpl/art_rect.h>

BEGIN_GNOME_DECLS

/*
 * The following text_layout stuff was shamelessly plundered
 * from libgnomeui/gnome-icon-text.[ch] by Federico Mena.
 *
 * It was hacked to use NautilusScalableFont and GdkPixbuf
 * instead of GdkFont and GdkDrawable.  We want to use the
 * same layout algorithm in Nautilus so that both the smooth
 * and not smooth text rendering cases have predictably 
 * similar result.
 *
 * I also made some minor Nautilus-like style changes. -re

 */
typedef struct
{
	char *text;
	int width;
	guint text_length;
} NautilusTextLayoutRow;

typedef struct
{
	GList *rows;
	const NautilusScalableFont *font;
	int font_size;
	int width;
	int height;
	int baseline_skip;
} NautilusTextLayout;

NautilusTextLayout *nautilus_text_layout_new   (const NautilusScalableFont *font,
						int                         font_size,
						const char                 *text,
						const char                 *separators,
						int                         max_width,
						gboolean                    confine);
void                nautilus_text_layout_paint (const NautilusTextLayout   *text_info,
						GdkPixbuf                  *pixbuf,
						int                         x,
						int                         y,
						GtkJustification            justification,
						guint32                     color,
						gboolean                    underlined);
void                nautilus_text_layout_free  (NautilusTextLayout         *text_info);

END_GNOME_DECLS

#endif /* NAUTILUS_TEXT_LAYOUT_H */


