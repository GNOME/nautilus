/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-smooth-text-layout.c - A GtkObject subclass for dealing with smooth text.

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

#include <config.h>
#include "nautilus-smooth-text-layout.h"

#include "nautilus-gtk-macros.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-string.h"
#include "nautilus-glyph.h"
#include "nautilus-debug-drawing.h"

#include <libgnome/gnome-i18n.h>

#define MIN_FONT_SIZE 5
#define DEFAULT_LINE_SPACING 2
#define DEFAULT_FONT_SIZE 14

#define UNDEFINED_DIMENSION -1

/* This magic string is copied from GtkLabel.  It lives there unlocalized as well. */
#define DEFAULT_LINE_WRAP_WIDTH_TEXT "This is a good enough length for any line to have."

#define DEFAULT_LINE_BREAK_CHARACTERS _(" -_,;.?/&")

/* Detail member struct */
struct NautilusSmoothTextLayoutDetails
{
	NautilusDimensions dimensions;

	char *text;
	int text_length;

 	/* Smooth attributes */
 	NautilusScalableFont *font;
 	int font_size;
 	int line_spacing;
 	int empty_line_height;

	/* Text lines */
	GList *text_line_list;
	int max_line_width;
	int num_empty_lines;
	int line_wrap_width;
	int total_line_height;
	
	gboolean wrap;
	char *line_break_characters;
};

/* GtkObjectClass methods */
static void   nautilus_smooth_text_layout_initialize_class (NautilusSmoothTextLayoutClass  *smooth_text_layout_class);
static void   nautilus_smooth_text_layout_initialize       (NautilusSmoothTextLayout       *smooth_text_layout);
static void   nautilus_smooth_text_layout_destroy          (GtkObject                      *object);

/* Private functions */
static void   smooth_text_layout_set_text                  (NautilusSmoothTextLayout       *smooth_text_layout,
							    const char                     *text,
							    int                             text_length);
static void   smooth_text_layout_clear_lines               (NautilusSmoothTextLayout       *smooth_text_layout);
static void   smooth_text_layout_ensure_lines              (const NautilusSmoothTextLayout *smooth_text_layout);
static int    smooth_text_layout_get_num_empty_lines       (const NautilusSmoothTextLayout *smooth_text_layout);
static int    smooth_text_layout_get_empty_line_height     (const NautilusSmoothTextLayout *smooth_text_layout);
static int    smooth_text_layout_get_max_line_width        (const NautilusSmoothTextLayout *smooth_text_layout);
static int    smooth_text_layout_get_total_line_height     (const NautilusSmoothTextLayout *smooth_text_layout);
static int    smooth_text_layout_get_line_wrap_width       (const NautilusSmoothTextLayout *smooth_text_layout);
static GList *smooth_text_layout_line_list_new             (const char                     *text,
							    int                             text_length,
							    NautilusScalableFont           *font,
							    int                             font_size);
static void   smooth_text_layout_line_list_free            (GList                          *smooth_line_list);
void          smooth_text_layout_line_list_draw_to_pixbuf  (GList                          *smooth_line_list,
							    GdkPixbuf                      *pixbuf,
							    int                             x,
							    int                             y,
							    GtkJustification                justification,
							    gboolean                        underlined,
							    int                             empty_line_height,
							    int                             max_line_width,
							    int                             line_spacing,
							    guint32                         color,
							    int                             opacity);
static GList *smooth_text_layout_line_list_new_wrapped     (const char                     *text,
							    int                             text_length,
							    NautilusScalableFont           *font,
							    int                             font_size,
							    int                             max_width,
							    const char                     *line_break_characters);

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
void                nautilus_text_layout_free  (NautilusTextLayout         *text_info);
					    
NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusSmoothTextLayout, nautilus_smooth_text_layout, GTK_TYPE_OBJECT)

/* Class init methods */
static void
nautilus_smooth_text_layout_initialize_class (NautilusSmoothTextLayoutClass *smooth_text_layout_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (smooth_text_layout_class);
	
	/* GtkObjectClass */
	object_class->destroy = nautilus_smooth_text_layout_destroy;
}

