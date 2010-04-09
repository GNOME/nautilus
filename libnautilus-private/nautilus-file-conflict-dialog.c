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

#include <string.h>
#include <glib-object.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <pango/pango.h>
#include <gtk/gtk.h>
#include <eel/eel-vfs-extensions.h>

#include "nautilus-file.h"
#include "nautilus-icon-info.h"

struct _NautilusFileConflictDialogDetails
{
	/* conflicting objects */
	GFile *source;
	GFile *destination;
	GFile *dest_dir;

	gchar *conflict_name;

	/* UI objects */
	GtkWidget *titles_vbox;
	GtkWidget *first_hbox;
	GtkWidget *second_hbox;
	GtkWidget *expander;
	GtkWidget *entry;
	GtkWidget *checkbox;
	GtkWidget *rename_button;
	GtkWidget *replace_button;
};

G_DEFINE_TYPE (NautilusFileConflictDialog,
	       nautilus_file_conflict_dialog,
	       GTK_TYPE_DIALOG);

#define NAUTILUS_FILE_CONFLICT_DIALOG_GET_PRIVATE(object)		\
	(G_TYPE_INSTANCE_GET_PRIVATE ((object), NAUTILUS_TYPE_FILE_CONFLICT_DIALOG, \
				      NautilusFileConflictDialogDetails))

static const gchar *
get_str_for_mtimes (NautilusFile *src,
		    NautilusFile *dest)
{
	time_t src_mtime, dest_mtime;

	src_mtime = nautilus_file_get_mtime (src);
	dest_mtime = nautilus_file_get_mtime (dest);

	if (src_mtime > dest_mtime)
		return _("An older");

	if (src_mtime < dest_mtime)
		return _("A newer");

	return _("Another");
}

