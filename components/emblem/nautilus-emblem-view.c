 /* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/*
 * Nautilus
 *
 * Copyright (C) 1999, 2000, 2001 Eazel, Inc.
 *
 * Nautilus is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Nautilus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author:  James Willcox  <jwillcox@gnome.org>
 *
 * This is a sidebar displaying emblems which can be dragged onto files to
 * set/unset the chosen emblem.
 *
 */

#include <config.h>
#include "nautilus-emblem-view.h"

#include <stdio.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-string.h>
#include <eel/eel-wrap-table.h>
#include <eel/eel-labeled-image.h>
#include <eel/eel-graphic-effects.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-vfs-extensions.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtkdnd.h>
#include <gtk/gtkeventbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtkdialog.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkstock.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkentry.h>
#include <librsvg/rsvg.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <gconf/gconf-client.h>
#include <libnautilus-private/nautilus-icon-factory.h>
#include <libnautilus-private/nautilus-icon-dnd.h>
#include <libnautilus-private/nautilus-emblem-utils.h>

struct NautilusEmblemViewDetails {
	GConfClient *client;
	GtkWidget *emblems_table;
};

#define ERASE_EMBLEM_KEYWORD			"erase"
#define STANDARD_EMBLEM_HEIGHT			52
#define EMBLEM_LABEL_SPACING			2

static void     nautilus_emblem_view_class_init      (NautilusEmblemViewClass *object_klass);
static void     nautilus_emblem_view_instance_init   (NautilusEmblemView *object);
static void     nautilus_emblem_view_finalize        (GObject     *object);
static void     nautilus_emblem_view_populate        (NautilusEmblemView *emblem_view);

static GtkTargetEntry drag_types[] = {
	{"property/keyword", 0, 0 }
};

enum {
	TARGET_URI_LIST,
	TARGET_URL
};

static GtkTargetEntry dest_types[] = {
	{"text/uri-list", 0, TARGET_URI_LIST},
	{"_NETSCAPE_URL", 0, TARGET_URL}
};

typedef struct _Emblem {
	GdkPixbuf *pixbuf;
	char *uri;
	char *name;
} Emblem;

BONOBO_CLASS_BOILERPLATE (NautilusEmblemView, nautilus_emblem_view,
			  NautilusView, NAUTILUS_TYPE_VIEW)

static void
nautilus_emblem_view_drag_data_get_cb (GtkWidget *widget,
				       GdkDragContext *context,
				       GtkSelectionData *data,
				       guint info,
				       guint time,
				       NautilusEmblemView *emblem_view)
{
	char *keyword;

	keyword = g_object_get_data (G_OBJECT (widget), "emblem-keyword");

	g_return_if_fail (keyword != NULL);

	gtk_selection_data_set (data, data->target, 8,
				keyword,
				strlen (keyword));
}

static void
nautilus_emblem_view_enter_notify_cb (GtkWidget *widget,
				       NautilusEmblemView *emblem_view)
{
	GdkPixbuf *pixbuf;
	EelLabeledImage *image;

	pixbuf = g_object_get_data (G_OBJECT (widget), "prelight-pixbuf");
	image = g_object_get_data (G_OBJECT (widget), "labeled-image");

	eel_labeled_image_set_pixbuf (EEL_LABELED_IMAGE (image), pixbuf);
}

static void
nautilus_emblem_view_leave_notify_cb (GtkWidget *widget,
				       NautilusEmblemView *emblem_view)
{
	GdkPixbuf *pixbuf;
	EelLabeledImage *image;

	pixbuf = g_object_get_data (G_OBJECT (widget), "original-pixbuf");
	image = g_object_get_data (G_OBJECT (widget), "labeled-image");

	eel_labeled_image_set_pixbuf (EEL_LABELED_IMAGE (image), pixbuf);
}

