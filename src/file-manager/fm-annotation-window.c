/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fm-annotation-window.c - window that lets user modify file annotations

   Copyright (C) 2001 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Andy Hertzfeld <andy@eazel.com>
*/

#include <config.h>
#include "fm-annotation-window.h"

#include "fm-error-reporting.h"
#include <gtk/gtkfilesel.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtknotebook.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkpixmap.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtktable.h>
#include <gtk/gtktext.h>
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-extensions/nautilus-annotation.h>
#include <libnautilus-extensions/nautilus-customization-data.h>
#include <eel/eel-ellipsizing-label.h>
#include <libnautilus-extensions/nautilus-entry.h>
#include <libnautilus-extensions/nautilus-file-attributes.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-font-factory.h>
#include <eel/eel-gdk-pixbuf-extensions.h>
#include <eel/eel-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <eel/eel-gnome-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <eel/eel-image.h>
#include <libnautilus-extensions/nautilus-link.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <eel/eel-stock-dialogs.h>
#include <eel/eel-string.h>
#include <libnautilus-extensions/nautilus-undo-signal-handlers.h>
#include <libnautilus/nautilus-undo.h>
#include <eel/eel-wrap-table.h>
#include <eel/eel-labeled-image.h>
#include <eel/eel-viewport.h>
#include <string.h>

struct FMAnnotationWindowDetails {
	NautilusFile *file;
	GtkWidget *file_icon;
	GtkLabel  *file_title;	
	GtkWidget *text_field;
};


static void real_destroy                          (GtkObject               *object);
static void fm_annotation_window_initialize_class (FMAnnotationWindowClass *class);
static void fm_annotation_window_initialize       (FMAnnotationWindow      *window);
static FMAnnotationWindow* create_annotation_window
						  (NautilusFile		   *file,
						   FMDirectoryView	   *directroy_view);

EEL_DEFINE_CLASS_BOILERPLATE (FMAnnotationWindow, fm_annotation_window, GNOME_TYPE_DIALOG)

static void
fm_annotation_window_initialize_class (FMAnnotationWindowClass *class)
{
	GtkObjectClass *object_class;

	object_class = GTK_OBJECT_CLASS (class);
	object_class->destroy = real_destroy;
}

static void
fm_annotation_window_initialize (FMAnnotationWindow *window)
{
	window->details = g_new0 (FMAnnotationWindowDetails, 1);
	window->details->file = NULL;
}

static void
real_destroy (GtkObject *object)
{
	FMAnnotationWindow *window;

	window = FM_ANNOTATION_WINDOW (object);

	nautilus_file_unref (window->details->file);
	
	g_free (window->details);
	
	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}
	
static GdkPixbuf *
get_pixbuf_for_annotation_window (NautilusFile *file)
{
	g_assert (NAUTILUS_IS_FILE (file));
		
	return nautilus_icon_factory_get_pixbuf_for_file (file, NULL, NAUTILUS_ICON_SIZE_LARGE, TRUE);
}


static void
update_annotation_window_icon (EelImage *image)
{
	GdkPixbuf	*pixbuf;
	NautilusFile	*file;

	g_assert (EEL_IS_IMAGE (image));

	file = gtk_object_get_data (GTK_OBJECT (image), "nautilus_file");

	g_assert (NAUTILUS_IS_FILE (file));
	
	pixbuf = get_pixbuf_for_annotation_window (file);

	eel_image_set_pixbuf (image, pixbuf);
	
	gdk_pixbuf_unref (pixbuf);
}

static GtkWidget *
create_image_widget_for_file (NautilusFile *file)
{
 	GtkWidget *image;
	GdkPixbuf *pixbuf;
	
	pixbuf = get_pixbuf_for_annotation_window (file);
	
	image = eel_image_new (NULL);

	eel_image_set_pixbuf (EEL_IMAGE (image), pixbuf);

	gdk_pixbuf_unref (pixbuf);

	nautilus_file_ref (file);
	gtk_object_set_data_full (GTK_OBJECT (image),
				  "nautilus_file",
				  file,
				  (GtkDestroyNotify) nautilus_file_unref);

	gtk_signal_connect_object_while_alive (nautilus_icon_factory_get (),
					       "icons_changed",
					       update_annotation_window_icon,
					       GTK_OBJECT (image));

	gtk_signal_connect_object_while_alive (GTK_OBJECT (file),
					       "changed",
					       update_annotation_window_icon,
					       GTK_OBJECT (image));
	return image;
}


static void
update_annotation_window_title (GtkWindow *window, NautilusFile *file)
{
	char *name, *title;

	g_assert (NAUTILUS_IS_FILE (file));
	g_assert (GTK_IS_WINDOW (window));

	name = nautilus_file_get_name (file);
	title = g_strdup_printf (_("%s Annotation"), name);
  	gtk_window_set_title (window, title);

	g_free (name);	
	g_free (title);
}

