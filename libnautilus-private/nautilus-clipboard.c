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
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Authors: Rebecca Schulman <rebecka@eazel.com>,
 *          Darin Adler <darin@bentspoon.com>
 */

#include <config.h>
#include "nautilus-clipboard.h"
#include "nautilus-file-utilities.h"

#include <libgnome/gnome-i18n.h>
#include <gtk/gtkclipboard.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkinvisible.h>
#include <gtk/gtkmain.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktext.h>
#include <string.h>

typedef void (* EditableFunction) (GtkEditable *editable);

static void selection_changed_callback            (GtkWidget *widget,
						   gpointer callback_data);
static void owner_change_callback (GtkClipboard        *clipboard,
				   GdkEventOwnerChange *event,
				   gpointer callback_data);
     
static void
action_cut_callback (GtkAction *action,
		     gpointer callback_data)
{
	gtk_editable_cut_clipboard (GTK_EDITABLE (callback_data));
}

static void
action_copy_callback (GtkAction *action,
		      gpointer callback_data)
{
	gtk_editable_copy_clipboard (GTK_EDITABLE (callback_data));
}

static void
action_paste_callback (GtkAction *action,
		       gpointer callback_data)
{
	gtk_editable_paste_clipboard (GTK_EDITABLE (callback_data));
}

static void
select_all (GtkEditable *editable)
{	
	gtk_editable_set_position (editable, -1);
	gtk_editable_select_region (editable, 0, -1);
}


static void
idle_source_destroy_callback (gpointer data,
			      GObject *where_the_object_was)
{
	g_source_destroy (data);
}

static gboolean
select_all_idle_callback (gpointer callback_data)
{
	GtkEditable *editable;
	GSource *source;

	editable = GTK_EDITABLE (callback_data);

	source = g_object_get_data (G_OBJECT (editable), 
				    "clipboard-select-all-source");

	g_object_weak_unref (G_OBJECT (editable), 
			     idle_source_destroy_callback,
			     source);
	
	g_object_set_data (G_OBJECT (editable), 
			   "clipboard-select-all-source",
			   NULL);

	select_all (editable);

	return FALSE;
}

static void
action_select_all_callback (GtkAction *action,
			    gpointer callback_data)
{
	GSource *source;
	GtkEditable *editable;

	editable = GTK_EDITABLE (callback_data);

	if (g_object_get_data (G_OBJECT (editable), 
			       "clipboard-select-all-source")) {
		return;
	}

	source = g_idle_source_new ();
	g_source_set_callback (source, select_all_idle_callback, editable, NULL);
	g_object_weak_ref (G_OBJECT (editable),
			   idle_source_destroy_callback,
			   source);
	g_source_attach (source, NULL);
	g_source_unref (source);

	g_object_set_data (G_OBJECT (editable),
			   "clipboard-select-all-source", 
			   source);
}

static void
received_clipboard_contents (GtkClipboard     *clipboard,
			     GtkSelectionData *selection_data,
			     gpointer          data)
{
	GtkActionGroup *action_group;
	GtkAction *action;

	action_group = data;

	action = gtk_action_group_get_action (action_group,
					      "Paste");
	if (action != NULL) {
		gtk_action_set_sensitive (action,
					  gtk_selection_data_targets_include_text (selection_data));
	}

	g_object_unref (action_group);
}


static void
set_paste_sensitive_if_clipboard_contains_data (GtkActionGroup *action_group)
{
	GtkAction *action;
	if (gdk_display_supports_selection_notification (gdk_display_get_default ())) {
		gtk_clipboard_request_contents (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD),
						gdk_atom_intern ("TARGETS", FALSE),
						received_clipboard_contents,
						g_object_ref (action_group));
	} else {
		/* If selection notification isn't supported, always activate Paste */
		action = gtk_action_group_get_action (action_group,
						      "Paste");
		gtk_action_set_sensitive (action, TRUE);
	}
}

static void
set_clipboard_menu_items_sensitive (GtkActionGroup *action_group)
{
	GtkAction *action;

	action = gtk_action_group_get_action (action_group,
					      "Cut");
	gtk_action_set_sensitive (action, TRUE);
	action = gtk_action_group_get_action (action_group,
					      "Copy");
	gtk_action_set_sensitive (action, TRUE);
}

