/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* File list widget for the Midnight Commander
 *
 * Copyright (C) 1999, 2000 The Free Software Foundation
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: Federico Mena <federico@nuclecu.unam.mx>
 * Modified by Ettore Perazzoli <ettore@gnu.org>
 */

/* FIXME this is a kludge to re-use broken CList.  Instead, I'd like to have a
   native List widget that uses a simple API similiar to the GnomeIconContainer
   one.  */

#include <config.h>
#include "gtkflist.h"

#include <gtk/gtkdnd.h>
#include "nautilus-glib-extensions.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-background.h"
#include "nautilus-list-column-title.h"

struct _GtkFListDetails
{
	/* Preferences */
	gboolean single_click_mode;

	/* The anchor row for range selections */
	int anchor_row;

	/* Mouse information saved on button press */
	int dnd_press_button;
	int dnd_press_x, dnd_press_y;
	int button_down_row;
	guint32 button_down_time;

	/* Delayed selection information */
	int dnd_select_pending;
	guint dnd_select_pending_state;

	GtkWidget *title;
};

/* maximum amount of milliseconds the mouse button is allowed to stay down and still be considered a click */
#define MAX_CLICK_TIME 1500

enum {
	CONTEXT_CLICK_SELECTION,
	CONTEXT_CLICK_BACKGROUND,
	ACTIVATE,
	START_DRAG,
	SELECTION_CHANGED,
	LAST_SIGNAL
};

enum {
	TARGET_COLOR
};

static GtkTargetEntry gtk_flist_dnd_target_table[] = {
	{ "application/x-color", 0, TARGET_COLOR }
};

static void activate_row (GtkFList *flist, gint row);

static void gtk_flist_initialize_class (GtkFListClass *class);
static void gtk_flist_initialize (GtkFList *flist);

static gint gtk_flist_button_press (GtkWidget *widget, GdkEventButton *event);
static gint gtk_flist_button_release (GtkWidget *widget, GdkEventButton *event);
static gint gtk_flist_motion (GtkWidget *widget, GdkEventMotion *event);
static gint gtk_flist_key (GtkWidget *widget, GdkEventKey *event);
static void gtk_flist_drag_begin (GtkWidget *widget, GdkDragContext *context);
static void gtk_flist_drag_end (GtkWidget *widget, GdkDragContext *context);
static void gtk_flist_drag_data_get (GtkWidget *widget, GdkDragContext *context,
				     GtkSelectionData *data, guint info, guint time);
static void gtk_flist_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time);
static gboolean gtk_flist_drag_motion (GtkWidget *widget, GdkDragContext *context,
				       gint x, gint y, guint time);
static gboolean gtk_flist_drag_drop (GtkWidget *widget, GdkDragContext *context,
				     gint x, gint y, guint time);
static void gtk_flist_drag_data_received (GtkWidget *widget, GdkDragContext *context,
					  gint x, gint y, GtkSelectionData *data,
					  guint info, guint time);
static void select_or_unselect_row_cb (GtkCList *clist, gint row, gint column, 
				       GdkEvent *event);

static void gtk_flist_clear (GtkCList *clist);
static void draw_row (GtkCList *flist, GdkRectangle *area, gint row, GtkCListRow *clist_row);

static void gtk_flist_realize (GtkWidget *widget);
static void gtk_flist_size_request (GtkWidget *widget, GtkRequisition *requisition);

static void gtk_flist_resize_column (GtkCList *widget, int column, int width);

static void gtk_flist_column_resize_track_start (GtkWidget *widget, int column);
static void gtk_flist_column_resize_track (GtkWidget *widget, int column);
static void gtk_flist_column_resize_track_end (GtkWidget *widget, int column);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (GtkFList, gtk_flist, GTK_TYPE_CLIST)

static guint flist_signals[LAST_SIGNAL];

