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

#include "nautilus-bonobo-ui.h"
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <bonobo/bonobo-ui-util.h>


static void
cut_callback (BonoboUIComponent *ui_component,
	      gpointer callback_data,
	      const char *cname)
{
	GtkEditable *editable_widget;

	g_assert (BONOBO_IS_UI_COMPONENT (ui_component));
	g_assert (strcmp (cname, "Cut") == 0);
	
	editable_widget = GTK_EDITABLE (callback_data);
	gtk_editable_cut_clipboard (editable_widget);
}

static void
copy_callback (BonoboUIComponent *ui_component,
	       gpointer callback_data,
	       const char *cname)
{
	GtkEditable *editable_widget;
	
	g_assert (BONOBO_IS_UI_COMPONENT (ui_component));
	g_assert (strcmp (cname, "Copy") == 0);
		
	editable_widget = GTK_EDITABLE (callback_data);
	gtk_editable_copy_clipboard (editable_widget);
}

static void
paste_callback (BonoboUIComponent *ui_component,
		gpointer callback_data,
		const char *cname)
{
	GtkEditable *editable_widget;

	g_assert (BONOBO_IS_UI_COMPONENT (ui_component));
	g_assert (strcmp (cname, "Paste") == 0);
		
	editable_widget = GTK_EDITABLE (callback_data);
	gtk_editable_paste_clipboard (editable_widget);
}

static void
clear_callback (BonoboUIComponent *ui_component,
		gpointer callback_data,
		const char *cname)
{
	GtkEditable *editable_widget;

	g_assert (BONOBO_IS_UI_COMPONENT (ui_component));
	g_assert (strcmp (cname, "Clear") == 0);


	editable_widget = GTK_EDITABLE (callback_data);
	gtk_editable_delete_selection (editable_widget);
}

#ifdef N0
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
#endif

static void
add_menu_items_callback (GtkWidget *widget,
			 GdkEventAny *event,
			 gpointer callback_data)
{
	BonoboUIComponent *ui_component;
	gpointer container_data;
	Bonobo_UIContainer container;
	BonoboUIVerb verbs [] = {
		BONOBO_UI_VERB ("Cut", (BonoboUIVerbFn) cut_callback),
		BONOBO_UI_VERB ("Copy", (BonoboUIVerbFn) copy_callback),
		BONOBO_UI_VERB ("Paste", (BonoboUIVerbFn) paste_callback),
		BONOBO_UI_VERB ("Clear", (BonoboUIVerbFn) clear_callback),
		BONOBO_UI_VERB_END
	};

	ui_component = gtk_object_get_data (GTK_OBJECT (widget), "clipboard_ui_component");
	container_data = gtk_object_get_data (GTK_OBJECT (widget), "associated_ui_container");
	container = * (Bonobo_UIContainer *) container_data;

	bonobo_ui_component_set_container (ui_component,
					   container);
	bonobo_ui_util_set_ui (ui_component, 
			       DATADIR,
			       "nautilus-clipboard-ui.xml",
			       "nautilus");
	g_assert (BONOBO_IS_UI_COMPONENT (ui_component));

	/* Add the verbs */
	bonobo_ui_component_add_verb_list_with_data (ui_component, verbs, widget);
	/* FIXME bugzilla.eazel.com 733: Update the sensitivities */
	
}

static void
remove_menu_items_callback (GtkWidget *widget,
			    GdkEventAny *event,
			    gpointer callback_data)
{
	BonoboUIComponent *ui_component;

	ui_component = gtk_object_get_data (GTK_OBJECT (widget), "clipboard_ui_component");
	bonobo_ui_component_unset_container (ui_component);

}

#ifdef N0
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

static void
container_copy_free (gpointer data)
{
	g_free (data);
}

static void
ui_component_remove_container_and_unref (gpointer data)
{
	BonoboUIComponent *ui;
	
	ui = BONOBO_UI_COMPONENT (data);
     
	bonobo_ui_component_unset_container (ui); 
	bonobo_object_unref (BONOBO_OBJECT (ui));
}

static void
finish_setting_up_editable  (GtkEditable *target,
			     Bonobo_UIContainer container)
{
	BonoboUIComponent *ui;
	Bonobo_UIContainer *ui_container;
	char *component_name;

	/* Create a unique component name for each clipboard item,
	   since they are registered and deregistered by name */
	component_name = g_strdup_printf ("Clipboard %p", target);
	ui = bonobo_ui_component_new (component_name);
	g_free (component_name);
	
	ui_container = g_new0 (Bonobo_UIContainer, 1);
	memcpy (ui_container, &container, sizeof (Bonobo_UIContainer));

	/* Free the ui component when we get rid of the widget */
	gtk_object_set_data_full (GTK_OBJECT (target), "clipboard_ui_component", 
				  ui, ui_component_remove_container_and_unref);
	gtk_object_set_data_full (GTK_OBJECT (target), "associated_ui_container",
				  ui_container, container_copy_free);

	gtk_signal_connect (GTK_OBJECT (target), "focus_in_event",
			    GTK_SIGNAL_FUNC (add_menu_items_callback),
			    NULL);
	gtk_signal_connect (GTK_OBJECT (target), "focus_out_event",
			    GTK_SIGNAL_FUNC (remove_menu_items_callback),
			    NULL);


}

static void
finish_setting_up_editable_from_bonobo_control_callback (gpointer data,
							 GdkEventAny *event,
							 gpointer user_data)
{
	GtkEditable *target;
	BonoboControl *control;

	target = GTK_EDITABLE (data);
	control = BONOBO_CONTROL (user_data);

	/* Don't set up the clipboard again on future focus_in's */
	gtk_signal_disconnect_by_func (GTK_OBJECT (data),
				       GTK_SIGNAL_FUNC (finish_setting_up_editable_from_bonobo_control_callback),
				       user_data);

	finish_setting_up_editable (target,
				    bonobo_control_get_remote_ui_container (control));

	/* Do the initial merging */
	add_menu_items_callback (GTK_WIDGET (target), 
				 NULL,
				 NULL);
				 

}

void
nautilus_clipboard_set_up_editable_from_bonobo_control (GtkEditable *target,
							BonoboControl *control)
{


	g_return_if_fail (GTK_IS_EDITABLE (target));
	g_return_if_fail (BONOBO_IS_CONTROL (control));

	/* Use lazy initialization, so that we wait until after
	   embedding, and thus for the ability to get the remote
	   ui container */
	gtk_signal_connect_while_alive (GTK_OBJECT (target),
					"focus_in_event",
					finish_setting_up_editable_from_bonobo_control_callback,
					control, GTK_OBJECT (control));

}
	

void
nautilus_clipboard_set_up_editable_from_bonobo_ui_container (GtkEditable *target,
							     Bonobo_UIContainer ui_container)
{
	

	g_return_if_fail (GTK_IS_EDITABLE (target));
	finish_setting_up_editable (target,
				    ui_container);
}

