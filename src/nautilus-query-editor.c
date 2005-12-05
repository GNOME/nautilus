/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Red Hat, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; see the file COPYING.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 * Author: Alexander Larsson <alexl@redhat.com>
 *
 */

#include <config.h>
#include "nautilus-query-editor.h"

#include <glib/gi18n.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-glib-extensions.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkbindings.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>

struct NautilusQueryEditorDetails {
	GtkWidget *entry;
	gboolean change_frozen;
	guint typing_timeout_id;
	gboolean is_visible;
	GtkWidget *invisible_vbox;
	GtkWidget *visible_vbox;
	
	NautilusSearchBar *bar;
};

enum {
	ACTIVATE,
	CANCEL,
	LAST_SIGNAL
}; 

static guint signals[LAST_SIGNAL];

static void  nautilus_query_editor_class_init       (NautilusQueryEditorClass *class);
static void  nautilus_query_editor_init             (NautilusQueryEditor      *editor);

static void entry_activate_cb (GtkWidget *entry, NautilusQueryEditor *editor);
static void entry_changed_cb  (GtkWidget *entry, NautilusQueryEditor *editor);

EEL_CLASS_BOILERPLATE (NautilusQueryEditor,
		       nautilus_query_editor,
		       GTK_TYPE_VBOX)

static void
nautilus_query_editor_finalize (GObject *object)
{
	NautilusQueryEditor *editor;

	editor = NAUTILUS_QUERY_EDITOR (object);

	g_free (editor->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_query_editor_dispose (GObject *object)
{
	NautilusQueryEditor *editor;

	editor = NAUTILUS_QUERY_EDITOR (object);
	

	if (editor->details->bar != NULL) {
		g_signal_handlers_disconnect_by_func (editor->details->entry,
						      entry_activate_cb,
						      editor);
		g_signal_handlers_disconnect_by_func (editor->details->entry,
						      entry_changed_cb,
						      editor);
		
		nautilus_search_bar_return_entry (editor->details->bar);
		eel_remove_weak_pointer (&editor->details->bar);
	}
	
	EEL_CALL_PARENT (G_OBJECT_CLASS, dispose, (object));
}

static void
nautilus_query_editor_class_init (NautilusQueryEditorClass *class)
{
	GObjectClass *gobject_class;
	GtkBindingSet *binding_set;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = nautilus_query_editor_finalize;
        gobject_class->dispose = nautilus_query_editor_dispose;

	signals[ACTIVATE] =
		g_signal_new ("activate",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusQueryEditorClass, activate),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__OBJECT,
		              G_TYPE_NONE, 1, NAUTILUS_TYPE_QUERY);

	signals[CANCEL] =
		g_signal_new ("cancel",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST | GTK_RUN_ACTION,
		              G_STRUCT_OFFSET (NautilusQueryEditorClass, cancel),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	binding_set = gtk_binding_set_by_class (class);
	gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0, "cancel", 0);
}

static gboolean
query_is_valid (NautilusQueryEditor *editor)
{
	const char *text;

	text = gtk_entry_get_text (GTK_ENTRY (editor->details->entry));

	return text != NULL && text[0] != '\0';
}

static void
entry_activate_cb (GtkWidget *entry, NautilusQueryEditor *editor)
{
	NautilusQuery *query;

	if (editor->details->typing_timeout_id) {
		g_source_remove (editor->details->typing_timeout_id);
		editor->details->typing_timeout_id = 0;
	}

	if (query_is_valid (editor)) {
		query = nautilus_query_editor_get_query (editor);
		g_signal_emit (editor, signals[ACTIVATE], 0, query);
		g_object_unref (query);
	}
}

static gboolean
typing_timeout_cb (gpointer user_data)
{
	NautilusQueryEditor *editor;
	NautilusQuery *query;

	editor = NAUTILUS_QUERY_EDITOR (user_data);

	if (query_is_valid (editor)) {
		query = nautilus_query_editor_get_query (editor);
		g_signal_emit (editor, signals[ACTIVATE], 0, query);
		g_object_unref (query);
	}

	editor->details->typing_timeout_id = 0;

	return FALSE;
}

#define TYPING_TIMEOUT 750

static void
entry_changed_cb (GtkWidget *entry, NautilusQueryEditor *editor)
{
	if (editor->details->change_frozen) {
		return;
	}

	if (editor->details->typing_timeout_id) {
		g_source_remove (editor->details->typing_timeout_id);
	}

	editor->details->typing_timeout_id =
		g_timeout_add (TYPING_TIMEOUT,
			       typing_timeout_cb,
			       editor);
}

static void
edit_clicked (GtkButton *button, NautilusQueryEditor *editor)
{
	nautilus_query_editor_set_visible (editor, TRUE);
}


static void
nautilus_query_editor_init (NautilusQueryEditor *editor)
{
	GtkWidget *hbox, *label, *button;

	editor->details = g_new0 (NautilusQueryEditorDetails, 1);
	editor->details->is_visible = TRUE;

	editor->details->invisible_vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (editor), editor->details->invisible_vbox,
			    FALSE, FALSE, 0);
	editor->details->visible_vbox = gtk_vbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (editor), editor->details->visible_vbox,
			    FALSE, FALSE, 0);
	/* Only show visible vbox */
	gtk_widget_show (editor->details->visible_vbox);

	/* Create invisible part: */
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (editor->details->invisible_vbox),
			    hbox, FALSE, FALSE, 0);
	gtk_widget_show (hbox);
	
	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), _("<b>Saved Search</b>"));
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	gtk_widget_show (label);
	
	button = gtk_button_new_with_label (_("Edit"));
	gtk_box_pack_end (GTK_BOX (hbox), button, FALSE, FALSE, 0);
	gtk_widget_show (button);

	g_signal_connect (button, "clicked",
			  G_CALLBACK (edit_clicked), editor);
	
}

