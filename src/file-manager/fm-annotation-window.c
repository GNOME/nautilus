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
#include <gtk/gtkvbox.h>
#include <libgnome/gnome-defs.h>
#include <libgnome/gnome-i18n.h>
#include <libgnomeui/gnome-dialog.h>
#include <libgnomeui/gnome-uidefs.h>
#include <libgnomevfs/gnome-vfs.h>
#include <libnautilus-extensions/nautilus-customization-data.h>
#include <libnautilus-extensions/nautilus-ellipsizing-label.h>
#include <libnautilus-extensions/nautilus-entry.h>
#include <libnautilus-extensions/nautilus-file-attributes.h>
#include <libnautilus-extensions/nautilus-file-utilities.h>
#include <libnautilus-extensions/nautilus-gdk-pixbuf-extensions.h>
#include <libnautilus-extensions/nautilus-glib-extensions.h>
#include <libnautilus-extensions/nautilus-global-preferences.h>
#include <libnautilus-extensions/nautilus-gnome-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-extensions.h>
#include <libnautilus-extensions/nautilus-gtk-macros.h>
#include <libnautilus-extensions/nautilus-icon-factory.h>
#include <libnautilus-extensions/nautilus-image.h>
#include <libnautilus-extensions/nautilus-link.h>
#include <libnautilus-extensions/nautilus-metadata.h>
#include <libnautilus-extensions/nautilus-stock-dialogs.h>
#include <libnautilus-extensions/nautilus-string.h>
#include <libnautilus-extensions/nautilus-undo-signal-handlers.h>
#include <libnautilus/nautilus-undo.h>
#include <libnautilus-extensions/nautilus-wrap-table.h>
#include <libnautilus-extensions/nautilus-labeled-image.h>
#include <libnautilus-extensions/nautilus-viewport.h>
#include <string.h>

struct FMAnnotationWindowDetails {
	NautilusFile *file;
	GtkWidget *file_icon;
	GtkLabel *file_title;	
};


static void real_destroy                          (GtkObject               *object);
static void fm_annotation_window_initialize_class (FMAnnotationWindowClass *class);
static void fm_annotation_window_initialize       (FMAnnotationWindow      *window);
static FMAnnotationWindow* create_annotation_window
						  (NautilusFile		   *file,
						   FMDirectoryView	   *directroy_view);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (FMAnnotationWindow, fm_annotation_window, GTK_TYPE_WINDOW)

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
	
	NAUTILUS_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}
	
static GdkPixbuf *
get_pixbuf_for_annotation_window (NautilusFile *file)
{
	g_assert (NAUTILUS_IS_FILE (file));
		
	return nautilus_icon_factory_get_pixbuf_for_file (file, NULL, NAUTILUS_ICON_SIZE_STANDARD, TRUE);
}


static void
update_annotation_window_icon (NautilusImage *image)
{
	GdkPixbuf	*pixbuf;
	NautilusFile	*file;

	g_assert (NAUTILUS_IS_IMAGE (image));

	file = gtk_object_get_data (GTK_OBJECT (image), "nautilus_file");

	g_assert (NAUTILUS_IS_FILE (file));
	
	pixbuf = get_pixbuf_for_annotation_window (file);

	nautilus_image_set_pixbuf (image, pixbuf);
	
	gdk_pixbuf_unref (pixbuf);
}

static GtkWidget *
create_image_widget_for_file (NautilusFile *file)
{
 	GtkWidget *image;
	GdkPixbuf *pixbuf;
	
	pixbuf = get_pixbuf_for_annotation_window (file);
	
	image = nautilus_image_new (NULL);

	nautilus_image_set_pixbuf (NAUTILUS_IMAGE (image), pixbuf);

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

/*
static void
annotation_window_file_changed_callback (GtkWindow *window, NautilusFile *file)
{
	g_assert (GTK_IS_WINDOW (window));
	g_assert (NAUTILUS_IS_FILE (file));

	if (nautilus_file_is_gone (file)) {
		gtk_widget_destroy (GTK_WIDGET (window));
	} else {
		update_annotation_window_title (window, file);
	}
}
*/

static FMAnnotationWindow *
create_annotation_window (NautilusFile *file,  FMDirectoryView *directory_view)
{
	FMAnnotationWindow *window;
	GtkWidget *hbox, *content_box;
	GtkWidget *label;
	char *file_name, *title;
	
	window = FM_ANNOTATION_WINDOW (gtk_widget_new (fm_annotation_window_get_type (), NULL));

	window->details->file = nautilus_file_ref (file);
	
  	gtk_container_set_border_width (GTK_CONTAINER (window), GNOME_PAD);
  	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);
	gtk_window_set_wmclass (GTK_WINDOW (window), "file_annotation", "Nautilus");

	/* allocate a vbox to hold the contents of the annotation window */
	content_box = gtk_vbox_new (FALSE, GNOME_PAD);
	gtk_container_add (GTK_CONTAINER (window), content_box);
	
	/* allocate an hbox to hold the icon and the title */
	hbox = gtk_hbox_new (FALSE, GNOME_PAD);
	gtk_box_pack_start (GTK_BOX (content_box), hbox, FALSE, FALSE, 0);
	 
	/* allocate an icon and title */
	window->details->file_icon = create_image_widget_for_file (window->details->file);
	gtk_box_pack_start (GTK_BOX (hbox), window->details->file_icon, FALSE, FALSE, GNOME_PAD);

	file_name = nautilus_file_get_name (window->details->file);
	title = g_strdup_printf (_("Add Annotation to\n%s"), file_name);
	label = gtk_label_new (title);
	gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
	g_free (file_name);
	g_free (title);
	
	gtk_box_pack_start (GTK_BOX (hbox), label, FALSE, FALSE, GNOME_PAD);
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

