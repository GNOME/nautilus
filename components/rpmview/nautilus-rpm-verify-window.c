/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* 
 * Nautilus
 *
 * Copyright (C) 2000 Eazel, Inc.
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
 * Author: Andy Hertzfeld <andy@eazel.com>
 *
 * This is the implementation for the rpm verify window dialog
 *
 */

#include <config.h>
#include "nautilus-rpm-verify-window.h"

#include <libgnome/gnome-defs.h>

#include <math.h>
#include <gnome.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-pixmap.h>
#include <libnautilus-extensions/nautilus-gdk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-label.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-scalable-font.h>
#include <libnautilus-extensions/nautilus-theme.h>

struct NautilusRPMVerifyWindowDetails {
	GtkWidget *package_name;
	GtkWidget *file_message;	
	GtkWidget *continue_button;
	GtkWidget *cancel_button;
};

static void     nautilus_rpm_verify_window_initialize_class	(NautilusRPMVerifyWindowClass *klass);
static void     nautilus_rpm_verify_window_initialize		(NautilusRPMVerifyWindow *rpm_verify_window);
static void	nautilus_rpm_verify_window_destroy		(GtkObject *object);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusRPMVerifyWindow, nautilus_rpm_verify_window, GNOME_TYPE_DIALOG)

static void
nautilus_rpm_verify_window_initialize_class (NautilusRPMVerifyWindowClass *rpm_verify_window_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (rpm_verify_window_class);
		
	object_class->destroy = nautilus_rpm_verify_window_destroy;
}

static void 
nautilus_rpm_verify_window_destroy (GtkObject *object)
{
	NautilusRPMVerifyWindow *rpm_verify_window;
	
	rpm_verify_window = NAUTILUS_RPM_VERIFY_WINDOW (object);
		
	g_free (NAUTILUS_RPM_VERIFY_WINDOW (object)->details);
	
	NAUTILUS_CALL_PARENT_CLASS (GTK_OBJECT_CLASS, destroy, (object));
}


/* initialize the rpm_verify_window */
static void
nautilus_rpm_verify_window_initialize (NautilusRPMVerifyWindow *rpm_verify_window)
{	
	GtkWidget *window_contents;
	GtkWidget *label;
	
	rpm_verify_window->details = g_new0 (NautilusRPMVerifyWindowDetails, 1);
	
	/* allocate a vbox to hold the contents */

	window_contents = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (GNOME_DIALOG(rpm_verify_window)->vbox), window_contents);
	gtk_widget_show (window_contents);
	
	/* allocate the package title label */
        label = nautilus_label_new ("");
	gtk_widget_show (label);
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), 20);
	nautilus_label_set_text_justification (NAUTILUS_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start (GTK_BOX (window_contents), label, FALSE, FALSE, 8);
	rpm_verify_window->details->package_name = label;
	
	/* allocate the message label */
        label = nautilus_label_new ("");
	gtk_widget_show (label);
	nautilus_label_set_font_size (NAUTILUS_LABEL (label), 14);
	nautilus_label_set_text_justification (NAUTILUS_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start (GTK_BOX (window_contents), label, FALSE, FALSE, 8);
	rpm_verify_window->details->file_message = label;
	
	/* configure the dialog */                                  
	gtk_widget_set_usize (GTK_WIDGET (rpm_verify_window), 200, 140);
	
	gnome_dialog_append_button ( GNOME_DIALOG(rpm_verify_window),
				     GNOME_STOCK_BUTTON_OK);
	
	gnome_dialog_set_close (GNOME_DIALOG(rpm_verify_window), TRUE);			
	gnome_dialog_close_hides (GNOME_DIALOG(rpm_verify_window), TRUE);			
}

/* allocate a new rpm_verify_window dialog */
GtkWidget*
nautilus_rpm_verify_window_new (const char *package_name)
{
	char *title_string;
	NautilusRPMVerifyWindow *rpm_verify_window;
	
	rpm_verify_window = gtk_type_new (nautilus_rpm_verify_window_get_type ());
	
	/* set up the window title */
	title_string = g_strdup_printf (_("Verifying %s..."), package_name);
	gtk_window_set_title (GTK_WINDOW (rpm_verify_window), title_string);
	
	/* set up the package name */
	nautilus_label_set_text (NAUTILUS_LABEL (rpm_verify_window->details->package_name), title_string);
	g_free (title_string);
	
	return GTK_WIDGET (rpm_verify_window);
}

void
nautilus_rpm_verify_window_set_message (NautilusRPMVerifyWindow *window, const char *message)
{
	nautilus_label_set_text (NAUTILUS_LABEL (window->details->file_message), message);
}
