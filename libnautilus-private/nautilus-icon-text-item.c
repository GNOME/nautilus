/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-icon-text-item:  an editable text block with word wrapping for the
 * GNOME canvas.
 *
 * Copyright (C) 1998, 1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena <federico@gimp.org>
 *
 */

#include <config.h>
#include "nautilus-icon-text-item.h"

#include "nautilus-entry.h"
#include "nautilus-theme.h"

#include <libnautilus/nautilus-undo.h>
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-glib-extensions.h>

#include <math.h>
#include <stdio.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwindow.h>
#include <libgnome/gnome-i18n.h>
#include <libart_lgpl/art_rgb_affine.h>
#include <libart_lgpl/art_rgb_rgba_affine.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/* Margins used to display the information */
#define MARGIN_X 2
#define MARGIN_Y 2

/* Default fontset to be used if the user specified fontset is not found */
#define DEFAULT_FONT_NAME "-adobe-helvetica-medium-r-normal--*-100-*-*-*-*-*-*,"	\
			  "-*-*-medium-r-normal--10-*-*-*-*-*-*-*,*"

/* Separators for text layout */
#define DEFAULT_SEPARATORS " \t-.[]#"

/* Aliases to minimize screen use in my laptop */
#define ITI(x)       NAUTILUS_ICON_TEXT_ITEM (x)
#define ITI_CLASS(x) NAUTILUS_ICON_TEXT_ITEM_CLASS (x)
#define IS_ITI(x)    NAUTILUS_IS_ICON_TEXT_ITEM (x)

typedef NautilusIconTextItem Iti;

/* Signal callbacks */
static void register_rename_undo (NautilusIconTextItem *item);
static void restore_from_undo_snapshot_callback (GtkObject *target, gpointer callback_data);

/* Private part of the NautilusIconTextItem structure */
typedef struct {
	/* Font */
	GdkFont *font;

	/* Create an offscreen window and place an entry inside it */
	NautilusEntry *entry;
	GtkWidget *entry_top;

	/* Store min width and height.  These are used when the text entry is empty. */
	int min_width;
	int min_height;

	gboolean undo_registered;
		
} ItiPrivate;


static GnomeCanvasItemClass *parent_class;

enum {
	ARG_0,
};

enum {
	TEXT_CHANGED,
	TEXT_EDITED,
	HEIGHT_CHANGED,
	WIDTH_CHANGED,
	EDITING_STARTED,
	EDITING_STOPPED,
	SELECTION_STARTED,
	SELECTION_STOPPED,
	LAST_SIGNAL
};
static guint iti_signals [LAST_SIGNAL] = { 0 };

static void
send_focus_event (Iti *iti, gboolean in)
{
	ItiPrivate *priv;
	GtkWidget *widget;
	gboolean has_focus;
	GdkEvent fake_event;
	
	g_assert (in == FALSE || in == TRUE);

	priv = iti->priv;
	if (priv->entry == NULL) {
		g_assert (!in);
		return;
	}

	widget = GTK_WIDGET (priv->entry);
	has_focus = GTK_WIDGET_HAS_FOCUS (widget);
	if (has_focus == in) {
		return;
	}
	
	memset (&fake_event, 0, sizeof (fake_event));
	fake_event.focus_change.type = GDK_FOCUS_CHANGE;
	fake_event.focus_change.window = widget->window;
	fake_event.focus_change.in = in;
	gtk_widget_event (widget, &fake_event);
	g_assert (GTK_WIDGET_HAS_FOCUS (widget) == in);
}

/* Stops the editing state of an icon text item */
static void
iti_stop_editing (Iti *iti)
{
	ItiPrivate *priv;

	priv = iti->priv;

	iti->editing = FALSE;

	send_focus_event (iti, FALSE);

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));

	gtk_signal_emit (GTK_OBJECT (iti), iti_signals[EDITING_STOPPED]);
}

/* Lays out the text in an icon item */
static void
layout_text (Iti *iti)
{
	ItiPrivate *priv;
	char *text;
	int old_width, old_height;
	int width, height;

	priv = iti->priv;

	/* Save old size */
	if (iti->ti) {
		old_width = iti->ti->width + 2 * MARGIN_X;
		old_height = iti->ti->height + 2 * MARGIN_Y;

		gnome_icon_text_info_free (iti->ti);
	} else {
		old_width = 2 * MARGIN_X;
		old_height = 2 * MARGIN_Y;
	}

	/* Change the text layout */
	if (iti->editing) {
		text = gtk_entry_get_text (GTK_ENTRY(priv->entry));
	} else {
		text = iti->text;
	}

	iti->ti = gnome_icon_layout_text (priv->font,
					  text,
					  DEFAULT_SEPARATORS,
					  iti->max_text_width - 2 * MARGIN_X,
					  TRUE);

	/* Check the sizes and see if we need to emit any signals */
	width = iti->ti->width + 2 * MARGIN_X;
	height = iti->ti->height + 2 * MARGIN_Y;

	if (width != old_width)
		gtk_signal_emit (GTK_OBJECT (iti), iti_signals[WIDTH_CHANGED]);

	if (height != old_height)
		gtk_signal_emit (GTK_OBJECT (iti), iti_signals[HEIGHT_CHANGED]);
	
	gtk_signal_emit (GTK_OBJECT (iti), iti_signals [TEXT_EDITED]);		
}

/* Accepts the text in the off-screen entry of an icon text item */
static void
iti_edition_accept (Iti *iti)
{
	ItiPrivate *priv;
	gboolean accept;

	priv = iti->priv;
	accept = TRUE;

	gtk_signal_emit (GTK_OBJECT (iti), iti_signals [TEXT_CHANGED], &accept);

	if (iti->editing){
		if (accept) {
			if (iti->is_text_allocated)
				g_free (iti->text);

			iti->text = g_strdup (gtk_entry_get_text (GTK_ENTRY(priv->entry)));
			iti->is_text_allocated = 1;
		}

		iti_stop_editing (iti);
	}
	layout_text (iti);

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}


