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
#include "nautilus-gtk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-art-gtk-extensions.h"
#include "nautilus-string.h"
#include "nautilus-debug-drawing.h"

/* These are arbitrary constants to catch insane values */
#define MIN_SMOOTH_FONT_SIZE 5
#define MAX_SMOOTH_FONT_SIZE 64

#define DEFAULT_FONT_SIZE 14
#define LINE_WRAP_SEPARATORS _(" -_,;.?/&")
#define LINE_OFFSET 2

#define SMOOTH_FONT_MULTIPLIER 1.3

/* This magic string is copied from GtkLabel.  It lives there unlocalized as well. */
#define DEFAULT_LINE_WRAP_WIDTH_TEXT "This is a good enough length for any line to have."

/* Arguments */
enum
{
	ARG_0,
	
	/* Deal with the GtkLabel arguments as well */
	ARG_LABEL,
	ARG_WRAP,
	ARG_JUSTIFY,

	ARG_BACKGROUND_MODE,
	ARG_IS_SMOOTH,
	ARG_TEXT_OPACITY,
	ARG_SMOOTH_FONT,
	ARG_SMOOTH_FONT_SIZE,
	ARG_SMOOTH_TEXT_COLOR,
	ARG_SMOOTH_DROP_SHADOW_OFFSET,
	ARG_SMOOTH_DROP_SHADOW_COLOR,
	ARG_SMOOTH_LINE_WRAP_WIDTH,
	ARG_ADJUST_WRAP_ON_RESIZE,

	ARG_TILE_HEIGHT,
	ARG_TILE_MODE_HORIZONTAL,
	ARG_TILE_MODE_VERTICAL,
	ARG_TILE_OPACITY,
	ARG_TILE_PIXBUF,
	ARG_TILE_WIDTH
};

/* Signals */
typedef enum
{
	DRAW_BACKGROUND,
	SET_IS_SMOOTH,
	LAST_SIGNAL
} LabelSignal;

/* Signals */
static guint label_signals[LAST_SIGNAL] = { 0 };

/* Detail member struct */
struct _NautilusLabelDetails
{
	gboolean is_smooth;

	/* Tile attributes */
	GdkPixbuf *tile_pixbuf;
	int tile_opacity;
	int tile_width;
	int tile_height;
	NautilusSmoothTileMode tile_mode_vertical;
	NautilusSmoothTileMode tile_mode_horizontal;

 	/* Smooth attributes */
 	NautilusScalableFont *smooth_font;
 	guint smooth_font_size;
	guint32 smooth_text_color;
	guint smooth_drop_shadow_offset;
	guint32 smooth_drop_shadow_color;
 	guint smooth_line_wrap_width;
	gboolean adjust_wrap_on_resize;

	/* Text */
  	int text_opacity;

	/* Text lines */
	int *text_line_widths;
	int *text_line_heights;
	int num_text_lines;
	int max_text_line_width;
	int total_text_line_height;

	/* Line wrapping */
	NautilusTextLayout **text_layouts;
	
	/* Background */
	NautilusSmoothBackgroundMode background_mode;
	guint32 solid_background_color;

	GdkPixbuf *solid_cache_pixbuf;
	gboolean never_smooth;
};

/* GtkObjectClass methods */
static void     nautilus_label_initialize_class             (NautilusLabelClass  *label_class);
static void     nautilus_label_initialize                   (NautilusLabel       *label);
static void     nautilus_label_destroy                      (GtkObject           *object);
static void     nautilus_label_set_arg                      (GtkObject           *object,
							     GtkArg              *arg,
							     guint                arg_id);
static void     nautilus_label_get_arg                      (GtkObject           *object,
							     GtkArg              *arg,
							     guint                arg_id);

/* GtkWidgetClass methods */
static void     nautilus_label_size_request                 (GtkWidget           *widget,
							     GtkRequisition      *requisition);
static void     nautilus_label_size_allocate                (GtkWidget           *widget,
							     GtkAllocation       *allocation);
static int      nautilus_label_expose_event                 (GtkWidget           *widget,
							     GdkEventExpose      *event);

/* NautilusLabel signals */
static void     nautilus_label_set_is_smooth_signal         (GtkWidget           *widget,
							     gboolean             is_smooth);

/* Private NautilusLabel things */
const char *    label_peek_text                             (const NautilusLabel *label);
static void     label_line_geometries_recompute             (NautilusLabel       *label);
static void     label_line_geometries_clear                 (NautilusLabel       *label);
static guint    label_get_empty_line_height                 (const NautilusLabel *label);
static guint    label_get_total_text_and_line_offset_height (const NautilusLabel *label);
static ArtIRect label_get_text_bounds                       (const NautilusLabel *label);
static ArtIRect label_get_text_frame                        (const NautilusLabel *label);
static ArtIRect label_get_tile_frame                        (const NautilusLabel *label);
static void     label_draw_text_to_pixbuf                   (NautilusLabel       *label,
							     GdkPixbuf           *pixbuf,
							     const ArtIRect      *destination_area,
							     int                  x,
							     int                  y);
static guint    label_get_default_line_wrap_width           (const NautilusLabel *label);
static void     label_solid_cache_pixbuf_clear              (NautilusLabel       *label);
static gboolean label_can_cache_contents                    (const NautilusLabel *label);
static gboolean label_is_smooth                             (const NautilusLabel *label);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusLabel, nautilus_label, GTK_TYPE_LABEL)