/* create the option table for the annotation window */
static GtkWidget *
create_options_table (FMAnnotationWindow *window)
{
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *type_menu_vbox;
	GtkWidget *access_menu_vbox;
	GtkWidget *option_menu;
	GtkWidget *new_menu;
	GtkWidget *menu_item;
	
	table = gtk_table_new (2, 2, FALSE);

	label = gtk_label_new (_("Type:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1, GTK_FILL, GTK_FILL, 4, 4);

	/* type options will eventually be derived from an xml file, but there's only one
	 * for now
	 */
	type_menu_vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);	
	option_menu = gtk_option_menu_new ();
	gtk_box_pack_end (GTK_BOX (type_menu_vbox), option_menu, TRUE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), type_menu_vbox, 1, 2, 0, 1, GTK_FILL, GTK_FILL, 4, 4);
	
	new_menu = gtk_menu_new ();
	menu_item = gtk_menu_item_new_with_label (_("free form note"));
        gtk_widget_show (new_menu);
        gtk_widget_show (menu_item);
        gtk_menu_append (GTK_MENU (new_menu), menu_item);
        gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), new_menu);

	/* now make the access option menu */
	label = gtk_label_new (_("Access:"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_FILL, GTK_FILL, 4, 4);
	
	access_menu_vbox = gtk_vbox_new (FALSE, GNOME_PAD_SMALL);	
	option_menu = gtk_option_menu_new ();
	gtk_box_pack_end (GTK_BOX (access_menu_vbox), option_menu, TRUE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), access_menu_vbox, 1, 2, 1, 2, GTK_FILL, GTK_FILL, 4, 4);

	new_menu = gtk_menu_new ();
	menu_item = gtk_menu_item_new_with_label (_("keep local"));
        gtk_widget_show (new_menu);
        gtk_widget_show (menu_item);
        gtk_menu_append (GTK_MENU (new_menu), menu_item);
        gtk_option_menu_set_menu (GTK_OPTION_MENU (option_menu), new_menu);

	menu_item = gtk_menu_item_new_with_label (_("share globally"));
        gtk_menu_append (GTK_MENU (new_menu), menu_item);

	return table;
}

/* this callback is invoked when the OK or Cancel buttons are clicked, to add an annotation and dismiss the window */
static void
annotation_clicked_callback (GtkWidget *dialog, int which_button, gpointer user_data)
{
	char *notes_text;
	FMAnnotationWindow *window;
	
	window = FM_ANNOTATION_WINDOW (dialog);
	
	if (which_button == GNOME_OK) {
		notes_text = gtk_editable_get_chars (GTK_EDITABLE (window->details->text_field), 0 , -1);
		
		/* type and access fields hard-wired for now */
		nautilus_annotation_add_annotation (window->details->file,
						    "text",
						    notes_text,
						    "local"); 
		g_free (notes_text);
	}
	gtk_widget_destroy (dialog);
}

/* create the annotation window, and allocate its widgets */
static FMAnnotationWindow *
create_annotation_window (NautilusFile *file,  FMDirectoryView *directory_view)
{
	FMAnnotationWindow *window;
	GtkWidget *hbox;
	GtkBox *content_box;
	GtkWidget *label;
	GtkWidget *table;
	char *file_name, *title;
	GdkFont *font;
	int position;
	char *annotation_text;
	
	window = FM_ANNOTATION_WINDOW (gtk_widget_new (fm_annotation_window_get_type (), NULL));

	window->details->file = nautilus_file_ref (file);
	
  	gtk_container_set_border_width (GTK_CONTAINER (window), GNOME_PAD);
  	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);
	gnome_dialog_set_default( GNOME_DIALOG (window), GNOME_OK);
	gtk_window_set_wmclass (GTK_WINDOW (window), "file_annotation", "Nautilus");

	/* add the buttons */
	gnome_dialog_append_buttons  (GNOME_DIALOG (window), GNOME_STOCK_BUTTON_OK, GNOME_STOCK_BUTTON_CANCEL, NULL);

	gtk_signal_connect (GTK_OBJECT (window), "clicked", (GtkSignalFunc) annotation_clicked_callback, NULL);

	/* get the container box of the dialog */
	content_box = GTK_BOX (GNOME_DIALOG (window)->vbox);

	/* allocate an hbox to hold the icon and the title */
	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (content_box), hbox, FALSE, FALSE, GNOME_PAD);
	 
	/* allocate an icon and title */
	window->details->file_icon = create_image_widget_for_file (window->details->file);
	gtk_box_pack_start (GTK_BOX (hbox), window->details->file_icon, FALSE, FALSE, GNOME_PAD);

	file_name = nautilus_file_get_name (window->details->file);
	title = g_strdup_printf (_("Add Annotation to %s"), file_name);
	label = gtk_label_new (title);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, GNOME_PAD);
	g_free (file_name);
	g_free (title);	

	/* now allocate a table to hold and populate the option boxes */
	table = create_options_table (window);
	gtk_box_pack_start (GTK_BOX (content_box), table, FALSE, FALSE, GNOME_PAD);
	
	/* add the views as defined by the currently selected type */
	/* at first, there is only one hardwired type, so just add it here */
	window->details->text_field = gtk_text_new (NULL, NULL);
        
	font = nautilus_font_factory_get_font_from_preferences (14);
	eel_gtk_widget_set_font (window->details->text_field, font);
	gdk_font_unref (font);

        gtk_text_set_editable (GTK_TEXT (window->details->text_field), TRUE);	
        gtk_box_pack_start (GTK_BOX (content_box), window->details->text_field, TRUE, TRUE, 0);	

	/* set up the annotation field with the initial text, if any */
	annotation_text = nautilus_annotation_get_annotation_for_display (window->details->file);
	position = 0;
	if (annotation_text) {
        	gtk_editable_insert_text (GTK_EDITABLE (window->details->text_field),
                                  annotation_text,
                                  strlen (annotation_text),
                                  &position);

		g_free (annotation_text);
	}
	
	update_annotation_window_title (GTK_WINDOW (window), window->details->file);

	gtk_widget_show_all (GTK_WIDGET (window));
	return window;
}

void
fm_annotation_window_present (NautilusFile *file, FMDirectoryView *directory_view)
{
	g_return_if_fail (NAUTILUS_IS_FILE (file));
	g_return_if_fail (FM_IS_DIRECTORY_VIEW (directory_view));
	
	create_annotation_window (file, directory_view);
}