static void
iti_entry_text_changed_by_clipboard (GtkObject *widget,
				     gpointer data)
{
	Iti *iti;
	GnomeCanvasItem *item;
	ItiPrivate *priv;

	/* Update text item to reflect changes */
	iti = NAUTILUS_ICON_TEXT_ITEM (data);
	layout_text (iti);
	priv = iti->priv;

	item = GNOME_CANVAS_ITEM (iti);
	gnome_canvas_item_request_update (item);

}


/* Callback used when the off-screen entry of an icon text item is activated.
 * When this happens, we have to accept edition.
 */
static void
iti_entry_activate (GtkWidget *entry, Iti *iti)
{
	iti_edition_accept (iti);
}

/* Starts the editing state of an icon text item */
static void
iti_start_editing (Iti *iti)
{
	ItiPrivate *priv;

	priv = iti->priv;

	if (iti->editing) {
		return;
	}

	/* Trick: The actual edition of the entry takes place in a NautilusEntry
	 * which is placed offscreen.  That way we get all of the advantages
	 * from NautilusEntry without duplicating code.  Yes, this is a hack.
	 */

	if (priv->entry_top == NULL) {
		priv->entry = (NautilusEntry *) nautilus_entry_new ();
		gtk_signal_connect (GTK_OBJECT (priv->entry), "activate",
				    GTK_SIGNAL_FUNC (iti_entry_activate), iti);
		/* Make clipboard functions cause an update the appearance of 
		   the icon text item itself, since the clipboard functions 
		   will change the offscreen entry */
		gtk_signal_connect_after (GTK_OBJECT (priv->entry), "changed",
					  GTK_SIGNAL_FUNC (iti_entry_text_changed_by_clipboard), iti);

		priv->entry_top = gtk_window_new (GTK_WINDOW_POPUP);
		gtk_container_add (GTK_CONTAINER (priv->entry_top), GTK_WIDGET (priv->entry));
		gtk_widget_set_uposition (priv->entry_top, 20000, 20000);
		gtk_widget_show_all (priv->entry_top);
	}

	gtk_entry_set_text (GTK_ENTRY(priv->entry), iti->text);

	gtk_editable_select_region (GTK_EDITABLE (priv->entry), 0, -1);

	iti->editing = TRUE;

	send_focus_event (iti, TRUE);

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));

	gtk_signal_emit (GTK_OBJECT (iti), iti_signals[EDITING_STARTED]);
}

/* Destroy method handler for the icon text item */
static void
iti_destroy (GtkObject *object)
{
	Iti *iti;
	ItiPrivate *priv;
	GnomeCanvasItem *item;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_ITI (object));

	iti = ITI (object);
	priv = iti->priv;
	item = GNOME_CANVAS_ITEM (object);

	/* Queue redraw of bounding box */
	gnome_canvas_request_redraw (item->canvas,
				     eel_round (item->x1),
				     eel_round (item->y1),
				     eel_round (item->x2),
				     eel_round (item->y2));

	/* Free everything */
	if (iti->font)
		gdk_font_unref (iti->font);

	if (iti->text && iti->is_text_allocated)
		g_free (iti->text);

	if (iti->ti)
		gnome_icon_text_info_free (iti->ti);

	if (priv->font)
		gdk_font_unref (priv->font);

	if (priv->entry_top) {
		gtk_widget_destroy (priv->entry_top);
	}

	g_free (priv);

	if (GTK_OBJECT_CLASS (parent_class)->destroy)
		(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}

/* set_arg handler for the icon text item */
static void
iti_set_arg (GtkObject *object, GtkArg *arg, guint arg_id)
{
	Iti *iti;
	ItiPrivate *priv;

	iti = ITI (object);
	priv = iti->priv;

	switch (arg_id) {
	default:
		break;
	}
}

/* Recomputes the bounding box of an icon text item */
static void
recompute_bounding_box (Iti *iti)
{
	GnomeCanvasItem *item;
	int width_c, height_c;
	double width_w, height_w;
	double x1, y1, x2, y2;
	ItiPrivate *priv;

	item = GNOME_CANVAS_ITEM (iti);

	priv = iti->priv;

	/* Compute width, height - scaled to world coords
	 */
	width_c  = iti->ti->width  + 2 * MARGIN_X;
	height_c = iti->ti->height + 2 * MARGIN_Y;
	
	/* Verify we are not smaller than default settings
	 */
	if (width_c < priv->min_width) {
		width_c = priv->min_width;
	}
	if (height_c < priv->min_height) {
		height_c = priv->min_height;
	}

	width_w  = width_c  / item->canvas->pixels_per_unit;
	height_w = height_c / item->canvas->pixels_per_unit;

	/* start with item coords
	 */
	x1 = iti->x_center;
	y1 = iti->y_top;
	
	/* do computations in world coords
	 */
	gnome_canvas_item_i2w (item, &x1, &y1);
	x1 -= width_w / 2;
	y1 -= MARGIN_Y / item->canvas->pixels_per_unit;
	x2 = x1 + width_w;
	y2 = y1 + height_w;

	/* store canvas coords in item
	 */
	gnome_canvas_w2c_d (item->canvas, x1, y1, &item->x1, &item->y1);
	gnome_canvas_w2c_d (item->canvas, x2, y2, &item->x2, &item->y2);
}

