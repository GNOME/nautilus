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

/* nautilus-inventory-enable-page.h - 
 */

#include <config.h>

#include "nautilus-inventory-enable-page.h"
#include <gtk/gtklabel.h>
#include <gtk/gtkprogressbar.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-view.h>
#include <eazel-inventory.h>

struct NautilusInventoryEnablePageDetails {
	EazelInventory *inventory;
	GtkWidget    *progress_bar;
	GtkWidget    *label;
	NautilusView *view;
	char         *next_uri;
};

static void     nautilus_inventory_enable_page_initialize_class    (NautilusInventoryEnablePageClass *klass);
static void     nautilus_inventory_enable_page_initialize          (NautilusInventoryEnablePage      *file);
static void     nautilus_inventory_enable_page_destroy             (GtkObject                        *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusInventoryEnablePage, nautilus_inventory_enable_page, GTK_TYPE_VBOX)

static void
nautilus_inventory_enable_page_initialize_class (NautilusInventoryEnablePageClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_inventory_enable_page_destroy;
}


static void
nautilus_inventory_enable_page_initialize (NautilusInventoryEnablePage *enable_page)
{
	enable_page->details = g_new0 (NautilusInventoryEnablePageDetails, 1);


	enable_page->details->progress_bar = gtk_progress_bar_new ();
	gtk_widget_show (enable_page->details->progress_bar);
	gtk_box_pack_start (GTK_BOX (enable_page), enable_page->details->progress_bar, FALSE, FALSE, 0);

	enable_page->details->label = gtk_label_new ("Collecting package data...");
	gtk_widget_show (enable_page->details->label);
	gtk_box_pack_start (GTK_BOX (enable_page), enable_page->details->label, FALSE, FALSE, 0);
}


GtkWidget *
nautilus_inventory_enable_page_new (NautilusView *view,
				    const char   *next_uri)
{
	NautilusInventoryEnablePage *enable_page;

	enable_page = NAUTILUS_INVENTORY_ENABLE_PAGE (gtk_widget_new (nautilus_inventory_enable_page_get_type (), NULL));

	enable_page->details->view = view;
	enable_page->details->next_uri = g_strdup (next_uri);

	return GTK_WIDGET (enable_page);
}


static void
callback (EazelInventory *inventory,
	  gboolean succeeded,
	  NautilusInventoryEnablePage *enable_page)
{
	gtk_progress_bar_update (GTK_PROGRESS_BAR (enable_page->details->progress_bar), 100);


	if (succeeded) {
		nautilus_view_open_location_in_this_window (enable_page->details->view,
							    enable_page->details->next_uri);
	} else {
		nautilus_view_go_back (enable_page->details->view);
	}
}

 
void
nautilus_inventory_enable_page_run (NautilusInventoryEnablePage *enable_page)
{
	CORBA_Environment ev;
	CORBA_exception_init (&ev);

	enable_page->details->inventory = eazel_inventory_get ();

	eazel_inventory_set_enabled (enable_page->details->inventory,
				     TRUE);
	eazel_inventory_upload (enable_page->details->inventory,
				(gpointer) callback,
				enable_page);

} 

static void
nautilus_inventory_enable_page_destroy (GtkObject *object)
{
	NautilusInventoryEnablePage *page;
	CORBA_Environment ev;

	page = NAUTILUS_INVENTORY_ENABLE_PAGE (object);
	CORBA_exception_init (&ev);

	if (page->details->inventory != CORBA_OBJECT_NIL) {
		gtk_object_unref (GTK_OBJECT (page->details->inventory));
	}
	g_free (page->details->next_uri);
	g_free (page->details);

	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

