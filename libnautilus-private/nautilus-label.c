/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-label.c - A widget to display a anti aliased text.

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
#include "nautilus-label.h"

#include <libgnome/gnome-i18n.h>
#include "nautilus-gtk-macros.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-string.h"

/* Arguments */
enum
{
	ARG_0,
	ARG_BACKGROUND_COLOR,
	ARG_BACKGROUND_TYPE,
	ARG_LABEL,
	ARG_PLACEMENT_TYPE,
};

/* Detail member struct */
struct _NautilusLabelDetail
{
	/* Text */
	char			*text;
	guint32			text_color;
	guchar			text_alpha;
	guint			text_width;
	guint			text_height;
	GtkJustification	text_justification;
	guint			line_offset;

	/* Drop shadow */
	guint			drop_shadow_offset;
	guint32			drop_shadow_color;

	/* Font */
	NautilusScalableFont	*font;
	guint			font_size;

	/* Text lines */
	guint			*text_line_widths;
	guint			*text_line_heights;
	guint			num_text_lines;
	guint			max_text_line_width;
	guint			total_text_line_height;

	/* Line wrapping */
	gboolean		line_wrap;
	guint			line_wrap_width;
	char 			*line_wrap_separators;
	NautilusTextLayout	**text_layouts;
};

/* GtkObjectClass methods */
static void  nautilus_label_initialize_class             (NautilusLabelClass     *label_class);
static void  nautilus_label_initialize                   (NautilusLabel          *label);
static void  nautilus_label_destroy                      (GtkObject              *object);
static void  nautilus_label_set_arg                      (GtkObject              *object,
							  GtkArg                 *arg,
							  guint                   arg_id);
static void  nautilus_label_get_arg                      (GtkObject              *object,
							  GtkArg                 *arg,
							  guint                   arg_id);
/* GtkWidgetClass methods */
static void  nautilus_label_size_request                 (GtkWidget              *widget,
							  GtkRequisition         *requisition);
static void  nautilus_label_size_allocate                (GtkWidget              *widget,
							  GtkAllocation          *allocation);
/* NautilusBufferedWidgetClass methods */
static void  render_buffer_pixbuf                        (NautilusBufferedWidget *buffered_widget,
							  GdkPixbuf              *buffer,
							  int                     horizontal_offset,
							  int                     vertical_offset);

/* Private NautilusLabel things */
static void  label_recompute_line_geometries             (NautilusLabel          *label);
static guint label_get_empty_line_height                 (NautilusLabel          *label);
static guint label_get_total_text_and_line_offset_height (NautilusLabel          *label);



NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusLabel, nautilus_label, NAUTILUS_TYPE_BUFFERED_WIDGET)

/* Class init methods */
static void
nautilus_label_initialize_class (NautilusLabelClass *label_class)
{
	GtkObjectClass			*object_class = GTK_OBJECT_CLASS (label_class);
	GtkWidgetClass			*widget_class = GTK_WIDGET_CLASS (label_class);
	NautilusBufferedWidgetClass	*buffered_widget_class = NAUTILUS_BUFFERED_WIDGET_CLASS (label_class);

#if 0
	/* Arguments */
	gtk_object_add_arg_type ("NautilusLabel::placement_type",
				 GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE,
				 ARG_PLACEMENT_TYPE);

	gtk_object_add_arg_type ("NautilusLabel::background_type",
				 GTK_TYPE_ENUM,
				 GTK_ARG_READWRITE,
				 ARG_BACKGROUND_TYPE);

	gtk_object_add_arg_type ("NautilusLabel::background_color",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_BACKGROUND_COLOR);

	gtk_object_add_arg_type ("NautilusLabel::label",
				 GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE,
				 ARG_LABEL);
#endif

	/* GtkObjectClass */
	object_class->destroy = nautilus_label_destroy;
	object_class->set_arg = nautilus_label_set_arg;
	object_class->get_arg = nautilus_label_get_arg;

	/* GtkWidgetClass */
	widget_class->size_request = nautilus_label_size_request;
	widget_class->size_allocate = nautilus_label_size_allocate;

	/* NautilusBufferedWidgetClass */
	buffered_widget_class->render_buffer_pixbuf = render_buffer_pixbuf;
}