/* Update method for the icon text item */
static void
iti_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	Iti *iti;

	iti = ITI (item);

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	gnome_canvas_request_redraw (item->canvas,
				     eel_round (item->x1),
				     eel_round (item->y1),
				     eel_round (item->x2),
				     eel_round (item->y2));
	recompute_bounding_box (iti);
	gnome_canvas_request_redraw (item->canvas,
				     eel_round (item->x1),
				     eel_round (item->y1),
				     eel_round (item->x2),
				     eel_round (item->y2));
}

/* utility to fetch a color from a theme */
static void
fetch_themed_color (const char *property_name, GdkColor *color)
{
	char *color_string;
	
	color_string = nautilus_theme_get_theme_data ("icon", property_name);
	if (color_string == NULL) {
		color_string = g_strdup ("rgb:FFFF/FFFF/FFFF");
	}
	gdk_color_parse (color_string, color);
	g_free (color_string);
}

/* Draw the icon text item's text when it is being edited */
static void
iti_paint_text (Iti *iti, GdkDrawable *drawable, int x, int y)
{
	ItiPrivate *priv;
	GnomeIconTextInfoRow *row;
	GnomeIconTextInfo *ti;
	GtkStyle *style;
	GdkGC *fg_gc, *bg_gc;
	GdkGC *gc, *bgc, *sgc, *bsgc;
	GList *item;
	GdkGCValues save_gc;
	GdkColor highlight_background_color, highlight_text_color, fill_color;
	int xpos, len;
	int cursor, offset, i;
	GnomeCanvasItem *canvas_item;

	xpos = 0;
	offset = 0;
	priv = iti->priv;
	style = GTK_WIDGET (GNOME_CANVAS_ITEM (iti)->canvas)->style;

	ti = iti->ti;
	len = 0;
	y += ti->font->ascent;

	cursor = 0;
	i = -1;
	
	/*
	 * Pointers to all of the GCs we use
	 */
	gc = style->fg_gc [GTK_STATE_NORMAL];
	bgc = style->bg_gc [GTK_STATE_NORMAL];
	sgc = style->fg_gc [GTK_STATE_SELECTED];
	bsgc = style->bg_gc [GTK_STATE_SELECTED];

	/* fetch the colors from the theme */
	fetch_themed_color ("highlight_background_color",  &highlight_background_color);
	fetch_themed_color ("highlight_text_color",  &highlight_text_color);
	fetch_themed_color ("text_fill_color",  &fill_color);
	
	/* Set up user defined colors */
	canvas_item = GNOME_CANVAS_ITEM (iti);
	gdk_colormap_alloc_color
		(gtk_widget_get_colormap (GTK_WIDGET (canvas_item->canvas)),
	 	&highlight_background_color, FALSE, TRUE);
	gdk_colormap_alloc_color
		(gtk_widget_get_colormap (GTK_WIDGET (canvas_item->canvas)),
	 	&highlight_text_color, FALSE, TRUE);
	gdk_colormap_alloc_color
		(gtk_widget_get_colormap (GTK_WIDGET (canvas_item->canvas)),
	 	&fill_color, FALSE, TRUE);
	
	for (item = ti->rows; item; item = item->next, len += (row ? row->text_length : 0)) {
		GdkWChar *text_wc;
		int text_length;		
		int sel_start, sel_end;

		row = item->data;

		if (!row) {
			y += ti->baseline_skip / 2;
			continue;
		}
		
		text_wc = row->text_wc;
		text_length = row->text_length;

		xpos = (ti->width - row->width) / 2;

		sel_start = GTK_EDITABLE (priv->entry)->selection_start_pos - len;
		sel_end   = GTK_EDITABLE (priv->entry)->selection_end_pos - len;
		offset    = 0;
		cursor    = GTK_EDITABLE (priv->entry)->current_pos - len;

		for (i = 0; *text_wc; text_wc++, i++) {
			int size, px;

			size = gdk_text_width_wc (ti->font, text_wc, 1);
			px = x + xpos + offset;
			
			if (i >= sel_start && i < sel_end) {
				/* Draw selection */
				fg_gc = sgc;
				bg_gc = bsgc;
				
				gdk_gc_get_values (bg_gc, &save_gc);
				
				gdk_gc_set_foreground (bg_gc, &highlight_background_color);
				gdk_draw_rectangle (drawable,
					    bg_gc,
					    TRUE,
					    px,
					    y - ti->font->ascent,
					    size, ti->baseline_skip);

				gdk_gc_set_foreground (bg_gc, &highlight_text_color);
				gdk_draw_text_wc (drawable,
					  ti->font,
					  fg_gc,
					  px, y,
					  text_wc, 1);
			} else {
				/* Draw unselected area */
				fg_gc = gc;
				bg_gc = bgc;
				gdk_gc_get_values (bg_gc, &save_gc);

				gdk_gc_set_foreground (bg_gc, &fill_color);
				gdk_draw_rectangle (drawable,
					    bg_gc,
					    TRUE,
					    px,
					    y - ti->font->ascent,
					    size, ti->baseline_skip);
					    
				gdk_draw_text_wc (drawable,
					  ti->font,
					  fg_gc,
					  px, y,
					  text_wc, 1);
			}
														  
			if (cursor == i) {
				gdk_draw_line (drawable,
					       gc,
					       px,
					       y - ti->font->ascent,
					       px,
					       y + ti->font->descent - 1);
			}
			offset += size;

			/* Restore colors */
			gdk_gc_set_foreground(bg_gc, &save_gc.foreground);
			gdk_gc_set_background(bg_gc, &save_gc.background);
		}
		
		y += ti->baseline_skip;
	}

	/* The i-beam should only be drawn at the end of a line of text if that line is the
	 * only or last line of text in a label.  We subtract one from the x position
	 * so the i-beam is not visually jammed against the edge of the bounding rect. */
	if (cursor == i) {
		int px = x + xpos + offset;
		y -= ti->baseline_skip;
		
		gdk_draw_line (drawable,
			       gc,
			       px - 1,
			       y - ti->font->ascent,
			       px - 1,
			       y + ti->font->descent - 1);
	}

}