static void
set_clipboard_menu_items_insensitive (GtkActionGroup *action_group)
{
	GtkAction *action;

	action = gtk_action_group_get_action (action_group,
					      "Cut");
	gtk_action_set_sensitive (action, FALSE);
	action = gtk_action_group_get_action (action_group,
					      "Copy");
	gtk_action_set_sensitive (action, FALSE);
}

typedef struct {
	GtkUIManager *ui_manager;
	GtkActionGroup *action_group;
	guint merge_id;
	gboolean editable_shares_selection_changes;
} TargetCallbackData;

static gboolean
clipboard_items_are_merged_in (GtkWidget *widget)
{
	return GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget),
						   "Nautilus:clipboard_menu_items_merged"));
}

static void
set_clipboard_items_are_merged_in (GObject *widget_as_object,
				   gboolean merged_in)
{
	g_object_set_data (widget_as_object,
			   "Nautilus:clipboard_menu_items_merged",
			   GINT_TO_POINTER (merged_in));
}

static char * clipboard_ui =
"<ui>"
"<menubar name='MenuBar'>"
"	<menu action='Edit'>"
"		<menuitem name='Cut' "
"			  action='Cut'/>"
"		<menuitem name='Copy' "
"			  action='Copy'/>"
"		<menuitem name='Paste' "
"			  action='Paste'/>"
"		<menuitem name='Select All'"
"			  action='Select All'/>"
"	</menu>"
"</menubar>"
"</ui>";


static void
merge_in_clipboard_menu_items (GObject *widget_as_object,
			       TargetCallbackData *target_data)
{
	gboolean add_selection_callback;

	g_assert (target_data != NULL);
	
	add_selection_callback = target_data->editable_shares_selection_changes;

	gtk_ui_manager_insert_action_group (target_data->ui_manager,
					    target_data->action_group, 0);
	
	target_data->merge_id = gtk_ui_manager_add_ui_from_string (target_data->ui_manager,
								   clipboard_ui, -1, NULL);

	set_paste_sensitive_if_clipboard_contains_data (target_data->action_group);
	
	g_signal_connect (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), "owner_change",
			  G_CALLBACK (owner_change_callback), target_data);
	
	if (add_selection_callback) {
		g_signal_connect_after (widget_as_object, "selection_changed",
					G_CALLBACK (selection_changed_callback), target_data);
		selection_changed_callback (GTK_WIDGET (widget_as_object),
					    target_data);
	} else {
		/* If we don't use sensitivity, everything should be on */
		set_clipboard_menu_items_sensitive (target_data->action_group);
	}
	set_clipboard_items_are_merged_in (widget_as_object, TRUE);
}

static void
merge_out_clipboard_menu_items (GObject *widget_as_object,
				TargetCallbackData *target_data)

{
	gboolean selection_callback_was_added;

	g_assert (target_data != NULL);

	gtk_ui_manager_remove_action_group (target_data->ui_manager,
					    target_data->action_group);
	
	if (target_data->merge_id != 0) {
		gtk_ui_manager_remove_ui (target_data->ui_manager,
					  target_data->merge_id);
		target_data->merge_id = 0;
	}

	g_signal_handlers_disconnect_matched (widget_as_object,
					      G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
					      0, 0, NULL,
					      G_CALLBACK (owner_change_callback),
					      target_data);
	
	selection_callback_was_added = target_data->editable_shares_selection_changes;

	if (selection_callback_was_added) {
		g_signal_handlers_disconnect_matched (widget_as_object,
						      G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA,
						      0, 0, NULL,
						      G_CALLBACK (selection_changed_callback),
						      target_data);
	}
	set_clipboard_items_are_merged_in (widget_as_object, FALSE);
}

static gboolean
focus_changed_callback (GtkWidget *widget,
			GdkEventAny *event,
			gpointer callback_data)
{
	/* Connect the component to the container if the widget has focus. */
	if (GTK_WIDGET_HAS_FOCUS (widget)) {
		if (!clipboard_items_are_merged_in (widget)) {
			merge_in_clipboard_menu_items (G_OBJECT (widget), callback_data);
		}
	} else {
		if (clipboard_items_are_merged_in (widget)) {
			merge_out_clipboard_menu_items (G_OBJECT (widget), callback_data);
		}
	}

	return FALSE;
}