void
nautilus_label_initialize (NautilusLabel *label)
{
	label->detail = g_new (NautilusLabelDetail, 1);

	label->detail->text = NULL;

	label->detail->font = nautilus_scalable_font_get_default_font ();
	label->detail->font_size = 12;

	label->detail->text_color = NAUTILUS_RGBA_COLOR_PACK (0, 0, 0, 255);
	label->detail->drop_shadow_color = NAUTILUS_RGBA_COLOR_PACK (255, 255, 255, 255);
	label->detail->text_alpha = 255;
	label->detail->text_width = 0;
	label->detail->text_height = 0;
	label->detail->text_justification = GTK_JUSTIFY_CENTER;

	label->detail->num_text_lines = 0;
	label->detail->max_text_line_width = 0;
	label->detail->total_text_line_height = 0;
	label->detail->text_line_widths = NULL;
	label->detail->text_line_heights = NULL;

	label->detail->line_offset = 2;
	label->detail->drop_shadow_offset = 0;

	label->detail->line_wrap = FALSE;
	label->detail->line_wrap_width = 400;
	label->detail->line_wrap_separators = g_strdup (_(" -_,;.?/&"));
	label->detail->text_layouts = NULL;
}

/* GtkObjectClass methods */
static void
nautilus_label_destroy (GtkObject *object)
{
 	NautilusLabel *label;
	
	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_LABEL (object));

	label = NAUTILUS_LABEL (object);

	g_free (label->detail->text);

	g_free (label->detail->text_line_widths);
	g_free (label->detail->text_line_heights);

	if (label->detail->text_layouts != NULL) {
		guint i;

		for (i = 0; i < label->detail->num_text_lines; i++) {
			g_assert (label->detail->text_layouts[i] != NULL);
			nautilus_text_layout_free (label->detail->text_layouts[i]);
		}

		g_free (label->detail->text_layouts);
	}

	g_free (label->detail);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_label_set_arg (GtkObject	*object,
			GtkArg		*arg,
			guint		arg_id)
{
	NautilusLabel		*label;

 	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_LABEL (object));

 	label = NAUTILUS_LABEL (object);

#if 0
 	switch (arg_id)
	{
	case ARG_PLACEMENT_TYPE:
		label->detail->placement_type = GTK_VALUE_ENUM (*arg);
		break;

	case ARG_BACKGROUND_TYPE:
		label->detail->background_type = GTK_VALUE_ENUM (*arg);
		break;

	case ARG_BACKGROUND_COLOR:
		label->detail->background_color = GTK_VALUE_UINT (*arg);
		break;

	case ARG_LABEL:
		nautilus_label_set_pixbuf (label, (GdkPixbuf*) GTK_VALUE_OBJECT (*arg));
		break;

 	default:
		g_assert_not_reached ();
	}
#endif
}

static void
nautilus_label_get_arg (GtkObject	*object,
			GtkArg		*arg,
			guint		arg_id)
{
	NautilusLabel	*label;

	g_return_if_fail (object != NULL);
	g_return_if_fail (NAUTILUS_IS_LABEL (object));
	
	label = NAUTILUS_LABEL (object);

#if 0
 	switch (arg_id)
	{
	case ARG_PLACEMENT_TYPE:
		GTK_VALUE_ENUM (*arg) = label->detail->placement_type;
		break;
		
	case ARG_BACKGROUND_TYPE:
		GTK_VALUE_ENUM (*arg) = label->detail->background_type;
		break;

	case ARG_BACKGROUND_COLOR:
		GTK_VALUE_UINT (*arg) = label->detail->background_color;
		break;

	case ARG_LABEL:
		GTK_VALUE_OBJECT (*arg) = (GtkObject *) nautilus_label_get_pixbuf (label);
		break;

 	default:
		g_assert_not_reached ();
	}
#endif
}

/* GtkWidgetClass methods */
static void
nautilus_label_size_request (GtkWidget		*widget,
			     GtkRequisition	*requisition)
{
	GtkMisc			*misc;
	NautilusLabel		*label;
	guint			text_width = 0;
	guint			text_height = 0;
	NautilusPixbufSize	tile_size;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (NAUTILUS_IS_LABEL (widget));
	g_return_if_fail (requisition != NULL);

	label = NAUTILUS_LABEL (widget);
	misc = GTK_MISC (widget);

	tile_size = nautilus_buffered_get_tile_pixbuf_size (NAUTILUS_BUFFERED_WIDGET (label));

	if (label->detail->num_text_lines > 0) {
		text_width = label->detail->max_text_line_width;
		text_height = label_get_total_text_and_line_offset_height (label);

		text_width += label->detail->drop_shadow_offset;
		text_height += label->detail->drop_shadow_offset;
	}

   	requisition->width = MAX (2, text_width);
   	requisition->height = MAX (2, MAX (text_height, tile_size.height));

    	requisition->width += (misc->xpad * 2);
    	requisition->height += (misc->ypad * 2);
}