/* Standard class initialization function */
static void
gtk_flist_initialize_class (GtkFListClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkCListClass *clist_class;
	GtkFListClass *flist_class;

	object_class = (GtkObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;
	clist_class = (GtkCListClass *) klass;
	flist_class = (GtkFListClass *) klass;

	flist_signals[CONTEXT_CLICK_SELECTION] =
		gtk_signal_new ("context_click_selection",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkFListClass, context_click_selection),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	flist_signals[CONTEXT_CLICK_BACKGROUND] =
		gtk_signal_new ("context_click_background",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkFListClass, context_click_background),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	flist_signals[ACTIVATE] =
		gtk_signal_new ("activate",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkFListClass, activate),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	flist_signals[START_DRAG] =
		gtk_signal_new ("start_drag",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkFListClass, start_drag),
				gtk_marshal_NONE__INT_POINTER,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_INT,
				GTK_TYPE_GDK_EVENT);
	flist_signals[SELECTION_CHANGED] =
		gtk_signal_new ("selection_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkFListClass, selection_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, flist_signals, LAST_SIGNAL);

	flist_class->column_resize_track_start = gtk_flist_column_resize_track_start;
	flist_class->column_resize_track = gtk_flist_column_resize_track;
	flist_class->column_resize_track_end = gtk_flist_column_resize_track_end;

	clist_class->clear = gtk_flist_clear;
	clist_class->draw_row = draw_row;
  	clist_class->resize_column = gtk_flist_resize_column;

	widget_class->button_press_event = gtk_flist_button_press;
	widget_class->button_release_event = gtk_flist_button_release;
	widget_class->motion_notify_event = gtk_flist_motion;
	widget_class->key_press_event = gtk_flist_key;
	widget_class->key_release_event = gtk_flist_key;
	widget_class->drag_begin = gtk_flist_drag_begin;
	widget_class->drag_end = gtk_flist_drag_end;
	widget_class->drag_data_get = gtk_flist_drag_data_get;
	widget_class->drag_leave = gtk_flist_drag_leave;
	widget_class->drag_motion = gtk_flist_drag_motion;
	widget_class->drag_drop = gtk_flist_drag_drop;
	widget_class->drag_data_received = gtk_flist_drag_data_received;
	widget_class->realize = gtk_flist_realize;
	widget_class->size_request = gtk_flist_size_request;
}

/* Standard object initialization function */
static void
gtk_flist_initialize (GtkFList *flist)
{	
	flist->details = g_new0 (GtkFListDetails, 1);
	flist->details->anchor_row = -1;
	
	/* This should be read from preferences */
	flist->details->single_click_mode = TRUE;

	/* GtkCList does not specify pointer motion by default */
	gtk_widget_add_events (GTK_WIDGET (flist), GDK_POINTER_MOTION_MASK);

	/* Get ready to accept some dragged stuff. */
	gtk_drag_dest_set (GTK_WIDGET (flist),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
			   gtk_flist_dnd_target_table,
			   NAUTILUS_N_ELEMENTS (gtk_flist_dnd_target_table),
			   GDK_ACTION_COPY);

	/* Emit "selection changed" signal when parent class changes selection */
	gtk_signal_connect (GTK_OBJECT (flist),
			    "select_row",
			    select_or_unselect_row_cb,
			    flist);
	gtk_signal_connect (GTK_OBJECT (flist),
			    "unselect_row",
			    select_or_unselect_row_cb,
			    flist);

	flist->details->title = GTK_WIDGET (nautilus_list_column_title_new());
}


static void
select_or_unselect_row_cb (GtkCList *clist, gint row, gint column, GdkEvent *event)
{
	/* This is the one bottleneck for all selection changes */
	gtk_signal_emit (GTK_OBJECT (clist), flist_signals[SELECTION_CHANGED]);
}

static void
activate_row (GtkFList *flist, gint row)
{
	GtkCListRow *elem;

	elem = g_list_nth (GTK_CLIST (flist)->row_list,
			   row)->data;
	gtk_signal_emit (GTK_OBJECT (flist),
			 flist_signals[ACTIVATE],
			 elem->data);
}

static gboolean
row_selected (GtkFList *flist, gint row)
{
	GtkCListRow *elem;

	elem = g_list_nth (GTK_CLIST (flist)->row_list, row)->data;

	return elem->state == GTK_STATE_SELECTED;
}

/* Selects the rows between the anchor to the specified row, inclusive.  */
static void
select_range (GtkFList *flist, int row)
{
	int min, max;
	int i;

	if (flist->details->anchor_row == -1)
		flist->details->anchor_row = row;

	if (row < flist->details->anchor_row) {
		min = row;
		max = flist->details->anchor_row;
	} else {
		min = flist->details->anchor_row;
		max = row;
	}

	for (i = min; i <= max; i++)
		gtk_clist_select_row (GTK_CLIST (flist), i, 0);
}

/* Handles row selection according to the specified modifier state */
static void
select_row (GtkFList *flist, int row, guint state)
{
	int range, additive;

	range = (state & GDK_SHIFT_MASK) != 0;
	additive = (state & GDK_CONTROL_MASK) != 0;

	if (!additive)
		gtk_clist_unselect_all (GTK_CLIST (flist));

	if (!range) {
		if (additive) {
			if (row_selected (flist, row))
				gtk_clist_unselect_row
					(GTK_CLIST (flist), row, 0);
			else
				gtk_clist_select_row
					(GTK_CLIST (flist), row, 0);
		} else {
			gtk_clist_select_row (GTK_CLIST (flist), row, 0);
		}
		flist->details->anchor_row = row;
	} else
		select_range (flist, row);
}

/* Our handler for button_press events.  We override all of GtkCList's broken
 * behavior.
 */
static gint
gtk_flist_button_press (GtkWidget *widget, GdkEventButton *event)
{
	GtkFList *flist;
	GtkCList *clist;
	int on_row;
	gint row, col;
	int retval;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_FLIST (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	flist = GTK_FLIST (widget);
	clist = GTK_CLIST (widget);
	retval = FALSE;

	if (event->window != clist->clist_window)
		return NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, button_press_event, (widget, event));

	on_row = gtk_clist_get_selection_info (clist, event->x, event->y, &row, &col);
	flist->details->button_down_time = event->time;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button == 1 || event->button == 2) {
			if (on_row) {

				/* Save the clicked row for DnD and single-click activate */
				
				flist->details->button_down_row = row;

				/* Save the mouse info for DnD */

				flist->details->dnd_press_button = event->button;
				flist->details->dnd_press_x = event->x;
				flist->details->dnd_press_y = event->y;
	
				/* Handle selection */

				if ((row_selected (flist, row)
				     && !(event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)))
				    || ((event->state & GDK_CONTROL_MASK)
					&& !(event->state & GDK_SHIFT_MASK))) {
					flist->details->dnd_select_pending = TRUE;
					flist->details->dnd_select_pending_state = event->state;
				}

				select_row (flist, row, event->state);
			} else {
				gtk_clist_unselect_all (clist);
			}

			retval = TRUE;
		} else if (event->button == 3) {
			if (on_row) {
				/* Context menu applies to all selected items. First use click
				 * to modify selection as appropriate, then emit signal that
				 * will bring up menu.
				 */
				select_row (flist, row, event->state);
				gtk_signal_emit (GTK_OBJECT (flist),
						 flist_signals[CONTEXT_CLICK_SELECTION]);
			} else
				gtk_signal_emit (GTK_OBJECT (flist),
						 flist_signals[CONTEXT_CLICK_BACKGROUND]);

			retval = TRUE;
		}

		break;

	case GDK_2BUTTON_PRESS:
		if (event->button == 1) {
			flist->details->dnd_select_pending = FALSE;
			flist->details->dnd_select_pending_state = 0;

			if (on_row) {
				/* Activate on double-click even if single_click_mode
				 * is set, so second click doesn't get passed to child
				 * directory.
				 */
				activate_row (flist, row);
			}

			retval = TRUE;
			break;
		}

	default:
		break;
	}

	return retval;
}