/* Class init methods */
static void
nautilus_label_initialize_class (NautilusLabelClass *label_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (label_class);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (label_class);

	/* GtkObjectClass */
	object_class->destroy = nautilus_label_destroy;
	object_class->set_arg = nautilus_label_set_arg;
	object_class->get_arg = nautilus_label_get_arg;
	
	/* GtkWidgetClass */
	widget_class->size_request = nautilus_label_size_request;
	widget_class->size_allocate = nautilus_label_size_allocate;
	widget_class->expose_event = nautilus_label_expose_event;

	/* NautilusLabelClass */
	label_class->set_is_smooth = nautilus_label_set_is_smooth_signal;

	/* Signals */
	label_signals[DRAW_BACKGROUND] = gtk_signal_new ("draw_background",
							 GTK_RUN_LAST,
							 object_class->type,
							 0,
							 gtk_marshal_NONE__POINTER_POINTER,
							 GTK_TYPE_NONE, 
							 2,
							 GTK_TYPE_POINTER,
							 GTK_TYPE_POINTER);

	label_signals[SET_IS_SMOOTH] = gtk_signal_new ("set_is_smooth",
						       GTK_RUN_LAST,
						       object_class->type,
						       GTK_SIGNAL_OFFSET (NautilusLabelClass, set_is_smooth),
						       gtk_marshal_NONE__BOOL,
						       GTK_TYPE_NONE, 
						       1,
						       GTK_TYPE_BOOL);

	gtk_object_class_add_signals (object_class, label_signals, LAST_SIGNAL);

	/* Arguments */
	gtk_object_add_arg_type ("NautilusLabel::tile_pixbuf",
				 GTK_TYPE_POINTER,
				 GTK_ARG_READWRITE,
				 ARG_TILE_PIXBUF);
	gtk_object_add_arg_type ("NautilusLabel::tile_opacity",
				 GTK_TYPE_INT,
				 GTK_ARG_READWRITE,
				 ARG_TILE_OPACITY);
	gtk_object_add_arg_type ("NautilusLabel::tile_width",
				 GTK_TYPE_INT,
				 GTK_ARG_READWRITE,
				 ARG_TILE_WIDTH);
	gtk_object_add_arg_type ("NautilusLabel::tile_height",
				 GTK_TYPE_INT,
				 GTK_ARG_READWRITE,
				 ARG_TILE_HEIGHT);
	gtk_object_add_arg_type ("NautilusLabel::tile_mode_vertical",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_TILE_MODE_VERTICAL);
	gtk_object_add_arg_type ("NautilusLabel::tile_mode_horizontal",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_TILE_MODE_HORIZONTAL);
	gtk_object_add_arg_type ("NautilusLabel::label",
				 GTK_TYPE_STRING,
				 GTK_ARG_READWRITE,
				 ARG_LABEL);
	gtk_object_add_arg_type ("NautilusLabel::wrap",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_WRAP);
	gtk_object_add_arg_type ("NautilusLabel::justify",
				 GTK_TYPE_JUSTIFICATION,
				 GTK_ARG_READWRITE,
				 ARG_JUSTIFY);
	gtk_object_add_arg_type ("NautilusLabel::is_smooth",
				 GTK_TYPE_BOOL,
				 GTK_ARG_READWRITE,
				 ARG_IS_SMOOTH);
	gtk_object_add_arg_type ("NautilusLabel::text_opacity",
				 GTK_TYPE_INT,
				 GTK_ARG_READWRITE,
				 ARG_TEXT_OPACITY);
	gtk_object_add_arg_type ("NautilusLabel::background_mode",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_BACKGROUND_MODE);
	gtk_object_add_arg_type ("NautilusLabel::smooth_font",
				 GTK_TYPE_OBJECT,
				 GTK_ARG_READWRITE,
				 ARG_SMOOTH_FONT);
	gtk_object_add_arg_type ("NautilusLabel::smooth_font_size",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_SMOOTH_FONT_SIZE);
	gtk_object_add_arg_type ("NautilusLabel::smooth_text_color",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_SMOOTH_TEXT_COLOR);
	gtk_object_add_arg_type ("NautilusLabel::smooth_drop_shadow_offset",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_SMOOTH_DROP_SHADOW_OFFSET);
	gtk_object_add_arg_type ("NautilusLabel::smooth_drop_shadow_color",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_SMOOTH_DROP_SHADOW_COLOR);
	gtk_object_add_arg_type ("NautilusLabel::smooth_line_wrap_width",
				 GTK_TYPE_UINT,
				 GTK_ARG_READWRITE,
				 ARG_SMOOTH_LINE_WRAP_WIDTH);

	/* Make this class inherit the same kind of theme stuff as GtkLabel */
	nautilus_gtk_class_name_make_like_existing_type ("NautilusLabel", GTK_TYPE_LABEL);

	/* Let the smooth widget machinery know that our class can be smooth */
	nautilus_smooth_widget_register_type (NAUTILUS_TYPE_LABEL);
}

void
nautilus_label_initialize (NautilusLabel *label)
{
	GTK_WIDGET_UNSET_FLAGS (label, GTK_CAN_FOCUS);
	GTK_WIDGET_SET_FLAGS (label, GTK_NO_WINDOW);

	label->details = g_new0 (NautilusLabelDetails, 1);

	label->details->text_opacity = NAUTILUS_OPACITY_FULLY_OPAQUE;
	label->details->smooth_font = nautilus_scalable_font_get_default_font ();
 	label->details->smooth_font_size = DEFAULT_FONT_SIZE;
	label->details->smooth_text_color = NAUTILUS_RGBA_COLOR_PACK (0, 0, 0, 255);
	label->details->smooth_drop_shadow_color = NAUTILUS_RGBA_COLOR_PACK (255, 255, 255, 255);
	label->details->smooth_line_wrap_width = label_get_default_line_wrap_width (label);

	label->details->tile_opacity = NAUTILUS_OPACITY_FULLY_OPAQUE;
 	label->details->tile_width = NAUTILUS_SMOOTH_TILE_EXTENT_FULL;
 	label->details->tile_height = NAUTILUS_SMOOTH_TILE_EXTENT_FULL;
	label->details->tile_mode_vertical = NAUTILUS_SMOOTH_TILE_SELF;
	label->details->tile_mode_horizontal = NAUTILUS_SMOOTH_TILE_SELF;
	label->details->background_mode = NAUTILUS_SMOOTH_BACKGROUND_GTK;

	nautilus_smooth_widget_register (GTK_WIDGET (label));
}

