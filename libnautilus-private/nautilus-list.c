/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-list.h: Enhanced version of GtkCList for Nautilus.

   Copyright (C) 1999, 2000 Free Software Foundation
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

   Authors: Federico Mena <federico@nuclecu.unam.mx>,
            Ettore Perazzoli <ettore@gnu.org>,
            John Sullivan <sullivan@eazel.com>,
	    Pavel Cisler <pavel@eazel.com>
 */

#include <config.h>
#include "nautilus-list.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtkbindings.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkenums.h>
#include <gtk/gtkmain.h>
#include <glib.h>

#include "nautilus-background.h"
#include "nautilus-gdk-extensions.h"
#include "nautilus-gdk-pixbuf-extensions.h"
#include "nautilus-glib-extensions.h"
#include "nautilus-global-preferences.h"
#include "nautilus-gtk-macros.h"
#include "nautilus-list-column-title.h"

/* Timeout for making the row currently selected for keyboard operation visible. */
/* FIXME bugzilla.eazel.com 611: 
 * This *must* be higher than the double-click time in GDK,
 * but there is no way to access its value from outside.
 */
#define KEYBOARD_ROW_REVEAL_TIMEOUT 300

struct NautilusListDetails
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

	/* Timeout used to make a selected row fully visible after a short
	 * period of time. (The timeout is needed to make sure
	 * double-clicking still works, and to optimize holding down arrow key.)
	 */
	guint keyboard_row_reveal_timer_id;
	int keyboard_row_to_reveal;

	/* Signal IDs that we sometimes want to block. */
	guint select_row_signal_id;
	guint unselect_row_signal_id;

	/* Delayed selection information */
	int dnd_select_pending;
	guint dnd_select_pending_state;

	GtkWidget *title;
};

/* maximum amount of milliseconds the mouse button is allowed to stay down and still be considered a click */
#define MAX_CLICK_TIME 1500

/* horizontal space between images in a pixbuf list cell */
#define PIXBUF_LIST_SPACING	2

/* Some #defines stolen from gtkclist.c that we need for other stolen code. */

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

static GtkTargetEntry nautilus_list_dnd_target_table[] = {
	{ "application/x-color", 0, TARGET_COLOR }
};

static void     activate_row                            (NautilusList         *list,
							 gint                  row);
static int      get_cell_horizontal_start_position      (GtkCList             *clist,
							 GtkCListRow          *clist_row,
							 int                   column,
							 int                   content_width);
static void     get_cell_style                          (GtkCList             *clist,
							 GtkCListRow          *clist_row,
							 gint                  state,
							 gint                  column,
							 GtkStyle            **style,
							 GdkGC               **fg_gc,
							 GdkGC               **bg_gc);
static void     nautilus_list_initialize_class          (NautilusListClass    *class);
static void     nautilus_list_initialize                (NautilusList         *list);
static void     nautilus_list_destroy                   (GtkObject            *object);
static gint     nautilus_list_button_press              (GtkWidget            *widget,
							 GdkEventButton       *event);
static gint     nautilus_list_button_release            (GtkWidget            *widget,
							 GdkEventButton       *event);
static gint     nautilus_list_motion                    (GtkWidget            *widget,
							 GdkEventMotion       *event);
static void     nautilus_list_drag_begin                (GtkWidget            *widget,
							 GdkDragContext       *context);
static void     nautilus_list_drag_end                  (GtkWidget            *widget,
							 GdkDragContext       *context);
static void     nautilus_list_drag_data_get             (GtkWidget            *widget,
							 GdkDragContext       *context,
							 GtkSelectionData     *data,
							 guint                 info,
							 guint                 time);
static void     nautilus_list_drag_leave                (GtkWidget            *widget,
							 GdkDragContext       *context,
							 guint                 time);
static gboolean nautilus_list_drag_motion               (GtkWidget            *widget,
							 GdkDragContext       *context,
							 gint                  x,
							 gint                  y,
							 guint                 time);
static gboolean nautilus_list_drag_drop                 (GtkWidget            *widget,
							 GdkDragContext       *context,
							 gint                  x,
							 gint                  y,
							 guint                 time);
static void     nautilus_list_drag_data_received        (GtkWidget            *widget,
							 GdkDragContext       *context,
							 gint                  x,
							 gint                  y,
							 GtkSelectionData     *data,
							 guint                 info,
							 guint                 time);
static void     nautilus_list_clear_keyboard_focus      (NautilusList         *list);
static void     nautilus_list_draw_focus                (GtkWidget            *widget);
static int      nautilus_list_get_first_selected_row    (NautilusList         *list);
static int      nautilus_list_get_last_selected_row     (NautilusList         *list);
static gint     nautilus_list_key_press                 (GtkWidget            *widget,
							 GdkEventKey          *event);
static void     nautilus_list_unselect_all              (GtkCList             *clist);
static void     nautilus_list_select_all                (GtkCList             *clist);
static void     reveal_row                              (NautilusList         *list,
							 int                   row);
static void     schedule_keyboard_row_reveal            (NautilusList         *list,
							 int                   row);
static void     unschedule_keyboard_row_reveal          (NautilusList         *list);
static void     emit_selection_changed                  (NautilusList         *clist);
static void     nautilus_list_clear                     (GtkCList             *clist);
static void     draw_row                                (GtkCList             *list,
							 GdkRectangle         *area,
							 gint                  row,
							 GtkCListRow          *clist_row);
static void     nautilus_list_realize                   (GtkWidget            *widget);
static void     nautilus_list_set_cell_contents         (GtkCList             *clist,
							 GtkCListRow          *clist_row,
							 gint                  column,
							 GtkCellType           type,
							 const gchar          *text,
							 guint8                spacing,
							 GdkPixmap            *pixmap,
							 GdkBitmap            *mask);
static void     nautilus_list_size_request              (GtkWidget            *widget,
							 GtkRequisition       *requisition);
static void     nautilus_list_resize_column             (GtkCList             *widget,
							 int                   column,
							 int                   width);
static void     nautilus_list_column_resize_track_start (GtkWidget            *widget,
							 int                   column);
static void     nautilus_list_column_resize_track       (GtkWidget            *widget,
							 int                   column);
static void     nautilus_list_column_resize_track_end   (GtkWidget            *widget,
							 int                   column);
static gboolean row_set_selected                        (NautilusList         *list,
							 int                   row,
							 GtkCListRow          *clist_row,
							 gboolean              select);
static gboolean select_row_unselect_others              (NautilusList         *list,
							 int                   row_to_select);
static void     click_policy_changed_callback           (gpointer              user_data);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusList, nautilus_list, GTK_TYPE_CLIST)

static guint list_signals[LAST_SIGNAL];

