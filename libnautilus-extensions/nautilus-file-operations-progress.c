/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* dfos-xfer-progress-dialog.c - Progress dialog for transfer operations in the
   GNOME Desktop File Operation Service.

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000 Eazel Inc.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: 
   	Ettore Perazzoli <ettore@gnu.org> 
   	Pavel Cisler <pavel@eazel.com> 
  */

#include <config.h>
#include <gnome.h>
#include <libgnomevfs/gnome-vfs-utils.h>
#include "nautilus-file-operations-progress.h"
#include "libnautilus-extensions/nautilus-gtk-extensions.h"
#include "libnautilus-extensions/nautilus-gdk-font-extensions.h"
#include "libnautilus-extensions/nautilus-gtk-macros.h"


#define LABEL_BOX_WIDTH 350	/* FIXME bugzilla.eazel.com 675: ? */
#define OPERATION_LABEL_WIDTH 65
#define PATH_TRIM_WIDTH LABEL_BOX_WIDTH - OPERATION_LABEL_WIDTH - 2 * 20

static void nautilus_file_operations_progress_initialize_class 	(NautilusFileOperationsProgressClass *klass);
static void nautilus_file_operations_progress_initialize 	(NautilusFileOperationsProgress *dialog);

NAUTILUS_DEFINE_CLASS_BOILERPLATE (NautilusFileOperationsProgress, nautilus_file_operations_progress, GNOME_TYPE_DIALOG);

struct NautilusFileOperationsProgressDetails {

	GtkWidget *progress_title_label;
	GtkWidget *progress_count_label;
	GtkWidget *operation_name_label;
	GtkWidget *item_name;
	GtkWidget *from_label;
	GtkWidget *from_path_label;
	GtkWidget *to_label;
	GtkWidget *to_path_label;

	GtkWidget *progress_bar;

	const char *from_prefix;
	const char *to_prefix;

	guint freeze_count;

	gulong file_index;
	gulong file_size;

	gulong bytes_copied;
	gulong total_bytes_copied;

	gulong files_total;
	gulong bytes_total;
};

/* Private functions.  */

static void
nautilus_file_operations_progress_update (NautilusFileOperationsProgress *dialog)
{
	if (dialog->details->bytes_total == 0) {
		/* we haven't set up the file count yet, do not update the progress
		 * bar until we do
		 */
		return;
	}

	gtk_progress_configure (GTK_PROGRESS (dialog->details->progress_bar),
				dialog->details->total_bytes_copied,
				0.0, dialog->details->bytes_total);
}

static void
set_text_unescaped_trimmed (GtkLabel *label, const char *text, guint max_width)
{
	char *unescaped_text;
	char *trimmed_text;
	
	if (text == NULL || text[0] == '\0') {
		gtk_label_set_text (GTK_LABEL (label), "");
		return;
	}
	
	unescaped_text = gnome_vfs_unescape_string_for_display (text);
	trimmed_text = nautilus_string_ellipsize_start (unescaped_text, 
		GTK_WIDGET (label)->style->font, max_width);
	gtk_label_set_text (GTK_LABEL (label), trimmed_text);
	
	g_free (trimmed_text);
	g_free (unescaped_text);
}

/* GnomeDialog default signal overrides.  */

/* This is just to make sure the dialog is not closed without explicit
   intervention.  */
static gboolean
nautilus_file_operations_progress_close (GnomeDialog *dialog)
{
	NautilusFileOperationsProgress *progress_dialog;

	progress_dialog = NAUTILUS_FILE_OPERATIONS_PROGRESS (dialog);
	return FALSE;
}

/* GtkObject methods.  */

static void
nautilus_file_operations_progress_destroy (GtkObject *object)
{
	NautilusFileOperationsProgress *dialog;

	dialog = NAUTILUS_FILE_OPERATIONS_PROGRESS (object);

	g_free (dialog->details);
}

/* Initialization.  */

static void
create_titled_label (GtkBox *vbox, GtkWidget **title_widget, GtkWidget **label_text_widget)
{
	GtkWidget *hbox;
	
	hbox = gtk_hbox_new (FALSE, 0);
	/* There might be a cleaner way of packing the text labels closer together
	 * than using -2 here. The default is too far appart.
	 */
	gtk_box_pack_start (vbox, hbox, FALSE, FALSE, -2);
	gtk_widget_show (hbox);
	gtk_widget_set_usize (hbox, LABEL_BOX_WIDTH, 0);

	*title_widget = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), *title_widget, FALSE, FALSE, 2);
	gtk_widget_show (*title_widget);
	gtk_widget_set_usize (*title_widget, OPERATION_LABEL_WIDTH, 0);
	gtk_label_set_justify (GTK_LABEL (*title_widget), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (*title_widget), 1, 0);
	nautilus_gtk_label_make_bold (GTK_LABEL (*title_widget));

	*label_text_widget = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), *label_text_widget, FALSE, FALSE, 2);
	gtk_widget_show (*label_text_widget);
	gtk_label_set_justify (GTK_LABEL (*label_text_widget), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment (GTK_MISC (*label_text_widget), 0, 0);
}

