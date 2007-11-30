/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* dfos-xfer-progress-dialog.c - Progress dialog for transfer operations in the
   GNOME Desktop File Operation Service.

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000, 2001 Eazel Inc.

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
#include "nautilus-file-operations-progress.h"

#include <eel/eel-glib-extensions.h>
#include <eel/eel-gtk-extensions.h>
#include <eel/eel-gtk-macros.h>
#include <eel/eel-vfs-extensions.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkprogressbar.h>
#include <gtk/gtkstock.h>
#include <gtk/gtktable.h>
#include <gtk/gtkvbox.h>
#include <glib/gi18n.h>
#include <glib/gurifuncs.h>
#include "nautilus-file-operations-progress-icons.h"

/* The default width of the progress dialog. It will be wider
 * only if the font is really huge and the fixed labels don't
 * fit in the window otherwise.
 */
#define PROGRESS_DIALOG_WIDTH 400

#define OUTER_BORDER       6
#define VERTICAL_SPACING   6
#define HORIZONTAL_SPACING 6

#define MINIMUM_TIME_UP    1000

#define SHOW_TIMEOUT	   1200
#define TIME_REMAINING_TIMEOUT 1000

static GdkPixbuf *empty_jar_pixbuf, *full_jar_pixbuf;

static void nautilus_file_operations_progress_class_init (NautilusFileOperationsProgressClass *klass);
static void nautilus_file_operations_progress_init       (NautilusFileOperationsProgress      *dialog);

EEL_CLASS_BOILERPLATE (NautilusFileOperationsProgress,
		       nautilus_file_operations_progress,
		       GTK_TYPE_DIALOG)

struct NautilusFileOperationsProgressDetails {
	GtkWidget *primary_text_label;
	GtkWidget *progress_title_label;
	GtkWidget *progress_count_label;
	GtkWidget *operation_name_label;
	GtkWidget *item_name;
	GtkWidget *from_label;
	GtkWidget *from_path_label;
	GtkWidget *to_label;
	GtkWidget *to_path_label;

	GtkWidget *progress_bar;

	char *progress_title;

	const char *from_prefix;
	const char *to_prefix;

	gulong files_total;
	gulong file_index;
	
	goffset bytes_copied;
	goffset bytes_total;

	/* system time (microseconds) when show timeout was started */
	gint64 start_time;

	/* system time (microseconds) when dialog was mapped */
	gint64 show_time;

	/* string for remaining time */
	char *remaining_time_string;
	
	/* time remaining in show timeout if it's paused and resumed */
	guint remaining_time;

	guint delayed_close_timeout_id;
	guint delayed_show_timeout_id;

	/* system time (microseconds) when first file transfer began */
	gint64 first_transfer_time;
	guint time_remaining_timeout_id;
	
	int progress_jar_position;
};

/* Private functions. */

static void
nautilus_file_operations_progress_update_icon (NautilusFileOperationsProgress *progress,
					       double fraction)
{
	GdkPixbuf *pixbuf;
	int position;

	position = gdk_pixbuf_get_height (empty_jar_pixbuf) * (1 - fraction);

	if (position == progress->details->progress_jar_position) {
		return;
	}

	progress->details->progress_jar_position = position;
	
	pixbuf = gdk_pixbuf_copy (empty_jar_pixbuf);
	gdk_pixbuf_copy_area (full_jar_pixbuf,
			      0, position,
			      gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf) - position,
			      pixbuf,
			      0, position);

	gtk_window_set_icon (GTK_WINDOW (progress), pixbuf);
	g_object_unref (pixbuf);
}