/* GtkObjectClass methods */
static void
nautilus_label_destroy (GtkObject *object)
{
 	NautilusLabel *label;
	
	g_return_if_fail (NAUTILUS_IS_LABEL (object));

	label = NAUTILUS_LABEL (object);

	nautilus_gdk_pixbuf_unref_if_not_null (label->details->tile_pixbuf);
	label->details->tile_pixbuf = NULL;
	label_solid_cache_pixbuf_clear (label);

	label_line_geometries_clear (label);
		
	g_free (label->details);

	/* Chain destroy */
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_label_set_arg (GtkObject *object,
			GtkArg *arg,
			guint arg_id)
{
	NautilusLabel *label;

	g_return_if_fail (NAUTILUS_IS_LABEL (object));

 	label = NAUTILUS_LABEL (object);

 	switch (arg_id)
	{

	case ARG_TILE_OPACITY:
		nautilus_label_set_tile_opacity (label, GTK_VALUE_INT (*arg));
		break;

	case ARG_TILE_PIXBUF:
		nautilus_label_set_tile_pixbuf (label, (GdkPixbuf *) GTK_VALUE_POINTER (*arg));
		break;
		
	case ARG_TILE_WIDTH:
		nautilus_label_set_tile_width (label, GTK_VALUE_INT (*arg));
		break;

	case ARG_TILE_HEIGHT:
		nautilus_label_set_tile_height (label, GTK_VALUE_INT (*arg));
		break;

	case ARG_TILE_MODE_VERTICAL:
		nautilus_label_set_tile_mode_vertical (label, GTK_VALUE_UINT (*arg));
		break;

	case ARG_TILE_MODE_HORIZONTAL:
		nautilus_label_set_tile_mode_horizontal (label, GTK_VALUE_UINT (*arg));
		break;

	case ARG_LABEL:
		nautilus_label_set_text (label, GTK_VALUE_STRING (*arg));
		break;
		
	case ARG_WRAP:
		nautilus_label_set_wrap (label, GTK_VALUE_BOOL (*arg));
		break;
		
	case ARG_JUSTIFY:
		nautilus_label_set_justify (label, GTK_VALUE_ENUM (*arg));
		break;
		
	case ARG_IS_SMOOTH:
		nautilus_label_set_is_smooth (label, GTK_VALUE_BOOL (*arg));
		break;

	case ARG_TEXT_OPACITY:
		nautilus_label_set_text_opacity (label, GTK_VALUE_INT (*arg));
		break;

	case ARG_BACKGROUND_MODE:
		nautilus_label_set_background_mode (label, GTK_VALUE_UINT (*arg));
		break;

	case ARG_SMOOTH_FONT:
		nautilus_label_set_smooth_font (label, (NautilusScalableFont *) GTK_VALUE_OBJECT (*arg));
		break;

	case ARG_SMOOTH_FONT_SIZE:
		nautilus_label_set_smooth_font_size (label, GTK_VALUE_UINT (*arg));
		break;

	case ARG_SMOOTH_TEXT_COLOR:
		nautilus_label_set_text_color (label, GTK_VALUE_UINT (*arg));
		break;

	case ARG_SMOOTH_DROP_SHADOW_OFFSET:
		nautilus_label_set_smooth_drop_shadow_offset (label, GTK_VALUE_UINT (*arg));
		break;

	case ARG_SMOOTH_DROP_SHADOW_COLOR:
		nautilus_label_set_smooth_drop_shadow_color (label, GTK_VALUE_UINT (*arg));
		break;

	case ARG_SMOOTH_LINE_WRAP_WIDTH:
		nautilus_label_set_smooth_line_wrap_width (label, GTK_VALUE_UINT (*arg));
		break;

	case ARG_ADJUST_WRAP_ON_RESIZE:
		nautilus_label_set_adjust_wrap_on_resize (label, GTK_VALUE_BOOL (*arg));
		break;

 	default:
		g_assert_not_reached ();
	}
}

static void
nautilus_label_get_arg (GtkObject *object,
			GtkArg *arg,
			guint arg_id)
{
	NautilusLabel	*label;

	g_return_if_fail (NAUTILUS_IS_LABEL (object));
	
	label = NAUTILUS_LABEL (object);

 	switch (arg_id)
	{
	case ARG_TILE_OPACITY:
		GTK_VALUE_INT (*arg) = nautilus_label_get_tile_opacity (label);
		break;
		
	case ARG_TILE_PIXBUF:
		GTK_VALUE_POINTER (*arg) = nautilus_label_get_tile_pixbuf (label);
		break;

	case ARG_TILE_WIDTH:
		GTK_VALUE_INT (*arg) = nautilus_label_get_tile_width (label);
		break;

	case ARG_TILE_HEIGHT:
		GTK_VALUE_INT (*arg) = nautilus_label_get_tile_height (label);
		break;

	case ARG_TILE_MODE_VERTICAL:
		GTK_VALUE_UINT (*arg) = nautilus_label_get_tile_mode_vertical (label);
		break;

	case ARG_TILE_MODE_HORIZONTAL:
		GTK_VALUE_UINT (*arg) = nautilus_label_get_tile_mode_horizontal (label);
		break;


	case ARG_LABEL:
		GTK_VALUE_STRING (*arg) = nautilus_label_get_text (label);
		break;

	case ARG_WRAP:
		GTK_VALUE_BOOL (*arg) = nautilus_label_get_wrap (label);
		break;

	case ARG_JUSTIFY:
		GTK_VALUE_ENUM (*arg) = nautilus_label_get_text_justify (label);
		break;

	case ARG_IS_SMOOTH:
		GTK_VALUE_BOOL (*arg) = nautilus_label_get_is_smooth (label);
		break;
		
	case ARG_TEXT_OPACITY:
		GTK_VALUE_INT (*arg) = nautilus_label_get_text_opacity (label);
		break;
		
	case ARG_BACKGROUND_MODE:
		GTK_VALUE_UINT (*arg) = nautilus_label_get_background_mode (label);
		break;

	case ARG_SMOOTH_FONT:
		GTK_VALUE_OBJECT (*arg) = (GtkObject *) nautilus_label_get_smooth_font (label);
		break;

	case ARG_SMOOTH_FONT_SIZE:
		GTK_VALUE_UINT (*arg) = nautilus_label_get_smooth_font_size (label);
		break;

	case ARG_SMOOTH_TEXT_COLOR:
		GTK_VALUE_UINT (*arg) = nautilus_label_get_text_color (label);
		break;

	case ARG_SMOOTH_DROP_SHADOW_OFFSET:
		GTK_VALUE_UINT (*arg) = nautilus_label_get_smooth_drop_shadow_offset (label);
		break;

	case ARG_SMOOTH_DROP_SHADOW_COLOR:
		GTK_VALUE_UINT (*arg) = nautilus_label_get_smooth_drop_shadow_color (label);
		break;

	case ARG_SMOOTH_LINE_WRAP_WIDTH:
		GTK_VALUE_UINT (*arg) = nautilus_label_get_smooth_line_wrap_width (label);
		break;

	case ARG_ADJUST_WRAP_ON_RESIZE:
		GTK_VALUE_BOOL (*arg) = nautilus_label_get_adjust_wrap_on_resize (label);
		break;

 	default:
		g_assert_not_reached ();
	}
}

/* GtkWidgetClass methods */
static void
nautilus_label_size_request (GtkWidget *widget,
			     GtkRequisition *requisition)
{
	NautilusLabel *label;

	ArtIRect text_frame;
	ArtIRect tile_frame;
	ArtIRect preferred_frame;

	g_return_if_fail (NAUTILUS_IS_LABEL (widget));
	g_return_if_fail (requisition != NULL);

 	label = NAUTILUS_LABEL (widget);

	if (!label_is_smooth (label)) {
		NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, size_request, (widget, requisition));
		return;
	}
	
	text_frame = label_get_text_frame (label);
	tile_frame = label_get_tile_frame (label);

	preferred_frame = nautilus_smooth_widget_get_preferred_frame (widget,
								      &text_frame,
								      &tile_frame,
								      label->details->tile_width,
								      label->details->tile_height);
   	requisition->width = preferred_frame.x1;
   	requisition->height = preferred_frame.y1;
}

static void
nautilus_label_size_allocate (GtkWidget *widget,
			      GtkAllocation *allocation)
{
	NautilusLabel *label;
	
 	g_return_if_fail (NAUTILUS_IS_LABEL (widget));
 	g_return_if_fail (allocation != NULL);
	
  	label = NAUTILUS_LABEL (widget);
	
	/* Pre chain size_allocate */
	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, size_allocate, (widget, allocation));

	if (label->details->adjust_wrap_on_resize) {
		label->details->smooth_line_wrap_width = allocation->width;
		label_line_geometries_recompute (label);
	}
}

/* Painting callback for non smooth case */
static void
label_paint_pixbuf_callback (GtkWidget *widget,
			     GdkDrawable *destination_drawable,
			     GdkGC *gc,
			     int source_x,
			     int source_y,
			     const ArtIRect *area,
			     gpointer callback_data)
{
	NautilusLabel *label;
	GdkEventExpose *event;

	g_return_if_fail (NAUTILUS_IS_LABEL (widget));
	g_return_if_fail (GTK_WIDGET_REALIZED (widget));
	g_return_if_fail (destination_drawable != NULL);
	g_return_if_fail (gc != NULL);
	g_return_if_fail (area != NULL && !art_irect_empty (area));

	label = NAUTILUS_LABEL (widget);

	event = (GdkEventExpose *) callback_data;

	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, expose_event, (widget, event));
}