/* Standard class initialization function */
static void
nautilus_list_initialize_class (NautilusListClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkCListClass *clist_class;
	NautilusListClass *list_class;

	GtkBindingSet *clist_binding_set;

	object_class = (GtkObjectClass *) klass;
	widget_class = (GtkWidgetClass *) klass;
	clist_class = (GtkCListClass *) klass;
	list_class = (NautilusListClass *) klass;

	list_signals[CONTEXT_CLICK_SELECTION] =
		gtk_signal_new ("context_click_selection",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusListClass, context_click_selection),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	list_signals[CONTEXT_CLICK_BACKGROUND] =
		gtk_signal_new ("context_click_background",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusListClass, context_click_background),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
	list_signals[ACTIVATE] =
		gtk_signal_new ("activate",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusListClass, activate),
				gtk_marshal_NONE__POINTER,
				GTK_TYPE_NONE, 1,
				GTK_TYPE_POINTER);
	list_signals[START_DRAG] =
		gtk_signal_new ("start_drag",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusListClass, start_drag),
				gtk_marshal_NONE__INT_POINTER,
				GTK_TYPE_NONE, 2,
				GTK_TYPE_INT,
				GTK_TYPE_GDK_EVENT);
	list_signals[SELECTION_CHANGED] =
		gtk_signal_new ("selection_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusListClass, selection_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);

	gtk_object_class_add_signals (object_class, list_signals, LAST_SIGNAL);

	/* Turn off the GtkCList key bindings that we want unbound.
	 * We only need to do this for the keys that we don't handle
	 * in nautilus_list_key_press. These extra ones are turned off
	 * to avoid inappropriate GtkCList code and to standardize the
	 * keyboard behavior in Nautilus.
	 */
	clist_binding_set = gtk_binding_set_by_class (clist_class);

	/* Use Control-A for Select All, not Control-/ */
	gtk_binding_entry_clear (clist_binding_set, 
				 '/', 
				 GDK_CONTROL_MASK);
	/* Don't use Control-\ for Unselect All (maybe invent Nautilus 
	 * standard for this?) */
	gtk_binding_entry_clear (clist_binding_set, 
				 '\\', 
				 GDK_CONTROL_MASK);
	/* Hide GtkCList's weird extend-selection-from-keyboard stuff.
	 * Users can use control-navigation and control-space to create
	 * extended selections.
	 */
	gtk_binding_entry_clear (clist_binding_set, 
				 GDK_Shift_L, 
				 GDK_RELEASE_MASK | GDK_SHIFT_MASK);
	gtk_binding_entry_clear (clist_binding_set, 
				 GDK_Shift_R, 
				 GDK_RELEASE_MASK | GDK_SHIFT_MASK);
	gtk_binding_entry_clear (clist_binding_set, 
				 GDK_Shift_L, 
				 GDK_RELEASE_MASK | GDK_SHIFT_MASK | GDK_CONTROL_MASK);
	gtk_binding_entry_clear (clist_binding_set, 
				 GDK_Shift_R, 
				 GDK_RELEASE_MASK | GDK_SHIFT_MASK | GDK_CONTROL_MASK);

	list_class->column_resize_track_start = nautilus_list_column_resize_track_start;
	list_class->column_resize_track = nautilus_list_column_resize_track;
	list_class->column_resize_track_end = nautilus_list_column_resize_track_end;

	clist_class->clear = nautilus_list_clear;
	clist_class->draw_row = draw_row;
  	clist_class->resize_column = nautilus_list_resize_column;
  	clist_class->set_cell_contents = nautilus_list_set_cell_contents;
  	clist_class->select_all = nautilus_list_select_all;
  	clist_class->unselect_all = nautilus_list_unselect_all;

	widget_class->button_press_event = nautilus_list_button_press;
	widget_class->button_release_event = nautilus_list_button_release;
	widget_class->motion_notify_event = nautilus_list_motion;
	widget_class->drag_begin = nautilus_list_drag_begin;
	widget_class->drag_end = nautilus_list_drag_end;
	widget_class->drag_data_get = nautilus_list_drag_data_get;
	widget_class->drag_leave = nautilus_list_drag_leave;
	widget_class->drag_motion = nautilus_list_drag_motion;
	widget_class->drag_drop = nautilus_list_drag_drop;
	widget_class->drag_data_received = nautilus_list_drag_data_received;
	widget_class->draw_focus = nautilus_list_draw_focus;
	widget_class->key_press_event = nautilus_list_key_press;
	widget_class->realize = nautilus_list_realize;
	widget_class->size_request = nautilus_list_size_request;

	object_class->destroy = nautilus_list_destroy;
}

/* Standard object initialization function */
static void
nautilus_list_initialize (NautilusList *list)
{	
	list->details = g_new0 (NautilusListDetails, 1);
	list->details->anchor_row = -1;
	
	/* GtkCList does not specify pointer motion by default */
	gtk_widget_add_events (GTK_WIDGET (list), GDK_POINTER_MOTION_MASK);

	/* Get ready to accept some dragged stuff. */
	gtk_drag_dest_set (GTK_WIDGET (list),
			   GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_DROP, 
			   nautilus_list_dnd_target_table,
			   NAUTILUS_N_ELEMENTS (nautilus_list_dnd_target_table),
			   GDK_ACTION_COPY);

	/* Emit "selection changed" signal when parent class changes selection */
	list->details->select_row_signal_id = gtk_signal_connect (GTK_OBJECT (list),
			    					  "select_row",
			    					  emit_selection_changed,
			    					  list);
	list->details->unselect_row_signal_id = gtk_signal_connect (GTK_OBJECT (list),
			    					    "unselect_row",
			    					    emit_selection_changed,
			    					    list);

	list->details->title = GTK_WIDGET (nautilus_list_column_title_new());

	/* Initialize the single click mode from preferences */
	list->details->single_click_mode = 
		(nautilus_preferences_get_enum (NAUTILUS_PREFERENCES_CLICK_POLICY,
						NAUTILUS_CLICK_POLICY_SINGLE) == NAUTILUS_CLICK_POLICY_SINGLE);

	/* Keep track of changes in clicking policy */
	nautilus_preferences_add_enum_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
						click_policy_changed_callback,
						list);
}

static void
nautilus_list_destroy (GtkObject *object)
{
	NautilusList *list;

	list = NAUTILUS_LIST (object);

	unschedule_keyboard_row_reveal (list);

	nautilus_preferences_remove_callback (NAUTILUS_PREFERENCES_CLICK_POLICY,
					      click_policy_changed_callback,
					      list);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));

	/* Must do this after calling the parent, because GtkCList calls
	 * the clear method, which must have a valid details pointer.
	 */
	g_free (list->details);
}

static void
emit_selection_changed (NautilusList *list) 
{
	g_assert (NAUTILUS_IS_LIST (list));
	gtk_signal_emit (GTK_OBJECT (list), list_signals[SELECTION_CHANGED]);
}

static void
activate_row (NautilusList *list, gint row)
{
	GtkCListRow *elem;

	elem = g_list_nth (GTK_CLIST (list)->row_list,
			   row)->data;
	gtk_signal_emit (GTK_OBJECT (list),
			 list_signals[ACTIVATE],
			 elem->data);
}

gboolean
nautilus_list_is_row_selected (NautilusList *list, gint row)
{
	GtkCListRow *elem;

	g_return_val_if_fail (row >= 0, FALSE);
	g_return_val_if_fail (row < GTK_CLIST (list)->rows, FALSE);

	elem = g_list_nth (GTK_CLIST (list)->row_list, row)->data;

	return elem->state == GTK_STATE_SELECTED;
}

/* Selects the rows between the anchor to the specified row, inclusive.
 * Returns TRUE if selection changed.  */
static gboolean
select_range (NautilusList *list, int row)
{
	int min, max;
	int i;
	gboolean selection_changed;

	selection_changed = FALSE;

	if (list->details->anchor_row == -1) {
		list->details->anchor_row = row;
	}

	if (row < list->details->anchor_row) {
		min = row;
		max = list->details->anchor_row;
	} else {
		min = list->details->anchor_row;
		max = row;
	}

	for (i = min; i <= max; i++) {
		selection_changed |= row_set_selected (list, i, NULL, TRUE);
	}

	return selection_changed;
}

