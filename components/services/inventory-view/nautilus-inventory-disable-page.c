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
 * Authors: Maciej Stachowiak <mjs@eazel.com>
 *          Ian McKellar <ian@eazel.com>
 */

/* nautilus-inventory-disable-page.h - 
 */

#include <config.h>

#include "nautilus-inventory-disable-page.h"
#include "eazel-services-header.h"
#include "eazel-services-extensions.h"
#include "nautilus-inventory-view-private.h"

#include <gtk/gtklabel.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-label.h>
#include <eel/eel-background.h>
#include <libnautilus/nautilus-view.h>
#include <eazel-inventory.h>

struct NautilusInventoryDisablePageDetails {
	EazelInventory 		*inventory;
	NautilusInventoryView 	*view;
};

static void     nautilus_inventory_disable_page_initialize_class    (NautilusInventoryDisablePageClass *klass);
static void     nautilus_inventory_disable_page_initialize          (NautilusInventoryDisablePage      *file);
static void     nautilus_inventory_disable_page_destroy             (GtkObject                        *object);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusInventoryDisablePage, nautilus_inventory_disable_page, GTK_TYPE_EVENT_BOX)

static void
nautilus_inventory_disable_page_initialize_class (NautilusInventoryDisablePageClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_inventory_disable_page_destroy;
}

static gint /*GtkFunction*/
disable_inventory_callback (gpointer data)
{
	NautilusInventoryDisablePage *disable_page = NAUTILUS_INVENTORY_DISABLE_PAGE (data);

	g_return_val_if_fail (disable_page != NULL, FALSE);

	eazel_inventory_set_enabled (disable_page->details->inventory, FALSE);

	nautilus_view_open_location_in_this_window (NAUTILUS_VIEW (disable_page->details->view),
			disable_page->details->view->details->next_uri);

	/* Unref the ref that was added when the callback was scheduled */ 
	gtk_object_unref (GTK_OBJECT (disable_page));

	return FALSE;
}

static void
nautilus_inventory_disable_page_initialize (NautilusInventoryDisablePage *disable_page)
{
	EelBackground *background;
	GtkWidget *header;
	GtkWidget *vbox;
	GtkWidget *label;

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (disable_page), vbox);

	background = eel_get_widget_background (GTK_WIDGET (disable_page));
        eel_background_set_color (background, EAZEL_SERVICES_BACKGROUND_COLOR_SPEC);

	disable_page->details = g_new0 (NautilusInventoryDisablePageDetails, 1);

	disable_page->details->inventory = eazel_inventory_get ();

	header = eazel_services_header_title_new (_("Disabling Inventory..."));
	gtk_widget_show (header);
	gtk_box_pack_start (GTK_BOX (vbox), header, FALSE, FALSE, 0);

	label = eazel_services_label_new (_("Please wait while we disable Eazel Inventory..."),
					       0,
					       0.5,
					       0.5,
					       0,
					       0,
					       EEL_RGB_COLOR_BLACK,
					       EEL_RGB_COLOR_WHITE,
					       NULL,
					       4,		/*relative size*/
					       TRUE);
	
	gtk_widget_show (label);
	gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);
}


GtkWidget *
nautilus_inventory_disable_page_new (NautilusInventoryView *view)
{
	NautilusInventoryDisablePage *disable_page;

	disable_page = NAUTILUS_INVENTORY_DISABLE_PAGE (gtk_widget_new (nautilus_inventory_disable_page_get_type (), NULL));

	disable_page->details->view = view;

	return GTK_WIDGET (disable_page);
}

static void
nautilus_inventory_disable_page_destroy (GtkObject *object)
{
	NautilusInventoryDisablePage *page;
	CORBA_Environment ev;

	page = NAUTILUS_INVENTORY_DISABLE_PAGE (object);
	CORBA_exception_init (&ev);

	if (page->details->inventory != CORBA_OBJECT_NIL) {
		gtk_object_unref (GTK_OBJECT (page->details->inventory));
	}
	g_free (page->details);

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

void
nautilus_inventory_disable_page_run (NautilusInventoryDisablePage *disable_page)
{
	/* Pause for impact */
	/* This ref is released in the callback function */
	gtk_object_ref (GTK_OBJECT (disable_page));
	gtk_timeout_add (3 * 1000, disable_inventory_callback, disable_page);
}

