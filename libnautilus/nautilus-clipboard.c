/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-clipboard.c
 *
 * Nautilus Clipboard support.  For now, routines to support component cut
 * and paste.
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000, 2001  Eazel, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Rebecca Schulman <rebecka@eazel.com>,
 *          Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "nautilus-clipboard.h"

#include "nautilus-bonobo-ui.h"
#include <bonobo/bonobo-ui-util.h>
#include <gtk/gtkinvisible.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktext.h>

typedef void (* EditableFunction) (GtkEditable *editable);

static void disconnect_set_up_in_control_handlers (GtkObject *object,
						   gpointer callback_data);
static void selection_changed_callback            (GtkWidget *widget,
						   gpointer callback_data);

static void
do_with_fake_current_event (EditableFunction function,
			    GtkEditable *editable)
{
	GdkEvent *current_event;
	GdkEvent fake_event;
	GtkWidget *fake_widget;

	/* Handle the simple case where no fakery is required. */
	current_event = gtk_get_current_event ();
	if (current_event != NULL) {
		gdk_event_free (current_event);
		(* function) (editable);
		return;
	}

	/* Call the function inside a fake event, so the event is
	 * non-NULL. This works around the gtk bug described in
	 * http://bugzilla.gnome.org/show_bug.cgi?id=51889 where the
	 * event must not be NULL.
	 */

	/* Make widget. */
	fake_widget = gtk_invisible_new ();
	gtk_signal_connect_object (GTK_OBJECT (fake_widget),
				   "client_event",
				   function,
				   GTK_OBJECT (editable));
	gtk_widget_realize (fake_widget);

	/* Make event. */
	memset (&fake_event, 0, sizeof (fake_event));
	fake_event.type = GDK_CLIENT_EVENT;
	fake_event.any.window = fake_widget->window;

	/* Handle event, calling through to function. */
	gtk_main_do_event (&fake_event);

	/* Discard widget. */
	gtk_widget_unref (fake_widget);
}

static void
cut_callback (BonoboUIComponent *ui,
	      gpointer callback_data,
	      const char *command_name)
{
	g_assert (BONOBO_IS_UI_COMPONENT (ui));
	g_assert (strcmp (command_name, "Cut") == 0);
	
	do_with_fake_current_event (gtk_editable_cut_clipboard,
				    GTK_EDITABLE (callback_data));
}

static void
copy_callback (BonoboUIComponent *ui,
	       gpointer callback_data,
	       const char *command_name)
{
	g_assert (BONOBO_IS_UI_COMPONENT (ui));
	g_assert (strcmp (command_name, "Copy") == 0);
	
	do_with_fake_current_event (gtk_editable_copy_clipboard,
				    GTK_EDITABLE (callback_data));
}

static void
paste_callback (BonoboUIComponent *ui,
		gpointer callback_data,
		const char *command_name)
{
	g_assert (BONOBO_IS_UI_COMPONENT (ui));
	g_assert (strcmp (command_name, "Paste") == 0);
	
	do_with_fake_current_event (gtk_editable_paste_clipboard,
				    GTK_EDITABLE (callback_data));
}

static void
clear_callback (BonoboUIComponent *ui,
		gpointer callback_data,
		const char *command_name)
{
	g_assert (BONOBO_IS_UI_COMPONENT (ui));
	g_assert (strcmp (command_name, "Clear") == 0);
	
	gtk_editable_delete_selection (GTK_EDITABLE (callback_data));
}

static void
select_all (GtkEditable *editable)
{
	int end;

	/* Workaround for bug in GtkText. It can't handle a -1
	 * passed in to set_position.
	 */
	end = -1;
	if (GTK_IS_TEXT (editable)) {
		end = gtk_text_get_length (GTK_TEXT (editable));
	}
	
	gtk_editable_set_position (editable, end);
	gtk_editable_select_region (editable, 0, end);
}

static gboolean
select_all_idle_callback (gpointer callback_data)
{
	GtkEditable *editable;

	editable = GTK_EDITABLE (callback_data);

	if (!GTK_OBJECT_DESTROYED (GTK_OBJECT (editable))) {
		select_all (editable);
	}

	gtk_object_unref (GTK_OBJECT (editable));

	return FALSE;
}