static void
nautilus_file_operations_progress_update (NautilusFileOperationsProgress *progress)
{
	double fraction;
	char *progress_count;
	char *remaining_time_string = NULL;

	if (progress->details->bytes_total == 0) {
		/* We haven't set up the file count yet */
		gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progress->details->progress_bar), progress->details->progress_title);
		return;
	}

	fraction = (double)progress->details->bytes_copied /
		progress->details->bytes_total;

	if (progress->details->remaining_time_string == NULL) {
		remaining_time_string = "";
	} else {
		remaining_time_string = progress->details->remaining_time_string;
	}

	progress_count = g_strdup_printf (_("%s %ld of %ld %s"),
					  progress->details->progress_title,
					  progress->details->file_index, 
					  progress->details->files_total,
					  remaining_time_string);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progress->details->progress_bar), progress_count);

	g_free (progress_count);

	gtk_progress_bar_set_fraction
		(GTK_PROGRESS_BAR (progress->details->progress_bar),
		 fraction);
	nautilus_file_operations_progress_update_icon (progress, fraction);
}

static void
set_text_unescaped_trimmed (GtkLabel *label, const char *text)
{
	char *unescaped_text;
	char *unescaped_utf8;
	
	if (text == NULL || text[0] == '\0') {
		gtk_label_set_text (label, "");
		return;
	}
	
	unescaped_text = g_uri_unescape_string (text, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH);
	unescaped_utf8 = eel_make_valid_utf8 (unescaped_text);
	gtk_label_set_text (label, unescaped_utf8);
	g_free (unescaped_utf8);
	g_free (unescaped_text);
}

/* This is just to make sure the dialog is not closed without explicit
 * intervention.
 */
static void
close_callback (GtkDialog *dialog)
{
}