/* Draw method handler for the icon text item */
static void
iti_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int update_width, int update_height)
{
	Iti *iti;
	GtkStyle *style;
	int width, height;
	int xofs, yofs;

	iti = ITI (item);

	width  = eel_round (item->x2 - item->x1);
	height = eel_round (item->y2 - item->y1);
	
	xofs = eel_round (item->x1) - x;
	yofs = eel_round (item->y1) - y;

	style = GTK_WIDGET (item->canvas)->style;

	if (iti->editing) {
		/* Draw outline around text */
		gdk_draw_rectangle (drawable,
				    style->fg_gc[GTK_STATE_NORMAL],
				    FALSE,
				    xofs, yofs,
				    width - 1, height - 1);
		
		iti_paint_text (iti, drawable, xofs + MARGIN_X, yofs + MARGIN_Y);
	} else {
		g_message ("Drawing, but not editing!!!!");
		if (iti->selected) {
			gdk_draw_rectangle (drawable,
				    style->bg_gc[GTK_STATE_SELECTED],
				    TRUE,
				    xofs, yofs,
				    width, height);
		}
		gnome_icon_paint_text (iti->ti,
				       drawable,
				       style->fg_gc[(iti->selected
						     ? GTK_STATE_SELECTED
						     : GTK_STATE_NORMAL)],
				       xofs + MARGIN_X,
				       yofs + MARGIN_Y,
				       GTK_JUSTIFY_CENTER);
	}
}

/* utility to draw a pixbuf to the anti-aliased canvas */

static void
draw_pixbuf_aa (GdkPixbuf *pixbuf, GnomeCanvasBuf *buf, double affine[6], int x_offset, int y_offset)
{
	void (* affine_function)
		(art_u8 *dst, int x0, int y0, int x1, int y1, int dst_rowstride,
		 const art_u8 *src, int src_width, int src_height, int src_rowstride,
		 const double affine[6],
		 ArtFilterLevel level,
		 ArtAlphaGamma *alpha_gamma);

	affine[4] += x_offset;
	affine[5] += y_offset;

	affine_function = gdk_pixbuf_get_has_alpha (pixbuf)
		? art_rgb_rgba_affine
		: art_rgb_affine;
	
	(* affine_function)
		(buf->buf,
		 buf->rect.x0, buf->rect.y0,
		 buf->rect.x1, buf->rect.y1,
		 buf->buf_rowstride,
		 gdk_pixbuf_get_pixels (pixbuf),
		 gdk_pixbuf_get_width (pixbuf),
		 gdk_pixbuf_get_height (pixbuf),
		 gdk_pixbuf_get_rowstride (pixbuf),
		 affine,
		 ART_FILTER_NEAREST,
		 NULL);

	affine[4] -= x_offset;
	affine[5] -= y_offset;
}

static void
iti_render (GnomeCanvasItem *item, GnomeCanvasBuf *buffer)
{
	GdkVisual *visual;
	GdkGC *gc;
	GdkColormap *colormap;
	GdkPixmap *pixmap;
	GdkPixbuf *text_pixbuf;
	double affine[6];
	int width, height;

	visual = gdk_visual_get_system ();
	art_affine_identity(affine);
	width  = eel_round (item->x2 - item->x1);
	height = eel_round (item->y2 - item->y1);
	
	/* allocate a pixmap to draw the text into, and clear it to white */
	pixmap = gdk_pixmap_new (NULL, width, height, visual->depth);

	gc = gdk_gc_new (pixmap);

	gdk_rgb_gc_set_foreground (gc, EEL_RGB_COLOR_WHITE);
	gdk_draw_rectangle (pixmap, gc, TRUE,
			    0, 0,
			    width,
			    height);
	gdk_gc_unref (gc);
	
	/* use a common routine to draw the label into the pixmap */
	iti_draw (item, pixmap, eel_round (item->x1), eel_round (item->y1), width, height);
	
	/* turn it into a pixbuf */
	colormap = gdk_colormap_new (visual, FALSE);
	text_pixbuf = gdk_pixbuf_get_from_drawable
		(NULL, pixmap, colormap,
		 0, 0,
		 0, 0, 
		 width, 
		 height);
	
	gdk_colormap_unref (colormap);
	gdk_pixmap_unref (pixmap);
		
	/* draw the pixbuf containing the label */
	draw_pixbuf_aa (text_pixbuf, buffer, affine, eel_round (item->x1), eel_round (item->y1));
	gdk_pixbuf_unref (text_pixbuf);

	buffer->is_bg = FALSE;
	buffer->is_buf = TRUE;
}


/* Point method handler for the icon text item */
static double
iti_point (GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item)
{
	double dx, dy;
	double cx_d, cy_d;

	*actual_item = item;

	gnome_canvas_w2c_d (item->canvas, x, y, &cx_d, &cy_d);

	if (cx_d < item->x1) {
		dx = item->x1 - cx_d;
	} else if (cx_d > item->x2) {
		dx = cx_d - item->x2;
	} else {
		dx = 0.0;
	}

	if (cy_d < item->y1) {
		dy = item->y1 - cy_d;
	} else if (cy_d > item->y2) {
		dy = cy_d - item->y2;
	} else {
		dy = 0.0;
	}

	return sqrt (dx * dx + dy * dy);
}