/* Compositing callback for smooth case */
static void
label_composite_pixbuf_callback (GtkWidget *widget,
				 GdkPixbuf *destination_pixbuf,
				 int source_x,
				 int source_y,
				 const ArtIRect *area,
				 int opacity,
				 gpointer callback_data)
{
	NautilusLabel *label;
	ArtIRect text_frame;
	ArtIRect text_bounds;
	ArtIRect workaround_buffer_area;
	GdkPixbuf *pixbuf;
	GdkEventExpose *event;
	ArtIRect text_dirty_area;
	ArtIRect dirty_area;
	
	g_return_if_fail (NAUTILUS_IS_LABEL (widget));
	g_return_if_fail (GTK_WIDGET_REALIZED (widget));
	g_return_if_fail (destination_pixbuf != NULL);
	g_return_if_fail (area != NULL && !art_irect_empty (area));

	label = NAUTILUS_LABEL (widget);
	event = (GdkEventExpose *) callback_data;

	text_frame = label_get_text_frame (label);
	text_bounds = label_get_text_bounds (label);
	
	g_return_if_fail (!art_irect_empty (&text_frame));

	/* Optimize the case where the background is solid */
	if (label_can_cache_contents (label)) {
		if (label->details->solid_cache_pixbuf == NULL) {
			label->details->solid_cache_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
									    FALSE,
									    8,
									    text_frame.x1,
									    text_frame.y1);
			
			nautilus_gdk_pixbuf_fill_rectangle_with_color (label->details->solid_cache_pixbuf,
								       NULL,
								       label->details->solid_background_color);
			
			label_draw_text_to_pixbuf (label,
						   label->details->solid_cache_pixbuf,
						   &text_frame,
						   0,
						   0);
		}

		nautilus_gdk_pixbuf_draw_to_pixbuf (label->details->solid_cache_pixbuf,
						    destination_pixbuf,
						    source_x,
						    source_y,
						    area);

		return;
	}

	/* We dont really need this information.  The point is to have
	 * the "smooth widget" figure it out and feed us only the final 
	 * content dirty rectangle.  We compute it now to workaround
	 * bug 2784.  See fixme below.
	 */
	dirty_area = nautilus_irect_assign_gdk_rectangle (&event->area);
	art_irect_intersect (&text_dirty_area, &text_bounds, &dirty_area);

	/* FIXME bugzilla.eazel.com 2784: 
	 * The reason we use a temporary buffer instead of
	 * drawing directly into the destination_pixbuf is that
	 * there currently is a serious bug in NautilusScalalbleFont
	 * that prevents clipping from happening when drawing smooth
	 * text.
	 */
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, text_frame.x1, text_frame.y1);

	/* Copy the dirty bits out of the destination to our temporary buffer */
 	workaround_buffer_area.x0 = text_dirty_area.x0 - text_bounds.x0;
 	workaround_buffer_area.y0 = text_dirty_area.y0 - text_bounds.y0;
 	workaround_buffer_area.x1 = workaround_buffer_area.x0 + gdk_pixbuf_get_width (destination_pixbuf);
 	workaround_buffer_area.y1 = workaround_buffer_area.y0 + gdk_pixbuf_get_height (destination_pixbuf);
	
	nautilus_gdk_pixbuf_draw_to_pixbuf (destination_pixbuf,
					    pixbuf,
					    area->x0,
					    area->y0,
					    &workaround_buffer_area);

	/* Now draw the full extent of the text to the buffer.  Once
	 * bug 2784 is fixed, we can simply pass this function a
	 * clip rectangle and lose the temporary buffer code above
	 */
	label_draw_text_to_pixbuf (label,
				   pixbuf,
				   &text_frame,
				   0,
				   0);

	/* And finally draw the composited bits back to the 
	 * destination buffer - just the dirty area.
	 * 
	 * Again, when bug 2784 is fixed this step is not needed.
	 */
	nautilus_gdk_pixbuf_draw_to_pixbuf (pixbuf,
					    destination_pixbuf,
					    text_dirty_area.x0 - text_bounds.x0,
					    text_dirty_area.y0 - text_bounds.y0,
					    area);
	
	gdk_pixbuf_unref (pixbuf);
}	

static int
nautilus_label_expose_event (GtkWidget *widget,
			     GdkEventExpose *event)
{
 	NautilusLabel *label;
	ArtIRect dirty_area;
	ArtIRect screen_dirty_area;
	ArtIRect smooth_text_bounds;
	ArtIRect widget_bounds;
	ArtIRect tile_bounds;

	g_return_val_if_fail (NAUTILUS_IS_LABEL (widget), TRUE);
	g_return_val_if_fail (GTK_WIDGET_REALIZED (widget), TRUE);
	g_return_val_if_fail (event != NULL, TRUE);
	g_return_val_if_fail (event->window == widget->window, TRUE);
	
 	label = NAUTILUS_LABEL (widget);

	/* The smooth and non smooth bounds are different.  We have
	 * no way to have GtkLabel tell us what its bounds are.
	 * So, we cheat and pretend that for the non smooth case,
	 * the text bounds are the whole widget.
	 *
	 * This works because the smooth widget paint handler will
	 * properly intersect these bounds and call the paint 
	 * callback.  We feed the paint callback the actual gdk
	 * expose event so that we feed the exact exposure area
	 * to GtkLabel's expose_event.
	 */
	widget_bounds = nautilus_irect_gtk_widget_get_bounds (widget);
	smooth_text_bounds = label_get_text_bounds (label);
	tile_bounds = nautilus_smooth_widget_get_tile_bounds (widget,
							      label->details->tile_pixbuf,
							      label->details->tile_width,
							      label->details->tile_height);
	
	/* Check for the dumb case when theres nothing to do */
	if (nautilus_strlen (label_peek_text (label)) == 0 && label->details->tile_pixbuf == NULL) {
		return TRUE;
	}

	/* Clip the dirty area to the screen */
	dirty_area = nautilus_irect_assign_gdk_rectangle (&event->area);
	screen_dirty_area = nautilus_irect_gdk_window_clip_dirty_area_to_screen (event->window,
										 &dirty_area);

	/* Make sure the area is screen visible before painting */
	if (!art_irect_empty (&screen_dirty_area)) {
		nautilus_smooth_widget_paint (widget,
					      widget->style->white_gc,
					      label_is_smooth (label),
					      label->details->background_mode,
					      label->details->solid_background_color,
					      label->details->tile_pixbuf,
					      &tile_bounds,
					      label->details->tile_opacity,
					      label->details->tile_mode_vertical,
					      label->details->tile_mode_horizontal,
					      label_is_smooth (label) ? &smooth_text_bounds : &widget_bounds,
					      label->details->text_opacity,
					      &screen_dirty_area,
					      label_paint_pixbuf_callback,
					      label_composite_pixbuf_callback,
					      event);
	}

	return TRUE;
}

/* NautilusLabel signals */
static void
nautilus_label_set_is_smooth_signal (GtkWidget *widget,
				     gboolean is_smooth)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (widget));

	nautilus_label_set_is_smooth (NAUTILUS_LABEL (widget), is_smooth);
}