/* Handles row selection according to the specified modifier state */
static void
select_row_from_mouse (NautilusList *list, int row, guint state)
{
	int range, additive;
	gboolean should_select_row;
	gboolean selection_changed;

	selection_changed = FALSE;

	range = (state & GDK_SHIFT_MASK) != 0;
	additive = (state & GDK_CONTROL_MASK) != 0;

	if (!additive) {
		selection_changed |= select_row_unselect_others (list, -1);
	}

	if (range) {
		selection_changed |= select_range (list, row);
	} else {
		should_select_row = !additive || !nautilus_list_is_row_selected (list, row);
		selection_changed |= row_set_selected (list, row, NULL, should_select_row);
		list->details->anchor_row = row;
	}

	if (selection_changed) {
		emit_selection_changed (list);
	}
}

/* 
 * row_set_selected:
 * 
 * Select or unselect a row. Return TRUE if selection has changed. 
 * Does not emit the SELECTION_CHANGED signal; it's up to the caller
 * to handle that.
 *
 * @list: The NautilusList in question.
 * @row: index of row number to select or unselect.
 * @clist_row: GtkCListRow pointer for given list. Passing this avoids
 * expensive lookup. If it's NULL, it will be looked up in this function.
 * @select: TRUE if row should be selected, FALSE otherwise.
 * 
 * Return Value: TRUE if selection has changed, FALSE otherwise.
 */
static gboolean
row_set_selected (NautilusList *list, int row, GtkCListRow *clist_row, gboolean select)
{
	g_assert (row >= 0 && row < GTK_CLIST (list)->rows);

	if (clist_row == NULL) {
		clist_row = ROW_ELEMENT (GTK_CLIST (list), row)->data;
	}

	if (select == (clist_row->state == GTK_STATE_SELECTED)) {
		return FALSE;
	}

	/* Block signal handlers so we can make sure the selection-changed
	 * signal gets sent only once.
	 */
	gtk_signal_handler_block (GTK_OBJECT(list), 
				  list->details->select_row_signal_id);
	gtk_signal_handler_block (GTK_OBJECT(list), 
				  list->details->unselect_row_signal_id);
	
	if (select) {
		gtk_clist_select_row (GTK_CLIST (list), row, -1);
	} else {
		gtk_clist_unselect_row (GTK_CLIST (list), row, -1);
	}

	gtk_signal_handler_unblock (GTK_OBJECT(list), 
				    list->details->select_row_signal_id);
	gtk_signal_handler_unblock (GTK_OBJECT(list), 
				    list->details->unselect_row_signal_id);

	return TRUE;
}

/**
 * select_row_unselect_others:
 * 
 * Change the selected rows as necessary such that only
 * the given row remains selected.
 * 
 * @list: The NautilusList in question.
 * @row: The row number to leave selected. Use -1 to leave
 * no row selected.
 * 
 * Return value: TRUE if the selection changed; FALSE otherwise.
 */
static gboolean
select_row_unselect_others (NautilusList *list, int row_to_select)
{
	GList *p;
	int row;
	gboolean selection_changed;

	g_return_val_if_fail (NAUTILUS_IS_LIST (list), FALSE);

	selection_changed = FALSE;
	for (p = GTK_CLIST (list)->row_list, row = 0; p != NULL; p = p->next, ++row) {
		selection_changed |= row_set_selected (list, row, p->data, row == row_to_select);
	}

	return selection_changed;
}

static void
nautilus_list_unselect_all (GtkCList *clist)
{
	g_return_if_fail (NAUTILUS_IS_LIST (clist));

	if (select_row_unselect_others (NAUTILUS_LIST (clist), -1)) {
		emit_selection_changed (NAUTILUS_LIST (clist));
	}
}

static void
nautilus_list_select_all (GtkCList *clist)
{
	GList *p;
	int row;
	gboolean selection_changed;

	g_return_if_fail (NAUTILUS_IS_LIST (clist));

	selection_changed = FALSE;
	for (p = clist->row_list, row = 0; p != NULL; p = p->next, ++row) {
		selection_changed |= row_set_selected (NAUTILUS_LIST (clist), row, p->data, TRUE);
	}

	if (selection_changed) {
		emit_selection_changed (NAUTILUS_LIST (clist));
	}
}

/* Our handler for button_press events.  We override all of GtkCList's broken
 * behavior.
 */