void
nautilus_smooth_text_layout_initialize (NautilusSmoothTextLayout *smooth_text_layout)
{
	smooth_text_layout->details = g_new0 (NautilusSmoothTextLayoutDetails, 1);
	smooth_text_layout->details->line_break_characters = g_strdup (DEFAULT_LINE_BREAK_CHARACTERS);
	smooth_text_layout->details->font = nautilus_scalable_font_get_default_font ();
	smooth_text_layout->details->font_size = DEFAULT_FONT_SIZE;
	smooth_text_layout->details->line_spacing = DEFAULT_LINE_SPACING;
	smooth_text_layout_clear_lines (smooth_text_layout);
}

/* GtkObjectClass methods */
static void
nautilus_smooth_text_layout_destroy (GtkObject *object)
{
 	NautilusSmoothTextLayout *smooth_text_layout;

	g_return_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (object));

	smooth_text_layout = NAUTILUS_SMOOTH_TEXT_LAYOUT (object);

	smooth_text_layout_clear_lines (smooth_text_layout);
	g_free (smooth_text_layout->details);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* Private NautilusSmoothTextLayout functions */
static void
smooth_text_layout_clear_lines (NautilusSmoothTextLayout *smooth_text_layout)
{

	g_return_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout));

	smooth_text_layout_line_list_free (smooth_text_layout->details->text_line_list);
	smooth_text_layout->details->text_line_list = NULL;
	smooth_text_layout->details->dimensions.width = UNDEFINED_DIMENSION;
	smooth_text_layout->details->dimensions.height = UNDEFINED_DIMENSION;
	smooth_text_layout->details->max_line_width = UNDEFINED_DIMENSION;
	smooth_text_layout->details->num_empty_lines = UNDEFINED_DIMENSION;
	smooth_text_layout->details->empty_line_height = UNDEFINED_DIMENSION;
	smooth_text_layout->details->line_wrap_width = UNDEFINED_DIMENSION;
	smooth_text_layout->details->total_line_height = UNDEFINED_DIMENSION;
}

static void
smooth_text_layout_ensure_lines (const NautilusSmoothTextLayout *smooth_text_layout)
{
	g_return_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout));

	if (smooth_text_layout->details->text_line_list != NULL) {
		return;
	}
	
	/* We cheat a little here.  Pretend text_line_list or wrap_text_layouts are mutable */
	if (smooth_text_layout->details->wrap) {
		smooth_text_layout->details->text_line_list = 
			smooth_text_layout_line_list_new_wrapped (smooth_text_layout->details->text,
								  smooth_text_layout->details->text_length,
								  smooth_text_layout->details->font,
								  smooth_text_layout->details->font_size,
								  smooth_text_layout_get_line_wrap_width (smooth_text_layout),
								  smooth_text_layout->details->line_break_characters);
	} else {
		smooth_text_layout->details->text_line_list = 
			smooth_text_layout_line_list_new (smooth_text_layout->details->text,
							  smooth_text_layout->details->text_length,
							  smooth_text_layout->details->font,
							  smooth_text_layout->details->font_size);
	}
}

static GList *
smooth_text_layout_line_list_new (const char *text,
				  int text_length,
				  NautilusScalableFont *font,
				  int font_size)
{
	GList *line_list = NULL;
	const char *line;
	const char *end;

	g_return_val_if_fail (NAUTILUS_IS_SCALABLE_FONT (font), NULL);
	g_return_val_if_fail (text_length >= 0, NULL);
	g_return_val_if_fail (font_size > MIN_FONT_SIZE, NULL);

	end = text + text_length;
	
	line = text;

	while (line != NULL && line <= end) {
		const char *next_line;
		int length;
		NautilusGlyph *glyph = NULL;

		next_line = strchr (line, '\n');

		if (next_line != NULL) {
			length = next_line - line;
		} else {
			length = end - line;
		}

		g_assert (length >= 0);

		if (length > 0) {
			glyph = nautilus_glyph_new (font, font_size, line, length);
		}

		line_list = g_list_append (line_list, glyph);

		if (next_line != NULL) {
			line = next_line + 1;
		}
		else {
			line = NULL;
		}
	}

	return line_list;
}

