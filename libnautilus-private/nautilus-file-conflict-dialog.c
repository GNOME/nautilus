/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-conflict-dialog: dialog that handles file conflicts
   during transfer operations.

   Copyright (C) 2008, Cosimo Cecchi

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.
   
   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
   
   Authors: Cosimo Cecchi <cosimoc@gnome.org>
*/

#include <config.h>
#include "nautilus-file-conflict-dialog.h"

#include <glib-object.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <gtk/gtkmessagedialog.h>
#include "nautilus-file.h"
#include "nautilus-icon-info.h"

struct _NautilusFileConflictDialogDetails
{
	/* conflicting objects */
	GFile *source;
	GFile *destination;
	GFile *dest_dir;
	
	/* UI objects */
	GtkWidget *first_hbox;
	GtkWidget *second_hbox;
	GtkWidget *expander;
	GtkWidget *entry;
};

G_DEFINE_TYPE (NautilusFileConflictDialog,
	       nautilus_file_conflict_dialog,
	       GTK_TYPE_MESSAGE_DIALOG);

#define NAUTILUS_FILE_CONFLICT_DIALOG_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), NAUTILUS_TYPE_FILE_CONFLICT_DIALOG, NautilusFileConflictDialogDetails))

static gboolean
is_dir (GFile *file)
{
	GFileInfo *info;
	gboolean res;

	res = FALSE;
	info = g_file_query_info (file,
				  G_FILE_ATTRIBUTE_STANDARD_TYPE,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
				  NULL, NULL);
	if (info) {
		res = g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY;
		g_object_unref (info);
	}
	
	return res;
}

static void
build_dialog_appearance (NautilusFileConflictDialog *dialog)
{
	GtkMessageDialog *mdialog;
	gboolean source_is_dir;
	gboolean dest_is_dir;
	NautilusFileConflictDialogDetails *details;
	char *primary_text, *secondary_text;
	char *src_name, *dest_name, *dest_dir_name;
	char *label_text;
	char *size, *date;
	//GdkPixbuf *src_pixbuf, *dest_pixbuf;
	GtkWidget *src_image, *dest_image;
	GtkWidget *src_label, *dest_label;
	NautilusFile *src, *dest, *dest_dir;
	
	g_print ("build dialog appearance \n");
	
	mdialog = GTK_MESSAGE_DIALOG (dialog);
	details = dialog->details;
	source_is_dir = is_dir (details->source);
	dest_is_dir = is_dir (details->destination);

	src = nautilus_file_get (details->source);
	dest = nautilus_file_get (details->destination);
	dest_dir = nautilus_file_get (details->dest_dir);

	src_name = nautilus_file_get_display_name (src);
	dest_name = nautilus_file_get_display_name (dest);
	dest_dir_name = nautilus_file_get_display_name (dest_dir);

	/* Set up the right labels */
	if (dest_is_dir) {
		if (source_is_dir) {
			primary_text = g_strdup_printf
				(_("A folder named \"%s\" already exists.  "
				   "Do you want to merge the source folder?"),
				 dest_name);
			secondary_text = g_strdup_printf
				(_("The source folder already exists in \"%s\".  "
				   "Merging will ask for confirmation before "
				   "replacing any files in the folder that "
				   "conflict with the files being copied."),
				 dest_dir_name);
		} else {
			primary_text = g_strdup_printf
				(_("A folder named \"%s\" already exists.  "
				   "Do you want to replace it?"), 
				 dest_name);
			secondary_text = g_strdup_printf
				(_("The folder already exists in \"%s\".  "
				   "Replacing it will remove all files in the folder."),
				 dest_dir_name);
		}
	} else {
		primary_text = g_strdup_printf
			(_("A file named \"%s\" already exists.  "
			   "Do you want to replace it?"),
			 dest_name);
		secondary_text = g_strdup_printf
			(_("The file already exists in \"%s\".  "
			   "Replacing it will overwrite its content."),
			 dest_dir_name);
	}

	gtk_message_dialog_set_markup (mdialog, primary_text);
	gtk_message_dialog_format_secondary_text (mdialog, secondary_text);
	g_free (primary_text);
	g_free (secondary_text);

	/* Set up file icons */
	//src_pixbuf = nautilus_file_get_icon_pixbuf (src,
	//					    NAUTILUS_ICON_SIZE_STANDARD,
	//					    FALSE,
	//					    0);
	//dest_pixbuf = nautilus_file_get_icon_pixbuf (dest,
	//					     NAUTILUS_ICON_SIZE_STANDARD,
	//					     FALSE,
	//					     0);
	//src_image = gtk_image_new_from_pixbuf (src_pixbuf);
	//dest_image = gtk_image_new_from_pixbuf (dest_pixbuf);
	src_image = gtk_image_new_from_stock (GTK_STOCK_FILE,
					      NAUTILUS_ICON_SIZE_STANDARD);
	dest_image = gtk_image_new_from_stock (GTK_STOCK_FILE,
					       NAUTILUS_ICON_SIZE_STANDARD);
	gtk_box_pack_start (GTK_BOX (details->first_hbox),
			    src_image, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (details->second_hbox),
			    dest_image, FALSE, FALSE, 0);
	gtk_widget_show (src_image);
	gtk_widget_show (dest_image);

	/* Set up labels */
	src_label = gtk_label_new (NULL);
	date = nautilus_file_get_string_attribute (src,
						   "date_modified");
	size = nautilus_file_get_string_attribute (src, "size");
	label_text = g_markup_printf_escaped (_("<b>Original file:</b> %s\n"
						"<i>Size:</i> %s\n"
						"<i>Last modified:</i> %s"),
						src_name,
						size,
						date);
	gtk_label_set_markup (GTK_LABEL (src_label),
			      label_text);
	g_free (size);
	g_free (date);
	g_free (label_text);
	
	dest_label = gtk_label_new (NULL);
	date = nautilus_file_get_string_attribute (dest,
						   "date_modified");
	size = nautilus_file_get_string_attribute (dest, "size");
	label_text = g_markup_printf_escaped (_("<b>Replace with:</b> %s\n"
						"<i>Size:</i> %s\n"
						"<i>Last modified:</i> %s"),
						dest_name,
						size,
						date);
	gtk_label_set_markup (GTK_LABEL (dest_label),
			      label_text);
	g_free (size);
	g_free (date);
	g_free (label_text);
	gtk_box_pack_start (GTK_BOX (details->first_hbox),
			    src_label, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (details->second_hbox),
			    dest_label, FALSE, FALSE, 0);
	gtk_widget_show (src_label);
	gtk_widget_show (dest_label);
	
	/* Add buttons */
}

