/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nautilus-stock-dialogs.h: Various standard dialogs for Nautilus.

   Copyright (C) 2000 Eazel, Inc.

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

   Authors: Darin Adler <darin@eazel.com>
*/

#ifndef NAUTILUS_STOCK_DIALOGS_H
#define NAUTILUS_STOCK_DIALOGS_H

#include <libgnomeui/gnome-dialog.h>

typedef void (* NautilusCancelCallback) (gpointer callback_data);

/* Dialog for cancelling something that normally is fast enough not to need a dialog. */
void         nautilus_timed_wait_start          (NautilusCancelCallback  cancel_callback,
						 gpointer                callback_data,
						 const char             *window_title,
						 const char             *wait_message,
						 GtkWindow              *parent_window);
void	     nautilus_timed_wait_start_with_duration (
						 int			 duration,
						 NautilusCancelCallback  cancel_callback,
						 gpointer                callback_data,
						 const char             *window_title,
						 const char             *wait_message,
						 GtkWindow              *parent_window);
void         nautilus_timed_wait_stop           (NautilusCancelCallback  cancel_callback,
						 gpointer                callback_data);

/* Basic dialog with buttons. */
int          nautilus_run_simple_dialog         (GtkWidget              *parent,
						 gboolean               ignore_close_box,
						 const char             *text,
						 const char             *title,
						 ...);

/* Variations on gnome stock dialogs; these do line wrapping, we don't
 * bother with non-parented versions, we allow setting the title,
 * and we return GnomeDialog pointers instead of GtkWidget pointers.
 */

GnomeDialog *nautilus_show_info_dialog               (const char             *informative_message,
						      const char	     *dialog_title,
						      GtkWindow              *parent);
GnomeDialog *nautilus_show_warning_dialog            (const char             *warning_message,
						      const char	     *dialog_title,
						      GtkWindow              *parent);
GnomeDialog *nautilus_show_error_dialog              (const char             *error_message,
						      const char	     *dialog_title,
						      GtkWindow              *parent);
GnomeDialog *nautilus_show_error_dialog_with_details (const char             *error_message,
						      const char	     *dialog_title,
						      const char             *detailed_error_message,
						      GtkWindow              *parent);
GnomeDialog *nautilus_show_yes_no_dialog             (const char             *question,
						      const char	     *dialog_title,
						      const char             *yes_label,
						      const char             *no_label,
						      GtkWindow              *parent);

GnomeDialog *nautilus_create_question_dialog         (const char             *question,
						      const char	     *dialog_title,
						      const char             *answer_zero,
						      const char             *answer_one,
						      GtkWindow              *parent);
GnomeDialog *nautilus_create_info_dialog             (const char             *informative_message,
						      const char             *dialog_title,
						      GtkWindow              *parent);

#endif /* NAUTILUS_STOCK_DIALOGS_H */