static void
smooth_text_layout_line_list_free (GList *smooth_line_list)
{
	GList *node;

	node = smooth_line_list;
	while (node) {
		if (node->data != NULL) {
			nautilus_glyph_free (node->data);
		}
		node = node->next;
	}

	g_list_free (smooth_line_list);
}

void
smooth_text_layout_line_list_draw_to_pixbuf (GList *text_line_list,
					     GdkPixbuf *pixbuf,
					     int x,
					     int y,
					     GtkJustification justification,
					     gboolean underlined,
					     int empty_line_height,
					     int max_line_width,
					     int line_spacing,
					     guint32 color,
					     int opacity)
{
 	GList *node;

	g_return_if_fail (text_line_list != NULL);
	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (justification >= GTK_JUSTIFY_LEFT && justification <= GTK_JUSTIFY_FILL);
	g_return_if_fail (empty_line_height > 0);
	g_return_if_fail (max_line_width > 0);
	g_return_if_fail (line_spacing >= 0);

	/* FIXME bugzilla.eazel.com 5087: Make sure the color we are fed is opaque.  The real solution is 
	 * to fix the callers.
	 */
	color = color | 0xFF000000;

 	node = text_line_list;
 	while (node) {
 		if (node->data != NULL) {
			NautilusGlyph *glyph;
			int text_x = 0;
			int text_y = 0;

			glyph = node->data;

			g_assert (max_line_width >= nautilus_glyph_get_width (glyph));

			switch (justification) {
			case GTK_JUSTIFY_LEFT:
				text_x = x;
				break;
				
			case GTK_JUSTIFY_CENTER:
			case GTK_JUSTIFY_FILL:
				text_x = x + (max_line_width - nautilus_glyph_get_width (glyph)) / 2;
				break;

			case GTK_JUSTIFY_RIGHT:
				text_x = x + (max_line_width - nautilus_glyph_get_width (glyph));
				break;
				
			default:
				g_assert_not_reached ();
				text_x = x;
			}
			
			text_y = y;
			
			nautilus_glyph_draw_to_pixbuf (glyph, pixbuf, text_x, text_y, NULL, color, opacity);

			/* Underline the text if needed */
			if (underlined) {
				ArtIRect underline_rect;

				/* FIXME bugzilla.eazel.com 2865: This underlining code should
				 * take into account the baseline.
				 */
				const int underline_height = 1;

				underline_rect = nautilus_glyph_intersect (glyph, text_x, text_y, NULL);
				underline_rect = nautilus_art_irect_inset (underline_rect, 1, 0);
  				underline_rect.y0 = underline_rect.y1 - underline_height;
				
				nautilus_gdk_pixbuf_fill_rectangle_with_color (pixbuf,
									       &underline_rect,
									       color);
			}
			
			y += nautilus_glyph_get_height (glyph) + line_spacing;
 		} else {
			y += empty_line_height;
		}
 		node = node->next;
 	}
}

