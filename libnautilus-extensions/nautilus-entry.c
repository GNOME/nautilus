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

#include <libnautilus-extensions/nautilus-undo-signal-handlers.h>

#include <orb/orbit.h>


enum {
	USER_CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

static void nautilus_entry_initialize 	    (NautilusEntry 	*entry);
static void nautilus_entry_initialize_class (NautilusEntryClass *class);
static void nautilus_entry_destroy	    (GtkObject 		*object);
static gint nautilus_entry_key_press 	    (GtkWidget   	*widget,
		     		      	     GdkEventKey 	*event);
static void nautilus_entry_insert_text      (GtkEditable    	*editable,
			 		     const gchar    	*text,
			 		     gint            	length,
			 		     gint           	*position);
static void nautilus_entry_delete_text      (GtkEditable    	*editable,
			 		     gint            	start_pos,
			 		     gint            	end_pos);

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
	
	editable_class->insert_text = nautilus_entry_insert_text;
	editable_class->delete_text = nautilus_entry_delete_text;

	/* Set up signals */
	signals[USER_CHANGED] = gtk_signal_new ("user_changed",
				GTK_RUN_FIRST,
				object_class->type,
				GTK_SIGNAL_OFFSET (NautilusEntryClass,
						   user_changed),
				gtk_marshal_NONE__NONE,
				GTK_TYPE_NONE, 0);
				
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
}

static void
nautilus_entry_initialize (NautilusEntry *entry)
{
	entry->user_edit = TRUE;

	nautilus_undo_setup_nautilus_entry_for_undo (entry);
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
}