/* Our handler for button_release events.  We override all of GtkCList's broken
 * behavior.
 */
static gint
gtk_flist_button_release (GtkWidget *widget, GdkEventButton *event)
{
	GtkFList *flist;
	GtkCList *clist;
	int on_row;
	gint row, col;
	int retval;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_FLIST (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	flist = GTK_FLIST (widget);
	clist = GTK_CLIST (widget);
	retval = FALSE;

	if (event->window != clist->clist_window)
		return NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, button_release_event, (widget, event));

	on_row = gtk_clist_get_selection_info (clist, event->x, event->y, &row, &col);

	if (!(event->button == 1 || event->button == 2))
		return FALSE;

	flist->details->dnd_press_button = 0;
	flist->details->dnd_press_x = 0;
	flist->details->dnd_press_y = 0;

	if (on_row) {
		/* Clean up after abortive drag-and-drop attempt (since user can't
		 * reorder list view items, releasing mouse in list view cancels
		 * drag-and-drop possibility). 
		 */
		if (flist->details->dnd_select_pending) {
			flist->details->dnd_select_pending = FALSE;
			flist->details->dnd_select_pending_state = 0;
		}

		/* Activate on single click if not extending selection, mouse hasn't moved to
		 * a different row, and not too much time has passed.
		 */
		if (flist->details->single_click_mode && 
		    !(event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)))
		{
			gint elapsed_time = event->time - flist->details->button_down_time;

			if (elapsed_time < MAX_CLICK_TIME && flist->details->button_down_row == row)
			{
				activate_row (flist, row);
			}
		}		
	
		retval = TRUE;
	}

	return retval;
}

/* stolen from gtkclist.c for now */

/* minimum allowed width of a column */
#define COLUMN_MIN_WIDTH 5

/* this defines the base grid spacing */
#define CELL_SPACING 1

/* added the horizontal space at the beginning and end of a row */
#define COLUMN_INSET 3

/* the width of the column resize windows */
#define DRAG_WIDTH  6

/* gives the left pixel of the given column in context of
 * the clist's hoffset */
#define COLUMN_LEFT_XPIXEL(clist, colnum)  ((clist)->column[(colnum)].area.x + \
					    (clist)->hoffset)

/* gives the top pixel of the given row in context of
 * the clist's voffset */
#define ROW_TOP_YPIXEL(clist, row) (((clist)->row_height * (row)) + \
				    (((row) + 1) * CELL_SPACING) + \
				    (clist)->voffset)

/* returns the row index from a y pixel location in the 
 * context of the clist's voffset */
#define ROW_FROM_YPIXEL(clist, y)  (((y) - (clist)->voffset) / \
				    ((clist)->row_height + CELL_SPACING))

/* returns the GList item for the nth row */
#define	ROW_ELEMENT(clist, row)	(((row) == (clist)->rows - 1) ? \
				 (clist)->row_list_end : \
				 g_list_nth ((clist)->row_list, (row)))

/* returns the total height of the list */
#define LIST_HEIGHT(clist)         (((clist)->row_height * ((clist)->rows)) + \
				    (CELL_SPACING * ((clist)->rows + 1)))

static void
gtk_flist_realize (GtkWidget *widget)
{
	GtkFList *flist;
	GtkCList *clist;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_FLIST (widget));

	flist = GTK_FLIST (widget);
	clist = GTK_CLIST (widget);

	clist->column[0].button = flist->details->title;

	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, realize, (widget));

	if (clist->title_window) {
		gtk_widget_set_parent_window (flist->details->title, clist->title_window);
	}
	gtk_widget_set_parent (flist->details->title, GTK_WIDGET (clist));
	gtk_widget_show (flist->details->title);
	
	GTK_CLIST_SET_FLAG (clist, CLIST_SHOW_TITLES);
}

/* this is here just temporarily */
static gint
list_requisition_width (GtkCList *clist) 
{
	gint width = CELL_SPACING;
	gint i;

	for (i = clist->columns - 1; i >= 0; i--) {
		if (!clist->column[i].visible)
			continue;

		if (clist->column[i].width_set)
			width += clist->column[i].width + CELL_SPACING + (2 * COLUMN_INSET);
		else if (GTK_CLIST_SHOW_TITLES(clist) && clist->column[i].button)
			width += clist->column[i].button->requisition.width;
	}

	return width;
}


