/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* xfer-progress-dialog.h - Progress dialog for transfer operations in the
   GNOME Desktop File Operation Service.

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000 Eazel Inc.

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

   Author: 
   	Ettore Perazzoli <ettore@gnu.org> 
   	Pavel Cisler <pavel@eazel.com>
*/

#ifndef _DFOS_XFER_PROGRESS_DIALOG_H
#define _DFOS_XFER_PROGRESS_DIALOG_H

#include <libgnomeui/gnome-dialog.h>

typedef struct DFOSXferProgressDialog DFOSXferProgressDialog;
typedef struct DFOSXferProgressDialogClass DFOSXferProgressDialogClass;

#define DFOS_XFER_PROGRESS_DIALOG(obj) \
  GTK_CHECK_CAST (obj, dfos_xfer_progress_dialog_get_type (), DFOSXferProgressDialog)
#define DFOS_XFER_PROGRESS_DIALOG_CLASS(klass) \
  GTK_CHECK_CLASS_CAST (klass, dfos_xfer_progress_dialog_get_type (), DFOSXferProgressDialogClass)
#define IS_DFOS_XFER_PROGRESS_DIALOG(obj) \
  GTK_CHECK_TYPE (obj, dfos_xfer_progress_dialog_get_type ())


guint		 dfos_xfer_progress_dialog_get_type	
						(void);

GtkWidget	*dfos_xfer_progress_dialog_new	(const char *title,
						 const char *operation_string,
						 const char *from_prefix,
						 const char *to_prefix,
						 gulong files_total,
						 gulong bytes_total);

void		 dfos_xfer_progres_dialog_set_progress_title
						(DFOSXferProgressDialog *dialog,
						 const char *progress_title);

void		 dfos_xfer_progress_dialog_set_total	
						(DFOSXferProgressDialog *dialog,
						 gulong files_total,
						 gulong bytes_total);

void		 dfos_xfer_progress_dialog_set_operation_string
						(DFOSXferProgressDialog *dialog,
						 const char *operation_string);

void		 dfos_xfer_progress_dialog_clear 
						(DFOSXferProgressDialog *dialog);

void		 dfos_xfer_progress_dialog_new_file
						(DFOSXferProgressDialog *dialog,
						 const char *progress_verb,
						 const char *item_name,
						 const char *from_path,
						 const char *to_path,
						 const char *from_prefix,
						 const char *to_prefix,
						 gulong file_index,
						 gulong size);

void		 dfos_xfer_progress_dialog_update_sizes
						(DFOSXferProgressDialog *dialog,
						 gulong bytes_done_in_file,
						 gulong bytes_done);

void		 dfos_xfer_progress_dialog_freeze
						(DFOSXferProgressDialog *dialog);

void		 dfos_xfer_progress_dialog_thaw	(DFOSXferProgressDialog *dialog);


typedef struct DFOSXferProgressDialogDetails DFOSXferProgressDialogDetails;

struct DFOSXferProgressDialog {
	GnomeDialog dialog;
	DFOSXferProgressDialogDetails *details;
};

struct DFOSXferProgressDialogClass {
	GnomeDialogClass parent_class;
};

#endif /* _DFOS_XFER_PROGRESS_DIALOG_H */
