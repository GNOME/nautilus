/* nautilus-icon-text-item:  an editable text block with word wrapping for the
 * GNOME canvas.
 *
 * Copyright (C) 1998, 1999 The Free Software Foundation
 *
 * Authors: Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena <federico@gimp.org>
 *
 * FIXME bugzilla.eazel.com 685: Provide a ref-count fontname caching like thing.
 */

#include <config.h>
#include "nautilus-icon-text-item.h"
#include "nautilus-entry.h"

#include <math.h>
#include <stdio.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkwindow.h>

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

/* Private part of the NautilusIconTextItem structure */
typedef struct {
	/* Font */
	GdkFont *font;

	/* Create an offscreen window and place an entry inside it */
	NautilusEntry *entry;
	GtkWidget *entry_top;

	/* Whether the user pressed the mouse while the item was unselected */
	guint unselected_click : 1;

	/* Whether we need to update the position */
	guint need_pos_update : 1;

	/* Whether we need to update the font */
	guint need_font_update : 1;

	/* Whether we need to update the text */
	guint need_text_update : 1;

	/* Whether we need to update because the editing/selected state changed */
	guint need_state_update : 1;

	/* Store min width and height.  These are used when the text entry is empty. */
	guint min_width;
	guint min_height;
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

static GdkFont *default_font;


/* Stops the editing state of an icon text item */
static void
iti_stop_editing (Iti *iti)
{
	ItiPrivate *priv;

	priv = iti->priv;

	iti->editing = FALSE;

	gtk_widget_destroy (priv->entry_top);
	priv->entry = NULL;
	priv->entry_top = NULL;

	priv->need_state_update = TRUE;
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
	if (iti->editing)
		text = gtk_entry_get_text (GTK_ENTRY(priv->entry));
	else
		text = iti->text;

	iti->ti = gnome_icon_layout_text (priv->font,
					  text,
					  DEFAULT_SEPARATORS,
					  iti->width - 2 * MARGIN_X,
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

	priv->need_text_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
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

	if (iti->editing)
		return;

	/* Trick: The actual edition of the entry takes place in a NautilusEntry
	 * which is placed offscreen.  That way we get all of the advantages
	 * from NautilusEntry without duplicating code.  Yes, this is a hack.
	 */
	priv->entry = (NautilusEntry *) nautilus_entry_new ();
	gtk_entry_set_text (GTK_ENTRY(priv->entry), iti->text);
	gtk_signal_connect (GTK_OBJECT (priv->entry), "activate",
			    GTK_SIGNAL_FUNC (iti_entry_activate), iti);

	priv->entry_top = gtk_window_new (GTK_WINDOW_POPUP);
	gtk_container_add (GTK_CONTAINER (priv->entry_top), GTK_WIDGET (priv->entry));
	gtk_widget_set_uposition (priv->entry_top, 20000, 20000);
	gtk_widget_show_all (priv->entry_top);

	gtk_editable_select_region (GTK_EDITABLE (priv->entry), 0, -1);

	iti->editing = TRUE;

	priv->need_state_update = TRUE;
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

	/* FIXME bugzilla.eazel.com 686: stop selection and editing */

	/* Queue redraw of bounding box */

	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

	/* Free everything */

	if (iti->font)
		gdk_font_unref (iti->font);

	if (iti->text && iti->is_text_allocated)
		g_free (iti->text);

	if (iti->ti)
		gnome_icon_text_info_free (iti->ti);

	if (priv->font)
		gdk_font_unref (priv->font);

	if (priv->entry_top)
		gtk_widget_destroy (priv->entry_top);

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

/* Loads the default font for icon text items if necessary */
static GdkFont *
get_default_font (void)
{
	if (!default_font) {
		/* FIXME bugzilla.eazel.com 687: this is never unref-ed */
		default_font = gdk_fontset_load (DEFAULT_FONT_NAME);
		g_assert (default_font != NULL);
	}

	return gdk_font_ref (default_font);
}

/* Recomputes the bounding box of an icon text item */
static void
recompute_bounding_box (Iti *iti)
{
	GnomeCanvasItem *item;
	double affine[6];
	ArtPoint p, q;
	int x1, y1, x2, y2;
	int width, height;
	ItiPrivate *priv;

	item = GNOME_CANVAS_ITEM (iti);

	priv = iti->priv;

	/* Compute width, height, position */
	width = iti->ti->width + 2 * MARGIN_X;
	height = iti->ti->height + 2 * MARGIN_Y;

	/* Verify we are not smaller than default settings */
	if (width < priv->min_width)
		width = priv->min_width;
	if (height < priv->min_height)
		height = priv->min_height;

	x1 = iti->x + (iti->width - width) / 2;
	y1 = iti->y;
	x2 = x1 + width;
	y2 = y1 + height;

	/* Translate to world coordinates */
	gnome_canvas_item_i2w_affine (item, affine);

	p.x = x1;
	p.y = y1;
	art_affine_point (&q, &p, affine);
	item->x1 = q.x;
	item->y1 = q.y;

	p.x = x2;
	p.y = y2;
	art_affine_point (&q, &p, affine);
	item->x2 = q.x;
	item->y2 = q.y;
}

/* Update method for the icon text item */
static void
iti_update (GnomeCanvasItem *item, double *affine, ArtSVP *clip_path, int flags)
{
	Iti *iti;
	ItiPrivate *priv;

	iti = ITI (item);
	priv = iti->priv;

	if (parent_class->update)
		(* parent_class->update) (item, affine, clip_path, flags);

	/* If necessary, queue a redraw of the old bounding box */

	if ((flags & GNOME_CANVAS_UPDATE_VISIBILITY)
	    || (flags & GNOME_CANVAS_UPDATE_AFFINE)
	    || priv->need_pos_update
	    || priv->need_font_update
	    || priv->need_text_update)
		gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

	/* Compute new bounds */

	if (priv->need_pos_update
	    || priv->need_font_update
	    || priv->need_text_update)
		recompute_bounding_box (iti);

	/* Queue redraw */

	gnome_canvas_request_redraw (item->canvas, item->x1, item->y1, item->x2, item->y2);

	priv->need_pos_update = FALSE;
	priv->need_font_update = FALSE;
	priv->need_text_update = FALSE;
	priv->need_state_update = FALSE;
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
	int xpos, len;
	int cursor, offset, i;

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
		sel_end = GTK_EDITABLE (priv->entry)->selection_end_pos - len;
		offset = 0;
		cursor = GTK_EDITABLE (priv->entry)->current_pos - len;

		for (i = 0; *text_wc; text_wc++, i++) {
			int size, px;

			size = gdk_text_width_wc (ti->font, text_wc, 1);

			if (i >= sel_start && i < sel_end) {
				fg_gc = sgc;
				bg_gc = bsgc;
			} else {
				fg_gc = gc;
				bg_gc = bgc;
			}

			px = x + xpos + offset;
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

			if (cursor == i)
				gdk_draw_line (drawable,
					       gc,
					       px,
					       y - ti->font->ascent,
					       px,
					       y + ti->font->descent - 1);

			offset += size;
		}
		
		y += ti->baseline_skip;
	}

	/* The i-beam should only be drawn at the end of a line of text if that line is the
	 * only or last line of text in a label.  We subtract one form the x position
	 * so the i-beam is not visually jammed against the edge of the bounding rect. */
	if (cursor == i){
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
iti_draw (GnomeCanvasItem *item, GdkDrawable *drawable, int x, int y, int width, int height)
{
	Iti *iti;
	GtkStyle *style;
	int w, h;
	int xofs, yofs;
	ItiPrivate *priv;

	iti = ITI (item);
	priv = iti->priv;

	if (iti->ti) {
		w = iti->ti->width + 2 * MARGIN_X;
		h = iti->ti->height + 2 * MARGIN_Y;

		/* Make sure we aren't smaller than default settings */
		if (w < priv->min_width)
			w = priv->min_width;
		if (h < priv->min_height)
			h = priv->min_height;			
	} else {
		w = 2 * MARGIN_X;
		h = 2 * MARGIN_Y;
	}
	
	xofs = item->x1 - x;
	yofs = item->y1 - y;

	style = GTK_WIDGET (item->canvas)->style;

	if (iti->editing) {
		gdk_draw_rectangle (drawable,
				    style->fg_gc[GTK_STATE_NORMAL],
				    FALSE,
				    xofs, yofs,
				    w - 1, h - 1);

		iti_paint_text (iti, drawable, xofs + MARGIN_X, yofs + MARGIN_Y);
	} else {

		if (iti->selected) {
			gdk_draw_rectangle (drawable,
				    style->bg_gc[GTK_STATE_SELECTED],
				    TRUE,
				    xofs, yofs,
				    w, h);
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

/* Point method handler for the icon text item */
static double
iti_point (GnomeCanvasItem *item, double x, double y, int cx, int cy, GnomeCanvasItem **actual_item)
{
	double dx, dy;

	*actual_item = item;

	if (cx < item->x1)
		dx = item->x1 - cx;
	else if (cx > item->x2)
		dx = cx - item->x2;
	else
		dx = 0.0;

	if (cy < item->y1)
		dy = item->y1 - cy;
	else if (cy > item->y2)
		dy = cy - item->y2;
	else
		dy = 0.0;

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

	priv->need_state_update = TRUE;
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

	priv->need_state_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
	gtk_signal_emit (GTK_OBJECT (iti), iti_signals[SELECTION_STOPPED]);
}

/* Handles selection range changes on the icon text item */
static void
iti_selection_motion (Iti *iti, int idx)
{
	ItiPrivate *priv;
	GtkEditable *e;

	priv = iti->priv;
	e = GTK_EDITABLE (priv->entry);

	if (idx < e->current_pos) {
		e->selection_start_pos = idx;
		e->selection_end_pos   = e->current_pos;
	} else {
		e->selection_start_pos = e->current_pos;
		e->selection_end_pos  = idx;
	}

	priv->need_state_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}

/* Event handler for icon text items */
static gint
iti_event (GnomeCanvasItem *item, GdkEvent *event)
{
	Iti *iti;
	ItiPrivate *priv;
	int idx;
	double x, y;
	int cx, cy;

	iti = ITI (item);
	priv = iti->priv;

	switch (event->type) {
	case GDK_KEY_PRESS:
		if (!iti->editing)
			break;
		
		switch(event->key.keyval) {
		
		/* Pass these events back to parent */		
		case GDK_Escape:
		case GDK_Return:
		case GDK_KP_Enter:
			return FALSE;
									
		default:			
			/* Check for control key operations */
			if (event->key.state & GDK_CONTROL_MASK) {
				return FALSE;
			}
			break;
		}

		/* Handle any events that reach us */
		gtk_widget_event (GTK_WIDGET (priv->entry), event);

		layout_text (iti);
		priv->need_text_update = TRUE;
		gnome_canvas_item_request_update (item);
		return TRUE;

	case GDK_BUTTON_PRESS:
		if (!iti->editing)
			break;

		if (iti->editing && event->button.button == 1) {
			gnome_canvas_w2c(item->canvas, event->button.x, event->button.y, &cx, &cy);			
			x = cx - (item->x1 + MARGIN_X);
			y = cy - (item->y1 + MARGIN_Y);
			idx = iti_idx_from_x_y (iti, x, y);

			iti_start_selecting (iti, idx, event->button.time);
		}
		return TRUE;

	case GDK_MOTION_NOTIFY:
		if (!iti->selecting)
			break;

		gnome_canvas_w2c(item->canvas, event->button.x, event->button.y, &cx, &cy);			
		x = cx - (item->x1 + MARGIN_X);
		y = cy - (item->y1 + MARGIN_Y);
		idx = iti_idx_from_x_y (iti, x, y);
		iti_selection_motion (iti, idx);
		return TRUE;

	case GDK_BUTTON_RELEASE:
		if (iti->selecting && event->button.button == 1)
			iti_stop_selecting (iti, event->button.time);
		else
			break;

		return TRUE;

	case GDK_FOCUS_CHANGE:
		if (iti->editing && event->focus_change.in == FALSE)
			iti_edition_accept (iti);

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
	int width, height;

	iti = ITI (item);

	if (iti->ti) {
		width = iti->ti->width + 2 * MARGIN_X;
		height = iti->ti->height + 2 * MARGIN_Y;
	} else {
		width = 2 * MARGIN_X;
		height = 2 * MARGIN_Y;
	}

	*x1 = iti->x + (iti->width - width) / 2;
	*y1 = iti->y;
	*x2 = *x1 + width;
	*y2 = *y1 + height;
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

	iti_signals [TEXT_CHANGED] =
		gtk_signal_new (
			"text_changed",
			GTK_RUN_LAST,
			object_class->type,
			GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, text_changed),
			gtk_marshal_BOOL__NONE,
			GTK_TYPE_BOOL, 0);

	iti_signals [TEXT_EDITED] =
		gtk_signal_new (
			"text_edited",
			GTK_RUN_LAST,
			object_class->type,
			GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, text_edited),
			gtk_marshal_NONE__NONE,
			GTK_TYPE_NONE, 0);

	iti_signals [HEIGHT_CHANGED] =
		gtk_signal_new (
			"height_changed",
			GTK_RUN_LAST,
			object_class->type,
			GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, height_changed),
			gtk_marshal_NONE__NONE,
			GTK_TYPE_NONE, 0);

	iti_signals [WIDTH_CHANGED] =
		gtk_signal_new (
			"width_changed",
			GTK_RUN_LAST,
			object_class->type,
			GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, width_changed),
			gtk_marshal_NONE__NONE,
			GTK_TYPE_NONE, 0);

	iti_signals[EDITING_STARTED] =
		gtk_signal_new (
			"editing_started",
			GTK_RUN_LAST,
			object_class->type,
			GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, editing_started),
			gtk_marshal_NONE__NONE,
			GTK_TYPE_NONE, 0);

	iti_signals[EDITING_STOPPED] =
		gtk_signal_new (
			"editing_stopped",
			GTK_RUN_LAST,
			object_class->type,
			GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, editing_stopped),
			gtk_marshal_NONE__NONE,
			GTK_TYPE_NONE, 0);

	iti_signals[SELECTION_STARTED] =
		gtk_signal_new (
			"selection_started",
			GTK_RUN_FIRST,
			object_class->type,
			GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, selection_started),
			gtk_marshal_NONE__NONE,
			GTK_TYPE_NONE, 0);

	iti_signals[SELECTION_STOPPED] =
		gtk_signal_new (
			"selection_stopped",
			GTK_RUN_FIRST,
			object_class->type,
			GTK_SIGNAL_OFFSET (NautilusIconTextItemClass, selection_stopped),
			gtk_marshal_NONE__NONE,
			GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, iti_signals, LAST_SIGNAL);

	object_class->destroy = iti_destroy;
	object_class->set_arg = iti_set_arg;

	item_class->update = iti_update;
	item_class->draw = iti_draw;
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
 * @x: X position in which to place the item.
 * @y: Y position in which to place the item.
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
nautilus_icon_text_item_configure (NautilusIconTextItem *iti, int x, int y,
				int width, GdkFont *font,
				const char *text, gboolean is_static)
{
	ItiPrivate *priv;
	GnomeIconTextInfo *min_text_info;

	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));
	g_return_if_fail (width > 2 * MARGIN_X);
	g_return_if_fail (text != NULL);

	priv = iti->priv;

	iti->x = x;
	iti->y = y;
	iti->width = width;

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

	/* FIXME bugzilla.eazel.com 684: 
	 * We update the font and layout here instead of in the
	 * ::update() method because the stupid icon list makes use of iti->ti
	 * and expects it to be valid at all times.  It should request the
	 * item's bounds instead.
	 */

	if (priv->font)
		gdk_font_unref (priv->font);

	priv->font = NULL;
	if (font)
		priv->font = gdk_font_ref (iti->font);
	if (!priv->font)
		priv->font = get_default_font ();

	layout_text (iti);

	/* Calculate and store min and max dimensions */	
	min_text_info = gnome_icon_layout_text (priv->font,
					  " ",
					  DEFAULT_SEPARATORS,
					  iti->width - 2 * MARGIN_X,
					  TRUE);
	priv->min_width = min_text_info->width + 2 * MARGIN_X;
	priv->min_height = min_text_info->height + 2 * MARGIN_Y;
	gnome_icon_text_info_free(min_text_info);

	/* Request update */
	priv->need_pos_update = TRUE;
	priv->need_font_update = TRUE;
	priv->need_text_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}