static void
gtk_flist_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	/* stolen from gtk_clist 
	 * make sure the proper title ammount is allocated for the column
	 * title view --  this would not otherwise be done because 
	 * GtkFList depends the buttons being there when doing a size calculation
	 */
	GtkFList *flist;
	GtkCList *clist;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_FLIST (widget));
	g_return_if_fail (requisition != NULL);

	clist = GTK_CLIST (widget);
	flist = GTK_FLIST (widget);

	requisition->width = 0;
	requisition->height = 0;

	/* compute the size of the column title (title) area */
	clist->column_title_area.height = 0;
	if (GTK_CLIST_SHOW_TITLES(clist) && flist->details->title) {
		GtkRequisition child_requisition;
		
		gtk_widget_size_request (flist->details->title,
					 &child_requisition);

		child_requisition.height = 20;
			/* for now */

		clist->column_title_area.height =
			MAX (clist->column_title_area.height,
			     child_requisition.height);
	}

	requisition->width += (widget->style->klass->xthickness +
			       GTK_CONTAINER (widget)->border_width) * 2;
	requisition->height += (clist->column_title_area.height +
				(widget->style->klass->ythickness +
				GTK_CONTAINER (widget)->border_width) * 2);


	requisition->width += list_requisition_width (clist);
	requisition->height += LIST_HEIGHT (clist);
}

static gint
new_column_width (GtkCList *clist, gint column,  gint *x)
{
	gint xthickness = GTK_WIDGET (clist)->style->klass->xthickness;
	gint width;
	gint cx;
	gint dx;
	gint last_column;

	/* first translate the x position from widget->window
	 * to clist->clist_window */
	cx = *x - xthickness;

	for (last_column = clist->columns - 1;
		last_column >= 0 && !clist->column[last_column].visible; last_column--);

	/* calculate new column width making sure it doesn't end up
	 * less than the minimum width */
	dx = (COLUMN_LEFT_XPIXEL (clist, column) + COLUMN_INSET +
		(column < last_column) * CELL_SPACING);
	width = cx - dx;

	if (width < MAX (COLUMN_MIN_WIDTH, clist->column[column].min_width)) {
		width = MAX (COLUMN_MIN_WIDTH, clist->column[column].min_width);
		cx = dx + width;
		*x = cx + xthickness;
	} else if (clist->column[column].max_width >= COLUMN_MIN_WIDTH &&
	   width > clist->column[column].max_width) {
		width = clist->column[column].max_width;
		cx = dx + clist->column[column].max_width;
		*x = cx + xthickness;
    	}

	if (cx < 0 || cx > clist->clist_window_width)
		*x = -1;

	return width;
}

static void
size_allocate_columns (GtkCList *clist, gboolean  block_resize)
{
	int xoffset = CELL_SPACING + COLUMN_INSET;
	int last_column;
	int i;

	/* find last visible column and calculate correct column width */
	for (last_column = clist->columns - 1;
	     last_column >= 0 && !clist->column[last_column].visible; last_column--)
		;

	if (last_column < 0)
		return;

	for (i = 0; i <= last_column; i++)  {
		if (!clist->column[i].visible)
			continue;

		clist->column[i].area.x = xoffset;
		if (clist->column[i].width_set) {
			if (!block_resize && GTK_CLIST_SHOW_TITLES(clist) &&
				clist->column[i].auto_resize && clist->column[i].button) {
				gint width;

				width = (clist->column[i].button->requisition.width -
					(CELL_SPACING + (2 * COLUMN_INSET)));

				if (width > clist->column[i].width)
					gtk_clist_set_column_width (clist, i, width);
			}

			clist->column[i].area.width = clist->column[i].width;
			xoffset += clist->column[i].width + CELL_SPACING + (2 * COLUMN_INSET);
		} else if (GTK_CLIST_SHOW_TITLES(clist) && clist->column[i].button) {
			clist->column[i].area.width =
				clist->column[i].button->requisition.width -
				(CELL_SPACING + (2 * COLUMN_INSET));
			xoffset += clist->column[i].button->requisition.width;
		}
	}

	clist->column[last_column].area.width += MAX (0, clist->clist_window_width + COLUMN_INSET - xoffset);
}

static void
size_allocate_title_buttons (GtkCList *clist)
{
	GtkAllocation button_allocation;
	int last_column;
	int last_button = 0;
	int i;

	button_allocation.x = clist->hoffset;
	button_allocation.y = 0;
	button_allocation.width = 0;
	button_allocation.height = clist->column_title_area.height;

	/* find last visible column */
	for (last_column = clist->columns - 1; last_column >= 0; last_column--)
		if (clist->column[last_column].visible)
			break;

	for (i = 0; i < last_column; i++) {
		if (!clist->column[i].visible) {
			last_button = i + 1;
			gdk_window_hide (clist->column[i].window);
			continue;
		}

		button_allocation.width += (clist->column[i].area.width +
				  	    CELL_SPACING + 2 * COLUMN_INSET);

		if (!clist->column[i + 1].button) {
			gdk_window_hide (clist->column[i].window);
			continue;
		}

		gtk_widget_size_allocate (clist->column[last_button].button,
					  &button_allocation); 
		button_allocation.x += button_allocation.width;
		button_allocation.width = 0;

		last_button = i + 1;
	}

	button_allocation.width += (clist->column[last_column].area.width +
				    2 * (CELL_SPACING + COLUMN_INSET));
	gtk_widget_size_allocate (clist->column[last_button].button,
				  &button_allocation);

}