/* recompute the line layout if it's dependent on the size */
static void
nautilus_label_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
	NautilusLabel *label;

	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));
	
	label = NAUTILUS_LABEL (widget);

	/* FIXME bugzilla.eazel.com 5083: 
	 * this never happens
	 */
	if ((int) label->detail->line_wrap_width == -1) {
		label_recompute_line_geometries (label);
	}
}

/* NautilusBufferedWidgetClass methods */
static void
render_buffer_pixbuf (NautilusBufferedWidget	*buffered_widget,
		      GdkPixbuf			*buffer,
		      int			horizontal_offset,
		      int			vertical_offset)
{
	NautilusLabel *label;
	GtkWidget *widget;
	ArtIRect clip_area;
	int text_x;
	int text_y;

	g_return_if_fail (NAUTILUS_IS_LABEL (buffered_widget));
	g_return_if_fail (buffer != NULL);

	label = NAUTILUS_LABEL (buffered_widget);

	if (label->detail->num_text_lines == 0) {
		return;
	}

	widget = GTK_WIDGET (buffered_widget);
	
	if (label->detail->max_text_line_width <= widget->allocation.width) {
		clip_area.x0 = ((int) widget->allocation.width - (int) label->detail->max_text_line_width) / 2;
	}
	else {
		clip_area.x0 = - ((int) label->detail->max_text_line_width - (int) widget->allocation.width) / 2;
	}
	
	if (label->detail->total_text_line_height <= widget->allocation.height) {
		clip_area.y0 = ((int) widget->allocation.height - (int) label->detail->total_text_line_height) / 2;
	}
	else {
		clip_area.y0 = - ((int) label->detail->total_text_line_height - (int) widget->allocation.height) / 2;
	}

	clip_area.x0 = 0;
	clip_area.y0 = 0;

	clip_area.x1 = widget->allocation.width;
	clip_area.y1 = widget->allocation.height;

	text_x = horizontal_offset;
	text_y = vertical_offset;

	if (label->detail->num_text_lines == 0) {
		return;
	}

	/* Line wrapping */
	if (label->detail->line_wrap) {
		guint i;
		guint x = text_x;
		guint y = text_y;
		
		for (i = 0; i < label->detail->num_text_lines; i++) {
			const NautilusTextLayout *text_layout = label->detail->text_layouts[i];

			if (label->detail->drop_shadow_offset > 0) {
				nautilus_text_layout_paint (text_layout, 
							    buffer, 
							    x + label->detail->drop_shadow_offset, 
							    y + label->detail->drop_shadow_offset,
							    label->detail->text_justification,
							    label->detail->drop_shadow_color,
							    FALSE);
			}

			nautilus_text_layout_paint (text_layout, 
						    buffer, 
						    x, 
						    y,
						    label->detail->text_justification,
						    label->detail->text_color,
						    FALSE);

			y += text_layout->height;
		}
	}
	/* No line wrapping */
	else {
		if (label->detail->drop_shadow_offset > 0) {
			nautilus_scalable_font_draw_text_lines_with_dimensions (label->detail->font,
										buffer,
										text_x + label->detail->drop_shadow_offset,
										text_y + label->detail->drop_shadow_offset,
										&clip_area,
										label->detail->font_size,
										label->detail->font_size,
										label->detail->text,
										label->detail->num_text_lines,
										label->detail->text_line_widths,
										label->detail->text_line_heights,
										label->detail->text_justification,
										label->detail->line_offset,
										label_get_empty_line_height (label),
										label->detail->drop_shadow_color,
										label->detail->text_alpha);
		}

		nautilus_scalable_font_draw_text_lines_with_dimensions (label->detail->font,
									buffer,
									text_x,
									text_y,
									&clip_area,
									label->detail->font_size,
									label->detail->font_size,
									label->detail->text,
									label->detail->num_text_lines,
									label->detail->text_line_widths,
									label->detail->text_line_heights,
									label->detail->text_justification,
									label->detail->line_offset,
									label_get_empty_line_height (label),
									label->detail->text_color,
									label->detail->text_alpha);
	}
}

