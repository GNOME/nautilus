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
 * Author: Maciej Stachowiak <mjs@eazel.com>
 */

/* nautilus-inventory-config-page.h - 
 */

#include <config.h>
#include "nautilus-inventory-config-page.h"
#include <gtk/gtklabel.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkentry.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkcheckbutton.h>
#include <eel/eel-gtk-macros.h>
#include <libnautilus/nautilus-view.h>
#include <eazel-inventory-service-interface.h>

#define SERVICE_IID "OAFIID:trilobite_inventory_service:eaae1152-1551-43d5-a764-52274131a9d5"

struct NautilusInventoryConfigPageDetails {
	GtkWidget                 *machine_entry;
	GtkWidget                 *warn_check_button;
	NautilusView              *view;
	Trilobite_Eazel_Inventory  inventory_service;
};

static void     nautilus_inventory_config_page_initialize_class    (NautilusInventoryConfigPageClass *klass);
static void     nautilus_inventory_config_page_initialize          (NautilusInventoryConfigPage      *file);
static void     nautilus_inventory_config_page_destroy             (GtkObject                        *object);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusInventoryConfigPage, nautilus_inventory_config_page, GTK_TYPE_VBOX)

static void
nautilus_inventory_config_page_initialize_class (NautilusInventoryConfigPageClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_inventory_config_page_destroy;
}

static void
next_button_callback (GtkWidget *button, 
                      NautilusInventoryConfigPage *config_page)
{
	CORBA_Environment ev;
	g_print ("clicked next.\n");

	CORBA_exception_init (&ev);

	Trilobite_Eazel_Inventory__set_machine_name (config_page->details->inventory_service,
						     gtk_entry_get_text
						     (GTK_ENTRY (config_page->details->machine_entry)), &ev);

	Trilobite_Eazel_Inventory__set_warn_before_upload (config_page->details->inventory_service,
							   gtk_toggle_button_get_active
							   (GTK_TOGGLE_BUTTON (config_page->details->warn_check_button)), &ev);


	Trilobite_Eazel_Inventory__set_enabled (config_page->details->inventory_service,
						CORBA_TRUE, &ev);


	Trilobite_Eazel_Inventory_upload (config_page->details->inventory_service, &ev);

	CORBA_exception_free (&ev);

	nautilus_view_open_location_in_this_window (config_page->details->view, "eazel-services:/inventory");
}

static void
nautilus_inventory_config_page_initialize (NautilusInventoryConfigPage *config_page)
{
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *hbox;
	BonoboObjectClient *object;
	char *initial_machine_name;
	gboolean initial_warn;

	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	config_page->details = g_new0 (NautilusInventoryConfigPageDetails, 1);

	object = bonobo_object_activate (SERVICE_IID, 0);

	config_page->details->inventory_service = bonobo_object_query_interface 
		(BONOBO_OBJECT (object), "IDL:Trilobite/Eazel/Inventory:1.0");

	bonobo_object_unref (BONOBO_OBJECT (object));

	label = gtk_label_new (_("Eazel Inventory Configuration"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (config_page), label, FALSE, FALSE, 0);
	
	label = gtk_label_new (_("blah blah blah"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (config_page), label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (config_page), hbox, FALSE, FALSE, 0);

	label = gtk_label_new (_("Machine Name:"));
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, 0);

	config_page->details->machine_entry = gtk_entry_new ();
	initial_machine_name = Trilobite_Eazel_Inventory__get_machine_name 
					(config_page->details->inventory_service, &ev);

	g_print ("initial machine name = `%s'\n", initial_machine_name);

	if ( (initial_machine_name == NULL) || (initial_machine_name[0] == '\n') ) {
		/* the user has never specified a name for this machine */
		g_print ("no name selected yet\n");

		/* TODO: perhaps guess an intial machine name based on host
		 * name...
		 */
		
	} else {
		gtk_entry_set_text (GTK_ENTRY(config_page->details->machine_entry), 
			initial_machine_name);
	}

	gtk_widget_show (config_page->details->machine_entry);
	gtk_box_pack_start (GTK_BOX (hbox), config_page->details->machine_entry, FALSE, FALSE, 0);

	config_page->details->warn_check_button = gtk_check_button_new_with_label (_("Warn me before upload"));
	initial_warn = Trilobite_Eazel_Inventory__get_warn_before_upload
                                        (config_page->details->inventory_service, &ev);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (config_page->details->warn_check_button), initial_warn);
	gtk_widget_show (config_page->details->warn_check_button);
	gtk_box_pack_start (GTK_BOX (config_page), config_page->details->warn_check_button, FALSE, FALSE, 0);
	
	button = gtk_button_new_with_label (_("Next"));
	gtk_widget_show (button);
	gtk_box_pack_start (GTK_BOX (config_page), button, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (button), "clicked", next_button_callback, config_page);
	
}


GtkWidget *
nautilus_inventory_config_page_new (NautilusView *view)
{
	NautilusInventoryConfigPage *config_page;

	config_page = NAUTILUS_INVENTORY_CONFIG_PAGE (gtk_widget_new (nautilus_inventory_config_page_get_type (), NULL));

	config_page->details->view = view;

	return GTK_WIDGET (config_page);
}


static void
nautilus_inventory_config_page_destroy (GtkObject *object)
{
	NautilusInventoryConfigPage *page = NAUTILUS_INVENTORY_CONFIG_PAGE (object);
	
	g_free (page->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}