static GList *
smooth_text_layout_line_list_new_wrapped (const char *text,
					  int text_length,
					  NautilusScalableFont *font,
					  int font_size,
					  int max_width,
					  const char *line_break_characters)
{
	GList *line_list = NULL;
	GList *layout_list = NULL;
	GList *layout_node;
	const char *line;
	const char *end;

	g_return_val_if_fail (NAUTILUS_IS_SCALABLE_FONT (font), NULL);
	g_return_val_if_fail (text_length >= 0, NULL);
	g_return_val_if_fail (font_size > MIN_FONT_SIZE, NULL);
	g_return_val_if_fail (max_width > 0, NULL);
	g_return_val_if_fail (line_break_characters != NULL, NULL);
	g_return_val_if_fail (line_break_characters[0] != '\0', NULL);

	end = text + text_length;
	line = text;

	while (line != NULL && line <= end) {
		/* NULL layout means empty line */
		NautilusTextLayout *layout = NULL;
		const char *next_line;
		int length;
		next_line = strchr (line, '\n');

		if (next_line != NULL) {
			length = next_line - line;
		} else {
			length = end - line;
		}

		g_assert (length >= 0);

		if (length > 0) {
			char *null_terminated_line;
			null_terminated_line = g_strndup (line, length);
			layout = nautilus_text_layout_new (font,
							   font_size,
							   null_terminated_line,
							   line_break_characters,
							   max_width,
							   TRUE);
			g_free (null_terminated_line);
		}

		layout_list = g_list_append (layout_list, layout);

		if (next_line != NULL) {
			line = next_line + 1;
		}
		else {
			line = NULL;
		}
	}

	layout_node = layout_list;
	while (layout_node != NULL) {
		if (layout_node->data != NULL) {
			NautilusTextLayout *layout;
			GList *layout_row_node;
			g_assert (layout_node->data != NULL);
			layout = layout_node->data;
			
			layout_row_node = layout->rows;
			while (layout_row_node != NULL) {
				/* NULL glyph means empty line */				
				NautilusGlyph *glyph = NULL;
				
				if (layout_row_node->data != NULL) {
					const NautilusTextLayoutRow *row;
					row = layout_row_node->data;
					
					glyph = nautilus_glyph_new (font, font_size, row->text, row->text_length);
				} else {
				}
				
				line_list = g_list_append (line_list, glyph);
				
				layout_row_node = layout_row_node->next;
			}
			
			nautilus_text_layout_free (layout);
		} else {
			line_list = g_list_append (line_list, NULL);
		}
		layout_node = layout_node->next;
	}
	
	g_list_free (layout_list);

	return line_list;
}

static int
smooth_text_layout_get_empty_line_height (const NautilusSmoothTextLayout *smooth_text_layout)
{
	g_return_val_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout), 0);

	if (smooth_text_layout->details->empty_line_height == UNDEFINED_DIMENSION) {
		smooth_text_layout->details->empty_line_height = smooth_text_layout->details->font_size / 2;
	}

	return smooth_text_layout->details->empty_line_height;
}

static int
smooth_text_layout_get_num_empty_lines (const NautilusSmoothTextLayout *smooth_text_layout)
{
	g_return_val_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout), 0);
	
	if (smooth_text_layout->details->num_empty_lines == UNDEFINED_DIMENSION) {
		GList *node;

		smooth_text_layout->details->num_empty_lines = 0;
		node = smooth_text_layout->details->text_line_list;
		while (node) {
			if (node->data == NULL) {
				smooth_text_layout->details->num_empty_lines++;
			}
			node = node->next;
		}
	}

	return smooth_text_layout->details->num_empty_lines;
}

static int
smooth_text_layout_get_max_line_width (const NautilusSmoothTextLayout *smooth_text_layout)
{
	g_return_val_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout), 0);
	
	if (smooth_text_layout->details->max_line_width == UNDEFINED_DIMENSION) {
		GList *node;

		smooth_text_layout->details->max_line_width = 0;
		node = smooth_text_layout->details->text_line_list;
		while (node) {
			if (node->data != NULL) {
				smooth_text_layout->details->max_line_width = MAX (smooth_text_layout->details->max_line_width,
										   nautilus_glyph_get_width (node->data));
			}
			node = node->next;
		}
	}

	return smooth_text_layout->details->max_line_width;
}

static int
smooth_text_layout_get_total_line_height (const NautilusSmoothTextLayout *smooth_text_layout)
{
	g_return_val_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout), 0);
	
	if (smooth_text_layout->details->total_line_height == UNDEFINED_DIMENSION) {
		GList *node;

		smooth_text_layout->details->total_line_height = 0;
		node = smooth_text_layout->details->text_line_list;
		while (node) {
			if (node->data != NULL) {
				smooth_text_layout->details->total_line_height += 
					nautilus_glyph_get_height (node->data);
			} else {
				smooth_text_layout->details->total_line_height += 
					smooth_text_layout_get_empty_line_height (smooth_text_layout);
			}

			node = node->next;
		}
	}

	return smooth_text_layout->details->total_line_height;
}

