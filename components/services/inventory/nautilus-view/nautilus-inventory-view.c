/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Copyright (C) 2000 Eazel, Inc
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
 * Author: J Shane Culpepper <pepper@eazel.com>
 */

#include <config.h>

#include <gnome-xml/tree.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <stdio.h>
#include <unistd.h>

#include "nautilus-inventory-view.h"



/* A NautilusContentView's private information. */
struct _NautilusInventoryViewDetails {
	char 		*uri;
	NautilusView	*nautilus_view;
	GtkWidget	*form;
	GtkWidget	*form_title;
	GtkWidget	*account_name;
	GtkWidget	*account_password;
	GtkWidget	*confirm_password;
	GtkWidget	*inventory_button;
	GtkWidget	*maintenance_button;
	GtkWidget	*feedback_text;
};

#define SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR	"rgb:BBBB/DDDD/FFFF"
#define SERVICE_DOMAIN_NAME			"eazel24.eazel.com"

static void	nautilus_inventory_view_initialize_class	(NautilusInventoryViewClass	*klass);
static void	nautilus_inventory_view_initialize		(NautilusInventoryView	*view);
static void	nautilus_inventory_view_destroy			(GtkObject		*object);
static void	inventory_load_location_callback		(NautilusView		*nautilus_view,
								 const char		*location,
	 							 NautilusInventoryView	*view);
static void	show_feedback				(NautilusInventoryView	*view,
							 char			*error_message);
static void	generate_inventory_form			(NautilusInventoryView	*view);
static void 	entry_changed_cb			(GtkWidget		*entry,
							 NautilusInventoryView	*view);
static void	inventory_button_cb			(GtkWidget		*button,
							 NautilusInventoryView	*view);
static void	maintenance_button_cb			(GtkWidget		*button,
							 NautilusInventoryView	*view);
static void	generate_form_title			(NautilusInventoryView	*view,
							 const char		*title_text);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusInventoryView, nautilus_inventory_view, GTK_TYPE_EVENT_BOX)

static void
generate_inventory_form (NautilusInventoryView	*view) {

	GtkTable	*table;
	GtkWidget	*temp_widget;
	GtkWidget	*temp_box;
	GtkWidget	*inventory_label;
	GtkWidget	*maintenance_button;
	GtkWidget	*maintenance_label;

	/* allocate a box to hold everything */
	view->details->form = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (view), view->details->form);
	gtk_widget_show (view->details->form);

	/* setup the title */
	generate_form_title (view, "Eazel Services Inventory");

	/* initialize the parent form */
	temp_box = gtk_hbox_new (FALSE, 4);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, 0, 0, 12);
	gtk_widget_show (temp_box);

	/* allocate a table to hold the inventory form */
	table = GTK_TABLE (gtk_table_new (2, 2, FALSE));

	/* username */
	temp_widget = gtk_label_new ("User Name: ");
	gtk_misc_set_alignment (GTK_MISC (temp_widget), 1.0, 0.5);
	gtk_table_attach (table, temp_widget, 0,1, 0,1, GTK_FILL, GTK_FILL, 2,2);
	gtk_widget_show (temp_widget);
	view->details->account_name = gtk_entry_new_with_max_length (36);
	gtk_table_attach (table, view->details->account_name, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 4,4);
	gtk_widget_show (view->details->account_name);

	/* password */
	temp_widget = gtk_label_new ("Password: ");
	gtk_misc_set_alignment (GTK_MISC (temp_widget), 1.0, 0.5);
	gtk_table_attach (table, temp_widget, 0,1, 1,2, GTK_FILL, GTK_FILL, 2,2);
	gtk_widget_show (temp_widget);
	view->details->account_password = gtk_entry_new_with_max_length (36);
	gtk_table_attach (table, view->details->account_password, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 4,4);
	gtk_entry_set_visibility (GTK_ENTRY (view->details->account_password), FALSE);
	gtk_widget_show (view->details->account_password);

	/* insert the table */
	gtk_box_pack_start (GTK_BOX (view->details->form), GTK_WIDGET(table), 0, 0, 4);
	gtk_widget_show (GTK_WIDGET(table));

	/* attach a changed signal to the 2 entry fields, so we can enable the button when something is typed into both fields */
	gtk_signal_connect (GTK_OBJECT (view->details->account_name),		"changed",	GTK_SIGNAL_FUNC (entry_changed_cb),	view);
	gtk_signal_connect (GTK_OBJECT (view->details->account_password),	"changed",	GTK_SIGNAL_FUNC (entry_changed_cb),	view);

	/* allocate the command buttons - first the inventory button */

	view->details->inventory_button = gtk_button_new ();
	inventory_label = gtk_label_new (" Login to Eazel ");
	gtk_widget_show (inventory_label);
	gtk_container_add (GTK_CONTAINER (view->details->inventory_button), inventory_label);

	temp_box = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (temp_box), view->details->inventory_button, FALSE, FALSE, 21);

	gtk_signal_connect (GTK_OBJECT (view->details->inventory_button),
			    "clicked",
			    GTK_SIGNAL_FUNC (inventory_button_cb), view);
	gtk_widget_set_sensitive (view->details->inventory_button, FALSE);
	gtk_widget_show (view->details->inventory_button);

        /* now allocate the account maintenance button */

        maintenance_button = gtk_button_new ();
        maintenance_label = gtk_label_new (" Account Maintenance ");
        gtk_widget_show (maintenance_label);
        gtk_container_add (GTK_CONTAINER (maintenance_button), maintenance_label);
        gtk_box_pack_start (GTK_BOX (temp_box), maintenance_button, FALSE, FALSE, 21);
        gtk_signal_connect (GTK_OBJECT (maintenance_button), "clicked",
			    GTK_SIGNAL_FUNC (maintenance_button_cb), view);
        gtk_widget_show (maintenance_button);

	/* show the buttons */
	gtk_widget_show (temp_box);
	gtk_box_pack_start (GTK_BOX (view->details->form), temp_box, FALSE, FALSE, 21);

        /* add a label for error messages, but don't show it until there's an error */
        view->details->feedback_text = gtk_label_new ("");
        gtk_box_pack_start (GTK_BOX (view->details->form), view->details->feedback_text, 0, 0, 8);
}

