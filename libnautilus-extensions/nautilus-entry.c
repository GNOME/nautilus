/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* NautilusEntry: one-line text editing widget. This consists of bug fixes
 * and other improvements to GtkEntry, and all the changes could be rolled
 * into GtkEntry some day.
 *
 * Copyright (C) 2000 Eazel, Inc.
 *
 * Author: John Sullivan <sullivan@eazel.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include "nautilus-entry.h"

#include "nautilus-gtk-macros.h"

#include <gdk/gdkkeysyms.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-undo-signal-handlers.h>

#include <orb/orbit.h>


enum {
	USER_CHANGED,
	SELECTION_CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

static void nautilus_entry_initialize 	    (NautilusEntry 	*entry);
static void nautilus_entry_initialize_class (NautilusEntryClass *class);
static void nautilus_entry_destroy	    (GtkObject 		*object);
static gint nautilus_entry_key_press 	    (GtkWidget   	*widget,
		     		      	     GdkEventKey 	*event);
static gint nautilus_entry_motion_notify    (GtkWidget          *widget,
					     GdkEventMotion *event);
static void nautilus_entry_insert_text      (GtkEditable    	*editable,
			 		     const gchar    	*text,
			 		     gint            	length,
			 		     gint           	*position);
static gboolean nautilus_entry_selection_clear (GtkWidget        *widget,
					     GdkEventSelection *event);
static void nautilus_entry_delete_text      (GtkEditable    	*editable,
			 		     gint            	start_pos,
			 		     gint            	end_pos);
static void nautilus_entry_set_selection    (GtkEditable        *editable,
					     gint               start_pos,
					     gint               end_pos);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusEntry, nautilus_entry, GTK_TYPE_ENTRY)

static void
nautilus_entry_initialize_class (NautilusEntryClass *class)
{
	GtkWidgetClass *widget_class;
	GtkObjectClass *object_class;
	GtkEditableClass *editable_class;
		
	widget_class = GTK_WIDGET_CLASS (class);
	object_class = GTK_OBJECT_CLASS (class);
	editable_class = GTK_EDITABLE_CLASS (class);
		
	widget_class->key_press_event = nautilus_entry_key_press;
	widget_class->motion_notify_event = nautilus_entry_motion_notify;
	widget_class->selection_clear_event = nautilus_entry_selection_clear;
	
	object_class->destroy = nautilus_entry_destroy;
	
	editable_class->insert_text = nautilus_entry_insert_text;
	editable_class->delete_text = nautilus_entry_delete_text;
	editable_class->set_selection = nautilus_entry_set_selection;


	/* Set up signals */
	signals[USER_CHANGED] = gtk_signal_new
		("user_changed",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusEntryClass,
				    user_changed),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);
	signals[SELECTION_CHANGED] = gtk_signal_new
		("selection_changed",
		 GTK_RUN_LAST,
		 object_class->type,
		 GTK_SIGNAL_OFFSET (NautilusEntryClass,
				    selection_changed),
		 gtk_marshal_NONE__NONE,
		 GTK_TYPE_NONE, 0);
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
nautilus_entry_initialize (NautilusEntry *entry)
{
	GtkWidget *widget;

	widget = GTK_WIDGET (entry);
	
	entry->user_edit = TRUE;
	entry->special_tab_handling = FALSE;
	entry->cursor_obscured = FALSE;
	entry->expand_tilde = FALSE;

	/* Allow pointer motion events so we can expose an obscured cursor if necessary */
	gtk_widget_set_events (widget, gtk_widget_get_events (widget) | GDK_POINTER_MOTION_MASK);

	nautilus_undo_set_up_nautilus_entry_for_undo (entry);
}

GtkWidget *
nautilus_entry_new (void)
{
	return gtk_widget_new (NAUTILUS_TYPE_ENTRY, NULL);
}

GtkWidget *
nautilus_entry_new_with_max_length (guint16 max)
{
	GtkWidget *widget;

	widget = gtk_widget_new (NAUTILUS_TYPE_ENTRY, NULL);
	GTK_ENTRY (widget)->text_max_length = max;

	return widget;
}