static void
file_list_ready_cb (GList *files,
		    gpointer user_data)
{
	NautilusFileConflictDialog *fcd = user_data;
	NautilusFile *src, *dest, *dest_dir;
	GtkDialog *dialog;
	gboolean source_is_dir,	dest_is_dir, should_show_type;
	NautilusFileConflictDialogDetails *details;
	char *primary_text, *secondary_text;
	char *src_name, *dest_name, *dest_dir_name, *edit_name;
	char *label_text;
	char *size, *date, *type = NULL;
	const gchar *time_str;
	GdkPixbuf *pixbuf;
	GtkWidget *image, *label;
	GString *str;
	PangoFontDescription *desc;

	dialog = GTK_DIALOG (fcd);
	details = fcd->details;
	
	dest_dir = g_list_nth_data (files, 0);
	dest = g_list_nth_data (files, 1);
	src = g_list_nth_data (files, 2);

	src_name = nautilus_file_get_display_name (src);
	dest_name = nautilus_file_get_display_name (dest);
	dest_dir_name = nautilus_file_get_display_name (dest_dir);

	source_is_dir = nautilus_file_is_directory (src);
	dest_is_dir = nautilus_file_is_directory (dest);

	type = nautilus_file_get_mime_type (dest);
	should_show_type = !nautilus_file_is_mime_type (src, type);

	g_free (type);
	type = NULL;

	time_str = get_str_for_mtimes (src, dest);
	/* Set up the right labels */
	if (dest_is_dir) {
		if (source_is_dir) {
			primary_text = g_strdup_printf
				(_("Merge folder \"%s\"?"),
				 dest_name);

			/* Translators: the first string in this printf
			 * is the "An older/A newer/Another" string defined some lines before.
			 */
			secondary_text = g_strdup_printf (
				_("%s folder with the same name already exists in \"%s\".\n"
				  "Merging will ask for confirmation before "
				  "replacing any files in the folder that "
				  "conflict with the files being copied."),
				time_str,
				dest_dir_name);
		} else {
			primary_text = g_strdup_printf
				(_("Replace folder \"%s\"?"), dest_name);
			secondary_text = g_strdup_printf
				(_("A folder with the same name already exists in \"%s\".\n"
				   "Replacing it will remove all files in the folder."),
				 dest_dir_name);
		}
	} else {
		primary_text = g_strdup_printf
			(_("Replace file \"%s\"?"), dest_name);

		/* Translators: the first string in this printf
		 * is the "An older/A newer/Another" string defined some lines before.
		 */
		secondary_text = g_strdup_printf (
			_("%s file with the same name already exists in \"%s\".\n"
			  "Replacing it will overwrite its content."),
			time_str,
			dest_dir_name);
	}

	label = gtk_label_new (NULL);
	gtk_label_set_text (GTK_LABEL (label), primary_text);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (details->titles_vbox),
			    label, FALSE, FALSE, 0);
	gtk_widget_modify_font (label, NULL);
	desc = pango_font_description_new ();
	pango_font_description_set_weight (desc, PANGO_WEIGHT_BOLD);
	pango_font_description_set_size (desc,
		pango_font_description_get_size (gtk_widget_get_style (label)->font_desc) * PANGO_SCALE_LARGE);
	gtk_widget_modify_font (label, desc);
	pango_font_description_free (desc);
	gtk_widget_show (label);

	label = gtk_label_new (secondary_text);
	gtk_misc_set_alignment (GTK_MISC (label), 0.0, 0.5);
	gtk_box_pack_start (GTK_BOX (details->titles_vbox),
			    label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	g_free (primary_text);
	g_free (secondary_text);

	/* Set up file icons */
	pixbuf = nautilus_file_get_icon_pixbuf (dest,
						NAUTILUS_ICON_SIZE_LARGE,
						TRUE,
						NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS);
	image = gtk_image_new_from_pixbuf (pixbuf);
	gtk_box_pack_start (GTK_BOX (details->first_hbox),
			    image, FALSE, FALSE, 0);
	gtk_widget_show (image);
	g_object_unref (pixbuf);

	pixbuf = nautilus_file_get_icon_pixbuf (src,
						NAUTILUS_ICON_SIZE_LARGE,
						TRUE,
						NAUTILUS_FILE_ICON_FLAGS_USE_THUMBNAILS);
	image = gtk_image_new_from_pixbuf (pixbuf);
	gtk_box_pack_start (GTK_BOX (details->second_hbox),
			    image, FALSE, FALSE, 0);
	gtk_widget_show (image);
	g_object_unref (pixbuf);

	/* Set up labels */
	label = gtk_label_new (NULL);
	date = nautilus_file_get_string_attribute (dest,
						   "date_modified");
	size = nautilus_file_get_string_attribute (dest, "size");

	if (should_show_type) {
		type = nautilus_file_get_string_attribute (dest, "type");
	}

	str = g_string_new (NULL);
	g_string_append_printf (str, "<b>%s</b>\n", _("Original file"));
	g_string_append_printf (str, "<i>%s</i> %s\n", _("Size:"), size);

	if (should_show_type) {
		g_string_append_printf (str, "<i>%s</i> %s\n", _("Type:"), type);
	}

	g_string_append_printf (str, "<i>%s</i> %s", _("Last modified:"), date);

	label_text = str->str;
	gtk_label_set_markup (GTK_LABEL (label),
			      label_text);
	gtk_box_pack_start (GTK_BOX (details->first_hbox),
			    label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	g_free (size);
	g_free (type);
	g_free (date);
	g_string_erase (str, 0, -1);

	/* Second label */
	label = gtk_label_new (NULL);
	date = nautilus_file_get_string_attribute (src,
						   "date_modified");
	size = nautilus_file_get_string_attribute (src, "size");

	if (should_show_type) {
		type = nautilus_file_get_string_attribute (src, "type");
	}

	g_string_append_printf (str, "<b>%s</b>\n", _("Replace with"));
	g_string_append_printf (str, "<i>%s</i> %s\n", _("Size:"), size);

	if (should_show_type) {
		g_string_append_printf (str, "<i>%s</i> %s\n", _("Type:"), type);
	}

	g_string_append_printf (str, "<i>%s</i> %s", _("Last modified:"), date);
	label_text = g_string_free (str, FALSE);

	gtk_label_set_markup (GTK_LABEL (label),
			      label_text);
	gtk_box_pack_start (GTK_BOX (details->second_hbox),
			    label, FALSE, FALSE, 0);
	gtk_widget_show (label);

	g_free (size);
	g_free (date);
	g_free (type);
	g_free (label_text);

	/* Populate the entry */
	edit_name = nautilus_file_get_edit_name (dest);
	details->conflict_name = edit_name;

	gtk_entry_set_text (GTK_ENTRY (details->entry), edit_name);

	if (source_is_dir && dest_is_dir) {
		gtk_button_set_label (GTK_BUTTON (details->replace_button),
				      _("Merge"));
	}
}

static void
build_dialog_appearance (NautilusFileConflictDialog *fcd)
{
	GList *files = NULL;
	NautilusFile *src, *dest, *dest_dir;

	src = nautilus_file_get (fcd->details->source);
	dest = nautilus_file_get (fcd->details->destination);
	dest_dir = nautilus_file_get (fcd->details->dest_dir);

	files = g_list_prepend (files, src);
	files = g_list_prepend (files, dest);
	files = g_list_prepend (files, dest_dir);

	nautilus_file_list_call_when_ready (files,
					    NAUTILUS_FILE_ATTRIBUTE_INFO |
					    NAUTILUS_FILE_ATTRIBUTE_LINK_INFO |
					    NAUTILUS_FILE_ATTRIBUTE_THUMBNAIL,
					    NULL, file_list_ready_cb, fcd);
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
entry_text_changed_cb (GtkEditable *entry,
		       NautilusFileConflictDialog *dialog)
{
	NautilusFileConflictDialogDetails *details;

	details = dialog->details;

	/* The rename button is visible only if there's text
	 * in the entry.
	 */
	if  (g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (entry)), "") != 0 &&
	     g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (entry)), details->conflict_name) != 0) {
		gtk_widget_hide (details->replace_button);
		gtk_widget_show (details->rename_button);
	} else {
		gtk_widget_hide (details->rename_button);
		gtk_widget_show (details->replace_button);
	}
}

