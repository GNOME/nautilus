/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-clipboard.c
 *
 * Nautilus Clipboard support.  For now, routines to support component cut
 * and paste.
 *
 * Copyright (C) 1999, 2000  Free Software Foundaton
 * Copyright (C) 2000  Eazel, Inc.
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
 *          Darin Adler <darin@eazel.com>
 */

#include <config.h>
#include "nautilus-clipboard.h"

#include <bonobo/bonobo-ui-util.h>
#include <gtk/gtksignal.h>

static void disconnect_set_up_in_control_handlers (GtkObject *object,
						   gpointer callback_data);

static void
cut_callback (BonoboUIComponent *ui,
	      gpointer callback_data,
	      const char *command_name)
{
	g_assert (BONOBO_IS_UI_COMPONENT (ui));
	g_assert (strcmp (command_name, "Cut") == 0);
	
	gtk_editable_cut_clipboard (GTK_EDITABLE (callback_data));
}

static void
copy_callback (BonoboUIComponent *ui,
	       gpointer callback_data,
	       const char *command_name)
{
	g_assert (BONOBO_IS_UI_COMPONENT (ui));
	g_assert (strcmp (command_name, "Copy") == 0);
		
	gtk_editable_copy_clipboard (GTK_EDITABLE (callback_data));
}

static void
paste_callback (BonoboUIComponent *ui,
		gpointer callback_data,
		const char *command_name)
{
	g_assert (BONOBO_IS_UI_COMPONENT (ui));
	g_assert (strcmp (command_name, "Paste") == 0);
		
	gtk_editable_paste_clipboard (GTK_EDITABLE (callback_data));
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

#ifdef SENSITIVITY_ADJUSTMENT_WORKS

static void
set_paste_sensitive_if_clipboard_contains_data (BonoboUIHandler *ui_handler)
{
	gboolean clipboard_contains_data;
	
	clipboard_contains_data = 
		(gdk_selection_owner_get (GDK_SELECTION_PRIMARY) != NULL);

	bonobo_ui_handler_menu_set_sensitivity (ui_handler,
						NAUTILUS_MENU_PATH_PASTE_ITEM,
						clipboard_contains_data);
}

static void
set_clipboard_menu_items_sensitive (BonoboUIHandler *ui_handler)
{

       	bonobo_ui_handler_menu_set_sensitivity (ui_handler,
						NAUTILUS_MENU_PATH_CUT_ITEM,
						TRUE);
	bonobo_ui_handler_menu_set_sensitivity (ui_handler,
						NAUTILUS_MENU_PATH_COPY_ITEM,
						TRUE);
	bonobo_ui_handler_menu_set_sensitivity (ui_handler,
						NAUTILUS_MENU_PATH_CLEAR_ITEM,
						TRUE);
	set_paste_sensitive_if_clipboard_contains_data (ui_handler);
}


static void
set_clipboard_menu_items_insensitive (BonoboUIHandler *ui_handler,
				      gboolean enable_paste_for_full_clipboard)
{
	
       	bonobo_ui_handler_menu_set_sensitivity (ui_handler,
						NAUTILUS_MENU_PATH_CUT_ITEM,
						FALSE);
	bonobo_ui_handler_menu_set_sensitivity (ui_handler,
						NAUTILUS_MENU_PATH_COPY_ITEM,
						FALSE);
	bonobo_ui_handler_menu_set_sensitivity (ui_handler,
						NAUTILUS_MENU_PATH_CLEAR_ITEM,
						FALSE);
	if (enable_paste_for_full_clipboard) {
		set_paste_sensitive_if_clipboard_contains_data (ui_handler);
	}
	else {
		bonobo_ui_handler_menu_set_sensitivity (ui_handler,
							NAUTILUS_MENU_PATH_PASTE_ITEM,
							FALSE);
	}
}

#endif

typedef struct {
	BonoboUIComponent *component;
	Bonobo_UIContainer container;
} TargetCallbackData;

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
		bonobo_ui_component_set_container (ui,
						   target_data->container);
		bonobo_ui_component_freeze (ui, NULL);
		bonobo_ui_util_set_ui (ui,
				       DATADIR,
				       "nautilus-clipboard-ui.xml",
				       "nautilus");
		bonobo_ui_component_thaw (ui, NULL);

		/* FIXME bugzilla.eazel.com 733: Update sensitivities of the
		 * commands now, also arrange for them to stay updated as the
		 * state of the editable changes.
		 */
	} else {
		bonobo_ui_component_unset_container (ui);
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

void
nautilus_clipboard_set_up_editable (GtkEditable *target,
				    Bonobo_UIContainer ui_container)
{
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Cut", cut_callback),
		BONOBO_UI_VERB ("Copy", copy_callback),
		BONOBO_UI_VERB ("Paste", paste_callback),
		BONOBO_UI_VERB ("Clear", clear_callback),
		BONOBO_UI_VERB_END
	};
	BonoboUIComponent *ui;
	TargetCallbackData *target_data;
	
	g_return_if_fail (GTK_IS_EDITABLE (target));
	g_return_if_fail (ui_container != CORBA_OBJECT_NIL);

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
		 bonobo_control_get_remote_ui_container (BONOBO_CONTROL (callback_data)));
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
					       BonoboControl *control)
{
	g_return_if_fail (GTK_IS_EDITABLE (target));
	g_return_if_fail (BONOBO_IS_CONTROL (control));

	/* Use lazy initialization, so that we wait until after
	 * embedding. At that point, the UI container will be set up,
	 * but it's not necessarily set up now.
	 */
	/* We'd like to use gtk_signal_connect_while_alive, but it's
	 * not compatible with gtk_signal_disconnect calls.
	 */
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