static int
smooth_text_layout_get_line_wrap_width (const NautilusSmoothTextLayout *smooth_text_layout)
{
	g_return_val_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout), 0);
	
	if (smooth_text_layout->details->line_wrap_width == UNDEFINED_DIMENSION) {
		smooth_text_layout->details->line_wrap_width = 
			nautilus_scalable_font_text_width (smooth_text_layout->details->font,
							   smooth_text_layout->details->font_size,
							   DEFAULT_LINE_WRAP_WIDTH_TEXT,
							   strlen (DEFAULT_LINE_WRAP_WIDTH_TEXT));
		
	}

	return smooth_text_layout->details->line_wrap_width;
}

/* Public NautilusSmoothTextLayout methods */

/**
 * nautilus_smooth_text_layout_new:
 * @family: The desired smooth_text_layout family.
 * @weight: The desired smooth_text_layout weight.
 * @slant: The desired smooth_text_layout slant.
 * @set_width: The desired smooth_text_layout set_width.
 *
 * Returns a new smooth_text_layout.
 *
 */
NautilusSmoothTextLayout *
nautilus_smooth_text_layout_new (const char *text,
				 int text_length,
				 NautilusScalableFont *font,
				 int font_size,
				 gboolean wrap)
{
	NautilusSmoothTextLayout *smooth_text_layout;

	g_return_val_if_fail (NAUTILUS_IS_SCALABLE_FONT (font), NULL);
	g_return_val_if_fail (font_size > MIN_FONT_SIZE, NULL);

	smooth_text_layout = NAUTILUS_SMOOTH_TEXT_LAYOUT (gtk_object_new (nautilus_smooth_text_layout_get_type (), NULL));
	gtk_object_ref (GTK_OBJECT (smooth_text_layout));
	gtk_object_sink (GTK_OBJECT (smooth_text_layout));

	smooth_text_layout_set_text (smooth_text_layout, text, text_length);
	nautilus_smooth_text_layout_set_font (smooth_text_layout, font);
	nautilus_smooth_text_layout_set_font_size (smooth_text_layout, font_size);
	nautilus_smooth_text_layout_set_wrap (smooth_text_layout, wrap);
	
	return smooth_text_layout;
}

void
nautilus_smooth_text_layout_draw_to_pixbuf (const NautilusSmoothTextLayout *smooth_text_layout,
					    GdkPixbuf *destination_pixbuf,
					    int source_x,
					    int source_y,
					    const ArtIRect *destination_area,
					    GtkJustification justification,
					    gboolean underlined,
					    guint32 color,
					    int opacity)
{
	NautilusDimensions dimensions;
	ArtIRect target;
	ArtIRect source;
	int target_width;
	int target_height;
	int source_width;
	int source_height;
	GdkPixbuf *sub_pixbuf;

	g_return_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout));
	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (destination_pixbuf));
	g_return_if_fail (destination_area != NULL);
	g_return_if_fail (justification >= GTK_JUSTIFY_LEFT && justification <= GTK_JUSTIFY_FILL);
	g_return_if_fail (!art_irect_empty (destination_area));

	smooth_text_layout_ensure_lines (smooth_text_layout);
	
	dimensions = nautilus_smooth_text_layout_get_dimensions (smooth_text_layout);

	g_return_if_fail (source_x >= 0);
	g_return_if_fail (source_y >= 0);
	g_return_if_fail (source_x < dimensions.width);
	g_return_if_fail (source_y < dimensions.height);

	/* Clip the destination area to the pixbuf dimensions; bail if no work */
	target = nautilus_gdk_pixbuf_intersect (destination_pixbuf, 0, 0, destination_area);
	if (art_irect_empty (&target)) {
 		return;
 	}

	/* Assign the source area */
	nautilus_art_irect_assign (&source,
				   source_x,
				   source_y,
				   dimensions.width - source_x,
				   dimensions.height - source_y);

	/* Adjust the target width if the source area is smaller than the
	 * source pixbuf dimensions */
	target_width = target.x1 - target.x0;
	target_height = target.y1 - target.y0;
	source_width = source.x1 - source.x0;
	source_height = source.y1 - source.y0;

	target.x1 = target.x0 + MIN (target_width, source_width);
	target.y1 = target.y0 + MIN (target_height, source_height);

	/* Use a sub area pixbuf for simplicity */
	sub_pixbuf = nautilus_gdk_pixbuf_new_from_pixbuf_sub_area (destination_pixbuf, &target);

	smooth_text_layout_line_list_draw_to_pixbuf (smooth_text_layout->details->text_line_list,
						     sub_pixbuf,
						     -source_x,
						     -source_y,
						     justification,
						     underlined,
						     smooth_text_layout_get_empty_line_height (smooth_text_layout),
						     smooth_text_layout_get_max_line_width (smooth_text_layout),
						     smooth_text_layout->details->line_spacing,
						     color,
						     opacity);

	gdk_pixbuf_unref (sub_pixbuf);
}