/* Given X, Y, a mouse position, return a valid index inside the edited text */
static int
iti_idx_from_x_y (Iti *iti, int x, int y)
{
	ItiPrivate *priv;
	GnomeIconTextInfoRow *row;
	int lines;
	int line, col, i, idx;
	GList *l;

	priv = iti->priv;

	if (iti->ti->rows == NULL)
		return 0;

	lines = g_list_length (iti->ti->rows);
	line = y / iti->ti->baseline_skip;

	if (line < 0)
		line = 0;
	else if (lines < line + 1)
		line = lines - 1;

	/* Compute the base index for this line */
	for (l = iti->ti->rows, idx = i = 0; i < line; l = l->next, i++) {
		row = l->data;
		idx += row->text_length;
	}

	row = g_list_nth (iti->ti->rows, line)->data;
	col = 0;
	if (row != NULL) {
		int first_char;
		int last_char;

		first_char = (iti->ti->width - row->width) / 2;
		last_char = first_char + row->width;

		if (x < first_char) {
			/* nothing */
		} else if (x > last_char) {
			col = row->text_length;
		} else {
			GdkWChar *s = row->text_wc;
			int pos = first_char;

			while (pos < last_char) {
				pos += gdk_text_width_wc (iti->ti->font, s, 1);
				if (pos > x)
					break;
				col++;
				s++;
			}
		}
	}

	idx += col;

	g_assert (idx <= GTK_ENTRY(priv->entry)->text_size);

	return idx;
}

/* Starts the selection state in the icon text item */
static void
iti_start_selecting (Iti *iti, int idx, guint32 event_time)
{
	ItiPrivate *priv;
	GtkEditable *e;
	GdkCursor *ibeam;

	priv = iti->priv;
	e = GTK_EDITABLE (priv->entry);

	gtk_editable_select_region (e, idx, idx);
	gtk_editable_set_position (e, idx);
	ibeam = gdk_cursor_new (GDK_XTERM);
	gnome_canvas_item_grab (GNOME_CANVAS_ITEM (iti),
				GDK_BUTTON_RELEASE_MASK |
				GDK_POINTER_MOTION_MASK,
				ibeam, event_time);
	gdk_cursor_destroy (ibeam);

	gtk_editable_select_region (e, idx, idx);
	e->current_pos = e->selection_start_pos;
	e->has_selection = TRUE;
	iti->selecting = TRUE;

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));

	gtk_signal_emit (GTK_OBJECT (iti), iti_signals[SELECTION_STARTED]);
}

/* Stops the selection state in the icon text item */
static void
iti_stop_selecting (Iti *iti, guint32 event_time)
{
	ItiPrivate *priv;
	GnomeCanvasItem *item;
	GtkEditable *e;

	priv = iti->priv;
	item = GNOME_CANVAS_ITEM (iti);
	e = GTK_EDITABLE (priv->entry);

	gnome_canvas_item_ungrab (item, event_time);
	e->has_selection = FALSE;
	iti->selecting = FALSE;

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
	gtk_signal_emit (GTK_OBJECT (iti), iti_signals[SELECTION_STOPPED]);
	/* Hack, since the real nautilus entry can't get this information */
	gtk_signal_emit_by_name (GTK_OBJECT (priv->entry), "selection_changed");
}

/* Handles selection range changes on the icon text item */
static void
iti_selection_motion (Iti *iti, int idx)
{
	ItiPrivate *priv;
	GtkEditable *e;

	g_assert (idx >= 0);
	
	priv = iti->priv;
	e = GTK_EDITABLE (priv->entry);

	if (idx < (int) e->current_pos) {
		e->selection_start_pos = idx;
		e->selection_end_pos = e->current_pos;
	} else {
		e->selection_start_pos = e->current_pos;
		e->selection_end_pos  = idx;
	}

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}

/* Ensure the item gets focused (both globally, and local to Gtk) */
static void
iti_ensure_focus (GnomeCanvasItem *item)
{
	GtkWidget *toplevel;

        /* gnome_canvas_item_grab_focus still generates focus out/in
         * events when focused_item == item
         */
        if (GNOME_CANVAS_ITEM (item)->canvas->focused_item != item) {
        	gnome_canvas_item_grab_focus (GNOME_CANVAS_ITEM (item));
        }

	toplevel = gtk_widget_get_toplevel (GTK_WIDGET (item->canvas));
	if (toplevel != NULL && GTK_WIDGET_REALIZED (toplevel)) {
		eel_gdk_window_focus (toplevel->window, GDK_CURRENT_TIME);
	}
}

/* Position insertion point based on arrow key event */
static void
iti_handle_arrow_key_event (NautilusIconTextItem *iti, GdkEvent *event)
{
	ItiPrivate *priv;
	GnomeIconTextInfoRow *row;
	GList *list;
	GtkEditable *editable;
	int index, lines, line;
	int position, pos_count, new_position;
	int cur_count, prev_count, next_count;
	float scale;
	
	/* Get number of lines.  Do nothing if we have only one line */
	lines = g_list_length (iti->ti->rows);
	if (lines <= 1) {
		return;
	}

	/* Figure out which line the current insertion point is on */
	priv = iti->priv;
	editable = GTK_EDITABLE (priv->entry);
	pos_count = position = gtk_editable_get_position (editable);
	for (list = iti->ti->rows, line = -1, index = 1; index <= lines; index++) {				
		if (list != NULL) {
			row = list->data;			
			if (pos_count > row->text_length) {		
				list = list->next;
				pos_count -= row->text_length;
			}
			else {
				line = index;				
				break;
			}
		}
	}
	
	/* Calculate new position of insertion point */
	switch (event->key.keyval) {

		case GDK_Up:
			/* Try to set insertion point to previous line */
			if (line > 1) {
				list = g_list_nth(iti->ti->rows, line - 1);
				row = list->data;
				cur_count = row->text_length;
				list = g_list_nth(iti->ti->rows, line - 2);
				row = list->data;				
				prev_count = row->text_length;
				scale = (float)prev_count / (float)cur_count;
				new_position = pos_count * scale;
				position -= prev_count + pos_count;
				position += new_position;
				gtk_editable_set_position (editable, position);
			}
			break;
			
		case GDK_Down:
			/* Try to set insertion point to next line */
			if (line < lines) {
				int new_position;
				list = g_list_nth(iti->ti->rows, line - 1);
				row = list->data;
				cur_count = row->text_length;
				list = g_list_nth(iti->ti->rows, line);
				row = list->data;				
				next_count = row->text_length;
				scale = (float)next_count / (float)cur_count;
				new_position = pos_count * scale;
				new_position += (position + (cur_count - pos_count));
				gtk_editable_set_position (editable, new_position);
			}
			break;

		default:
			break;

	}

}

