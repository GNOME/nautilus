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

#include <gtk/gtklabel.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtk.h>
#include <gnome.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-background.h>
#include <libnautilus/nautilus-view.h>
#include <eazel-inventory.h>

struct NautilusInventoryDisablePageDetails {
	EazelInventory *inventory;
	GtkWidget    *progress_bar;
	GtkWidget    *label;
	NautilusView *nautilus_view;
	char         *next_uri;
};

static void     nautilus_inventory_disable_page_initialize_class    (NautilusInventoryDisablePageClass *klass);
static void     nautilus_inventory_disable_page_initialize          (NautilusInventoryDisablePage      *file);
static void     nautilus_inventory_disable_page_destroy             (GtkObject                        *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusInventoryDisablePage, nautilus_inventory_disable_page, GTK_TYPE_EVENT_BOX)

static void
nautilus_inventory_disable_page_initialize_class (NautilusInventoryDisablePageClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_inventory_disable_page_destroy;
}

static void
yes_callback (GtkWidget *button, gpointer data)
{
	NautilusInventoryDisablePage *disable_page = NAUTILUS_INVENTORY_DISABLE_PAGE (data);

	eazel_inventory_set_enabled (disable_page->details->inventory,
				     FALSE);

	nautilus_view_open_location_in_this_window (disable_page->details->nautilus_view, 
			"eazel:" /*FIXME*/);

}

static void
no_callback (GtkWidget *button, gpointer data)
{
	NautilusInventoryDisablePage *disable_page = NAUTILUS_INVENTORY_DISABLE_PAGE (data);

	nautilus_view_go_back (disable_page->details->nautilus_view);

}


static void
nautilus_inventory_disable_page_initialize (NautilusInventoryDisablePage *disable_page)
{
	NautilusBackground *background;
	GtkWidget *header;
	GtkWidget *vbox;
	GtkWidget *buttonbox;
	GtkWidget *yes, *no;

	vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (disable_page), vbox);

	background = nautilus_get_widget_background (GTK_WIDGET (disable_page));
        nautilus_background_set_color (background, EAZEL_SERVICES_BACKGROUND_COLOR_SPEC);

	disable_page->details = g_new0 (NautilusInventoryDisablePageDetails, 1);

	disable_page->details->inventory = eazel_inventory_get ();

	header = eazel_services_header_title_new (_("Disable Inventory [FIXME]"));
	gtk_widget_show (header);
	gtk_box_pack_start (GTK_BOX (vbox), header, FALSE, FALSE, 0);
	
	disable_page->details->label = gtk_label_new (_("An explanation of how why and what we're doing here."));
	gtk_widget_show (disable_page->details->label);
	gtk_box_pack_start (GTK_BOX (vbox), disable_page->details->label, FALSE, FALSE, 0);

	/*
	disable_page->details->progress_bar = gtk_progress_bar_new ();
	gtk_widget_show (disable_page->details->progress_bar);
	gtk_box_pack_start (GTK_BOX (vbox), disable_page->details->progress_bar, FALSE, FALSE, 0);
	*/
	
	buttonbox = gtk_hbutton_box_new ();
	gtk_button_box_set_layout (GTK_BUTTON_BOX (buttonbox),
			GTK_BUTTONBOX_SPREAD);
	gtk_widget_show (buttonbox);
	gtk_box_pack_start (GTK_BOX (vbox), buttonbox, FALSE, FALSE, 0);

	/* FIXME use stock buttons? */
	yes = gtk_button_new_with_label (_("Yes"));
	gtk_widget_show (yes);
	gtk_signal_connect (GTK_OBJECT (yes), "clicked", 
			GTK_SIGNAL_FUNC (yes_callback), disable_page);
	gtk_container_add (GTK_CONTAINER (buttonbox), yes);

	no = gtk_button_new_with_label (_("No"));
	gtk_widget_show (no);
	gtk_signal_connect (GTK_OBJECT (no), "clicked", 
			GTK_SIGNAL_FUNC (no_callback), disable_page);
	gtk_container_add (GTK_CONTAINER (buttonbox), no);

}


GtkWidget *
nautilus_inventory_disable_page_new (NautilusView *view,
				    const char   *next_uri)
{
	NautilusInventoryDisablePage *disable_page;

	disable_page = NAUTILUS_INVENTORY_DISABLE_PAGE (gtk_widget_new (nautilus_inventory_disable_page_get_type (), NULL));

	disable_page->details->nautilus_view = view;
	disable_page->details->next_uri = g_strdup (next_uri);

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
	g_free (page->details->next_uri);
	g_free (page->details);

	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