static void
expander_activated_cb (GtkExpander *w,
		       NautilusFileConflictDialog *dialog)
{
	NautilusFileConflictDialogDetails *details;
	int start_pos, end_pos;

	details = dialog->details;

	if (!gtk_expander_get_expanded (w)) {
		gtk_widget_grab_focus (details->entry);

		if (g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (details->entry)),
			       details->conflict_name) == 0) {
			eel_filename_get_rename_region (details->conflict_name,
							&start_pos, &end_pos);
			gtk_editable_select_region (GTK_EDITABLE (details->entry),
						    start_pos, end_pos);
		}
	}
}

static void
checkbox_toggled_cb (GtkToggleButton *t,
		     NautilusFileConflictDialog *dialog)
{
	NautilusFileConflictDialogDetails *details;

	details = dialog->details;

	gtk_widget_set_sensitive (details->expander,
				  !gtk_toggle_button_get_active (t));
	gtk_widget_set_sensitive (details->rename_button,
				  !gtk_toggle_button_get_active (t));

	if  (!gtk_toggle_button_get_active (t) &&
	     g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (details->entry)),
			"") != 0 &&
	     g_strcmp0 (gtk_entry_get_text (GTK_ENTRY (details->entry)),
			details->conflict_name) != 0) {
		gtk_widget_hide (details->replace_button);
		gtk_widget_show (details->rename_button);
	} else {
		gtk_widget_hide (details->rename_button);
		gtk_widget_show (details->replace_button);
	}
}

static void
reset_button_clicked_cb (GtkButton *w,
			 NautilusFileConflictDialog *dialog)
{
	NautilusFileConflictDialogDetails *details;
	int start_pos, end_pos;

	details = dialog->details;

	gtk_entry_set_text (GTK_ENTRY (details->entry),
			    details->conflict_name);
	eel_filename_get_rename_region (details->conflict_name,
					&start_pos, &end_pos);
	gtk_editable_select_region (GTK_EDITABLE (details->entry),
				    start_pos, end_pos);

}

