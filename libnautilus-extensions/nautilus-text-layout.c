/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-text-layout.c - Functions to layout text.

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

#include <config.h>
#include "nautilus-text-layout.h"
#include "nautilus-string.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"

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
 *
 */
static void
text_layout_free_row (gpointer data,
		      gpointer user_data)
{
	NautilusTextLayoutRow *row;

	if (data) {
		row = data;
		g_free (row->text);
		g_free (row);
	}
}

/**
 * nautilus_text_layout_free:
 * @ti: An icon text info structure.
 *
 * Frees a &NautilusTextLayout structure.  You should call this instead of
 * freeing the structure yourself.
 */
void
nautilus_text_layout_free (NautilusTextLayout *text_layout)
{
	g_list_foreach (text_layout->rows, text_layout_free_row, NULL);
	g_list_free (text_layout->rows);
	g_free (text_layout);
}

/**
 * nautilus_text_layout_new:
 * @font:       Name of the font that will be used to render the text.
 * @text:       Text to be formatted.
 * @separators: Separators used for word wrapping, can be NULL.
 * @max_width:  Width in pixels to be used for word wrapping.
 * @confine:    Whether it is mandatory to wrap at @max_width.
 *
 * Creates a new &NautilusTextLayout structure by wrapping the specified
 * text.  If non-NULL, the @separators argument defines a set of characters
 * to be used as word delimiters for performing word wrapping.  If it is
 * NULL, then only spaces will be used as word delimiters.
 *
 * The @max_width argument is used to specify the width at which word
 * wrapping will be performed.  If there is a very long word that does not
 * fit in a single line, the @confine argument can be used to specify
 * whether the word should be unconditionally split to fit or whether
 * the maximum width should be increased as necessary.
 *
 * Return value: A newly-created &NautilusTextLayout structure.
 */
NautilusTextLayout *
nautilus_text_layout_new (const NautilusScalableFont *font,
			  int font_size,
			  const char *text,
			  const char *separators,
			  int max_width,
			  gboolean confine)
{
	NautilusTextLayout *text_layout;
	NautilusTextLayoutRow *row;
	const char *row_end;
	const char *s, *word_start, *word_end, *old_word_end;
	char *sub_text;
	int i, w_len;
	int w;
	const char *text_iter;
	int text_len, separators_len;

	g_return_val_if_fail (font != NULL, NULL);
	g_return_val_if_fail (font_size > 0, NULL);
	g_return_val_if_fail (text != NULL, NULL);
	g_return_val_if_fail (nautilus_strlen (text) > 0, NULL);

	if (!separators)
		separators = " ";

	text_len = strlen (text);

	separators_len = strlen (separators);

	text_layout = g_new (NautilusTextLayout, 1);

	text_layout->rows = NULL;
	text_layout->font = font;
	text_layout->font_size = font_size;
	text_layout->width = 0;
	text_layout->height = 0;
	text_layout->baseline_skip = font_size;

	word_end = NULL;

	text_iter = text;
	while (*text_iter) {
		for (row_end = text_iter; *row_end != 0 && *row_end != '\n'; row_end++);

		/* Accumulate words from this row until they don't fit in the max_width */

		s = text_iter;

		while (s < row_end) {
			word_start = s;
			old_word_end = word_end;
			for (word_end = word_start; *word_end; word_end++) {
				const char *p;
				for (p = separators; *p; p++) {
					if (*word_end == *p)
						goto found;
				}
			}
		  found:
			if (word_end < row_end)
				word_end++;

			if (nautilus_scalable_font_text_width (font, font_size, text_iter, word_end - text_iter) > max_width) {
				if (word_start == text_iter) {
					if (confine) {
						/* We must force-split the word.  Look for a proper
                                                 * place to do it.
						 */

						w_len = word_end - word_start;
						
						for (i = 1; i < w_len; i++) {
							w = nautilus_scalable_font_text_width (font, font_size, word_start, i);
							if (w > max_width) {
								if (i == 1)
									/* Shit, not even a single character fits */
									max_width = w;
								else
									break;
							}
						}

						/* Create sub-row with the chars that fit */

						sub_text = g_strndup (word_start, i - 1);
						
						row = g_new (NautilusTextLayoutRow, 1);
						row->text = sub_text;
						row->text_length = i - 1;
						row->width = nautilus_scalable_font_text_width (font, font_size, 
												sub_text, 
												strlen (sub_text));

						text_layout->rows = g_list_append (text_layout->rows, row);

						if (row->width > text_layout->width)
							text_layout->width = row->width;

						text_layout->height += text_layout->baseline_skip;

						/* Bump the text pointer */

						text_iter += i - 1;
						s = text_iter;

						continue;
					} else
						max_width = nautilus_scalable_font_text_width (font, font_size, word_start, word_end - word_start);

					continue; /* Retry split */
				} else {
					word_end = old_word_end; /* Restore to region that does fit */
					break; /* Stop the loop because we found something that doesn't fit */
				}
			}

			s = word_end;
		}

		/* Append row */

		if (text_iter == row_end) {
			/* We are on a newline, so append an empty row */

			text_layout->rows = g_list_append (text_layout->rows, NULL);
			text_layout->height += text_layout->baseline_skip / 2;

			/* Next! */

			text_iter = row_end + 1;
		} else {
			/* Create subrow and append it to the list */

			int sub_len;
			sub_len = word_end - text_iter;

			sub_text = g_strndup (text_iter, sub_len);

			row = g_new (NautilusTextLayoutRow, 1);
			row->text = sub_text;
			row->text_length = sub_len;
			row->width = nautilus_scalable_font_text_width (font, font_size, sub_text, sub_len);

			text_layout->rows = g_list_append (text_layout->rows, row);

			if (row->width > text_layout->width)
				text_layout->width = row->width;

			text_layout->height += text_layout->baseline_skip;

			/* Next! */

			text_iter = word_end;
		}
	}

	return text_layout;
}