/* GObject methods. */
static void
nautilus_file_operations_progress_finalize (GObject *object)
{
	NautilusFileOperationsProgress *progress;

	progress = NAUTILUS_FILE_OPERATIONS_PROGRESS (object);

	g_free (progress->details);

	EEL_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

/* GtkObject methods.  */

static void
nautilus_file_operations_progress_destroy (GtkObject *object)
{
	NautilusFileOperationsProgress *progress;

	progress = NAUTILUS_FILE_OPERATIONS_PROGRESS (object);

	if (progress->details->delayed_close_timeout_id != 0) {
		g_source_remove (progress->details->delayed_close_timeout_id);
		progress->details->delayed_close_timeout_id = 0;
	}
	
	if (progress->details->delayed_show_timeout_id != 0) {
		g_source_remove (progress->details->delayed_show_timeout_id);
		progress->details->delayed_show_timeout_id = 0;
	}

	if (progress->details->time_remaining_timeout_id != 0) {
		g_source_remove (progress->details->time_remaining_timeout_id);
		progress->details->time_remaining_timeout_id = 0;
	}

	if (progress->details->remaining_time_string != NULL) {
		g_free (progress->details->remaining_time_string);
		progress->details->remaining_time_string = NULL;
	}

	if (progress->details->progress_title != NULL) {
		g_free (progress->details->progress_title);
		progress->details->progress_title = NULL;
	}

	EEL_CALL_PARENT (GTK_OBJECT_CLASS, destroy, (object));
}

/* Initialization.  */

static void
create_titled_label (GtkTable *table, int row, GtkWidget **title_widget, GtkWidget **label_text_widget)
{
	*title_widget = gtk_label_new ("");
	eel_gtk_label_make_bold (GTK_LABEL (*title_widget));
	gtk_misc_set_alignment (GTK_MISC (*title_widget), 1, 0);
	gtk_table_attach (table, *title_widget,
			  0, 1,
			  row, row + 1,
			  GTK_FILL, 0,
			  0, 0);
	gtk_widget_show (*title_widget);

	*label_text_widget = gtk_label_new (NULL);
        gtk_label_set_ellipsize (GTK_LABEL (*label_text_widget), PANGO_ELLIPSIZE_START);
	gtk_misc_set_alignment (GTK_MISC (*label_text_widget), 0, 0);
	gtk_table_attach (table, *label_text_widget,
			  1, 2,
			  row, row + 1,
			  GTK_FILL | GTK_EXPAND, 0,
			  0, 0);
	gtk_widget_show (*label_text_widget);
}

static void
map_callback (GtkWidget *widget)
{
	NautilusFileOperationsProgress *progress;

	progress = NAUTILUS_FILE_OPERATIONS_PROGRESS (widget);

	EEL_CALL_PARENT (GTK_WIDGET_CLASS, map, (widget));

	progress->details->show_time = eel_get_system_time ();
}

static gboolean
delete_event_callback (GtkWidget *widget,
		       GdkEventAny *event)
{
	/* Do nothing -- we shouldn't be getting a close event because
	 * the dialog should not have a close box.
	 */
	return TRUE;
}

static void
nautilus_file_operations_progress_init (NautilusFileOperationsProgress *progress)
{
	GtkWidget *hbox, *vbox, *progress_vbox, *file_hbox;
	GtkTable *titled_label_table;

	progress->details = g_new0 (NautilusFileOperationsProgressDetails, 1);

	vbox = gtk_vbox_new (FALSE, VERTICAL_SPACING);
	gtk_container_set_border_width (GTK_CONTAINER (vbox), OUTER_BORDER);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (progress)->vbox), vbox, TRUE, TRUE, 0);

	progress->details->primary_text_label = gtk_label_new("");
	gtk_misc_set_alignment (GTK_MISC (progress->details->primary_text_label), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (vbox), progress->details->primary_text_label, FALSE, FALSE, 0);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

	titled_label_table = GTK_TABLE (gtk_table_new (2, 2, FALSE));
	gtk_table_set_row_spacings (titled_label_table, 4);
	gtk_table_set_col_spacings (titled_label_table, 4);

	create_titled_label (titled_label_table, 0,
			     &progress->details->from_label, 
			     &progress->details->from_path_label);
	create_titled_label (titled_label_table, 1,
			     &progress->details->to_label, 
			     &progress->details->to_path_label);

	gtk_box_pack_start (GTK_BOX (vbox), GTK_WIDGET (titled_label_table), FALSE, FALSE, 0);

	/* progress vbox */
	progress_vbox = gtk_vbox_new (TRUE, 0);
	gtk_box_pack_start (GTK_BOX (vbox), progress_vbox, FALSE, FALSE, 0);

	/* progress bar */
	progress->details->progress_bar = gtk_progress_bar_new ();
	gtk_window_set_default_size (GTK_WINDOW (progress), PROGRESS_DIALOG_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (progress_vbox), progress->details->progress_bar, FALSE, TRUE, 0);
	/* prevent a resizing of the bar when a real text is inserted later */
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (progress->details->progress_bar), " ");

	/* file hbox */
	file_hbox = gtk_hbox_new (FALSE, HORIZONTAL_SPACING);
	gtk_box_pack_start (GTK_BOX (progress_vbox), file_hbox, TRUE, TRUE, 2);

	/* progress verb */
	progress->details->operation_name_label = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (progress->details->operation_name_label), 0.0, 0.0);
	gtk_box_pack_start (GTK_BOX (file_hbox), progress->details->operation_name_label, FALSE, FALSE, 0);

	/* file label */
	progress->details->item_name = gtk_label_new (NULL);
	gtk_misc_set_alignment (GTK_MISC (progress->details->item_name), 0.0, 0.0);
	gtk_label_set_ellipsize (GTK_LABEL (progress->details->item_name), PANGO_ELLIPSIZE_END);
	gtk_box_pack_start (GTK_BOX (file_hbox), progress->details->item_name, TRUE, TRUE, 0);


	/* Set window icon */
	gtk_window_set_icon (GTK_WINDOW (progress), empty_jar_pixbuf);

	/* Set progress jar position */
	progress->details->progress_jar_position = gdk_pixbuf_get_height (empty_jar_pixbuf);

	gtk_widget_show_all (vbox);
}