static void
nautilus_file_conflict_dialog_init (NautilusFileConflictDialog *fcd)
{
	GtkWidget *hbox, *vbox, *vbox2, *alignment;
	GtkWidget *widget, *dialog_area;
	NautilusFileConflictDialogDetails *details;
	GtkDialog *dialog;

	details = fcd->details = NAUTILUS_FILE_CONFLICT_DIALOG_GET_PRIVATE (fcd);
	dialog = GTK_DIALOG (fcd);

	/* Setup the main hbox */
	hbox = gtk_hbox_new (FALSE, 12);
	dialog_area = gtk_dialog_get_content_area (dialog);
	gtk_box_pack_start (GTK_BOX (dialog_area), hbox, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 6);

	/* Setup the dialog image */
	widget = gtk_image_new_from_stock (GTK_STOCK_DIALOG_WARNING,
					   GTK_ICON_SIZE_DIALOG);
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 0);
	gtk_misc_set_alignment (GTK_MISC (widget), 0.5, 0.0);

	/* Setup the vbox containing the dialog body */
	vbox = gtk_vbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (hbox), vbox, FALSE, FALSE, 0);

	/* Setup the vbox for the dialog labels */
	widget = gtk_vbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE, FALSE, 0);
	details->titles_vbox = widget;

	/* Setup the hboxes to pack file infos into */
	alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
	g_object_set (alignment, "left-padding", 12, NULL);
	vbox2 = gtk_vbox_new (FALSE, 12);
	gtk_container_add (GTK_CONTAINER (alignment), vbox2);
	gtk_box_pack_start (GTK_BOX (vbox), alignment, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
	details->first_hbox = hbox;

	hbox = gtk_hbox_new (FALSE, 12);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox, FALSE, FALSE, 0);
	details->second_hbox = hbox;

	/* Setup the expander for the rename action */
	details->expander = gtk_expander_new_with_mnemonic (_("_Select a new name for the destination"));
	gtk_box_pack_start (GTK_BOX (vbox2), details->expander, FALSE, FALSE, 0);
	g_signal_connect (details->expander, "activate",
			  G_CALLBACK (expander_activated_cb), dialog);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_container_add (GTK_CONTAINER (details->expander), hbox);

	widget = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 6);
	details->entry = widget;
	g_signal_connect (widget, "changed",
			  G_CALLBACK (entry_text_changed_cb), dialog);

	widget = gtk_button_new_with_label (_("Reset"));
	gtk_button_set_image (GTK_BUTTON (widget),
			      gtk_image_new_from_stock (GTK_STOCK_UNDO,
							GTK_ICON_SIZE_MENU));
	gtk_box_pack_start (GTK_BOX (hbox), widget, FALSE, FALSE, 6);
	g_signal_connect (widget, "clicked",
			  G_CALLBACK (reset_button_clicked_cb), dialog);

	gtk_widget_show_all (alignment);


	/* Setup the checkbox to apply the action to all files */
	widget = gtk_check_button_new_with_mnemonic (_("Apply this action to all files"));
	gtk_box_pack_start (GTK_BOX (vbox),
			    widget, FALSE, FALSE, 0);
	details->checkbox = widget;
	g_signal_connect (widget, "toggled",
			  G_CALLBACK (checkbox_toggled_cb), dialog);

	/* Add buttons */
	gtk_dialog_add_buttons (dialog,
				GTK_STOCK_CANCEL,
				GTK_RESPONSE_CANCEL,
				_("_Skip"),
				CONFLICT_RESPONSE_SKIP,
				NULL);
	details->rename_button =
		gtk_dialog_add_button (dialog,
				       _("Re_name"),
				       CONFLICT_RESPONSE_RENAME);
	gtk_widget_hide (details->rename_button);

	details->replace_button =
		gtk_dialog_add_button (dialog,
				       _("Replace"),
				       CONFLICT_RESPONSE_REPLACE);

	/* Setup HIG properties */
	gtk_container_set_border_width (GTK_CONTAINER (dialog), 5);
	gtk_box_set_spacing (GTK_BOX (dialog->vbox), 14);
	gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
	gtk_dialog_set_has_separator (dialog, FALSE);

	gtk_widget_show_all (dialog_area);
}

static void
do_finalize (GObject *self)
{
	NautilusFileConflictDialogDetails *details =
		NAUTILUS_FILE_CONFLICT_DIALOG (self)->details;

	g_free (details->conflict_name);

	G_OBJECT_CLASS (nautilus_file_conflict_dialog_parent_class)->finalize (self);
}

static void
nautilus_file_conflict_dialog_class_init (NautilusFileConflictDialogClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = do_finalize;

	g_type_class_add_private (klass, sizeof (NautilusFileConflictDialogDetails));
}

char *
nautilus_file_conflict_dialog_get_new_name (NautilusFileConflictDialog *dialog)
{
	return g_strdup (gtk_entry_get_text
			 (GTK_ENTRY (dialog->details->entry)));
}

gboolean
nautilus_file_conflict_dialog_get_apply_to_all (NautilusFileConflictDialog *dialog)
{
	return gtk_toggle_button_get_active 
		(GTK_TOGGLE_BUTTON (dialog->details->checkbox));
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
					   NULL));
	set_source_and_destination (dialog,
				    source,
				    destination,
				    dest_dir);
	gtk_window_set_transient_for (GTK_WINDOW (dialog),
				      parent);
	return dialog;
}