/* Event handler for icon text items */
static gint
iti_event (GnomeCanvasItem *item, GdkEvent *event)
{
	Iti *iti;
	ItiPrivate *priv;
	int idx;
	double cx, cy;
	
	iti = ITI (item);
	priv = iti->priv;

	switch (event->type) {
	case GDK_KEY_PRESS:
		if (!iti->editing) {
			break;
		}
		
		switch(event->key.keyval) {
		
		/* Pass these events back to parent */		
		case GDK_Escape:
		case GDK_Return:
		case GDK_KP_Enter:
			return FALSE;

		/* Handle up and down arrow keys.  GdkEntry does not know 
		 * how to handle multi line items */
		case GDK_Up:		
		case GDK_Down:
			iti_handle_arrow_key_event(iti, event);
			break;
			
		default:			
			/* Check for control key operations */
			if (event->key.state & GDK_CONTROL_MASK) {
				return FALSE;
			}
			
			/* Register undo transaction if neccessary */	
			if (!priv->undo_registered) {
				priv->undo_registered = TRUE;
				register_rename_undo (iti);
			}

			/* Handle any events that reach us */
			gtk_widget_event (GTK_WIDGET (priv->entry), event);
			break;
		}
		
		/* Update text item to reflect changes */
		layout_text (iti);
		gnome_canvas_item_request_update (item);
		return TRUE;

	case GDK_BUTTON_PRESS:
		if (!iti->editing) {
			break;
		}

		if (event->button.button == 1) {
			gnome_canvas_w2c_d (item->canvas, event->button.x, event->button.y, &cx, &cy);						
			idx = iti_idx_from_x_y (iti,
						eel_round (cx - (item->x1 + MARGIN_X)),
						eel_round (cy - (item->y1 + MARGIN_Y)));
			iti_start_selecting (iti, idx, event->button.time);
		}
		return TRUE;

	case GDK_MOTION_NOTIFY:
		if (!iti->selecting)
			break;

		gtk_widget_event (GTK_WIDGET (priv->entry), event);
		gnome_canvas_w2c_d (item->canvas, event->button.x, event->button.y, &cx, &cy);			
		idx = iti_idx_from_x_y (iti,
					eel_round (cx - (item->x1 + MARGIN_X)),
					eel_round (cy - (item->y1 + MARGIN_Y)));
		iti_selection_motion (iti, idx);
		return TRUE;

	case GDK_BUTTON_RELEASE:
		if (iti->selecting && event->button.button == 1)
			iti_stop_selecting (iti, event->button.time);
		else
			break;

		return TRUE;

	case GDK_FOCUS_CHANGE:
		/* FIXME bugzilla.gnome.org 45484:
		 * Working around bug in the GnomeCanvas widget's focus_in/focus_out
		 * methods. They (all widgets) should be setting/usetting these flags.
		 * GnomeCanvas doesn't. We need it set so the GtkWindow passes us all
		 * focus out events. Once GnomeCanvas is fixed, this can be removed.
		 */
		if (event->focus_change.in) {
			GTK_WIDGET_SET_FLAGS (item->canvas, GTK_HAS_FOCUS);
		} else {
			GTK_WIDGET_UNSET_FLAGS (item->canvas, GTK_HAS_FOCUS);
		}

		if (iti->editing) {
			gtk_widget_event (GTK_WIDGET (priv->entry), event);

			if (!event->focus_change.in) {
				iti_edition_accept (iti);
			}
		}
		return TRUE;

	default:
		break;
	}

	return FALSE;
}

/* Bounds method handler for the icon text item */
static void
iti_bounds (GnomeCanvasItem *item, double *x1, double *y1, double *x2, double *y2)
{
	Iti *iti;
	int width_c, height_c;
	double width_w, height_w;

	iti = ITI (item);

	if (iti->ti) {
		width_c  = iti->ti->width  + 2 * MARGIN_X;
		height_c = iti->ti->height + 2 * MARGIN_Y;
	} else {
		width_c  = 2 * MARGIN_X;
		height_c = 2 * MARGIN_Y;
	}

	width_w  = width_c / item->canvas->pixels_per_unit;
	height_w = height_c / item->canvas->pixels_per_unit;

	/* start with item coords
	 */
	*x1 = iti->x_center;
	*y1 = iti->y_top;
	
	/* do computations in world coords
	 */
	gnome_canvas_item_i2w (item, x1, y1);
	*x1 -= width_w / 2;
	*y1 -= MARGIN_Y / item->canvas->pixels_per_unit;
	*x2 = *x1 + width_w;
	*y2 = *y1 + height_w;

	/* convert back to item coords
	 */
	gnome_canvas_item_w2i (item, x1, y1);
	gnome_canvas_item_w2i (item, x2, y2);
}