/* Private NautilusLabel things */
static void
label_draw_text_to_pixbuf (NautilusLabel *label,
			   GdkPixbuf *pixbuf,
			   const ArtIRect *destination_area,
			   int x,
			   int y)
{
	ArtIRect pixbuf_frame;
	
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (destination_area != NULL);
	g_return_if_fail (nautilus_gdk_pixbuf_is_valid (pixbuf));
	g_return_if_fail (!art_irect_empty (destination_area));
	
	pixbuf_frame = nautilus_gdk_pixbuf_get_frame (pixbuf);

	g_return_if_fail (pixbuf_frame.x1 >= (destination_area->x1 - destination_area->x0));
	g_return_if_fail (pixbuf_frame.y1 >= (destination_area->y1 - destination_area->y0));

	/* Line wrapping */
	if (nautilus_label_get_wrap (label)) {
		int i;
		
		for (i = 0; i < label->details->num_text_lines; i++) {
			const NautilusTextLayout *text_layout = label->details->text_layouts[i];
			
			if (label->details->smooth_drop_shadow_offset > 0) {
				nautilus_text_layout_paint (text_layout, 
							    pixbuf, 
							    x + label->details->smooth_drop_shadow_offset, 
							    y + label->details->smooth_drop_shadow_offset,
							    nautilus_label_get_text_justify (label),
							    label->details->smooth_drop_shadow_color,
							    FALSE);
			}
			
			nautilus_text_layout_paint (text_layout, 
						    pixbuf, 
						    x, 
						    y,
						    nautilus_label_get_text_justify (label),
						    label->details->smooth_text_color,
						    FALSE);
			
			y += text_layout->height;
		}
	}
	/* No line wrapping */
	else {
		if (label->details->smooth_drop_shadow_offset > 0) {
			nautilus_scalable_font_draw_text_lines_with_dimensions (
				label->details->smooth_font,
				pixbuf,
				x + label->details->smooth_drop_shadow_offset,
				x + label->details->smooth_drop_shadow_offset,
				destination_area,
				label->details->smooth_font_size,
				label->details->smooth_font_size,
				label_peek_text (label),
				label->details->num_text_lines,
				label->details->text_line_widths,
				label->details->text_line_heights,
				nautilus_label_get_text_justify (label),
				LINE_OFFSET,
				label_get_empty_line_height (label),
				label->details->smooth_drop_shadow_color,
				label->details->text_opacity);
		}
		
		nautilus_scalable_font_draw_text_lines_with_dimensions (
			label->details->smooth_font,
			pixbuf,
			x,
			y,
			destination_area,
			label->details->smooth_font_size,
			label->details->smooth_font_size,
			label_peek_text (label),
			label->details->num_text_lines,
			label->details->text_line_widths,
			label->details->text_line_heights,
			nautilus_label_get_text_justify (label),
			LINE_OFFSET,
			label_get_empty_line_height (label),
			label->details->smooth_text_color,
			label->details->text_opacity);
	}
}

static guint
label_get_default_line_wrap_width (const NautilusLabel *label)
{
	guint width;
	guint height;

	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);
	
	nautilus_scalable_font_measure_text (label->details->smooth_font,
					     label->details->smooth_font_size,
					     label->details->smooth_font_size,
					     DEFAULT_LINE_WRAP_WIDTH_TEXT,
					     strlen (DEFAULT_LINE_WRAP_WIDTH_TEXT),
					     &width,
					     &height);

	return width;
}

static ArtIRect
label_get_text_frame (const NautilusLabel *label)
{
	ArtIRect text_frame;

	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), NAUTILUS_ART_IRECT_EMPTY);

	text_frame = NAUTILUS_ART_IRECT_EMPTY;

	if (label->details->num_text_lines > 0) {
		text_frame.x1 = 
			label->details->max_text_line_width
			+ label->details->smooth_drop_shadow_offset;
		
		text_frame.y1 = label_get_total_text_and_line_offset_height (label)
			+ label->details->smooth_drop_shadow_offset;
	}

	return text_frame;
}

static ArtIRect
label_get_text_bounds (const NautilusLabel *label)
{
	ArtIRect text_frame;
	ArtIRect text_bounds;
	ArtIRect bounds;

	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), NAUTILUS_ART_IRECT_EMPTY);

	text_frame = label_get_text_frame (label);

	if (art_irect_empty (&text_frame)) {
		return NAUTILUS_ART_IRECT_EMPTY;
	}
	
	bounds = nautilus_irect_gtk_widget_get_bounds (GTK_WIDGET (label));

	text_bounds = nautilus_art_irect_align (&bounds,
						text_frame.x1,
						text_frame.y1,
						GTK_MISC (label)->xalign,
						GTK_MISC (label)->yalign);
		

	return text_bounds;
}

static ArtIRect
label_get_tile_frame (const NautilusLabel *label)
{
	ArtIRect tile_frame;

	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), NAUTILUS_ART_IRECT_EMPTY);

	if (!label->details->tile_pixbuf) {
		return NAUTILUS_ART_IRECT_EMPTY;
	}

	tile_frame.x0 = 0;
	tile_frame.y0 = 0;
	tile_frame.x1 = gdk_pixbuf_get_width (label->details->tile_pixbuf);
	tile_frame.y1 = gdk_pixbuf_get_height (label->details->tile_pixbuf);

	return tile_frame;
}

static void
label_solid_cache_pixbuf_clear (NautilusLabel *label)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	nautilus_gdk_pixbuf_unref_if_not_null (label->details->solid_cache_pixbuf);
	label->details->solid_cache_pixbuf = NULL;
}

static gboolean
label_can_cache_contents (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), FALSE);

	return (label->details->background_mode == NAUTILUS_SMOOTH_BACKGROUND_SOLID_COLOR)
		&& !label->details->tile_pixbuf;
}

const char *
label_peek_text (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), NULL);

	return GTK_LABEL (label)->label;
}

static guint
label_get_empty_line_height (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	/* If we wanted to crunch lines together, we could add a divider
	 * here.  For now we just use the font size for empty lines. */
	return label->details->smooth_font_size;
}

static guint
label_get_total_text_and_line_offset_height (const NautilusLabel *label)
{
	guint total_height;

	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	total_height = label->details->total_text_line_height;
	
	if (label->details->num_text_lines > 1) {
		total_height += ((label->details->num_text_lines - 1) * LINE_OFFSET);
	}

	return total_height;
}

static void
label_line_geometries_clear (NautilusLabel *label)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	
	g_free (label->details->text_line_widths);
	g_free (label->details->text_line_heights);
	label->details->text_line_widths = NULL;
	label->details->text_line_heights = NULL;

	if (label->details->text_layouts != NULL) {
		int i;

		for (i = 0; i < label->details->num_text_lines; i++) {
			g_assert (label->details->text_layouts[i] != NULL);
			nautilus_text_layout_free (label->details->text_layouts[i]);
		}

		g_free (label->details->text_layouts);
		label->details->text_layouts = NULL;
	}
	
	label->details->num_text_lines = 0;

	label->details->max_text_line_width = 0;
	label->details->total_text_line_height = 0;
}

static void
label_line_geometries_recompute (NautilusLabel *label)
{
	const char *text;

	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	text = label_peek_text (label);

	label_solid_cache_pixbuf_clear (label);
	label_line_geometries_clear (label);

	if (nautilus_strlen (text) == 0) {
		return;
	}
	
	label->details->num_text_lines = nautilus_str_count_characters (text, '\n') + 1;

	/* Line wrapping */
	if (nautilus_label_get_wrap (label)) {
		char **pieces;
		int i;

		label->details->text_layouts = g_new (NautilusTextLayout *, label->details->num_text_lines);

		pieces = g_strsplit (text, "\n", 0);

		for (i = 0; pieces[i] != NULL; i++) {
			char *text_piece = pieces[i];

			g_assert (i < label->details->num_text_lines);

			/* Make empty lines appear.  A single '\n' for example. */
			if (text_piece[0] == '\0') {
				text_piece = " ";
			}

			label->details->text_layouts[i] = nautilus_text_layout_new (label->details->smooth_font,
										   label->details->smooth_font_size,
										   text_piece,
										   LINE_WRAP_SEPARATORS,
										   label->details->smooth_line_wrap_width, 
										   TRUE);

			label->details->total_text_line_height += label->details->text_layouts[i]->height;
			
			if (label->details->text_layouts[i]->width > label->details->max_text_line_width) {
				label->details->max_text_line_width = label->details->text_layouts[i]->width;
			}
		}

		g_strfreev (pieces);
	}
	/* No line wrapping */
	else {
		label->details->text_line_widths = g_new (guint, label->details->num_text_lines);
		label->details->text_line_heights = g_new (guint, label->details->num_text_lines);
		
		nautilus_scalable_font_measure_text_lines (label->details->smooth_font,
							   label->details->smooth_font_size,
							   label->details->smooth_font_size,
							   text,
							   label->details->num_text_lines,
							   label_get_empty_line_height (label),
							   label->details->text_line_widths,
							   label->details->text_line_heights,
							   &label->details->max_text_line_width,
							   &label->details->total_text_line_height);
	}
}