static void
get_cell_style (GtkCList *clist, GtkCListRow *clist_row,
		gint state, gint column, GtkStyle **style,
		GdkGC **fg_gc, GdkGC **bg_gc)
{
	gint fg_state;

	if ((state == GTK_STATE_NORMAL) &&
	    (GTK_WIDGET (clist)->state == GTK_STATE_INSENSITIVE))
		fg_state = GTK_STATE_INSENSITIVE;
	else
		fg_state = state;

	if (clist_row->cell[column].style) {
		if (style)
			*style = clist_row->cell[column].style;
		if (fg_gc)
			*fg_gc = clist_row->cell[column].style->fg_gc[fg_state];
		if (bg_gc) {
			if (state == GTK_STATE_SELECTED)
				*bg_gc = clist_row->cell[column].style->bg_gc[state];
			else
	  			*bg_gc = clist_row->cell[column].style->base_gc[state];
		}
	} else if (clist_row->style) {
		if (style)
			*style = clist_row->style;
		if (fg_gc)
			*fg_gc = clist_row->style->fg_gc[fg_state];
		if (bg_gc) {
			if (state == GTK_STATE_SELECTED)
				*bg_gc = clist_row->style->bg_gc[state];
			else
				*bg_gc = clist_row->style->base_gc[state];
		}
	} else {
		if (style)
			*style = GTK_WIDGET (clist)->style;
		if (fg_gc)
			*fg_gc = GTK_WIDGET (clist)->style->fg_gc[fg_state];
		if (bg_gc) {
			if (state == GTK_STATE_SELECTED)
				*bg_gc = GTK_WIDGET (clist)->style->bg_gc[state];
			else
				*bg_gc = GTK_WIDGET (clist)->style->base_gc[state];
		}

		if (state != GTK_STATE_SELECTED) {
			if (fg_gc && clist_row->fg_set)
				*fg_gc = clist->fg_gc;
			if (bg_gc && clist_row->bg_set)
				*bg_gc = clist->bg_gc;
		}
	}
}

static gint
draw_cell_pixmap (GdkWindow *window, GdkRectangle *clip_rectangle, GdkGC *fg_gc,
		  GdkPixmap *pixmap, GdkBitmap *mask,
		  gint x, gint y, gint width, gint height)
{
	gint xsrc = 0;
	gint ysrc = 0;

	if (mask) {
		gdk_gc_set_clip_mask (fg_gc, mask);
		gdk_gc_set_clip_origin (fg_gc, x, y);
	}

	if (x < clip_rectangle->x) {
		xsrc = clip_rectangle->x - x;
		width -= xsrc;
		x = clip_rectangle->x;
	}
	
	if (x + width > clip_rectangle->x + clip_rectangle->width)
		width = clip_rectangle->x + clip_rectangle->width - x;

	if (y < clip_rectangle->y) {
		ysrc = clip_rectangle->y - y;
		height -= ysrc;
		y = clip_rectangle->y;
	}
	if (y + height > clip_rectangle->y + clip_rectangle->height)
		height = clip_rectangle->y + clip_rectangle->height - y;

	gdk_draw_pixmap (window, fg_gc, pixmap, xsrc, ysrc, x, y, width, height);
	gdk_gc_set_clip_origin (fg_gc, 0, 0);
	if (mask)
		gdk_gc_set_clip_mask (fg_gc, NULL);

	return x + MAX (width, 0);
}