/* Class initialization function for the icon text item */
static void
iti_class_init (NautilusIconTextItemClass *text_item_class)
{
	GtkObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) text_item_class;
	item_class   = (GnomeCanvasItemClass *) text_item_class;

	parent_class = gtk_type_class (gnome_canvas_item_get_type ());

	iti_signals [TEXT_CHANGED] = gtk_signal_new
		("text_changed",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, text_changed),
		 gtk_marshal_BOOL__NONE,
		 GTK_TYPE_BOOL, 0);

	iti_signals [TEXT_EDITED] = gtk_signal_new
		("text_edited",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, text_edited),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);

	iti_signals [HEIGHT_CHANGED] = gtk_signal_new
		("height_changed",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, height_changed),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);

	iti_signals [WIDTH_CHANGED] = gtk_signal_new
		("width_changed",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, width_changed),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);

	iti_signals[EDITING_STARTED] = gtk_signal_new
		("editing_started",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, editing_started),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);

	iti_signals[EDITING_STOPPED] = gtk_signal_new
		("editing_stopped",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, editing_stopped),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);

	iti_signals[SELECTION_STARTED] = gtk_signal_new
		("selection_started",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, selection_started),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);

	iti_signals[SELECTION_STOPPED] = gtk_signal_new
		("selection_stopped",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, selection_stopped),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, iti_signals, LAST_SIGNAL);

	object_class->destroy = iti_destroy;
	object_class->set_arg = iti_set_arg;

	item_class->update = iti_update;
	item_class->draw = iti_draw;
	item_class->render = iti_render;
	item_class->point = iti_point;
	item_class->bounds = iti_bounds;
	item_class->event = iti_event;
}

/* Object initialization function for the icon text item */
static void
iti_init (NautilusIconTextItem *iti)
{
	ItiPrivate *priv;

	priv = g_new0 (ItiPrivate, 1);
	iti->priv = priv;
}




/**
 * nautilus_icon_text_item_configure:
 * @iti: An icon text item.
 * @x_center: X position of item's center - item coords.
 * @y_top: Y position of item's top - item coords.
 * @width: Maximum width allowed for this item, to be used for word wrapping.
 * @font: Name of the fontset that should be used to display the text.
 * @text: Text that is going to be displayed.
 * @is_editable: Deprecated.
 * @is_static: Whether @text points to a static string or not.
 *
 * This routine is used to configure a &NautilusIconTextItem.
 *
 * @x and @y specify the cordinates where the item is placed inside the canvas.
 * The @x coordinate should be the leftmost position that the icon text item can
 * assume at any one time, that is, the left margin of the column in which the
 * icon is to be placed.  The @y coordinate specifies the top of the icon text
 * item.
 *
 * @width is the maximum width allowed for this icon text item.  The coordinates
 * define the upper-left corner of an icon text item with maximum width; this may
 * actually be outside the bounding box of the item if the text is narrower than
 * the maximum width.
 *
 * If @is_static is true, it means that there is no need for the item to
 * allocate memory for the string (it is a guarantee that the text is allocated
 * by the caller and it will not be deallocated during the lifetime of this
 * item).  This is an optimization to reduce memory usage for large icon sets.
 */
void
nautilus_icon_text_item_configure (NautilusIconTextItem *iti, double x_center, double y_top,
				int max_text_width, GdkFont *font,
				const char *text, gboolean is_static)
{
	ItiPrivate *priv;
	GnomeIconTextInfo *min_text_info;

	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));
	g_return_if_fail (max_text_width > 2 * MARGIN_X);
	g_return_if_fail (text != NULL);

	priv = iti->priv;

	iti->x_center = x_center;
	iti->y_top = y_top;
	iti->max_text_width = max_text_width;

	if (iti->text && iti->is_text_allocated)
		g_free (iti->text);

	iti->is_text_allocated = !is_static;

	/* This cast is to shut up the compiler */
	if (is_static)
		iti->text = (char *) text;
	else
		iti->text = g_strdup (text);

	if (iti->font)
		gdk_font_unref (iti->font);

	iti->font = gdk_font_ref (font);

	if (priv->font)
		gdk_font_unref (priv->font);

	priv->font = NULL;
	if (font)
		priv->font = gdk_font_ref (iti->font);
	if (!priv->font)
		priv->font = gdk_fontset_load (DEFAULT_FONT_NAME);

	layout_text (iti);

	/* Calculate and store min and max dimensions */	
	min_text_info = gnome_icon_layout_text (priv->font,
					  " ",
					  DEFAULT_SEPARATORS,
					  iti->max_text_width - 2 * MARGIN_X,
					  TRUE);

	priv->min_width  = min_text_info->width  + 2 * MARGIN_X;
	priv->min_height = min_text_info->height + 2 * MARGIN_Y;
	gnome_icon_text_info_free(min_text_info);

	priv->undo_registered = FALSE;

	/* Request update */
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}

/**
 * nautilus_icon_text_item_setxy:
 * @iti:  An icon text item.
 * @x_center: X position of item's center - item coords.
 * @y_top: Y position of item's top - item coords.
 *
 * Sets the coordinates at which the icon text item should be placed.
 *
 * See also: nautilus_icon_text_item_configure().
 */
void
nautilus_icon_text_item_setxy (NautilusIconTextItem *iti, double x_center, double y_top)
{
	ItiPrivate *priv;

	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));

	priv = iti->priv;

	iti->x_center = x_center;
	iti->y_top = y_top;

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}