static void 
nautilus_entry_destroy (GtkObject *object)
{
	NautilusEntry *entry;

	entry = NAUTILUS_ENTRY (object);

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static void
obscure_cursor (NautilusEntry *entry)
{
	if (entry->cursor_obscured) {
		return;
	}
	
	entry->cursor_obscured = TRUE;	
	nautilus_gdk_window_set_invisible_cursor (GTK_ENTRY (entry)->text_area);
}


static gint 
nautilus_entry_key_press (GtkWidget *widget, GdkEventKey *event)
{
	NautilusEntry *entry;
	GtkEditable *editable;
	int position;
	gint return_code;
	
	entry = NAUTILUS_ENTRY (widget);
	editable = GTK_EDITABLE (widget);
	
	if (!editable->editable) {
		return FALSE;
	}

	switch (event->keyval) {
	case GDK_Tab:
		/* The location bar entry wants TAB to work kind of
		 * like it does in the shell for command completion,
		 * so if we get a tab and there's a selection, we
		 * should position the insertion point at the end of
		 * the selection.
		 */
		if (entry->special_tab_handling
		    && editable->has_selection) {
			position = strlen (gtk_entry_get_text (GTK_ENTRY (editable)));
			gtk_entry_select_region (GTK_ENTRY (editable), position, position);
			return TRUE;
		}
		break;
	
	case GDK_KP_Enter:
		/* Fix bug in GtkEntry where keypad Enter key inserts
		 * a character rather than activating like the other
		 * Enter key.
		 */
		gtk_widget_activate (widget);
		return TRUE;		

	case GDK_slash:
		if (entry->expand_tilde) {
			/* FIXME: This handles only ~/, not the fancier variants. */
			if (strcmp (gtk_entry_get_text (GTK_ENTRY (entry)), "~") == 0) {
				gtk_entry_set_text (GTK_ENTRY (entry), g_get_home_dir ());
			}
		}
		break;
		
	default:
		break;
	}

	/* Filter out default GTKEntry alt and control key bindings. They have numerous conflicts
	 * with Nautilus menu keyboard accelerators.
	 */		
	if (event->state & GDK_CONTROL_MASK || event->state & GDK_MOD1_MASK) {
		return FALSE;
	}
	
	obscure_cursor (entry);

	return_code = NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS,
						  key_press_event,
						  (widget, event));
	gtk_signal_emit (GTK_OBJECT (widget), signals[SELECTION_CHANGED]);

	return return_code;
	
}

static gint
nautilus_entry_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
	gint return_code;
	guint old_start_pos, old_end_pos;
	GdkCursor *cursor;
	NautilusEntry *entry;

	/* Reset cursor to I-Beam */
	entry = NAUTILUS_ENTRY (widget);
	if (entry->cursor_obscured) {
		cursor = gdk_cursor_new (GDK_XTERM);
		gdk_window_set_cursor (GTK_ENTRY (entry)->text_area, cursor);
		gdk_cursor_destroy (cursor);
		entry->cursor_obscured = FALSE;
	}

	if (GTK_ENTRY (widget)->button == 0) {
    	return FALSE;
    }

	old_start_pos = GTK_EDITABLE (widget)->selection_start_pos;
	old_end_pos = GTK_EDITABLE (widget)->selection_end_pos;

	return_code = NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, motion_notify_event, (widget, event));
	if (GTK_EDITABLE (widget)->selection_start_pos != old_start_pos ||
	    GTK_EDITABLE (widget)->selection_end_pos != old_end_pos) {
		gtk_signal_emit (GTK_OBJECT (widget), signals[SELECTION_CHANGED]);
	}

	return return_code;
}

/**
 * nautilus_entry_select_all
 *
 * Select all text, leaving the text cursor position at the end.
 * 
 * @entry: A NautilusEntry
 **/
void
nautilus_entry_select_all (NautilusEntry *entry)
{
	g_return_if_fail (NAUTILUS_IS_ENTRY (entry));

	gtk_editable_set_position (GTK_EDITABLE (entry), -1);
	gtk_editable_select_region (GTK_EDITABLE (entry), 0, -1);
}

