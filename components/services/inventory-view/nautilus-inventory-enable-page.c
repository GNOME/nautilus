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
#include "nautilus-inventory-view-private.h"
#include "eazel-services-header.h"
#include "eazel-services-extensions.h"

#include <gnome.h>
#include <gtk/gtkprogressbar.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus/nautilus-view.h>
#include <eazel-inventory.h>
#include <libnautilus-extensions/nautilus-background.h>


#undef DEBUG_MESSAGES

#ifdef DEBUG_MESSAGES
#define DEBUG_MSG(x)	g_print x
#else
#define DEBUG_MSG(x)
#endif

struct NautilusInventoryEnablePageDetails {
	EazelInventory *		inventory;
	GtkWidget *			progress_bar;
	GtkWidget *			label;
	NautilusInventoryView *		view;
	guint				timeout_id;
	gboolean			timeout_added;
};

static void     nautilus_inventory_enable_page_initialize_class    (NautilusInventoryEnablePageClass *klass);
static void     nautilus_inventory_enable_page_initialize          (NautilusInventoryEnablePage      *file);
static void     nautilus_inventory_enable_page_destroy             (GtkObject                        *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusInventoryEnablePage, nautilus_inventory_enable_page, GTK_TYPE_EVENT_BOX)

static void
nautilus_inventory_enable_page_initialize_class (NautilusInventoryEnablePageClass *klass)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (klass);
	
	object_class->destroy = nautilus_inventory_enable_page_destroy;
}

static int /*GtkFunction*/
callback_progress_update (gpointer data)
{
	NautilusInventoryEnablePage *enable_page;

	enable_page = NAUTILUS_INVENTORY_ENABLE_PAGE (data);

	g_return_val_if_fail (enable_page != NULL, 0);

	DEBUG_MSG (("%s: updating...\n", __FUNCTION__));

	/* I guess the progress bar only cares that the new value is different */
	/* (note that this won't collapse to 0, because 0.75 and 0.25 can be represented w/o rounding) */
	gtk_progress_set_value (GTK_PROGRESS (enable_page->details->progress_bar), 
		1.0 - gtk_progress_get_value (GTK_PROGRESS (enable_page->details->progress_bar)));

	return TRUE;
}

static void
nautilus_inventory_enable_page_initialize (NautilusInventoryEnablePage *enable_page)
{
	GtkWidget *header;
	GtkWidget *vbox_top;
	GtkWidget *align_outer;
	GtkWidget *align_progress;
	GtkWidget *aligned_box;
	
	enable_page->details = g_new0 (NautilusInventoryEnablePageDetails, 1);

	align_outer = gtk_alignment_new (0.5, 0.5, 1.0, 0.0);
	align_progress = gtk_alignment_new (0.5, 0.5, 0.20, 1.0);
	aligned_box = gtk_vbox_new (FALSE, 20);
	vbox_top = gtk_vbox_new (FALSE, 0);

	gtk_container_add (GTK_CONTAINER (enable_page), vbox_top);
	
        nautilus_background_set_color (nautilus_get_widget_background (GTK_WIDGET (enable_page)), 
        	EAZEL_SERVICES_BACKGROUND_COLOR_SPEC);

	header = eazel_services_header_title_new (_("Uploading your System Inventory..."));
	gtk_box_pack_start (GTK_BOX (vbox_top), header, FALSE, FALSE, 0);

	gtk_box_pack_start (GTK_BOX (vbox_top), align_outer, TRUE, TRUE, 0);

	gtk_container_add (GTK_CONTAINER (align_outer), aligned_box);

	enable_page->details->progress_bar = gtk_progress_bar_new ();
	gtk_progress_set_activity_mode (GTK_PROGRESS (enable_page->details->progress_bar), TRUE);
	gtk_progress_set_value (GTK_PROGRESS (enable_page->details->progress_bar), 0.75);

	enable_page->details->label = eazel_services_label_new (_("Please wait while we upload your System Inventory..."),
					       0,
					       0.5,
					       0.5,
					       0,
					       0,
					       NAUTILUS_RGB_COLOR_BLACK,
					       NAUTILUS_RGB_COLOR_WHITE,
					       NULL,
					       4,		/*relative size*/
					       TRUE);

	gtk_box_pack_start (GTK_BOX (aligned_box), enable_page->details->label, FALSE, FALSE, 0);

	gtk_container_add (GTK_CONTAINER (align_progress), enable_page->details->progress_bar);

	gtk_box_pack_start (GTK_BOX (aligned_box), align_progress, FALSE, FALSE, 0);

	gtk_widget_show_all (GTK_WIDGET (enable_page));
}


