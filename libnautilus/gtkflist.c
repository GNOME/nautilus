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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gtkflist.h"

#include <gtk/gtkdnd.h>
#include "nautilus-gtk-macros.h"
#include "nautilus-background.h"

struct _GtkFListDetails
{
	/* The anchor row for range selections */
	int anchor_row;

	/* Mouse button and position saved on button press */
	int dnd_press_button;
	int dnd_press_x, dnd_press_y;

	/* Delayed selection information */
	int dnd_select_pending;
	guint dnd_select_pending_state;
	int dnd_select_pending_row;
};

#define ARRAY_LENGTH(a) (sizeof(a) / sizeof((a)[0]))

enum {
	ROW_POPUP_MENU,
	EMPTY_POPUP_MENU,
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

static void gtk_flist_clear (GtkCList *clist);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (GtkFList, gtk_flist, GTK_TYPE_CLIST)

static guint flist_signals[LAST_SIGNAL];

/* Standard class initialization function */
static void
gtk_flist_initialize_class (GtkFListClass *class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkCListClass *clist_class;

	object_class = (GtkObjectClass *) class;
	widget_class = (GtkWidgetClass *) class;
	clist_class = (GtkCListClass *) class;

	flist_signals[ROW_POPUP_MENU] =
		gtk_signal_new ("row_popup_menu",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkFListClass, row_popup_menu),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_GDK_EVENT);
	flist_signals[EMPTY_POPUP_MENU] =
		gtk_signal_new ("empty_popup_menu",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (GtkFListClass, empty_popup_menu),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_GDK_EVENT);
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
				GTK_SIGNAL_OFFSET (GtkFListClass, start_drag),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, flist_signals, LAST_SIGNAL);

	clist_class->clear = gtk_flist_clear;

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
}

/* Standard object initialization function */
static void
gtk_flist_initialize (GtkFList *flist)
{

	flist->details = g_new0 (GtkFListDetails, 1);
	flist->details->anchor_row = -1;

	/* GtkCList does not specify pointer motion by default */
	gtk_widget_add_events (GTK_WIDGET (flist), GDK_POINTER_MOTION_MASK);

	/* Get ready to accept some dragged stuff. */
	gtk_drag_dest_set (GTK_WIDGET (flist),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
			   gtk_flist_dnd_target_table,
			   ARRAY_LENGTH (gtk_flist_dnd_target_table),
			   GDK_ACTION_COPY);
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

	gtk_signal_emit (GTK_OBJECT (flist), flist_signals[SELECTION_CHANGED]);
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

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button == 1 || event->button == 2) {
			if (on_row) {
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
					flist->details->dnd_select_pending_row = row;
				}

				select_row (flist, row, event->state);
			} else {
				gtk_clist_unselect_all (clist);
			}

			retval = TRUE;
		} else if (event->button == 3) {
			if (on_row) {
				select_row (flist, row, event->state);
				gtk_signal_emit (GTK_OBJECT (flist),
						 flist_signals[ROW_POPUP_MENU],
						 event);
			} else
				gtk_signal_emit (GTK_OBJECT (flist),
						 flist_signals[EMPTY_POPUP_MENU],
						 event);

			retval = TRUE;
		}

		break;

	case GDK_2BUTTON_PRESS:
		if (event->button == 1) {
			GtkCListRow *elem;

			flist->details->dnd_select_pending = FALSE;
			flist->details->dnd_select_pending_state = 0;

			if (on_row) {
				elem = g_list_nth (GTK_CLIST (flist)->row_list,
						   row)->data;
				gtk_signal_emit (GTK_OBJECT (flist),
						 flist_signals[ACTIVATE],
						 elem->data);
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
		if (flist->details->dnd_select_pending) {
			/*  select_row (flist, row, flist->details->dnd_select_pending_state); */
			flist->details->dnd_select_pending = FALSE;
			flist->details->dnd_select_pending_state = 0;
		}

		retval = TRUE;
	}

	return retval;
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
			    flist->details->dnd_select_pending_row,
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
	gtk_clist_construct (GTK_CLIST (flist), columns, titles);

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
