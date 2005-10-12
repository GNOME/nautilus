/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * Copyright (C) 2005 Novell, Inc.
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
 * Author: Anders Carlsson <andersca@imendio.com>
 *
 */

#include <config.h>
#include "nautilus-search-bar.h"

#include <glib/gi18n.h>
#include <eel/eel-gtk-macros.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkalignment.h>
#include <gtk/gtkbindings.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>

struct NautilusSearchBarDetails {
	GtkWidget *entry;
	gboolean change_frozen;
	guint typing_timeout_id;
};

enum {
	ACTIVATE,
	CANCEL,
	LAST_SIGNAL
}; 

static guint signals[LAST_SIGNAL];

static void  nautilus_search_bar_class_init       (NautilusSearchBarClass *class);
static void  nautilus_search_bar_init             (NautilusSearchBar      *bar);

EEL_CLASS_BOILERPLATE (NautilusSearchBar,
		       nautilus_search_bar,
		       GTK_TYPE_EVENT_BOX)

static void
finalize (GObject *object)
{
	NautilusSearchBar *bar;

	bar = NAUTILUS_SEARCH_BAR (object);

	g_free (bar->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
nautilus_search_bar_class_init (NautilusSearchBarClass *class)
{
	GObjectClass *gobject_class;
	GtkBindingSet *binding_set;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;

	signals[ACTIVATE] =
		g_signal_new ("activate",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST,
		              G_STRUCT_OFFSET (NautilusSearchBarClass, activate),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	signals[CANCEL] =
		g_signal_new ("cancel",
		              G_TYPE_FROM_CLASS (class),
		              G_SIGNAL_RUN_LAST | GTK_RUN_ACTION,
		              G_STRUCT_OFFSET (NautilusSearchBarClass, cancel),
		              NULL, NULL,
		              g_cclosure_marshal_VOID__VOID,
		              G_TYPE_NONE, 0);

	binding_set = gtk_binding_set_by_class (class);
	gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0, "cancel", 0);
}

static gboolean
query_is_valid (NautilusSearchBar *bar)
{
	const char *text;

	text = gtk_entry_get_text (GTK_ENTRY (bar->details->entry));

	return text != NULL && text[0] != '\0';
}

static void
entry_activate_cb (GtkWidget *entry, NautilusSearchBar *bar)
{
	if (bar->details->typing_timeout_id) {
		g_source_remove (bar->details->typing_timeout_id);
		bar->details->typing_timeout_id = 0;
	}

	if (query_is_valid (bar)) {
		g_signal_emit (bar, signals[ACTIVATE], 0);
	}
}

static gboolean
typing_timeout_cb (gpointer user_data)
{
	NautilusSearchBar *bar;

	bar = NAUTILUS_SEARCH_BAR (user_data);

	if (query_is_valid (bar)) {
		g_signal_emit (bar, signals[ACTIVATE], 0);
	}

	bar->details->typing_timeout_id = 0;

	return FALSE;
}

#define TYPING_TIMEOUT 750

static void
entry_changed_cb (GtkWidget *entry, NautilusSearchBar *bar)
{
	if (bar->details->change_frozen) {
		return;
	}

	if (bar->details->typing_timeout_id) {
		g_source_remove (bar->details->typing_timeout_id);
	}

	bar->details->typing_timeout_id =
		g_timeout_add (TYPING_TIMEOUT,
			       typing_timeout_cb,
			       bar);
}

static void
nautilus_search_bar_init (NautilusSearchBar *bar)
{
	GtkWidget *alignment;
	GtkWidget *hbox;
	GtkWidget *label, *frame, *event_box;

	bar->details = g_new0 (NautilusSearchBarDetails, 1);

	gtk_widget_modify_bg (GTK_WIDGET (bar),
			      GTK_STATE_NORMAL, 
			      &GTK_WIDGET (bar)->style->bg[GTK_STATE_SELECTED]);

	alignment = gtk_alignment_new (0.5, 0.5,
				       1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
				   6, 6, 6, 6);
	gtk_widget_show (alignment);
	gtk_container_add (GTK_CONTAINER (bar), alignment);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);

	gtk_container_add (GTK_CONTAINER (alignment), hbox);

	label = gtk_label_new ("Search");
	gtk_widget_modify_fg (GTK_WIDGET (label),
			      GTK_STATE_NORMAL,
			      &GTK_WIDGET (label)->style->fg[GTK_STATE_SELECTED]);
	gtk_widget_show (label);

	frame = gtk_frame_new (NULL);
	gtk_widget_modify_bg (GTK_WIDGET (frame),
			      GTK_STATE_NORMAL,
			      &GTK_WIDGET (frame)->style->bg[GTK_STATE_SELECTED]);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_NONE);
	gtk_widget_show (frame);
	alignment = gtk_alignment_new (0.5, 0.5,
				       1.0, 1.0);
	gtk_alignment_set_padding (GTK_ALIGNMENT (alignment),
				   2, 2, 2, 2);
	gtk_widget_show (alignment);

	event_box = gtk_event_box_new ();
	gtk_widget_modify_bg (GTK_WIDGET (event_box),
			      GTK_STATE_NORMAL,
			      &GTK_WIDGET (event_box)->style->bg[GTK_STATE_SELECTED]);
	gtk_widget_show (event_box);
	gtk_container_add (GTK_CONTAINER (frame), event_box);

	gtk_container_add (GTK_CONTAINER (event_box), alignment);
	gtk_container_add (GTK_CONTAINER (alignment), label);
	gtk_box_pack_start (GTK_BOX (hbox), frame, FALSE, FALSE, 0);

	bar->details->entry = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), bar->details->entry, TRUE, TRUE, 0);

	g_signal_connect (bar->details->entry, "activate",
			  G_CALLBACK (entry_activate_cb), bar);
	g_signal_connect (bar->details->entry, "changed",
			  G_CALLBACK (entry_changed_cb), bar);

	gtk_widget_show (bar->details->entry);
}

void
nautilus_search_bar_grab_focus (NautilusSearchBar *bar)
{
	gtk_widget_grab_focus (bar->details->entry);
}

NautilusQuery *
nautilus_search_bar_get_query (NautilusSearchBar *bar)
{
	const char *query_text;
	NautilusQuery *query;

	query_text = gtk_entry_get_text (GTK_ENTRY (bar->details->entry));

	/* Empty string is a NULL query */
	if (query_text && query_text[0] == '\0') {
		return NULL;
	}
	
	query = nautilus_query_new ();
	nautilus_query_set_text (query, query_text);

	return query;
}

void
nautilus_search_bar_clear_query (NautilusSearchBar *bar)
{
	bar->details->change_frozen = TRUE;
	gtk_entry_set_text (GTK_ENTRY (bar->details->entry), "");
	bar->details->change_frozen = FALSE;
}

GtkWidget *
nautilus_search_bar_new (void)
{
	GtkWidget *bar;

	bar = g_object_new (NAUTILUS_TYPE_SEARCH_BAR, NULL);

	return bar;
}

void
nautilus_search_bar_set_query (NautilusSearchBar *bar, NautilusQuery *query)
{
	const char *text;

	if (!query) {
		nautilus_search_bar_clear_query (bar);
		return;
	}

	text = nautilus_query_get_text (query);
	if (!text) {
		text = "";
	}

	bar->details->change_frozen = TRUE;
	gtk_entry_set_text (GTK_ENTRY (bar->details->entry), text);
	bar->details->change_frozen = FALSE;
}
