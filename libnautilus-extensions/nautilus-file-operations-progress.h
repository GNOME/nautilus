/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* xfer-progress-dialog.h - Progress dialog for transfer operations in the
   GNOME Desktop File Operation Service.

   Copyright (C) 1999, 2000 Free Software Foundation

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   This program distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; see the file COPYING.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Author: Ettore Perazzoli <ettore@gnu.org> */

#ifndef _DFOS_XFER_PROGRESS_DIALOG_H
#define _DFOS_XFER_PROGRESS_DIALOG_H

#include <libgnomeui/gnome-dialog.h>


#define DFOS_XFER_PROGRESS_DIALOG(obj) \
  GTK_CHECK_CAST (obj, dfos_xfer_progress_dialog_get_type (), DFOSXferProgressDialog)
#define DFOS_XFER_PROGRESS_DIALOG_CLASS(klass) \
  GTK_CHECK_CLASS_CAST (klass, dfos_xfer_progress_dialog_get_type (), DFOSXferProgressDialogClass)
#define IS_DFOS_XFER_PROGRESS_DIALOG(obj) \
  GTK_CHECK_TYPE (obj, dfos_xfer_progress_dialog_get_type ())


struct _DFOSXferProgressDialog {
	GnomeDialog dialog;

	GtkWidget *operation_label;
	GtkWidget *source_label;
	GtkWidget *target_label;
	GtkWidget *progress_bar;

	gchar *operation_string;

	guint freeze_count;

	gulong file_index;
	gulong file_size;

	gulong bytes_copied;
	gulong total_bytes_copied;

	gulong files_total;
	gulong bytes_total;
};
typedef struct _DFOSXferProgressDialog DFOSXferProgressDialog;

struct _DFOSXferProgressDialogClass {
	GnomeDialogClass parent_class;
};
typedef struct _DFOSXferProgressDialogClass DFOSXferProgressDialogClass;


guint		 dfos_xfer_progress_dialog_get_type
						(void);

GtkWidget	*dfos_xfer_progress_dialog_new	(const gchar *title,
						 const gchar *operation_string,
						 gulong files_total,
						 gulong bytes_total);

void		 dfos_xfer_progress_dialog_set_total
						(DFOSXferProgressDialog *dialog,
						 gulong files_total,
						 gulong bytes_total);

void		 dfos_xfer_progress_dialog_set_operation_string
						(DFOSXferProgressDialog *dialog,
						 const gchar *operation_string);

void		 dfos_xfer_progress_dialog_new_file
						(DFOSXferProgressDialog *dialog,
						 const gchar *source_uri,
						 const gchar *target_uri,
						 gulong size);

void		 dfos_xfer_progress_dialog_update
						(DFOSXferProgressDialog *dialog,
						 gulong bytes_done_in_file,
						 gulong bytes_done);

void		 dfos_xfer_progress_dialog_freeze
						(DFOSXferProgressDialog *dialog);

void		 dfos_xfer_progress_dialog_thaw	(DFOSXferProgressDialog *dialog);

#endif /* _DFOS_XFER_PROGRESS_DIALOG_H */
