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
#include <gtk/gtkalignment.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>

struct NautilusSearchBarDetails {
	GtkWidget *entry;
	GtkWidget *search_button;
};

enum {
	ACTIVATE,
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
}

static gboolean
query_is_valid (NautilusSearchBar *bar)
{
	const char *text;

	text = gtk_entry_get_text (GTK_ENTRY (bar->details->entry));

	return text != NULL && text[0] != '\0';
}

static void
button_clicked_cb (GtkWidget *entry, NautilusSearchBar *bar)
{
	if (query_is_valid (bar)) {
		g_signal_emit (bar, signals[ACTIVATE], 0);
	}
}

static void
entry_activate_cb (GtkWidget *entry, NautilusSearchBar *bar)
{
	gtk_widget_activate (bar->details->search_button);
}

static void
entry_changed_cb (GtkWidget *entry, NautilusSearchBar *bar)
{
	gtk_widget_set_sensitive (bar->details->search_button,
				  query_is_valid (bar));
}

static void
nautilus_search_bar_init (NautilusSearchBar *bar)
{
	GtkWidget *alignment;
	GtkWidget *hbox;
	GtkWidget *image;
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

	image = gtk_image_new_from_icon_name ("stock_search", GTK_ICON_SIZE_MENU);
	gtk_widget_show (image);
	bar->details->search_button = gtk_button_new_with_label (_("Find Now"));
	g_signal_connect (bar->details->search_button, "clicked",
			  G_CALLBACK (button_clicked_cb), bar);
	gtk_button_set_image (GTK_BUTTON (bar->details->search_button), image);
	gtk_widget_set_sensitive (bar->details->search_button, FALSE);

	gtk_widget_show (bar->details->search_button);
	gtk_box_pack_start (GTK_BOX (hbox), bar->details->search_button, FALSE, FALSE, 0);
	
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
	gtk_entry_set_text (GTK_ENTRY (bar->details->entry), "");
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
	
	gtk_entry_set_text (GTK_ENTRY (bar->details->entry), text);
}