void
nautilus_smooth_text_layout_draw_to_pixbuf_shadow (const NautilusSmoothTextLayout *smooth_text_layout,
						   GdkPixbuf *destination_pixbuf,
						   int source_x,
						   int source_y,
						   const ArtIRect *destination_area,
						   int shadow_offset,
						   GtkJustification justification,
						   gboolean underlined,
						   guint32 color,
						   guint32 shadow_color,
						   int opacity)
{
	NautilusDimensions dimensions;
	ArtIRect target;
	ArtIRect source;
	int target_width;
	int target_height;
	int source_width;
	int source_height;
	GdkPixbuf *sub_pixbuf;

	g_return_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout));
	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (destination_pixbuf));
	g_return_if_fail (destination_area != NULL);
	g_return_if_fail (shadow_offset > 0);
	g_return_if_fail (justification >= GTK_JUSTIFY_LEFT && justification <= GTK_JUSTIFY_FILL);
	g_return_if_fail (!art_irect_empty (destination_area));

	smooth_text_layout_ensure_lines (smooth_text_layout);
	
	dimensions = nautilus_smooth_text_layout_get_dimensions (smooth_text_layout);
	dimensions.width += shadow_offset;
	dimensions.height += shadow_offset;

	g_return_if_fail (source_x >= 0);
	g_return_if_fail (source_y >= 0);
	g_return_if_fail (source_x < dimensions.width);
	g_return_if_fail (source_y < dimensions.height);

	/* Clip the destination area to the pixbuf dimensions; bail if no work */
	target = nautilus_gdk_pixbuf_intersect (destination_pixbuf, 0, 0, destination_area);
	if (art_irect_empty (&target)) {
 		return;
 	}

	/* Assign the source area */
	nautilus_art_irect_assign (&source,
				   source_x,
				   source_y,
				   dimensions.width - source_x,
				   dimensions.height - source_y);

	/* Adjust the target width if the source area is smaller than the
	 * source pixbuf dimensions */
	target_width = target.x1 - target.x0;
	target_height = target.y1 - target.y0;
	source_width = source.x1 - source.x0;
	source_height = source.y1 - source.y0;

	target.x1 = target.x0 + MIN (target_width, source_width);
	target.y1 = target.y0 + MIN (target_height, source_height);

	/* Use a sub area pixbuf for simplicity */
	sub_pixbuf = nautilus_gdk_pixbuf_new_from_pixbuf_sub_area (destination_pixbuf, &target);

	/* Draw the shadow text */
	smooth_text_layout_line_list_draw_to_pixbuf (smooth_text_layout->details->text_line_list,
						     sub_pixbuf,
						     -source_x + shadow_offset,
						     -source_y + shadow_offset,
						     justification,
						     underlined,
						     smooth_text_layout_get_empty_line_height (smooth_text_layout),
						     smooth_text_layout_get_max_line_width (smooth_text_layout),
						     smooth_text_layout->details->line_spacing,
						     shadow_color,
						     opacity);

	/* Draw the text */
	smooth_text_layout_line_list_draw_to_pixbuf (smooth_text_layout->details->text_line_list,
						     sub_pixbuf,
						     -source_x,
						     -source_y,
						     justification,
						     underlined,
						     smooth_text_layout_get_empty_line_height (smooth_text_layout),
						     smooth_text_layout_get_max_line_width (smooth_text_layout),
						     smooth_text_layout->details->line_spacing,
						     color,
						     opacity);

	gdk_pixbuf_unref (sub_pixbuf);
}

