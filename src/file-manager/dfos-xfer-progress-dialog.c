/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* dfos-xfer-progress-dialog.c - Progress dialog for transfer operations in the
   GNOME Desktop File Operation Service.

   Copyright (C) 1999, 2000 Free Software Foundation

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
#include <libgnomevfs/gnome-vfs-utils.h>
#include "dfos-xfer-progress-dialog.h"

#include <gnome.h>


#define DIALOG_WIDTH 350	/* FIXME bugzilla.eazel.com 675: ? */


static GnomeDialogClass *parent_class;


/* Private functions.  */

static void
update (DFOSXferProgressDialog *dialog)
{
	gtk_progress_configure (GTK_PROGRESS (dialog->progress_bar),
				dialog->total_bytes_copied,
				0.0, dialog->bytes_total);
}

/* This code by Jonathan Blandford (jrb@redhat.com) was shamelessly ripped from
   `gnome/gdialog.c' in Midnight Commander with minor changes.  */
static char *
trim_string (const char *string,
	     GdkFont *font,
	     guint length,
	     guint cur_length)
{
        static guint dotdotdot = 0;
        char *string_copy = NULL;
        gint len;

        if (!dotdotdot)
                dotdotdot = gdk_string_width (font, "...");

        /* Cut the font length of string to length. */

        length -= dotdotdot;
        len = (gint) ((1.0 - (gfloat) length / (gfloat) cur_length)
		      * strlen (string));
        
        /* we guess a starting point */
        if (gdk_string_width (font, string + len) < length) {
                while (gdk_string_width (font, string + len) < length)
                        len --;
                len++;
        } else {
                while (gdk_string_width (font, string + len) > length)
                        len ++;
        }

        string_copy = g_strdup_printf ("...%s", string + len);
        return string_copy;
}

static void
set_text_unescaped_trimmed (GtkLabel *label,
			    const char *text,
			    const char *trimmable_text,
			    guint max_width)
{
	GdkFont *font;
	char *unescaped_text;
	char *trimmed_text;
	char *unescaped_trimmable_text;
	char *s;
	guint text_width;
	guint trimmable_text_width;

	font = GTK_WIDGET (label)->style->font;

	trimmed_text = NULL;
	unescaped_text = NULL;
	unescaped_trimmable_text = NULL;
	
	if (text) {
		unescaped_text = gnome_vfs_unescape_string_for_display (text);
	}
	if (trimmable_text) {
		unescaped_trimmable_text = gnome_vfs_unescape_string_for_display (trimmable_text);
	}
	
	if (unescaped_text != NULL) {
		text_width = gdk_string_width (font, unescaped_text);
	} else {
		text_width = 0;
	}

	if (unescaped_trimmable_text != NULL)
		trimmable_text_width = gdk_string_width (font, unescaped_trimmable_text);
	else
		trimmable_text_width = 0;

	if (text_width + trimmable_text_width <= max_width) {
		s = g_strconcat (unescaped_text, unescaped_trimmable_text, NULL);
	} else {

		trimmed_text = trim_string (unescaped_trimmable_text,
					    font,
					    max_width - text_width,
					    trimmable_text_width);
		s = g_strconcat (unescaped_text, trimmed_text, NULL);
	}

	gtk_label_set_text (GTK_LABEL (label), s);
	g_free (s);
	g_free (trimmed_text);
	g_free (unescaped_text);
	g_free (unescaped_trimmable_text);
}

/* GnomeDialog signals.  */

/* This is just to make sure the dialog is not closed without explicit
   intervention.  */
static gboolean
do_close (GnomeDialog *dialog)
{
	DFOSXferProgressDialog *progress_dialog;

	progress_dialog = DFOS_XFER_PROGRESS_DIALOG (dialog);
	return FALSE;
}

/* GtkObject methods.  */

static void
destroy (GtkObject *object)
{
	DFOSXferProgressDialog *dialog;

	dialog = DFOS_XFER_PROGRESS_DIALOG (object);

	g_free (dialog->operation_string);
}

/* Initialization.  */

static GtkWidget *
create_label_in_box (GtkBox *vbox)
{
	GtkWidget *widget;

	widget = gtk_label_new ("");
	gtk_label_set_justify (GTK_LABEL (widget), GTK_JUSTIFY_LEFT);
	gtk_box_pack_start (vbox, widget, TRUE, TRUE, 0);
	gtk_widget_show (widget);

	return widget;
}

static void
init (DFOSXferProgressDialog *dialog)
{
	GnomeDialog *gnome_dialog;
	GtkBox *vbox;

	gnome_dialog = GNOME_DIALOG (dialog);
	vbox = GTK_BOX (gnome_dialog->vbox);

	dialog->operation_label = create_label_in_box (vbox);
	dialog->source_label = create_label_in_box (vbox);
	dialog->target_label = create_label_in_box (vbox);

	dialog->progress_bar = gtk_progress_bar_new ();
	gtk_progress_bar_set_bar_style (GTK_PROGRESS_BAR (dialog->progress_bar),
					GTK_PROGRESS_CONTINUOUS);
	gtk_progress_bar_set_orientation (GTK_PROGRESS_BAR (dialog->progress_bar),
					  GTK_PROGRESS_LEFT_TO_RIGHT);
	gtk_widget_set_usize (GTK_WIDGET (dialog->progress_bar), DIALOG_WIDTH,
			      -1);
	gtk_box_pack_start (vbox, dialog->progress_bar, FALSE, TRUE, 0);
	gtk_widget_show (dialog->progress_bar);

	dialog->operation_string = NULL;

	dialog->file_index = 0;
	dialog->file_size = 0;
	dialog->files_total = 0;
	dialog->bytes_total = 0;
	dialog->bytes_copied = 0;
	dialog->total_bytes_copied = 0;

	dialog->freeze_count = 0;
}