static void
selection_changed_callback (GtkWidget *widget,
			    gpointer callback_data)
{
	TargetCallbackData *target_data;
	GtkEditable *editable;
	int start, end;

	target_data = (TargetCallbackData *) callback_data;
	g_assert (target_data != NULL);

	editable = GTK_EDITABLE (widget);

	if (gtk_editable_get_selection_bounds (editable, &start, &end) && start != end) {
		set_clipboard_menu_items_sensitive (target_data->action_group);
	} else {
		set_clipboard_menu_items_insensitive (target_data->action_group);
	}
}

static void
owner_change_callback (GtkClipboard        *clipboard,
		       GdkEventOwnerChange *event,
		       gpointer callback_data)
{
	TargetCallbackData *target_data;

	g_assert (callback_data != NULL);
	target_data = callback_data;

	set_paste_sensitive_if_clipboard_contains_data (target_data->action_group);
}

static void
target_destroy_callback (GtkObject *object,
			 gpointer callback_data)
{
	TargetCallbackData *target_data;

	g_assert (callback_data != NULL);
	target_data = callback_data;

	if (clipboard_items_are_merged_in (GTK_WIDGET(object))) {
		merge_out_clipboard_menu_items (G_OBJECT (object), callback_data);
	}
}

static void
target_data_free (TargetCallbackData *target_data)
{
	g_object_unref (target_data->action_group);
	g_free (target_data);
}

static GtkActionEntry clipboard_entries[] = {
  { "Cut", GTK_STOCK_CUT,                  /* name, stock id */
    N_("Cut _Text"), "<control>x",                /* label, accelerator */
    N_("Cut the selected text to the clipboard"),                   /* tooltip */ 
    G_CALLBACK (action_cut_callback) },
  { "Copy", GTK_STOCK_COPY,                  /* name, stock id */
    N_("_Copy Text"), "<control>c",                /* label, accelerator */
    N_("Copy the selected text to the clipboard"),                   /* tooltip */ 
    G_CALLBACK (action_copy_callback) },
  { "Paste", GTK_STOCK_PASTE,                  /* name, stock id */
    N_("_Paste Text"), "<control>v",                /* label, accelerator */
    N_("Paste the text stored on the clipboard"),                   /* tooltip */ 
    G_CALLBACK (action_paste_callback) },
  { "Select All", NULL,                  /* name, stock id */
    N_("Select _All"), "<control>A",                /* label, accelerator */
    N_("Select all the text in a text field"),                   /* tooltip */ 
    G_CALLBACK (action_select_all_callback) },
};

static TargetCallbackData *
initialize_clipboard_component_with_callback_data (GtkEditable *target,
						   GtkUIManager *ui_manager,
						   gboolean shares_selection_changes)
{
	GtkActionGroup *action_group;
	TargetCallbackData *target_data;

	action_group = gtk_action_group_new ("ClipboardActions");
	gtk_action_group_set_translation_domain (action_group, GETTEXT_PACKAGE);
	gtk_action_group_add_actions (action_group, 
				      clipboard_entries, G_N_ELEMENTS (clipboard_entries),
				      target);
	
	/* Do the actual connection of the UI to the container at
	 * focus time, and disconnect at both focus and destroy
	 * time.
	 */
	target_data = g_new (TargetCallbackData, 1);
	target_data->ui_manager = ui_manager;
	target_data->action_group = action_group;
	target_data->editable_shares_selection_changes = shares_selection_changes;
	
	return target_data;
}

void
nautilus_clipboard_set_up_editable (GtkEditable *target,
				    GtkUIManager *ui_manager,
				    gboolean shares_selection_changes)
{
	TargetCallbackData *target_data;
	
	g_return_if_fail (GTK_IS_EDITABLE (target));
	g_return_if_fail (ui_manager != NULL);

	target_data = initialize_clipboard_component_with_callback_data
		(target, 
		 ui_manager,
		 shares_selection_changes);

	g_signal_connect (target, "focus_in_event",
			  G_CALLBACK (focus_changed_callback), target_data);
	g_signal_connect (target, "focus_out_event",
			  G_CALLBACK (focus_changed_callback), target_data);
	g_signal_connect (target, "destroy",
			  G_CALLBACK (target_destroy_callback), target_data);

	g_object_weak_ref (G_OBJECT (target), (GWeakNotify) target_data_free, target_data);
	
	/* Call the focus changed callback once to merge if the window is
	 * already in focus.
	 */
	focus_changed_callback (GTK_WIDGET (target), NULL, target_data);
}
