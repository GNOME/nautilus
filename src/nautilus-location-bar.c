/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
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
 * Author: Maciej Stachowiak <mjs@eazel.com>
 *         Ettore Perazzoli <ettore@gnu.org>
 *         Michael Meeks <michael@nuclecu.unam.mx>
 *	   Andy Hertzfeld <andy@eazel.com>
 *
 */

/* nautilus-location-bar.c - Location bar for Nautilus
 */

#include <config.h>
#include "nautilus-location-bar.h"

#include "nautilus-location-entry.h"
#include "nautilus-window-private.h"
#include "nautilus-window.h"
#include <eel/eel-accessibility.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtksignal.h>
#include <glib/gi18n.h>
#include <libgnomeui/gnome-stock-icons.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libnautilus-private/nautilus-icon-dnd.h>
#include <libnautilus-private/nautilus-clipboard.h>
#include <stdio.h>
#include <string.h>

#define NAUTILUS_DND_URI_LIST_TYPE 	  "text/uri-list"
#define NAUTILUS_DND_TEXT_PLAIN_TYPE 	  "text/plain"

static const char untranslated_location_label[] = N_("Location:");
static const char untranslated_go_to_label[] = N_("Go To:");
#define LOCATION_LABEL _(untranslated_location_label)
#define GO_TO_LABEL _(untranslated_go_to_label)

struct NautilusLocationBarDetails {
	GtkLabel *label;
	NautilusEntry *entry;
	
	char *last_location;
	
	guint idle_id;
};

enum {
	NAUTILUS_DND_MC_DESKTOP_ICON,
	NAUTILUS_DND_URI_LIST,
	NAUTILUS_DND_TEXT_PLAIN,
	NAUTILUS_DND_NTARGETS
};

static const GtkTargetEntry drag_types [] = {
	{ NAUTILUS_DND_URI_LIST_TYPE,   0, NAUTILUS_DND_URI_LIST },
	{ NAUTILUS_DND_TEXT_PLAIN_TYPE, 0, NAUTILUS_DND_TEXT_PLAIN },
};

static const GtkTargetEntry drop_types [] = {
	{ NAUTILUS_DND_URI_LIST_TYPE,   0, NAUTILUS_DND_URI_LIST },
	{ NAUTILUS_DND_TEXT_PLAIN_TYPE, 0, NAUTILUS_DND_TEXT_PLAIN },
};

static char *nautilus_location_bar_get_location     (NautilusNavigationBar    *navigation_bar); 
static void  nautilus_location_bar_set_location     (NautilusNavigationBar    *navigation_bar,
						     const char               *location);
static void  nautilus_location_bar_class_init       (NautilusLocationBarClass *class);
static void  nautilus_location_bar_init             (NautilusLocationBar      *bar);
static void  nautilus_location_bar_update_label     (NautilusLocationBar      *bar);

EEL_CLASS_BOILERPLATE (NautilusLocationBar,
		       nautilus_location_bar,
		       NAUTILUS_TYPE_NAVIGATION_BAR)

static NautilusNavigationWindow *
nautilus_location_bar_get_window (GtkWidget *bar)
{
	return NAUTILUS_NAVIGATION_WINDOW (gtk_widget_get_ancestor (bar, NAUTILUS_TYPE_WINDOW));
}

