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

#include <string.h>
#include "nautilus-global-preferences.h"
#include "nautilus-undo-signal-handlers.h"
#include <eel/eel-gdk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-i18n.h>

struct NautilusEntryDetails {
	gboolean use_emacs_shortcuts;
	gboolean user_edit;
	gboolean special_tab_handling;
	gboolean cursor_obscured;
};

enum {
	USER_CHANGED,
	SELECTION_CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

static void nautilus_entry_init       (NautilusEntry      *entry);
static void nautilus_entry_class_init (NautilusEntryClass *class);

EEL_CLASS_BOILERPLATE (NautilusEntry,
		       nautilus_entry,
		       GTK_TYPE_ENTRY)

static void
emacs_shortcuts_preference_changed_callback (gpointer callback_data)
{
	NautilusEntry *entry;

	entry = NAUTILUS_ENTRY (callback_data);

	entry->details->use_emacs_shortcuts =
		eel_preferences_get_boolean (NAUTILUS_PREFERENCES_USE_EMACS_SHORTCUTS);
}

static void
nautilus_entry_init (NautilusEntry *entry)
{
	GtkWidget *widget;

	widget = GTK_WIDGET (entry);
	entry->details = g_new0 (NautilusEntryDetails, 1);
	
	entry->details->user_edit = TRUE;

	/* Allow pointer motion events so we can expose an obscured cursor if necessary */
	gtk_widget_set_events (widget, gtk_widget_get_events (widget) | GDK_POINTER_MOTION_MASK);

	nautilus_undo_set_up_nautilus_entry_for_undo (entry);

	eel_preferences_add_callback (NAUTILUS_PREFERENCES_USE_EMACS_SHORTCUTS,
				      emacs_shortcuts_preference_changed_callback,
				      entry);
	emacs_shortcuts_preference_changed_callback (entry);
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

	eel_preferences_remove_callback (NAUTILUS_PREFERENCES_USE_EMACS_SHORTCUTS,
					 emacs_shortcuts_preference_changed_callback,
					 entry);
	
	g_free (entry->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
obscure_cursor (NautilusEntry *entry)
{
	if (entry->details->cursor_obscured) {
		return;
	}
	
	entry->details->cursor_obscured = TRUE;	
	eel_gdk_window_set_invisible_cursor (GTK_ENTRY (entry)->text_area);
}

static gboolean
nautilus_entry_key_press (GtkWidget *widget, GdkEventKey *event)
{
	NautilusEntry *entry;
	GtkEditable *editable;
	int position;
	gboolean old_has, new_has;
	gboolean result;
	
	entry = NAUTILUS_ENTRY (widget);
	editable = GTK_EDITABLE (widget);
	
	if (!gtk_editable_get_editable (editable)) {
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
		if (entry->details->special_tab_handling && gtk_editable_get_selection_bounds (editable, NULL, NULL)) {
			position = strlen (gtk_entry_get_text (GTK_ENTRY (editable)));
			gtk_entry_select_region (GTK_ENTRY (editable), position, position);
			return TRUE;
		}
		break;
	
	case GDK_KP_Enter:
		/* Work around bug in GtkEntry where keypad Enter key
		 * inserts a character rather than activating like the
		 * other Enter key.
		 */
		gtk_widget_activate (widget);
		return TRUE;		
		
	default:
		break;
	}

	if (!entry->details->use_emacs_shortcuts) {
		/* Filter out the emacs-style keyboard shortcuts in
		 * GtkEntry for alt and control keys. They have
		 * numerous conflicts with menu keyboard shortcuts.
		 */
		if (event->state & GDK_CONTROL_MASK || event->state & GDK_MOD1_MASK) {
			return FALSE;
		}
	}
	
	obscure_cursor (entry);

	old_has = gtk_editable_get_selection_bounds (editable, NULL, NULL);

	result = EEL_CALL_PARENT_WITH_RETURN_VALUE
		(GTK_WIDGET_CLASS, key_press_event, (widget, event));

	/* Pressing a key usually changes the selection if there is a selection.
	 * If there is not selection, we can save work by not emitting a signal.
	 */
	if (result) {
		new_has = gtk_editable_get_selection_bounds (editable, NULL, NULL);
		if (old_has || new_has) {
			g_signal_emit (widget, signals[SELECTION_CHANGED], 0);
		}
	}

	return result;
	
}

static gboolean
nautilus_entry_motion_notify (GtkWidget *widget, GdkEventMotion *event)
{
	int result;
	gboolean old_had, new_had;
	int old_start, old_end, new_start, new_end;
	GdkCursor *cursor;
	NautilusEntry *entry;
	GtkEditable *editable;

	entry = NAUTILUS_ENTRY (widget);
	editable = GTK_EDITABLE (widget);

	/* Reset cursor to I-Beam */
	if (entry->details->cursor_obscured) {
		cursor = gdk_cursor_new (GDK_XTERM);
		gdk_window_set_cursor (GTK_ENTRY (entry)->text_area, cursor);
		gdk_cursor_destroy (cursor);
		entry->details->cursor_obscured = FALSE;
	}

	old_had = gtk_editable_get_selection_bounds (editable, &old_start, &old_end);

	result = EEL_CALL_PARENT_WITH_RETURN_VALUE
		(GTK_WIDGET_CLASS, motion_notify_event, (widget, event));

	/* Send a signal if dragging the mouse caused the selection to change. */
	if (result) {
		new_had = gtk_editable_get_selection_bounds (editable, &new_start, &new_end);
		if (old_had != new_had || (old_had && (old_start != new_start || old_end != new_end))) {
			g_signal_emit (widget, signals[SELECTION_CHANGED], 0);
		}
	}
	
	return result;
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
select_all_at_idle (gpointer callback_data)
{
	nautilus_entry_select_all (NAUTILUS_ENTRY (callback_data));
	g_object_unref (callback_data);

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
	GSource *source;
	
	g_return_if_fail (NAUTILUS_IS_ENTRY (entry));

	/* If the text cursor position changes in this routine
	 * then gtk_entry_key_press will unselect (and we want
	 * to move the text cursor position to the end).
	 */

	source = g_idle_source_new ();
	g_source_set_callback (source, select_all_at_idle, entry, NULL);
	g_signal_connect_swapped (entry, "destroy",
				  G_CALLBACK (g_source_destroy), source);
	g_source_attach (source, NULL);
	g_source_unref (source);
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

	entry->details->user_edit = FALSE;
	gtk_entry_set_text (GTK_ENTRY (entry), text);
	entry->details->user_edit = TRUE;
	
	g_signal_emit (entry, signals[SELECTION_CHANGED], 0);
}

#if GNOME2_CONVERSION_COMPLETE

static void
nautilus_entry_set_selection_bounds (GtkEditable *editable,
				     int start_pos,
				     int end_pos)
{
	EEL_CALL_PARENT (GTK_EDITABLE_CLASS, set_selection_bounds,
			 (editable, start_pos, end_pos));

	g_signal_emit (editable, signals[SELECTION_CHANGED], 0);
}

#endif

static gboolean
nautilus_entry_button_press (GtkWidget *widget,
			     GdkEventButton *event)
{
	gboolean result;

	result = EEL_CALL_PARENT_WITH_RETURN_VALUE
		(GTK_WIDGET_CLASS, button_press_event, (widget, event));

	if (result) {
		g_signal_emit (widget, signals[SELECTION_CHANGED], 0);
	}

	return result;
}

static gboolean
nautilus_entry_button_release (GtkWidget *widget,
			       GdkEventButton *event)
{
	gboolean result;

	result = EEL_CALL_PARENT_WITH_RETURN_VALUE
		(GTK_WIDGET_CLASS, button_release_event, (widget, event));

	if (result) {
		g_signal_emit (widget, signals[SELECTION_CHANGED], 0);
	}

	return result;
}

#if GNOME2_CONVERSION_COMPLETE

static void
nautilus_entry_insert_text (GtkEditable *editable, const gchar *text,
			    int length, int *position)
{
	NautilusEntry *entry;

	entry = NAUTILUS_ENTRY(editable);

	/* Fire off user changed signals */
	if (entry->details->user_edit) {
		g_signal_emit (editable, signals[USER_CHANGED], 0);
	}

	EEL_CALL_PARENT (GTK_EDITABLE_CLASS, insert_text, 
			      (editable, text, length, position));

	g_signal_emit (editable, signals[SELECTION_CHANGED], 0);
}
			 		     
static void 
nautilus_entry_delete_text (GtkEditable *editable, int start_pos, int end_pos)
{
	NautilusEntry *entry;
	
	entry = NAUTILUS_ENTRY (editable);

	/* Fire off user changed signals */
	if (entry->details->user_edit) {
		g_signal_emit (editable, signals[USER_CHANGED], 0);
	}
	
	EEL_CALL_PARENT (GTK_EDITABLE_CLASS, delete_text, 
			      (editable, start_pos, end_pos));

	g_signal_emit (editable, signals[SELECTION_CHANGED], 0);
}

#endif

/* Overridden to work around GTK bug. The selection_clear_event is queued
 * when the selection changes. Changing the selection to NULL and then
 * back to the original selection owner still sends the event, so the
 * selection owner then gets the selection ripped away from it. We ran into
 * this with type-completion behavior in NautilusLocationBar (see bug 5313).
 * There's a FIXME comment that seems to be about this same issue in
 * gtk+/gtkselection.c, gtk_selection_clear.
 */
static gboolean
nautilus_entry_selection_clear (GtkWidget *widget,
			        GdkEventSelection *event)
{
	g_return_val_if_fail (NAUTILUS_IS_ENTRY (widget), FALSE);
	
	if (gdk_selection_owner_get (event->selection) == widget->window) {
		return FALSE;
	}
	
	return EEL_CALL_PARENT_WITH_RETURN_VALUE
		(GTK_WIDGET_CLASS, selection_clear_event, (widget, event));
}

static void
nautilus_entry_class_init (NautilusEntryClass *class)
{
	GtkWidgetClass *widget_class;
	GtkObjectClass *object_class;
#if GNOME2_CONVERSION_COMPLETE
	GtkEditableClass *editable_class;
#endif
		
	widget_class = GTK_WIDGET_CLASS (class);
	object_class = GTK_OBJECT_CLASS (class);
#if GNOME2_CONVERSION_COMPLETE
	editable_class = GTK_EDITABLE_CLASS (class);
#endif
		
	widget_class->button_press_event = nautilus_entry_button_press;
	widget_class->button_release_event = nautilus_entry_button_release;
	widget_class->key_press_event = nautilus_entry_key_press;
	widget_class->motion_notify_event = nautilus_entry_motion_notify;
	widget_class->selection_clear_event = nautilus_entry_selection_clear;
	
	object_class->destroy = nautilus_entry_destroy;
	
#if GNOME2_CONVERSION_COMPLETE
	editable_class->insert_text = nautilus_entry_insert_text;
	editable_class->delete_text = nautilus_entry_delete_text;
	editable_class->set_selection_bounds = nautilus_entry_set_selection_bounds;
#endif

	/* Set up signals */
	signals[USER_CHANGED] = g_signal_new
		("user_changed",
		 G_TYPE_FROM_CLASS (object_class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusEntryClass,
				    user_changed),
		 NULL, NULL,
		 gtk_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
	signals[SELECTION_CHANGED] = g_signal_new
		("selection_changed",
		 G_TYPE_FROM_CLASS (object_class),
		 G_SIGNAL_RUN_LAST,
		 G_STRUCT_OFFSET (NautilusEntryClass,
				    selection_changed),
		 NULL, NULL,
		 gtk_marshal_VOID__VOID,
		 G_TYPE_NONE, 0);
}

void
nautilus_entry_set_special_tab_handling (NautilusEntry *entry,
					 gboolean special_tab_handling)
{
	g_return_if_fail (NAUTILUS_IS_ENTRY (entry));

	entry->details->special_tab_handling = special_tab_handling;
}
