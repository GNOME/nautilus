/* GNOME GUI Library
 * Copyright (C) 1997, 1998 Jay Painter
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Cambridge, MA 02139, USA.
 */

#include <config.h>
#include <stdarg.h>
#include "gnome-messagebox.h"
#include <string.h> /* for strcmp */
#include <gtk/gtk.h>
#include <libtrilobite/trilobite-i18n.h>


#include "fake-stock.h"

#define GNOME_MESSAGE_BOX_WIDTH  425
#define GNOME_MESSAGE_BOX_HEIGHT 125


static void gnome_message_box_class_init (GnomeMessageBoxClass *klass);
static void gnome_message_box_init       (GnomeMessageBox      *messagebox);

static GnomeDialogClass *parent_class;

guint
gnome_message_box_get_type ()
{
	static guint message_box_type = 0;

	if (!message_box_type)
	{
		GtkTypeInfo message_box_info =
		{
			"GnomeMessageBox",
			sizeof (GnomeMessageBox),
			sizeof (GnomeMessageBoxClass),
			(GtkClassInitFunc) gnome_message_box_class_init,
			(GtkObjectInitFunc) gnome_message_box_init,
			(GtkArgSetFunc) NULL,
			(GtkArgGetFunc) NULL,
		};

		message_box_type = gtk_type_unique (gnome_dialog_get_type (), &message_box_info);
	}

	return message_box_type;
}

static void
gnome_message_box_class_init (GnomeMessageBoxClass *klass)
{
	parent_class = gtk_type_class (gnome_dialog_get_type ());
}

static void
gnome_message_box_init (GnomeMessageBox *message_box)
{
  
}

/**
 * gnome_message_box_new:
 * @message: The message to be displayed.
 * @message_box_type: The type of the message
 * @...: A NULL terminated list of strings to use in each button.
 *
 * Creates a dialog box of type @message_box_type with @message.  A number
 * of buttons are inserted on it.  You can use the GNOME stock identifiers
 * to create gnome-stock-buttons.
 *
 * Returns a widget that has the dialog box.
 */
GtkWidget*
gnome_message_box_new (const gchar           *message,
		       const gchar           *message_box_type, ...)
{
	va_list ap;
	GnomeMessageBox *message_box;
	GtkWidget *label, *hbox;
	GtkWidget *pixmap = NULL;
	GtkWidget *alignment;
	GtkStyle *style;

	va_start (ap, message_box_type);
	
	message_box = gtk_type_new (gnome_message_box_get_type ());

	style = gtk_widget_get_style (GTK_WIDGET (message_box));

	gtk_window_set_title (GTK_WINDOW (message_box), _("Question"));
	pixmap = fake_stock_pixmap_new_from_xpm_data (gnome_question_xpm);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX(GNOME_DIALOG(message_box)->vbox),
			    hbox, TRUE, TRUE, 10);
	gtk_widget_show (hbox);

	if (pixmap) {
		gtk_box_pack_start (GTK_BOX (hbox), 
				    pixmap, FALSE, TRUE, 0);
		gtk_widget_show (pixmap);
	} 

	label = gtk_label_new (message);
	gtk_label_set_justify (GTK_LABEL (label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_padding (GTK_MISC (label), GNOME_PAD, 0);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	/* Add some extra space on the right to balance the pixmap */
	if (pixmap) {
		alignment = gtk_alignment_new (0., 0., 0., 0.);
		gtk_widget_set_usize (alignment, GNOME_PAD, -1);
		gtk_widget_show (alignment);
		
		gtk_box_pack_start (GTK_BOX (hbox), alignment, FALSE, FALSE, 0);
	}
	
	while (TRUE) {
	  gchar * button_name;

	  button_name = va_arg (ap, gchar *);
	  
	  if (button_name == NULL) {
	    break;
	  }
	  
	  gnome_dialog_append_button ( GNOME_DIALOG(message_box), 
				       button_name);
	};
	
	va_end (ap);

	if (g_list_length (GNOME_DIALOG (message_box)->buttons) > 0) {
		gtk_widget_grab_focus (g_list_last (GNOME_DIALOG (message_box)->buttons)->data);
	}

	gnome_dialog_set_close ( GNOME_DIALOG(message_box),
				 TRUE );

	return GTK_WIDGET (message_box);
}

/**
 * gnome_message_box_newv:
 * @message: The message to be displayed.
 * @message_box_type: The type of the message
 * @buttons: a NULL terminated array with the buttons to insert.
 *
 * Creates a dialog box of type @message_box_type with @message.  A number
 * of buttons are inserted on it, the messages come from the @buttons array.
 * You can use the GNOME stock identifiers to create gnome-stock-buttons.
 *
 * Returns a widget that has the dialog box.
 */
GtkWidget*
gnome_message_box_newv (const gchar           *message,
		        const gchar           *message_box_type,
			const gchar 	     **buttons)
{
	GnomeMessageBox *message_box;
	GtkWidget *label, *hbox;
	GtkWidget *pixmap = NULL;
	GtkStyle *style;
	gint i = 0;

	message_box = gtk_type_new (gnome_message_box_get_type ());

	style = gtk_widget_get_style (GTK_WIDGET (message_box));


	gtk_window_set_title (GTK_WINDOW (message_box), _("Question"));
	pixmap = fake_stock_pixmap_new_from_xpm_data (gnome_question_xpm);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX(GNOME_DIALOG(message_box)->vbox),
			    hbox, TRUE, TRUE, 10);
	gtk_widget_show (hbox);

	if (pixmap) {
		gtk_box_pack_start (GTK_BOX(hbox), 
				    pixmap, FALSE, TRUE, 0);
		gtk_widget_show (pixmap);
	}

	label = gtk_label_new (message);
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);

	while (buttons[i]) {
	  gnome_dialog_append_button ( GNOME_DIALOG(message_box), 
				       buttons[i]);
	  i++;
	};
	
	if (g_list_length (GNOME_DIALOG (message_box)->buttons) > 0) {
		gtk_widget_grab_focus (g_list_last (GNOME_DIALOG (message_box)->buttons)->data);
	}
	
	gnome_dialog_set_close ( GNOME_DIALOG(message_box),
				 TRUE );

	return GTK_WIDGET (message_box);
}

/* These two here for backwards compatibility */

void
gnome_message_box_set_modal (GnomeMessageBox     *message_box)
{
  g_warning("gnome_message_box_set_modal is deprecated.\n");
  gtk_window_set_modal(GTK_WINDOW(message_box),TRUE);
}

void
gnome_message_box_set_default (GnomeMessageBox     *message_box,
			       gint                button)
{
  g_warning("gnome_message_box_set_default is deprecated.\n");
  gnome_dialog_set_default(GNOME_DIALOG(message_box), button);

  if (g_list_length (GNOME_DIALOG (message_box)->buttons) > 0) {
	  gtk_widget_grab_focus (g_list_last (GNOME_DIALOG (message_box)->buttons)->data);
  }
}



