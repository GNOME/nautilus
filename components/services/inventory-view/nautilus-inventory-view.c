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

/* nautilus-inventory-view.c - system inventory view
   component.  */

#include <config.h>
#include "nautilus-inventory-view.h"

#include "nautilus-inventory-enable-page.h"
#include "nautilus-inventory-disable-page.h"

#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus/nautilus-bonobo-ui.h>
#include <bonobo/bonobo-control.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-stock.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtksignal.h>

struct NautilusInventoryViewDetails {
	GtkWidget *notebook;
	GtkWidget *enable_page;
	GtkWidget *disable_page;
};

static void nautilus_inventory_view_initialize_class (NautilusInventoryViewClass *klass);
static void nautilus_inventory_view_initialize       (NautilusInventoryView      *view);
static void nautilus_inventory_view_destroy          (GtkObject                  *object);
static void inventory_load_location_callback         (NautilusView               *nautilus_view,
						      const char                 *location,
						      gpointer                    user_data);


NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusInventoryView,
				   nautilus_inventory_view,
				   NAUTILUS_TYPE_VIEW)

     
static void
nautilus_inventory_view_initialize_class (NautilusInventoryViewClass *klass)
{
	GtkObjectClass *object_class;
	
	g_assert (NAUTILUS_IS_INVENTORY_VIEW_CLASS (klass));

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_inventory_view_destroy;
}

static void
nautilus_inventory_view_initialize (NautilusInventoryView *view)
{
	g_assert (NAUTILUS_IS_INVENTORY_VIEW (view));

	view->details = g_new0 (NautilusInventoryViewDetails, 1);
	
	view->details->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_border (GTK_NOTEBOOK (view->details->notebook), FALSE);
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (view->details->notebook), FALSE);
	

	view->details->enable_page = nautilus_inventory_enable_page_new (NAUTILUS_VIEW (view),
									 "eazel-services:/inventory/updates");
	view->details->disable_page = nautilus_inventory_disable_page_new (NAUTILUS_VIEW (view),
									 "eazel-services:/inventory/updates");
	gtk_notebook_append_page (GTK_NOTEBOOK (view->details->notebook),
				  view->details->enable_page,
				  gtk_label_new (""));
	gtk_notebook_append_page (GTK_NOTEBOOK (view->details->notebook),
				  view->details->disable_page,
				  gtk_label_new (""));

	gtk_widget_show_all (view->details->notebook);

	nautilus_view_construct (NAUTILUS_VIEW (view), 
				 view->details->notebook);
	
	gtk_signal_connect (GTK_OBJECT (view), 
			    "load_location",
			    inventory_load_location_callback, 
			    NULL);
}

static void
nautilus_inventory_view_destroy (GtkObject *object)
{
	NautilusInventoryView *view;
	
	view = NAUTILUS_INVENTORY_VIEW (object);
	
	g_free (view->details);

	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}




static void
inventory_load_location_callback (NautilusView *nautilus_view, 
				  const char *location,
				  gpointer user_data)
{
	NautilusInventoryView *view;
	
	g_assert (NAUTILUS_IS_VIEW (nautilus_view));
	g_assert (location != NULL);
	
	view = NAUTILUS_INVENTORY_VIEW (nautilus_view);
	
	nautilus_view_report_load_underway (nautilus_view);
	
	if (strcmp (location, "eazel-inventory:enable") == 0) {
		gtk_notebook_set_page (GTK_NOTEBOOK (view->details->notebook), 0);
		nautilus_view_report_load_complete (nautilus_view);
		nautilus_inventory_enable_page_run (NAUTILUS_INVENTORY_ENABLE_PAGE 
						    (view->details->enable_page));
	} else if (strcmp (location, "eazel-inventory:disable") == 0) {
		gtk_notebook_set_page (GTK_NOTEBOOK (view->details->notebook), 1);
		nautilus_view_report_load_complete (nautilus_view);
	} else {
		nautilus_view_report_load_failed (nautilus_view);
	}
}
