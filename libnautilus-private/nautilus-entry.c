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
#include <gtk/gtkmain.h>
#include <gtk/gtkwidget.h>

static void nautilus_entry_initialize 	    (NautilusEntry 	*entry);
static void nautilus_entry_initialize_class (NautilusEntryClass *class);
static gint nautilus_entry_key_press 	    (GtkWidget   	*widget,
		     		      	     GdkEventKey 	*event);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusEntry, nautilus_entry, GTK_TYPE_ENTRY)

static void
nautilus_entry_initialize_class (NautilusEntryClass *class)
{
	GtkWidgetClass *widget_class;

	widget_class = GTK_WIDGET_CLASS (class);

	widget_class->key_press_event = nautilus_entry_key_press;
}

static void
nautilus_entry_initialize (NautilusEntry *entry)
{
	/* Nothing to do here yet. */
}

GtkWidget*
nautilus_entry_new (void)
{
  return GTK_WIDGET (gtk_type_new (NAUTILUS_TYPE_ENTRY));
}

static gint 
nautilus_entry_key_press (GtkWidget *widget, GdkEventKey *event)
{
	g_assert (NAUTILUS_IS_ENTRY (widget));

	if (!GTK_EDITABLE (widget)->editable) {
		return FALSE;
	}

	/* Fix bug in GtkEntry where keypad Enter key inserts a
	 * character rather than activating like the other Enter key.
	 */
	switch (event->keyval) {
		case GDK_KP_Enter:
			gtk_widget_activate (widget);
			return TRUE;
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