static void
class_init (DFOSXferProgressDialogClass *class)
{
	GtkObjectClass *object_class;
	GnomeDialogClass *dialog_class;

	parent_class = gtk_type_class (gnome_dialog_get_type ());

	object_class = GTK_OBJECT_CLASS (class);
	dialog_class = GNOME_DIALOG_CLASS (class);

	object_class->destroy = destroy;

	dialog_class->close = do_close;
}

/* Public functions.  */

guint
dfos_xfer_progress_dialog_get_type (void)
{
	static guint type = 0;

	if (type == 0) {
		GtkTypeInfo info = {
			"DFOSXferProgressDialog",
			sizeof (DFOSXferProgressDialog),
			sizeof (DFOSXferProgressDialogClass),
			(GtkClassInitFunc) class_init,
			(GtkObjectInitFunc) init,
			NULL,
			NULL,
			NULL
		};

		type = gtk_type_unique (gnome_dialog_get_type (), &info);
	}

	return type;
}

GtkWidget *
dfos_xfer_progress_dialog_new (const char *title,
			       const char *operation_string,
    			       const char *from_prefix,
			       const char *to_prefix,
			       gulong total_files,
			       gulong total_bytes)
{
	GtkWidget *widget;

	widget = gtk_type_new (dfos_xfer_progress_dialog_get_type ());

	dfos_xfer_progress_dialog_set_operation_string (DFOS_XFER_PROGRESS_DIALOG (widget),
							operation_string);
	dfos_xfer_progress_dialog_set_total (DFOS_XFER_PROGRESS_DIALOG (widget),
					     total_files, total_bytes);

	gtk_window_set_title (GTK_WINDOW (widget), title);

	gnome_dialog_append_button (GNOME_DIALOG (widget),
				    GNOME_STOCK_BUTTON_CANCEL);

	DFOS_XFER_PROGRESS_DIALOG (widget)->from_prefix = from_prefix;
	DFOS_XFER_PROGRESS_DIALOG (widget)->to_prefix = to_prefix;
	
	return widget;
}

void
dfos_xfer_progress_dialog_set_total (DFOSXferProgressDialog *dialog,
				     gulong files_total,
				     gulong bytes_total)
{
	g_return_if_fail (IS_DFOS_XFER_PROGRESS_DIALOG (dialog));

	dialog->files_total = files_total;
	dialog->bytes_total = bytes_total;

	update (dialog);
}

void
dfos_xfer_progress_dialog_set_operation_string (DFOSXferProgressDialog *dialog,
						const char *operation_string)
{
	g_return_if_fail (IS_DFOS_XFER_PROGRESS_DIALOG (dialog));

	gtk_label_set_text (GTK_LABEL (dialog->operation_label),
			    operation_string);
	dialog->operation_string = g_strdup (operation_string);
}

void
dfos_xfer_progress_dialog_new_file (DFOSXferProgressDialog *dialog,
				    const char *source_uri,
				    const char *target_uri,
    				    const char *from_prefix,
				    const char *to_prefix,
				    gulong file_index,
				    gulong size)
{
	char *s;

	g_return_if_fail (IS_DFOS_XFER_PROGRESS_DIALOG (dialog));
	g_return_if_fail (GTK_WIDGET_REALIZED (dialog));

	dialog->file_index = file_index;
	dialog->bytes_copied = 0;
	dialog->file_size = size;

	dialog->from_prefix = from_prefix;
	dialog->to_prefix = to_prefix;

	s = g_strdup_printf ("%s file %ld/%ld",
			     dialog->operation_string,
			     dialog->file_index, dialog->files_total);
	gtk_label_set_text (GTK_LABEL (dialog->operation_label), s);
	g_free (s);

	set_text_unescaped_trimmed (GTK_LABEL (dialog->source_label),
		dialog->from_prefix, source_uri, DIALOG_WIDTH);

	if (dialog->to_prefix != NULL && dialog->target_label != NULL)
		set_text_unescaped_trimmed (GTK_LABEL (dialog->target_label),
			dialog->to_prefix, target_uri, DIALOG_WIDTH);


	update (dialog);
}

void
dfos_xfer_progress_dialog_clear (DFOSXferProgressDialog *dialog)
{
	gtk_label_set_text (GTK_LABEL (dialog->source_label), "");
	gtk_label_set_text (GTK_LABEL (dialog->target_label), "");

	dialog->files_total = 0;
	dialog->bytes_total = 0;

	update (dialog);
}

void
dfos_xfer_progress_dialog_update (DFOSXferProgressDialog *dialog,
				  gulong bytes_done_in_file,
				  gulong bytes_done)
{
	g_return_if_fail (IS_DFOS_XFER_PROGRESS_DIALOG (dialog));

	dialog->bytes_copied = bytes_done_in_file;
	dialog->total_bytes_copied = bytes_done;

	update (dialog);
}

void
dfos_xfer_progress_dialog_freeze (DFOSXferProgressDialog *dialog)
{
	dialog->freeze_count++;
}

void
dfos_xfer_progress_dialog_thaw (DFOSXferProgressDialog *dialog)
{
	if (dialog->freeze_count > 0)
		dialog->freeze_count--;
}