NautilusDimensions
nautilus_smooth_text_layout_get_dimensions (const NautilusSmoothTextLayout *smooth_text_layout)
{
	g_return_val_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout), NAUTILUS_DIMENSIONS_EMPTY);

	smooth_text_layout_ensure_lines (smooth_text_layout);

	if (smooth_text_layout->details->dimensions.width == UNDEFINED_DIMENSION) {
		const int max_line_width = smooth_text_layout_get_max_line_width (smooth_text_layout);
		const int num_lines = g_list_length (smooth_text_layout->details->text_line_list);
		const int num_empty_lines = smooth_text_layout_get_num_empty_lines (smooth_text_layout);
		const int total_line_height = smooth_text_layout_get_total_line_height (smooth_text_layout);

		g_assert (num_lines >= 0);
		g_assert (num_empty_lines >= 0);
		g_assert (num_lines >= num_empty_lines);
		
		smooth_text_layout->details->dimensions.width = max_line_width;
		smooth_text_layout->details->dimensions.height = total_line_height;
		
		if (num_lines > 1) {
			smooth_text_layout->details->dimensions.height += 
				(num_lines - 1) * smooth_text_layout->details->line_spacing;
		}
	}

	return smooth_text_layout->details->dimensions;
}

int
nautilus_smooth_text_layout_get_width (const NautilusSmoothTextLayout *smooth_text_layout)
{
	NautilusDimensions dimensions;

	g_return_val_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout), 0);

	dimensions = nautilus_smooth_text_layout_get_dimensions (smooth_text_layout);

	return dimensions.width;
}

int
nautilus_smooth_text_layout_get_height (const NautilusSmoothTextLayout *smooth_text_layout)
{
	NautilusDimensions dimensions;

	g_return_val_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout), 0);

	dimensions = nautilus_smooth_text_layout_get_dimensions (smooth_text_layout);

	return dimensions.height;
}

void
nautilus_smooth_text_layout_set_wrap (NautilusSmoothTextLayout *smooth_text_layout,
				      gboolean wrap)
{
	g_return_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout));

	if (smooth_text_layout->details->wrap == wrap) {
		return;
	}

	smooth_text_layout_clear_lines (smooth_text_layout);
	smooth_text_layout->details->wrap = wrap;
}

gboolean
nautilus_smooth_text_layout_get_wrap (const NautilusSmoothTextLayout *smooth_text_layout)
{
	g_return_val_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout), FALSE);

	return smooth_text_layout->details->wrap;
}

void
nautilus_smooth_text_layout_set_font (NautilusSmoothTextLayout *smooth_text_layout,
				      NautilusScalableFont *font)
{
	g_return_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout));
	g_return_if_fail (NAUTILUS_IS_SCALABLE_FONT (font));

	if (smooth_text_layout->details->font == font) {
		return;
	}

	smooth_text_layout_clear_lines (smooth_text_layout);
	gtk_object_unref (GTK_OBJECT (smooth_text_layout->details->font));
	gtk_object_ref (GTK_OBJECT (font));
	smooth_text_layout->details->font = font;
}

NautilusScalableFont *
nautilus_smooth_text_layout_get_font (const NautilusSmoothTextLayout *smooth_text_layout)
{
	g_return_val_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout), NULL);

	gtk_object_ref (GTK_OBJECT (smooth_text_layout->details->font));
	return smooth_text_layout->details->font;
}