static void
select_all_callback (BonoboUIComponent *ui,
		     gpointer callback_data,
		     const char *command_name)
{
	GtkEditable *editable;

	g_assert (BONOBO_IS_UI_COMPONENT (ui));
	g_assert (strcmp (command_name, "Select All") == 0);

	editable = GTK_EDITABLE (callback_data);
	gtk_object_ref (GTK_OBJECT (editable));
	gtk_idle_add (select_all_idle_callback, editable);
}

static void
set_menu_item_sensitive (BonoboUIComponent *component,
			 const char *path,
			 gboolean sensitive)
{
	g_assert (BONOBO_IS_UI_COMPONENT (component));
	bonobo_ui_component_set_prop (component, path,
				      "sensitive",
				      sensitive ? "1" : "0",
				      NULL);
	
}

#if 0
static void
set_paste_sensitive_if_clipboard_contains_data (BonoboUIComponent *component)
{
	gboolean clipboard_contains_data;
	
	/* FIXME: This check is wrong, because gdk_selection_owner_get
	 * will only return non-null if the clipboard owner is in
	 * process, which may not be the case, and we may still be
	 * able to paste data.
	 */
	/* FIXME: PRIMARY is wrong here. We are interested in
	 * CLIPBOARD, not PRIMARY.
	 */
	/* FIXME: This doesn't tell us what kind of data is on the
	 * clipboard, and we only want to be sensitive if it's text.
	 */
	clipboard_contains_data = 
		(gdk_selection_owner_get (GDK_SELECTION_PRIMARY) != NULL);

	set_menu_item_sensitive (component,
				 NAUTILUS_COMMAND_PASTE,
				 clipboard_contains_data);
}
#endif

static void
set_clipboard_menu_items_sensitive (BonoboUIComponent *component)
{
       	set_menu_item_sensitive (component,
				 NAUTILUS_COMMAND_CUT,
				 TRUE);
	set_menu_item_sensitive (component,
				 NAUTILUS_COMMAND_COPY,
				 TRUE);
	set_menu_item_sensitive (component,
				 NAUTILUS_COMMAND_CLEAR,
				 TRUE);
}

static void
set_clipboard_menu_items_insensitive (BonoboUIComponent *component)
{
       	set_menu_item_sensitive (component,
				 NAUTILUS_COMMAND_CUT,
				 FALSE);
	set_menu_item_sensitive (component,
				 NAUTILUS_COMMAND_COPY,
				 FALSE);
	set_menu_item_sensitive (component,
				 NAUTILUS_COMMAND_CLEAR,
				 FALSE);
}

typedef struct {
	BonoboUIComponent *component;
	Bonobo_UIContainer container;
	gboolean           editable_shares_selection_changes;
} TargetCallbackData;

static gboolean
clipboard_items_are_merged_in (GtkWidget *widget)
{
	return GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (widget),
						     "Nautilus:clipboard_menu_items_merged"));
}

static void
set_clipboard_items_are_merged_in (GtkObject *widget_as_object,
				   gboolean merged_in)
{
	gtk_object_set_data (widget_as_object,
			     "Nautilus:clipboard_menu_items_merged",
			     GINT_TO_POINTER (merged_in));
}

static void
merge_in_clipboard_menu_items (GtkObject *widget_as_object,
			       TargetCallbackData *target_data)
{
	BonoboUIComponent *ui;
	Bonobo_UIContainer container;
	gboolean add_selection_callback;

	g_assert (target_data != NULL);
	ui = target_data->component;
	container = target_data->container;
	add_selection_callback = target_data->editable_shares_selection_changes;

	bonobo_ui_component_set_container (ui,
					   container);
	bonobo_ui_component_freeze (ui, NULL);
	bonobo_ui_util_set_ui (ui,
			       DATADIR,
			       "nautilus-clipboard-ui.xml",
			       "nautilus");
	
	if (add_selection_callback) {
		gtk_signal_connect_after (GTK_OBJECT (widget_as_object), "selection_changed",
					  selection_changed_callback, target_data);
		selection_changed_callback (GTK_WIDGET (widget_as_object),
					    target_data);
			
	} else {
		/* If we don't use sensitivity, everything should be on */
		set_clipboard_menu_items_sensitive (ui);
	}
	set_clipboard_items_are_merged_in (widget_as_object, TRUE);
	bonobo_ui_component_thaw (ui, NULL);
}