/**
 * nautilus_text_layout_paint:
 * @ti:       An icon text info structure.
 * @drawable: Target drawable.
 * @gc:       GC used to render the string.
 * @x:        Left coordinate for text.
 * @y:        Upper coordinate for text.
 * @just:     Justification for text.
 *
 * Paints the formatted text in the icon text info structure onto a drawable.
 * This is just a sample implementation; applications can choose to use other
 * rendering functions.
 */
void
nautilus_text_layout_paint (const NautilusTextLayout *text_layout,
			    GdkPixbuf *destination_pixbuf,
			    int x, 
			    int y, 
			    GtkJustification justification,
			    guint32 color,
			    gboolean underlined)
{
	GList *item;
	const NautilusTextLayoutRow *row;
	int xpos;

	g_return_if_fail (text_layout != NULL);
	g_return_if_fail (destination_pixbuf != NULL);
	g_return_if_fail (justification >= GTK_JUSTIFY_LEFT && justification <= GTK_JUSTIFY_FILL);
	
	/* FIXME bugzilla.eazel.com 5087: Make sure the color we are fed is opaque.  The real solution is 
	 * to fix the callers.
	 */
	color = NAUTILUS_RGBA_COLOR_PACK (NAUTILUS_RGBA_COLOR_GET_R (color),
					  NAUTILUS_RGBA_COLOR_GET_G (color),
					  NAUTILUS_RGBA_COLOR_GET_B (color),
					  NAUTILUS_OPACITY_FULLY_OPAQUE);

	for (item = text_layout->rows; item; item = item->next) {
		if (item->data) {
			row = item->data;

			switch (justification) {
			case GTK_JUSTIFY_LEFT:
				xpos = 0;
				break;

			case GTK_JUSTIFY_RIGHT:
				xpos = text_layout->width - row->width;
				break;

			case GTK_JUSTIFY_CENTER:
				xpos = (text_layout->width - row->width) / 2;
				break;

			default:
				/* Anyone care to implement GTK_JUSTIFY_FILL? */
				g_warning ("Justification type %d not supported.  Using left-justification.",
					   (int) justification);
				xpos = 0;
			}

			nautilus_scalable_font_draw_text (text_layout->font,
							  destination_pixbuf,
							  x + xpos,
							  y,
							  NULL,
							  text_layout->font_size,
							  row->text,
							  row->text_length,
							  color,
							  NAUTILUS_OPACITY_FULLY_OPAQUE);
			
			/* Underline the text if needed */
			if (underlined) {
				ArtIRect underline_rect;

				/* FIXME bugzilla.eazel.com 2865: This underlining code should
				 * take into account the baseline for the rendered string rather
				 * that doing the '-2' nonsense.
				 */
				nautilus_art_irect_assign (&underline_rect,
							   x + xpos,
							   y + text_layout->font_size - 2,
							   row->width,
							   1);

				nautilus_gdk_pixbuf_fill_rectangle_with_color (destination_pixbuf,
									       &underline_rect,
									       color);
			}
			
			y += text_layout->baseline_skip;
		} else
			y += text_layout->baseline_skip / 2;
	}
}