static void
setup_internal_entry (NautilusQueryEditor *editor)
{
	GtkWidget *hbox, *label;
	
	/* Create visible part: */
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (editor->details->visible_vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new ("");
	gtk_label_set_markup (GTK_LABEL (label), _("<b>Search for:</b>"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	editor->details->entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), editor->details->entry, TRUE, TRUE, 0);

	g_signal_connect (editor->details->entry, "activate",
			  G_CALLBACK (entry_activate_cb), editor);
	g_signal_connect (editor->details->entry, "changed",
			  G_CALLBACK (entry_changed_cb), editor);
	gtk_widget_show (editor->details->entry);
}

static void
setup_external_entry (NautilusQueryEditor *editor, GtkWidget *entry)
{
	GtkWidget *hbox, *label;
	
	/* Create visible part: */
	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (editor->details->visible_vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new ("search...");
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);
	
	editor->details->entry = entry;
	g_signal_connect (editor->details->entry, "activate",
			  G_CALLBACK (entry_activate_cb), editor);
	g_signal_connect (editor->details->entry, "changed",
			  G_CALLBACK (entry_changed_cb), editor);
}

void
nautilus_query_editor_set_visible (NautilusQueryEditor *editor,
				   gboolean visible)
{
	editor->details->is_visible = visible;
	if (visible) {
		gtk_widget_show (editor->details->visible_vbox);
		gtk_widget_hide (editor->details->invisible_vbox);
	} else {
		gtk_widget_hide (editor->details->visible_vbox);
		gtk_widget_show (editor->details->invisible_vbox);
	}
}

void
nautilus_query_editor_grab_focus (NautilusQueryEditor *editor)
{
	gtk_widget_grab_focus (editor->details->entry);
}

NautilusQuery *
nautilus_query_editor_get_query (NautilusQueryEditor *editor)
{
	const char *query_text;
	NautilusQuery *query;

	query_text = gtk_entry_get_text (GTK_ENTRY (editor->details->entry));

	/* Empty string is a NULL query */
	if (query_text && query_text[0] == '\0') {
		return NULL;
	}
	
	query = nautilus_query_new ();
	nautilus_query_set_text (query, query_text);

	return query;
}

void
nautilus_query_editor_clear_query (NautilusQueryEditor *editor)
{
	editor->details->change_frozen = TRUE;
	gtk_entry_set_text (GTK_ENTRY (editor->details->entry), "");
	editor->details->change_frozen = FALSE;
}

GtkWidget *
nautilus_query_editor_new (gboolean start_hidden)
{
	GtkWidget *editor;

	editor = g_object_new (NAUTILUS_TYPE_QUERY_EDITOR, NULL);

	nautilus_query_editor_set_visible (NAUTILUS_QUERY_EDITOR (editor),
					   !start_hidden);
	
	setup_internal_entry (NAUTILUS_QUERY_EDITOR (editor));
		
	return editor;
}

GtkWidget*
nautilus_query_editor_new_with_bar (gboolean start_hidden,
				    NautilusSearchBar *bar)
{
	GtkWidget *entry;
	NautilusQueryEditor *editor;

	editor = NAUTILUS_QUERY_EDITOR (g_object_new (NAUTILUS_TYPE_QUERY_EDITOR, NULL));

	nautilus_query_editor_set_visible (editor, !start_hidden);

	editor->details->bar = bar;
	eel_add_weak_pointer (&editor->details->bar);
	
	entry = nautilus_search_bar_borrow_entry (bar);
	setup_external_entry (editor, entry);
	
	return GTK_WIDGET (editor);
}

void
nautilus_query_editor_set_query (NautilusQueryEditor *editor, NautilusQuery *query)
{
	const char *text;

	if (!query) {
		nautilus_query_editor_clear_query (editor);
		return;
	}

	text = nautilus_query_get_text (query);
	if (!text) {
		text = "";
	}

	editor->details->change_frozen = TRUE;
	gtk_entry_set_text (GTK_ENTRY (editor->details->entry), text);
	editor->details->change_frozen = FALSE;
}