static void
nautilus_file_operations_progress_class_init (NautilusFileOperationsProgressClass *klass)
{
	GObjectClass *gobject_class;
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkDialogClass *dialog_class;

	gobject_class = G_OBJECT_CLASS (klass);
	object_class = GTK_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
	dialog_class = GTK_DIALOG_CLASS (klass);


	gobject_class->finalize = nautilus_file_operations_progress_finalize;
	
	object_class->destroy = nautilus_file_operations_progress_destroy;

	/* The progress dialog should not have a title and a close box.
	 * Some broken window manager themes still show the window title.
	 * Make clicking the close box do nothing in that case to prevent
	 * a crash.
	 */
	widget_class->delete_event = delete_event_callback;
	widget_class->map = map_callback;

	dialog_class->close = close_callback;

	/* Load the jar pixbufs */
	empty_jar_pixbuf = gdk_pixbuf_new_from_inline (-1, progress_jar_empty_icon, FALSE, NULL);
	full_jar_pixbuf = gdk_pixbuf_new_from_inline (-1, progress_jar_full_icon, FALSE, NULL);
	
}

static gboolean
time_remaining_callback (gpointer callback_data)
{
	int elapsed_time;
	int transfer_rate;
	int time_remaining;
	char *str;
	NautilusFileOperationsProgress *progress;
	
	progress = NAUTILUS_FILE_OPERATIONS_PROGRESS (callback_data);
	
	elapsed_time = (eel_get_system_time () - progress->details->first_transfer_time) / 1000000;

	if (elapsed_time == 0) {
		progress->details->time_remaining_timeout_id =
			g_timeout_add (TIME_REMAINING_TIMEOUT, time_remaining_callback, progress);
		
		return FALSE;
	}
	
	transfer_rate = progress->details->bytes_copied / elapsed_time;

	if (transfer_rate == 0) {
		progress->details->time_remaining_timeout_id =
			g_timeout_add (TIME_REMAINING_TIMEOUT, time_remaining_callback, progress);

		return FALSE;
	}

	time_remaining = (progress->details->bytes_total -
			  progress->details->bytes_copied) / transfer_rate;

	if (progress->details->bytes_copied > progress->details->bytes_total) {
		/* This shouldn't be neccessary, but gnome-vfs seems to add the bytes processed during
		 * the cleanup phase to bytes_copied. So we try avoid showing unrealistic ETAs here.
		 */
		str = g_strdup_printf ("%s", " ");
	}
	else if (time_remaining >= 3600) {
		str = g_strdup_printf (_("(%d:%02d:%02d Remaining)"), 
				       time_remaining / 3600, (time_remaining % 3600) / 60, (time_remaining % 3600) % 60);

	}
	else {
		str = g_strdup_printf (_("(%d:%02d Remaining)"), 
				       time_remaining / 60, time_remaining % 60);
	}

	g_free (progress->details->remaining_time_string);
	progress->details->remaining_time_string = g_strdup (str);

	nautilus_file_operations_progress_update (progress);

	g_free (str);

	progress->details->time_remaining_timeout_id =
		g_timeout_add (TIME_REMAINING_TIMEOUT, time_remaining_callback, progress);
	
	return FALSE;
}

static gboolean
delayed_show_callback (gpointer callback_data)
{
	NautilusFileOperationsProgress *progress;
	
	progress = NAUTILUS_FILE_OPERATIONS_PROGRESS (callback_data);
	
	progress->details->delayed_show_timeout_id = 0;
	
	gtk_widget_show (GTK_WIDGET (progress));
	
	return FALSE;
}