/* Private NautilusLabel things */
static guint
label_get_empty_line_height (NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	/* If we wanted to crunch lines together, we could add a multiplier
	 * here.  For now we just use the font size for empty lines. */
	return label->detail->font_size;
}

static guint
label_get_total_text_and_line_offset_height (NautilusLabel *label)
{
	guint total_height;

	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	total_height = label->detail->total_text_line_height;
	
	if (label->detail->num_text_lines > 1) {
		total_height += ((label->detail->num_text_lines - 1) * label->detail->line_offset);
	}

	return total_height;
}

/* utility routine to get the allocation width of the passed in width, as constrained by the width of its parents */
static int
get_clipped_width (GtkWidget *widget)
{
	GtkWidget *current_container;
	int clipped_width, container_offset, effective_width;
	
	clipped_width = widget->allocation.width;
	container_offset = widget->allocation.x;
	
	current_container = widget->parent;
	while (current_container != NULL) {
		effective_width = current_container->allocation.width - container_offset;
		if (effective_width < clipped_width) {
			clipped_width = effective_width;
		}
		container_offset = current_container->allocation.x;
		current_container = current_container->parent;
	}	
	return clipped_width;
}

static void
label_recompute_line_geometries (NautilusLabel *label)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	g_free (label->detail->text_line_widths);
	g_free (label->detail->text_line_heights);
	label->detail->text_line_widths = NULL;
	label->detail->text_line_heights = NULL;

	if (label->detail->text_layouts != NULL) {
		guint i;

		for (i = 0; i < label->detail->num_text_lines; i++) {
			g_assert (label->detail->text_layouts[i] != NULL);
			nautilus_text_layout_free (label->detail->text_layouts[i]);
		}

		g_free (label->detail->text_layouts);
		label->detail->text_layouts = NULL;
	}
	
	label->detail->num_text_lines = 0;

	label->detail->max_text_line_width = 0;
	label->detail->total_text_line_height = 0;

	if (nautilus_strlen (label->detail->text) == 0) {
		return;
	}
	
	label->detail->num_text_lines = nautilus_str_count_characters (label->detail->text, '\n') + 1;

	/* Line wrapping */
	if (label->detail->line_wrap) {
		char **pieces;
		guint i;
		int clipped_widget_width, wrap_width;
		
		label->detail->text_layouts = g_new (NautilusTextLayout *, label->detail->num_text_lines);
		clipped_widget_width = get_clipped_width (GTK_WIDGET (label));
		
		pieces = g_strsplit (label->detail->text, "\n", 0);

		for (i = 0; pieces[i] != NULL; i++) {
			char *text_piece = pieces[i];

			g_assert (i < label->detail->num_text_lines);

			/* Make empty lines appear.  A single '\n' for example. */
			if (text_piece[0] == '\0') {
				text_piece = " ";
			}

			/* determine the width to use for wrapping.  A wrap width of -1 means use all of the available space. */
			/* Don't use it if the widget is too small, since we won't be able to fit any words in */
			/* FIXME bugzilla.eazel.com 5083:
			 * the -1 case never happens
			 */ 
			if ((int) label->detail->line_wrap_width == -1 && clipped_widget_width > 32) {
				wrap_width = clipped_widget_width;
			} else {
				wrap_width = label->detail->line_wrap_width;
			}
			label->detail->text_layouts[i] = nautilus_text_layout_new (label->detail->font,
										   label->detail->font_size,
										   text_piece,
										   label->detail->line_wrap_separators,
										   wrap_width, 
										   TRUE);

			label->detail->total_text_line_height += label->detail->text_layouts[i]->height;
			
			if (label->detail->text_layouts[i]->width > (int) label->detail->max_text_line_width) {
				label->detail->max_text_line_width = label->detail->text_layouts[i]->width;
			}
		}

		g_strfreev (pieces);
	}
	/* No line wrapping */
	else {
		label->detail->text_line_widths = g_new (guint, label->detail->num_text_lines);
		label->detail->text_line_heights = g_new (guint, label->detail->num_text_lines);
		
		nautilus_scalable_font_measure_text_lines (label->detail->font,
							   label->detail->font_size,
							   label->detail->font_size,
							   label->detail->text,
							   label->detail->num_text_lines,
							   label_get_empty_line_height (label),
							   label->detail->text_line_widths,
							   label->detail->text_line_heights,
							   &label->detail->max_text_line_width,
							   &label->detail->total_text_line_height);
	}
}