static void
drag_data_received_callback (GtkWidget *widget,
		       	     GdkDragContext *context,
		       	     int x,
		       	     int y,
		       	     GtkSelectionData *data,
		             guint info,
		             guint32 time,
			     gpointer callback_data)
{
	char **names;
	NautilusApplication *application;
	int name_count;
	NautilusWindow *new_window;
	NautilusNavigationWindow *window;
	GdkScreen      *screen;
	gboolean new_windows_for_extras;
	char *prompt;
	char *detail;
	GFile *location;

	g_assert (NAUTILUS_IS_LOCATION_BAR (widget));
	g_assert (data != NULL);
	g_assert (callback_data == NULL);

	names = g_uri_list_extract_uris (data->data);

	if (names == NULL || *names == NULL) {
		g_warning ("No D&D URI's");
		gtk_drag_finish (context, FALSE, FALSE, time);
		return;
	}

	window = nautilus_location_bar_get_window (widget);
	new_windows_for_extras = FALSE;
	/* Ask user if they really want to open multiple windows
	 * for multiple dropped URIs. This is likely to have been
	 * a mistake.
	 */
	name_count = g_strv_length (names);
	if (name_count > 1) {
		prompt = g_strdup_printf (ngettext("Do you want to view %d location?",
						   "Do you want to view %d locations?",
						   name_count), 
					  name_count);
		detail = g_strdup_printf (ngettext("This will open %d separate window.",
						   "This will open %d separate windows.",
						   name_count),
					  name_count);			  
		/* eel_run_simple_dialog should really take in pairs
		 * like gtk_dialog_new_with_buttons() does. */
		new_windows_for_extras = eel_run_simple_dialog 
			(GTK_WIDGET (window),
			 TRUE,
			 GTK_MESSAGE_QUESTION,
			 prompt,
			 detail,
			 GTK_STOCK_CANCEL, GTK_STOCK_OK,
			 NULL) != 0 /* GNOME_OK */;

		g_free (prompt);
		g_free (detail);
		
		if (!new_windows_for_extras) {
			gtk_drag_finish (context, FALSE, FALSE, time);
			return;
		}
	}

	nautilus_navigation_bar_set_location (NAUTILUS_NAVIGATION_BAR (widget),
					      names[0]);	
	nautilus_navigation_bar_location_changed (NAUTILUS_NAVIGATION_BAR (widget));

	if (new_windows_for_extras) {
		int i;

		application = NAUTILUS_WINDOW (window)->application;
		screen = gtk_window_get_screen (GTK_WINDOW (window));

		for (i = 1; names[i] != NULL; ++i) {
			new_window = nautilus_application_create_navigation_window (application, NULL, screen);
			location = g_file_new_for_uri (names[i]);
			nautilus_window_go_to (new_window, location);
			g_object_unref (location);
		}
	}

	g_strfreev (names);

	gtk_drag_finish (context, TRUE, FALSE, time);
}

static void
drag_data_get_callback (GtkWidget *widget,
		  	GdkDragContext *context,
		  	GtkSelectionData *selection_data,
		  	guint info,
		 	guint32 time,
			gpointer callback_data)
{
	NautilusNavigationBar *bar;
	char *entry_text;

	g_assert (selection_data != NULL);
	bar = NAUTILUS_NAVIGATION_BAR (callback_data);

	entry_text = nautilus_navigation_bar_get_location (bar);
	
	switch (info) {
	case NAUTILUS_DND_URI_LIST:
	case NAUTILUS_DND_TEXT_PLAIN:
		gtk_selection_data_set (selection_data,
					selection_data->target,
					8, (guchar *) entry_text,
					eel_strlen (entry_text));
		break;
	default:
		g_assert_not_reached ();
	}
	g_free (entry_text);
}

/* routine that determines the usize for the label widget as larger
   then the size of the largest string and then sets it to that so
   that we don't have localization problems. see
   gtk_label_finalize_lines in gtklabel.c (line 618) for the code that
   we are imitating here. */

static void
style_set_handler (GtkWidget *widget, GtkStyle *previous_style)
{
	PangoLayout *layout;
	int width, width2;

	layout = gtk_label_get_layout (GTK_LABEL(widget));

	layout = pango_layout_copy (layout);

	pango_layout_set_text (layout, LOCATION_LABEL, -1);
	pango_layout_get_pixel_size (layout, &width, NULL);
	
	pango_layout_set_text (layout, GO_TO_LABEL, -1);
	pango_layout_get_pixel_size (layout, &width2, NULL);
	width = MAX (width, width2);

	width += 2 * GTK_MISC (widget)->xpad;

	gtk_widget_set_size_request (widget, width, -1);

	g_object_unref (layout);
}

static gboolean
label_button_pressed_callback (GtkWidget             *widget,
			       GdkEventButton        *event)
{
	NautilusNavigationWindow *window;
	NautilusView             *view;
	GtkWidget                *label;

	if (event->button != 3) {
		return FALSE;
	}

	window = nautilus_location_bar_get_window (widget->parent);
	view = NAUTILUS_WINDOW (window)->content_view;
	label = GTK_BIN (widget)->child;
	/* only pop-up if the URI in the entry matches the displayed location */
	if (view == NULL ||
	    strcmp (gtk_label_get_text (GTK_LABEL (label)), LOCATION_LABEL)) {
		return FALSE;
	}

	nautilus_view_pop_up_location_context_menu (view, event);

	return FALSE;
}