static GtkWidget *
create_emblem_widget (NautilusEmblemView *emblem_view,
		      const char *name)
{
	GtkWidget *image, *event_box;
	GdkPixbuf *pixbuf;
	GdkPixbuf *prelight_pixbuf;
	char *display_name, *keyword;
	
	pixbuf = nautilus_icon_factory_get_pixbuf_from_name (name, NULL,
							     NAUTILUS_ICON_SIZE_STANDARD,
							     &display_name);

	keyword = nautilus_emblem_get_keyword_from_icon_name (name);
	if (display_name == NULL) {
		display_name = g_strdup (keyword);
	}

	image = eel_labeled_image_new (display_name, pixbuf);
	g_free (display_name);

	eel_labeled_image_set_fixed_image_height (EEL_LABELED_IMAGE (image),
						  STANDARD_EMBLEM_HEIGHT);
	eel_labeled_image_set_spacing (EEL_LABELED_IMAGE (image),
				       EMBLEM_LABEL_SPACING);
	event_box = gtk_event_box_new ();
	gtk_container_add (GTK_CONTAINER (event_box), image);

	prelight_pixbuf = eel_create_spotlight_pixbuf (pixbuf);
	

	gtk_drag_source_set (event_box, GDK_BUTTON1_MASK, drag_types,
			     G_N_ELEMENTS (drag_types),
			     GDK_ACTION_COPY | GDK_ACTION_MOVE);

	gtk_drag_source_set_icon_pixbuf (event_box, pixbuf);

	g_signal_connect (event_box, "drag-data-get",
			  G_CALLBACK (nautilus_emblem_view_drag_data_get_cb),
			  emblem_view);
	g_signal_connect (event_box, "enter-notify-event",
			  G_CALLBACK (nautilus_emblem_view_enter_notify_cb),
			  emblem_view);
	g_signal_connect (event_box, "leave-notify-event",
			  G_CALLBACK (nautilus_emblem_view_leave_notify_cb),
			  emblem_view);
	
	g_object_set_data_full (G_OBJECT (event_box),
				"emblem-keyword",
				keyword, g_free);
	g_object_set_data_full (G_OBJECT (event_box),
				"original-pixbuf",
				pixbuf, g_object_unref);
	g_object_set_data_full (G_OBJECT (event_box),
				"prelight-pixbuf",
				prelight_pixbuf, g_object_unref);
	g_object_set_data (G_OBJECT (event_box),
			   "labeled-image", image);

	return event_box;
}

static void
emblem_name_entry_changed_cb (GtkWidget *entry, Emblem *emblem)
{
	char *text;

	g_free (emblem->name);

	text = gtk_editable_get_chars (GTK_EDITABLE (entry), 0, -1);

	emblem->name = g_strdup (text);
}


static void
destroy_emblem (Emblem *emblem, gpointer user_data)
{
	g_return_if_fail (emblem != NULL);


	if (emblem->pixbuf != NULL) {
		g_object_unref (emblem->pixbuf);
		emblem->pixbuf = NULL;
	}

	if (emblem->name != NULL) {
		g_free (emblem->name);
		emblem->name = NULL;
	}

	if (emblem->uri != NULL) {
		g_free (emblem->uri);
		emblem->uri = NULL;
	}
	
	g_free (emblem);
}

static void
destroy_emblem_list (GSList *list)
{
	g_slist_foreach (list, (GFunc)destroy_emblem, NULL);
	g_slist_free (list);
}

static GtkWidget *
create_add_emblems_dialog (NautilusEmblemView *emblem_view,
			   GSList *emblems)
{
	GtkWidget *dialog, *label, *table, *image, *entry, *scroller, *hbox;
	Emblem *emblem;
	GSList *list;
	int num_emblems;
	
	dialog = gtk_dialog_new_with_buttons (_("Add Emblems..."),
					      NULL,
					      0,
					      GTK_STOCK_CANCEL,
					      GTK_RESPONSE_CANCEL,
					      GTK_STOCK_OK,
					      GTK_RESPONSE_OK,
					      GTK_STOCK_HELP,
					      GTK_RESPONSE_HELP,
					      NULL);

	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
					 GTK_RESPONSE_OK);

	/* FIXME:  make a better message */
	if (g_slist_length (emblems) > 1) {
		label = gtk_label_new (_("Enter a descriptive name next to each emblem.  This name will be used in other places to identify the emblem."));
	} else {
		label = gtk_label_new (_("Enter a descriptive name next to the emblem.  This name will be used in other places to identify the emblem."));
	}
	
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    label, FALSE, FALSE, GNOME_PAD);
	gtk_widget_show (label);
	
	scroller = eel_scrolled_wrap_table_new (TRUE, &table);
	eel_wrap_table_set_x_spacing (EEL_WRAP_TABLE (table), GNOME_PAD);
	eel_wrap_table_set_y_spacing (EEL_WRAP_TABLE (table), GNOME_PAD);
	
	num_emblems=0;
	list = emblems;
	while (list != NULL) {
		/* walk through the list of emblems, and create a pixbuf
		 * and entry for each one
		 */

		emblem = (Emblem *)list->data;
		list = list->next;

		image = gtk_image_new_from_pixbuf (emblem->pixbuf);

		hbox = gtk_hbox_new (TRUE, 0);
		gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);

		entry = gtk_entry_new ();
		gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
		g_signal_connect (entry, "changed",
				  G_CALLBACK (emblem_name_entry_changed_cb),
				  emblem);

		gtk_box_pack_start (GTK_BOX (hbox), entry, FALSE, FALSE, 0);
		gtk_container_add (GTK_CONTAINER (table), hbox);

		num_emblems++;
	}

	gtk_container_set_border_width (GTK_CONTAINER (dialog), GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox),
			    scroller, TRUE, TRUE, GNOME_PAD);
	gtk_widget_show_all (scroller);

	/* we expand the window to hold up to about 4 emblems, but after that
	 * let the scroller do its thing.  Is there a better way to do this?
	 */
	gtk_window_set_default_size (GTK_WINDOW (dialog), 400,
				     MIN (120+(60*num_emblems), 350));

	g_object_set_data_full (G_OBJECT (dialog), "emblems-to-add",
				emblems, (GDestroyNotify)destroy_emblem_list);

	return dialog;
}