GtkWidget *
nautilus_inventory_enable_page_new (NautilusInventoryView *view)
{
	NautilusInventoryEnablePage *enable_page;

	enable_page = NAUTILUS_INVENTORY_ENABLE_PAGE (gtk_widget_new (nautilus_inventory_enable_page_get_type (), NULL));

	enable_page->details->view = view;

	return GTK_WIDGET (enable_page);
}

static int /*GtkFunction*/
callback_timeout_error_navigate (gpointer data)
{
	NautilusInventoryEnablePage *enable_page;

	enable_page = NAUTILUS_INVENTORY_ENABLE_PAGE (data);

	g_return_val_if_fail (enable_page != NULL, FALSE);

	DEBUG_MSG (("%s: navigating to '%s'\n", __FUNCTION__, "eazel:"));

	nautilus_view_open_location_in_this_window (NAUTILUS_VIEW (enable_page->details->view),
					    "eazel:");

	/* This ref was added when callback was schedule */
	gtk_object_unref (GTK_OBJECT (enable_page));

	return FALSE;
}

static void /*EazelInventoryDoneCallback*/
callback_eazel_inventory (EazelInventory *inventory,
	 		  gboolean succeeded,
	  		  gpointer data)
{
	NautilusInventoryEnablePage *enable_page;

	enable_page = NAUTILUS_INVENTORY_ENABLE_PAGE (data);

	g_return_if_fail (enable_page != NULL);

	DEBUG_MSG (("%s: inventory upload complete: %s\n", __FUNCTION__, succeeded ? "SUCCESS" : "FAILURE"));

	if (!GTK_OBJECT_DESTROYED (enable_page)) {
		gtk_timeout_remove (enable_page->details->timeout_id);
		enable_page->details->timeout_id = 0;
		enable_page->details->timeout_added = FALSE;
	}

	if (succeeded) {
		DEBUG_MSG (("%s: navigating to '%s'\n", __FUNCTION__, enable_page->details->view->details->next_uri));

		nautilus_view_open_location_in_this_window (NAUTILUS_VIEW (enable_page->details->view),
						    enable_page->details->view->details->next_uri);
	} else {
		nautilus_label_set_wrap (NAUTILUS_LABEL (enable_page->details->label), TRUE);
		nautilus_label_set_justify (NAUTILUS_LABEL (enable_page->details->label), GTK_JUSTIFY_CENTER);

		nautilus_label_set_text (NAUTILUS_LABEL (enable_page->details->label), _("I'm sorry, an error prevented your System Inventory from being uploaded\n"));
		/* Pause for impact */
		/* This ref is released in the callback function */
		gtk_object_ref (GTK_OBJECT (enable_page));
		gtk_timeout_add (3 * 1000, callback_timeout_error_navigate, enable_page);		
	}

	/* release the ref that was added when the callback was scheduled */
	gtk_object_unref (GTK_OBJECT (enable_page));
}

 
void
nautilus_inventory_enable_page_run (NautilusInventoryEnablePage *enable_page)
{
	CORBA_Environment ev;
	CORBA_exception_init (&ev);

	enable_page->details->inventory = eazel_inventory_get ();

	DEBUG_MSG (("%s: starting inventory upload\n", __FUNCTION__));

	eazel_inventory_set_enabled (enable_page->details->inventory,
				     TRUE);

	/* Released in callback */
	gtk_object_ref (GTK_OBJECT (enable_page));

	eazel_inventory_upload (enable_page->details->inventory,
				callback_eazel_inventory,
				enable_page);

	enable_page->details->timeout_id = gtk_timeout_add (80, callback_progress_update, enable_page);
	enable_page->details->timeout_added = TRUE;
} 

static void
nautilus_inventory_enable_page_destroy (GtkObject *object)
{
	NautilusInventoryEnablePage *enable_page;
	CORBA_Environment ev;

	enable_page = NAUTILUS_INVENTORY_ENABLE_PAGE (object);
	CORBA_exception_init (&ev);

	if (enable_page->details->timeout_added) {
		gtk_timeout_remove (enable_page->details->timeout_id);
	}

	if (enable_page->details->inventory != CORBA_OBJECT_NIL) {
		gtk_object_unref (GTK_OBJECT (enable_page->details->inventory));
	}
	g_free (enable_page->details);

	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

