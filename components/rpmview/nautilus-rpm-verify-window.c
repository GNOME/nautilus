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

#include <eel/eel-gdk-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-label.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdk.h>
#include <gnome.h>
#include <gtk/gtksignal.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-util.h>
#include <libgnomeui/gnome-pixmap.h>
#include <math.h>

struct NautilusRPMVerifyWindowDetails {
	GtkWidget *package_name;
	GtkWidget *file_message;	
		
	gboolean error_mode;
	GtkWidget *continue_button;
	GtkWidget *cancel_button;

	unsigned long amount, total;
	char *current_file;
};

enum {
	CONTINUE,
	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL];

static void     nautilus_rpm_verify_window_initialize_class	(NautilusRPMVerifyWindowClass *klass);
static void     nautilus_rpm_verify_window_initialize		(NautilusRPMVerifyWindow *rpm_verify_window);
static void	nautilus_rpm_verify_window_destroy		(GtkObject *object);

EEL_DEFINE_CLASS_BOILERPLATE (NautilusRPMVerifyWindow, nautilus_rpm_verify_window, GNOME_TYPE_DIALOG)

static void
nautilus_rpm_verify_window_initialize_class (NautilusRPMVerifyWindowClass *rpm_verify_window_class)
{
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (rpm_verify_window_class);

	signals[CONTINUE]
		= gtk_signal_new ("continue",
				  GTK_RUN_LAST,
				  object_class->type,
				  GTK_SIGNAL_OFFSET (NautilusRPMVerifyWindowClass,
						     continue_verify),
				  gtk_marshal_NONE__NONE,
				  GTK_TYPE_NONE, 0);
	
	gtk_object_class_add_signals (object_class, signals, LAST_SIGNAL);
		
	object_class->destroy = nautilus_rpm_verify_window_destroy;
}