static void
merge_out_clipboard_menu_items (GtkObject *widget_as_object,
				TargetCallbackData *target_data)

{
	BonoboUIComponent *ui;
	gboolean selection_callback_was_added;

	g_assert (target_data != NULL);
	ui = BONOBO_UI_COMPONENT (target_data->component);
	selection_callback_was_added = target_data->editable_shares_selection_changes;
	bonobo_ui_component_unset_container (ui);

	if (selection_callback_was_added) {
		gtk_signal_disconnect_by_func (GTK_OBJECT (widget_as_object),
					       selection_changed_callback,
					       target_data);
	}
	set_clipboard_items_are_merged_in (widget_as_object, FALSE);
}

static void
focus_changed_callback (GtkWidget *widget,
			GdkEventAny *event,
			gpointer callback_data)
{
	TargetCallbackData *target_data;
	BonoboUIComponent *ui;

	g_assert (GTK_IS_EDITABLE (widget));
	g_assert (callback_data != NULL);
	target_data = callback_data;
	g_assert (BONOBO_IS_UI_COMPONENT (target_data->component));

	/* Connect the component to the container if the widget has focus. */
	ui = target_data->component;
	if (GTK_WIDGET_HAS_FOCUS (widget)) {
		if (!clipboard_items_are_merged_in (widget)) {
			merge_in_clipboard_menu_items (GTK_OBJECT (widget),
						       target_data);
		}
	} else {
		if (clipboard_items_are_merged_in (widget)) {
			merge_out_clipboard_menu_items (GTK_OBJECT (widget),
							target_data);
		}

	}
}

static void
selection_changed_callback (GtkWidget *widget,
			    gpointer callback_data)
{
	TargetCallbackData *target_data;
	BonoboUIComponent *component;
	GtkEditable *editable;

	target_data = (TargetCallbackData *) callback_data;
	g_assert (target_data != NULL);

	component = target_data->component;
	editable = GTK_EDITABLE (widget);

	if (editable->selection_start_pos != editable->selection_end_pos) {
		set_clipboard_menu_items_sensitive (component);
	} else {
		set_clipboard_menu_items_insensitive (component);
	}
}

static void
target_destroy_callback (GtkObject *object,
			 gpointer callback_data)
{
	TargetCallbackData *target_data;

	g_assert (callback_data != NULL);
	target_data = callback_data;
	g_assert (BONOBO_IS_UI_COMPONENT (target_data->component));

	/* Disconnect the component from the container, and then free
	 * everything.
	 */
	bonobo_ui_component_unset_container (target_data->component);
	bonobo_object_unref (BONOBO_OBJECT (target_data->component));
	bonobo_object_release_unref (target_data->container, NULL);
	g_free (target_data);
}

static TargetCallbackData *
initialize_clipboard_component_with_callback_data (GtkEditable *target,
						   Bonobo_UIContainer ui_container,
						   gboolean shares_selection_changes)
{
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Cut", cut_callback),
		BONOBO_UI_VERB ("Copy", copy_callback),
		BONOBO_UI_VERB ("Paste", paste_callback),
		BONOBO_UI_VERB ("Clear", clear_callback),
		BONOBO_UI_VERB ("Select All", select_all_callback),
		BONOBO_UI_VERB_END
	};
	BonoboUIComponent *ui;
	TargetCallbackData *target_data;

	/* Create the UI component and set up the verbs. */
	ui = bonobo_ui_component_new_default ();
	bonobo_ui_component_add_verb_list_with_data (ui, verbs, target);

	/* Do the actual connection of the UI to the container at
	 * focus time, and disconnect at both focus and destroy
	 * time.
	 */
	target_data = g_new (TargetCallbackData, 1);
	bonobo_object_ref (BONOBO_OBJECT (ui));
	target_data->component = ui;
	target_data->container = bonobo_object_dup_ref (ui_container, NULL);
	target_data->editable_shares_selection_changes = 
		shares_selection_changes;

	return target_data;
}