/* callback to enable/disable the inventory button when something is typed in the field */
static void
entry_changed_cb (GtkWidget	*entry, NautilusInventoryView	*view) {

	char		*user_name;
	char		*password;
	gboolean	button_enabled;

	user_name = gtk_entry_get_text (GTK_ENTRY (view->details->account_name));
	password = gtk_entry_get_text (GTK_ENTRY (view->details->account_password));

	button_enabled = user_name && strlen (user_name) && password && strlen (password);
	gtk_widget_set_sensitive (view->details->inventory_button, button_enabled);
}

/* callback to handle the inventory button.  Right now only dumps a simple feedback message. */
static void
inventory_button_cb (GtkWidget	*button, NautilusInventoryView	*view) {

	char		*user_name;
	char		*password;
	gboolean	registered_ok;

	user_name = gtk_entry_get_text (GTK_ENTRY (view->details->account_name));
	password = gtk_entry_get_text (GTK_ENTRY (view->details->account_password));
	registered_ok = FALSE;

	gtk_widget_hide (view->details->feedback_text);

	/* Do Nothing right now just return an ok response */
	registered_ok = TRUE;

	if (registered_ok == FALSE) {
		show_feedback (view, "I don't do anything !");
	}

	if (registered_ok) {
		show_feedback (view, "eazel:summary");
	}
}

/* callback to point account maintenance button to webpage */
static void
maintenance_button_cb (GtkWidget	*button, NautilusInventoryView	*view) {

	show_feedback (view, "http://www.eazel.com");
}

/* utility routine to show an error message */
static void
show_feedback (NautilusInventoryView	*view, char	*error_message) {

        gtk_label_set_text (GTK_LABEL (view->details->feedback_text), error_message);
        gtk_widget_show (view->details->feedback_text);
}

static void
generate_form_title (NautilusInventoryView	*view,
		     const char			*title_text) {

        GtkWidget	*temp_widget;
        char		*file_name;
        GtkWidget	*temp_container;
        GdkFont		*font;

        temp_container = gtk_hbox_new (FALSE, 0);

        gtk_box_pack_start (GTK_BOX (view->details->form), temp_container, 0, 0, 4);
        gtk_widget_show (temp_container);

        file_name = nautilus_pixmap_file ("eazel-logo.gif");
        temp_widget = GTK_WIDGET (gnome_pixmap_new_from_file (file_name));
        gtk_box_pack_start (GTK_BOX(temp_container), temp_widget, 0, 0, 8);
        gtk_widget_show (temp_widget);
        g_free (file_name);

        view->details->form_title = gtk_label_new (title_text);

        font = nautilus_font_factory_get_font_from_preferences (18);
        nautilus_gtk_widget_set_font (view->details->form_title, font);
        gdk_font_unref (font);

        gtk_box_pack_start (GTK_BOX (temp_container), view->details->form_title, 0, 0, 8);
        gtk_widget_show (view->details->form_title);
}

static void
nautilus_inventory_view_initialize_class (NautilusInventoryViewClass *klass) {

	GtkObjectClass	*object_class;
	GtkWidgetClass	*widget_class;
	
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	parent_class = gtk_type_class (gtk_event_box_get_type ());
	object_class->destroy = nautilus_inventory_view_destroy;
}

static void
nautilus_inventory_view_initialize (NautilusInventoryView *view) {

	NautilusBackground	*background;

	view->details = g_new0 (NautilusInventoryViewDetails, 1);
	view->details->nautilus_view = nautilus_view_new (GTK_WIDGET (view));
	gtk_signal_connect (GTK_OBJECT (view->details->nautilus_view), 
			    "load_location",
			    GTK_SIGNAL_FUNC (inventory_load_location_callback), 
			    view);

	background = nautilus_get_widget_background (GTK_WIDGET (view));
	nautilus_background_set_color (background, SERVICE_VIEW_DEFAULT_BACKGROUND_COLOR);

	gtk_widget_show (GTK_WIDGET (view));
}

static void
nautilus_inventory_view_destroy (GtkObject *object) {

	NautilusInventoryView	*view;
	
	view = NAUTILUS_INVENTORY_VIEW (object);

	if (view->details->uri) {
		g_free (view->details->uri);
	}
	g_free (view->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));

}

NautilusView *
nautilus_inventory_view_get_nautilus_view (NautilusInventoryView *view) {

	return view->details->nautilus_view;
}

void
nautilus_inventory_view_load_uri (NautilusInventoryView	*view,
			     	  const char		*uri) {

	/* dispose of any old uri and copy in the new one */	
	g_free (view->details->uri);
	view->details->uri = g_strdup (uri);

	/* dispose of any old form that was installed */
	if (view->details->form != NULL) {
		gtk_widget_destroy (view->details->form);
		view->details->form = NULL;
	}

	generate_inventory_form (view);
}

static void
inventory_load_location_callback (NautilusView		*nautilus_view, 
			      const char		*location,
			      NautilusInventoryView	*view) {

	g_assert (nautilus_view == view->details->nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);
	
	nautilus_inventory_view_load_uri (view, location);
	
	nautilus_view_report_load_complete (nautilus_view);
}