static void
remove_widget (GtkWidget *widget, GtkContainer *container)
{
	gtk_container_remove (container, widget);
}

static void
nautilus_emblem_view_refresh (NautilusEmblemView *emblem_view)
{
	nautilus_emblem_refresh_list ();

	gtk_container_foreach (GTK_CONTAINER (emblem_view->details->emblems_table),
			       (GtkCallback)remove_widget,
			       emblem_view->details->emblems_table);
	
	nautilus_emblem_view_populate (emblem_view);
}

static void
add_emblems_dialog_response_cb (GtkWidget *dialog, int response,
				NautilusEmblemView *emblem_view)
{
	Emblem *emblem;
	GSList *emblems;

	switch (response) {
	case GTK_RESPONSE_CANCEL:
		gtk_widget_destroy (dialog);
		break;

	case GTK_RESPONSE_HELP:
		g_message ("Implement me!");
		break;

	case GTK_RESPONSE_OK:
		emblems = g_object_get_data (G_OBJECT (dialog),
					     "emblems-to-add");

		while (emblems != NULL) {
			emblem = (Emblem *)emblems->data;

			nautilus_emblem_install_custom_emblem (emblem->pixbuf,
							       emblem->name,
							       emblem->name,
							       GTK_WINDOW (dialog));
		
			emblems = emblems->next;
		}

		gtk_widget_destroy (dialog);

		nautilus_emblem_view_refresh (emblem_view);
		break;
	}
}

static void
show_add_emblems_dialog (NautilusEmblemView *emblem_view,
			 GSList *emblems)
{
	GtkWidget *dialog;
	
	g_return_if_fail (emblems != NULL);

	dialog = create_add_emblems_dialog (emblem_view, emblems);

	if (dialog == NULL) {
		return;
	}

	g_signal_connect (dialog, "response",
			  G_CALLBACK (add_emblems_dialog_response_cb),
			  emblem_view);

	gtk_window_present (GTK_WINDOW (dialog));
}

static void
nautilus_emblem_view_drag_received_cb (GtkWidget *widget,
				       GdkDragContext *drag_context,
				       gint x,
				       gint y,
				       GtkSelectionData *data,
				       guint info,
				       guint time,
				       NautilusEmblemView *emblem_view)
{
	GSList *emblems;
	Emblem *emblem;
	GdkPixbuf *pixbuf;
	char *uri;
	GList *uris, *l;

	emblems = NULL;
	
	switch (info) {
	case TARGET_URI_LIST:
		if (data->format != 8 ||
		    data->length == 0) {
			g_message ("URI list had wrong format (%d) or length (%d)\n",
				   data->format, data->length);
			return;
		}

		uris = nautilus_icon_dnd_uri_list_extract_uris (data->data);
		l = uris;
		while (l != NULL) {
			uri = l->data;
			l = l->next;

			pixbuf = nautilus_emblem_load_pixbuf_for_emblem (uri);

			if (pixbuf == NULL) {
				/* this one apparently isn't an image, or
				 * at least not one that we know how to read
				 */
				continue;
			}

			emblem = g_new (Emblem, 1);
			emblem->uri = g_strdup (uri);
			emblem->name = NULL; /* created later on by the user */
			emblem->pixbuf = pixbuf;

			emblems = g_slist_prepend (emblems, emblem);
		}
		nautilus_icon_dnd_uri_list_free_strings (uris);
 
		if (emblems != NULL) {
			show_add_emblems_dialog (emblem_view, emblems);
		}
		break;
	
	case TARGET_URL:
		/* the point of this section is to allow people to drop URIs
		 * from netscape/galeon/whatever, and add them as emblems.
		 * However, eel_gdk_pixbuf_load() is failing, so it doesn't
		 * work :/
		 */
		
		if (data->format != 8 ||
		    data->length == 0) {
			g_message ("URI had wrong format (%d) or length (%d)\n",
				   data->format, data->length);
			return;
		}

		uri = g_strndup (data->data, data->length);
		
		pixbuf = nautilus_emblem_load_pixbuf_for_emblem (uri);

		if (pixbuf != NULL) {
			emblem = g_new (Emblem, 1);
			emblem->uri = g_strdup (uri);
			emblem->name = NULL;
			emblem->pixbuf = pixbuf;

			emblems = g_slist_prepend (NULL, emblem);

			show_add_emblems_dialog (emblem_view, emblems);
		}
		break;
	}
}