NautilusFileOperationsProgress *
nautilus_file_operations_progress_new (const char *title,
				       const char *operation_string,
				       const char *from_prefix,
				       const char *to_prefix,
				       gulong total_files,
				       goffset total_bytes,
				       gboolean use_timeout)
{
	GtkWidget *widget;
	gchar *primary_text;
	NautilusFileOperationsProgress *progress;

	widget = gtk_widget_new (nautilus_file_operations_progress_get_type (), NULL);
	progress = NAUTILUS_FILE_OPERATIONS_PROGRESS (widget);

	nautilus_file_operations_progress_set_operation_string (progress, operation_string);
	nautilus_file_operations_progress_set_total (progress, total_files, total_bytes);

	gtk_window_set_title (GTK_WINDOW (widget), title);
	gtk_window_set_wmclass (GTK_WINDOW (widget), "file_progress", "Nautilus");
	/* ensure that minimize button is shown and the window appears in the tasklist */
	gtk_window_set_type_hint (GTK_WINDOW (widget), GDK_WINDOW_TYPE_HINT_NORMAL);

	primary_text = g_markup_printf_escaped ("<big><b>%s</b></big>", title);
	gtk_label_set_markup(GTK_LABEL (progress->details->primary_text_label),
			primary_text);
	g_free (primary_text);

	gtk_dialog_add_button (GTK_DIALOG (widget), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

	gtk_container_set_border_width (GTK_CONTAINER (widget), OUTER_BORDER);

	/* Disable separator */
	gtk_dialog_set_has_separator (GTK_DIALOG (widget), FALSE);

	progress->details->from_prefix = from_prefix;
	progress->details->to_prefix = to_prefix;

	if (use_timeout) {
		progress->details->start_time = eel_get_system_time ();	
		progress->details->delayed_show_timeout_id =
			g_timeout_add (SHOW_TIMEOUT, delayed_show_callback, progress);
	}
	
	return progress;
}

void
nautilus_file_operations_progress_set_total (NautilusFileOperationsProgress *progress,
					     gulong files_total,
					     goffset bytes_total)
{
	g_return_if_fail (NAUTILUS_IS_FILE_OPERATIONS_PROGRESS (progress));

	progress->details->files_total = files_total;
	progress->details->bytes_total = bytes_total;

	nautilus_file_operations_progress_update (progress);
}

void
nautilus_file_operations_progress_set_operation_string (NautilusFileOperationsProgress *progress,
							const char *operation_string)
{
	g_return_if_fail (NAUTILUS_IS_FILE_OPERATIONS_PROGRESS (progress));

	g_free (progress->details->progress_title);
	progress->details->progress_title = g_strdup (operation_string);

	nautilus_file_operations_progress_update (progress);
}

void
nautilus_file_operations_progress_new_file (NautilusFileOperationsProgress *progress,
					    const char *progress_verb,
					    const char *item_name,
					    const char *from_path,
					    const char *to_path,
					    const char *from_prefix,
					    const char *to_prefix,
					    gulong file_index,
					    goffset size)
{
	char *operation_markup;
	char *item_markup;

	g_return_if_fail (NAUTILUS_IS_FILE_OPERATIONS_PROGRESS (progress));

	progress->details->from_prefix = from_prefix;
	progress->details->to_prefix = to_prefix;

	if (progress->details->bytes_total > 0) {
		/* we haven't set up the file count yet, do not update the progress
		 * count until we do
		 */
		operation_markup = g_markup_printf_escaped ("<i>%s</i>", progress_verb);
		gtk_label_set_markup (GTK_LABEL (progress->details->operation_name_label),
				    operation_markup);
		g_free (operation_markup);

		item_markup = g_markup_printf_escaped ("<i>%s</i>", item_name);
		gtk_label_set_markup (GTK_LABEL (progress->details->item_name), item_markup);
		g_free (item_markup);

		progress->details->file_index = file_index;

		gtk_label_set_text (GTK_LABEL (progress->details->from_label), from_prefix);
		set_text_unescaped_trimmed 
			(GTK_LABEL (progress->details->from_path_label), from_path);
	
		if (progress->details->to_prefix != NULL && progress->details->to_path_label != NULL) {
			gtk_label_set_text (GTK_LABEL (progress->details->to_label), to_prefix);
			set_text_unescaped_trimmed 
				(GTK_LABEL (progress->details->to_path_label), to_path);
		}

		if (progress->details->first_transfer_time == 0) {
			progress->details->first_transfer_time = eel_get_system_time ();
		}
	}

	nautilus_file_operations_progress_update (progress);
}

void
nautilus_file_operations_progress_clear (NautilusFileOperationsProgress *progress)
{
	gtk_label_set_text (GTK_LABEL (progress->details->from_label), "");
	gtk_label_set_text (GTK_LABEL (progress->details->from_path_label), "");
	gtk_label_set_text (GTK_LABEL (progress->details->to_label), "");
	gtk_label_set_text (GTK_LABEL (progress->details->to_path_label), "");

	progress->details->files_total = 0;
	progress->details->bytes_total = 0;

	nautilus_file_operations_progress_update (progress);
}

void
nautilus_file_operations_progress_update_sizes (NautilusFileOperationsProgress *progress,
						goffset bytes_done_in_file,
						goffset bytes_done)
{
	g_return_if_fail (NAUTILUS_IS_FILE_OPERATIONS_PROGRESS (progress));

	progress->details->bytes_copied = bytes_done;


	if (progress->details->time_remaining_timeout_id == 0) {
		/* The first time we wait five times as long before
		 * starting to show the time remaining */
		progress->details->time_remaining_timeout_id =
				g_timeout_add (TIME_REMAINING_TIMEOUT * 5, time_remaining_callback, progress);

	}
	
	nautilus_file_operations_progress_update (progress);
}

static gboolean
delayed_close_callback (gpointer callback_data)
{
	NautilusFileOperationsProgress *progress;

	progress = NAUTILUS_FILE_OPERATIONS_PROGRESS (callback_data);

	progress->details->delayed_close_timeout_id = 0;
	gtk_object_destroy (GTK_OBJECT (progress));
	return FALSE;
}

void
nautilus_file_operations_progress_done (NautilusFileOperationsProgress *progress)
{
	guint time_up;

	if (!GTK_WIDGET_MAPPED (progress)) {
		gtk_object_destroy (GTK_OBJECT (progress));
		return;
	}
	g_assert (progress->details->start_time != 0);

	/* compute time up in milliseconds */
	time_up = (eel_get_system_time () - progress->details->show_time) / 1000;
	if (time_up >= MINIMUM_TIME_UP) {
		gtk_object_destroy (GTK_OBJECT (progress));
		return;
	}
	
	/* No cancel button once the operation is done. */
	gtk_dialog_set_response_sensitive (GTK_DIALOG (progress), GTK_RESPONSE_CANCEL, FALSE);

	progress->details->delayed_close_timeout_id = g_timeout_add
		(MINIMUM_TIME_UP - time_up,
		 delayed_close_callback,
		 progress);
}

void
nautilus_file_operations_progress_pause_timeout (NautilusFileOperationsProgress *progress)
{
	guint time_up;

	if (progress->details->delayed_show_timeout_id == 0) {
		progress->details->remaining_time = 0;
		return;
	}
	
	time_up = (eel_get_system_time () - progress->details->start_time) / 1000;
	
	if (time_up >= SHOW_TIMEOUT) {
		progress->details->remaining_time = 0;
		return;
	}
	
	g_source_remove (progress->details->delayed_show_timeout_id);
	progress->details->delayed_show_timeout_id = 0;
	progress->details->remaining_time = SHOW_TIMEOUT - time_up;
}

void
nautilus_file_operations_progress_resume_timeout (NautilusFileOperationsProgress *progress)
{
	if (progress->details->delayed_show_timeout_id != 0) {
		return;
	}
	
	if (progress->details->remaining_time <= 0) {
		return;
	}
	
	progress->details->delayed_show_timeout_id =
		g_timeout_add (progress->details->remaining_time,
			       delayed_show_callback,
			       progress);
			       
	progress->details->start_time = eel_get_system_time () - 
			1000 * (SHOW_TIMEOUT - progress->details->remaining_time);
					
	progress->details->remaining_time = 0;
}