/**
 * nautilus_icon_text_item_select:
 * @iti: An icon text item
 * @sel: Whether the icon text item should be displayed as selected.
 *
 * This function is used to control whether an icon text item is displayed as
 * selected or not.  Mouse events are ignored by the item when it is unselected;
 * when the user clicks on a selected icon text item, it will start the text
 * editing process.
 */
void
nautilus_icon_text_item_select (NautilusIconTextItem *iti, int sel)
{
	ItiPrivate *priv;

	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));

	priv = iti->priv;

	if (!iti->selected == !sel)
		return;

	iti->selected = sel ? TRUE : FALSE;

	if (!iti->selected && iti->editing) {
		iti_edition_accept (iti);
	}

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}

/**
 * nautilus_icon_text_item_set_text:
 * @iti: An icon text item.
 *
 * Returns the current text string in an icon text item.  The client should not
 * free this string, as it is internal to the icon text item.
 */
void
nautilus_icon_text_item_set_text (NautilusIconTextItem *iti, const char *text)
{
	ItiPrivate *priv;

	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));

	priv = iti->priv;

	gtk_entry_set_text (GTK_ENTRY(priv->entry), text);
	gtk_editable_select_region (GTK_EDITABLE (priv->entry), 0, -1);

	layout_text (iti);

	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}

/**
 * nautilus_icon_text_item_get_text:
 * @iti: An icon text item.
 *
 * Returns the current text string in an icon text item.  The client should not
 * free this string, as it is internal to the icon text item.
 */
const char *
nautilus_icon_text_item_get_text (NautilusIconTextItem *iti)
{
	ItiPrivate *priv;

	g_return_val_if_fail (iti != NULL, NULL);
	g_return_val_if_fail (IS_ITI (iti), NULL);

	priv = iti->priv;

	if (iti->editing) {
		return gtk_entry_get_text (GTK_ENTRY(priv->entry));
	} else {
		return iti->text;
	}
}


/**
 * nautilus_icon_text_item_start_editing:
 * @iti: An icon text item.
 *
 * Starts the editing state of an icon text item.
 **/
void
nautilus_icon_text_item_start_editing (NautilusIconTextItem *iti)
{
	ItiPrivate *priv;

	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));

	priv = iti->priv;

	if (iti->editing) {
		return;
	}

	iti->selected = TRUE; /* Ensure that we are selected */
	iti_ensure_focus (GNOME_CANVAS_ITEM (iti));
	iti_start_editing (iti);
}

/**
 * nautilus_icon_text_item_stop_editing:
 * @iti: An icon text item.
 * @accept: Whether to accept the current text or to discard it.
 *
 * Terminates the editing state of an icon text item.  The @accept argument
 * controls whether the item's current text should be accepted or discarded.  If
 * it is discarded, then the icon's original text will be restored.
 **/
void
nautilus_icon_text_item_stop_editing (NautilusIconTextItem *iti,
				      gboolean accept)
{		
	ItiPrivate *priv;
	
	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));
	
	priv = iti->priv;

	if (!iti->editing) {
		return;
	}

	if (accept) {
		iti_edition_accept (iti);
	} else {
		iti_stop_editing (iti);
	}
}


/**
 * nautilus_icon_text_item_get_type:
 * @void:
 *
 * Registers the &NautilusIconTextItem class if necessary, and returns the type ID
 * associated to it.
 *
 * Return value: the type ID of the &NautilusIconTextItem class.
 **/
GtkType
nautilus_icon_text_item_get_type (void)
{
	static GtkType iti_type = 0;

	if (!iti_type) {
		static const GtkTypeInfo iti_info = {
			"NautilusIconTextItem",
			sizeof (NautilusIconTextItem),
			sizeof (NautilusIconTextItemClass),
			(GtkClassInitFunc) iti_class_init,
			(GtkObjectInitFunc) iti_init,
			NULL, /* reserved_1 */
			NULL, /* reserved_2 */
			(GtkClassInitFunc) NULL
		};

		iti_type = gtk_type_unique (gnome_canvas_item_get_type (), &iti_info);
	}

	return iti_type;
}

/**
 * nautilus_icon_text_item_get_renaming_editable
 * @GtkEditable *
 * Return the editable widget doing the renaming
 **/
GtkEditable *
nautilus_icon_text_item_get_renaming_editable (NautilusIconTextItem *item)
{
	ItiPrivate *priv;

	priv = item->priv;
	return GTK_EDITABLE (priv->entry);
}
					      


/* register_undo
 * 
 * Get text at start of edit operation and store in undo data as 
 * string with a key of "undo_text".
 */
static void
register_rename_undo (NautilusIconTextItem *item)
{
	ItiPrivate *priv;

	priv = item->priv;
	nautilus_undo_register
		(GTK_OBJECT (item),
		 restore_from_undo_snapshot_callback,
		 g_strdup (gtk_entry_get_text (GTK_ENTRY (priv->entry))),
		 (GDestroyNotify) g_free,
		 _("Rename"),
		 _("Undo Typing"),
		 _("Restore the old name"),
		 _("Redo Typing"),
		 _("Restore the changed name"));
}


/* restore_from_undo_snapshot_callback
 * 
 * Restore edited text to data stored in undoable.  Data is stored as 
 * a string with a key of "undo_text".
 */
static void
restore_from_undo_snapshot_callback (GtkObject *target, gpointer callback_data)
{
	NautilusIconTextItem *item;
	ItiPrivate *priv;

	item = NAUTILUS_ICON_TEXT_ITEM (target);
	priv = item->priv;
	
	/* Register a new undo transaction for redo. */
	register_rename_undo (item);
		
	/* Restore the text. */
	nautilus_icon_text_item_set_text (item, callback_data);

	/* Reset the registered flag so we get a new item for future editing. */
	priv->undo_registered = FALSE;
}
