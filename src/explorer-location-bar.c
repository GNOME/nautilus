/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* explorer-location-bar.c - Location bar for the GNOME Explorer.

   Copyright (C) 1999 Free Software Foundation

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Ettore Perazzoli <ettore@gnu.org>
   DnD code added by Michael Meeks <michael@nuclecu.unam.mx>
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gnome.h>

#include "explorer-location-bar.h"

#define EXPLORER_DND_URI_LIST_TYPE 	  "text/uri-list"
#define EXPLORER_DND_TEXT_PLAIN_TYPE 	  "text/plain"
#define EXPLORER_DND_URL_TYPE		  "_NETSCAPE_URL"

typedef enum {
	EXPLORER_DND_MC_DESKTOP_ICON,
	EXPLORER_DND_URI_LIST,
	EXPLORER_DND_TEXT_PLAIN,
	EXPLORER_DND_URL,
	EXPLORER_DND_NTARGETS
} ExplorerDndTargetType;

static GtkHBoxClass *parent_class;
enum {
	LOCATION_CHANGED,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };

static GtkTargetEntry drag_types [] = {
	{ EXPLORER_DND_URI_LIST_TYPE,   0, EXPLORER_DND_URI_LIST },
	{ EXPLORER_DND_TEXT_PLAIN_TYPE, 0, EXPLORER_DND_TEXT_PLAIN },
	{ EXPLORER_DND_URL_TYPE,        0, EXPLORER_DND_URL }
};
static const int ndrag_types = sizeof (drag_types) / sizeof (drag_types[0]);

static GtkTargetEntry drop_types [] = {
	{ EXPLORER_DND_URI_LIST_TYPE,   0, EXPLORER_DND_URI_LIST },
	{ EXPLORER_DND_TEXT_PLAIN_TYPE, 0, EXPLORER_DND_TEXT_PLAIN },
	{ EXPLORER_DND_URL_TYPE,        0, EXPLORER_DND_URL }
};
static const int ndrop_types = sizeof (drop_types) / sizeof (drop_types[0]);


static void
drag_data_received_cb (GtkWidget *widget,
		       GdkDragContext *context,
		       gint x,
		       gint y,
		       GtkSelectionData *data,
		       guint info,
		       guint32 time,
		       ExplorerLocationBar *location_bar)
{
	GList *names;

	g_return_if_fail (data != NULL);

	names = gnome_uri_list_extract_uris (data->data);

	if (!names) {
		g_warning ("No D&D URI's");
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	if (g_list_length (names) > 1)
		g_warning ("FIXME: should we clone ourselfs ?");

	explorer_location_bar_set_uri_string (location_bar,
					      names->data);
	gtk_signal_emit (GTK_OBJECT (location_bar),
			 signals[LOCATION_CHANGED], names->data);

	gnome_uri_list_free_strings (names);

	gtk_drag_finish (context, TRUE, FALSE, time);
}

static void
drag_data_get_cb (GtkWidget *widget,
		  GdkDragContext *context,
		  GtkSelectionData *selection_data,
		  guint info,
		  guint32 time,
		  ExplorerLocationBar *location_bar)
{
	gchar *entry_txt;

	g_return_if_fail (location_bar != NULL);
	g_return_if_fail (selection_data != NULL);

	entry_txt = gtk_entry_get_text (GTK_ENTRY (location_bar->entry));
	g_return_if_fail (entry_txt != NULL);

	switch (info) {
	case EXPLORER_DND_URI_LIST:
	case EXPLORER_DND_TEXT_PLAIN:
	case EXPLORER_DND_URL:
		gtk_selection_data_set (selection_data,
					selection_data->target,
					8, (guchar *)entry_txt,
					strlen (entry_txt));
		break;
	default:
		g_assert_not_reached ();
	}

	g_free (entry_txt);
}

static void
location_changed (ExplorerLocationBar *location_bar)
{
	g_return_if_fail (location_bar != NULL);
	g_return_if_fail (EXPLORER_IS_LOCATION_BAR
			  (location_bar));
}

static void
editable_activated_cb (GtkEditable *editable,
		       ExplorerLocationBar *location_bar)
{
	g_return_if_fail (location_bar != NULL);
	g_return_if_fail (editable != NULL);
	g_return_if_fail (EXPLORER_IS_LOCATION_BAR
			  (location_bar));

	gtk_signal_emit (GTK_OBJECT (location_bar),
			 signals[LOCATION_CHANGED],
			 gtk_entry_get_text(GTK_ENTRY(editable)));
}	


static void
destroy (GtkObject *object)
{
	ExplorerLocationBar *location_bar;

	location_bar = EXPLORER_LOCATION_BAR (object);

	gtk_widget_destroy (location_bar->label);
	gtk_widget_destroy (location_bar->entry);

	(* GTK_OBJECT_CLASS (parent_class)->destroy) (object);
}


static void
class_init (ExplorerLocationBarClass *class)
{
	GtkObjectClass *object_class;

	parent_class = gtk_type_class (gtk_hbox_get_type ());

	object_class = GTK_OBJECT_CLASS (class);

	object_class->destroy = destroy;

	signals[LOCATION_CHANGED]
		= gtk_signal_new ("location_changed",
				  GTK_RUN_FIRST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (ExplorerLocationBarClass,
						     location_changed),
				  gtk_marshal_NONE__STRING,
				  GTK_TYPE_NONE, 1, GTK_TYPE_STRING);

	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);

	class->location_changed = location_changed;
}

