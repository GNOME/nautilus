/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-file-operations-progress.h - Progress dialog for transfer 
   operations in the GNOME Desktop File Operation Service.

   Copyright (C) 1999, 2000 Free Software Foundation
   Copyright (C) 2000, 2001 Eazel Inc.

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

#ifndef NAUTILUS_FILE_OPERATIONS_PROGRESS_H
#define NAUTILUS_FILE_OPERATIONS_PROGRESS_H

#include <libgnomeui/gnome-dialog.h>

typedef struct NautilusFileOperationsProgressDetails NautilusFileOperationsProgressDetails;

typedef struct {
	GnomeDialog dialog;
	NautilusFileOperationsProgressDetails *details;
} NautilusFileOperationsProgress;

typedef struct {
	GnomeDialogClass parent_class;
} NautilusFileOperationsProgressClass;

#define NAUTILUS_FILE_OPERATIONS_PROGRESS(obj) \
  GTK_CHECK_CAST (obj, nautilus_file_operations_progress_get_type (), NautilusFileOperationsProgress)
#define NAUTILUS_FILE_OPERATIONS_PROGRESS_CLASS(klass) \
  GTK_CHECK_CLASS_CAST (klass, nautilus_file_operations_progress_get_type (), NautilusFileOperationsProgressClass)
#define NAUTILUS_IS_FILE_OPERATIONS_PROGRESS(obj) \
  GTK_CHECK_TYPE (obj, nautilus_file_operations_progress_get_type ())

guint                           nautilus_file_operations_progress_get_type             (void);
NautilusFileOperationsProgress *nautilus_file_operations_progress_new                  (const char                     *title,
											const char                     *operation_string,
											const char                     *from_prefix,
											const char                     *to_prefix,
											gulong                          files_total,
											gulong                          bytes_total);
void                            nautilus_file_operations_progress_done                 (NautilusFileOperationsProgress *dialog);
void                            nautilus_file_operations_progress_set_progress_title   (NautilusFileOperationsProgress *dialog,
											const char                     *progress_title);
void                            nautilus_file_operations_progress_set_total            (NautilusFileOperationsProgress *dialog,
											gulong                          files_total,
											gulong                          bytes_total);
void                            nautilus_file_operations_progress_set_operation_string (NautilusFileOperationsProgress *dialog,
											const char                     *operation_string);
void                            nautilus_file_operations_progress_clear                (NautilusFileOperationsProgress *dialog);
void                            nautilus_file_operations_progress_new_file             (NautilusFileOperationsProgress *dialog,
											const char                     *progress_verb,
											const char                     *item_name,
											const char                     *from_path,
											const char                     *to_path,
											const char                     *from_prefix,
											const char                     *to_prefix,
											gulong                          file_index,
											gulong                          size);
void                            nautilus_file_operations_progress_update_sizes         (NautilusFileOperationsProgress *dialog,
											gulong                          bytes_done_in_file,
											gulong                          bytes_done);

#endif /* NAUTILUS_FILE_OPERATIONS_PROGRESS_H */