gboolean
label_is_smooth (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), FALSE);

	return !label->details->never_smooth && label->details->is_smooth;
}

/* Public NautilusLabel methods */
GtkWidget *
nautilus_label_new (const char *text)
{
	NautilusLabel *label;
	
	label = NAUTILUS_LABEL (gtk_widget_new (nautilus_label_get_type (), NULL));
	
	nautilus_label_set_text (label, text);
	
	return GTK_WIDGET (label);
}

void
nautilus_label_set_smooth_font (NautilusLabel *label,
				NautilusScalableFont *smooth_font)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (NAUTILUS_IS_SCALABLE_FONT (smooth_font));
	
	if (label->details->smooth_font == smooth_font) {
		return;
	}
	
	if (label->details->smooth_font != NULL) {
		gtk_object_unref (GTK_OBJECT (label->details->smooth_font));
	}

	gtk_object_ref (GTK_OBJECT (smooth_font));
	label->details->smooth_font = smooth_font;

	/* Update the line wrap width */
	label->details->smooth_line_wrap_width = label_get_default_line_wrap_width (label);
	
	label_line_geometries_recompute (label);
	
	gtk_widget_queue_resize (GTK_WIDGET (label));
}

NautilusScalableFont *
nautilus_label_get_smooth_font (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), NULL);
	
	if (label->details->smooth_font != NULL) {
		gtk_object_ref (GTK_OBJECT (label->details->smooth_font));
	}

	return label->details->smooth_font;
}

void
nautilus_label_set_smooth_font_size (NautilusLabel *label,
				     guint smooth_font_size)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (smooth_font_size > MIN_SMOOTH_FONT_SIZE);

	if (label->details->smooth_font_size == smooth_font_size) {
		return;
	}

	label->details->smooth_font_size = smooth_font_size;

	/* Update the line wrap width */
	label->details->smooth_line_wrap_width = label_get_default_line_wrap_width (label);

	label_line_geometries_recompute (label);

	gtk_widget_queue_resize (GTK_WIDGET (label));
}

guint
nautilus_label_get_smooth_font_size (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	return label->details->smooth_font_size;
}

/* This function is an ugly hack.  The issue is that we want
 * GtkLabel to flush its cached requisition dimensions.  GtkLabel
 * caches these for efficiency.  Unfortunately, there is no public
 * way to do this for GtkLabel.  So, instead we trick the GtkLabel
 * into thinking that its justification has changed.  As a result
 * of this phony change, GtkLabel will flush the requisition cache
 * Of course, we don't really change the justification.  We hack 
 * the old one to a different value and tell GtkLabel to use the 
 * old real justification.
 */
static void
label_force_cached_requisition_flush (NautilusLabel *label)
{
	GtkJustification real_justification;
	GtkJustification phony_justification;

	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	
	real_justification = GTK_LABEL (label)->jtype;

	phony_justification = real_justification + 1;

	if (phony_justification >= GTK_JUSTIFY_FILL) {
		real_justification = GTK_JUSTIFY_LEFT;
	}

	GTK_LABEL (label)->jtype = phony_justification;
	gtk_label_set_justify (GTK_LABEL (label), real_justification);
}

void
nautilus_label_set_is_smooth (NautilusLabel *label,
			      gboolean is_smooth)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	if (label->details->never_smooth) {
		return;
	}

	if (label->details->is_smooth == is_smooth) {
		return;
	}

	label->details->is_smooth = is_smooth;

	label_line_geometries_recompute (label);

	/* Force GtkLabel to flush its cached requisition dimensions.
	 * GtkLabel caches its requisition for efficiency.  We need this
	 * dimensions to be flushed when our is_smooth attribute changes.
	 * The reason is that the geometry of the widget is dependent on
	 * whether it is_smooth or not.
	 */
	label_force_cached_requisition_flush (label);

	gtk_widget_queue_resize (GTK_WIDGET (label));
}

gboolean
nautilus_label_get_is_smooth (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), FALSE);

	return label_is_smooth (label);
}

/**
 * nautilus_label_set_tile_pixbuf:
 *
 * @label: A NautilusLabel
 * @pixbuf:          The new tile pixbuf
 *
 * Change the tile pixbuf.  A 'pixbuf' value of NULL, means dont use a
 * tile pixbuf - this is the default behavior for the widget.
 */
void
nautilus_label_set_tile_pixbuf (NautilusLabel *label,
				GdkPixbuf *pixbuf)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	
	if (pixbuf != label->details->tile_pixbuf) {
		nautilus_gdk_pixbuf_unref_if_not_null (label->details->tile_pixbuf);
		nautilus_gdk_pixbuf_ref_if_not_null (pixbuf);
		
		label->details->tile_pixbuf = pixbuf;

		gtk_widget_queue_resize (GTK_WIDGET (label));
	}
}

/**
 * nautilus_label_get_tile_pixbuf:
 *
 * @label: A NautilusLabel
 *
 * Return value: A reference to the tile_pixbuf.  Needs to be unreferenced with 
 * gdk_pixbuf_unref()
 */
GdkPixbuf*
nautilus_label_get_tile_pixbuf (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), NULL);

	nautilus_gdk_pixbuf_ref_if_not_null (label->details->tile_pixbuf);
	
	return label->details->tile_pixbuf;
}

void
nautilus_label_set_text_opacity (NautilusLabel *label,
				 int opacity)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (opacity >= NAUTILUS_OPACITY_FULLY_TRANSPARENT);
	g_return_if_fail (opacity <= NAUTILUS_OPACITY_FULLY_OPAQUE);

	label->details->text_opacity = opacity;

	gtk_widget_queue_draw (GTK_WIDGET (label));
}

int
nautilus_label_get_text_opacity (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), NAUTILUS_OPACITY_FULLY_OPAQUE);

	return label->details->text_opacity;
}

void
nautilus_label_set_tile_opacity (NautilusLabel *label,
				 int tile_opacity)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (tile_opacity >= NAUTILUS_OPACITY_FULLY_TRANSPARENT);
	g_return_if_fail (tile_opacity <= NAUTILUS_OPACITY_FULLY_OPAQUE);

	if (label->details->tile_opacity == tile_opacity) {
		return;
	}

	label->details->tile_opacity = tile_opacity;

	gtk_widget_queue_draw (GTK_WIDGET (label));
}

int
nautilus_label_get_tile_opacity (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), NAUTILUS_OPACITY_FULLY_OPAQUE);

	return label->details->tile_opacity;
}

void
nautilus_label_set_tile_width (NautilusLabel *label,
			       int tile_width)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (tile_width >= NAUTILUS_SMOOTH_TILE_EXTENT_ONE_STEP);
	
	if (label->details->tile_width == tile_width) {
		return;
	}

	label->details->tile_width = tile_width;

	gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * nautilus_label_get_tile_width:
 *
 * @label: A NautilusLabel
 *
 * Return value: The tile width.
 */