static void
init (ExplorerLocationBar *location_bar)
{
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *eventbox;

	eventbox = gtk_event_box_new ();
	gtk_container_set_border_width (GTK_CONTAINER (eventbox),
					GNOME_PAD_SMALL);
	label = gtk_label_new (_("Location:"));
	gtk_container_add   (GTK_CONTAINER (eventbox), label);
	gtk_box_pack_start  (GTK_BOX (location_bar), eventbox, FALSE, TRUE,
			     GNOME_PAD_SMALL);

	entry = gtk_entry_new ();
	gtk_signal_connect (GTK_OBJECT (entry), "activate",
			    editable_activated_cb, location_bar);
	gtk_box_pack_start (GTK_BOX (location_bar), entry, TRUE, TRUE, 0);

	/* Drag source */
	gtk_drag_source_set (GTK_WIDGET (eventbox), 
			     GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
			     drag_types, ndrag_types,
			     GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_signal_connect  (GTK_OBJECT (eventbox), "drag_data_get",
			     GTK_SIGNAL_FUNC (drag_data_get_cb),
			     location_bar);

	/* Drag dest. */
	gtk_drag_dest_set  (GTK_WIDGET (location_bar),
			    GTK_DEST_DEFAULT_ALL,
			    drop_types, ndrop_types,
			    GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_signal_connect (GTK_OBJECT (location_bar), "drag_data_received",
			    GTK_SIGNAL_FUNC (drag_data_received_cb),
			    location_bar);

	gtk_widget_show (entry);
	gtk_widget_show_all (eventbox);

	location_bar->label = label;
	location_bar->entry = entry;
}


GtkType
explorer_location_bar_get_type (void)
{
	static GtkType type = 0;

	if (type == 0) {
		GtkTypeInfo info = {
			"ExplorerLocationBar",
			sizeof (ExplorerLocationBar),
			sizeof (ExplorerLocationBarClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			NULL,
			NULL,
			NULL
		};

		type = gtk_type_unique (gtk_hbox_get_type (), &info);
	}

	return type;
}

GtkWidget *
explorer_location_bar_new (void)
{
	return gtk_widget_new (explorer_location_bar_get_type (), NULL);
}

void
explorer_location_bar_set_uri_string (ExplorerLocationBar *bar,
				      const gchar *uri_string)
{
	g_return_if_fail (bar != NULL);
	g_return_if_fail (EXPLORER_IS_LOCATION_BAR (bar));

	if (uri_string == NULL)
		uri_string = "";

	gtk_entry_set_text (GTK_ENTRY (bar->entry), uri_string);
}