static gint
nautilus_list_button_press (GtkWidget *widget, GdkEventButton *event)
{
	NautilusList *list;
	GtkCList *clist;
	int on_row;
	gint row, col;
	int retval;

	g_return_val_if_fail (NAUTILUS_IS_LIST (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	list = NAUTILUS_LIST (widget);
	clist = GTK_CLIST (widget);
	retval = FALSE;

	if (event->window != clist->clist_window)
		return NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, button_press_event, (widget, event));

	on_row = gtk_clist_get_selection_info (clist, event->x, event->y, &row, &col);
	list->details->button_down_time = event->time;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button == 1 || event->button == 2) {
			if (on_row) {

				/* Save the clicked row for DnD and single-click activate */
				
				list->details->button_down_row = row;

				/* Save the mouse info for DnD */

				list->details->dnd_press_button = event->button;
				list->details->dnd_press_x = event->x;
				list->details->dnd_press_y = event->y;
	
				/* Handle selection */

				if ((nautilus_list_is_row_selected (list, row)
				     && !(event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)))
				    || ((event->state & GDK_CONTROL_MASK)
					&& !(event->state & GDK_SHIFT_MASK))) {
					list->details->dnd_select_pending = TRUE;
					list->details->dnd_select_pending_state = event->state;
				}

				select_row_from_mouse (list, row, event->state);
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
				select_row_from_mouse (list, row, event->state);
				gtk_signal_emit (GTK_OBJECT (list),
						 list_signals[CONTEXT_CLICK_SELECTION]);
			} else
				gtk_signal_emit (GTK_OBJECT (list),
						 list_signals[CONTEXT_CLICK_BACKGROUND]);

			retval = TRUE;
		}

		break;

	case GDK_2BUTTON_PRESS:
		if (event->button == 1) {
			list->details->dnd_select_pending = FALSE;
			list->details->dnd_select_pending_state = 0;

			if (on_row) {
				/* Activate on double-click even if single_click_mode
				 * is set, so second click doesn't get passed to child
				 * directory.
				 */
				activate_row (list, row);
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
nautilus_list_button_release (GtkWidget *widget, GdkEventButton *event)
{
	NautilusList *list;
	GtkCList *clist;
	GtkCListRow *clist_row;
	int on_row;
	gint row, col;
	GtkStyle *style;
	int text_x, text_width;
	int retval;

	g_return_val_if_fail (NAUTILUS_IS_LIST (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	list = NAUTILUS_LIST (widget);
	clist = GTK_CLIST (widget);
	retval = FALSE;

	if (event->window != clist->clist_window)
		return NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, button_release_event, (widget, event));

	on_row = gtk_clist_get_selection_info (clist, event->x, event->y, &row, &col);

	if (!(event->button == 1 || event->button == 2))
		return FALSE;

	list->details->dnd_press_button = 0;
	list->details->dnd_press_x = 0;
	list->details->dnd_press_y = 0;

	if (on_row) {
		/* Clean up after abortive drag-and-drop attempt (since user can't
		 * reorder list view items, releasing mouse in list view cancels
		 * drag-and-drop possibility). 
		 */
		if (list->details->dnd_select_pending) {
			list->details->dnd_select_pending = FALSE;
			list->details->dnd_select_pending_state = 0;
		}

		/* 
		 * Activate on single click if not extending selection, mouse hasn't moved to
		 * a different row, not too much time has passed, and this is a link-type cell.
		 */
		if (list->details->single_click_mode && 
		    !(event->state & (GDK_CONTROL_MASK | GDK_SHIFT_MASK)))
		{
			gint elapsed_time = event->time - list->details->button_down_time;

			if (elapsed_time < MAX_CLICK_TIME && list->details->button_down_row == row)
			{
				clist_row = ROW_ELEMENT (clist, row)->data;
				if (clist_row->cell[col].type == NAUTILUS_CELL_LINK_TEXT) {
					/* One final test. Check whether the click was in the
					 * horizontal bounds of the displayed text.
					 */
					get_cell_style (clist, clist_row, GTK_STATE_NORMAL, col, &style, NULL, NULL);
					text_width = gdk_string_width (style->font, GTK_CELL_TEXT (clist_row->cell[col])->text);
					text_x = get_cell_horizontal_start_position (clist, clist_row, col, text_width);
					if (event->x >= text_x && event->x <= text_x + text_width) {
						activate_row (list, row);
					}
				}
			}
		}		
	
		retval = TRUE;
	}

	return retval;
}

static void
nautilus_list_clear_keyboard_focus (NautilusList *list)
{
	if (GTK_CLIST (list)->focus_row >= 0) {
		gtk_widget_draw_focus (GTK_WIDGET (list));
	}

	GTK_CLIST (list)->focus_row = -1;
}

static void
nautilus_list_set_keyboard_focus (NautilusList *list, int row)
{
	g_assert (row >= 0 && row < GTK_CLIST (list)->rows);

	if (row == GTK_CLIST (list)->focus_row) {
		return;
	}

	nautilus_list_clear_keyboard_focus (list);

	GTK_CLIST (list)->focus_row = row;

	gtk_widget_draw_focus (GTK_WIDGET (list));
}

static void
nautilus_list_keyboard_move_to (NautilusList *list, int row, GdkEventKey *event)
{
	GtkCList *clist;

	g_assert (NAUTILUS_IS_LIST (list));
	g_assert (row >= 0 || row < GTK_CLIST (list)->rows);

	clist = GTK_CLIST (list);

	if ((event->state & GDK_CONTROL_MASK) != 0) {
		/* Move the keyboard focus. */
		nautilus_list_set_keyboard_focus (list, row);
	} else {
		/* Select row and get rid of special keyboard focus. */
		nautilus_list_clear_keyboard_focus (list);
		if (select_row_unselect_others (list, row)) {
			emit_selection_changed (list);
		}
	}

	schedule_keyboard_row_reveal (list, row);
}

static gboolean
keyboard_row_reveal_timeout_callback (gpointer data)
{
	NautilusList *list;
	int row;

	GDK_THREADS_ENTER ();

	list = NAUTILUS_LIST (data);
	row = list->details->keyboard_row_to_reveal;

	if (row >= 0 && row < GTK_CLIST (list)->rows) {	
		/* Only reveal the row if it's still the keyboard focus
		 * or if it's still selected.
		 */
		/* FIXME bugzilla.eazel.com 612: 
		 * Need to unschedule this if the user scrolls explicitly.
		 */
		if (row == GTK_CLIST (list)->focus_row
		    || nautilus_list_is_row_selected (list, row)) {
			reveal_row (list, row);
		}
		list->details->keyboard_row_reveal_timer_id = 0;
	}

	GDK_THREADS_LEAVE ();

	return FALSE;
}

static void
unschedule_keyboard_row_reveal (NautilusList *list) 
{
	if (list->details->keyboard_row_reveal_timer_id != 0) {
		gtk_timeout_remove (list->details->keyboard_row_reveal_timer_id);
	}
}

static void
schedule_keyboard_row_reveal (NautilusList *list, int row)
{
	unschedule_keyboard_row_reveal (list);

	list->details->keyboard_row_to_reveal = row;
	list->details->keyboard_row_reveal_timer_id
		= gtk_timeout_add (KEYBOARD_ROW_REVEAL_TIMEOUT,
				   keyboard_row_reveal_timeout_callback,
				   list);
}

static void
reveal_row (NautilusList *list, int row)
{
	GtkCList *clist;

	g_assert (NAUTILUS_IS_LIST (list));
	
	clist = GTK_CLIST (list);
	
	if (ROW_TOP_YPIXEL (clist, row) + clist->row_height >
      		      clist->clist_window_height) {
		gtk_clist_moveto (clist, row, -1, 1, 0);
     	} else if (ROW_TOP_YPIXEL (clist, row) < 0) {
		gtk_clist_moveto (clist, row, -1, 0, 0);
     	}
}

static void
nautilus_list_keyboard_navigation_key_press (NautilusList *list, GdkEventKey *event,
			          	     GtkScrollType scroll_type, gboolean jump_to_end)
{
	GtkCList *clist;
	int start_row;
	int destination_row;
	int rows_per_page;

	g_assert (NAUTILUS_IS_LIST (list));

	clist = GTK_CLIST (list);
	
	if (scroll_type == GTK_SCROLL_JUMP) {
		destination_row = (jump_to_end ?
				   clist->rows - 1 :
				   0);
	} else {
		/* Choose the row to start with.
		 * If we have a keyboard focus, start with it.
		 * If there's a selection, use the selected row farthest toward the end.
		 */

		if (GTK_CLIST (list)->focus_row >= 0) {
			start_row = clist->focus_row;
		} else {
			start_row = (scroll_type == GTK_SCROLL_STEP_FORWARD || scroll_type == GTK_SCROLL_PAGE_FORWARD ?
				     nautilus_list_get_last_selected_row (list) :
				     nautilus_list_get_first_selected_row (list));
		}

		/* If there's no row to start with, select the row farthest toward the end.
		 * If there is a row to start with, select the next row in the arrow direction.
		 */
		if (start_row < 0) {
			destination_row = (scroll_type == GTK_SCROLL_STEP_FORWARD || scroll_type == GTK_SCROLL_PAGE_FORWARD ?
					   clist->rows - 1 :
					   0);
		} else if (scroll_type == GTK_SCROLL_STEP_FORWARD) {
			destination_row = MIN (clist->rows - 1, start_row + 1);
		} else if (scroll_type == GTK_SCROLL_STEP_BACKWARD) {
			destination_row = MAX (0, start_row - 1);
		} else {
			g_assert (scroll_type == GTK_SCROLL_PAGE_FORWARD || GTK_SCROLL_PAGE_BACKWARD);
			rows_per_page = (2 * clist->clist_window_height -
					 clist->row_height - CELL_SPACING) /
					(2 * (clist->row_height + CELL_SPACING));
			
			if (scroll_type == GTK_SCROLL_PAGE_FORWARD) {
				destination_row = MIN (clist->rows - 1, 
						       start_row + rows_per_page);
			} else {
				destination_row = MAX (0,
						       start_row - rows_per_page);
			}
		}
	}

	nautilus_list_keyboard_move_to (list, destination_row, event);
}			   

static void
nautilus_list_keyboard_home (NautilusList *list, GdkEventKey *event)
{
	/* Home selects the first row.
	 * Control-Home sets the keyboard focus to the first row.
	 */
	nautilus_list_keyboard_navigation_key_press (list, event, GTK_SCROLL_JUMP, FALSE); 
}

static void
nautilus_list_keyboard_end (NautilusList *list, GdkEventKey *event)
{
	/* End selects the last row.
	 * Control-End sets the keyboard focus to the last row.
	 */
	nautilus_list_keyboard_navigation_key_press (list, event, GTK_SCROLL_JUMP, TRUE); 
}

static void
nautilus_list_keyboard_up (NautilusList *list, GdkEventKey *event)
{
	/* Up selects the next higher row.
	 * Control-Up sets the keyboard focus to the next higher icon.
	 */
	nautilus_list_keyboard_navigation_key_press (list, event, GTK_SCROLL_STEP_BACKWARD, FALSE); 
}

static void
nautilus_list_keyboard_down (NautilusList *list, GdkEventKey *event)
{
	/* Down selects the next lower row.
	 * Control-Down sets the keyboard focus to the next lower icon.
	 */
	nautilus_list_keyboard_navigation_key_press (list, event, GTK_SCROLL_STEP_FORWARD, FALSE); 
}

static void
nautilus_list_keyboard_page_up (NautilusList *list, GdkEventKey *event)
{
	/* Page Up selects a row one screenful higher.
	 * Control-Page Up sets the keyboard focus to the row one screenful higher.
	 */
	nautilus_list_keyboard_navigation_key_press (list, event, GTK_SCROLL_PAGE_BACKWARD, FALSE); 
}

static void
nautilus_list_keyboard_page_down (NautilusList *list, GdkEventKey *event)
{
	/* Page Down selects a row one screenful lower.
	 * Control-Page Down sets the keyboard focus to the row one screenful lower.
	 */
	nautilus_list_keyboard_navigation_key_press (list, event, GTK_SCROLL_PAGE_FORWARD, FALSE); 
}

static void
nautilus_list_keyboard_space (NautilusList *list, GdkEventKey *event)
{
	if (event->state & GDK_CONTROL_MASK) {
		gtk_signal_emit_by_name (GTK_OBJECT (list), "toggle_focus_row");
	}
}

static void
nautilus_list_activate_selected_items (NautilusList *list)
{
	int row;

	for (row = 0; row < GTK_CLIST (list)->rows; ++row) {
		if (nautilus_list_is_row_selected (list, row)) {
			activate_row (list, row);
		}
	}
}

static int
nautilus_list_key_press (GtkWidget *widget,
		 	 GdkEventKey *event)
{
	NautilusList *list;

	list = NAUTILUS_LIST (widget);

	switch (event->keyval) {
	case GDK_Home:
		nautilus_list_keyboard_home (list, event);
		break;
	case GDK_End:
		nautilus_list_keyboard_end (list, event);
		break;
	case GDK_Page_Up:
		nautilus_list_keyboard_page_up (list, event);
		break;
	case GDK_Page_Down:
		nautilus_list_keyboard_page_down (list, event);
		break;
	case GDK_Up:
		nautilus_list_keyboard_up (list, event);
		break;
	case GDK_Down:
		nautilus_list_keyboard_down (list, event);
		break;
	case GDK_space:
		nautilus_list_keyboard_space (list, event);
		break;
	case GDK_Return:
		nautilus_list_activate_selected_items (list);
		break;
	default:
		if (NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, key_press_event, (widget, event))) {
			return TRUE;
		} else {
			return FALSE;
		}
	}

	return TRUE;
}

static void
nautilus_list_realize (GtkWidget *widget)
{
	NautilusList *list;
	GtkCList *clist;

	g_return_if_fail (NAUTILUS_IS_LIST (widget));

	list = NAUTILUS_LIST (widget);
	clist = GTK_CLIST (widget);

	clist->column[0].button = list->details->title;

	NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, realize, (widget));

	if (clist->title_window) {
		gtk_widget_set_parent_window (list->details->title, clist->title_window);
	}
	gtk_widget_set_parent (list->details->title, GTK_WIDGET (clist));
	gtk_widget_show (list->details->title);
	
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
nautilus_list_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
	/* stolen from gtk_clist 
	 * make sure the proper title ammount is allocated for the column
	 * title view --  this would not otherwise be done because 
	 * NautilusList depends the buttons being there when doing a size calculation
	 */
	NautilusList *list;
	GtkCList *clist;

	g_return_if_fail (NAUTILUS_IS_LIST (widget));
	g_return_if_fail (requisition != NULL);

	clist = GTK_CLIST (widget);
	list = NAUTILUS_LIST (widget);

	requisition->width = 0;
	requisition->height = 0;

	/* compute the size of the column title (title) area */
	clist->column_title_area.height = 0;
	if (GTK_CLIST_SHOW_TITLES(clist) && list->details->title) {
		GtkRequisition child_requisition;
		
		gtk_widget_size_request (list->details->title,
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
nautilus_list_draw_focus (GtkWidget *widget)
{
	GdkGCValues saved_values;
	GtkCList *clist;

	g_return_if_fail (NAUTILUS_IS_LIST (widget));

	if (!GTK_WIDGET_DRAWABLE (widget) || !GTK_WIDGET_CAN_FOCUS (widget)) {
  		return;
  	}

	clist = GTK_CLIST (widget);
	if (clist->focus_row < 0) {
		return;
	}

  	gdk_gc_get_values (clist->xor_gc, &saved_values);

  	gdk_gc_set_stipple (clist->xor_gc, nautilus_stipple_bitmap ());
  	gdk_gc_set_fill (clist->xor_gc, GDK_STIPPLED);

    	gdk_draw_rectangle (clist->clist_window, clist->xor_gc, FALSE,
		0, ROW_TOP_YPIXEL(clist, clist->focus_row),
		clist->clist_window_width - 1,
		clist->row_height - 1);
	/* Resetting the stipple to the saved value causes death
	 * deep in Bonobo X handling, believe it or not. Fortunately
	 * we don't need to.
	 */
  	gdk_gc_set_fill (clist->xor_gc, saved_values.fill);
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

static void
gdk_window_size_as_rectangle (GdkWindow *gdk_window, GdkRectangle *rectangle)
{
	gint width, height;

	gdk_window_get_size (gdk_window, &width, &height);	
	rectangle->width = width;
	rectangle->height = height;
}

static gint
draw_cell_pixmap (GdkWindow *window, GdkRectangle *clip_rectangle, GdkGC *fg_gc,
		  GdkPixmap *pixmap, GdkBitmap *mask,
		  gint x, gint y)
{
	GdkRectangle image_rectangle;
	GdkRectangle intersect_rectangle;

	gdk_window_size_as_rectangle (pixmap, &image_rectangle);
	image_rectangle.x = x;
	image_rectangle.y = y;

	if (!gdk_rectangle_intersect (clip_rectangle, &image_rectangle, &intersect_rectangle)) {
		return x;
	}
	
	if (mask) {
		gdk_gc_set_clip_mask (fg_gc, mask);
		gdk_gc_set_clip_origin (fg_gc, x, y);
	}

	gdk_draw_pixmap (window, fg_gc, pixmap, 
			 intersect_rectangle.x - x, intersect_rectangle.y - y, 
			 image_rectangle.x, image_rectangle.y, 
			 intersect_rectangle.width, intersect_rectangle.height);

	if (mask) {
		gdk_gc_set_clip_origin (fg_gc, 0, 0);
		gdk_gc_set_clip_mask (fg_gc, NULL);
	}

	return x + intersect_rectangle.width;
}

static gint
draw_cell_pixbuf (GdkWindow *window, GdkRectangle *clip_rectangle, GdkGC *fg_gc,
		  GdkPixbuf *pixbuf, gint x, gint y)
{
	GdkRectangle image_rectangle;
	GdkRectangle intersect_rectangle;

	image_rectangle.width = gdk_pixbuf_get_width (pixbuf);
	image_rectangle.height = gdk_pixbuf_get_height (pixbuf);
	image_rectangle.x = x;
	image_rectangle.y = y;

	if (!gdk_rectangle_intersect (clip_rectangle, &image_rectangle, &intersect_rectangle)) {
		return x;
	}
	
	gdk_pixbuf_render_to_drawable_alpha (pixbuf, window, 
			 		     intersect_rectangle.x - x, intersect_rectangle.y - y, 
			 		     image_rectangle.x, image_rectangle.y, 
					     intersect_rectangle.width, intersect_rectangle.height,
					     GDK_PIXBUF_ALPHA_BILEVEL, 128, GDK_RGB_DITHER_MAX,
					     0, 0);

	return x + intersect_rectangle.width;
}

/**
 * get_cell_horizontal_start_position:
 * 
 * Get the leftmost x value at which the contents of this cell are painted.
 * 
 * @clist: The list in question.
 * @row: The row data structure for the target cell.
 * @column: The column of the target cell.
 * @content_width: The already-computed width of the cell contents.
 * 
 * Return value: x value at which the contents of this cell are painted.
 */
static int
get_cell_horizontal_start_position (GtkCList *clist, GtkCListRow *clist_row, int column, int content_width)
{
	int initial_offset;

	initial_offset = clist->column[column].area.x + 
			 clist->hoffset + 
			 clist_row->cell[column].horizontal;
	
	switch (clist->column[column].justification) {
		case GTK_JUSTIFY_LEFT:
			return initial_offset;
		case GTK_JUSTIFY_RIGHT:
			return initial_offset + clist->column[column].area.width - content_width;
		case GTK_JUSTIFY_CENTER:
		case GTK_JUSTIFY_FILL:
		default:
			return initial_offset + (clist->column[column].area.width - content_width)/2;
	}
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
      GdkGCValues saved_values;

      GList *p;

      gint width;
      gint height;
      gint pixmap_width;
      gint offset = 0;
      gint baseline;
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
      width = 0;
      pixmap_width = 0;
      offset = 0;
      switch ((NautilusCellType)clist_row->cell[i].type)
	{
	case NAUTILUS_CELL_TEXT:
	case NAUTILUS_CELL_LINK_TEXT:
	  width = gdk_string_width (style->font,
				    GTK_CELL_TEXT (clist_row->cell[i])->text);
	  break;
	case NAUTILUS_CELL_PIXMAP:
	  gdk_window_get_size (GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap,
			       &pixmap_width, &height);
	  width = pixmap_width;
	  break;
	case NAUTILUS_CELL_PIXTEXT:
	  gdk_window_get_size (GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
			       &pixmap_width, &height);
	  width = (pixmap_width +
		   GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing +
		   gdk_string_width (style->font,
				     GTK_CELL_PIXTEXT
				     (clist_row->cell[i])->text));
	  break;
	case NAUTILUS_CELL_PIXBUF_LIST:
	  for (p = NAUTILUS_CELL_PIXBUF_LIST (clist_row->cell[i])->pixbufs; p != NULL; p = p->next) {
	  	if (width != 0) {
			width += PIXBUF_LIST_SPACING;
	  	}
	  	width += gdk_pixbuf_get_width (p->data);
	  }
	  break;
	default:
	  continue;
	  break;
	}

      offset = get_cell_horizontal_start_position (clist, clist_row, i, width);

      /* Draw Text and/or Pixmap */
      switch ((NautilusCellType)clist_row->cell[i].type)
	{
	case NAUTILUS_CELL_PIXMAP:
	  draw_cell_pixmap (clist->clist_window, &clip_rectangle, fg_gc,
			    GTK_CELL_PIXMAP (clist_row->cell[i])->pixmap,
			    GTK_CELL_PIXMAP (clist_row->cell[i])->mask,
			    offset,
			    clip_rectangle.y + clist_row->cell[i].vertical +
			    (clip_rectangle.height - height) / 2);
	  break;
	case NAUTILUS_CELL_PIXTEXT:
	  offset =
	    draw_cell_pixmap (clist->clist_window, &clip_rectangle, fg_gc,
			      GTK_CELL_PIXTEXT (clist_row->cell[i])->pixmap,
			      GTK_CELL_PIXTEXT (clist_row->cell[i])->mask,
			      offset,
			      clip_rectangle.y + clist_row->cell[i].vertical+
			      (clip_rectangle.height - height) / 2);
	  offset += GTK_CELL_PIXTEXT (clist_row->cell[i])->spacing;
	case NAUTILUS_CELL_TEXT:
	case NAUTILUS_CELL_LINK_TEXT:
	  if (style != GTK_WIDGET (clist)->style)
	    row_center_offset = (((clist->row_height - style->font->ascent -
				  style->font->descent - 1) / 2) + 1.5 +
				 style->font->ascent);
	  else
	    row_center_offset = clist->row_center_offset;

	  baseline = row_rectangle.y + row_center_offset + clist_row->cell[i].vertical;

	  gdk_gc_set_clip_rectangle (fg_gc, &clip_rectangle);

	  /* For link text cells, draw with blue link-like color and use underline. */
	  if ((NautilusCellType)clist_row->cell[i].type == NAUTILUS_CELL_LINK_TEXT
	      && NAUTILUS_LIST (clist)->details->single_click_mode) {
		if (state == GTK_STATE_NORMAL) {
			gdk_gc_get_values (fg_gc, &saved_values);
			gdk_rgb_gc_set_foreground (fg_gc, NAUTILUS_RGB_COLOR_BLUE);			
		}
	  }
	  gdk_draw_string (clist->clist_window, style->font, fg_gc,
			   offset,
			   baseline,
			   ((NautilusCellType)clist_row->cell[i].type == GTK_CELL_PIXTEXT) ?
			   GTK_CELL_PIXTEXT (clist_row->cell[i])->text :
			   GTK_CELL_TEXT (clist_row->cell[i])->text);

	  if ((NautilusCellType)clist_row->cell[i].type == NAUTILUS_CELL_LINK_TEXT
	      && NAUTILUS_LIST (clist)->details->single_click_mode) {
	        gdk_draw_line (clist->clist_window, fg_gc,
	        	       offset, baseline + 1,
	        	       offset + width, baseline + 1); 
		/* Revert color change we made a moment ago. */
		if (state == GTK_STATE_NORMAL) {
			gdk_gc_set_foreground (fg_gc, &saved_values.foreground);
		}
	  }
	  gdk_gc_set_clip_rectangle (fg_gc, NULL);
	  break;
	case NAUTILUS_CELL_PIXBUF_LIST:
	  {
		  GdkPixmap *gdk_pixmap;
		  GdkBitmap *mask;
		  guint	     pixbuf_width;
		  guint	     ellipsis_width;

		  ellipsis_width = gdk_string_width (style->font, "...");
		  
		  for (p = NAUTILUS_CELL_PIXBUF_LIST (clist_row->cell[i])->pixbufs; p != NULL; p = p->next) {
			  gdk_pixbuf_render_pixmap_and_mask (p->data, &gdk_pixmap, &mask, 128);
			  pixbuf_width = gdk_pixbuf_get_width (p->data);

			  if ((p->next != NULL && pixbuf_width + ellipsis_width >= 
			  			 clip_rectangle.x + clip_rectangle.width - offset) ||
			      (pixbuf_width >= clip_rectangle.x + clip_rectangle.width - offset)) {
				/* Not enough room for this icon & ellipsis, just draw ellipsis. */
				
				gdk_draw_string (clist->clist_window, style->font, fg_gc,
					 offset,
					 clip_rectangle.y + clip_rectangle.height/2,
					 "...");

				break;
			  }

			  height = gdk_pixbuf_get_height (p->data);

		  	  offset = draw_cell_pixbuf (clist->clist_window,
			  			     &clip_rectangle, fg_gc,
			  			     p->data,
					    	     offset,
					    	     clip_rectangle.y + clist_row->cell[i].vertical +
					    	     (clip_rectangle.height - height) / 2);

			   offset += PIXBUF_LIST_SPACING;
		  }
	  break;
	  }
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
nautilus_list_resize_column (GtkCList *clist, int column, int width)
{
	/* override resize column to invalidate the title */
	NautilusList *list;

	g_assert (NAUTILUS_IS_LIST (clist));

	list = NAUTILUS_LIST (clist);

	gtk_widget_queue_draw (list->details->title);
		
	NAUTILUS_CALL_PARENT_CLASS (GTK_CLIST_CLASS, resize_column, (clist, column, width));
}

/* Macros borrowed from gtkclist.c */
/* returns the GList item for the nth row */
#define	ROW_ELEMENT(clist, row)	(((row) == (clist)->rows - 1) ? \
				 (clist)->row_list_end : \
				 g_list_nth ((clist)->row_list, (row)))


#define GTK_CLIST_CLASS_FW(_widget_) GTK_CLIST_CLASS (((GtkObject*) (_widget_))->klass)

/* redraw the list if it's not frozen */
#define CLIST_UNFROZEN(clist)     (((GtkCList*) (clist))->freeze_count == 0)

/**
 * nautilus_list_mark_cell_as_link:
 * 
 * Mark a text cell as a link cell. Link cells are drawn differently,
 * and activate rather than select on single-click. The cell must
 * be a text cell (not a pixmap cell or one of the other types).
 * 
 * @list: The NautilusList in question.
 * @column: The column of the desired cell.
 * @row: The row of the desired cell.
 */
void
nautilus_list_mark_cell_as_link (NautilusList *list,
				 gint row,
				 gint column)
{
	GtkCListRow *clist_row;
	GtkCList *clist;

	g_return_if_fail (NAUTILUS_IS_LIST (list));

	clist = GTK_CLIST (list);

	g_return_if_fail (row >= 0 && row < clist->rows);
	g_return_if_fail (column >= 0 && column < clist->columns);
	
	clist_row = ROW_ELEMENT (clist, row)->data;

	/* 
	 * We only support changing text cells to links. Maybe someday
	 * we'll support pixmap or pixtext link cells too. 
	 */
	g_return_if_fail ((NautilusCellType)clist_row->cell[column].type == NAUTILUS_CELL_TEXT);

	clist_row->cell[column].type = NAUTILUS_CELL_LINK_TEXT;
}				


static void
nautilus_list_set_cell_contents (GtkCList    *clist,
		   		 GtkCListRow *clist_row,
		   		 gint         column,
		   		 GtkCellType  type,
		   		 const gchar *text,
		   		 guint8       spacing,
		   		 GdkPixmap   *pixmap,
		   		 GdkBitmap   *mask)
{
	/* 
	 * Note that we don't do the auto_resize bracketing here that's done
	 * in the parent class. It would require copying over huge additional
	 * chunks of code. We might decide we need that someday, but the
	 * chances seem larger that we'll switch away from CList first.
	 */

	if ((NautilusCellType)clist_row->cell[column].type == NAUTILUS_CELL_PIXBUF_LIST) {
		/* Clean up old data, which parent class doesn't know about. */
		nautilus_gdk_pixbuf_list_free (NAUTILUS_CELL_PIXBUF_LIST (clist_row->cell[column])->pixbufs);
	}

	/* If old cell was a link-text cell, convert it back to a normal text
	 * cell so it gets cleaned up properly by GtkCList code.
	 */
	if ((NautilusCellType)clist_row->cell[column].type == NAUTILUS_CELL_LINK_TEXT) {
		clist_row->cell[column].type = NAUTILUS_CELL_TEXT;
	}

	NAUTILUS_CALL_PARENT_CLASS (GTK_CLIST_CLASS, set_cell_contents, (clist, clist_row, column, type, text, spacing, pixmap, mask));


	if ((NautilusCellType)type == NAUTILUS_CELL_PIXBUF_LIST) {
		clist_row->cell[column].type = NAUTILUS_CELL_PIXBUF_LIST;
		/* Hideously, we concealed our list of pixbufs in the pixmap parameter. */
	  	NAUTILUS_CELL_PIXBUF_LIST (clist_row->cell[column])->pixbufs = (GList *)pixmap;
	}
}

/**
 * nautilus_list_set_pixbuf_list:
 * 
 * Set the contents of a cell to a list of similarly-sized GdkPixbufs.
 * 
 * @list: The NautilusList in question.
 * @row: The row of the target cell.
 * @column: The column of the target cell.
 * @pixbufs: A GList of GdkPixbufs.
 */
void 	   
nautilus_list_set_pixbuf_list (NautilusList *list,
			       gint row,
			       gint column,
			       GList *pixbufs)
{
  GtkCList    *clist;
  GtkCListRow *clist_row;

  g_return_if_fail (NAUTILUS_IS_LIST (list));

  clist = GTK_CLIST (list);

  if (row < 0 || row >= clist->rows)
    return;
  if (column < 0 || column >= clist->columns)
    return;

  clist_row = ROW_ELEMENT (clist, row)->data;

  /*
   * We have to go through the set_cell_contents bottleneck, which only
   * allows expected parameter types. Since our pixbuf_list is not an
   * expected parameter type, we have to sneak it in by casting it into
   * one of the expected parameters.
   */
  GTK_CLIST_CLASS_FW (clist)->set_cell_contents
    (clist, clist_row, column, (GtkCellType)NAUTILUS_CELL_PIXBUF_LIST, NULL, 0, (GdkPixmap *)pixbufs, NULL);

  /* redraw the list if it's not frozen */
  if (CLIST_UNFROZEN (clist))
    {
      if (gtk_clist_row_is_visible (clist, row) != GTK_VISIBILITY_NONE)
	GTK_CLIST_CLASS_FW (clist)->draw_row (clist, NULL, row, clist_row);
    }
}

static void
nautilus_list_track_new_column_width (GtkCList *clist, int column, int new_width)
{
	NautilusList *list;

	list = NAUTILUS_LIST (clist);

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
}

/* Our handler for motion_notify events.  We override all of GtkCList's broken
 * behavior.
 */
static gint
nautilus_list_motion (GtkWidget *widget, GdkEventMotion *event)
{
	NautilusList *list;
	GtkCList *clist;

	g_return_val_if_fail (NAUTILUS_IS_LIST (widget), FALSE);
	g_return_val_if_fail (event != NULL, FALSE);

	list = NAUTILUS_LIST (widget);
	clist = GTK_CLIST (widget);

	if (event->window != clist->clist_window)
		return NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, motion_notify_event, (widget, event));

	if (!((list->details->dnd_press_button == 1 && (event->state & GDK_BUTTON1_MASK))
	      || (list->details->dnd_press_button == 2 && (event->state & GDK_BUTTON2_MASK))))
		return FALSE;

	/* This is the same threshold value that is used in gtkdnd.c */

	if (MAX (abs (list->details->dnd_press_x - event->x),
		 abs (list->details->dnd_press_y - event->y)) <= 3)
		return FALSE;

	/* Handle any pending selections */

	if (list->details->dnd_select_pending) {
		select_row_from_mouse (list,
			    	       list->details->button_down_row,
			    	       list->details->dnd_select_pending_state);

		list->details->dnd_select_pending = FALSE;
		list->details->dnd_select_pending_state = 0;
	}

	gtk_signal_emit (GTK_OBJECT (list),
			 list_signals[START_DRAG],
			 list->details->dnd_press_button,
			 event);
	return TRUE;
}

void 
nautilus_list_column_resize_track_start (GtkWidget *widget, int column)
{
	GtkCList *clist;

	g_return_if_fail (NAUTILUS_IS_LIST (widget));

	clist = GTK_CLIST (widget);
	clist->drag_pos = column;
}

void 
nautilus_list_column_resize_track (GtkWidget *widget, int column)
{
	GtkCList *clist;
	int x;

	g_return_if_fail (NAUTILUS_IS_LIST (widget));

	clist = GTK_CLIST (widget);

	gtk_widget_get_pointer (widget, &x, NULL);
	nautilus_list_track_new_column_width (clist, column, 
					  new_column_width (clist, column, &x));

}

void 
nautilus_list_column_resize_track_end (GtkWidget *widget, int column)
{
	GtkCList *clist;

	g_return_if_fail (NAUTILUS_IS_LIST (widget));

	clist = GTK_CLIST (widget);
	clist->drag_pos = -1;
}

/* We override the drag_begin signal to do nothing */
static void
nautilus_list_drag_begin (GtkWidget *widget, GdkDragContext *context)
{
	/* nothing */
}

/* We override the drag_end signal to do nothing */
static void
nautilus_list_drag_end (GtkWidget *widget, GdkDragContext *context)
{
	/* nothing */
}

/* We override the drag_data_get signal to do nothing */
static void
nautilus_list_drag_data_get (GtkWidget *widget, GdkDragContext *context,
			 GtkSelectionData *data, guint info, guint time)
{
	/* nothing */
}

/* We override the drag_leave signal to do nothing */
static void
nautilus_list_drag_leave (GtkWidget *widget, GdkDragContext *context, guint time)
{
	/* nothing */
}

/* We override the drag_motion signal to do nothing */
static gboolean
nautilus_list_drag_motion (GtkWidget *widget, GdkDragContext *context,
		       gint x, gint y, guint time)
{
	return FALSE;
}

/* We override the drag_drop signal to do nothing */
static gboolean
nautilus_list_drag_drop (GtkWidget *widget, GdkDragContext *context,
		     gint x, gint y, guint time)
{
	return FALSE;
}

/* We override the drag_data_received signal to accept colors. */
static void
nautilus_list_drag_data_received (GtkWidget *widget, GdkDragContext *context,
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
nautilus_list_clear (GtkCList *clist)
{
	NautilusList *list;

	g_return_if_fail (NAUTILUS_IS_LIST (clist));

	list = NAUTILUS_LIST (clist);
	list->details->anchor_row = -1;

	NAUTILUS_CALL_PARENT_CLASS (GTK_CLIST_CLASS, clear, (clist));
}

static void
click_policy_changed_callback (gpointer user_data)
{
	NautilusList *list;

	g_assert (NAUTILUS_IS_LIST (user_data));
	
	list = NAUTILUS_LIST (user_data);

	list->details->single_click_mode = 
		(nautilus_preferences_get_enum (NAUTILUS_PREFERENCES_CLICK_POLICY,
						NAUTILUS_CLICK_POLICY_SINGLE) == NAUTILUS_CLICK_POLICY_SINGLE);
}


/**
 * nautilus_list_new_with_titles:
 * @columns: The number of columns in the list
 * @titles: The titles for the columns
 * 
 * Return value: The newly-created file list.
 **/
GtkWidget *
nautilus_list_new_with_titles (int columns, const char * const *titles)
{
	NautilusList *list;

	list = gtk_type_new (nautilus_list_get_type ());
	gtk_clist_construct (GTK_CLIST (list), columns, NULL);
	if (titles) {
		GtkCList *clist;
		int index;

		clist = GTK_CLIST(list);
		
		for (index = 0; index < columns; index++) {
  			clist->column[index].title = g_strdup (titles[index]);
  		}
    	}

	gtk_clist_set_selection_mode (GTK_CLIST (list),
				      GTK_SELECTION_MULTIPLE);

	return GTK_WIDGET (list);
}

static int
nautilus_list_get_first_selected_row (NautilusList *list)
{
	GtkCListRow *row;
	GList *p;
	int row_number;

	g_return_val_if_fail (NAUTILUS_IS_LIST (list), -1);

	row_number = 0;
	for (p = GTK_CLIST (list)->row_list; p != NULL; p = p->next) {
		row = p->data;
		if (row->state == GTK_STATE_SELECTED) {
			return row_number;	
		}

		++row_number;
	}

	return -1;
}

static int
nautilus_list_get_last_selected_row (NautilusList *list)
{
	GtkCListRow *row;
	GList *p;
	int row_number;

	g_return_val_if_fail (NAUTILUS_IS_LIST (list), -1);

	row_number = GTK_CLIST (list)->rows - 1;
	for (p = GTK_CLIST (list)->row_list_end; p != NULL; p = p->prev) {
		row = p->data;
		if (row->state == GTK_STATE_SELECTED) {
			return row_number;	
		}

		--row_number;
	}

	return -1;
}

GList *
nautilus_list_get_selection (NautilusList *list)
{
	GList *retval;
	GList *p;

	g_return_val_if_fail (NAUTILUS_IS_LIST (list), NULL);

	retval = NULL;
	for (p = GTK_CLIST (list)->row_list; p != NULL; p = p->next) {
		GtkCListRow *row;

		row = p->data;
		if (row->state == GTK_STATE_SELECTED)
			retval = g_list_prepend (retval, row->data);
	}

	return retval;
}

void
nautilus_list_set_selection (NautilusList *list, GList *selection)
{
	GList *p;
	gboolean selection_changed;
	gboolean select_this;
	int i;

	g_return_if_fail (NAUTILUS_IS_LIST (list));

	/* FIXME bugzilla.eazel.com 613: 
	   Selecting n items in an m-element container is an
	   O(m*n) task using this algorithm, making it quadratic if
	   you select them all with this method, which actually
	   happens if you select all in list view and switch to icon
	   view. We should build a hash table from the list first;
	   then we can get O(m+n) performance. */

	selection_changed = FALSE;

	for (p = GTK_CLIST (list)->row_list, i = 0; p != NULL; p = p->next, i++) {
		GtkCListRow *row;
		gpointer row_data;
		
		row = p->data;
		row_data = row->data;

		select_this = (NULL != g_list_find (selection, row_data));

		selection_changed |= row_set_selected (list, i, row_data, select_this);
	}

	if (selection_changed) {
		emit_selection_changed (list);
	}
}