static gboolean
select_all_at_idle (NautilusEntry *entry)
{
	nautilus_entry_select_all (entry);
	return FALSE;
}

/**
 * nautilus_entry_select_all_at_idle
 *
 * Select all text at the next idle, not immediately.
 * This is useful when reacting to a key press, because
 * changing the selection and the text cursor position doesn't
 * work in a key_press signal handler.
 * 
 * @entry: A NautilusEntry
 **/
void
nautilus_entry_select_all_at_idle (NautilusEntry *entry)
{
	g_return_if_fail (NAUTILUS_IS_ENTRY (entry));

	/* If the text cursor position changes in this routine
	 * then gtk_entry_key_press will unselect (and we want
	 * to move the text cursor position to the end).
	 */
	gtk_idle_add ((GtkFunction)select_all_at_idle, entry);
}

/**
 * nautilus_entry_set_text
 *
 * This function wraps gtk_entry_set_text.  It sets undo_registered
 * to TRUE and preserves the old value for a later restore.  This is
 * done so the programmatic changes to the entry do not register
 * with the undo manager.
 *  
 * @entry: A NautilusEntry
 * @test: The text to set
 **/

void
nautilus_entry_set_text (NautilusEntry *entry, const gchar *text)
{
	g_return_if_fail (NAUTILUS_IS_ENTRY (entry));

	entry->user_edit = FALSE;
	gtk_entry_set_text (GTK_ENTRY (entry), text);
	entry->user_edit = TRUE;
	
	gtk_signal_emit (GTK_OBJECT (entry), signals[SELECTION_CHANGED]);
}

void 
nautilus_entry_set_selection (GtkEditable *editable,
			      gint start_pos,
			      gint end_pos)
{

	NAUTILUS_CALL_PARENT_CLASS (GTK_EDITABLE_CLASS, set_selection,
				    (editable, start_pos, end_pos));
	gtk_signal_emit (GTK_OBJECT (editable), signals[SELECTION_CHANGED]);
}

static void 
nautilus_entry_insert_text (GtkEditable *editable, const gchar *text,
			    gint length, gint *position)
{
	NautilusEntry *entry;

	entry = NAUTILUS_ENTRY(editable);

	/* Fire off user changed signals */
	if (entry->user_edit) {
		gtk_signal_emit (GTK_OBJECT (editable), signals[USER_CHANGED]);
	}

	NAUTILUS_CALL_PARENT_CLASS (GTK_EDITABLE_CLASS, insert_text, 
				   (editable, text, length, position));
	gtk_signal_emit (GTK_OBJECT (editable), signals[SELECTION_CHANGED]);

}

			 		     
static void 
nautilus_entry_delete_text (GtkEditable *editable, gint start_pos, gint end_pos)
{
	NautilusEntry *entry;
	

	entry = NAUTILUS_ENTRY(editable);

	/* Fire off user changed signals */
	if (entry->user_edit) {
		gtk_signal_emit (GTK_OBJECT (editable), signals[USER_CHANGED]);
	}
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_EDITABLE_CLASS, delete_text, 
				   (editable, start_pos, end_pos));

	gtk_signal_emit (GTK_OBJECT (editable), signals[SELECTION_CHANGED]);
}

/* Overridden to work around GTK bug. The selection_clear_event is queued
 * when the selection changes. Changing the selection to NULL and then
 * back to the original selection owner still sends the event, so the
 * selection owner then gets the selection ripped away from it. We ran into
 * this with type-completion behavior in NautilusLocationBar (see bug 5313).
 * There's a FIXME comment that seems to be about this same issue in
 * gtk+/gtkselection.c, gtk_selection_clear.
 */
static gboolean
nautilus_entry_selection_clear (GtkWidget         *widget,
			        GdkEventSelection *event)
{
  g_return_val_if_fail (NAUTILUS_IS_ENTRY (widget), FALSE);
  g_return_val_if_fail (event != NULL, FALSE);

  if (gdk_selection_owner_get (event->selection) == widget->window) {
	return FALSE;
  }

  return NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, 
  				     selection_clear_event, 
				     (widget, event));
}