/**
 * nautilus_icon_text_item_setxy:
 * @iti:  An icon text item.
 * @x: X position.
 * @y: Y position.
 *
 * Sets the coordinates at which the icon text item should be placed.
 *
 * See also: nautilus_icon_text_item_configure().
 */
void
nautilus_icon_text_item_setxy (NautilusIconTextItem *iti, int x, int y)
{
	ItiPrivate *priv;

	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));

	priv = iti->priv;

	iti->x = x;
	iti->y = y;

	priv->need_pos_update = TRUE;
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

	if (!iti->selected && iti->editing)
		iti_edition_accept (iti);

	priv->need_state_update = TRUE;
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

	priv->need_text_update = TRUE;
	gnome_canvas_item_request_update (GNOME_CANVAS_ITEM (iti));
}

/**
 * nautilus_icon_text_item_get_text:
 * @iti: An icon text item.
 *
 * Returns the current text string in an icon text item.  The client should not
 * free this string, as it is internal to the icon text item.
 */
char *
nautilus_icon_text_item_get_text (NautilusIconTextItem *iti)
{
	ItiPrivate *priv;

	g_return_val_if_fail (iti != NULL, NULL);
	g_return_val_if_fail (IS_ITI (iti), NULL);

	priv = iti->priv;

	if (iti->editing)
		return gtk_entry_get_text (GTK_ENTRY(priv->entry));
	else
		return iti->text;
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
	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));

	if (iti->editing)
		return;

	iti->selected = TRUE; /* Ensure that we are selected */
	gnome_canvas_item_grab_focus (GNOME_CANVAS_ITEM (iti));
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
	g_return_if_fail (iti != NULL);
	g_return_if_fail (IS_ITI (iti));

	if (!iti->editing)
		return;

	if (accept)
		iti_edition_accept (iti);
	else
		iti_stop_editing (iti);
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
 * nautilus_icon_text_item_get_margins:
 * @void:
 *
 * Return the x and y margins of th etext item
 **/
void
nautilus_icon_text_item_get_margins (int *x, int *y)
{
	*x = MARGIN_X;
	*y = MARGIN_Y;
}