/* Public NautilusLabel */
GtkWidget *
nautilus_label_new (const char *text)
{
	NautilusLabel *label;
	
	label = NAUTILUS_LABEL (gtk_widget_new (nautilus_label_get_type (), NULL));

	nautilus_label_set_text (label, text);
	
	return GTK_WIDGET (label);
}

void
nautilus_label_set_text (NautilusLabel	*label,
			 const gchar	*text)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	if (nautilus_str_is_equal (text, label->detail->text)) {
		return;
	}
	
	g_free (label->detail->text);
	label->detail->text = text ? g_strdup (text) : NULL;
	
	label_recompute_line_geometries (label);

	gtk_widget_queue_resize (GTK_WIDGET (label));
}

gchar*
nautilus_label_get_text (NautilusLabel *label)
{
	g_return_val_if_fail (label != NULL, NULL);
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), NULL);

	return label->detail->text ? g_strdup (label->detail->text) : NULL;
}

void
nautilus_label_set_font (NautilusLabel		*label,
			 NautilusScalableFont	*font)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (NAUTILUS_IS_SCALABLE_FONT (font));
	g_return_if_fail (font != NULL);

	if (label->detail->font == font) {
		return;
	}

	if (label->detail->font != NULL) {
		gtk_object_unref (GTK_OBJECT (label->detail->font));
		label->detail->font = NULL;
	}

	gtk_object_ref (GTK_OBJECT (font));

	label->detail->font = font;

	label_recompute_line_geometries (label);
	
	gtk_widget_queue_resize (GTK_WIDGET (label));
}

void
nautilus_label_set_font_from_components (NautilusLabel	*label,
					 const char	*family,
					 const char	*weight,
					 const char	*slant,
					 const char	*set_width)
{
	NautilusScalableFont *font;

	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (family != NULL);
	
	font = NAUTILUS_SCALABLE_FONT (nautilus_scalable_font_new (family, weight, slant, set_width));

	if (font != NULL) {
		nautilus_label_set_font (label, font);
		gtk_object_unref (GTK_OBJECT (font));
	}
}

NautilusScalableFont *
nautilus_label_get_font (const NautilusLabel *label)
{
	g_return_val_if_fail (label != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	if (label->detail->font != NULL) {
		gtk_object_ref (GTK_OBJECT (label->detail->font));
	}

	return label->detail->font;
}

void
nautilus_label_set_font_size (NautilusLabel	*label,
			      guint		font_size)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (font_size > 0);

	if (label->detail->font_size == font_size) {
		return;
	}

	label->detail->font_size = font_size;

	label_recompute_line_geometries (label);

	gtk_widget_queue_resize (GTK_WIDGET (label));
}

guint
nautilus_label_get_font_size (const NautilusLabel *label)
{
	g_return_val_if_fail (label != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	return label->detail->font_size;
}

void
nautilus_label_set_text_color (NautilusLabel	*label,
			       guint32		text_color)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	if (label->detail->text_color == text_color) {
		return;
	}

	label->detail->text_color = text_color;
	
	nautilus_buffered_widget_clear_buffer (NAUTILUS_BUFFERED_WIDGET (label));
	
	gtk_widget_queue_draw (GTK_WIDGET (label));
}

guint32
nautilus_label_get_text_color (const NautilusLabel *label)
{
	g_return_val_if_fail (label != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	return label->detail->text_color;
}

void
nautilus_label_set_text_alpha (NautilusLabel	*label,
			       guchar		text_alpha)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	if (label->detail->text_alpha == text_alpha) {
		return;
	}

	label->detail->text_alpha = text_alpha;

	nautilus_buffered_widget_clear_buffer (NAUTILUS_BUFFERED_WIDGET (label));
	
	gtk_widget_queue_draw (GTK_WIDGET (label));
}

guchar
nautilus_label_get_text_alpha (const NautilusLabel *label)
{
	g_return_val_if_fail (label != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	return label->detail->text_alpha;
}

void
nautilus_label_set_text_justification (NautilusLabel	*label,
				       GtkJustification	justification)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (justification >= GTK_JUSTIFY_LEFT && justification <= GTK_JUSTIFY_FILL);

	if (label->detail->text_justification == justification) {
		return;
	}

	label->detail->text_justification = justification;

	nautilus_buffered_widget_clear_buffer (NAUTILUS_BUFFERED_WIDGET (label));
	
	gtk_widget_queue_draw (GTK_WIDGET (label));
}