static void
set_source_and_destination (GtkWidget *w,
			    GFile *source,
			    GFile *destination,
			    GFile *dest_dir)
{
	NautilusFileConflictDialog *dialog;
	NautilusFileConflictDialogDetails *details;
	
	dialog = NAUTILUS_FILE_CONFLICT_DIALOG (w);
	details = dialog->details;
	
	details->source = source;
	details->destination = destination;
	details->dest_dir = dest_dir;
	
	build_dialog_appearance (dialog);
}

static void
nautilus_file_conflict_dialog_init (NautilusFileConflictDialog *dialog)
{
	GtkWidget *first_hbox, *second_hbox;
	GtkWidget *expander, *entry;
	NautilusFileConflictDialogDetails *details;
	GtkMessageDialog *mdialog;
	
	details = dialog->details = NAUTILUS_FILE_CONFLICT_DIALOG_GET_PRIVATE (dialog);
	mdialog = GTK_MESSAGE_DIALOG (dialog);
	
	/* Setup the hboxes to pack file infos into */
	first_hbox = gtk_hbox_new (FALSE, 6);
	second_hbox = gtk_hbox_new (FALSE, 6);
	gtk_box_pack_start (GTK_BOX (mdialog->label->parent),
			    first_hbox, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (mdialog->label->parent),
			    second_hbox, FALSE, FALSE, 0);
	gtk_widget_show (first_hbox);
	gtk_widget_show (second_hbox);
	details->first_hbox = first_hbox;
	details->second_hbox = second_hbox;
	
	/* Setup the expander for the rename action */
	expander = gtk_expander_new (_("Select a new name for the destination"));
	entry = gtk_entry_new ();
	gtk_container_add (GTK_CONTAINER (expander),
			   entry);
	gtk_box_pack_start (GTK_BOX (mdialog->label->parent),
			    expander, FALSE, FALSE, 0);
	gtk_widget_show (expander);
	details->expander = expander;
	details->entry = entry;
	
	g_print ("end of init\n");
}

static void
nautilus_file_conflict_dialog_class_init (NautilusFileConflictDialogClass *klass)
{
	nautilus_file_conflict_dialog_parent_class = g_type_class_peek_parent (klass);
	
	g_type_class_add_private (klass, sizeof (NautilusFileConflictDialogDetails));
}

GtkWidget *
nautilus_file_conflict_dialog_new (GtkWindow *parent,
				   GFile *source,
				   GFile *destination,
				   GFile *dest_dir)
{
	GtkWidget *dialog;
	
	dialog = GTK_WIDGET (g_object_new (NAUTILUS_TYPE_FILE_CONFLICT_DIALOG,
					   "title", _("File conflict"),
					   "message-type", GTK_MESSAGE_QUESTION,
					   NULL));
	set_source_and_destination (dialog,
				    source,
				    destination,
				    dest_dir);
	gtk_window_set_transient_for (GTK_WINDOW (dialog),
				      parent);
	return dialog;
}