int
nautilus_label_get_tile_width (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	return label->details->tile_width;
}

void
nautilus_label_set_tile_height (NautilusLabel *label,
				int tile_height)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (tile_height >= NAUTILUS_SMOOTH_TILE_EXTENT_ONE_STEP);
	
	if (label->details->tile_height == tile_height) {
		return;
	}

	label->details->tile_height = tile_height;

	gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * nautilus_label_get_tile_height:
 *
 * @label: A NautilusLabel
 *
 * Return value: The tile height.
 */
int
nautilus_label_get_tile_height (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	return label->details->tile_height;
}

void
nautilus_label_set_tile_mode_vertical (NautilusLabel *label,
				       NautilusSmoothTileMode tile_mode_vertical)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (tile_mode_vertical >= NAUTILUS_SMOOTH_TILE_SELF);
	g_return_if_fail (tile_mode_vertical <= NAUTILUS_SMOOTH_TILE_ANCESTOR);

	if (label->details->tile_mode_vertical == tile_mode_vertical) {
		return;
	}

	label->details->tile_mode_vertical = tile_mode_vertical;

	gtk_widget_queue_draw (GTK_WIDGET (label));
}

NautilusSmoothTileMode
nautilus_label_get_tile_mode_vertical (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);
	
	return label->details->tile_mode_vertical;
}

void
nautilus_label_set_tile_mode_horizontal (NautilusLabel *label,
					 NautilusSmoothTileMode tile_mode_horizontal)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (tile_mode_horizontal >= NAUTILUS_SMOOTH_TILE_SELF);
	g_return_if_fail (tile_mode_horizontal <= NAUTILUS_SMOOTH_TILE_ANCESTOR);

	if (label->details->tile_mode_horizontal == tile_mode_horizontal) {
		return;
	}

	label->details->tile_mode_horizontal = tile_mode_horizontal;

	gtk_widget_queue_draw (GTK_WIDGET (label));
}

NautilusSmoothTileMode
nautilus_label_get_tile_mode_horizontal (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);
	
	return label->details->tile_mode_horizontal;
}

void
nautilus_label_set_tile_pixbuf_from_file_name (NautilusLabel *label,
					       const char *tile_file_name)
{
	GdkPixbuf *tile_pixbuf;

	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (tile_file_name != NULL);

	tile_pixbuf = gdk_pixbuf_new_from_file (tile_file_name);
	
	if (tile_pixbuf != NULL) {
		nautilus_label_set_tile_pixbuf (label, tile_pixbuf);
		gdk_pixbuf_unref (tile_pixbuf);
	}
}

void
nautilus_label_set_background_mode (NautilusLabel *label,
				    NautilusSmoothBackgroundMode background_mode)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (background_mode >= NAUTILUS_SMOOTH_BACKGROUND_GTK);
	g_return_if_fail (background_mode <= NAUTILUS_SMOOTH_BACKGROUND_SOLID_COLOR);

	if (label->details->background_mode == background_mode) {
		return;
	}

	label->details->background_mode = background_mode;

	gtk_widget_queue_draw (GTK_WIDGET (label));
}

NautilusSmoothBackgroundMode
nautilus_label_get_background_mode (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);
	
	return label->details->background_mode;
}

void
nautilus_label_set_solid_background_color (NautilusLabel *label,
					   guint32 solid_background_color)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	
	if (label->details->solid_background_color == solid_background_color) {
		return;
	}

	label->details->solid_background_color = solid_background_color;
	
	gtk_widget_queue_draw (GTK_WIDGET (label));
}

guint32
nautilus_label_get_solid_background_color (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);
	
	return label->details->solid_background_color;
}

/**
 * nautilus_label_set_wrap_width:
 *
 * @label: A NautilusLabel
 * @line_wrap_width: The new line wrap width.
 *
 * The line wrap width is something.
 * 
 */
void
nautilus_label_set_smooth_line_wrap_width (NautilusLabel *label,
					   guint line_wrap_width)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	
	if (label->details->smooth_line_wrap_width == line_wrap_width) {
		return;
	}
	
	label->details->smooth_line_wrap_width = line_wrap_width;
	
	label_line_geometries_recompute (label);
	
	gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * nautilus_label_get_smooth_line_wrap_width:
 *
 * @label: A NautilusLabel
 *
 * Return value: A boolean value indicating whether the label
 * is currently line wrapping text.
 */
guint
nautilus_label_get_smooth_line_wrap_width (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), FALSE);

	return label->details->smooth_line_wrap_width;
}

void
nautilus_label_set_text_color (NautilusLabel *label,
			       guint32 text_color)
{
	char *color_spec;

	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	if (label->details->smooth_text_color == text_color) {
		return;
	}
	
	label->details->smooth_text_color = text_color;

	color_spec = nautilus_gdk_rgb_to_color_spec (text_color);

	nautilus_gtk_widget_set_foreground_color (GTK_WIDGET (label), color_spec);
	
	g_free (color_spec);
	
	gtk_widget_queue_draw (GTK_WIDGET (label));
}

guint32
nautilus_label_get_text_color (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	return label->details->smooth_text_color;
}

/**
 * nautilus_label_set_smooth_drop_shadow_offset:
 *
 * @label: A NautilusLabel
 * @smooth_drop_shadow_offset: The new drop shadow offset.  

 * The drop shadow offset is specified in pixels.  If greater than zero,
 * the label will render on top of a nice shadow.  The shadow will be
 * offset from the label text by 'smooth_drop_shadow_offset' pixels.
 */
void
nautilus_label_set_smooth_drop_shadow_offset (NautilusLabel *label,
					      guint drop_shadow_offset)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	if (label->details->smooth_drop_shadow_offset == drop_shadow_offset) {
		return;
	}
	
	label->details->smooth_drop_shadow_offset = drop_shadow_offset;

	gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * nautilus_label_get_smooth_drop_shadow_offset:
 *
 * @label: A NautilusLabel
 *
 * Return value: The line offset in pixels.
 */
guint
nautilus_label_get_smooth_drop_shadow_offset (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	return label->details->smooth_drop_shadow_offset;
}

/**
 * nautilus_label_set_smooth_drop_shadow_color:
 *
 * @label: A NautilusLabel
 * @smooth_drop_shadow_color: The new drop shadow color.
 *
 * Return value: The drop shadow color.
 */
void
nautilus_label_set_smooth_drop_shadow_color (NautilusLabel *label,
					     guint32 drop_shadow_color)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	if (label->details->smooth_drop_shadow_color == drop_shadow_color) {
		return;
	}
	
	label->details->smooth_drop_shadow_color = drop_shadow_color;
	
	gtk_widget_queue_draw (GTK_WIDGET (label));
}

/**
 * nautilus_label_get_smooth_drop_shadow_color:
 *
 * @label: A NautilusLabel
 *
 * Return value: The drop shadow color.
 */
guint32
nautilus_label_get_smooth_drop_shadow_color (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);
	
	return label->details->smooth_drop_shadow_color;
}

void
nautilus_label_set_justify (NautilusLabel *label,
			    GtkJustification justification)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	g_return_if_fail (justification >= GTK_JUSTIFY_LEFT);
	g_return_if_fail (justification <= GTK_JUSTIFY_FILL);

	if (nautilus_label_get_text_justify (label) == justification) {
		return;
	}

	gtk_label_set_justify (GTK_LABEL (label), justification);
	
	gtk_widget_queue_draw (GTK_WIDGET (label));
}

