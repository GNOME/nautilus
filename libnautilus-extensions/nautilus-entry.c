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
#include <libnautilus/nautilus-undo-manager.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>

#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>

static void nautilus_entry_initialize 	    (NautilusEntry 	*entry);
static void nautilus_entry_initialize_class (NautilusEntryClass *class);
static void nautilus_entry_destroy	    (GtkObject 		*object);	
static void nautilus_entry_changed 	    (GtkEditable 	*entry);
static gint nautilus_entry_key_press 	    (GtkWidget   	*widget,
		     		      	     GdkEventKey 	*event);

/* Undo callbacks */
static void save_undo_snapshot_callback (NautilusUndoable *object);
static void restore_from_undo_snapshot_callback (NautilusUndoable *object);


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

	object_class->destroy = nautilus_entry_destroy;
	editable_class->changed = nautilus_entry_changed;
}

static void
nautilus_entry_initialize (NautilusEntry *entry)
{
	entry->undo_text = NULL;
	entry->undo_registered = TRUE;
	entry->use_undo = FALSE;
	entry->handle_undo_key = FALSE;
}

GtkWidget*
nautilus_entry_new (void)
{
  return GTK_WIDGET (gtk_type_new (NAUTILUS_TYPE_ENTRY));
}

static void 
nautilus_entry_destroy (GtkObject *object)
{
	NautilusEntry *entry;

	entry = NAUTILUS_ENTRY (object);
	if (entry->undo_text != NULL) {
		g_free (entry->undo_text);
	}

	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}

static gint 
nautilus_entry_key_press (GtkWidget *widget, GdkEventKey *event)
{
	NautilusEntry *entry;
	
	g_assert (NAUTILUS_IS_ENTRY (widget));
	
	if (!GTK_EDITABLE (widget)->editable) {
		return FALSE;
	}

	entry = NAUTILUS_ENTRY(widget);

	/* Fix bug in GtkEntry where keypad Enter key inserts a
	 * character rather than activating like the other Enter key.
	 */
	switch (event->keyval) {
		case GDK_KP_Enter:
			gtk_widget_activate (widget);
			return TRUE;

		/* Undo */
		case 'z':
			if (event->state & GDK_CONTROL_MASK && entry->use_undo == TRUE
			    && entry->handle_undo_key == TRUE) {
				nautilus_undo_manager_undo_last_transaction();
				return FALSE;
			}
			break;
			
		default:
			break;
	}

	return NAUTILUS_CALL_PARENT_CLASS (GTK_WIDGET_CLASS, key_press_event, (widget, event));
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
 * nautilus_entry_changed
 *
 * @entry: A NautilusEntry
 **/

static void
nautilus_entry_changed (GtkEditable *editable)
{
	NautilusEntry *entry;
	
	g_assert (GTK_IS_EDITABLE (editable));
	g_assert (NAUTILUS_IS_ENTRY (editable));

	entry = NAUTILUS_ENTRY(editable);

	/* Register undo transaction */	
	if (!entry->undo_registered && entry->use_undo) {
		nautilus_undo_manager_begin_transaction (_("Edit"));
		nautilus_undoable_save_undo_snapshot (GTK_OBJECT(entry), save_undo_snapshot_callback,
					      restore_from_undo_snapshot_callback);
		nautilus_undo_manager_end_transaction ();

		entry->undo_registered = TRUE;
	}
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_EDITABLE_CLASS, changed, (editable));	
}

/* save_undo_snapshot_callback
 * 
 * Get text at start of edit operation and store in undo data as 
 * string with a key of "undo_text".
 */
static void
save_undo_snapshot_callback (NautilusUndoable *undoable)
{
	NautilusEntry *target;
		
	target = NAUTILUS_ENTRY (undoable->undo_target_class);

	if (!target->use_undo)
		return;
	
	/* Add our undo data to the data list */
	g_datalist_set_data(&undoable->undo_data, "undo_text", g_strdup(target->undo_text));
}


/* restore_from_undo_snapshot_callback
 * 
 * Restore edited text to data stored in undoable.  Data is stored as 
 * a string with a key of "undo_text".
 */
static void
restore_from_undo_snapshot_callback (NautilusUndoable *undoable)
{		
	char *undo_text;
	NautilusEntry *entry;

	entry = NAUTILUS_ENTRY (undoable->undo_target_class);

	if (!entry->use_undo)
		return;

	/* Get copy of entry text */
	entry->undo_registered = FALSE;
	if (entry->undo_text != NULL) {
		g_free (entry->undo_text);
	}
	entry->undo_text = g_strdup (gtk_entry_get_text (GTK_ENTRY(entry)));
	
	
	undo_text = g_datalist_get_data (&undoable->undo_data, "undo_text");
	if (undo_text != NULL) {
		gtk_entry_set_text(GTK_ENTRY(entry), undo_text);
	}

	/* Get copy of entry text */
	entry->undo_registered = FALSE;
	if (entry->undo_text != NULL) {
		g_free (entry->undo_text);
	}
	entry->undo_text = g_strdup (gtk_entry_get_text (GTK_ENTRY(entry)));
}

/* nautilus_entry_enable_undo
 *
 * Enable undo mechanism in entry item. 
 */
void 
nautilus_entry_enable_undo (NautilusEntry *entry, gboolean value)
{
	g_assert (entry);
	g_assert (NAUTILUS_IS_ENTRY (entry));
	
	entry->undo_registered = !value;
	entry->use_undo = value;

	if (!entry->undo_registered) {
		/* Get copy of entry text */
		if (entry->undo_text != NULL) {
			g_free (entry->undo_text);
		}
		entry->undo_text = g_strdup (gtk_entry_get_text (GTK_ENTRY(entry)));
	}
}

/* nautilus_entry_enable_undo_key
 *
 * Allow the use od ctl-z from within widget.  This should only be 
 * set if there is no menu bar to use to undo the widget.
 */
void 
nautilus_entry_enable_undo_key (NautilusEntry *entry, gboolean value)
{
	entry->handle_undo_key = value;
}