static gboolean
delete_event_callback (GtkWidget *widget, GdkEventAny *event)
{
	/* Do nothing -- we shouldn't be getting a close event because
	 * the dialog should not have a close box.
	 */
	return TRUE;
}

static void
nautilus_file_operations_progress_initialize (NautilusFileOperationsProgress *dialog)
{
	GnomeDialog *gnome_dialog;
	GtkBox *vbox;
	GtkWidget *hbox;

	dialog->details = g_new0 (NautilusFileOperationsProgressDetails, 1);

	
	gnome_dialog = GNOME_DIALOG (dialog);
	vbox = GTK_BOX (gnome_dialog->vbox);

	/* This is evil but makes the dialog look less cramped. */
	gtk_container_border_width (GTK_CONTAINER (vbox), 5);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (vbox, hbox, TRUE, TRUE, 3);
	gtk_widget_show (hbox);

	/* label- */
	/* Files remaining to be copied: */
	dialog->details->progress_title_label = gtk_label_new ("");
	gtk_label_set_justify (GTK_LABEL (dialog->details->progress_title_label), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start (GTK_BOX (hbox), dialog->details->progress_title_label, FALSE, FALSE, 0);
	gtk_widget_show (dialog->details->progress_title_label);
	nautilus_gtk_label_make_bold (GTK_LABEL (dialog->details->progress_title_label));


	/* label -- */
	/* 24 of 30 */
	dialog->details->progress_count_label = gtk_label_new ("");
	gtk_label_set_justify (GTK_LABEL (dialog->details->progress_count_label), GTK_JUSTIFY_RIGHT);
	gtk_box_pack_end (GTK_BOX (hbox), dialog->details->progress_count_label, FALSE, FALSE, 0);
	gtk_widget_show (dialog->details->progress_count_label);
	nautilus_gtk_label_make_bold (GTK_LABEL (dialog->details->progress_count_label));

	/* progress bar */
	dialog->details->progress_bar = gtk_progress_bar_new ();
	gtk_progress_bar_set_bar_style (GTK_PROGRESS_BAR (dialog->details->progress_bar),
					GTK_PROGRESS_CONTINUOUS);
	gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (dialog->details->progress_bar),
					  GTK_PROGRESS_LEFT_TO_RIGHT);
	gtk_widget_set_usize (GTK_WIDGET (dialog->details->progress_bar), LABEL_BOX_WIDTH,
			      -1);
	gtk_box_pack_start (vbox, dialog->details->progress_bar, FALSE, TRUE, 0);
	gtk_widget_show (dialog->details->progress_bar);

	create_titled_label (vbox, &dialog->details->operation_name_label, 
		&dialog->details->item_name);
	create_titled_label (vbox, &dialog->details->from_label, 
		&dialog->details->from_path_label);
	create_titled_label (vbox, &dialog->details->to_label, 
		&dialog->details->to_path_label);


	dialog->details->file_index = 0;
	dialog->details->file_size = 0;
	dialog->details->files_total = 0;
	dialog->details->bytes_total = 0;
	dialog->details->bytes_copied = 0;
	dialog->details->total_bytes_copied = 0;

	dialog->details->freeze_count = 0;
}

static void
nautilus_file_operations_progress_initialize_class (NautilusFileOperationsProgressClass *klass)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GnomeDialogClass *dialog_class;

	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	dialog_class = GNOME_DIALOG_CLASS (klass);

	object_class->destroy = nautilus_file_operations_progress_destroy;
	dialog_class->close = nautilus_file_operations_progress_close;

	/* The progress dialog should not have a title and a close box.
	 * Some broken window manager themes still show the window title.
	 * Make clicking the close box do nothing in that case to prevent
	 * a crash.
	 */
	widget_class->delete_event = delete_event_callback;
}

GtkWidget *
nautilus_file_operations_progress_new (const char *title,
				       const char *operation_string,
				       const char *from_prefix,
				       const char *to_prefix,
				       gulong total_files,
				       gulong total_bytes)
{
	GtkWidget *widget;
	NautilusFileOperationsProgress *dialog;

	widget = gtk_widget_new (nautilus_file_operations_progress_get_type (), NULL);
	dialog = NAUTILUS_FILE_OPERATIONS_PROGRESS (widget);

	nautilus_file_operations_progress_set_operation_string (dialog, operation_string);
	nautilus_file_operations_progress_set_total (dialog, total_files, total_bytes);

	gtk_window_set_title (GTK_WINDOW (widget), title);
	gtk_window_set_wmclass (GTK_WINDOW (widget), "file_progress", "Nautilus");

	gnome_dialog_append_button (GNOME_DIALOG (widget), GNOME_STOCK_BUTTON_CANCEL);

	dialog->details->from_prefix = from_prefix;
	dialog->details->to_prefix = to_prefix;
	
	return widget;
}