GtkJustification
nautilus_label_get_text_justify (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), 0);

	return GTK_LABEL (label)->jtype;
}

void
nautilus_label_set_text (NautilusLabel *label,
			 const char *text)
{
	GtkLabel *gtk_label;

	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	gtk_label = GTK_LABEL (label);

	if (nautilus_str_is_equal (text, gtk_label->label)) {
		return;
	}

	gtk_label_set_text (gtk_label, text);
	
	label_line_geometries_recompute (label);

	gtk_widget_queue_resize (GTK_WIDGET (label));
}

char*
nautilus_label_get_text (const NautilusLabel *label)
{
	GtkLabel *gtk_label;

	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), NULL);

	gtk_label = GTK_LABEL (label);

	return gtk_label->label ? g_strdup (gtk_label->label) : NULL;
}

/**
 * nautilus_label_set_wrap:
 *
 * @label: A NautilusLabel
 * @line_wrap: A boolean value indicating whether the label should
 * line wrap words if they dont fit in the horizontally allocated
 * space.
 *
 */
void
nautilus_label_set_wrap (NautilusLabel *label,
			 gboolean line_wrap)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	if (nautilus_label_get_wrap (label) == line_wrap) {
		return;
	}

	gtk_label_set_line_wrap (GTK_LABEL (label), line_wrap);
	
	label_line_geometries_recompute (label);
	
	gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * nautilus_label_get_wrap:
 *
 * @label: A NautilusLabel
 *
 * Return value: A boolean value indicating whether the label
 * is currently line wrapping text.
 */
gboolean
nautilus_label_get_wrap (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), FALSE);

	return GTK_LABEL (label)->wrap;
}

/**
 * nautilus_label_new_solid:
 *
 * @text: Text or NULL
 * @drop_shadow_offset: Drop shadow offset.
 * @drop_shadow_color: Drop shadow color.
 * @text_color: Text color.
 * @x_alignment: Horizontal alignment.
 * @y_alignment: Vertical alignment.
 * @x_padding: Amount to pad label in the x direction.
 * @y_padding: Amount to pad label in the y direction.
 * @background_color: Background color.
 * @tile_pixbuf: Pixbuf to use for tile or NULL.
 *
 * Create a label with a solid background.
 *
 * Return value: Newly created label with all the given values.
 */
GtkWidget *
nautilus_label_new_solid (const char *text,
			  guint drop_shadow_offset,
			  guint32 drop_shadow_color,
			  guint32 text_color,
			  float x_alignment,
			  float y_alignment,
			  int x_padding,
			  int y_padding,
			  guint32 background_color,
			  GdkPixbuf *tile_pixbuf)
{
	NautilusLabel *label;

 	label = NAUTILUS_LABEL (nautilus_label_new (text ? text : ""));

 	nautilus_label_set_background_mode (NAUTILUS_LABEL (label), NAUTILUS_SMOOTH_BACKGROUND_SOLID_COLOR);
	nautilus_label_set_smooth_drop_shadow_color (label, drop_shadow_color);
	nautilus_label_set_smooth_drop_shadow_offset (label, drop_shadow_offset);
	nautilus_label_set_text_color (label, text_color);
 	nautilus_label_set_solid_background_color (NAUTILUS_LABEL (label), background_color);

 	gtk_misc_set_padding (GTK_MISC (label), x_padding, y_padding);
 	gtk_misc_set_alignment (GTK_MISC (label), x_alignment, y_alignment);
	
	if (tile_pixbuf != NULL) {
		nautilus_label_set_tile_pixbuf (NAUTILUS_LABEL (label), tile_pixbuf);
	}
	
	return GTK_WIDGET (label);
}

/**
 * nautilus_gtk_label_make_bold.
 *
 * Switches the font of label to a bold equivalent.
 * @label: The label.
 **/
void
nautilus_label_make_bold (NautilusLabel *label)
{
	NautilusScalableFont *bold_font;

	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	nautilus_gtk_label_make_bold (GTK_LABEL (label));

	bold_font = nautilus_scalable_font_make_bold (label->details->smooth_font);

	if (bold_font != NULL) {
		nautilus_label_set_smooth_font (label, bold_font);
		gtk_object_unref (GTK_OBJECT (bold_font));
	}
	
	label_line_geometries_recompute (label);

	gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * nautilus_gtk_label_make_larger.
 *
 * Switches the font of label to a larger version of the font.
 * @label: The label.
 **/
void
nautilus_label_make_larger (NautilusLabel *label,
			    guint num_steps)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	label->details->smooth_font_size += num_steps;

	nautilus_gtk_label_make_larger (GTK_LABEL (label), num_steps);

	label_line_geometries_recompute (label);

	gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * nautilus_gtk_label_make_smaller.
 *
 * Switches the font of label to a smaller version of the font.
 * @label: The label.
 **/
void
nautilus_label_make_smaller (NautilusLabel *label,
			     guint num_steps)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	label->details->smooth_font_size -= num_steps;
	
	nautilus_gtk_label_make_smaller (GTK_LABEL (label), num_steps);

	label_line_geometries_recompute (label);
	
	gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * nautilus_label_set_never_smooth
 *
 * @label: A NautilusLabel.
 * @never_smooth: A boolean value indicating whether the label can NEVER be smooth.
 *
 * Force an label to never be smooth.  Calls to nautilus_label_set_is_smooth () will
 * thus be ignored.  This is useful if you want to use a NautilusLabel in a situation
 * wherre smoothness does not make sense - for example, in a dialog with other "normal"
 * GtkLabel widgets for consistency.
 */
void
nautilus_label_set_never_smooth (NautilusLabel *label,
				 gboolean never_smooth)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));
	
	label->details->never_smooth = never_smooth;

	label_line_geometries_recompute (label);

	/* Force GtkLabel to flush its cached requisition dimensions.
	 * GtkLabel caches its requisition for efficiency.  We need this
	 * dimensions to be flushed when our is_smooth attribute changes.
	 * The reason is that the geometry of the widget is dependent on
	 * whether it is_smooth or not.
	 */
	label_force_cached_requisition_flush (label);

	gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * nautilus_label_set_adjust_wrap_on_resize:
 *
 * @label: A NautilusLabel
 * @adjust_wrap_on_resize: A boolean value indicating whether the label should
 * automatically update the line_wrap_width when its resized.
 */
void
nautilus_label_set_adjust_wrap_on_resize (NautilusLabel *label,
					  gboolean adjust_wrap_on_resize)
{
	g_return_if_fail (NAUTILUS_IS_LABEL (label));

	if (label->details->adjust_wrap_on_resize == adjust_wrap_on_resize) {
		return;
	}

	label->details->adjust_wrap_on_resize = adjust_wrap_on_resize;
	
	gtk_widget_queue_resize (GTK_WIDGET (label));
}

/**
 * nautilus_label_get_wrap:
 *
 * @label: A NautilusLabel
 *
 * Return value: A boolean value indicating whether the label
 * automatically updates the line_wrap_width when its resized.
 */
gboolean
nautilus_label_get_adjust_wrap_on_resize (const NautilusLabel *label)
{
	g_return_val_if_fail (NAUTILUS_IS_LABEL (label), FALSE);

	return label->details->adjust_wrap_on_resize;
}