static void 
nautilus_rpm_verify_window_destroy (GtkObject *object)
{
	NautilusRPMVerifyWindow *rpm_verify_window;
	
	rpm_verify_window = NAUTILUS_RPM_VERIFY_WINDOW (object);
		
	g_free (NAUTILUS_RPM_VERIFY_WINDOW (object)->details);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* handle the continue button  */
static void
continue_button_callback (GtkWidget *widget, NautilusRPMVerifyWindow *rpm_verify_window)
{
	gtk_signal_emit (GTK_OBJECT (rpm_verify_window),
		signals[CONTINUE]);	
}

/* handle the cancel button */
static void
cancel_button_callback (GtkWidget *widget, NautilusRPMVerifyWindow *rpm_verify_window)
{
	nautilus_rpm_verify_window_set_error_mode (rpm_verify_window, FALSE);
	gnome_dialog_close (GNOME_DIALOG (rpm_verify_window));
	
}

/* initialize the rpm_verify_window */
static void
nautilus_rpm_verify_window_initialize (NautilusRPMVerifyWindow *rpm_verify_window)
{	
	GtkWidget *window_contents;
	GtkWidget *label, *button_box;
	
	rpm_verify_window->details = g_new0 (NautilusRPMVerifyWindowDetails, 1);
	rpm_verify_window->details->current_file = NULL;
	/* allocate a vbox to hold the contents */

	window_contents = gtk_vbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (GNOME_DIALOG(rpm_verify_window)->vbox), window_contents);
	gtk_widget_show (window_contents);
	
	/* allocate the package title label */
        label = eel_label_new ("");
	gtk_widget_show (label);
	eel_label_make_larger (EEL_LABEL (label), 2);
	eel_label_set_justify (EEL_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start (GTK_BOX (window_contents), label, FALSE, FALSE, 8);
	rpm_verify_window->details->package_name = label;
	
	/* allocate the message label */
        label = eel_label_new ("");
	gtk_widget_show (label);
	eel_label_set_justify (EEL_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start (GTK_BOX (window_contents), label, FALSE, FALSE, 8);
	rpm_verify_window->details->file_message = label;
	
	/* allocate the error mode buttons */
	button_box = gtk_hbox_new (FALSE, 2);
	gtk_widget_show (button_box);
	gtk_box_pack_start (GTK_BOX (window_contents), button_box, FALSE, FALSE, 8);
	
	rpm_verify_window->details->continue_button = gtk_button_new_with_label (_("Continue"));
	gtk_box_pack_start (GTK_BOX (button_box), rpm_verify_window->details->continue_button, FALSE, FALSE, 4);
	gtk_signal_connect (GTK_OBJECT (rpm_verify_window->details->continue_button), "clicked",
			    GTK_SIGNAL_FUNC (continue_button_callback), rpm_verify_window);
	
	rpm_verify_window->details->cancel_button = gtk_button_new_with_label (_("Cancel"));
	gtk_box_pack_start (GTK_BOX (button_box), rpm_verify_window->details->cancel_button, FALSE, FALSE, 4);
	gtk_signal_connect (GTK_OBJECT (rpm_verify_window->details->cancel_button), "clicked",
			   GTK_SIGNAL_FUNC (cancel_button_callback), rpm_verify_window);
	
	/* configure the dialog */                                  
	gtk_widget_set_usize (GTK_WIDGET (rpm_verify_window), 420, 180);
	
	gnome_dialog_append_button ( GNOME_DIALOG(rpm_verify_window),
				     GNOME_STOCK_BUTTON_OK);
	
	gnome_dialog_set_close (GNOME_DIALOG(rpm_verify_window), TRUE);			
	gnome_dialog_close_hides (GNOME_DIALOG(rpm_verify_window), TRUE);			
}

/* allocate a new rpm_verify_window dialog */
GtkWidget *
nautilus_rpm_verify_window_new (const char *package_name)
{
	char *title_string;
	NautilusRPMVerifyWindow *rpm_verify_window;
	
	rpm_verify_window = NAUTILUS_RPM_VERIFY_WINDOW (gtk_widget_new (nautilus_rpm_verify_window_get_type (), NULL));
	
	/* set up the window title */
	title_string = g_strdup_printf (_("Verifying %s..."), package_name);
	gtk_window_set_title (GTK_WINDOW (rpm_verify_window), title_string);
	
	/* set up the package name */
	eel_label_set_text (EEL_LABEL (rpm_verify_window->details->package_name), title_string);
	g_free (title_string);
	
	return GTK_WIDGET (rpm_verify_window);
}

void
nautilus_rpm_verify_window_set_message (NautilusRPMVerifyWindow *window, const char *message)
{
	eel_label_set_text (EEL_LABEL (window->details->file_message), message);
        while (gtk_events_pending ()) {
                gtk_main_iteration ();
        }
}

static void
nautilus_rpm_verify_window_update_message (NautilusRPMVerifyWindow *window)
{
	char *message = NULL;
	if (window->details->error_mode) {
		message = g_strdup_printf (_("Failed on \"%s\""), window->details->current_file); 
	} else {
		/* TRANSLATORS: this is printed while verifying files from packages, 
		   %s is the filename, %d/%d is filenumber of total-number-of-files */
		message = g_strdup_printf (_("Checking \"%s\" (%ld/%ld)"), 
					   window->details->current_file, 
					   window->details->amount, 
					   window->details->total);
	}
	nautilus_rpm_verify_window_set_message (window, message);
	g_free (message);
}

void
nautilus_rpm_verify_window_set_progress (NautilusRPMVerifyWindow *window, 
					 const char *file, 
					 unsigned long amount, 
					 unsigned long total)
{
	g_free (window->details->current_file);
	window->details->current_file = g_strdup (file);
	window->details->amount = amount;
	window->details->total = total;
	nautilus_rpm_verify_window_update_message (window);
}

void
nautilus_rpm_verify_window_set_error_mode (NautilusRPMVerifyWindow *window, gboolean error_mode)
{
	if (window->details->error_mode != error_mode) {
		window->details->error_mode = error_mode;
		if (error_mode) {
			gtk_widget_show (window->details->continue_button);
			gtk_widget_show (window->details->cancel_button);
		} else {
			gtk_widget_hide (window->details->continue_button);
			gtk_widget_hide (window->details->cancel_button);
		}
		nautilus_rpm_verify_window_update_message (window);			
	}
}