void
nautilus_file_operations_progress_set_total (NautilusFileOperationsProgress *dialog,
					     gulong files_total,
					     gulong bytes_total)
{
	g_return_if_fail (IS_NAUTILUS_FILE_OPERATIONS_PROGRESS (dialog));

	dialog->details->files_total = files_total;
	dialog->details->bytes_total = bytes_total;

	nautilus_file_operations_progress_update (dialog);
}

void
nautilus_file_operations_progress_set_operation_string (NautilusFileOperationsProgress *dialog,
							const char *operation_string)
{
	g_return_if_fail (IS_NAUTILUS_FILE_OPERATIONS_PROGRESS (dialog));

	gtk_label_set_text (GTK_LABEL (dialog->details->progress_title_label),
			    operation_string);
}

void
nautilus_file_operations_progress_new_file (NautilusFileOperationsProgress *dialog,
					    const char *progress_verb,
					    const char *item_name,
					    const char *from_path,
					    const char *to_path,
					    const char *from_prefix,
					    const char *to_prefix,
					    gulong file_index,
					    gulong size)
{
	char *progress_count;

	g_return_if_fail (IS_NAUTILUS_FILE_OPERATIONS_PROGRESS (dialog));
	g_return_if_fail (GTK_WIDGET_REALIZED (dialog));

	dialog->details->file_index = file_index;
	dialog->details->bytes_copied = 0;
	dialog->details->file_size = size;

	dialog->details->from_prefix = from_prefix;
	dialog->details->to_prefix = to_prefix;

	if (dialog->details->bytes_total > 0) {
		/* we haven't set up the file count yet, do not update the progress
		 * count until we do
		 */
		gtk_label_set_text (GTK_LABEL (dialog->details->operation_name_label), progress_verb);
		set_text_unescaped_trimmed (GTK_LABEL (dialog->details->item_name),
			item_name, PATH_TRIM_WIDTH);

		progress_count = g_strdup_printf (_("%ld of %ld"), dialog->details->file_index, 
			dialog->details->files_total);
		gtk_label_set_text (GTK_LABEL (dialog->details->progress_count_label), progress_count);
		g_free (progress_count);

		gtk_label_set_text (GTK_LABEL (dialog->details->from_label), from_prefix);
		set_text_unescaped_trimmed (GTK_LABEL (dialog->details->from_path_label),
			from_path, PATH_TRIM_WIDTH);
	
		if (dialog->details->to_prefix != NULL && dialog->details->to_path_label != NULL) {
			gtk_label_set_text (GTK_LABEL (dialog->details->to_label), to_prefix);
			set_text_unescaped_trimmed (GTK_LABEL (dialog->details->to_path_label),
				to_path, PATH_TRIM_WIDTH);
		}
	}

	nautilus_file_operations_progress_update (dialog);
}

void
nautilus_file_operations_progress_clear (NautilusFileOperationsProgress *dialog)
{
	gtk_label_set_text (GTK_LABEL (dialog->details->from_label), "");
	gtk_label_set_text (GTK_LABEL (dialog->details->from_path_label), "");
	gtk_label_set_text (GTK_LABEL (dialog->details->to_label), "");
	gtk_label_set_text (GTK_LABEL (dialog->details->to_path_label), "");

	dialog->details->files_total = 0;
	dialog->details->bytes_total = 0;

	nautilus_file_operations_progress_update (dialog);
}

void
nautilus_file_operations_progress_update_sizes (NautilusFileOperationsProgress *dialog,
						gulong bytes_done_in_file,
						gulong bytes_done)
{
	g_return_if_fail (IS_NAUTILUS_FILE_OPERATIONS_PROGRESS (dialog));

	dialog->details->bytes_copied = bytes_done_in_file;
	dialog->details->total_bytes_copied = bytes_done;

	nautilus_file_operations_progress_update (dialog);
}

void
nautilus_file_operations_progress_freeze (NautilusFileOperationsProgress *dialog)
{
	dialog->details->freeze_count++;
}

void
nautilus_file_operations_progress_thaw (NautilusFileOperationsProgress *dialog)
{
	if (dialog->details->freeze_count > 0) {
		dialog->details->freeze_count--;
	}
}