GtkJustification
nautilus_label_get_text_justification (const NautilusLabel *label)
{
	g_return_val_if_fail (label != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	return label->detail->text_justification;
}

/**
 * nautilus_label_set_line_offset:
 *
 * @label: A NautilusLabel
 * @line_offset: The new line offset offset in pixels.
 *
 * Change the line offset.  Obviously, this is only interesting if the 
 * label is displaying text that contains '\n' characters.
 * 
 */
void
nautilus_label_set_line_offset (NautilusLabel	*label,
				guint		line_offset)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	if (label->detail->line_offset == line_offset) {
		return;
	}

	label->detail->line_offset = line_offset;
	
	gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * nautilus_label_get_line_offset:
 *
 * @label: A NautilusLabel
 *
 * Return value: The line offset in pixels.
 */
guint
nautilus_label_get_line_offset (const NautilusLabel *label)
{
	g_return_val_if_fail (label != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	return label->detail->line_offset;
}

/**
 * nautilus_label_set_drop_shadow_offset:
 *
 * @label: A NautilusLabel
 * @drop_shadow_offset: The new drop shadow offset.  

 * The drop shadow offset is specified in pixels.  If greater than zero,
 * the label will render on top of a nice shadow.  The shadow will be
 * offset from the label text by 'drop_shadow_offset' pixels.
 */
void
nautilus_label_set_drop_shadow_offset (NautilusLabel	*label,
				       guint		drop_shadow_offset)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	if (label->detail->drop_shadow_offset == drop_shadow_offset) {
		return;
	}

	label->detail->drop_shadow_offset = drop_shadow_offset;
	
	gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * nautilus_label_get_drop_shadow_offset:
 *
 * @label: A NautilusLabel
 *
 * Return value: The line offset in pixels.
 */
guint
nautilus_label_get_drop_shadow_offset (const NautilusLabel *label)
{
	g_return_val_if_fail (label != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	return label->detail->drop_shadow_offset;
}

/**
 * nautilus_label_set_drop_shadow_color:
 *
 * @label: A NautilusLabel
 * @drop_shadow_color: The new drop shadow color.
 *
 * Return value: The drop shadow color.
 */
void
nautilus_label_set_drop_shadow_color (NautilusLabel	*label,
				      guint32		drop_shadow_color)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	if (label->detail->drop_shadow_color == drop_shadow_color) {
		return;
	}
	
	label->detail->drop_shadow_color = drop_shadow_color;
	
	nautilus_buffered_widget_clear_buffer (NAUTILUS_BUFFERED_WIDGET (label));
	
	gtk_widget_queue_draw (GTK_WIDGET (label));
}

/**
 * nautilus_label_get_drop_shadow_color:
 *
 * @label: A NautilusLabel
 *
 * Return value: The drop shadow color.
 */
guint32
nautilus_label_get_drop_shadow_color (const NautilusLabel *label)
{
	g_return_val_if_fail (label != NULL, 0);
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);
	
	return label->detail->drop_shadow_color;
}

/**
 * nautilus_label_set_line_wrap:
 *
 * @label: A NautilusLabel
 * @line_wrap: A boolean value indicating whether the label should
 * line wrap words if they dont fit in the horizontally allocated
 * space.
 *
 */
void
nautilus_label_set_line_wrap (NautilusLabel	*label,
			      gboolean		line_wrap)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	if (label->detail->line_wrap == line_wrap) {
		return;
	}
	
	label->detail->line_wrap = line_wrap;

	label_recompute_line_geometries (label);
	
	gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * nautilus_label_get_line_wrap:
 *
 * @label: A NautilusLabel
 *
 * Return value: A boolean value indicating whether the label
 * is currently line wrapping text.
 */
gboolean
nautilus_label_get_line_wrap (const NautilusLabel *label)
{
	g_return_val_if_fail (label != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), FALSE);

	return label->detail->line_wrap;
}

/**
 * nautilus_label_set_line_wrap_width:
 *
 * @label: A NautilusLabel
 * @line_wrap_width: The new line wrap width.
 *
 * The line wrap width is something.
 * 
 */