static int
get_editable_number_of_chars (GtkEditable *editable)
{
	char *text;
	int length;

	text = gtk_editable_get_chars (editable, 0, -1);
	length = g_utf8_strlen (text, -1);
	g_free (text);
	return length;
}

static void
set_position_and_selection_to_end (GtkEditable *editable)
{
	int end;

	end = get_editable_number_of_chars (editable);
	gtk_editable_select_region (editable, end, end);
	gtk_editable_set_position (editable, end);
}

static void
editable_event_after_callback (GtkEntry *entry,
			       GdkEvent *event,
			       gpointer user_data)
{
	nautilus_location_bar_update_label (NAUTILUS_LOCATION_BAR (user_data));
}

static void
real_activate (NautilusNavigationBar *navigation_bar)
{
	NautilusLocationBar *bar;

	bar = NAUTILUS_LOCATION_BAR (navigation_bar);

	/* Put the keyboard focus in the text field when switching to this mode,
	 * and select all text for easy overtyping 
	 */
	gtk_widget_grab_focus (GTK_WIDGET (bar->details->entry));
	nautilus_entry_select_all (bar->details->entry);
}

static void
real_cancel (NautilusNavigationBar *navigation_bar)
{
	char *last_location;

	last_location = NAUTILUS_LOCATION_BAR (navigation_bar)->details->last_location;
	nautilus_navigation_bar_set_location (navigation_bar, last_location);
}