static void
draw_row (GtkCList     *clist,
	  GdkRectangle *area,
	  gint          row,
	  GtkCListRow  *clist_row)
{
  GtkWidget *widget;
  GdkRectangle *rect;
  GdkRectangle row_rectangle;
  GdkRectangle cell_rectangle;
  GdkRectangle clip_rectangle;
  GdkRectangle intersect_rectangle;
  gint last_column;
  gint state;
  gint i;

  g_return_if_fail (clist != NULL);

  /* bail now if we arn't drawable yet */
  if (!GTK_WIDGET_DRAWABLE (clist) || row < 0 || row >= clist->rows)
    return;

  widget = GTK_WIDGET (clist);

  /* if the function is passed the pointer to the row instead of null,
   * it avoids this expensive lookup */
  if (!clist_row)
    clist_row = ROW_ELEMENT (clist, row)->data;

  /* rectangle of the entire row */
  row_rectangle.x = 0;
  row_rectangle.y = ROW_TOP_YPIXEL (clist, row);
  row_rectangle.width = clist->clist_window_width;
  row_rectangle.height = clist->row_height;

  /* rectangle of the cell spacing above the row */
  cell_rectangle.x = 0;
  cell_rectangle.y = row_rectangle.y - CELL_SPACING;
  cell_rectangle.width = row_rectangle.width;
  cell_rectangle.height = CELL_SPACING;

  /* rectangle used to clip drawing operations, its y and height
   * positions only need to be set once, so we set them once here. 
   * the x and width are set withing the drawing loop below once per
   * column */
  clip_rectangle.y = row_rectangle.y;
  clip_rectangle.height = row_rectangle.height;

  if (clist_row->state == GTK_STATE_NORMAL)
    {
      if (clist_row->fg_set)
	gdk_gc_set_foreground (clist->fg_gc, &clist_row->foreground);
      if (clist_row->bg_set)
	gdk_gc_set_foreground (clist->bg_gc, &clist_row->background);
    }

  state = clist_row->state;

  /* draw the cell borders and background */
  if (area)
    {
      rect = &intersect_rectangle;
      if (gdk_rectangle_intersect (area, &cell_rectangle,
				   &intersect_rectangle))
	gdk_draw_rectangle (clist->clist_window,
			    widget->style->base_gc[GTK_STATE_ACTIVE],
			    TRUE,
			    intersect_rectangle.x,
			    intersect_rectangle.y,
			    intersect_rectangle.width,
			    intersect_rectangle.height);

      /* the last row has to clear its bottom cell spacing too */
      if (clist_row == clist->row_list_end->data)
	{
	  cell_rectangle.y += clist->row_height + CELL_SPACING;

	  if (gdk_rectangle_intersect (area, &cell_rectangle,
				       &intersect_rectangle))
	    gdk_draw_rectangle (clist->clist_window,
				widget->style->base_gc[GTK_STATE_ACTIVE],
				TRUE,
				intersect_rectangle.x,
				intersect_rectangle.y,
				intersect_rectangle.width,
				intersect_rectangle.height);
	}

      if (!gdk_rectangle_intersect (area, &row_rectangle,&intersect_rectangle))
	return;

    }
  else
    {
      rect = &clip_rectangle;
      gdk_draw_rectangle (clist->clist_window,
			  widget->style->base_gc[GTK_STATE_ACTIVE],
			  TRUE,
			  cell_rectangle.x,
			  cell_rectangle.y,
			  cell_rectangle.width,
			  cell_rectangle.height);

      /* the last row has to clear its bottom cell spacing too */
      if (clist_row == clist->row_list_end->data)
	{
	  cell_rectangle.y += clist->row_height + CELL_SPACING;

	  gdk_draw_rectangle (clist->clist_window,
			      widget->style->base_gc[GTK_STATE_ACTIVE],
			      TRUE,
			      cell_rectangle.x,
			      cell_rectangle.y,
			      cell_rectangle.width,
			      cell_rectangle.height);     
	}	  
    }
  
  for (last_column = clist->columns - 1;
       last_column >= 0 && !clist->column[last_column].visible; last_column--)
    ;

  /* iterate and draw all the columns (row cells) and draw their contents */
  for (i = 0; i < clist->columns; i++)
    {
      GtkStyle *style;
      GdkGC *fg_gc;
      GdkGC *bg_gc;

      gint width;
      gint height;
      gint pixmap_width;
      gint offset = 0;
      gint row_center_offset;

      if (!clist->column[i].visible)
	continue;

      get_cell_style (clist, clist_row, state, i, &style, &fg_gc, &bg_gc);

      clip_rectangle.x = clist->column[i].area.x + clist->hoffset;
      clip_rectangle.width = clist->column[i].area.width;

      /* calculate clipping region clipping region */
      clip_rectangle.x -= COLUMN_INSET + CELL_SPACING;
      clip_rectangle.width += (2 * COLUMN_INSET + CELL_SPACING +
			       (i == last_column) * CELL_SPACING);
      
      if (area && !gdk_rectangle_intersect (area, &clip_rectangle,
					    &intersect_rectangle))
	continue;

      gdk_draw_rectangle (clist->clist_window, bg_gc, TRUE,
			  rect->x, rect->y, rect->width, rect->height);

      clip_rectangle.x += COLUMN_INSET + CELL_SPACING;
      clip_rectangle.width -= (2 * COLUMN_INSET + CELL_SPACING +
			       (i == last_column) * CELL_SPACING);

      /* calculate real width for column justification */
      pixmap_width = 0;
      offset = 0;
      switch (clist_row->cell[i].type)
	{
	case GTK_CELL_TEXT:
	  width = gdk_string_width (style->font,
				    GTK_CELL_TEXT (clist_row->cell[i])->text);
	  break;
	case GTK_CELL_PIXMAP:
	  gdk_window_get_size (GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap,
			       &pixmap_width, &height);
	  width = pixmap_width;
	  break;
	case GTK_CELL_PIXTEXT:
	  gdk_window_get_size (GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
			       &pixmap_width, &height);
	  width = (pixmap_width +
		   GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing +
		   gdk_string_width (style->font,
				     GTK_CELL_PIXTEXT
				     (clist_row->cell[i])->text));
	  break;
	default:
	  continue;
	  break;
	}

      switch (clist->column[i].justification)
	{
	case GTK_JUSTIFY_LEFT:
	  offset = clip_rectangle.x + clist_row->cell[i].horizontal;
	  break;
	case GTK_JUSTIFY_RIGHT:
	  offset = (clip_rectangle.x + clist_row->cell[i].horizontal +
		    clip_rectangle.width - width);
	  break;
	case GTK_JUSTIFY_CENTER:
	case GTK_JUSTIFY_FILL:
	  offset = (clip_rectangle.x + clist_row->cell[i].horizontal +
		    (clip_rectangle.width / 2) - (width / 2));
	  break;
	};

      /* Draw Text and/or Pixmap */
      switch (clist_row->cell[i].type)
	{
	case GTK_CELL_PIXMAP:
	  draw_cell_pixmap (clist->clist_window, &clip_rectangle, fg_gc,
			    GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap,
			    GTK_CELL_PIXMAP (clist_row->cell[i])->mask,
			    offset,
			    clip_rectangle.y + clist_row->cell[i].vertical +
			    (clip_rectangle.height - height) / 2,
			    pixmap_width, height);
	  break;
	case GTK_CELL_PIXTEXT:
	  offset =
	    draw_cell_pixmap (clist->clist_window, &clip_rectangle, fg_gc,
			      GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
			      GTK_CELL_PIXTEXT (clist_row->cell[i])->mask,
			      offset,
			      clip_rectangle.y + clist_row->cell[i].vertical+
			      (clip_rectangle.height - height) / 2,
			      pixmap_width, height);
	  offset += GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;
	case GTK_CELL_TEXT:
	  if (style != GTK_WIDGET (clist)->style)
	    row_center_offset = (((clist->row_height - style->font->ascent -
				  style->font->descent - 1) / 2) + 1.5 +
				 style->font->ascent);
	  else
	    row_center_offset = clist->row_center_offset;

	  gdk_gc_set_clip_rectangle (fg_gc, &clip_rectangle);
	  gdk_draw_string (clist->clist_window, style->font, fg_gc,
			   offset,
			   row_rectangle.y + row_center_offset + 
			   clist_row->cell[i].vertical,
			   (clist_row->cell[i].type == GTK_CELL_PIXTEXT) ?
			   GTK_CELL_PIXTEXT (clist_row->cell[i])->text :
			   GTK_CELL_TEXT (clist_row->cell[i])->text);
	  gdk_gc_set_clip_rectangle (fg_gc, NULL);
	  break;
	default:
	  break;
	}
    }

  /* draw focus rectangle */
  if (clist->focus_row == row &&
      GTK_WIDGET_CAN_FOCUS (widget) && GTK_WIDGET_HAS_FOCUS(widget))
    {
      if (!area)
	gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE,
			    row_rectangle.x, row_rectangle.y,
			    row_rectangle.width - 1, row_rectangle.height - 1);
      else if (gdk_rectangle_intersect (area, &row_rectangle,
					&intersect_rectangle))
	{
	  gdk_gc_set_clip_rectangle (clist->xor_gc, &intersect_rectangle);
	  gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE,
			      row_rectangle.x, row_rectangle.y,
			      row_rectangle.width - 1,
			      row_rectangle.height - 1);
	  gdk_gc_set_clip_rectangle (clist->xor_gc, NULL);
	}
    }
}