static GtkWidget *
nautilus_emblem_view_create_container (NautilusEmblemView *emblem_view)
{
	GtkWidget *emblems_table, *scroller;

	/* The emblems wrapped table */
	scroller = eel_scrolled_wrap_table_new (TRUE, &emblems_table);

	gtk_container_set_border_width (GTK_CONTAINER (emblems_table), GNOME_PAD);

	/* set up dnd for adding emblems */
	gtk_drag_dest_set (scroller,
			   GTK_DEST_DEFAULT_ALL,
			   dest_types, G_N_ELEMENTS (dest_types),
			   GDK_ACTION_COPY | GDK_ACTION_MOVE);

	g_signal_connect (scroller, "drag-data-received",
			  G_CALLBACK (nautilus_emblem_view_drag_received_cb),
			  emblem_view);
			
	gtk_widget_show (scroller);

	emblem_view->details->emblems_table = emblems_table;

	return scroller;
}

static void
nautilus_emblem_view_populate (NautilusEmblemView *emblem_view)
{
	GList *icons, *l;
	GtkWidget *emblem_widget;
	char *name;

	
	/* FIXME:  need to get the erase emblem somehow
	emblem_widget = create_emblem_widget (emblem_view, "erase", FALSE);
	gtk_container_add (GTK_CONTAINER (emblems_table), emblem_widget);
	*/

	icons = nautilus_emblem_list_availible ();

	l = icons;
	while (l != NULL) {
		name = (char *)l->data;
		l = l->next;

		if (!nautilus_emblem_should_show_in_list (name)) {
			continue;
		}

		emblem_widget = create_emblem_widget (emblem_view, name);
		
		gtk_container_add
			(GTK_CONTAINER (emblem_view->details->emblems_table),
			 emblem_widget);
	}
	eel_g_list_free_deep (icons);

	gtk_widget_show_all (emblem_view->details->emblems_table);
}

static void
nautilus_emblem_view_instance_init (NautilusEmblemView *emblem_view)
{
	BonoboControl *control;
	GtkWidget *widget;
	
	emblem_view->details = g_new0 (NautilusEmblemViewDetails, 1);

	emblem_view->details->client = gconf_client_get_default ();

	widget = nautilus_emblem_view_create_container (emblem_view);
	nautilus_emblem_view_populate (emblem_view);

	control = bonobo_control_new (widget);
	nautilus_view_construct_from_bonobo_control
				(NAUTILUS_VIEW (emblem_view), control);

}

static void
nautilus_emblem_view_finalize (GObject *object)
{
	NautilusEmblemView *emblem_view;

	g_return_if_fail (NAUTILUS_IS_EMBLEM_VIEW (object));
	emblem_view = NAUTILUS_EMBLEM_VIEW (object);

	if (emblem_view->details != NULL) {
		if (emblem_view->details->client != NULL) {
			g_object_unref (emblem_view->details->client);
		}

		g_free (emblem_view->details);
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
nautilus_emblem_view_class_init (NautilusEmblemViewClass *object_klass)
{
	GObjectClass *gobject_class;
	
	NautilusEmblemViewClass *klass;

	klass = NAUTILUS_EMBLEM_VIEW_CLASS (object_klass);
	gobject_class = G_OBJECT_CLASS (object_klass);

	gobject_class->finalize = nautilus_emblem_view_finalize;
}