static void
finalize (GObject *object)
{
	NautilusLocationBar *bar;

	bar = NAUTILUS_LOCATION_BAR (object);

	g_free (bar->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
destroy (GtkObject *object)
{
	NautilusLocationBar *bar;

	bar = NAUTILUS_LOCATION_BAR (object);
	
	/* cancel the pending idle call, if any */
	if (bar->details->idle_id != 0) {
		g_source_remove (bar->details->idle_id);
		bar->details->idle_id = 0;
	}
	
	g_free (bar->details->last_location);
	bar->details->last_location = NULL;
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

static void
nautilus_location_bar_class_init (NautilusLocationBarClass *class)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;
	NautilusNavigationBarClass *navigation_bar_class;

	gobject_class = G_OBJECT_CLASS (class);
	gobject_class->finalize = finalize;
	
	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = destroy;
	
	navigation_bar_class = NAUTILUS_NAVIGATION_BAR_CLASS (class);

	navigation_bar_class->activate = real_activate;
	navigation_bar_class->cancel = real_cancel;
	navigation_bar_class->get_location = nautilus_location_bar_get_location;
	navigation_bar_class->set_location = nautilus_location_bar_set_location;
}

static void
nautilus_location_bar_init (NautilusLocationBar *bar)
{
	GtkWidget *label;
	GtkWidget *entry;
	GtkWidget *event_box;
	GtkWidget *hbox;

	bar->details = g_new0 (NautilusLocationBarDetails, 1);

	hbox = gtk_hbox_new (0, FALSE);

	event_box = gtk_event_box_new ();
	gtk_event_box_set_visible_window (GTK_EVENT_BOX (event_box), FALSE);
	
	gtk_container_set_border_width (GTK_CONTAINER (event_box),
					GNOME_PAD_SMALL);
	label = gtk_label_new (LOCATION_LABEL);
	gtk_container_add   (GTK_CONTAINER (event_box), label);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (label), 1, 0.5);
	g_signal_connect (label, "style_set", 
			  G_CALLBACK (style_set_handler), NULL);

	gtk_box_pack_start (GTK_BOX (hbox), event_box, FALSE, TRUE,
			    GNOME_PAD_SMALL);

	entry = nautilus_location_entry_new ();
	
	g_signal_connect_object (entry, "activate",
				 G_CALLBACK (nautilus_navigation_bar_location_changed),
				 bar, G_CONNECT_SWAPPED);
	g_signal_connect_object (entry, "event_after",
				 G_CALLBACK (editable_event_after_callback), bar, G_CONNECT_AFTER);

	gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);

	eel_accessibility_set_up_label_widget_relation (label, entry);

	gtk_container_add (GTK_CONTAINER (bar), hbox);


	/* Label context menu */
	g_signal_connect (event_box, "button-press-event",
			  G_CALLBACK (label_button_pressed_callback), NULL);

	/* Drag source */
	gtk_drag_source_set (GTK_WIDGET (event_box), 
			     GDK_BUTTON1_MASK | GDK_BUTTON3_MASK,
			     drag_types, G_N_ELEMENTS (drag_types),
			     GDK_ACTION_COPY | GDK_ACTION_LINK);
	g_signal_connect_object (event_box, "drag_data_get",
				 G_CALLBACK (drag_data_get_callback), bar, 0);

	/* Drag dest. */
	gtk_drag_dest_set (GTK_WIDGET (bar),
			   GTK_DEST_DEFAULT_ALL,
			   drop_types, G_N_ELEMENTS (drop_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
	g_signal_connect (bar, "drag_data_received",
			  G_CALLBACK (drag_data_received_callback), NULL);

	gtk_widget_show_all (hbox);

	bar->details->label = GTK_LABEL (label);
	bar->details->entry = NAUTILUS_ENTRY (entry);	
}

GtkWidget *
nautilus_location_bar_new (NautilusNavigationWindow *window)
{
	GtkWidget *bar;
	NautilusLocationBar *location_bar;

	bar = gtk_widget_new (NAUTILUS_TYPE_LOCATION_BAR, NULL);
	location_bar = NAUTILUS_LOCATION_BAR (bar);

	/* Clipboard */
	nautilus_clipboard_set_up_editable
		(GTK_EDITABLE (location_bar->details->entry),
		 nautilus_window_get_ui_manager (NAUTILUS_WINDOW (window)),
		 TRUE);

	return bar;
}

static void
nautilus_location_bar_set_location (NautilusNavigationBar *navigation_bar,
				    const char *location)
{
	NautilusLocationBar *bar;
	char *formatted_location;
	GFile *file;

	g_assert (location != NULL);
	
	bar = NAUTILUS_LOCATION_BAR (navigation_bar);

	/* Note: This is called in reaction to external changes, and 
	 * thus should not emit the LOCATION_CHANGED signal. */

	if (eel_uri_is_search (location)) {
		nautilus_location_entry_set_special_text (NAUTILUS_LOCATION_ENTRY (bar->details->entry),
							  "");
	} else {
		file = g_file_new_for_uri (location);
		formatted_location = g_file_get_parse_name (file);
		g_object_unref (file);
		nautilus_entry_set_text (NAUTILUS_ENTRY (bar->details->entry),
					 formatted_location);
		set_position_and_selection_to_end (GTK_EDITABLE (bar->details->entry));
		g_free (formatted_location);
	}

	/* remember the original location for later comparison */
	
	if (bar->details->last_location != location) {
		g_free (bar->details->last_location);
		bar->details->last_location = g_strdup (location);
	}

	nautilus_location_bar_update_label (bar);
}

/**
 * nautilus_location_bar_get_location
 *
 * Get the "URI" represented by the text in the location bar.
 *
 * @bar: A NautilusLocationBar.
 *
 * returns a newly allocated "string" containing the mangled
 * (by g_file_parse_name) text that the user typed in...maybe a URI 
 * but not guaranteed.
 *
 **/
static char *
nautilus_location_bar_get_location (NautilusNavigationBar *navigation_bar) 
{
	NautilusLocationBar *bar;
	char *user_location, *uri;
	GFile *location;

	bar = NAUTILUS_LOCATION_BAR (navigation_bar);
	
	user_location = gtk_editable_get_chars (GTK_EDITABLE (bar->details->entry), 0, -1);
	location = g_file_parse_name (user_location);
	g_free (user_location);
	uri = g_file_get_uri (location);
	g_object_unref (location);
	return uri;
}
	       
/**
 * nautilus_location_bar_update_label
 *
 * if the text in the entry matches the uri, set the label to "location", otherwise use "goto"
 *
 **/
static void
nautilus_location_bar_update_label (NautilusLocationBar *bar)
{
	const char *current_text;
	GFile *location;
	GFile *last_location;
	
	current_text = gtk_entry_get_text (GTK_ENTRY (bar->details->entry));
	location = g_file_parse_name (current_text);
	last_location = g_file_parse_name (bar->details->last_location);
	
	if (g_file_equal (last_location, location)) {
		gtk_label_set_text (GTK_LABEL (bar->details->label), LOCATION_LABEL);
	} else {		 
		gtk_label_set_text (GTK_LABEL (bar->details->label), GO_TO_LABEL);
	}

	g_object_unref (location);
	g_object_unref (last_location);
}