static void
draw_rows (GtkCList *clist, GdkRectangle *area)
{
	GList *list;
	gint i;
	gint first_row;
	gint last_row;

	if (clist->row_height == 0 || !GTK_WIDGET_DRAWABLE (clist))
		return;

	first_row = ROW_FROM_YPIXEL (clist, area->y);
	last_row = ROW_FROM_YPIXEL (clist, area->y + area->height);

	/* this is a small special case which exposes the bottom cell line
	 * on the last row -- it might go away if I change the wall the cell
	 * spacings are drawn
	 */
	if (clist->rows == first_row)
		first_row--;

	list = ROW_ELEMENT (clist, first_row);
	for (i = first_row; i <= last_row ; i++) {
		if (list == NULL)
			break;

		GTK_CLIST_CLASS ((GTK_OBJECT (clist))->klass)->draw_row (clist, area, i, 
									   list->data);
		list = list->next;
	}
}

static void 
gtk_flist_resize_column (GtkCList *clist, int column, int width)
{
	/* override resize column to invalidate the title */
	GtkFList *flist;

	g_assert (GTK_IS_FLIST (clist));

	flist = GTK_FLIST (clist);

	gtk_widget_queue_draw (flist->details->title);
		
	NAUTILUS_CALL_PARENT_CLASS (GTK_CLIST_CLASS, resize_column, (clist, column, width));
}

static void
gtk_flist_track_new_column_width (GtkCList *clist, int column, int new_width)
{
	GtkFList *flist;
	GdkRectangle title_redraw_area;

	flist = GTK_FLIST (clist);

	/* pin new_width to min and max values */
	if (new_width < MAX (COLUMN_MIN_WIDTH, clist->column[column].min_width))
		new_width = MAX (COLUMN_MIN_WIDTH, clist->column[column].min_width);
	if (clist->column[column].max_width >= 0 &&
	    new_width > clist->column[column].max_width)
		new_width = clist->column[column].max_width;

	/* check to see if the pinned value is still different */
	if (clist->column[column].width == new_width)
		return;

	/* set the new width */
	clist->column[column].width = new_width;
	clist->column[column].width_set = TRUE;

	size_allocate_columns (clist, TRUE);
	size_allocate_title_buttons (clist);

	/* redraw the invalid columns */
	if (clist->freeze_count == 0) {
	
  		GdkRectangle area;

		area = clist->column_title_area;
		area.x = clist->column[column].area.x;
		area.height += clist->clist_window_height;

		draw_rows (clist, &area);
	}

	/* draw the column title
	 * ToDo:
	 * fix this up
	 */
	title_redraw_area.x = GTK_WIDGET (flist->details->title)->allocation.x;
	title_redraw_area.y = GTK_WIDGET (flist->details->title)->allocation.y;
	title_redraw_area.width = GTK_WIDGET (flist->details->title)->allocation.width;
	title_redraw_area.height = GTK_WIDGET (flist->details->title)->allocation.height;

	(GTK_WIDGET_CLASS (NAUTILUS_KLASS (flist->details->title)))->
		draw (flist->details->title, &title_redraw_area);
}

/* Our handler for motion_notify events.  We override all of GtkCList's broken
 * behavior.
 */