void
nautilus_label_set_line_wrap_width (NautilusLabel	*label,
				    guint		line_wrap_width)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	if (label->detail->line_wrap_width == line_wrap_width) {
		return;
	}
	
	label->detail->line_wrap_width = line_wrap_width;

	label_recompute_line_geometries (label);
	
	gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * nautilus_label_get_line_wrap_width:
 *
 * @label: A NautilusLabel
 *
 * Return value: A boolean value indicating whether the label
 * is currently line wrapping text.
 */
guint
nautilus_label_get_line_wrap_width (const NautilusLabel *label)
{
	g_return_val_if_fail (label != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), FALSE);

	return label->detail->line_wrap_width;
}

/**
 * nautilus_label_set_line_wrap_separators:
 *
 * @label: A NautilusLabel
 * @line_wrap_separators: The new line wrap separators.
 *
 * The line wrap separators is something.
 * 
 */
void
nautilus_label_set_line_wrap_separators (NautilusLabel	*label,
					 const char *line_wrap_separators)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (line_wrap_separators != NULL);
	g_return_if_fail (line_wrap_separators[0] != '\0');
	g_return_if_fail (strlen (line_wrap_separators) > 0);

	if (nautilus_str_is_equal (label->detail->line_wrap_separators, line_wrap_separators)) {
		return;
	}

	g_free (label->detail->line_wrap_separators);
	label->detail->line_wrap_separators = g_strdup (line_wrap_separators);

	label_recompute_line_geometries (label);
	
	gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * nautilus_label_get_line_wrap_separators:
 *
 * @label: A NautilusLabel
 *
 * Return value: A boolean value indicating whether the label
 * is currently line wrapping text.
 */
char *
nautilus_label_get_line_wrap_separators (const NautilusLabel *label)
{
	g_return_val_if_fail (label != NULL, FALSE);
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), FALSE);

	return g_strdup (label->detail->line_wrap_separators);
}

/**
 * nautilus_label_new_loaded:
 *
 * @text: Text or NULL
 * @family: Font family or NULL
 * @weight: Font weight or NULL
 * @font_size: Font size in pixels
 * @drop_shadow_offset: Drop shadow offset
 * @drop_shadow_color: Drop shadow color
 * @text_color: Text color
 * @xpadding: Amount to pad label in the x direction.
 * @ypadding: Amount to pad label in the y direction.
 * @vertical_offset: Amount to offset the label vertically.
 * @horizontal_offset: Amount to offset the label horizontally.
 * @background_color: Background color.
 * @tile_pixbuf: Pixbuf to use for tile or NULL
 *
 * Return value: Newly created label with all the given values.
 */
GtkWidget *
nautilus_label_new_loaded (const char *text,
			   const char *family,
			   const char *weight,
			   guint font_size,
			   guint drop_shadow_offset,
			   guint32 drop_shadow_color,
			   guint32 text_color,
			   gint xpadding,
			   gint ypadding,
			   guint vertical_offset,
			   guint horizontal_offset,
			   guint32 background_color,
			   GdkPixbuf *tile_pixbuf)
{
	NautilusLabel *label;

 	label = NAUTILUS_LABEL (nautilus_label_new (text ? text : ""));

	if (family != NULL) {
		nautilus_label_set_font_from_components (label, family, weight, NULL, NULL);
	}

	nautilus_label_set_font_size (label, font_size);
	nautilus_label_set_drop_shadow_offset (label, drop_shadow_offset);
	nautilus_buffered_widget_set_background_type (NAUTILUS_BUFFERED_WIDGET (label), NAUTILUS_BACKGROUND_SOLID);
	nautilus_buffered_widget_set_background_color (NAUTILUS_BUFFERED_WIDGET (label), background_color);
	nautilus_label_set_drop_shadow_color (label, drop_shadow_color);
	nautilus_label_set_text_color (label, text_color);
	nautilus_buffered_widget_set_vertical_offset (NAUTILUS_BUFFERED_WIDGET (label), vertical_offset);
	nautilus_buffered_widget_set_horizontal_offset (NAUTILUS_BUFFERED_WIDGET (label), horizontal_offset);

	gtk_misc_set_padding (GTK_MISC (label), xpadding, ypadding);

	if (tile_pixbuf != NULL) {
		nautilus_buffered_widget_set_tile_pixbuf (NAUTILUS_BUFFERED_WIDGET (label), tile_pixbuf);
	}
	
	return GTK_WIDGET (label);
}