void
nautilus_clipboard_set_up_editable (GtkEditable *target,
				    Bonobo_UIContainer ui_container,
				    gboolean shares_selection_changes)
{
	TargetCallbackData *target_data;
	
	g_return_if_fail (GTK_IS_EDITABLE (target));
	g_return_if_fail (ui_container != CORBA_OBJECT_NIL);

	target_data = initialize_clipboard_component_with_callback_data
		(target, 
		 ui_container,
		 shares_selection_changes);

	gtk_signal_connect_after (GTK_OBJECT (target), "focus_in_event",
				  focus_changed_callback, target_data);
	gtk_signal_connect_after (GTK_OBJECT (target), "focus_out_event",
				  focus_changed_callback, target_data);

	gtk_signal_connect (GTK_OBJECT (target), "destroy",
			    target_destroy_callback, target_data);
	
	/* Call the focus changed callback once to merge if the window is
	 * already in focus.
	 */
	focus_changed_callback (GTK_WIDGET (target), NULL, target_data);
}


static gboolean
widget_was_set_up_with_selection_sensitivity (GtkWidget *widget)
{
	return GPOINTER_TO_INT (gtk_object_get_data (GTK_OBJECT (widget),
						     "Nautilus:shares_selection_changes"));
}

static void
first_focus_callback (GtkWidget *widget,
		      GdkEventAny *event,
		      gpointer callback_data)
{
	/* Don't set up the clipboard again on future focus_in's. This
	 * is one-time work.
	 */
	disconnect_set_up_in_control_handlers
		(GTK_OBJECT (widget),
		 callback_data);

	/* Do the rest of the setup. */
	nautilus_clipboard_set_up_editable
		(GTK_EDITABLE (widget),
		 bonobo_control_get_remote_ui_container (BONOBO_CONTROL (callback_data)),
		 widget_was_set_up_with_selection_sensitivity (widget));
}

static void
control_destroyed_callback (GtkObject *object,
			    gpointer callback_data)
{
	disconnect_set_up_in_control_handlers
		(object, callback_data);
}

void
nautilus_clipboard_set_up_editable_in_control (GtkEditable *target,
					       BonoboControl *control,
					       gboolean shares_selection_changes)
{
	g_return_if_fail (GTK_IS_EDITABLE (target));
	g_return_if_fail (BONOBO_IS_CONTROL (control));

	if (GTK_WIDGET_HAS_FOCUS (target)) {
		nautilus_clipboard_set_up_editable
			(target,
			 bonobo_control_get_remote_ui_container (control),
			 shares_selection_changes);
		return;
	}

	/* Use lazy initialization, so that we wait until after
	 * embedding. At that point, the UI container will be set up,
	 * but it's not necessarily set up now.
	 */
	/* We'd like to use gtk_signal_connect_while_alive, but it's
	 * not compatible with gtk_signal_disconnect calls.
	 */
	gtk_object_set_data (GTK_OBJECT (target),
			     "Nautilus:shares_selection_changes",
			     GINT_TO_POINTER (shares_selection_changes));
	gtk_signal_connect (GTK_OBJECT (target),
			    "focus_in_event",
			    first_focus_callback,
			    control);
	gtk_signal_connect (GTK_OBJECT (target),
			    "destroy",
			    control_destroyed_callback,
			    control);
}

static void
disconnect_set_up_in_control_handlers (GtkObject *object,
				       gpointer callback_data)
{
	gtk_signal_disconnect_by_func (object,
				       first_focus_callback,
				       callback_data);
	gtk_signal_disconnect_by_func (object,
				       control_destroyed_callback,
				       callback_data);
}