static gint
gtk_flist_motion (GtkWidget *widget, GdkEventMotion *event)
{
	GtkFList *flist;
	GtkCList *clist;

	g_return_val_if_fail (widget != NULL, FALSE);
	g_return_val_if_fail (GTK_IS_FLIST (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	flist = GTK_FLIST (widget);
	clist = GTK_CLIST (widget);

	if (event->window != clist->clist_window)
		return NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, motion_notify_event, (widget, event));

	if (!((flist->details->dnd_press_button == 1 && (event->state & GDK_BUTTON1_MASK))
	      || (flist->details->dnd_press_button == 2 && (event->state & GDK_BUTTON2_MASK))))
		return FALSE;

	/* This is the same threshold value that is used in gtkdnd.c */

	if (MAX (abs (flist->details->dnd_press_x - event->x),
		 abs (flist->details->dnd_press_y - event->y)) <= 3)
		return FALSE;

	/* Handle any pending selections */

	if (flist->details->dnd_select_pending) {
		select_row (flist,
			    flist->details->button_down_row,
			    flist->details->dnd_select_pending_state);

		flist->details->dnd_select_pending = FALSE;
		flist->details->dnd_select_pending_state = 0;
	}

	gtk_signal_emit (GTK_OBJECT (flist),
			 flist_signals[START_DRAG],
			 flist->details->dnd_press_button,
			 event);
	return TRUE;
}

void 
gtk_flist_column_resize_track_start (GtkWidget *widget, int column)
{
	GtkCList *clist;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_FLIST (widget));

	clist = GTK_CLIST (widget);
	clist->drag_pos = column;
}

void 
gtk_flist_column_resize_track (GtkWidget *widget, int column)
{
	GtkCList *clist;
	int x;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_FLIST (widget));

	clist = GTK_CLIST (widget);

	gtk_widget_get_pointer (widget, &x, NULL);
	gtk_flist_track_new_column_width (clist, column, 
					  new_column_width (clist, column, &x));

}

void 
gtk_flist_column_resize_track_end (GtkWidget *widget, int column)
{
	GtkCList *clist;

	g_return_if_fail (widget != NULL);
	g_return_if_fail (GTK_IS_FLIST (widget));

	clist = GTK_CLIST (widget);
	clist->drag_pos = -1;
}

/* Our handler for key_press and key_release events.  We do nothing, and we do
 * this to avoid GtkCList's broken behavior.
 */
static gint
gtk_flist_key (GtkWidget *widget, GdkEventKey *event)
{
	return FALSE;
}

/* We override the drag_begin signal to do nothing */
static void
gtk_flist_drag_begin (GtkWidget *widget, GdkDragContext *context)
{
	/* nothing */
}

/* We override the drag_end signal to do nothing */
static void
gtk_flist_drag_end (GtkWidget *widget, GdkDragContext *context)
{
	/* nothing */
}

/* We override the drag_data_get signal to do nothing */
static void
gtk_flist_drag_data_get (GtkWidget *widget, GdkDragContext *context,
			 GtkSelectionData *data, guint info, guint time)
{
	/* nothing */
}

/* We override the drag_leave signal to do nothing */
static void
gtk_flist_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time)
{
	/* nothing */
}

/* We override the drag_motion signal to do nothing */
static gboolean
gtk_flist_drag_motion (GtkWidget *widget, GdkDragContext *context,
		       gint x, gint y, guint time)
{
	return FALSE;
}

/* We override the drag_drop signal to do nothing */
static gboolean
gtk_flist_drag_drop (GtkWidget *widget, GdkDragContext *context,
		     gint x, gint y, guint time)
{
	return FALSE;
}

/* We override the drag_data_received signal to accept colors. */
static void
gtk_flist_drag_data_received (GtkWidget *widget, GdkDragContext *context,
			      gint x, gint y, GtkSelectionData *data,
			      guint info, guint time)
{
	switch (info) {
	case TARGET_COLOR:
		nautilus_background_receive_dropped_color
			(nautilus_get_widget_background (widget),
			 widget, x, y, data);
		break;
	default:
		g_assert_not_reached ();
	}
}

/* Our handler for the clear signal of the clist.  We have to reset the anchor
 * to null.
 */
static void
gtk_flist_clear (GtkCList *clist)
{
	GtkFList *flist;

	g_return_if_fail (clist != NULL);
	g_return_if_fail (GTK_IS_FLIST (clist));

	flist = GTK_FLIST (clist);
	flist->details->anchor_row = -1;

	NAUTILUS_CALL_PARENT_CLASS (GTK_CLIST_CLASS, clear, (clist));
}


/**
 * gtk_flist_new_with_titles:
 * @columns: The number of columns in the list
 * @titles: The titles for the columns
 * 
 * Return value: The newly-created file list.
 **/
GtkWidget *
gtk_flist_new_with_titles (int columns, char **titles)
{
	GtkFList *flist;

	flist = gtk_type_new (gtk_flist_get_type ());
	gtk_clist_construct (GTK_CLIST (flist), columns, NULL);
	if (titles) {
		GtkCList *clist;
		int index;

		clist = GTK_CLIST(flist);
		
		for (index = 0; index < columns; index++) {
  			clist->column[index].title = g_strdup (titles[index]);
  		}
    	}

	gtk_clist_set_selection_mode (GTK_CLIST (flist),
				      GTK_SELECTION_MULTIPLE);

	return GTK_WIDGET (flist);
}

GList *
gtk_flist_get_selection (GtkFList *flist)
{
	GList *retval;
	GList *p;

	g_return_val_if_fail (flist != NULL, NULL);
	g_return_val_if_fail (GTK_IS_FLIST (flist), NULL);

	retval = NULL;
	for (p = GTK_CLIST (flist)->row_list; p != NULL; p = p->next) {
		GtkCListRow *row;

		row = p->data;
		if (row->state == GTK_STATE_SELECTED)
			retval = g_list_prepend (retval, row->data);
	}

	return retval;
}