void
nautilus_smooth_text_layout_set_font_size (NautilusSmoothTextLayout *smooth_text_layout,
					   int font_size)
{
	g_return_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout));
	g_return_if_fail (font_size > MIN_FONT_SIZE);

	if (smooth_text_layout->details->font_size == font_size) {
		return;
	}
	
	smooth_text_layout_clear_lines (smooth_text_layout);
	smooth_text_layout->details->font_size = font_size;
}

int
nautilus_smooth_text_layout_get_font_size (const NautilusSmoothTextLayout *smooth_text_layout)
{
	g_return_val_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout), 0);

	return smooth_text_layout->details->font_size;
}

void
nautilus_smooth_text_layout_set_line_spacing (NautilusSmoothTextLayout *smooth_text_layout,
					      int line_spacing)
{
	g_return_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout));
	
	if (smooth_text_layout->details->line_spacing == line_spacing) {
		return;
	}
	
	smooth_text_layout_clear_lines (smooth_text_layout);
	smooth_text_layout->details->line_spacing = line_spacing;
}

int
nautilus_smooth_text_layout_get_line_spacing (const NautilusSmoothTextLayout *smooth_text_layout)
{
	g_return_val_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout), 0);

	return smooth_text_layout->details->line_spacing;
}

void
nautilus_smooth_text_layout_set_empty_line_height (NautilusSmoothTextLayout *smooth_text_layout,
						   int empty_line_height)
{
	g_return_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout));
	
	if (smooth_text_layout->details->empty_line_height == empty_line_height) {
		return;
	}
	
	smooth_text_layout_clear_lines (smooth_text_layout);
	smooth_text_layout->details->empty_line_height = empty_line_height;
}

int
nautilus_smooth_text_layout_get_empty_line_height (const NautilusSmoothTextLayout *smooth_text_layout)
{
	g_return_val_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout), 0);

	return smooth_text_layout->details->empty_line_height;
}

static void
smooth_text_layout_set_text (NautilusSmoothTextLayout *smooth_text_layout,
			     const char *text,
			     int text_length)
{
	g_return_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout));

	if (smooth_text_layout->details->text == text
	    && smooth_text_layout->details->text_length == text_length) {
		return;
	}
	
	smooth_text_layout_clear_lines (smooth_text_layout);
	g_free (smooth_text_layout->details->text);
	smooth_text_layout->details->text = g_strdup (text);
	smooth_text_layout->details->text_length = text_length;
}

void
nautilus_smooth_text_layout_set_line_break_characters (NautilusSmoothTextLayout *smooth_text_layout,
						       const char *line_break_characters)
{
	g_return_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout));
	g_return_if_fail (nautilus_strlen (line_break_characters) > 0);

	if (nautilus_str_is_equal (smooth_text_layout->details->line_break_characters, line_break_characters)) {
		return;
	}

	smooth_text_layout_clear_lines (smooth_text_layout);
	g_free (smooth_text_layout->details->line_break_characters);
	smooth_text_layout->details->line_break_characters = g_strdup (line_break_characters);
}

char *
nautilus_smooth_text_layout_get_line_break_characters (const NautilusSmoothTextLayout *smooth_text_layout)
{
	g_return_val_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout), FALSE);

	return g_strdup (smooth_text_layout->details->text);
}

void
nautilus_smooth_text_layout_set_line_wrap_width (NautilusSmoothTextLayout *smooth_text_layout,
						 int line_wrap_width)
{
	g_return_if_fail (NAUTILUS_IS_SMOOTH_TEXT_LAYOUT (smooth_text_layout));

	if (smooth_text_layout->details->line_wrap_width == line_wrap_width) {
		return;
	}

	smooth_text_layout_clear_lines (smooth_text_layout);
	smooth_text_layout->details->line_wrap_width = line_wrap_width;
}

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

	text_layout = g_new0 (NautilusTextLayout, 1);

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
						
						row = g_new0 (NautilusTextLayoutRow, 1);
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

			row = g_new0 (NautilusTextLayoutRow, 1);
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
